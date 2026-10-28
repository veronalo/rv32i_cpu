#include "game.h"
// Row-major 4x4 masks, bit (y*4+x); I,O,T,S,Z,J,L. No external game code/assets.
static const uint16_t shapes[7][4]={
    {0x00f0,0x4444,0x0f00,0x2222},
    {0x0066,0x0066,0x0066,0x0066},
    {0x0072,0x0262,0x0270,0x0232},
    {0x0036,0x0462,0x0360,0x0231},
    {0x0063,0x0264,0x0630,0x0132},
    {0x0071,0x0226,0x0470,0x0322},
    {0x0074,0x0622,0x0170,0x0223}
};
uint16_t tetris_shape(unsigned p,unsigned r){return shapes[p%7][r&3];}
static uint32_t random(Tetris& g){g.rng^=g.rng<<13;g.rng^=g.rng>>17;g.rng^=g.rng<<5;return g.rng;}
static unsigned draw(Tetris& g){
    if(g.bag_index>=7){
        for(unsigned i=0;i<7;++i)g.bag[i]=uint8_t(i);
        for(unsigned i=6;i>0;--i){unsigned j=random(g)%(i+1);uint8_t t=g.bag[i];g.bag[i]=g.bag[j];g.bag[j]=t;}
        g.bag_index=0;
    }
    return g.bag[g.bag_index++];
}
bool tetris_fits(const Tetris& g,int x,int y,unsigned r){
    uint16_t mask=tetris_shape(g.piece,r);
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)if(mask&(1u<<(row*4+col))){
        int bx=x+col,by=y+row;
        if(bx<0||bx>=10||by>=20)return false;
        if(by>=0&&g.board[by][bx])return false;
    }
    return true;
}
static void spawn(Tetris& g){
    g.piece=g.next;g.next=draw(g);g.x=3;g.y=0;g.rotation=0;g.gravity=0;g.repeat=0;
    if(!tetris_fits(g,g.x,g.y,0))g.mode=TETRIS_OVER;
}
void tetris_reset(Tetris& g,bool title){
    g={};g.rng=0x51f15eadu;g.bag_index=7;g.level=1;
    g.mode=title?TETRIS_TITLE:TETRIS_PLAY;g.next=draw(g);spawn(g);
}
static void lock_piece(Tetris& g){
    uint16_t mask=tetris_shape(g.piece,g.rotation);
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)if(mask&(1u<<(row*4+col))){
        int y=g.y+row;
        if(y<0){g.mode=TETRIS_OVER;return;}
        g.board[y][g.x+col]=uint8_t(g.piece+1);
    }
    ++g.locks;
    unsigned count=0;
    for(int row=19;row>=0;--row){
        bool full=true;for(unsigned x=0;x<10;++x)if(!g.board[row][x])full=false;
        if(full){
            ++count;
            for(int y=row;y>0;--y)for(unsigned x=0;x<10;++x)g.board[y][x]=g.board[y-1][x];
            for(unsigned x=0;x<10;++x)g.board[0][x]=0;
            ++row; // recheck the row shifted into the same position
        }
    }
    static const unsigned points[5]={0,100,300,500,800};
    g.score+=points[count]*g.level;g.lines+=count;g.level=1+g.lines/10;
    spawn(g);
}
static void rotate(Tetris& g,bool reverse){
    unsigned r=(g.rotation+(reverse?3:1))&3;
    // Small wall/floor kicks, deliberately not a claim of complete SRS rules.
    static const int8_t dx[7]={0,-1,1,-2,2,0,0},dy[7]={0,0,0,0,0,-1,-2};
    for(unsigned i=0;i<7;++i)if(tetris_fits(g,g.x+dx[i],g.y+dy[i],r)){
        g.x+=dx[i];g.y+=dy[i];g.rotation=r;return;
    }
}
void tetris_step(Tetris& g,uint16_t keys){
    uint16_t down=uint16_t(keys&~g.previous_keys);g.previous_keys=keys;
    if(down&TK_RESET){tetris_reset(g,false);g.previous_keys=keys;return;}
    if(g.mode==TETRIS_TITLE||g.mode==TETRIS_OVER){
        if(down&TK_ENTER){tetris_reset(g,false);g.previous_keys=keys;}
        return;
    }
    if(down&TK_ENTER){g.mode=g.mode==TETRIS_PAUSE?TETRIS_PLAY:TETRIS_PAUSE;return;}
    if(g.mode!=TETRIS_PLAY)return;
    if(down&TK_ROTATE)rotate(g,false);
    else if(down&TK_CCW)rotate(g,true);
    unsigned horizontal=keys&(TK_LEFT|TK_RIGHT);
    if(horizontal==TK_LEFT||horizontal==TK_RIGHT){
        ++g.repeat;
        if((down&horizontal)||g.repeat>=9){
            if(g.repeat>=9)g.repeat=6;
            int next_x=g.x+(horizontal==TK_LEFT?-1:1);
            if(tetris_fits(g,next_x,g.y,g.rotation))g.x=next_x;
        }
    }else g.repeat=0;
    // W is both UP and JUMP in the existing keyboard interface. Give rotation
    // precedence, so holding W does not accidentally hard-drop the new piece.
    if((down&TK_DROP)&&!(keys&TK_ROTATE)){
        while(tetris_fits(g,g.x,g.y+1,g.rotation)){++g.y;g.score+=2;}
        lock_piece(g);return;
    }
    unsigned interval=g.level<15?32-2*g.level:2;
    if(keys&TK_DOWN)interval=2;
    if(++g.gravity>=interval){
        g.gravity=0;
        if(tetris_fits(g,g.x,g.y+1,g.rotation)){++g.y;if(keys&TK_DOWN)++g.score;}
        else lock_piece(g);
    }
}
static uint16_t tile(unsigned c,unsigned fg=3,unsigned bg=0){return uint16_t(c|(fg<<8)|(bg<<12));}
static void text(TetrisScene& s,unsigned x,unsigned y,const char* p,unsigned fg=3,unsigned bg=0){
    while(*p&&x<40)s.tiles[y*40+x++]=tile(unsigned(*p++),fg,bg);
}
static void number(TetrisScene& s,unsigned x,unsigned y,uint32_t n){
    char digits[11];unsigned len=0;
    do{digits[len++]=char('0'+n%10);n/=10;}while(n&&len<10);
    while(len&&x<40)s.tiles[y*40+x++]=tile(unsigned(digits[--len]));
}
void tetris_scene(const Tetris& g,TetrisScene& s){
    for(unsigned i=0;i<1200;++i)s.tiles[i]=tile(' ');
    const char* title="TETRIS";
    for(unsigned i=0;i<6;++i)s.tiles[2*40+14+i*2]=tile(unsigned(title[i]),8+i);
    text(s,13,3,"RV32I PIXEL BLOCKS",4);
    for(unsigned y=5;y<27;++y)for(unsigned x=7;x<19;++x)
        s.tiles[y*40+x]=tile(' ',3,(x==7||x==18||y==5||y==26)?6:1);
    for(unsigned y=0;y<20;++y)for(unsigned x=0;x<10;++x)
        s.tiles[(y+6)*40+x+8]=g.board[y][x]?tile(128,7+g.board[y][x],1):tile('.',2,1);
    auto paint=[&](int x,int y,uint16_t shape,unsigned fg,unsigned glyph){
        for(int row=0;row<4;++row)for(int col=0;col<4;++col)
            if((shape&(1u<<(row*4+col)))&&x+col>=0&&x+col<10&&y+row>=0&&y+row<20)
                s.tiles[unsigned(y+row+6)*40+unsigned(x+col+8)]=tile(glyph,fg,1);
    };
    if(g.mode!=TETRIS_OVER){
        int ghost=g.y;while(tetris_fits(g,g.x,ghost+1,g.rotation))++ghost;
        paint(g.x,ghost,tetris_shape(g.piece,g.rotation),5,129);
        paint(g.x,g.y,tetris_shape(g.piece,g.rotation),g.piece+8,128);
    }
    text(s,22,6,"SCORE",4);number(s,22,7,g.score);
    text(s,22,9,"LINES",4);number(s,22,10,g.lines);
    text(s,22,12,"LEVEL",4);number(s,22,13,g.level);
    text(s,22,15,"NEXT",4);
    uint16_t next=tetris_shape(g.next,0);
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
        if(next&(1u<<(row*4+col)))s.tiles[(17+row)*40+22+col]=tile(128,g.next+8);
    text(s,22,22,"A/D  MOVE",4);text(s,22,23,"W/Z  ROTATE",4);
    text(s,22,24,"S    SOFT DROP",4);text(s,22,25,"SPACE  DROP",4);
    text(s,9,28,"ENTER PAUSE   R RESTART",4);
    if(g.mode!=TETRIS_PLAY){
        for(unsigned y=13;y<18;++y)for(unsigned x=8;x<18;++x)s.tiles[y*40+x]=tile(' ',3,2);
        text(s,8,14,g.mode==TETRIS_TITLE?"  READY?  ":g.mode==TETRIS_PAUSE?"  PAUSED  ":"GAME OVER!",3,2);
        text(s,8,16,"  ENTER   ",4,2);
    }
}
uint32_t tetris_hash(const Tetris& g){
    uint32_t h=2166136261u;
    auto add=[&](uint32_t v){h=(h^v)*16777619u;};
    for(unsigned y=0;y<20;++y)for(unsigned x=0;x<10;++x)add(g.board[y][x]);
    for(unsigned i=0;i<7;++i)add(g.bag[i]);
    add(g.rng);add(g.bag_index);add(g.piece);add(g.next);add(g.rotation);add(g.mode);
    add(g.score);add(g.lines);add(g.level);add(g.locks);add(g.previous_keys);add(g.gravity);add(g.repeat);
    add(uint32_t(g.x));add(uint32_t(g.y));return h;
}
