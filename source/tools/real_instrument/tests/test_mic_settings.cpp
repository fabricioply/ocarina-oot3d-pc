// v0.6.0: the saved microphone choice, the startup priority and the menu's
// device list model (pure logic, no audio device).
#include "real_instrument/mic_settings.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
using namespace real_instrument;
namespace {
const std::vector<MicDeviceEntry> kDevices{
    {"{0.0.1.00000000}.{aaaa}", "Microfone (USB Audio)"},
    {"{0.0.1.00000000}.{bbbb}", "Headset"},
    {"{0.0.1.00000000}.{cccc}", "Headset"},
};
void RoundTrip() {
    MicSettings settings;
    settings.enabled = true;
    settings.deviceId = "{0.0.1.00000000}.{aaaa}";
    settings.deviceName = "Microfone \"Sal\xC3\xA3o\" \\ 1\n";
    const auto text = SerializeMicSettings(settings);
    CHECK(text.find("ocarina_microfone_v1") != std::string::npos);
    MicSettings parsed;
    std::string error;
    CHECK(ParseMicSettings(text, &parsed, &error));
    CHECK(parsed == settings);
    // BOM, unknown keys, numbers, null and \u escapes (with a surrogate pair).
    CHECK(ParseMicSettings("\xEF\xBB\xBF{\"enabled\": false, \"x\": 12.5, \"y\": null, \"device_name\": \"\\u00e9\\ud83c\\udfb5\"}", &parsed, &error));
    CHECK(!parsed.enabled && parsed.deviceId.empty() && parsed.deviceName == "\xC3\xA9\xF0\x9F\x8E\xB5");
    CHECK(ParseMicSettings("{}", &parsed, &error) && parsed == MicSettings{});
    // Malformed and wrong types are rejected with a message.
    for (const char* bad : {"", "[]", "{\"enabled\": 1}", "{\"enabled\": true,}", "{\"device_id\": true}",
                            "{\"format\": \"other\"}", "{\"device_id\": \"x\"} trailing", "{\"device_id\": \"\\ud800\"}"}) {
        error.clear();
        CHECK(!ParseMicSettings(bad, &parsed, &error));
        CHECK(!error.empty());
    }
}
void Files() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("ocarina_mic_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = dir / "config" / kMicSettingsFileName;
    MicSettings settings, loaded;
    std::string error;
    CHECK(LoadMicSettingsFile(path, &loaded, &error) == MicFileLoad::Missing);
    settings.enabled = true;
    settings.deviceId = "id-1";
    settings.deviceName = "Nome";
    CHECK(SaveMicSettingsFile(path, settings, &error));  // creates config/
    CHECK(LoadMicSettingsFile(path, &loaded, &error) == MicFileLoad::Loaded && loaded == settings);
    settings.enabled = false;
    CHECK(SaveMicSettingsFile(path, settings, &error));  // replaces the file
    CHECK(LoadMicSettingsFile(path, &loaded, &error) == MicFileLoad::Loaded && loaded == settings);
    CHECK(!std::filesystem::exists(path.string() + ".tmp"));
    { std::ofstream(path, std::ios::trunc) << "{ not json"; }
    CHECK(LoadMicSettingsFile(path, &loaded, &error) == MicFileLoad::Invalid && !error.empty());
    std::filesystem::remove_all(dir);
}
void Resolution() {
    // By ID first, then by name (the ID changed), else the default.
    auto r = ResolveMicDevice(kDevices, {true, "{0.0.1.00000000}.{cccc}", "Headset"});
    CHECK(r.found && r.id == "{0.0.1.00000000}.{cccc}" && !r.matchedByName);
    r = ResolveMicDevice(kDevices, {true, "gone", "Microfone (USB Audio)"});
    CHECK(r.found && r.matchedByName && r.id == "{0.0.1.00000000}.{aaaa}");
    r = ResolveMicDevice(kDevices, {true, "gone", "Unplugged"});
    CHECK(!r.found && r.id.empty() && r.name == kMicDefaultDeviceLabel);
    r = ResolveMicDevice(kDevices, {});
    CHECK(r.found && r.requestedDefault && r.id.empty());
    r = ResolveMicDevice({}, {true, "gone", "Unplugged"});
    CHECK(!r.found && r.id.empty());
    CHECK(ParseMicDeviceIndex("2", 3) == std::optional<std::size_t>(2));
    CHECK(!ParseMicDeviceIndex("3", 3) && !ParseMicDeviceIndex("-1", 3) && !ParseMicDeviceIndex("1x", 3) &&
          !ParseMicDeviceIndex("", 3) && !ParseMicDeviceIndex("99999999999", 3));
}
void Startup() {
    const MicSettings saved{false, "{0.0.1.00000000}.{bbbb}", "Headset"};
    // Environment index > saved > default.
    auto plan = PlanMicStartup("1", "0", saved, kDevices);
    CHECK(plan.enabled && plan.source == MicSelectionSource::Environment && plan.device.id == kDevices[0].id && plan.notice.empty());
    plan = PlanMicStartup("1", "", saved, kDevices);
    CHECK(plan.enabled && plan.source == MicSelectionSource::Saved && plan.device.id == kDevices[1].id);
    plan = PlanMicStartup("", "", std::nullopt, kDevices);
    CHECK(!plan.enabled && plan.source == MicSelectionSource::Default && plan.device.id.empty());
    // An index outside the list falls to the saved choice, with a notice.
    plan = PlanMicStartup("1", "7", saved, kDevices);
    CHECK(plan.source == MicSelectionSource::Saved && plan.device.id == kDevices[1].id && !plan.notice.empty());
    plan = PlanMicStartup("1", "abc", std::nullopt, kDevices);
    CHECK(plan.source == MicSelectionSource::Default && plan.device.id.empty() && !plan.notice.empty());
    // Enabled: the launcher's =1 wins, =0 turns it off, otherwise the saved box.
    CHECK(!PlanMicStartup("", "", saved, kDevices).enabled);
    CHECK(PlanMicStartup("", "", MicSettings{true, "", ""}, kDevices).enabled);
    CHECK(!PlanMicStartup("0", "", MicSettings{true, "", ""}, kDevices).enabled);
    // A saved device that is unplugged: the default, and a notice naming it.
    plan = PlanMicStartup("1", "", MicSettings{true, "gone", "USB Mic"}, kDevices);
    CHECK(plan.device.id.empty() && !plan.device.found && plan.notice.find("USB Mic") != std::string::npos);
    CHECK(plan.wanted.deviceName == "USB Mic");  // kept, so it returns when replugged
}
void Choices() {
    const auto labels = MicChoiceLabels(kDevices);
    CHECK(labels.size() == 4 && labels[0] == kMicDefaultDeviceLabel && labels[1] == "Microfone (USB Audio)");
    CHECK(labels[2] == "Headset (1)" && labels[3] == "Headset (2)");
    CHECK(MicChoiceLabels({}).size() == 1);
    CHECK(MicChoiceIndex(kDevices, "") == 0 && MicChoiceIndex(kDevices, kDevices[2].id) == 3 && MicChoiceIndex(kDevices, "gone") == 0);
    auto chosen = MicSettingsForChoice(kDevices, 2, true);
    CHECK(chosen.enabled && chosen.deviceId == kDevices[1].id && chosen.deviceName == "Headset");
    CHECK(MicSettingsForChoice(kDevices, 0, false) == MicSettings{});
    CHECK(MicSettingsForChoice(kDevices, 9, false) == MicSettings{});
}
void Display() {
    CHECK(MicLevelBar(0.0f, 10).find('|') == std::string::npos);
    CHECK(MicLevelBar(1.0f, 10) == "||||||||||");
    const auto half = MicLevelBar(0.001f, 10);  // -60 dB: empty; -30 dB: half
    CHECK(half.find('|') == std::string::npos);
    const auto mid = MicLevelBar(0.0316f, 10);
    CHECK(mid.substr(0, 5) == "|||||" && mid[5] != '|');
    CHECK(MicLevelDecibels(0.1f) == "-20 dB" && MicLevelDecibels(0) == "sil\xC3\xAAncio");
    CHECK(MicNoteName(67.2f) == "Sol4" && MicNoteName(74) == "R\xC3\xA9" "5" && MicNoteName(60) == "D\xC3\xB3" "4");
    CHECK(MicNoteName(-5) == "?");
}
}
int main() {
    RoundTrip();
    Files();
    Resolution();
    Startup();
    Choices();
    Display();
}
