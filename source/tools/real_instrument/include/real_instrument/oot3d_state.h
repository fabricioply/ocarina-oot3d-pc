#pragma once
#include "game_input.h"
#include "song_catalog.h"
#include <limits>
namespace real_instrument {
// These addresses belong to TriAevum's canonical execution family, not the
// raw USA dump. Forge normalizes the user's source before executing it.
template<class Memory>
OcarinaGameState ReadOot3dOcarinaState(Memory& memory,bool topScreen,bool hostUiOpen) {
    OcarinaGameState result;
    if(!topScreen || hostUiOpen)return result;
    uint32_t scene=0,page=0,transition=0,extension=0,flags=0;
    uint8_t mode=0;uint16_t action=0,recognition=0;
    if(!memory.Read32(0x005043E0U,&scene)||!scene||scene>UINT32_MAX-0x2B82U
        || !memory.Read32(0x005093F8U,&page)||page!=12
        || !memory.Read32(0x0050941CU,&transition)||transition
        || !memory.Read8(scene+0x100U,&mode)||mode!=3
        || !memory.Read16(scene+0x2B80U,&action)||action>1
        || !memory.Read16(scene+0x2B82U,&recognition)||recognition!=0xFF
        || !memory.Read32(scene+0x20ACU,&extension))return result;
    if(extension && (extension>UINT32_MAX-0x1714U
        || !memory.Read32(extension+0x1714U,&flags)||(flags&0x01000000U)))return result;
    uint32_t learned=0;
    if(!memory.Read32(0x00587A14U,&learned))return result;
    // Note types in the game's UI/song tables: D4,F4,A4,B4,D5.
    // Native 3DS buttons: L,R,Y,X,A. No host keyboard simulation.
    constexpr std::array<uint32_t,5> buttons{0x200U,0x100U,0x800U,0x400U,0x1U};
    const auto& catalog=NativeSongCatalog();
    for(uint32_t index=0;index<catalog.size();++index){
        const auto& expected=catalog[index];auto& out=result.songs[index];
        uint32_t count=0,notes=0,flagIndex=0,mask=0;
        if(!memory.Read32(0x004D53C8U+index*4U,&count)||count!=expected.length
            ||!memory.Read32(0x004D541CU+index*4U,&notes)||!notes||notes>UINT32_MAX-32U
            ||!memory.Read32(0x0050A3B0U+index*4U,&flagIndex)||flagIndex!=expected.flagItem
            ||!memory.Read32(0x0053C9D4U+flagIndex*4U,&mask)||!mask)return {};
        out.learned=(learned&mask)!=0;
        for(uint32_t i=0;i<count;++i){uint32_t type=0;
            if(!memory.Read32(notes+i*4U,&type)||type!=expected.notes[i]||type>=buttons.size())return {};
            out.buttons.push_back(buttons[type]);
        }
    }
    result.context=scene;result.canPlay=true;return result;
}
}
