#include "real_instrument/pitch_detector.h"
#include <algorithm>
#include <cmath>
namespace real_instrument {
PitchDetector::PitchDetector(float floor) : noiseFloor_(floor) {
    if (!std::isfinite(floor) || floor < 0 || floor > 1) noiseFloor_ = 0.005f;
}
PitchFrame PitchDetector::Process(const float* pcm, size_t count, uint32_t rate, double t) {
    PitchFrame result{t};
    if (!pcm || count < 512 || count > 32768 || rate < 8000 || rate > 192000 || !std::isfinite(t)) return result;
    double energy=0, mean=0;
    for(size_t i=0;i<count;++i) {
        if (!std::isfinite(pcm[i])) return result;
        mean += pcm[i];
    }
    mean /= static_cast<double>(count);
    for(size_t i=0;i<count;++i) { double v=pcm[i]-mean; energy+=v*v; }
    result.rms=static_cast<float>(std::sqrt(energy/static_cast<double>(count)));
    if(result.rms < noiseFloor_) return result;
    const size_t minTau=std::max<size_t>(2,rate/4000);
    const size_t maxTau=std::min<size_t>(rate/60,count/2-1);
    if (minTau>=maxTau) return result;
    difference_.assign(maxTau+1,1.0);
    const size_t window=count-maxTau;
    double running=0;
    for(size_t tau=1;tau<=maxTau;++tau) {
        double difference=0;
        for(size_t i=0;i<window;++i) { double d=pcm[i]-pcm[i+tau]; difference+=d*d; }
        running+=difference;
        difference_[tau]=running>1e-15 ? difference*static_cast<double>(tau)/running : 1.0;
    }
    size_t best=0;
    for(size_t tau=minTau;tau<maxTau;++tau) {
        if(difference_[tau]<0.25) {
            // Breathy voice adds small local ripples to the YIN valley. The
            // first upward sample is not necessarily the period minimum.
            // Search the rest of this valley, bounded before the next period.
            best=tau;
            const size_t end=std::min(maxTau,tau+std::max<size_t>(3,tau/3));
            for(size_t candidate=tau+1;candidate<end;++candidate){
                if(difference_[candidate]>0.35)break;
                if(difference_[candidate]<difference_[best])best=candidate;
            }
            break;
        }
    }
    if(!best) return result;
    double refined=static_cast<double>(best);
    const double left=difference_[best-1], center=difference_[best], right=difference_[best+1];
    const double denominator=left-2*center+right;
    if(std::abs(denominator)>1e-15) refined+=std::clamp(0.5*(left-right)/denominator,-0.5,0.5);
    double hz=static_cast<double>(rate)/refined;
    if(hz<60 || hz>4000) return result;
    result.confidence=static_cast<float>(std::clamp(1-center,0.0,1.0));
    result.midiPitch=static_cast<float>(69+12*std::log2(hz/440));
    result.voiced=result.confidence>=0.75f;
    return result;
}
}
