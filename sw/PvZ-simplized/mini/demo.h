#pragma once
#include <stdint.h>
#include "pointer.h"
inline uint32_t pvz_ui_demo_mouse(uint32_t frame){
    switch(frame){
    case 0:return pvz_click(160,220);
    case 2:return pvz_click(220,110); // locked mode -> explanation
    case 4:return pvz_click(160,167); // back
    case 6:case 70:return pvz_click(225,60);
    case 10:case 48:case 74:return pvz_click(52,20);
    case 12:case 50:case 76:return pvz_click(62,135);
    case 20:case 40:case 60:case 80:return pvz_click(285,20);
    case 30:return pvz_click(160,111); // resume
    case 44:return pvz_click(160,139); // restart
    case 64:return pvz_click(160,167); // selector
    default:return 0;
    }
}
inline uint32_t pvz_demo_mouse(uint32_t frame){
    if(frame==0)return pvz_click(160,220); // title -> selector
    if(frame==2)return pvz_click(225,60); // adventure -> level
    if(frame<3)return 0;
    frame-=3;
    if(frame==1||frame==5||frame==9)return pvz_click(52,20);
    if(frame==2)return pvz_click(62,135);
    if(frame==6)return pvz_click(92,135);
    if(frame==10)return pvz_click(122,135);
    // Click each freshly spawned sun, using only the deterministic input
    // schedule, not host game state. Sun #n appears at logic age 20+90*n.
    // Stop collection before victory: clicks on the victory banner retry.
    if(frame>=30&&frame<1200&&(frame-30)%90==0){
        uint32_t rng=0x12345678u;
        for(unsigned i=0;i<=(frame-30)/90;++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;}
        return pvz_click(35+rng%240+9,43+12+9);
    }
    return 0;
}
inline uint16_t pvz_demo_keys(uint32_t frame){
    if(frame==0||frame==2)return 8;
    if(frame<3)return 0;
    frame-=3;
    if(frame==2||frame==6||frame==10)return 1;
    if(frame==4||frame==8)return 128;
    if(frame>20&&frame%30==0)return 2;
    return 0;
}
inline uint32_t pvz_campaign_mouse(uint32_t frame){
    if(frame<1450)return pvz_demo_mouse(frame);
    if(frame==1450)return pvz_click(160,135); // next level / Dave enters
    if(frame==1480||frame==1510)return pvz_click(210,100); // two dialogue pages
    if(frame>=1572&&frame<=1594){
        unsigned local=frame-1572;
        if(local%4==0)return pvz_click(52,20);
        if(local%4==2){unsigned plant=local/4;return pvz_click(62+(plant%2)*30,86+(plant/2)*44);}
    }
    if(frame>=1601&&frame<2700&&(frame-1601)%90==0){
        uint32_t rng=0x12345678u;
        for(unsigned i=0;i<=(frame-1601)/90;++i){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;}
        return pvz_click(35+rng%240+9,64);
    }
    return 0;
}
