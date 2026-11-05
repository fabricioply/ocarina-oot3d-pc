#include "real_instrument/mic_controller.h"
#include "wasapi_capture.h"
#include <cstdlib>
#include <exception>
#include <future>
#include <iostream>
#include <optional>
#include <utility>
namespace real_instrument {
namespace {
std::string Environment(const char* name){
#ifdef _MSC_VER
    char* value=nullptr;size_t count=0;
    if(_dupenv_s(&value,&count,name)!=0)return {};
    std::string result=value?value:"";std::free(value);return result;
#else
    const char* value=std::getenv(name);return value?value:"";
#endif
}
std::string PathText(const std::filesystem::path& path){
    const auto text=path.u8string();return std::string(text.begin(),text.end());
}
void Log(const std::string& text){std::cerr<<"Ocarina v0.6.0: "<<text<<'\n';}
}
MicController::Options MicController::FromEnvironment(std::filesystem::path configPath){
    Options options;
    options.configPath=std::move(configPath);
    options.environmentEnabled=Environment("TRIAEVUM_OCARINA_MIC");
    options.environmentDevice=Environment("TRIAEVUM_OCARINA_MIC_DEVICE");
    return options;
}
MicController::MicController(Options options):options_(std::move(options)){
    MicSettings saved;std::string error;
    std::optional<MicSettings> loaded;
    switch(LoadMicSettingsFile(options_.configPath,&saved,&error)){
    case MicFileLoad::Loaded: loaded=saved;saved_=saved;break;
    case MicFileLoad::Missing: break;
    case MicFileLoad::Invalid:
        notice_="Arquivo de microfone ignorado ("+error+")";break;
    }
    ListDevices();
    auto plan=PlanMicStartup(options_.environmentEnabled,options_.environmentDevice,loaded,devices_);
    enabled_=plan.enabled;wanted_=plan.wanted;resolvedId_=plan.device.id;
    if(!plan.notice.empty())notice_=notice_.empty()?plan.notice:notice_+". "+plan.notice;
    if(!listError_.empty())notice_=notice_.empty()?listError_:notice_+". "+listError_;
    Log(std::string("microfone ")+(enabled_?"ligado":"desligado")+", dispositivo: "+plan.device.name
        +" ("+(plan.source==MicSelectionSource::Environment?"TRIAEVUM_OCARINA_MIC_DEVICE"
              :plan.source==MicSelectionSource::Saved?"escolha salva":"padrao")+"), arquivo: "+PathText(options_.configPath));
    if(!notice_.empty())Log(notice_);
    if(enabled_)Microphone();
}
MicController::~MicController()=default;
GameMicrophone& MicController::Microphone(){
    if(!mic_){
        mic_=std::make_unique<GameMicrophone>();
        mic_->SetDevice(resolvedId_);
        mic_->SetMonitoring(monitoring_);
    }
    return *mic_;
}
void MicController::ListDevices(){
    // On a thread of its own: the frame thread may live in a COM apartment
    // (SDL, the window) that the audio enumeration must not change.
    try{
        const auto listed=std::async(std::launch::async,[]{return WasapiCapture::ListDevices();}).get();
        devices_.clear();
        for(const auto& device:listed)devices_.push_back({device.id,device.name});
        listError_.clear();
    }catch(const std::exception& error){
        listError_=std::string("N\xC3\xA3o foi poss\xC3\xADvel listar os microfones: ")+error.what();
    }
    ++revision_;
}
std::string MicController::Save(){
    std::string error;
    if(SaveMicSettingsFile(options_.configPath,saved_,&error))return {};
    Log("nao foi possivel salvar "+PathText(options_.configPath)+": "+error);
    return error;
}
MicController::Result MicController::SetEnabled(bool enabled){
    if(enabled==enabled_)return {enabled?"Microfone j\xC3\xA1 ligado":"Microfone j\xC3\xA1 desligado",false};
    enabled_=enabled;saved_.enabled=enabled;
    // A fresh recognition state either way; off also releases the ocarina.
    if(enabled)Microphone().Reset();else if(mic_)mic_->Reset();
    Log(enabled?"microfone ligado pelo menu":"microfone desligado pelo menu");
    const auto error=Save();
    if(!error.empty())return {"Aplicado, mas n\xC3\xA3o salvo: "+error,true};
    return {enabled?"Microfone ligado e salvo":"Microfone desligado e salvo",false};
}
std::vector<std::string> MicController::ChoiceLabels() const{return MicChoiceLabels(devices_);}
int MicController::SelectedChoice() const{return MicChoiceIndex(devices_,resolvedId_);}
MicController::Result MicController::SelectChoice(int choice){
    if(choice<0||choice>static_cast<int>(devices_.size()))return {"Escolha de microfone inv\xC3\xA1lida",true};
    wanted_=MicSettingsForChoice(devices_,choice,enabled_);
    resolvedId_=wanted_.deviceId;
    saved_.deviceId=wanted_.deviceId;saved_.deviceName=wanted_.deviceName;saved_.enabled=enabled_;
    // Applied at once: the capture worker stops the old device and opens this one.
    if(mic_)mic_->SetDevice(resolvedId_);
    const std::string name=choice==0?std::string(kMicDefaultDeviceLabel):wanted_.deviceName;
    Log("dispositivo escolhido no menu: "+name);
    const auto error=Save();
    if(!error.empty())return {"Usando "+name+", mas n\xC3\xA3o salvo: "+error,true};
    return {"Usando "+name+" (salvo)",false};
}
MicController::Result MicController::RefreshDevices(){
    ListDevices();
    if(!listError_.empty())return {listError_,true};
    const auto resolution=ResolveMicDevice(devices_,wanted_);
    const bool fellBack=mic_&&mic_->Status().fellBack;
    if(resolution.id!=resolvedId_||fellBack){
        resolvedId_=resolution.id;
        if(mic_)mic_->SetDevice(resolvedId_);
    }
    notice_.clear();
    std::string message=std::to_string(devices_.size())+(devices_.size()==1?" microfone encontrado":" microfones encontrados");
    if(!resolution.found){
        notice_="Microfone \""+(wanted_.deviceName.empty()?wanted_.deviceId:wanted_.deviceName)
            +"\" n\xC3\xA3o encontrado; usando o padr\xC3\xA3o do Windows";
        message+=". "+notice_;
    }
    return {message,!resolution.found};
}
void MicController::SetMonitoring(bool monitoring){
    if(monitoring==monitoring_)return;
    monitoring_=monitoring;
    if(monitoring)Microphone().SetMonitoring(true);
    else if(mic_)mic_->SetMonitoring(false);
}
void MicController::ResetGame(){if(mic_)mic_->Reset();}
MicrophoneStatus MicController::Status() const{
    if(!mic_){MicrophoneStatus s;s.requestedDeviceId=resolvedId_;s.activeDeviceId=resolvedId_;return s;}
    return mic_->Status();
}
std::string MicController::NameOf(const std::string& id) const{
    if(id.empty())return kMicDefaultDeviceLabel;
    for(const auto& device:devices_)if(device.id==id)return device.name;
    return id==wanted_.deviceId&&!wanted_.deviceName.empty()?wanted_.deviceName:id;
}
std::string MicController::ActiveDeviceLabel() const{
    const auto status=Status();
    return NameOf(status.open?status.activeDeviceId:resolvedId_);
}
}
