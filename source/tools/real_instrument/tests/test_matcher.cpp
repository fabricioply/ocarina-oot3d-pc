#include "real_instrument/song_catalog.h"
#include "test_support.h"
#include <algorithm>
using namespace real_instrument;
std::optional<SongResult> play(SongMatcher& m,std::vector<float> pitches,double duration=0.3,double start=0) {
    std::optional<SongResult> result;
    double t=start;
    for(float p:pitches) { auto r=m.Push({t,t+duration,p,0.99f}); if(r) result=r; t+=duration+0.08; }
    return result;
}
int main() {
    // All twelve standard songs, including the 5/7/8-note warp melodies.
    const std::vector<std::pair<const char*,std::vector<float>>> melodies={
        {"Saria's Song",{65,69,71,65,69,71}},
        {"Epona's Song",{74,71,69,74,71,69}},
        {"Sun's Song",{69,65,74,69,65,74}},
        {"Song of Storms",{62,65,74,62,65,74}},
        {"Minuet of Forest",{62,74,71,69,71,69}},
        {"Bolero of Fire",{65,62,65,62,69,65,69,65}},
        {"Serenade of Water",{62,65,69,69,71}},
        {"Requiem of Spirit",{62,65,62,69,65,62}},
        {"Nocturne of Shadow",{71,69,69,62,71,69,65}},
        {"Prelude of Light",{74,69,74,69,71,74}}};
    CHECK(SongCatalog().size()==12);
    for(const auto& entry:melodies) {
        SongMatcher all(SongCatalog());auto recognized=play(all,entry.second);
        CHECK(recognized);CHECK(recognized->id==entry.first);
    }

    // Scope old negative cases to the original two-song catalog. Some of
    // those phrases intentionally approximate newly supported melodies.
    auto original=SongCatalog();
    original.erase(std::remove_if(original.begin(),original.end(),[](const SongDefinition& song){
        return song.id!="Song of Time" && song.id!="Zelda's Lullaby";
    }),original.end());
    for(float start:{48.f,60.f,72.f}) for(double duration:{0.15,0.6}) {
        SongMatcher m(SongCatalog());
        auto r=play(m,{start,start-7,start-4,start,start-7,start-4},duration);
        CHECK(r); CHECK(r->id=="Song of Time"); CHECK(r->confidence>=0.8f);
    }
    SongMatcher m(original);
    CHECK(play(m,{60.4f,52.6f,56.4f,59.6f,53.4f,55.6f}));
    m.Reset(); CHECK(!play(m,{60,53,56,60,53}));
    m.Reset(); CHECK(!play(m,{60,53,59,60,53,56}));
    m.Reset(); CHECK(!play(m,{56,53,60,56,53,60}));
    m.Reset(); CHECK(!play(m,{60,54,58,63,57,61}));
    SongMatcher precise(SongCatalog(),false);
    CHECK(!play(precise,{60.4f,52.6f,56.4f,59.6f,53.4f,55.6f}));
    auto duplicate=SongCatalog(); duplicate.push_back({"ambiguous",{0,-7,-4,0,-7,-4}});
    SongMatcher ambiguous(duplicate); CHECK(!play(ambiguous,{60,53,56,60,53,56}));
    m.Reset(); CHECK(!play(m,{60,53,56,60,53,56},5));
    m.Reset(); CHECK(play(m,{60,53,56,60,53,56}));
    CHECK(!play(m,{60,53,56,60,53,56}));
    m.Reset(); CHECK(play(m,{60,53,56,60,53,56}));
    SongMatcher lullaby(SongCatalog());
    auto z=play(lullaby,{71,74,69,71,74,69});
    CHECK(z);CHECK(z->id=="Zelda's Lullaby");
    lullaby.Reset();z=play(lullaby,{59,62,57,59,62,57});
    CHECK(z);CHECK(z->id=="Zelda's Lullaby");
    lullaby.Reset();CHECK(!play(lullaby,{71,74,69,71,74,73}));
    // Slide / grace note between stable melody notes. Full melody is still
    // required; a short intermediate pitch must not break the alignment.
    SongMatcher ornamented(SongCatalog());double t=0;std::optional<SongResult> found;
    const float embellished[]={60,53,54,53,56,60,53,56};
    const double lengths[]={0.4,0.3,0.10,0.3,0.4,0.4,0.3,0.4};
    for(size_t i=0;i<8;++i){auto r=ornamented.Push({t,t+lengths[i],embellished[i],0.99f});if(r)found=r;t+=lengths[i];}
    CHECK(found);CHECK(found->id=="Song of Time");
    // Bends around the target can be broader than precise tuning, with the
    // whole melody still matching a common transposition.
    SongMatcher assistedBends(SongCatalog());
    CHECK(play(assistedBends,{60.8f,52.2f,56.8f,59.2f,53.8f,55.2f}));
    // Stable wrong notes cannot be deleted to fabricate a song.
    SongMatcher wrongStable(SongCatalog());
    CHECK(!play(wrongStable,{60,53,59,56,60,53,56}));
    SongMatcher preciseOrnaments(SongCatalog(),false);t=0;found.reset();
    for(size_t i=0;i<8;++i){auto r=preciseOrnaments.Push({t,t+lengths[i],embellished[i],0.99f});if(r)found=r;t+=lengths[i];}
    CHECK(!found);
    SongMatcher hinting(original);
    hinting.Push({0,0.4,71,0.99f});
    CHECK(!hinting.Hint());
    hinting.Push({0.4,0.8,74,0.99f});
    auto hint=hinting.Hint();CHECK(hint);CHECK(hint->id=="Zelda's Lullaby");
    CHECK(hint->matchedNotes==2);CHECK(hint->totalNotes==6);
    hinting.Reset();CHECK(!hinting.Hint());
    // A compressed Storms-like rise must not be silently turned into Saria.
    // Both repeated triplets need the characteristic small/big final leap.
    SongMatcher uncertain(SongCatalog());
    CHECK(!play(uncertain,{60,63,67,60,63,67}));
    auto uncertainHint=uncertain.Hint();CHECK(uncertainHint);
    CHECK(uncertainHint->id=="Saria's Song");
    CHECK(uncertainHint->alternative=="Song of Storms");
    // A complete clear retry resolves without closing/restarting the session.
    auto resolved=play(uncertain,{60,63,72,60,63,72},0.3,3);
    CHECK(resolved);CHECK(resolved->id=="Song of Storms");
    for(float base:{48.f,62.f,74.f}){
        SongMatcher saria(SongCatalog()),storms(SongCatalog());
        auto small=play(saria,{base+.2f,base+3.8f,base+6.1f,base-.2f,base+4.2f,base+5.9f});
        CHECK(small);CHECK(small->id=="Saria's Song");
        auto large=play(storms,{base+.2f,base+2.8f,base+12.1f,base-.2f,base+3.2f,base+11.9f});
        CHECK(large);CHECK(large->id=="Song of Storms");
    }
    // Mixed evidence between the two halves must not choose Saria either.
    SongMatcher mixed(SongCatalog());
    CHECK(!play(mixed,{60,63,67,60,64,66}));
    // Actual local log: ascending motifs between Saria and Storms should
    // offer the pair, not disappear or activate a different song.
    SongMatcher logged(SongCatalog());
    CHECK(!play(logged,{55.0247f,59.2852f,64.4151f,53.7477f,59.7846f,64.6631f}));
    CHECK(logged.Hint());CHECK(logged.Hint()->alternative=="Song of Storms");
    // Fragments separated by a new attempt must not complete an old melody.
    SongMatcher attempts(SongCatalog());
    CHECK(!play(attempts,{71,74,69}));
    CHECK(!play(attempts,{71,74,69},0.3,3));
    // A tiny transition fragment is ignored, rather than wiping the phrase.
    SongMatcher tiny(SongCatalog());
    tiny.Push({0,.3,65,.99f});tiny.Push({.3,.6,69,.99f});
    tiny.Push({.6,.64,70,.99f});tiny.Push({.64,.94,71,.99f});
    tiny.Push({.94,1.24,65,.99f});tiny.Push({1.24,1.54,69,.99f});
    auto tinyResult=tiny.Push({1.54,1.84,71,.99f});CHECK(tinyResult);CHECK(tinyResult->id=="Saria's Song");
    // Assisted mode: a sloppy but same-shaped phrase must be PROPOSED, never lost.
    // Zelda's Lullaby (+3,+5,-9,+3,+5 ideal) played with off intervals.
    SongMatcher sloppy(SongCatalog());
    CHECK(!play(sloppy,{71,76,68,73,77,69}));
    auto sloppyHint=sloppy.Finish();CHECK(sloppyHint);CHECK(!sloppyHint->candidates.empty());
    // Wrong direction is not "similar": no proposal.
    SongMatcher wrongWay(SongCatalog());
    CHECK(!play(wrongWay,{60,62,64,66,68,70}));
    CHECK(!wrongWay.Finish());
    // Precise (non-assisted) mode never proposes.
    SongMatcher strict(SongCatalog(),false);
    play(strict,{71,76,68,73,77,69});
    CHECK(!strict.Finish());
}
