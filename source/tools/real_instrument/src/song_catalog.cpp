#include "real_instrument/song_catalog.h"
namespace real_instrument {
const std::array<NativeSongDefinition,12>& NativeSongCatalog() {
    // Verified against locally normalized OoT3D tables 0x4D53C8/0x4D541C.
    static const std::array<NativeSongDefinition,12> songs{{
        {"Zelda's Lullaby",{3,4,2,3,4,2},6,96},
        {"Sun's Song",{2,1,4,2,1,4},6,99},
        {"Minuet of Forest",{0,4,3,2,3,2},6,90},
        {"Requiem of Spirit",{0,1,0,2,1,0},6,93},
        {"Epona's Song",{4,3,2,4,3,2},6,97},
        {"Song of Time",{2,0,1,2,0,1},6,100},
        {"Bolero of Fire",{1,0,1,0,2,1,2,1},8,91},
        {"Nocturne of Shadow",{3,2,2,0,3,2,1},7,94},
        {"Saria's Song",{1,2,3,1,2,3},6,98},
        {"Song of Storms",{0,1,4,0,1,4},6,101},
        {"Serenade of Water",{0,1,2,2,3},5,92},
        {"Prelude of Light",{4,2,4,2,3,4},6,95}
    }};
    return songs;
}
std::vector<SongDefinition> SongCatalog() {
    constexpr float pitches[]={62,65,69,71,74};
    std::vector<SongDefinition> result;
    for(const auto& native:NativeSongCatalog()) {
        SongDefinition song{native.id,{}};
        for(uint32_t i=0;i<native.length;++i)
            song.semitones.push_back(pitches[native.notes[i]]-pitches[native.notes[0]]);
        result.push_back(std::move(song));
    }
    return result;
}
}
