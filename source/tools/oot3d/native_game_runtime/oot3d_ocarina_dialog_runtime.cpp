#include "oot3d_ocarina_dialog_runtime.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include <cstdarg>
#include <fstream>
namespace {
// Diagnostics go to stderr and to a file next to the executable, so a run is
// diagnosable even when stderr is not redirected.
void Say(const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list copy;
  va_copy(copy, args);
  std::vfprintf(stderr, format, args);
  va_end(args);
  if (FILE *file = std::fopen("microfone-dialogo-diag.log", "a")) {
    std::vfprintf(file, format, copy);
    std::fclose(file);
  }
  va_end(copy);
}
}  // namespace

namespace Oot3dNativeGame {
namespace {
constexpr std::uint32_t kMessageContext = 0x005C1878U;
// Message_StartTextbox(play, textId, actor): the real entry (it opens the message
// context at 0x4C0BC8 and marks it active). 0x2E9A3C is Message_ContinueTextbox's
// helper and must not be used to open a fresh box.
constexpr std::uint32_t kStartTextbox = 0x00367C7CU;
constexpr std::uint32_t kBuildTag = 127;  // layout nudge
constexpr std::uint32_t kInputBoundary = 0x0041E988U;
constexpr std::uint32_t kTextbox = 0x1AU;  // "Do you want to play again?" (Yes/No)
constexpr std::uint32_t kStateChoice = 7U;    // Message_GetState() == TEXT_STATE_CHOICE
constexpr std::uint32_t kOffState = 0xF38U;   // 0 = no textbox
constexpr std::uint32_t kOffMode = 0xFA0U;
constexpr std::uint32_t kOffNext = 0xFA4U;
constexpr std::uint32_t kOffChoice = 0xFC4U;  // 0 = first option
constexpr std::uint32_t kOffLanguage = 0xF3CU;
constexpr std::uint32_t kOffTable = 0x1CU;    // message table object
constexpr std::uint32_t kRecordHeader = 0x10U;
constexpr std::uint32_t kContextBytes = 0x1090U;  // message context, up to the last field seen to change

std::uint8_t U8(NativeA32Memory &m, std::uint32_t a) {
  std::uint8_t v = 0xFF;
  m.Read8(a, &v);
  return v;
}
std::uint32_t U32(NativeA32Memory &m, std::uint32_t a) {
  std::uint32_t v = 0xFFFFFFFFU;
  m.Read32(a, &v);
  return v;
}

// Developer aid: a compact snapshot of the guest globals and play state, to diff
// the game's state before and after the confirmation box.
void DumpRanges(NativeA32Memory &memory, const char *path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return;
  const std::pair<std::uint32_t, std::uint32_t> ranges[] = {
      {0x00500000U, 0x000C4000U}, {0x0871E000U, 0x00012000U}};
  const std::uint32_t count = 2;
  out.write(reinterpret_cast<const char *>(&count), 4);
  std::vector<std::uint8_t> page(0x1000);
  for (const auto &range : ranges) {
    out.write(reinterpret_cast<const char *>(&range.first), 4);
    out.write(reinterpret_cast<const char *>(&range.second), 4);
    for (std::uint32_t offset = 0; offset < range.second; offset += 0x1000) {
      if (!memory.ReadBytes(range.first + offset, page)) std::fill(page.begin(), page.end(), 0);
      out.write(reinterpret_cast<const char *>(page.data()), 0x1000);
    }
  }
}

// Developer aid: writes the guest CPU state and every mapped page, so a faulting
// call can be replayed offline in an ARM emulator.
void DumpGuest(NativeA32Memory &memory, const oot3d::recomp::a32::GuestState &state) {
  std::ofstream out("microfone-estado.bin", std::ios::binary | std::ios::trunc);
  if (!out) return;
  const char magic[8] = {'A', '3', '2', 'D', 'U', 'M', 'P', '1'};
  out.write(magic, 8);
  for (std::uint32_t r : state.r) out.write(reinterpret_cast<const char *>(&r), 4);
  out.write(reinterpret_cast<const char *>(&state.cpsr), 4);
  out.write(reinterpret_cast<const char *>(&state.fpscr), 4);
  out.write(reinterpret_cast<const char *>(&state.thread_pointer), 4);
  for (std::uint32_t v : state.vfp) out.write(reinterpret_cast<const char *>(&v), 4);
  std::vector<std::uint8_t> page(0x1000);
  std::uint64_t runStart = 0;
  bool inRun = false;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> runs;
  for (std::uint64_t p = 0; p <= 0x100000ULL; ++p) {
    const bool mapped = p < 0x100000ULL && memory.IsMapped(static_cast<std::uint32_t>(p << 12), 0x1000);
    if (mapped && !inRun) { inRun = true; runStart = p; }
    if (!mapped && inRun) {
      inRun = false;
      runs.emplace_back(static_cast<std::uint32_t>(runStart << 12), static_cast<std::uint32_t>((p - runStart) << 12));
    }
  }
  const std::uint32_t count = static_cast<std::uint32_t>(runs.size());
  out.write(reinterpret_cast<const char *>(&count), 4);
  for (const auto &run : runs) {
    out.write(reinterpret_cast<const char *>(&run.first), 4);
    out.write(reinterpret_cast<const char *>(&run.second), 4);
    for (std::uint32_t offset = 0; offset < run.second; offset += 0x1000) {
      if (!memory.ReadBytes(run.first + offset, page)) std::fill(page.begin(), page.end(), 0);
      out.write(reinterpret_cast<const char *>(page.data()), 0x1000);
    }
  }
}
}  // namespace

void OcarinaDialogRuntime::Reset() noexcept {
  mCalling = false;
  mPhase = Phase::Idle;
  mResult = Result::None;
  mPatched = false;
}

bool OcarinaDialogRuntime::Request(const std::string &title, int textVariant) {
  mTextVariant = textVariant;
  if (mPhase != Phase::Idle) return false;
  mProbe = std::getenv("TRIAEVUM_OCARINA_DIALOG_PROBE") != nullptr;
  mTitle = title;
  mResult = Result::Pending;
  mPhase = Phase::WantOpen;
  // The ocarina session keeps its own state in the message context. Keep an
  // exact copy so it can be put back once the confirmation box is gone.
  mSavedContext.assign(kContextBytes, 0);
  mHaveSavedContext = mMemory.ReadBytes(kMessageContext, mSavedContext);
  return true;
}

OcarinaDialogRuntime::Result OcarinaDialogRuntime::Poll() {
  if (mResult == Result::None || mResult == Result::Pending) return mResult;
  const Result result = mResult;
  mResult = Result::None;
  return result;
}

void OcarinaDialogRuntime::Cancel() {
  if (mPatched) RestoreText();
  mPhase = Phase::Idle;
  mCalling = false;
  mResult = Result::None;
}

bool OcarinaDialogRuntime::PatchText() {
  const std::uint32_t language = U32(mMemory, kMessageContext + kOffLanguage);
  if (language >= 10U) return false;
  const std::uint32_t table = kMessageContext + kOffTable;
  const std::uint32_t base = U32(mMemory, table + 4U);     // loaded message file image
  const std::uint32_t records = U32(mMemory, table + 8U);  // sorted 0x60-byte records
  if (!base || base == 0xFFFFFFFFU || !records || records == 0xFFFFFFFFU) return false;
  const std::uint32_t count = U32(mMemory, base + 8U);
  if (count == 0U || count > 0x10000U) return false;
  std::uint32_t lo = 0, hi = count, rec = 0;
  while (lo < hi) {
    const std::uint32_t mid = (lo + hi) / 2U;
    const std::uint32_t id = U32(mMemory, records + mid * 0x60U);
    if (id == kTextbox) {
      rec = records + mid * 0x60U;
      break;
    }
    if (id < kTextbox) lo = mid + 1U; else hi = mid;
  }
  if (!rec) return false;
  mEntryAddress = rec + kRecordHeader + language * 8U;
  mOldOffset = U32(mMemory, mEntryAddress);
  mOldSize = U32(mMemory, mEntryAddress + 4U);
  if (!mOldSize || mOldSize > 0x1000U) return false;
  std::string title = mTitle;
  title.erase(std::remove_if(title.begin(), title.end(),
                             [](char c) {
                               return static_cast<unsigned char>(c) < 0x20U ||
                                      static_cast<unsigned char>(c) > 0x7EU;
                             }),
              title.end());
  std::vector<std::uint8_t> text(title.begin(), title.end());
  const std::uint8_t tail[] = {0x7F, 0x1C, 0x7F, 0x1C, 0x7F, 0x1A, 0xFF, 0xFF, 0xFF, 0xFF,
                               0x7F, 0x1D, 'B',  'Y',  'e',  's',  0x7F, 0x1C, 'N',  'o',
                               0x7F, 0x1D, 0x00, 0x7F, 0x00};
  // Developer text variants (to find which control codes the whole-AOT module
  // can run): 1 = plain text only, 2 = choice without color codes.
  static const char *textVariantEnv = std::getenv("TRIAEVUM_OCARINA_DIALOG_TEXT");
  const int textVariant = mTextVariant != 0 ? mTextVariant : (textVariantEnv ? std::atoi(textVariantEnv) : 0);
  if (textVariant == 1) {
    const std::uint8_t plain[] = {0x7F, 0x00};
    text.insert(text.end(), std::begin(plain), std::end(plain));
  } else if (textVariant == 2) {
    const std::uint8_t nocolor[] = {0x7F, 0x1C, 0x7F, 0x1C, 0x7F, 0x1A, 0xFF, 0xFF, 0xFF, 0xFF,
                                    'Y',  'e',  's',  0x7F, 0x1C, 'N',  'o',  0x7F, 0x00};
    text.insert(text.end(), std::begin(nocolor), std::end(nocolor));
  } else {
    text.insert(text.end(), std::begin(tail), std::end(tail));
  }
  Say("OCARINA_DIALOG text variant=%d size=%u old=%u\n", textVariant, static_cast<unsigned>(text.size()), mOldSize);
  if (text.size() > mOldSize) return false;
  mTextAddress = base + mOldOffset;
  mOldText.assign(mOldSize, 0);
  if (!mMemory.ReadBytes(mTextAddress, mOldText)) return false;
  // Sanity: the original is the bowling-alley prompt (English: "Do you want to play").
  static const char kExpected[] = "Do you want to play";
  if (language == 1U && !std::equal(kExpected, kExpected + 19, mOldText.begin())) return false;
  if (!mMemory.WriteBytes(mTextAddress, text)) return false;
  if (!mMemory.Write32(mEntryAddress + 4U, static_cast<std::uint32_t>(text.size()))) {
    mMemory.WriteBytes(mTextAddress, mOldText);
    return false;
  }
  mPatched = true;
  return true;
}

void OcarinaDialogRuntime::RestoreText() {
  if (!mPatched) return;
  mMemory.WriteBytes(mTextAddress, mOldText);
  mMemory.Write32(mEntryAddress + 4U, mOldSize);
  mPatched = false;
}

void OcarinaDialogRuntime::Finish(Result result) {
  if (std::getenv("TRIAEVUM_OCARINA_DIALOG_DUMP") != nullptr && mPhase != Phase::WantOpen) {
    DumpRanges(mMemory, "microfone-depois.bin");
    Say("OCARINA_DIALOG memory ranges dumped to microfone-depois.bin\n");
  }
  RestoreText();
  if (mHaveSavedContext && mPhase != Phase::WantOpen &&
      std::getenv("TRIAEVUM_OCARINA_DIALOG_NORESTORE") == nullptr) {
    // "core" (default): only the flag header, one state word and the message-state
    // block. "full": the whole snapshot (recognized the song but crashed the game
    // afterwards, so stale words must not come back). TRIAEVUM_OCARINA_DIALOG_NORESTORE
    // disables restoring entirely.
    static const char *restoreEnv = std::getenv("TRIAEVUM_OCARINA_DIALOG_RESTORE");
    const bool full = restoreEnv != nullptr && std::string(restoreEnv) == "full";
    const auto put = [&](std::uint32_t from, std::uint32_t to) {
      if (to > mSavedContext.size()) to = static_cast<std::uint32_t>(mSavedContext.size());
      if (from >= to) return;
      std::vector<std::uint8_t> part(mSavedContext.begin() + from, mSavedContext.begin() + to);
      mMemory.WriteBytes(kMessageContext + from, part);
    };
    if (full) {
      mMemory.WriteBytes(kMessageContext, mSavedContext);
    } else {
      put(0x000U, 0x020U);
      put(0x164U, 0x168U);
      put(0xF38U, 0xFD0U);
    }
    if (mProbe) Say("OCARINA_DIALOG message context restored (%s)\n", full ? "full" : "core");
  }
  mHaveSavedContext = false;
  mPhase = Phase::Idle;
  mCalling = false;
  mResult = result;
}

bool OcarinaDialogRuntime::Execute(oot3d::recomp::a32::GuestState &state,
                                   oot3d::recomp::a32::ExecutionResult *result,
                                   std::uint32_t *blocksConsumed) {
  if (mCalling) {
    // Message_StartTextbox returned to the input boundary.
    const std::uint32_t returned = state.r[0];
    state = mCallerState;
    mCalling = false;
    if (mManualOpen) {
      // The loader (0x2CD2B4) returned. Do what Message_StartTextbox does after it
      // (0x4C0BC8: actor, state 1, next 2; 0x367D50: active flag) but WITHOUT its
      // trailing HUD-mode reset (0x340BDC(play,0)), which closes the ocarina page.
      mManualOpen = false;
      if (returned == 0U) {
        if (mProbe) Say("OCARINA_DIALOG loader failed\n");
        Finish(Result::Failed);
        return false;
      }
      mMemory.Write32(kMessageContext + kOffMode, 0U);
      mMemory.Write8(kMessageContext + kOffState, 1U);
      mMemory.Write32(kMessageContext + kOffNext, 2U);
      mMemory.Write8(kMessageContext + 0xDU, 1U);
    }
    if (mProbe) Say( "OCARINA_DIALOG StartTextbox returned; msg state=%u\n",
                             static_cast<unsigned>(U8(mMemory, kMessageContext + kOffState)));
    mPhase = Phase::Open;
    return false;
  }
  if (mPhase != Phase::WantOpen) return false;
  // Only open over an idle message context that belongs to a running play state
  // (Message_StartTextbox dereferences it).
  if (U32(mMemory, kMessageContext + 8U) == 0U) { Finish(Result::Failed); return false; }
  if (const std::uint8_t busy = U8(mMemory, kMessageContext + kOffState); busy != 0U) {
    // Another message owns the context. A finished one (state 6, waiting to be
    // closed) can be closed the way Message_CloseTextbox (0x2E9A1C) does; anything
    // else is given a little time, then the request is abandoned.
    ++mBusyFrames;
    if (mProbe && (mBusyFrames == 1U || mBusyFrames % 60U == 0U))
      Say("OCARINA_DIALOG waiting: message context busy, state=%u (%u refreshes)\n",
          static_cast<unsigned>(busy), mBusyFrames);
    if (mBusyFrames == 45U && busy == 6U) {
      mMemory.Write32(kMessageContext + kOffNext, 3U);
      mMemory.Write8(kMessageContext + kOffState, 0xEU);
      if (mProbe) Say("OCARINA_DIALOG closed a finished message (state 6)\n");
    }
    if (mBusyFrames > 240U) {
      if (mProbe) Say("OCARINA_DIALOG gave up: message context stayed busy\n");
      mBusyFrames = 0U;
      Finish(Result::Failed);
    }
    return false;
  }
  mBusyFrames = 0U;
  if (state.r[13] < 0x20U || !mMemory.IsWritable(state.r[13] - 0x20U, 0x20U)) {
    Finish(Result::Failed);
    return false;
  }
  // Developer variants to isolate a fault: "getstate" calls the harmless
  // Message_GetState; "nopatch" opens the unmodified message.
  static const char *variantEnv = std::getenv("TRIAEVUM_OCARINA_DIALOG_VARIANT");
  std::string variant = variantEnv ? variantEnv : "";
  // Keys select the call layer: 10 = leaf Message_GetState, 11 = text loader only.
  if (mTextVariant == 10) variant = "getstate";
  if (mTextVariant == 11) variant = "loader";
  if (variant != "getstate" && variant != "nopatch" && !PatchText()) {
    if (mProbe) Say( "OCARINA_DIALOG patch failed\n");
    Finish(Result::Failed);
    return false;
  }
  if (std::getenv("TRIAEVUM_OCARINA_DIALOG_DUMP") != nullptr) {
    DumpRanges(mMemory, "microfone-antes.bin");
    Say("OCARINA_DIALOG memory ranges dumped to microfone-antes.bin\n");
  }
  mCallerState = state;
  std::uint32_t target = kStartTextbox;
  state.r[0] = U32(mMemory, kMessageContext + 8U);  // play state
  state.r[1] = kTextbox;
  state.r[2] = 0;
  mManualOpen = false;
  if (variant.empty() || variant == "manual") {
    // Default: open without the HUD-mode reset (see the return handler).
    const std::uint32_t play = U32(mMemory, kMessageContext + 8U);
    // 0x343F0C(play, play+0x224C): clears two flags unless both gates are zero.
    std::uint8_t gate = 0xFF;
    std::uint16_t gate2 = 0xFFFF;
    mMemory.Read8(play + 0x225DU, &gate);
    mMemory.Read16(play + 0x2200U + 0x5EU, &gate2);
    if (!(gate == 0U && gate2 == 0U)) {
      mMemory.Write8(play + 0x224CU + 0x10U, 0U);
      mMemory.Write8(play + 0x224CU + 0x11U, 0U);
    }
    target = 0x002CD2B4U;
    state.r[0] = kMessageContext;
    mManualOpen = true;
  }
  if (variant == "loader") {
    target = 0x002CD2B4U;  // message loader (msgCtx, textId)
    state.r[0] = kMessageContext;
  }
  if (variant == "getstate") {
    target = 0x002C2700U;  // Message_GetState(msgCtx)
    state.r[0] = kMessageContext;
  }
  if (mProbe) Say("OCARINA_DIALOG calling 0x%08X variant=%s play=0x%08X sp=0x%08X\n", target,
                  variant.c_str(), U32(mMemory, kMessageContext + 8U), state.r[13]);
  state.r[13] -= 0x20U;
  state.r[14] = kInputBoundary;
  state.r[15] = target;
  *result = {oot3d::recomp::a32::ExitKind::Branch, target,
             oot3d::recomp::a32::FallbackReason::None, kInputBoundary};
  if (blocksConsumed) *blocksConsumed = 1;
  mCalling = true;
  mPhase = Phase::Opening;
  return true;
}

void OcarinaDialogRuntime::Observe(std::uint64_t frame, bool confirmPressed) {
  if (mPhase == Phase::Idle || mPhase == Phase::WantOpen) {
    mFramesInPhase = 0;
    return;
  }
  if (mCalling) return;
  const std::uint8_t state = U8(mMemory, kMessageContext + kOffState);
  const std::uint32_t mode = U32(mMemory, kMessageContext + kOffMode);
  const std::uint32_t next = U32(mMemory, kMessageContext + kOffNext);
  const std::uint32_t choice = U32(mMemory, kMessageContext + kOffChoice);
  if (mProbe && frame != mLastLogged) {
    mLastLogged = frame;
    Say( "OCARINA_DIALOG frame=%llu phase=%d state=%u mode=%u next=%u choice=%u draw[c,d,e]=%u,%u,%u\n",
                 static_cast<unsigned long long>(frame), static_cast<int>(mPhase), state, mode,
                 next, choice, static_cast<unsigned>(U8(mMemory, kMessageContext + 0xCU)),
                 static_cast<unsigned>(U8(mMemory, kMessageContext + 0xDU)),
                 static_cast<unsigned>(U8(mMemory, kMessageContext + 0xEU)));
  }
  ++mFramesInPhase;
  if (mPhase == Phase::Opening) {  // StartTextbox returned; wait for the choice state.
    mPhase = Phase::Open;
    mFramesInPhase = 0;
    mSawChoice = false;
    mChosen = -1;
  }
  if (mPhase == Phase::Open) {
    if (state == kStateChoice) {
      mSawChoice = true;
      if (confirmPressed && choice <= 1U) {
        mChosen = static_cast<int>(choice);
        mPhase = Phase::Closing;
        mFramesInPhase = 0;
      }
    } else if (state == 0U && (mSawChoice || mFramesInPhase > 120U)) {
      Finish(Result::Failed);  // box vanished (or never opened) without a confirmed choice
    } else if (mFramesInPhase > 3600U) {
      Finish(Result::Failed);
    }
    return;
  }
  if (mPhase == Phase::Closing) {
    // Let the game process the confirmation; with no owning actor nothing else
    // closes the box, so do what Message_CloseTextbox (0x2E9A1C) does.
    if (mFramesInPhase == 8U && state != 0U && state != 0xEU) {
      mMemory.Write32(kMessageContext + kOffNext, 3U);
      mMemory.Write8(kMessageContext + kOffState, 0xEU);
      if (mProbe) Say( "OCARINA_DIALOG closing box (state was %u)\n", static_cast<unsigned>(state));
    }
    if (state == 0U || mFramesInPhase > 240U) Finish(mChosen == 0 ? Result::Yes : Result::No);
  }
}

}  // namespace Oot3dNativeGame
