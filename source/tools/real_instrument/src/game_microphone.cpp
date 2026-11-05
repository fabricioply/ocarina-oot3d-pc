#include "real_instrument/game_microphone.h"
#include "real_instrument/mic_settings.h"
#include "real_instrument/pitch_detector.h"
#include "real_instrument/session.h"
#include "real_instrument/song_catalog.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include "wasapi_capture.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
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
using Clock=std::chrono::steady_clock;
constexpr auto kRetryDelay=std::chrono::seconds(2);
// The menu's meter: a pitch every ~40 ms over the latest ~43 ms of mono audio,
// only while monitoring and not listening (the session measures it then).
// Used by the capture thread alone.
class MonitorPitch {
public:
    std::optional<PitchFrame> Push(const float* pcm,size_t frames,uint32_t channels,uint32_t rate,double t){
        if(!pcm||!channels||rate<8000||rate>192000)return std::nullopt;
        if(rate!=rate_){
            rate_=rate;ring_.assign(std::max<size_t>(512,static_cast<size_t>(2048.0*rate/48000.0)),0.f);
            window_.resize(ring_.size());cursor_=filled_=since_=0;hop_=std::max<size_t>(1,rate/25);
        }
        std::optional<PitchFrame> result;
        for(size_t i=0;i<frames;++i){
            double mono=0;for(uint32_t c=0;c<channels;++c)mono+=pcm[i*channels+c];
            ring_[cursor_]=static_cast<float>(mono/channels);cursor_=(cursor_+1)%ring_.size();
            filled_=std::min(filled_+1,ring_.size());++since_;
            if(filled_==ring_.size()&&since_>=hop_){
                since_=0;for(size_t j=0;j<ring_.size();++j)window_[j]=ring_[(cursor_+j)%ring_.size()];
                result=detector_.Process(window_.data(),window_.size(),rate,t+static_cast<double>(i)/rate);
            }
        }
        return result;
    }
private:
    PitchDetector detector_{0.005f};
    uint32_t rate_=0;
    size_t cursor_=0,filled_=0,since_=0,hop_=1920;
    std::vector<float> ring_,window_;
};
float MonoRms(const float* pcm,size_t frames,uint32_t channels){
    if(!pcm||!frames||!channels)return 0;
    double energy=0;
    for(size_t i=0;i<frames;++i){
        double mono=0;for(uint32_t c=0;c<channels;++c)mono+=pcm[i*channels+c];
        mono/=channels;if(std::isfinite(mono))energy+=mono*mono;
    }
    return static_cast<float>(std::sqrt(energy/static_cast<double>(frames)));
}
}
struct GameMicrophone::Impl {
    RecognitionSession session;
    OcarinaGameInput game;
    WasapiCapture capture;
    mutable std::mutex mutex;
    std::condition_variable wake;
    bool done=false,wanted=false,listening=false;
    uint64_t generation=UINT64_MAX;
    std::string failure;
    std::ofstream trace{"microfone-notas-v0.4.3.log",std::ios::app};
    double lastLevel=-1;
    uint32_t lastButtons=0;
    std::string pendingLabel;
    // v0.6.0: device selection, monitoring and the menu's status.
    bool legacyDevice=true;      // no SetDevice yet: TRIAEVUM_OCARINA_MIC_DEVICE as in v0.5
    std::string requestedId;     // empty: the Windows default
    bool fallback=false;         // the requested device failed: the default is open
    uint64_t deviceGeneration=0;
    std::atomic<bool> monitoring{false};
    Clock::time_point retryAfter{};
    bool open=false;
    std::string activeId;
    float level=0;
    std::string lastNote;
    Clock::time_point lastNoteAt{};
    std::string deviceError;
    MonitorPitch monitor;        // capture thread only
    std::thread worker;
    void Log(const std::string& text){
        std::cerr<<text<<'\n';if(trace)trace<<text<<'\n'<<std::flush;
    }
    Impl():worker([this]{Run();}){Log("Ocarina v0.6.0: diagnostico local de notas; sem gravacao de audio.");}
    ~Impl(){
        {std::lock_guard<std::mutex> lock(mutex);done=true;wanted=false;session.Stop();}
        wake.notify_one();if(worker.joinable())worker.join();
    }
    bool Desired() const {return wanted||monitoring.load();}
    // Called with the lock held.
    void Fail(const std::string& error){
        failure=error;deviceError=error;wanted=false;session.Stop();retryAfter=Clock::now()+kRetryDelay;
        Log("Ocarina microphone: "+error+"; manual buttons remain available.");
    }
    void NoteSeen(const PitchFrame& pitch){
        if(!pitch.voiced)return;
        lastNote=MicNoteName(pitch.midiPitch);lastNoteAt=Clock::now();
    }
    void OnAudio(const float* pcm,size_t frames,uint32_t channels,uint32_t rate,double t,bool gap){
        const float rms=MonoRms(pcm,frames,channels);
        std::optional<PitchFrame> monitored;
        bool listeningNow=false;
        {std::lock_guard<std::mutex> audioLock(mutex);listeningNow=wanted;}
        // The meter's pitch runs outside the lock: the game thread never waits for it.
        if(monitoring.load()&&!listeningNow)monitored=monitor.Push(pcm,frames,channels,rate,t);
        std::lock_guard<std::mutex> audioLock(mutex);
        level=std::max(rms,level*0.85f);
        if(monitored)NoteSeen(*monitored);
        if(!wanted)return;
        if(gap){session.Reset();lastLevel=-1;Log("Captura: descontinuidade, frase reiniciada.");}
        auto update=session.Process(pcm,frames,channels,rate,t);
        if(update.hasPitch)NoteSeen(update.pitch);
        if(update.hasPitch && (lastLevel<0 || t-lastLevel>=1)){
            std::ostringstream text;text<<"Sinal: nivel="<<update.pitch.rms
                <<" periodicidade="<<update.pitch.confidence<<" altura_midi="<<update.pitch.midiPitch
                <<" voz="<<update.pitch.voiced;Log(text.str());lastLevel=t;
        }
        for(const auto& note:update.notes){
            constexpr const char* names[]={"Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"};
            int midi=static_cast<int>(std::lround(note.midiPitch));
            std::ostringstream text;text<<"Nota: "<<names[(midi%12+12)%12]<<midi/12-1
                <<" midi="<<note.midiPitch<<" duracao="<<note.endedAt-note.startedAt;
            Log(text.str());
        }
        if(update.hint && !update.song){
            std::ostringstream text;text<<"Trecho: "<<update.hint->id<<' '<<update.hint->matchedNotes
                <<'/'<<update.hint->totalNotes<<" ajuste="<<update.hint->score;Log(text.str());
        }
        if(update.hint && !update.song && !update.hint->candidates.empty()){
            if(game.OfferConfirmation(update.hint->candidates)){
                Log("Ocarina: frase parecida com varias musicas; aguardando escolha no jogo (teclas 1-4 ou mouse).");
                wanted=false;wake.notify_one();
            }
        }
        if(update.song){
            game.Recognized(*update.song);wanted=false;wake.notify_one();pendingLabel=update.song->id;
            Log("Ocarina microphone recognized: "+update.song->id);
        }
    }
    void StartCapture(const std::string& device){
        capture.Start(device,[this](const float* pcm,size_t frames,uint32_t channels,uint32_t rate,double t,bool gap){
            OnAudio(pcm,frames,channels,rate,t,gap);
        });
    }
    // Opens the requested device, or the Windows default if it cannot be
    // opened. Runs on the worker without the lock; true when a capture runs.
    bool Open(){
        bool legacy=false,useDefault=false;std::string id;
        {std::lock_guard<std::mutex> lock(mutex);legacy=legacyDevice;id=requestedId;useDefault=fallback;}
        std::string device;
        try{
            if(legacy){
                const auto text=Environment("TRIAEVUM_OCARINA_MIC_DEVICE");
                if(!text.empty()){
                    size_t used=0;auto index=std::stoul(text,&used);
                    auto devices=WasapiCapture::ListDevices();
                    if(used!=text.size()||index>=devices.size())throw std::runtime_error("invalid microphone index");
                    device=devices[index].id;
                }
            }else if(!useDefault)device=id;
            StartCapture(device);
            std::lock_guard<std::mutex> lock(mutex);
            open=true;activeId=device;if(!useDefault)deviceError.clear();
            return true;
        }catch(const std::exception& error){
            if(legacy||device.empty()){std::lock_guard<std::mutex> lock(mutex);Fail(error.what());return false;}
            const std::string first=error.what();
            try{
                StartCapture("");
                std::lock_guard<std::mutex> lock(mutex);
                open=true;activeId.clear();fallback=true;
                deviceError="N\xC3\xA3o foi poss\xC3\xADvel abrir o microfone escolhido ("+first+"); usando o padr\xC3\xA3o do Windows";
                Log("Ocarina microphone: "+deviceError);
                return true;
            }catch(const std::exception& second){
                std::lock_guard<std::mutex> lock(mutex);
                Fail("Microfone escolhido indispon\xC3\xADvel ("+first+") e padr\xC3\xA3o do Windows indispon\xC3\xADvel ("+second.what()+")");
                return false;
            }
        }
    }
    void Closed(){std::lock_guard<std::mutex> lock(mutex);open=false;level=0;}
    void Run(){
        bool isOpen=false;uint64_t openGeneration=0;
        std::unique_lock<std::mutex> lock(mutex);
        while(!done){
            wake.wait_for(lock,std::chrono::milliseconds(100),[&]{
                return done||(isOpen&&(!Desired()||openGeneration!=deviceGeneration))
                    ||(!isOpen&&Desired()&&Clock::now()>=retryAfter);});
            if(done)break;
            const bool target=Desired();
            const bool changed=isOpen&&openGeneration!=deviceGeneration;
            const uint64_t current=deviceGeneration;
            const bool waiting=Clock::now()<retryAfter;
            lock.unlock();
            // A device switch stops the old capture before the new one opens.
            if(isOpen&&(!target||changed)){capture.Stop();isOpen=false;Closed();}
            if(target&&!isOpen&&!waiting){openGeneration=current;isOpen=Open();}
            if(isOpen){
                auto error=capture.Error();
                if(!error.empty()){
                    capture.Stop();isOpen=false;Closed();
                    lock.lock();
                    if(openGeneration!=deviceGeneration){
                        // Another device was chosen meanwhile: it opens on the next pass.
                    }else if(!legacyDevice&&!fallback&&!requestedId.empty()){
                        // The chosen device vanished (unplugged): continue on the default.
                        fallback=true;
                        deviceError="Microfone escolhido parou ("+error+"); usando o padr\xC3\xA3o do Windows";
                        Log("Ocarina microphone: "+deviceError);
                        session.Reset();
                    }else{
                        Fail(error);
                    }
                    lock.unlock();
                }
            }
            lock.lock();
        }
        lock.unlock();capture.Stop();
    }
};
bool GameMicrophone::EnabledFromEnvironment(){
    return Environment("TRIAEVUM_OCARINA_MIC")=="1";
}
GameMicrophone::GameMicrophone():impl_(std::make_unique<Impl>()){}
GameMicrophone::~GameMicrophone()=default;
OcarinaInputUpdate GameMicrophone::Tick(const OcarinaGameState& state,bool manual,double seconds){
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);
    if(!state.canPlay)p.failure.clear();
    auto effective=state;if(!p.failure.empty())effective.canPlay=false;
    auto output=p.game.Update(effective,manual,seconds);
    if(output.listen!=p.listening){
        p.Log(output.listen?"Ocarina microphone listening":"Ocarina microphone paused");
        if(output.listen){
            p.lastLevel=-1;const auto& catalog=NativeSongCatalog();
            std::string text="Musicas aprendidas:";
            for(size_t i=0;i<catalog.size();++i)if(state.songs[i].learned)text+=" ["+std::string(catalog[i].id)+"]";
            p.Log(text);
        }
    }
    if(output.generation!=p.generation||output.listen!=p.listening){
        p.session.Stop();if(output.listen)p.session.Start();
        p.generation=output.generation;p.listening=output.listen;
    }
    if(!p.pendingLabel.empty()){
        std::ostringstream text;text<<"Decisao do jogo para "<<p.pendingLabel<<": botoes="<<output.buttons
            <<" escuta="<<output.listen<<" manual="<<manual<<" contexto_valido="<<state.canPlay;
        p.Log(text.str());p.pendingLabel.clear();
    }
    if(output.buttons!=p.lastButtons){
        std::ostringstream text;text<<"Replay: botoes="<<output.buttons<<" t="<<seconds;
        p.Log(text.str());p.lastButtons=output.buttons;
    }
    p.wanted=output.listen;p.wake.notify_one();return output;
}
std::optional<SongConfirmation> GameMicrophone::Confirmation() const{
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);return p.game.Confirmation();
}
bool GameMicrophone::Resolve(uint64_t token,int choice){
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);
    // The A that confirmed the box also reaches the ocarina as a stray note; wait so
    // the game forgets it before the phrase is replayed (seconds, env overridable).
    double delay=1.5;
    {const auto text=Environment("TRIAEVUM_OCARINA_REPLAY_DELAY");
     if(!text.empty()){try{delay=std::stod(text);}catch(...){}}}
    bool ok=p.game.ResolveConfirmation(token,choice,delay);
    if(ok){p.Log(choice==2?"Confirmacao: tentar novamente":"Confirmacao: "+std::string(choice==0?"Saria's Song":"Song of Storms"));
        if(choice!=2)p.pendingLabel=choice==0?"Saria's Song":"Song of Storms";}
    return ok;
}
void GameMicrophone::Reset(){
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);
    p.pendingLabel.clear();p.game.Reset();p.session.Stop();p.wanted=false;p.listening=false;p.generation=UINT64_MAX;p.failure.clear();p.wake.notify_one();
}
void GameMicrophone::SetDevice(const std::string& deviceId){
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);
    p.legacyDevice=false;p.requestedId=deviceId;p.fallback=false;p.deviceError.clear();p.failure.clear();
    p.retryAfter={};++p.deviceGeneration;
    p.Log("Ocarina microphone: dispositivo selecionado "+(deviceId.empty()?std::string("(padrao do Windows)"):deviceId));
    p.wake.notify_one();
}
void GameMicrophone::SetMonitoring(bool monitoring){
    auto& p=*impl_;
    if(p.monitoring.exchange(monitoring)==monitoring)return;
    std::lock_guard<std::mutex> lock(p.mutex);
    if(monitoring)p.retryAfter={};  // opening the meter retries a failed device at once
    p.wake.notify_one();
}
MicrophoneStatus GameMicrophone::Status() const{
    auto& p=*impl_;std::lock_guard<std::mutex> lock(p.mutex);
    MicrophoneStatus s;
    s.open=p.open;s.listening=p.listening;s.monitoring=p.monitoring.load();s.fellBack=p.fallback;
    s.requestedDeviceId=p.requestedId;s.activeDeviceId=p.activeId;s.level=p.open?p.level:0.f;
    s.lastNote=p.lastNote;
    if(!p.lastNote.empty())s.lastNoteAgeSeconds=std::chrono::duration<double>(Clock::now()-p.lastNoteAt).count();
    s.error=!p.deviceError.empty()?p.deviceError:p.failure;
    return s;
}
}
