#pragma once
#include <string>
namespace real_instrument {
struct PitchFrame {
    double timeSeconds = 0;
    float midiPitch = 0;
    float confidence = 0;
    float rms = 0;
    bool voiced = false;
};
struct NoteEvent {
    double startedAt = 0;
    double endedAt = 0;
    float midiPitch = 0;
    float confidence = 0;
};
struct SongResult { std::string id; float confidence = 0; };
}
