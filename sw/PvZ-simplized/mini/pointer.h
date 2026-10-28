#pragma once
#include <stdint.h>
// Virtual mouse click packet: valid[31], right[18], left[17], y[16:9], x[8:0].
// Coordinates are in the 320x240 game image, not the NVBoard window.
inline uint32_t pvz_click(unsigned x,unsigned y,bool right=false){
    return 0x80000000u|(right?1u<<18:1u<<17)|(y<<9)|x;
}
inline bool pvz_map_pointer(int x,int y,int left,int top,int width,int height,unsigned& gx,unsigned& gy){
    if(width<=0||height<=0||x<left||y<top||x-left>=width||y-top>=height)return false;
    gx=unsigned(x-left)*320/unsigned(width);gy=unsigned(y-top)*240/unsigned(height);return true;
}
