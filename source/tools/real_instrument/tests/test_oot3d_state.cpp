#include "real_instrument/oot3d_state.h"
#include "test_support.h"
#include <unordered_map>
struct Memory {
    std::unordered_map<uint32_t,uint32_t> words;
    bool Read32(uint32_t p,uint32_t* out){auto it=words.find(p);if(it==words.end())return false;*out=it->second;return true;}
    bool Read16(uint32_t p,uint16_t* out){uint32_t n;if(!Read32(p,&n))return false;*out=static_cast<uint16_t>(n);return true;}
    bool Read8(uint32_t p,uint8_t* out){uint32_t n;if(!Read32(p,&n))return false;*out=static_cast<uint8_t>(n);return true;}
};
Memory fixture(){
    Memory m;m.words={{0x5043E0,0x600000},{0x5093F8,12},{0x50941C,0},
        {0x600100,3},{0x602B80,1},{0x602B82,0xFF},{0x6020AC,0},{0x587A14,0x11000},
        {0x4D53C8,6},{0x4D53DC,6},{0x4D541C,0x500000},{0x4D5430,0x500100},
        {0x50A3B0,96},{0x50A3C4,100},{0x53CB54,0x1000},{0x53CB64,0x10000}};
    const std::vector<std::vector<uint32_t>> notes={
        {3,4,2,3,4,2},{2,1,4,2,1,4},{0,4,3,2,3,2},{0,1,0,2,1,0},
        {4,3,2,4,3,2},{2,0,1,2,0,1},{1,0,1,0,2,1,2,1},{3,2,2,0,3,2,1},
        {1,2,3,1,2,3},{0,1,4,0,1,4},{0,1,2,2,3},{4,2,4,2,3,4}};
    const uint32_t items[]={96,99,90,93,97,100,91,94,98,101,92,95};
    for(uint32_t index=0;index<notes.size();++index){
        uint32_t ptr=0x510000+index*0x100;
        // Retain the old fixture addresses used by corrupt-pointer tests.
        if(index==0)ptr=0x500000;if(index==5)ptr=0x500100;
        m.words[0x4D53C8+index*4]=static_cast<uint32_t>(notes[index].size());
        m.words[0x4D541C+index*4]=ptr;m.words[0x50A3B0+index*4]=items[index];
        m.words[0x53C9D4+items[index]*4]=1U<<(items[index]-84);
        for(uint32_t j=0;j<notes[index].size();++j)m.words[ptr+j*4]=notes[index][j];
    }
    return m;
}
int main(){
    auto m=fixture();auto s=real_instrument::ReadOot3dOcarinaState(m,true,false);
    CHECK(s.canPlay);CHECK(s.songs[5].learned);CHECK(s.songs[0].learned);
    CHECK(s.songs[5].buttons[0]==0x800);CHECK(s.songs[0].buttons[0]==0x400);
    CHECK(!real_instrument::ReadOot3dOcarinaState(m,false,false).canPlay);
    CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,true).canPlay);
    for(auto value:{0U,1U,2U,11U}){m=fixture();m.words[0x5093F8]=value;CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);}
    for(auto value:{2U,14U}){m=fixture();m.words[0x602B80]=value;CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);}
    m=fixture();m.words[0x602B82]=4;CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);
    m=fixture();m.words[0x587A14]=0;s=real_instrument::ReadOot3dOcarinaState(m,true,false);CHECK(s.canPlay);CHECK(!s.songs[5].learned);
    m=fixture();m.words.erase(0x500100);CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);
    m=fixture();m.words[0x500100]=4;CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);
    m=fixture();m.words[0x6020AC]=0xFFFFFF00;CHECK(!real_instrument::ReadOot3dOcarinaState(m,true,false).canPlay);
}
