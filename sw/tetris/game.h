#pragma once
#include <stdint.h>
enum { TETRIS_WIDTH=10,TETRIS_HEIGHT=20,TETRIS_COLS=40,TETRIS_ROWS=30 };
enum { TETRIS_TITLE,TETRIS_PLAY,TETRIS_PAUSE,TETRIS_OVER };
enum { TK_DROP=1,TK_CCW=2,TK_ENTER=8,TK_ROTATE=16,TK_DOWN=32,TK_LEFT=64,TK_RIGHT=128,TK_RESET=256 };
struct Tetris {
    uint8_t board[20][10],bag[7];
    uint32_t rng,bag_index,piece,next,rotation,mode,score,lines,level,locks;
    uint32_t previous_keys,gravity,repeat;
    int32_t x,y;
};
struct TetrisScene { uint16_t tiles[1200]; };
inline uint32_t tetris_metrics(const Tetris& g){
    return (g.mode<<24)|((g.locks&4095u)<<12)|(g.lines&4095u);
}
uint16_t tetris_shape(unsigned piece,unsigned rotation);
bool tetris_fits(const Tetris& g,int x,int y,unsigned rotation);
void tetris_reset(Tetris& g,bool title=true);
void tetris_step(Tetris& g,uint16_t keys);
void tetris_scene(const Tetris& g,TetrisScene& scene);
uint32_t tetris_hash(const Tetris& g);
