#pragma once
#include "pitch_detector.h"
#include "note_tracker.h"
#include "song_matcher.h"
namespace real_instrument {
struct SessionUpdate {
    PitchFrame pitch;
    bool hasPitch=false, discontinuity=false;
    std::vector<NoteEvent> notes;
    std::optional<SongResult> song;
    std::optional<SongHint> hint;
};
class RecognitionSession {
public:
    explicit RecognitionSession(bool assisted=true,float noiseFloor=0.005f);
    void Start();
    void Stop();
    void Reset();
    SessionUpdate Process(const float* interleaved,size_t frames,uint32_t channels,uint32_t rate,double startSeconds);
private:
    PitchDetector detector_;
    NoteTracker tracker_;
    SongMatcher matcher_;
    bool running_=false;
    float noiseFloor_=0.005f;
    uint32_t rate_=0;
    size_t cursor_=0,filled_=0,sinceHop_=0,hop_=512;
    double nextTime_=-1,lastNoteEnd_=-1,unvoicedSince_=-1;
    std::vector<float> ring_,window_;
};
}
