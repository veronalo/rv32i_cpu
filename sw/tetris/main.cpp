#include "game.h"
#include "art.h"
#define VIDEO(offset) (*(volatile uint32_t*)(0x20000000u+(offset)))
#define IO(offset) (*(volatile uint32_t*)(0x30000000u+(offset)))
static Tetris game;
static TetrisScene scene;
extern "C" int main(){
    for(unsigned i=0;i<16;++i)VIDEO(0x3400+i*4)=tetris_palette[i];
    for(unsigned i=0;i<64;++i)for(unsigned half=0;half<2;++half){
        uint32_t word=0;
        for(unsigned j=0;j<4;++j)word|=uint32_t(tetris_font[i][half*4+j])<<(j*8);
        VIDEO(0x3000+(i+32)*8+half*4)=word;
    }
    tetris_reset(game);
    // Let the first PS/2 make code arrive before sampling the title-screen keys.
    while(IO(0x0c)==0){}
    for(uint32_t frame=1;;++frame){
        while(VIDEO(0x2000)&1){}
        tetris_step(game,uint16_t(IO(0)));
        tetris_scene(game,scene);
        for(unsigned i=0;i<1200;++i)VIDEO(i*4)=scene.tiles[i];
        VIDEO(0x2000)=1;
        IO(0x14)=tetris_hash(game);
        IO(0x18)=tetris_metrics(game);
        IO(0x10)=frame;
    }
}
