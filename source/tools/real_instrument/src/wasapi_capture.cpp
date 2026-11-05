#include "wasapi_capture.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
#include <future>
#include <memory>
#include <sstream>
#include <stdexcept>
namespace real_instrument {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT h,const char* operation){
    if(FAILED(h)){std::ostringstream s;s<<operation<<" (0x"<<std::hex<<static_cast<unsigned long>(h)<<')';throw std::runtime_error(s.str());}
}
struct ComScope {
    // A thread already in another apartment (a UI thread) can still use the
    // device enumerator; only a scope that initialized COM uninitializes it.
    ComScope(){
        const HRESULT h=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        if(h==RPC_E_CHANGED_MODE)return;
        check(h,"Inicializar audio COM");initialized=true;
    }
    ~ComScope(){if(initialized)CoUninitialize();}
    bool initialized=false;
};
std::string utf8(const wchar_t* s){
    if(!s)return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s,-1,nullptr,0,nullptr,nullptr);
    if(n<=1)return {};
    std::string result(static_cast<size_t>(n),'\0');
    WideCharToMultiByte(CP_UTF8,0,s,-1,result.data(),n,nullptr,nullptr);result.pop_back();return result;
}
std::wstring wide(const std::string& s){
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,nullptr,0);
    if(n<1)throw std::runtime_error("ID do microfone invalido");
    std::wstring result(static_cast<size_t>(n),L'\0');
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,result.data(),n);result.pop_back();return result;
}
ComPtr<IMMDeviceEnumerator> enumerator(){
    ComPtr<IMMDeviceEnumerator> e;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&e)),"Enumerar microfones");return e;
}
struct FreeFormat {void operator()(WAVEFORMATEX* p)const{CoTaskMemFree(p);}};
}
WasapiCapture::~WasapiCapture(){Stop();}
std::vector<DeviceInfo> WasapiCapture::ListDevices(){
    ComScope scope;auto e=enumerator();ComPtr<IMMDeviceCollection> devices;
    check(e->EnumAudioEndpoints(eCapture,DEVICE_STATE_ACTIVE,&devices),"Listar dispositivos");
    UINT count=0;check(devices->GetCount(&count),"Contar microfones");std::vector<DeviceInfo> result;
    for(UINT i=0;i<count;++i){
        ComPtr<IMMDevice> device;check(devices->Item(i,&device),"Ler dispositivo");
        LPWSTR raw=nullptr;check(device->GetId(&raw),"Ler ID");std::string id=utf8(raw);CoTaskMemFree(raw);
        ComPtr<IPropertyStore> properties;check(device->OpenPropertyStore(STGM_READ,&properties),"Ler nome");
        PROPVARIANT v;PropVariantInit(&v);
        HRESULT h=properties->GetValue(PKEY_Device_FriendlyName,&v);
        std::string name=SUCCEEDED(h)&&v.vt==VT_LPWSTR?utf8(v.pwszVal):id;PropVariantClear(&v);
        result.push_back({id,name});
    }
    return result;
}
void WasapiCapture::Start(const std::string& id,Callback callback){
    Stop();if(!callback)throw std::runtime_error("Callback de audio ausente");
    {std::lock_guard<std::mutex> lock(mutex_);error_.clear();}
    stop_=false;std::promise<void> ready;auto future=ready.get_future();
    thread_=std::thread([this,id,callback=std::move(callback),ready=std::move(ready)]()mutable{
        bool announced=false;
        try{
            ComScope scope;auto e=enumerator();ComPtr<IMMDevice> device;
            if(id.empty())check(e->GetDefaultAudioEndpoint(eCapture,eConsole,&device),"Abrir microfone padrao");
            else {auto w=wide(id);check(e->GetDevice(w.c_str(),&device),"Abrir microfone selecionado");}
            ComPtr<IAudioClient> client;
            check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())),"Ativar captura");
            WAVEFORMATEX* raw=nullptr;check(client->GetMixFormat(&raw),"Ler formato de audio");
            std::unique_ptr<WAVEFORMATEX,FreeFormat> format(raw);
            bool floating=raw->wFormatTag==WAVE_FORMAT_IEEE_FLOAT;
            bool pcm=raw->wFormatTag==WAVE_FORMAT_PCM;
            if(raw->wFormatTag==WAVE_FORMAT_EXTENSIBLE && raw->cbSize>=22){
                const auto* ext=reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(raw);
                floating=IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)!=0;
                pcm=IsEqualGUID(ext->SubFormat,KSDATAFORMAT_SUBTYPE_PCM)!=0;
            }
            const uint32_t channels=raw->nChannels,rate=raw->nSamplesPerSec,bits=raw->wBitsPerSample;
            const uint32_t bytes=bits/8;
            if(channels<1 || channels>32 || rate<8000 || rate>192000
                || (!(floating&&bits==32) && !(pcm&&(bits==16||bits==24||bits==32)))
                || raw->nBlockAlign!=channels*bytes)throw std::runtime_error("Formato do microfone nao suportado; selecione PCM 16/24/32 bits ou float 32 bits");
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,0,1000000,0,raw,nullptr),"Inicializar captura (confira permissao do microfone no Windows)");
            ComPtr<IAudioCaptureClient> capture;check(client->GetService(IID_PPV_ARGS(&capture)),"Obter captura");
            UINT32 capacity=0;check(client->GetBufferSize(&capacity),"Ler buffer");
            std::vector<float> samples(static_cast<size_t>(capacity)*channels);
            check(client->Start(),"Iniciar microfone");
            struct StopClient {IAudioClient* p;~StopClient(){p->Stop();}} stopClient{client.Get()};
            announced=true;ready.set_value();
            while(!stop_){
                UINT32 packet=0;check(capture->GetNextPacketSize(&packet),"Ler pacote");
                if(!packet){Sleep(5);continue;}
                while(packet && !stop_){
                    BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 position=0,qpc=0;
                    check(capture->GetBuffer(&data,&frames,&flags,&position,&qpc),"Receber audio");
                    struct Release {IAudioCaptureClient* p;UINT32 n;~Release(){if(p)p->ReleaseBuffer(n);}} release{capture.Get(),frames};
                    if(frames>capacity)throw std::runtime_error("Pacote maior que buffer de captura");
                    const size_t count=static_cast<size_t>(frames)*channels;
                    if(flags&AUDCLNT_BUFFERFLAGS_SILENT)std::fill_n(samples.data(),count,0.f);
                    else for(size_t i=0;i<count;++i){
                        const BYTE* p=data+i*bytes;
                        if(floating){float v;std::memcpy(&v,p,4);samples[i]=v;}
                        else if(bits==16){int16_t v;std::memcpy(&v,p,2);samples[i]=static_cast<float>(v)/32768.f;}
                        else if(bits==32){int32_t v;std::memcpy(&v,p,4);samples[i]=static_cast<float>(v)/2147483648.f;}
                        else {int32_t v=p[0]|(p[1]<<8)|(p[2]<<16);if(v&0x800000)v-=0x1000000;samples[i]=static_cast<float>(v)/8388608.f;}
                    }
                    callback(samples.data(),frames,channels,rate,static_cast<double>(position)/rate,(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)!=0);
                    // ReleaseBuffer must run before asking for the next packet.
                    release.p->ReleaseBuffer(release.n);release.n=0;release.p=nullptr;
                    check(capture->GetNextPacketSize(&packet),"Proximo pacote");
                }
            }
        }catch(const std::exception& ex){
            {std::lock_guard<std::mutex> lock(mutex_);error_=ex.what();}
            if(!announced)ready.set_exception(std::current_exception());
        }
    });
    try{future.get();}catch(...){Stop();throw;}
}
void WasapiCapture::Stop(){stop_=true;if(thread_.joinable())thread_.join();}
std::string WasapiCapture::Error()const{std::lock_guard<std::mutex> lock(mutex_);return error_;}
}
