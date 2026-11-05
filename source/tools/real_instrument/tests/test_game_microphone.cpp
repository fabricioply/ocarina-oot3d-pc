#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "real_instrument/game_microphone.h"
#include "test_support.h"
#include <chrono>
int main(){
    char original[32768]{};DWORD length=GetEnvironmentVariableA("TRIAEVUM_OCARINA_MIC_DEVICE",original,sizeof(original));
    CHECK(length<sizeof(original));
    // The implementation reads the CRT environment. SetEnvironmentVariable
    // alone changes only Win32's copy after CRT startup.
    struct Restore {const char* text;bool exists;~Restore(){_putenv_s("TRIAEVUM_OCARINA_MIC_DEVICE",exists?text:"");}} restore{original,length>0};
    _putenv_s("TRIAEVUM_OCARINA_MIC_DEVICE","invalid-test-device");
    real_instrument::GameMicrophone mic;
    CHECK(!mic.Tick({},false,0).listen);
    real_instrument::OcarinaGameState state;state.canPlay=true;state.context=123;
    CHECK(mic.Tick(state,false,0.1).listen);
    bool recovered=false;
    for(int i=0;i<200;++i){
        Sleep(10);auto output=mic.Tick(state,false,0.11+i*0.01);
        if(!output.listen && output.audioGain==1){recovered=true;break;}
    }
    CHECK(recovered);mic.Reset();CHECK(!mic.Tick({},false,3).listen);
}
