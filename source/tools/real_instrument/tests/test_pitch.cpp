#include "real_instrument/pitch_detector.h"
#include "test_support.h"
#include <limits>
#include <random>
using namespace real_instrument;
int main() {
    PitchDetector detector;
    const double hz[] = {220,440,880};
    const float want[] = {57,69,81};
    for(int h=0;h<2;++h) for(int i=0;i<3;++i) {
        auto pcm=tone(hz[i],h!=0);
        auto result=detector.Process(pcm.data(),pcm.size(),48000,0);
        CHECK(result.voiced); CHECK(std::abs(result.midiPitch-want[i])<0.25f);
    }
    // Breath noise in a voiced vowel: weaker periodicity than a clean guitar
    // or sine wave must still provide a usable pitch, not just white noise.
    std::mt19937 voiceRng(42);std::uniform_real_distribution<float> breath(-0.035f,0.035f);
    std::vector<float> voice(2048);
    for(size_t i=0;i<voice.size();++i){
        double phase=6.283185307179586*155*i/48000.;
        voice[i]=static_cast<float>(0.05*std::sin(phase)+0.025*std::sin(2*phase)+0.012*std::sin(3*phase))+breath(voiceRng);
    }
    auto voiced=detector.Process(voice.data(),voice.size(),48000,0);
    CHECK(voiced.voiced);CHECK(std::abs(voiced.midiPitch-static_cast<float>(69+12*std::log2(155./440)))<0.4f);
    // A whistle often crosses the old 1600 Hz ceiling; detecting a lower
    // subharmonic here corrupts relative intervals when the phrase crosses it.
    for(double frequency:{1400.,1800.,2400.,3200.}) {
        auto whistle=tone(frequency,false,2048);
        auto pitch=detector.Process(whistle.data(),whistle.size(),48000,0);
        CHECK(pitch.voiced);
        CHECK(std::abs(pitch.midiPitch-static_cast<float>(69+12*std::log2(frequency/440)))<0.25f);
    }
    // Singing Song of Time an octave down reaches D2: a legitimate humming
    // register that the initial 100 Hz lower limit discarded entirely.
    auto low=tone(73.416192,true);
    auto lowPitch=detector.Process(low.data(),low.size(),48000,0);
    CHECK(lowPitch.voiced);CHECK(std::abs(lowPitch.midiPitch-38)<0.25f);
    std::vector<float> pcm(4096,0);
    CHECK(!detector.Process(pcm.data(),pcm.size(),48000,0).voiced);
    std::mt19937 rng(27); std::uniform_real_distribution<float> noise(-0.2f,0.2f);
    for(auto& v:pcm) v=noise(rng);
    CHECK(!detector.Process(pcm.data(),pcm.size(),48000,0).voiced);
    CHECK(!detector.Process(nullptr,4096,48000,0).voiced);
    CHECK(!detector.Process(pcm.data(),pcm.size(),0,0).voiced);
    pcm[40]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!detector.Process(pcm.data(),pcm.size(),48000,0).voiced);
    CHECK(!detector.Process(pcm.data(),pcm.size(),48000,std::numeric_limits<double>::infinity()).voiced);
}

