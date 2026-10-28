#pragma once
#include "game.h"
#include "art.h"
// Reference renderer for native preview/tests only, never linked into RTL driver.
inline uint32_t tetris_pixel(const TetrisScene& s,unsigned x,unsigned y){
    uint16_t tile=s.tiles[(y/16)*40+x/16];
    unsigned px=x%16,py=y%16,c=tile&255,fg=(tile>>8)&15;
    uint32_t rgb=tetris_palette[(tile>>12)&15];
    if(c==128){
        rgb=tetris_palette[fg];
        if(px==0||py==0)rgb=0xffffff;
        if(px==15||py==15)rgb=(tetris_palette[fg]>>1)&0x7f7f7f;
    }else if(c==129){
        if(px==1||px==14||py==1||py==14)rgb=tetris_palette[fg];
    }else if(c>=32&&c<96&&px>=3&&px<13&&py>=1&&py<15){
        if(tetris_font[c-32][(py-1)/2]&(1u<<(4-(px-3)/2)))rgb=tetris_palette[fg];
    }
    return rgb;
}
