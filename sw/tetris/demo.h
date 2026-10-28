#pragma once
#include <stdint.h>
// Fixed inputs shared by reference and real CPU tests, not host game logic.
inline uint16_t tetris_demo_keys(unsigned frame){
    // Fixed placement script after restart, exercising actual line clears.
    // These are constants, not an AI/controller reading the live Game state.
    if(frame==300)return 256;
    if(frame>=302){
        static const int8_t placements[12][2]={
            {0,7},{0,-1},{0,7},{1,6},{0,2},{0,0},
            {0,3},{1,4},{1,4},{3,3},{1,3},{0,1}
        };
        unsigned n=(frame-302)/32,t=(frame-302)%32;
        if(n>=12)return 0;
        if(t<6&&!(t&1)&&t/2<unsigned(placements[n][0]))return 17;
        int shift=int(placements[n][1])-3;
        unsigned distance=unsigned(shift<0?-shift:shift);
        if(t>=8&&!(t&1)&&(t-8)/2<distance)return shift<0?64:128;
        if(t==28)return 1;
        return 0;
    }
    if(frame==0||frame==90||frame==105)return 8; // start, pause, resume
    if(frame>=115&&frame<120)return 64;
    if(frame>=30&&frame<36)return 128;
    if(frame==15||frame==122)return 17; // shared keyboard W: UP + JUMP alias
    if(frame==45||frame==75||frame==130||frame==160||frame==190)return 1;
    if(frame>=200&&frame<220)return 32;
    if(frame==230)return 256; // restart
    if(frame==260)return 1;
    return 0;
}
