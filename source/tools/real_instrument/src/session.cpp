#include "real_instrument/session.h"
#include "real_instrument/song_catalog.h"
#include <algorithm>
#include <cmath>
namespace real_instrument {
RecognitionSession::RecognitionSession(bool assisted,float floor):detector_(floor),tracker_(floor),matcher_(SongCatalog(),assisted) {
    if(std::isfinite(floor) && floor>=0 && floor<=1)noiseFloor_=floor;
}
void RecognitionSession::Start(){running_=true;Reset();}
void RecognitionSession::Stop(){running_=false;Reset();}
void RecognitionSession::Reset(){tracker_.Reset();matcher_.Reset();rate_=0;cursor_=filled_=sinceHop_=0;nextTime_=-1;lastNoteEnd_=-1;unvoicedSince_=-1;}
SessionUpdate RecognitionSession::Process(const float* pcm,size_t frames,uint32_t channels,uint32_t rate,double start) {
    SessionUpdate out;
    if(!running_)return out;
    if(!pcm || !frames || channels<1 || channels>32 || rate<8000 || rate>192000
        || !std::isfinite(start) || frames>rate/4) {Reset();out.discontinuity=true;return out;}
    if(rate_ && (rate_!=rate || (nextTime_>=0 && std::abs(start-nextTime_)>0.03))) {
        Reset();out.discontinuity=true;
    }
    if(!rate_) {
        rate_=rate;
        const size_t count=std::max<size_t>(512,static_cast<size_t>(2048.0*rate/48000.0));
        ring_.assign(count,0);window_.resize(count);
        hop_=std::max<size_t>(1,static_cast<size_t>(512.0*rate/48000.0));
    }
    for(size_t i=0;i<frames;++i) {
        double mono=0;
        for(uint32_t c=0;c<channels;++c) {
            float v=pcm[i*channels+c];
            if(!std::isfinite(v)) {Reset();out={};out.discontinuity=true;return out;}
            mono+=v;
        }
        ring_[cursor_]=static_cast<float>(mono/channels);
        cursor_=(cursor_+1)%ring_.size(); filled_=std::min(filled_+1,ring_.size());++sinceHop_;
        if(filled_==ring_.size() && sinceHop_>=hop_) {
            sinceHop_=0;
            for(size_t j=0;j<ring_.size();++j) window_[j]=ring_[(cursor_+j)%ring_.size()];
            auto pitch=detector_.Process(window_.data(),window_.size(),rate,start+static_cast<double>(i)/rate);
            // The long pitch window contains old sound after release. A short energy
            // gate timestamps silence promptly rather than sustaining the last note.
            double recentEnergy=0;
            for(size_t j=window_.size()-hop_;j<window_.size();++j)recentEnergy+=window_[j]*window_[j];
            pitch.rms=static_cast<float>(std::sqrt(recentEnergy/static_cast<double>(hop_)));
            if(pitch.rms<noiseFloor_)pitch.voiced=false;
            out.pitch=pitch;out.hasPitch=true;
            if(auto note=tracker_.Push(pitch)) {
                out.notes.push_back(*note);
                if(auto song=matcher_.Push(*note))out.song=std::move(song);
                out.hint=matcher_.Hint();
                lastNoteEnd_=note->endedAt;
            }
            if(pitch.voiced)unvoicedSince_=-1;else if(unvoicedSince_<0)unvoicedSince_=pitch.timeSeconds;
            if(lastNoteEnd_>=0 && unvoicedSince_>=0 && pitch.timeSeconds-std::max(unvoicedSince_,lastNoteEnd_)>=0.9){
                lastNoteEnd_=-1;
                if(auto offer=matcher_.Finish())out.hint=offer;
            }
        }
    }
    nextTime_=start+static_cast<double>(frames)/rate;
    return out;
}
}
