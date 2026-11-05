#pragma once
#include "types.h"
#include <optional>
#include <vector>
namespace real_instrument {
struct SongDefinition { std::string id; std::vector<float> semitones; };
struct SongHint { std::string id;size_t matchedNotes=0,totalNotes=0;float score=0;std::string alternative;std::vector<std::string> candidates; };
class SongMatcher {
public:
    explicit SongMatcher(std::vector<SongDefinition> catalog, bool assisted=true);
    std::optional<SongResult> Push(NoteEvent note);
    std::optional<SongHint> Finish();
    bool HasNotes()const{return !notes_.empty();}
    void Reset();
    std::optional<SongHint> Hint()const{return hint_;}
private:
    std::vector<SongDefinition> catalog_;
    std::vector<NoteEvent> notes_;
    bool assisted_, latched_=false;
    std::optional<SongHint> hint_;
};
}
