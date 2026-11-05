#pragma once
// Microphone choice for the ocarina (v0.6.0): the saved file, the startup
// priority and the device list shown in the in-game menu (Esc > Microfone da
// Ocarina). Pure logic: no Windows, no audio, no threads, so it is unit tested
// and portable. Devices are kept by their endpoint ID and their friendly name,
// never by an index alone (indices shift when a device is plugged or removed).
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace real_instrument {

inline constexpr const char* kMicSettingsFormat = "ocarina_microfone_v1";
inline constexpr const char* kMicSettingsFileName = "ocarina_microfone.json";
inline constexpr const char* kMicDefaultDeviceLabel = "Padr\xC3\xA3o do Windows";

struct MicDeviceEntry {
    std::string id;   // the Windows endpoint ID
    std::string name; // the friendly name
};

// What the file holds. An empty deviceId means the Windows default device.
struct MicSettings {
    bool enabled = false;
    std::string deviceId;
    std::string deviceName;
    bool operator==(const MicSettings& other) const {
        return enabled == other.enabled && deviceId == other.deviceId && deviceName == other.deviceName;
    }
    bool operator!=(const MicSettings& other) const { return !(*this == other); }
};

[[nodiscard]] std::string SerializeMicSettings(const MicSettings& settings);
// A flat JSON object; unknown keys are ignored. False with a message on a
// malformed text or a wrong "format".
bool ParseMicSettings(const std::string& text, MicSettings* settings, std::string* error);
enum class MicFileLoad { Loaded, Missing, Invalid };
MicFileLoad LoadMicSettingsFile(const std::filesystem::path& path, MicSettings* settings, std::string* error);
// Written to a temporary file next to it, then moved over it.
bool SaveMicSettingsFile(const std::filesystem::path& path, const MicSettings& settings, std::string* error);

// The device a selection resolves to in the current list: by endpoint ID,
// then by friendly name (the ID can change after a driver update), else the
// Windows default.
struct MicDeviceResolution {
    std::string id;           // empty: the Windows default
    std::string name;         // the friendly name, or kMicDefaultDeviceLabel
    bool requestedDefault = true;
    bool found = true;        // false: the wanted device is not in the list
    bool matchedByName = false;
};
[[nodiscard]] MicDeviceResolution ResolveMicDevice(const std::vector<MicDeviceEntry>& devices, const MicSettings& wanted);

// TRIAEVUM_OCARINA_MIC_DEVICE: a decimal index into the list.
[[nodiscard]] std::optional<std::size_t> ParseMicDeviceIndex(const std::string& text, std::size_t count);

enum class MicSelectionSource { Default, Environment, Saved };
struct MicStartupPlan {
    bool enabled = false;
    MicSettings wanted;       // the runtime selection (enabled + device)
    MicDeviceResolution device;
    MicSelectionSource source = MicSelectionSource::Default;
    std::string notice;       // why something was ignored; empty if nothing was
};
// Priority: TRIAEVUM_OCARINA_MIC_DEVICE (back-compat) > saved choice > Windows
// default. Enabled: TRIAEVUM_OCARINA_MIC=1 turns the microphone on (the
// launcher), =0 off; otherwise the saved checkbox; without a file, off.
[[nodiscard]] MicStartupPlan PlanMicStartup(const std::string& environmentEnabled,
                                            const std::string& environmentDevice,
                                            const std::optional<MicSettings>& saved,
                                            const std::vector<MicDeviceEntry>& devices);

// The menu's device list: "Padrão do Windows" first, then the devices (equal
// names are numbered so every entry can be told apart).
[[nodiscard]] std::vector<std::string> MicChoiceLabels(const std::vector<MicDeviceEntry>& devices);
// The entry of a device ID (0 = the default, also for an ID not in the list).
[[nodiscard]] int MicChoiceIndex(const std::vector<MicDeviceEntry>& devices, const std::string& deviceId);
// The selection an entry stands for; the default for an entry out of range.
[[nodiscard]] MicSettings MicSettingsForChoice(const std::vector<MicDeviceEntry>& devices, int choice, bool enabled);

// Display helpers for the menu: an RMS level as a bar of `cells` dots (-60 dBFS
// to 0 dBFS), as dBFS text, and a MIDI pitch as a Portuguese note name ("Sol4").
[[nodiscard]] std::string MicLevelBar(float rms, int cells);
[[nodiscard]] std::string MicLevelDecibels(float rms);
[[nodiscard]] std::string MicNoteName(float midiPitch);

}
