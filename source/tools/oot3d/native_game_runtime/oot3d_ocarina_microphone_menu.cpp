#include "oot3d_ocarina_microphone_menu.h"

#include <utility>

namespace Oot3dNativeGame {
namespace {

Menu::WriteResult ToMenu(const real_instrument::MicController::Result &result) {
  return {result.message, result.error};
}

class OcarinaMicrophoneMenu final : public InGameMenuMicrophone {
public:
  explicit OcarinaMicrophoneMenu(std::shared_ptr<real_instrument::MicController> controller)
      : mController(std::move(controller)) {}
  bool Enabled() const override { return mController->Enabled(); }
  Menu::WriteResult SetEnabled(bool enabled) override { return ToMenu(mController->SetEnabled(enabled)); }
  std::vector<std::string> DeviceChoices() const override { return mController->ChoiceLabels(); }
  int SelectedDevice() const override { return mController->SelectedChoice(); }
  Menu::WriteResult SelectDevice(int choice) override { return ToMenu(mController->SelectChoice(choice)); }
  Menu::WriteResult RefreshDevices() override { return ToMenu(mController->RefreshDevices()); }
  std::uint64_t DevicesRevision() const override { return mController->DevicesRevision(); }
  void SetMonitoring(bool monitoring) override { mController->SetMonitoring(monitoring); }
  InGameMenuMicrophoneStatus Status() const override {
    const auto status = mController->Status();
    InGameMenuMicrophoneStatus result;
    result.Enabled = mController->Enabled();
    result.Open = status.open;
    result.Listening = status.listening;
    result.Level = status.level;
    result.LevelText = real_instrument::MicLevelBar(status.level, 20) + "  " + real_instrument::MicLevelDecibels(status.level);
    result.LastNote = status.lastNote;
    result.LastNoteAgeSeconds = status.lastNoteAgeSeconds;
    result.DeviceLabel = mController->ActiveDeviceLabel();
    result.Problem = !status.error.empty() ? status.error : mController->Notice();
    const auto path = mController->ConfigPath().u8string();
    result.ConfigPath = std::string(path.begin(), path.end());
    return result;
  }

private:
  std::shared_ptr<real_instrument::MicController> mController;
};

} // namespace

std::shared_ptr<InGameMenuMicrophone>
CreateOcarinaMicrophoneMenu(std::shared_ptr<real_instrument::MicController> controller) {
  if (!controller) return nullptr;
  return std::make_shared<OcarinaMicrophoneMenu>(std::move(controller));
}

} // namespace Oot3dNativeGame
