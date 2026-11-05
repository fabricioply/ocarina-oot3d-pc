#pragma once
#include "song_matcher.h"
#include <array>
#include <cstdint>
namespace real_instrument {
struct NativeSongDefinition {
    const char* id;
    std::array<uint8_t,8> notes;
    uint32_t length,flagItem;
};
// Canonical OoT3D table order. Note types: D4,F4,A4,B4,D5.
const std::array<NativeSongDefinition,12>& NativeSongCatalog();
std::vector<SongDefinition> SongCatalog();
}
