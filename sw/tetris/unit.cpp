#include "game.h"
#include "render.h"
#include "demo.h"
#include <cassert>
#include <cstdio>
static void pulse(Tetris& g,uint16_t keys){tetris_step(g,keys);tetris_step(g,0);}
static void clear(Tetris& g){for(auto& row:g.board)for(auto& cell:row)cell=0;}
int main(){
    for(unsigned p=0;p<7;++p)for(unsigned r=0;r<4;++r){
        unsigned count=0,mask=tetris_shape(p,r);while(mask){count+=mask&1;mask>>=1;}assert(count==4);
    }
    Tetris g;tetris_reset(g);assert(g.mode==TETRIS_TITLE);
    uint32_t start=tetris_hash(g);tetris_step(g,0);assert(tetris_hash(g)==start);
    pulse(g,TK_ENTER);assert(g.mode==TETRIS_PLAY);
    pulse(g,TK_ENTER);assert(g.mode==TETRIS_PAUSE);
    int y=g.y;for(unsigned i=0;i<60;++i)tetris_step(g,TK_DOWN);assert(g.y==y);
    pulse(g,TK_ENTER);assert(g.mode==TETRIS_PLAY);
    // Each seven-bag contains every tetromino exactly once, across refills.
    tetris_reset(g,false);
    for(unsigned bag=0;bag<5;++bag){
        unsigned seen=0;
        for(unsigned i=0;i<7;++i){seen|=1u<<g.piece;clear(g);pulse(g,TK_DROP);assert(g.mode==TETRIS_PLAY);}
        assert(seen==127);
    }
    tetris_reset(g,false);g.piece=0;g.rotation=1;g.x=-2;g.y=0;
    assert(tetris_fits(g,g.x,g.y,g.rotation));pulse(g,TK_ROTATE);
    assert(g.rotation==2&&tetris_fits(g,g.x,g.y,g.rotation)); // left wall kick
    tetris_reset(g,false);g.piece=2;
    pulse(g,TK_ROTATE|TK_DROP);assert(g.rotation==1&&g.locks==0); // W alias
    unsigned rotation=g.rotation;for(unsigned i=0;i<20;++i)tetris_step(g,TK_ROTATE|TK_DROP);
    assert(g.rotation==((rotation+1)&3)&&g.locks==0); // once per press
    tetris_reset(g,false);g.piece=1;
    for(unsigned i=0;i<60;++i)tetris_step(g,TK_LEFT);
    assert(g.x==-1&&tetris_fits(g,g.x,g.y,g.rotation));
    for(unsigned i=0;i<60;++i)tetris_step(g,TK_RIGHT);
    assert(g.x==7&&tetris_fits(g,g.x,g.y,g.rotation));
    // Four simultaneous lines, top rows compacted, score/level advancement.
    tetris_reset(g,false);g.piece=0;g.rotation=1;g.x=2;g.y=0;g.lines=8;
    for(unsigned row=16;row<20;++row)for(unsigned x=0;x<10;++x)g.board[row][x]=(x==4)?0:2;
    g.board[0][0]=7;pulse(g,TK_DROP);
    assert(g.lines==12&&g.level==2&&g.locks==1&&g.score>=800);
    assert(g.board[4][0]==7&&g.board[0][0]==0);
    // Two rows cleared together, no stale row at top.
    tetris_reset(g,false);g.piece=1;g.x=3;g.y=0;
    for(unsigned row=18;row<20;++row)for(unsigned x=0;x<10;++x)g.board[row][x]=(x==4||x==5)?0:3;
    pulse(g,TK_DROP);assert(g.lines==2&&g.score>=300);
    // Blocked next spawn -> game over, then Enter retries.
    tetris_reset(g,false);g.piece=1;g.x=-1;
    for(unsigned x=3;x<8;++x){g.board[0][x]=1;g.board[1][x]=1;}
    pulse(g,TK_DROP);assert(g.mode==TETRIS_OVER);pulse(g,TK_ENTER);assert(g.mode==TETRIS_PLAY&&g.locks==0);
    // Long deterministic stress: every live piece fits and cells remain valid.
    uint32_t random=0x12345678;
    for(unsigned i=0;i<50000;++i){
        random^=random<<13;random^=random>>17;random^=random<<5;
        uint16_t keys=0;
        if(g.mode==TETRIS_OVER)keys=TK_RESET;
        else switch(random%12){case 0:keys=TK_LEFT;break;case 1:keys=TK_RIGHT;break;
            case 2:keys=TK_ROTATE|TK_DROP;break;case 3:keys=TK_CCW;break;case 4:keys=TK_DROP;break;case 5:keys=TK_DOWN;break;}
        tetris_step(g,keys);
        if(g.mode==TETRIS_PLAY)assert(tetris_fits(g,g.x,g.y,g.rotation));
        for(auto& row:g.board)for(auto cell:row)assert(cell<=7);
    }
    TetrisScene scene;tetris_scene(g,scene);
    for(auto cell:scene.tiles)assert((cell&255)<130);
    for(unsigned frame=0;frame<600;++frame){if(!frame)tetris_reset(g);tetris_step(g,tetris_demo_keys(frame));}
    assert(g.mode==TETRIS_PLAY&&g.locks>=8&&g.lines>=2);
    std::printf("PASS: seven shapes/bag, collision/kicks, W alias/repeat, pause, 2/4-line clear, level, game over/restart, 50000 stress steps; RAM=%zu+%zu bytes\n",sizeof(g),sizeof(scene));
}
