#pragma once

#include "oot3d_native_a32_memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Oot3dNativeGame {

// Opens a real OoT3D textbox (the game's own Message_StartTextbox, its own
// font, window and Yes/No cursor) with text supplied by the host. The text is
// written over the body of an existing two-choice message record for the
// duration of the box and restored afterwards, so no asset is modified. The
// choice is read from the game's message context.
class OcarinaDialogRuntime final {
public:
  enum class Result { None, Pending, Yes, No, Failed };
  explicit OcarinaDialogRuntime(NativeA32Memory &memory) : mMemory(memory) {}

  // title: plain ASCII text shown above a native Yes/No choice.
  bool Request(const std::string &title, int textVariant = 0);
  Result Poll();  // Pending until the player chose; consumed on Yes/No/Failed.
  void Cancel();
  bool Active() const noexcept { return mPhase != Phase::Idle; }
  // Per host refresh: advances the observation state machine.
  // confirmPressed: the player pressed A this refresh (consumed by the game's
  // next update). Reads the highlighted option while the box is in choice state.
  void Observe(std::uint64_t frame, bool confirmPressed);
  bool Pending() const noexcept { return mCalling; }
  bool NeedsExecution() const noexcept { return mCalling || mPhase == Phase::WantOpen; }
  bool Execute(oot3d::recomp::a32::GuestState &state,
               oot3d::recomp::a32::ExecutionResult *result,
               std::uint32_t *blocksConsumed);
  void Reset() noexcept;

private:
  enum class Phase { Idle, WantOpen, Opening, Open, Closing };
  bool PatchText();
  void RestoreText();
  void Finish(Result result);
  NativeA32Memory &mMemory;
  Phase mPhase = Phase::Idle;
  bool mCalling = false;
  bool mManualOpen = false;
  std::vector<std::uint8_t> mSavedContext;
  bool mHaveSavedContext = false;
  std::uint32_t mBusyFrames = 0;
  bool mPatched = false;
  std::string mTitle;
  int mTextVariant = 0;
  Result mResult = Result::None;
  oot3d::recomp::a32::GuestState mCallerState{};
  std::uint32_t mEntryAddress = 0;  // record entry (offset,size) in guest memory
  std::uint32_t mOldOffset = 0, mOldSize = 0;
  std::uint32_t mTextAddress = 0;
  std::vector<std::uint8_t> mOldText;
  std::uint64_t mLastLogged = 0;
  std::uint32_t mFramesInPhase = 0;
  bool mSawChoice = false;
  int mChosen = -1;
  bool mProbe = false;
};

}  // namespace Oot3dNativeGame
