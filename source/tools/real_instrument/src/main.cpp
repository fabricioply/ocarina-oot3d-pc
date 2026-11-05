#include "real_instrument/session.h"
#include "real_instrument/song_catalog.h"
#include "wasapi_capture.h"
#include <windows.h>
#include <conio.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
using namespace real_instrument;
namespace {
std::string noteName(float pitch){
    const char* names[]={"Do","Do#","Re","Re#","Mi","Fa","Fa#","Sol","Sol#","La","La#","Si"};
    int midi=static_cast<int>(std::lround(pitch));
    return std::string(names[(midi%12+12)%12])+std::to_string(midi/12-1);
}
double number(const std::string& s){
    size_t consumed=0;double value=std::stod(s,&consumed);
    if(consumed!=s.size()||!std::isfinite(value))throw std::runtime_error("Valor numerico invalido");return value;
}
}
int main(int argc,char** argv){
    SetConsoleOutputCP(CP_UTF8);
    try{
        bool list=false,assisted=true,choose=false;int index=-1;float floor=0.005f;double seconds=0;
        for(int i=1;i<argc;++i){
            std::string arg=argv[i];
            auto value=[&](){if(i+1>=argc)throw std::runtime_error("Falta valor para "+arg);return std::string(argv[++i]);};
            if(arg=="--list-devices")list=true;
            else if(arg=="--choose")choose=true;
            else if(arg=="--device"){double n=number(value());if(n<0||n>1000||std::floor(n)!=n)throw std::runtime_error("Indice de microfone invalido");index=static_cast<int>(n);}
            else if(arg=="--mode"){auto m=value();if(m!="assisted"&&m!="precise")throw std::runtime_error("Modo: assisted ou precise");assisted=m=="assisted";}
            else if(arg=="--noise-floor"){double n=number(value());if(n<0||n>1)throw std::runtime_error("Sensibilidade deve estar entre 0 e 1");floor=static_cast<float>(n);}
            else if(arg=="--seconds"){seconds=number(value());if(seconds<=0||seconds>86400)throw std::runtime_error("Duracao invalida");}
            else if(arg=="--help"){
                std::cout<<"OcarinaMicrofone [--choose] [--list-devices] [--device N] [--mode assisted|precise] [--noise-floor 0.005] [--seconds N]\n";return 0;
            }else throw std::runtime_error("Opcao desconhecida: "+arg);
        }
        auto devices=WasapiCapture::ListDevices();
        if(list){for(size_t i=0;i<devices.size();++i)std::cout<<i<<": "<<devices[i].name<<'\n';return devices.empty()?2:0;}
        if(devices.empty())throw std::runtime_error("Nenhum microfone ativo. Confira as configuracoes de som do Windows.");
        if(choose && index<0){
            std::cout<<"Escolha o microfone:\n";
            for(size_t i=0;i<devices.size();++i)std::cout<<i<<": "<<devices[i].name<<'\n';
            std::cout<<"Numero (ENTER = padrao do Windows): "<<std::flush;
            std::string choice;std::getline(std::cin,choice);
            if(!choice.empty()){
                double n=number(choice);
                if(n<0||n>1000||std::floor(n)!=n)throw std::runtime_error("Indice de microfone invalido");
                index=static_cast<int>(n);
            }
        }
        if(index>=static_cast<int>(devices.size()))throw std::runtime_error("Microfone nao encontrado; use --list-devices");
        std::string id=index>=0?devices[static_cast<size_t>(index)].id:"";
        std::cout<<"TESTE LOCAL DE MICROFONE - OCARINA v0.4.2\n"
                 <<"Modo: "<<(assisted?"assistido":"preciso")<<" | sem gravacao e sem conexao de rede\n"
                 <<"Diagnostico independente: nao controla o jogo. Reconhece as 12 musicas padrao.\n";
        for(const auto& song:NativeSongCatalog()){
            std::cout<<song.id<<": ";
            constexpr float pitches[]={62,65,69,71,74};
            for(uint32_t i=0;i<song.length;++i)std::cout<<noteName(pitches[song.notes[i]])<<' ';
            std::cout<<'\n';
        }
        std::cout
                 <<"Pode mudar a tonalidade da frase inteira. Uma nota por vez.\n"
                 <<"Pode ligar as notas com slide ou pull-off; sustente a chegada.\n"
                 <<"Variacoes breves e enfeites podem ser ignorados pelo modo assistido.\n"
                 <<"Notas repetidas: articule de novo, com uma pequena separacao.\n"
                 <<"Depois da ultima nota, fique em silencio por um instante.\n"
                 <<"Ouvindo/Sustentando = medicao; NOTA ESTAVEL = candidata, sujeita ao contexto.\n"
                 <<"R = nova tentativa | Q ou ESC = sair\n\n"
                 <<"Microfone: "<<(index>=0?devices[static_cast<size_t>(index)].name:"padrao do Windows")<<"\n"<<std::flush;
        RecognitionSession session(assisted,floor);session.Start();std::mutex state;
        WasapiCapture capture;float lastPitch=-100;double lastDisplay=-1;unsigned confirmed=0;
        capture.Start(id,[&](const float* pcm,size_t frames,uint32_t channels,uint32_t rate,double t,bool gap){
            std::lock_guard<std::mutex> lock(state);
            if(gap){session.Reset();lastPitch=-100;lastDisplay=-1;confirmed=0;}
            auto update=session.Process(pcm,frames,channels,rate,t);
            if(update.hasPitch && (lastDisplay<0 || t-lastDisplay>=0.25)){
                if(update.pitch.voiced && (std::abs(update.pitch.midiPitch-lastPitch)>0.4f || t-lastDisplay>1.0)){
                    const bool sustained=std::abs(update.pitch.midiPitch-lastPitch)<=0.4f;
                    std::cout<<(sustained?"Sustentando: ":"Ouvindo: ")<<noteName(update.pitch.midiPitch)<<" ("<<std::fixed<<std::setprecision(1)<<update.pitch.midiPitch<<")\n";
                    lastPitch=update.pitch.midiPitch;lastDisplay=t;
                }else if(!update.pitch.voiced && t-lastDisplay>2.0){
                    std::cout<<"Aguardando nota... nivel "<<std::setprecision(4)<<update.pitch.rms<<"\n";lastDisplay=t;lastPitch=-100;
                }
            }
            for(const auto& note:update.notes)std::cout<<"NOTA ESTAVEL #"<<++confirmed<<": "<<noteName(note.midiPitch)<<" ("<<std::fixed<<std::setprecision(2)<<note.midiPitch<<")\n";
            if(update.hint && !update.song && !update.hint->alternative.empty())
                std::cout<<"Ambiguo: "<<update.hint->id<<" ou "<<update.hint->alternative
                    <<". Repita a frase: Saria sobe pouco na terceira nota; Storms sobe bastante.\n";
            else if(update.hint && !update.song)std::cout<<"Trecho mais proximo: "<<update.hint->id<<" - "<<update.hint->matchedNotes<<'/'<<update.hint->totalNotes<<" notas\n";
            if(update.song)std::cout<<"\n*** RECONHECIDA: "<<update.song->id<<" ***\nPressione R para tentar novamente.\n\n";
            std::cout<<std::flush;
        });
        auto began=std::chrono::steady_clock::now();
        while(true){
            if(_kbhit()){
                int key=_getch();if(key=='q'||key=='Q'||key==27)break;
                if(key=='r'||key=='R'){std::lock_guard<std::mutex> lock(state);session.Reset();confirmed=0;lastPitch=-100;lastDisplay=-1;std::cout<<"Nova tentativa.\n"<<std::flush;}
            }
            auto error=capture.Error();if(!error.empty())throw std::runtime_error(error);
            if(seconds>0 && std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()>=seconds)break;
            Sleep(20);
        }
        capture.Stop();session.Stop();return 0;
    }catch(const std::exception& error){std::cerr<<"Erro: "<<error.what()<<'\n';return 1;}
}
