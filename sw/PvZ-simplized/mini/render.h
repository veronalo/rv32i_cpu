#pragma once
// Software renderer for native preview / test reference only, never RV32I runtime.
#include "game.h"
#include "../generated/assets.h"
#include <vector>
inline void render(const Scene& scene,std::vector<uint32_t>& pixels){
    const auto* bg=reinterpret_cast<const uint8_t*>(scene.background==1?pvz_title:scene.background==2?pvz_selector:scene.background==3?pvz_level2:pvz_background);
    const auto* atlas=reinterpret_cast<const uint8_t*>(pvz_atlas);
    for(unsigned i=0;i<320*240;++i)pixels[i]=0xff000000u|pvz_palette[bg[i]];
    for(const auto& object:scene.objects){
        if(!(object.texture>>31))continue;
        int x=int16_t(object.xy),y=int16_t(object.xy>>16);
        unsigned w=object.size&255,h=(object.size>>8)&255,offset=object.texture&0x3ffff;
        for(unsigned py=0;py<h;++py)for(unsigned px=0;px<w;++px){
            int dx=x+int(px),dy=y+int(py);if(dx<0||dy<0||dx>=320||dy>=240)continue;
            auto index=atlas[offset+py*w+px];if(index)pixels[dy*320+dx]=0xff000000u|pvz_palette[index];
        }
    }
}
