// v0.6.0: the host's microphone owner with real Windows enumeration, a
// temporary config file and no environment. It never listens; a device is
// opened only by the monitoring check at the end (no audio is kept).
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "real_instrument/mic_controller.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
using namespace real_instrument;
int main() {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("ocarina_mic_ctl_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = dir / kMicSettingsFileName;
    {
        MicController::Options options;
        options.configPath = path;
        options.environmentDevice = "999";  // not in any list
        MicController controller(options);
        CHECK(!controller.Enabled() && controller.GameInput() == nullptr);
        CHECK(controller.Notice().find("TRIAEVUM_OCARINA_MIC_DEVICE") != std::string::npos);
        CHECK(!controller.ChoiceLabels().empty() && controller.SelectedChoice() == 0);
        CHECK(!std::filesystem::exists(path));  // startup never writes
        CHECK(!controller.SelectChoice(-1).message.empty() && controller.SelectChoice(-1).error);
        auto result = controller.SelectChoice(0);
        CHECK(!result.error && std::filesystem::exists(path));
        result = controller.SetEnabled(true);
        CHECK(!result.error && controller.Enabled() && controller.GameInput() != nullptr);
        const auto revision = controller.DevicesRevision();
        controller.RefreshDevices();
        CHECK(controller.DevicesRevision() != revision);
        CHECK(!controller.ActiveDeviceLabel().empty());
    }
    {
        // The saved choice is read back: on, the default device.
        MicController::Options options;
        options.configPath = path;
        MicController controller(options);
        CHECK(controller.Enabled() && controller.SelectedChoice() == 0);
        CHECK(!controller.SetEnabled(false).error && controller.GameInput() == nullptr);
        // TRIAEVUM_OCARINA_MIC=1 (the launcher) wins over the saved box.
        options.environmentEnabled = "1";
        MicController forced(options);
        CHECK(forced.Enabled());
    }
    {
        // A device that cannot be opened: an error line, and either the
        // default device (fallback) or, on a PC without one, a failure. No crash.
        GameMicrophone mic;
        mic.SetDevice("not-a-real-windows-endpoint");
        mic.SetMonitoring(true);
        bool reported = false, fellBack = false;
        for (int i = 0; i < 300 && !reported; ++i) {
            Sleep(10);
            const auto status = mic.Status();
            reported = !status.error.empty();
            if (reported && status.open) {
                CHECK(status.fellBack && status.activeDeviceId.empty());
                fellBack = true;
            }
        }
        CHECK(reported);
        mic.SetDevice("");  // back to the default; the error line clears
        if (fellBack) CHECK(mic.Status().error.empty());
        mic.SetMonitoring(false);
    }
    std::filesystem::remove_all(dir);
}
