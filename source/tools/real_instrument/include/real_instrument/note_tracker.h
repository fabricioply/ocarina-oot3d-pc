#pragma once
#include "types.h"
#include <optional>
namespace real_instrument {
class NoteTracker {
public:
    explicit NoteTracker(float noiseFloor=0.005f);
    std::optional<NoteEvent> Push(PitchFrame frame);
    void Reset();
private:
    std::optional<NoteEvent> active_;
    std::optional<PitchFrame> candidate_;
    double lastTime_=-1, lastVoiced_=0;
    double sumPitch_=0, sumConfidence_=0;
    unsigned samples_=0;
    double activePitchSum_=0;
    unsigned activeSamples_=0;
    float peakRms_=0;
    float noiseFloor_=0.005f;
};
}
