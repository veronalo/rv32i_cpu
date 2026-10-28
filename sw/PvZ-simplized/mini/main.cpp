#include "game.h"
#include "../generated/assets.h"
#define VIDEO32(offset) (*(volatile uint32_t*)(0x20000000u+(offset)))
#define IO32(offset) (*(volatile uint32_t*)(0x30000000u+(offset)))
static Game game;
static Scene scene;
extern "C" int main(){
    for(unsigned i=0;i<sizeof(pvz_background)/4;++i)VIDEO32(i*4)=pvz_background[i];
    for(unsigned i=0;i<sizeof(pvz_title)/4;++i)VIDEO32(0x60000+i*4)=pvz_title[i];
    for(unsigned i=0;i<sizeof(pvz_selector)/4;++i)VIDEO32(0x80000+i*4)=pvz_selector[i];
    for(unsigned i=0;i<sizeof(pvz_level2)/4;++i)VIDEO32(0xc0000+i*4)=pvz_level2[i];
    for(unsigned i=0;i<sizeof(pvz_atlas)/4;++i)
        VIDEO32(i<32768?0x20000+i*4:0xa0000+(i-32768)*4)=pvz_atlas[i];
    for(unsigned i=0;i<256;++i)VIDEO32(0x40000+i*4)=pvz_palette[i];
    pvz_reset(game,true);
    for(uint32_t frame=1;;++frame){
        while(VIDEO32(0x41400)&1){}
        uint32_t click=IO32(0x20);
        if(click>>31)IO32(0x24)=1;
        pvz_step(game,uint16_t(IO32(0)),click);
        pvz_scene(game,scene);
        const uint32_t* words=reinterpret_cast<const uint32_t*>(scene.objects);
        for(unsigned i=0;i<scene.count*4;++i)VIDEO32(0x41000+i*4)=words[i];
        VIDEO32(0x41408)=scene.count;
        VIDEO32(0x41404)=scene.background;
        VIDEO32(0x41400)=1;
        IO32(0x14)=pvz_hash(game);IO32(0x18)=pvz_metrics(game);IO32(0x10)=frame;
    }
}
