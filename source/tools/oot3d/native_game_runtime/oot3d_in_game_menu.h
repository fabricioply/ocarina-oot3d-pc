#pragma once

// The in-game menu (Esc, or R3 on a pad): a controller-friendly subset of the
// settings, drawn by the integrated UI renderer and acting only through the
// owners of each setting (the graphics runtime, the controls configuration,
// the TopScreen configuration, the game language, Photo mode and the
// save-state library). The TopScreen rows are the ones F1 offers (HUD size,
// the gameplay camera, C-stick aiming, the SELECT action and the gameplay
// options); everything without a row here stays in F1, which this menu never
// changes.

#include "oot3d_dev_free_camera.h"
#include "oot3d_game_language.h"
#include "oot3d_menu_font.h"
#include "oot3d_menu_model.h"
#include "oot3d_native_control_config.h"
#include "oot3d_photo_mode_panel.h"
#include "oot3d_savestate_library.h"
#include "oot3d_top_screen_config.h"
#include "oot3d_ui/ui_primitives.h"

#include "fast/oot3d/graphics_settings_runtime.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Oot3dNativeGame {

// Held state of the host controls the menu reads, sampled with the game input.
struct InGameMenuHostInput {
  bool Up = false, Down = false, Left = false, Right = false;
  bool Confirm = false, Back = false;
  bool TabPrevious = false, TabNext = false;
  // Esc or R3: opens and closes the menu.
  bool Toggle = false;
  // The mouse over the window, in window pixels, with the presentation
  // picture (the aspect ratio's letterbox the menu is drawn in) in the same
  // pixels; the menu maps the pointer onto its canvas through the picture, so
  // the window's shape never moves a hit. `PointerWheel` is notches this
  // frame.
  bool PointerPresent = false;
  float PointerX = 0.0F;
  float PointerY = 0.0F;
  std::uint32_t PictureX = 0U;
  std::uint32_t PictureY = 0U;
  std::uint32_t PictureWidth = 0U;
  std::uint32_t PictureHeight = 0U;
  bool PointerPressed = false;
  float PointerWheel = 0.0F;
};

// OcarinaMod (PC only): the microphone the ocarina listens to, shown as the
// "Microfone da Ocarina" tab. Host configuration, never part of the game: the
// song confirmation itself stays the game's native text box.
struct InGameMenuMicrophoneStatus {
  bool Enabled = false;
  bool Open = false;
  bool Listening = false;
  float Level = 0.0F;            // RMS 0..1
  std::string LevelText;         // the level as a bar and dBFS
  std::string LastNote;          // "Sol4", empty if none yet
  double LastNoteAgeSeconds = -1.0;
  std::string DeviceLabel;       // the device in use (or chosen, while closed)
  std::string Problem;           // an error or notice, empty if none
  std::string ConfigPath;
};

class InGameMenuMicrophone {
public:
  virtual ~InGameMenuMicrophone() = default;
  [[nodiscard]] virtual bool Enabled() const = 0;
  virtual Menu::WriteResult SetEnabled(bool enabled) = 0;
  // "Padrão do Windows" first, then the devices.
  [[nodiscard]] virtual std::vector<std::string> DeviceChoices() const = 0;
  [[nodiscard]] virtual int SelectedDevice() const = 0;
  virtual Menu::WriteResult SelectDevice(int choice) = 0;
  virtual Menu::WriteResult RefreshDevices() = 0;
  // Changes whenever DeviceChoices() may have changed.
  [[nodiscard]] virtual std::uint64_t DevicesRevision() const = 0;
  // While the tab is shown the capture stays open for the meter.
  virtual void SetMonitoring(bool monitoring) = 0;
  [[nodiscard]] virtual InGameMenuMicrophoneStatus Status() const = 0;
};

struct InGameMenuOwners {
  std::shared_ptr<NativeControlConfigRuntime> Controls;
  // Null unless the TopScreen profile is active: the TopScreen rows then stay out.
  std::shared_ptr<TopScreenUiConfigRuntime> TopScreen;
  std::shared_ptr<GameLanguageSettings> Language;
  std::shared_ptr<SavestateLibrary> Savestates;
  std::shared_ptr<DevFreeCamera> FreeCamera;
  std::shared_ptr<PhotoModeState> PhotoMode;
  // Null without the OcarinaMod microphone: the tab then stays out.
  std::shared_ptr<InGameMenuMicrophone> Microphone;
};

class InGameMenu {
public:
  explicit InGameMenu(InGameMenuOwners owners);

  // Once per presented frame. `hostGuiOwnsInput` is the ImGui surface (F1)
  // being open: the menu closes and ignores its controls while it is.
  void Update(const InGameMenuHostInput &input, bool hostGuiOwnsInput, double elapsedSeconds);
  [[nodiscard]] bool IsOpen() const noexcept { return mModel.IsOpen(); }
  // The frame's primitives on the given canvas (canvas units).
  [[nodiscard]] std::vector<oot3d::ui::UiPrimitive> BuildPresentation(float canvasWidth, float canvasHeight);

  [[nodiscard]] Menu::Model &Model() noexcept { return mModel; }
  [[nodiscard]] const Menu::Model &Model() const noexcept { return mModel; }
  // Re-reads every owner and the rows of the current tab.
  void Refresh();

private:
  Menu::Document BuildDocument();
  void BuildVideoTab(Menu::Document &document);
  void BuildGraphicsTab(Menu::Document &document);
  void BuildControlsTab(Menu::Document &document);
  void BuildPhotoModeTab(Menu::Document &document);
  void BuildGameTab(Menu::Document &document);
  void BuildMicrophoneTab(Menu::Document &document);
  // The microphone tab's live rows (level, note, device) and its monitoring.
  void SyncMicrophone();
  void UpdateMenu(const InGameMenuHostInput &input, bool hostGuiOwnsInput, double elapsedSeconds);
  void RefreshSnapshots();
  Menu::WriteResult ApplyGraphics(Fast::Oot3d::GraphicsSettings settings, bool keepPreset);
  Menu::WriteResult ApplyControls(const NativeControlConfig &config);
  Menu::WriteResult ApplyTopScreen(const TopScreenUiConfig &config);
  void UpdateDisplayConfirmation();
  void UpdateSavestateOutcome();

  InGameMenuOwners mOwners;
  Fast::Oot3d::GraphicsSettings mGraphics;
  Fast::Oot3d::GraphicsCapabilities mCapabilities;
  NativeControlConfig mControls;
  TopScreenUiConfig mTopScreen;
  Menu::Model mModel;
  bool mToggleHeld = false;
  bool mConfirmationModal = false;
  double mRefreshTimer = 0.0;
  std::optional<std::chrono::steady_clock::time_point> mSeenSavestateOutcome;
  static constexpr std::size_t kNoRow = static_cast<std::size_t>(-1);
  std::size_t mMicrophoneTab = kNoRow;
  std::size_t mMicDeviceRow = kNoRow, mMicActiveRow = kNoRow, mMicLevelRow = kNoRow, mMicNoteRow = kNoRow,
              mMicProblemRow = kNoRow, mMicFileRow = kNoRow;
  std::uint64_t mMicRevision = 0;
  bool mMicrophoneVisible = false;
};

} // namespace Oot3dNativeGame
