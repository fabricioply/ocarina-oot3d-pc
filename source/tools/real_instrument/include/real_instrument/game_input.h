#pragma once
#include "types.h"
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
namespace real_instrument {
struct NativeSongInput {bool learned=false;std::vector<uint32_t> buttons{};};
struct OcarinaGameState {
    bool canPlay=false;
    uint64_t context=0;
    std::array<NativeSongInput,12> songs;
};
struct SongChoice {std::string id;bool learned=false;};
struct SongConfirmation {
    uint64_t token=0;
    bool sariaLearned=false,stormsLearned=false;
    std::vector<SongChoice> choices; // the "try again" option is index choices.size()
};
struct OcarinaInputUpdate {
    uint32_t buttons=0;
    bool listen=false;
    float audioGain=1;
    uint64_t generation=0;
};
// No guest-memory writes: recognized phrases are replayed as ordinary native
// controller notes, leaving event selection to the game.
class OcarinaGameInput {
public:
    OcarinaInputUpdate Update(const OcarinaGameState& state,bool manualInput,double seconds);
    void Recognized(SongResult result);
    void OfferConfirmation();
    // Offers the given candidates (learned ones only); false if none can be played.
    bool OfferConfirmation(const std::vector<std::string>& ids);
    std::optional<SongConfirmation> Confirmation()const;
    bool ResolveConfirmation(uint64_t token,int choice,double replayDelaySeconds=0); // index into choices, or choices.size()=retry
    void Reset();
private:
    enum class Phase {Idle,Listening,Confirm,Replay,AwaitClose,Manual};
    Phase phase_=Phase::Idle;
    uint64_t context_=0,generation_=0;
    double replayStart_=0,lastTime_=-1,replayDelay_=0;
    std::optional<SongResult> pending_;
    std::vector<uint32_t> replay_;
    bool sariaLearned_=false,stormsLearned_=false;
    std::array<bool,12> learned_{};
    std::vector<std::string> offered_;
};
}

