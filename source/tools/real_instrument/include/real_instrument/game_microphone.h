#pragma once
#include "game_input.h"
#include <memory>
#include <optional>
#include <string>
namespace real_instrument {
// What the in-game menu shows about the microphone (v0.6.0).
struct MicrophoneStatus {
    bool open = false;          // the capture is running
    bool listening = false;     // the ocarina is waiting for a phrase
    bool monitoring = false;    // the menu keeps the capture open for the meter
    bool fellBack = false;      // the chosen device failed: the Windows default is in use
    std::string requestedDeviceId; // empty: the Windows default
    std::string activeDeviceId;    // the device opened (empty: the Windows default)
    float level = 0;            // recent RMS level, 0..1
    std::string lastNote;       // the last voiced pitch ("Sol4"), empty if none yet
    double lastNoteAgeSeconds = -1;
    std::string error;          // the last open/capture problem, empty if none
};
class GameMicrophone {
public:
    static bool EnabledFromEnvironment();
    GameMicrophone();
    ~GameMicrophone();
    GameMicrophone(const GameMicrophone&)=delete;
    GameMicrophone& operator=(const GameMicrophone&)=delete;
    OcarinaInputUpdate Tick(const OcarinaGameState& state,bool manualInput,double seconds);
    std::optional<SongConfirmation> Confirmation() const;
    bool Resolve(uint64_t token,int choice); // 0=Saria,1=Storms,2=retry
    void Reset();
    // v0.6.0, callable from any thread. Selects the device by endpoint ID
    // (empty: the Windows default) and applies it at once: an open capture is
    // stopped and reopened on the capture worker. If the device cannot be
    // opened, or disappears later, the Windows default is used and Status()
    // reports the error. Until it is called the device comes from
    // TRIAEVUM_OCARINA_MIC_DEVICE as before.
    void SetDevice(const std::string& deviceId);
    // Keeps the capture open while the ocarina is not listening, so the menu
    // can show the input level and the last note. Samples are not recognized.
    void SetMonitoring(bool monitoring);
    MicrophoneStatus Status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
