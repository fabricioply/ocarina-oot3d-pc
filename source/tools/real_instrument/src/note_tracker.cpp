#include "real_instrument/note_tracker.h"
#include <algorithm>
#include <cmath>
namespace real_instrument {
NoteTracker::NoteTracker(float floor) {
    if(std::isfinite(floor) && floor>=0 && floor<=1)noiseFloor_=floor;
}
std::optional<NoteEvent> NoteTracker::Push(PitchFrame f) {
    if(!std::isfinite(f.timeSeconds)) { Reset(); return {}; }
    if(lastTime_>=0 && (f.timeSeconds<=lastTime_ || f.timeSeconds-lastTime_>0.25)) Reset();
    const double elapsed=lastTime_>=0 ? f.timeSeconds-lastTime_ : 0;
    // Follow the local envelope. A fixed lifetime peak falsely treats natural
    // decay as a release, then starts another note at the quieter volume.
    peakRms_*=static_cast<float>(std::exp(-elapsed/0.1));
    lastTime_=f.timeSeconds;
    const bool valid=f.voiced && std::isfinite(f.midiPitch) && std::isfinite(f.rms)
        && std::isfinite(f.confidence) && f.confidence>=0.75f && f.rms>=noiseFloor_
        && (!active_ || f.rms>=peakRms_*0.25f);
    if(!valid) {
        candidate_.reset();
        // Real low-energy articulation can separate repeated notes faster
        // than a pitch-confidence dropout. Keep the longer debounce for
        // vibrato/noisy frames whose signal is still audible.
        const double releaseSeconds=f.rms<noiseFloor_ ? 0.025 : 0.06;
        if(active_ && f.timeSeconds-lastVoiced_>=releaseSeconds) {
            auto done=active_; active_.reset(); peakRms_=0; return done;
        }
        return {};
    }
    if(active_ && std::abs(f.midiPitch-active_->midiPitch)<=0.75f) {
        activePitchSum_+=f.midiPitch; ++activeSamples_;
        active_->midiPitch=static_cast<float>(activePitchSum_/activeSamples_);
        candidate_.reset(); active_->endedAt=f.timeSeconds; lastVoiced_=f.timeSeconds;
        peakRms_=std::max(peakRms_,f.rms); return {};
    }
    if(!candidate_ || std::abs(f.midiPitch-candidate_->midiPitch)>0.75f) {
        candidate_=f; sumPitch_=f.midiPitch; sumConfidence_=f.confidence; samples_=1;
        return {};
    }
    sumPitch_+=f.midiPitch; sumConfidence_+=f.confidence; ++samples_;
    // A nearby pitch during a held note needs more evidence: this keeps a
    // brief D-D#-D bend/vibrato in one note, without swallowing a stable D#.
    const bool nearby=active_ && std::max(std::abs(candidate_->midiPitch-active_->midiPitch),
        std::abs(f.midiPitch-active_->midiPitch))<=1.5f;
    const double settleSeconds=nearby ? 0.18 : (active_ ? 0.05 : 0.06);
    if(f.timeSeconds-candidate_->timeSeconds<settleSeconds) return {};
    // A moving slide has not landed. Restart the candidate around its current
    // pitch instead of committing staircase notes along the trajectory.
    if(active_ && std::abs(f.midiPitch-candidate_->midiPitch)>0.25f) {
        candidate_=f;sumPitch_=f.midiPitch;sumConfidence_=f.confidence;samples_=1;
        return {};
    }
    auto done=active_;
    active_=NoteEvent{candidate_->timeSeconds,f.timeSeconds,
        static_cast<float>(sumPitch_/samples_),static_cast<float>(sumConfidence_/samples_)};
    activePitchSum_=sumPitch_; activeSamples_=samples_;
    candidate_.reset(); lastVoiced_=f.timeSeconds; peakRms_=f.rms;
    return done;
}
void NoteTracker::Reset() {
    active_.reset(); candidate_.reset(); lastTime_=-1; lastVoiced_=0;
    sumPitch_=0; sumConfidence_=0; samples_=0; peakRms_=0;
    activePitchSum_=0;activeSamples_=0;
}
}
