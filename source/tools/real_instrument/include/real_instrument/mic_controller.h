#pragma once
// The PC microphone owner of the host (v0.6.0): the on/off switch, the device
// list, the saved choice (<config>/ocarina_microfone.json) and the
// GameMicrophone the game frame ticks. The in-game menu (Esc > Microfone da
// Ocarina) drives it. Not thread-safe: call it from the game frame thread;
// GameMicrophone does its own locking against the capture threads.
#include "game_microphone.h"
#include "mic_settings.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace real_instrument {
class MicController {
public:
    struct Options {
        std::filesystem::path configPath;     // the saved choice
        std::string environmentEnabled;       // TRIAEVUM_OCARINA_MIC
        std::string environmentDevice;        // TRIAEVUM_OCARINA_MIC_DEVICE
    };
    struct Result {std::string message; bool error=false;};
    // Options with the two environment variables read.
    static Options FromEnvironment(std::filesystem::path configPath);
    explicit MicController(Options options);
    ~MicController();
    MicController(const MicController&)=delete;
    MicController& operator=(const MicController&)=delete;

    [[nodiscard]] bool Enabled() const noexcept {return enabled_;}
    Result SetEnabled(bool enabled);
    // "Padrão do Windows" first, then the devices of the last listing.
    [[nodiscard]] std::vector<std::string> ChoiceLabels() const;
    [[nodiscard]] int SelectedChoice() const;
    Result SelectChoice(int choice);
    // Lists the devices again (a device was plugged or removed) and re-applies
    // the chosen one: back on it if it returned, the default if it is gone.
    Result RefreshDevices();
    // Changes whenever the list may have changed.
    [[nodiscard]] uint64_t DevicesRevision() const noexcept {return revision_;}
    void SetMonitoring(bool monitoring);
    // The microphone the frame ticks; null while it is off.
    [[nodiscard]] GameMicrophone* GameInput() noexcept {return enabled_?mic_.get():nullptr;}
    // A save state was loaded: recognition and replay restart.
    void ResetGame();
    [[nodiscard]] MicrophoneStatus Status() const;
    // The friendly name of the device in use, or of the chosen one when closed.
    [[nodiscard]] std::string ActiveDeviceLabel() const;
    // Startup and listing problems (an unknown index, a missing saved device,
    // an unreadable or unwritable file); empty when there are none.
    [[nodiscard]] const std::string& Notice() const noexcept {return notice_;}
    [[nodiscard]] const std::filesystem::path& ConfigPath() const noexcept {return options_.configPath;}
private:
    GameMicrophone& Microphone();
    void ListDevices();
    std::string Save();
    std::string NameOf(const std::string& id) const;
    Options options_;
    bool enabled_=false;
    MicSettings wanted_;          // the selection: environment, saved or chosen in the menu
    MicSettings saved_;           // what the file holds; only menu changes write it
    std::vector<MicDeviceEntry> devices_;
    std::string resolvedId_;      // what the microphone was asked to open
    std::string notice_;
    std::string listError_;
    uint64_t revision_=1;
    bool monitoring_=false;
    std::unique_ptr<GameMicrophone> mic_;
};
}
