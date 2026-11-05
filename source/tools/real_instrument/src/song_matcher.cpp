#include "real_instrument/song_matcher.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
namespace real_instrument {
namespace {
bool Connected(const std::vector<NoteEvent>& notes,size_t from,size_t to) {
    for(size_t i=from;i<to;++i)if(notes[i+1].startedAt-notes[i].endedAt>0.03)return false;
    return true;
}
bool Ornament(const std::vector<NoteEvent>& notes,size_t index,size_t before,size_t after) {
    const auto& n=notes[index];
    const float low=std::min(notes[before].midiPitch,notes[after].midiPitch);
    const float high=std::max(notes[before].midiPitch,notes[after].midiPitch);
    const float distance=std::min(std::abs(n.midiPitch-notes[before].midiPitch),std::abs(n.midiPitch-notes[after].midiPitch));
    // Brief neighboring pitches and intermediate steps of a slide can be
    // discarded. Stable unrelated notes may not be deleted to invent a song.
    if(n.endedAt-n.startedAt<=0.18 && (distance<=1.5f || (n.midiPitch>=low-0.5f && n.midiPitch<=high+0.5f)))return true;
    // A sustained note can have been split by a short excursion and returned
    // to its original pitch. Accept duplicate fragments only across continuous
    // sound, not two deliberately rearticulated notes separated by a pause.
    for(size_t anchor:{before,after}) {
        if(std::abs(n.midiPitch-notes[anchor].midiPitch)>0.35f)continue;
        size_t first=std::min(index,anchor),last=std::max(index,anchor);
        if(!Connected(notes,first,last))continue;
        bool adjacent=true;
        for(size_t i=first+1;i<last;++i) {
            if(notes[i].endedAt-notes[i].startedAt>0.18
                || std::abs(notes[i].midiPitch-n.midiPitch)>1.5f)adjacent=false;
        }
        if(adjacent)return true;
    }
    return false;
}
bool DistinctiveRise(const std::vector<NoteEvent>& notes,const SongDefinition& song,
    size_t begin,const std::vector<size_t>& selected) {
    const bool saria=song.id=="Saria's Song",storms=song.id=="Song of Storms";
    if(!saria && !storms)return true;
    // Absolute tuning remains free. Check the two adjacent rises in EACH
    // three-note motif, so averaging can't hide contradictory halves.
    for(size_t motif=0;motif+2<selected.size();motif+=3){
        float first=notes[begin+selected[motif+1]].midiPitch-notes[begin+selected[motif]].midiPitch;
        float final=notes[begin+selected[motif+2]].midiPitch-notes[begin+selected[motif+1]].midiPitch;
        if(saria && !(first>=2.5f && final>=0.5f && final<=3.5f && first-final>=0.5f))return false;
        if(storms && !(first>=1.f && final>=6.f && final-first>=2.5f))return false;
    }
    return true;
}
bool RisingMotifs(const std::vector<NoteEvent>& notes){
    if(notes.size()<6)return false;
    const size_t begin=notes.size()-6;
    for(size_t motif:{0U,3U}){
        float a=notes[begin+motif].midiPitch,b=notes[begin+motif+1].midiPitch,c=notes[begin+motif+2].midiPitch;
        if(b-a<2.f || b-a>6.5f || c-b<0.5f || c-b>12.f || c-a<4.f || c-a>16.f)return false;
    }
    for(size_t i=0;i<3;++i)if(std::abs(notes[begin+i].midiPitch-notes[begin+i+3].midiPitch)>2.f)return false;
    return true;
}
float AlignmentScore(const std::vector<NoteEvent>& notes,const SongDefinition& song,
    size_t begin,size_t span,const std::vector<size_t>& selected,bool assisted,bool requireSignature) {
    if(requireSignature && !DistinctiveRise(notes,song,begin,selected))return 0;
    const size_t length=selected.size();
    std::vector<float> offsets;offsets.reserve(length);
    for(size_t i=0;i<length;++i)offsets.push_back(notes[begin+selected[i]].midiPitch-song.semitones[i]);
    std::sort(offsets.begin(),offsets.end());
    float offset=offsets[length/2];
    if(length%2==0)offset=(offset+offsets[length/2-1])*0.5f;
    const float tolerance=assisted?1.5f:0.35f;
    double error=0;
    for(size_t i=0;i<length;++i){
        float residual=notes[begin+selected[i]].midiPitch-song.semitones[i]-offset;
        if(!std::isfinite(residual)||std::abs(residual)>tolerance)return 0;
        error+=residual*residual;
    }
    for(size_t i=0;i+1<length;++i){
        for(size_t skipped=selected[i]+1;skipped<selected[i+1];++skipped)
            if(!Ornament(notes,begin+skipped,begin+selected[i],begin+selected[i+1]))return 0;
    }
    const float rms=static_cast<float>(std::sqrt(error/static_cast<double>(length)));
    return 1.0f-(assisted?0.2f:0.3f)*rms-0.035f*static_cast<float>(span-length);
}
// Assisted fallback: "parecido com o esperado". Each interval must go the same
// direction as the song's (repeats stay repeats) and may be off by several
// semitones. Used only to PROPOSE candidates for the player to confirm.
float LooseScore(const std::vector<NoteEvent>& notes,const SongDefinition& song) {
    const size_t length=song.semitones.size();
    if(length<5||length>notes.size())return 0;
    const size_t begin=notes.size()-length;
    double total=0;
    for(size_t i=0;i+1<length;++i){
        float expected=song.semitones[i+1]-song.semitones[i];
        float observed=notes[begin+i+1].midiPitch-notes[begin+i].midiPitch;
        if(std::abs(expected)<0.5f){if(std::abs(observed)>1.6f)return 0;}
        else if(observed*expected<=0 || std::abs(observed)<1.f)return 0;
        float diff=std::abs(observed-expected);
        if(diff>5.f)return 0;
        total+=diff;
    }
    return 1.0f-static_cast<float>(total/static_cast<double>(length-1))/6.0f;
}
float BestAlignment(const std::vector<NoteEvent>& notes,const SongDefinition& song,bool assisted,bool requireSignature=true) {
    const size_t length=song.semitones.size();
    if(length<3||length>notes.size()||length>32)return 0;
    float best=0;
    for(size_t span=length;span<=std::min(notes.size(),length+(assisted?2U:0U));++span){
        const size_t begin=notes.size()-span;
        std::vector<size_t> selected;selected.reserve(length);selected.push_back(0);
        // At most two insertions: bounded combinations, not unconstrained
        // subsequence search. The first and last observed notes must match.
        std::function<void(size_t)> visit=[&](size_t next){
            if(selected.size()==length-1){
                if(next>span-1)return;
                selected.push_back(span-1);
                best=std::max(best,AlignmentScore(notes,song,begin,span,selected,assisted,requireSignature));
                selected.pop_back();return;
            }
            size_t remaining=length-selected.size()-1;
            for(size_t i=next;i+remaining<span;++i){selected.push_back(i);visit(i+1);selected.pop_back();}
        };
        visit(1);
    }
    return best;
}
}
SongMatcher::SongMatcher(std::vector<SongDefinition> catalog,bool assisted) : catalog_(std::move(catalog)),assisted_(assisted) {}
std::optional<SongResult> SongMatcher::Push(NoteEvent n) {
    if(latched_) return {};
    if(!std::isfinite(n.midiPitch) || !std::isfinite(n.startedAt) || !std::isfinite(n.endedAt)
        || !std::isfinite(n.confidence) || n.confidence<0.75f || n.endedAt<n.startedAt) { Reset(); return {}; }
    if(n.endedAt-n.startedAt<0.06)return {};
    if(!notes_.empty() && (n.startedAt<notes_.back().endedAt || n.startedAt-notes_.back().endedAt>1.25)) notes_.clear();
    notes_.push_back(n);
    while(notes_.size()>32 || (!notes_.empty() && n.endedAt-notes_.front().startedAt>20)) notes_.erase(notes_.begin());
    float best=0,second=0,hintRank=0,uncertainRise=0; std::string id;hint_.reset();
    for(const auto& song:catalog_) {
        for(size_t count=2;count<=std::min(song.semitones.size(),notes_.size());++count){
            SongDefinition prefix=song;prefix.semitones.resize(count);
            // Two notes can provide a display hint, never activate a song.
            float score=0;
            if(count==2){
                const auto& a=notes_[notes_.size()-2];const auto& b=notes_.back();
                float error=std::abs((b.midiPitch-a.midiPitch)-(prefix.semitones[1]-prefix.semitones[0]));
                if(error<=(assisted_?2.0f:0.7f))score=1.0f-error*0.2f;
            }else score=BestAlignment(notes_,prefix,assisted_);
            float rank=score*static_cast<float>(count)/static_cast<float>(song.semitones.size());
            if(score>=0.8f && rank>hintRank){hintRank=rank;hint_=SongHint{song.id,count,song.semitones.size(),score};}
        }
        float score=BestAlignment(notes_,song,assisted_);
        if(assisted_ && (song.id=="Saria's Song" || song.id=="Song of Storms") && score<0.8f){
            float withoutSignature=BestAlignment(notes_,song,assisted_,false);
            if(withoutSignature>=0.8f)uncertainRise=std::max(uncertainRise,withoutSignature);
        }
        if(score>best) {second=best;best=score;id=song.id;} else second=std::max(second,score);
    }
    const bool risingPair=assisted_ && RisingMotifs(notes_);
    const bool clearPair=(id=="Saria's Song" || id=="Song of Storms") && best>=0.9f && best-second>=0.1f;
    if(risingPair && !clearPair){
        hint_=SongHint{"Saria's Song",6,6,std::max(best,uncertainRise),"Song of Storms",{"Saria's Song","Song of Storms"}};
        notes_.clear();return {};
    }
    if(best>=0.8f && best-second>=0.1f) {latched_=true;return SongResult{id,best};}
    if(uncertainRise>=0.8f){
        hint_=SongHint{"Saria's Song",6,6,uncertainRise,"Song of Storms",{"Saria's Song","Song of Storms"}};
        // A retry starts a new phrase. Otherwise the tail of this rejected
        // attempt can combine with its beginning and activate another song.
        notes_.clear();
    }
    return {};
}
std::optional<SongHint> SongMatcher::Finish() {
    // Called when the player stopped (pause after the phrase). A full-length
    // phrase that loosely resembles songs is offered for confirmation.
    std::optional<SongHint> offer;
    if(assisted_ && !latched_ && notes_.size()>=5){
        std::vector<std::pair<float,std::string>> ranked;
        for(const auto& song:catalog_){
            if(notes_.size()>song.semitones.size()+1)continue;
            float score=LooseScore(notes_,song);
            if(score>=0.45f)ranked.emplace_back(score,song.id);
        }
        if(!ranked.empty()){
            std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.first>b.first;});
            SongHint h{ranked[0].second,0,0,ranked[0].first,"",{}};
            for(size_t i=0;i<ranked.size() && i<3;++i)h.candidates.push_back(ranked[i].second);
            h.alternative=h.candidates.size()>1?h.candidates[1]:std::string("-");
            offer=h;
        }
    }
    notes_.clear();
    return offer;
}
void SongMatcher::Reset() { notes_.clear();latched_=false;hint_.reset(); }
}
