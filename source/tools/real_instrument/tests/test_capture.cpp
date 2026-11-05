#include "wasapi_capture.h"
#include "test_support.h"
int main(){
    real_instrument::WasapiCapture capture;bool rejected=false;
    try {capture.Start("not-a-real-windows-endpoint",[](const float*,size_t,uint32_t,uint32_t,double,bool){});}
    catch(const std::exception&){rejected=true;}
    CHECK(rejected);capture.Stop();capture.Stop();
}
