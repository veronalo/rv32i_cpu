#include "game.h"
#include "../generated/assets.h"

void pvz_reset(Game& g,bool title,unsigned level) {
    uint32_t* words=reinterpret_cast<uint32_t*>(&g);
    for(unsigned i=0;i<sizeof(g)/4;++i)words[i]=0;
    g.level=level?1:0;g.sun=g.level?600:300;g.cursor=1;g.rng=0x12345678u;
    if(title)g.mode=PVZ_TITLE;
}
static int center(unsigned col) { return 32+int(col)*30; }
static unsigned rows(const Game& g){return g.level?3:1;}
static unsigned target(const Game& g){return g.level?9:6;}
static int top(const Game& g,unsigned row){return g.level?64+int(row)*44:107;}
static void restart(Game& g,uint16_t keys){unsigned level=g.level;pvz_reset(g,false,level);g.previous_keys=keys;}
void pvz_step(Game& g,uint16_t keys,uint32_t click) {
    unsigned mx=click&511,my=(click>>9)&255;
    bool left=(click&0x80020000u)==0x80020000u&&mx<320&&my<240;
    bool right=(click&0x80040000u)==0x80040000u;
    unsigned down=keys&~g.previous_keys;
    if(down&256){restart(g,keys);return;}
    g.previous_keys=keys;
    bool menu=left&&mx>=267&&mx<317&&my>=15&&my<26;
    if(g.mode==PVZ_TITLE){
        if(left||(down&8))g.mode=PVZ_SELECT;
        return;
    }
    if(g.mode==PVZ_SELECT){
        if((down&8)||(left&&mx>=162&&mx<303&&my>=26&&my<76)){
            pvz_reset(g);g.previous_keys=keys;
        }else if(left&&((mx>=162&&my>=76&&my<185)||(mx>=200&&my>=217&&my<237))){g.mode=PVZ_HELP;}
        return;
    }
    if(g.mode==PVZ_HELP){
        if((down&8)||(left&&mx>=90&&mx<230&&my>=157&&my<177))g.mode=PVZ_SELECT;
        return;
    }
    if(g.mode==PVZ_DAVE){
        ++g.cutscene;
        if(right||(down&8)){g.mode=PVZ_READY;g.cutscene=0;}
        else if(left&&g.cutscene>=12){
            if(g.dialog==0){g.dialog=1;g.cutscene=24;}
            else {g.mode=PVZ_READY;g.cutscene=0;}
        }
        return;
    }
    if(g.mode==PVZ_READY){if(++g.cutscene>=60){g.mode=PVZ_PLAY;g.cutscene=0;}return;}
    if(g.mode==PVZ_PAUSE||g.mode==PVZ_WIN||g.mode==PVZ_LOSE){
        bool button=left&&mx>=90&&mx<230;
        if(button&&my>=157&&my<177){g.mode=PVZ_SELECT;g.selected=0;return;}
        if(g.mode==PVZ_PAUSE){
            if((down&8)||menu||(button&&my>=101&&my<121)){g.mode=PVZ_PLAY;return;}
            if(button&&my>=129&&my<149)restart(g,keys);
        }else if((down&8)||(button&&my>=125&&my<145)){
            if(g.mode==PVZ_WIN&&g.level==0){pvz_reset(g,false,1);g.mode=PVZ_DAVE;g.previous_keys=keys;}
            else restart(g,keys);
        }else if(menu){g.mode=PVZ_SELECT;}
        return;
    }
    if((down&8)||menu){g.mode=PVZ_PAUSE;return;}
    if(g.mode)return;
    if(right)g.selected=0;
    if(left){
        bool collected=false;
        // Suns are on top of the lawn; collecting one must never also plant.
        for(auto& s:g.suns)if(s.life&&int(mx)>=s.x-3&&int(mx)<s.x+21&&int(my)>=s.y-3&&int(my)<s.y+21){
            g.sun+=25;s.life=0;collected=true;break;
        }
        if(!collected){
            if(mx>=40&&mx<68&&my>=2&&my<40)g.selected=g.sun>=100?!g.selected:0;
            else if(mx>=17&&mx<287){
                for(unsigned row=0;row<rows(g);++row)if(int(my)>=top(g,row)&&int(my)<top(g,row)+44){
                    g.cursor=row*9+(mx-17)/30;
                    if(g.selected&&g.sun>=100&&!g.plants[g.cursor].hp){
                        g.sun-=100;g.plants[g.cursor]={300,0};g.selected=0;
                    }
                    break;
                }
            }
        }
    }
    ++g.age;
    if((down&64)&&g.cursor) --g.cursor;
    if((down&128)&&g.cursor<rows(g)*9-1) ++g.cursor;
    if((down&1)&&g.plants[g.cursor].hp==0&&g.sun>=100){
        g.sun-=100;g.plants[g.cursor].hp=300;g.plants[g.cursor].cooldown=0;
    }
    if(g.age%90==20){
        for(auto& s:g.suns)if(!s.life){
            g.rng^=g.rng<<13;g.rng^=g.rng>>17;g.rng^=g.rng<<5;
            s.x=35+int(g.rng%240);s.y=43;s.life=300;break;
        }
    }
    for(auto& s:g.suns)if(s.life){
        --s.life;if(s.y<110)++s.y;
        if(keys&2){g.sun+=25;s.life=0;}
    }
    if(g.spawned<target(g)&&g.age>=120+g.spawned*(g.level?100:180)){
        for(auto& z:g.zombies)if(z.hp<=0){z.x=305*256;z.hp=270;z.eating=0;z.row=int(g.spawned%rows(g));++g.spawned;break;}
    }
    for(unsigned c=0;c<rows(g)*9;++c){
        auto& p=g.plants[c];if(!p.hp)continue;
        if(p.cooldown)--p.cooldown;
        bool target=false;
        for(const auto& z:g.zombies)if(z.hp>0&&z.row==int(c/9)&&z.x/256>center(c%9)){target=true;break;}
        if(target&&!p.cooldown){
            for(auto& b:g.bullets)if(!b.active){b.x=center(c%9)+17;b.active=1;b.row=int(c/9);p.cooldown=45;break;}
        }
    }
    for(auto& b:g.bullets)if(b.active){
        b.x+=6;if(b.x>320){b.active=0;continue;}
        Zombie* hit=nullptr;
        for(auto& z:g.zombies)if(z.hp>0&&z.row==b.row&&b.x>=z.x/256-7&&b.x<=z.x/256+14)
            if(!hit||z.x<hit->x)hit=&z;
        if(hit){hit->hp-=20;b.active=0;if(hit->hp<=0)++g.kills;}
    }
    for(auto& z:g.zombies)if(z.hp>0){
        z.eating=0;
        for(int c=8;c>=0;--c)if(g.plants[z.row*9+c].hp>0&&z.x/256<=center(c)+13&&z.x/256>=center(c)-9){
            auto& plant=g.plants[z.row*9+c];z.eating=1;plant.hp-=3;
            if(plant.hp<0)plant.hp=0;
            break;
        }
        if(!z.eating)z.x-=102;
        if(z.x<10*256)g.mode=2;
    }
    if(g.kills==target(g))g.mode=PVZ_WIN;
}
static void emit(Scene& scene,unsigned asset,int x,int y){
    if(scene.count>=PVZ_OBJECTS)return;
    const Asset& a=pvz_assets[asset];
    scene.objects[scene.count++]={uint32_t(uint16_t(x))|(uint32_t(uint16_t(y))<<16),
        a.offset|0x80000000u,uint32_t(a.w)|(uint32_t(a.h)<<8),0};
}
static void counters(const Game& g,Scene& scene){
    unsigned money=g.sun>999?999:g.sun;
    emit(scene,SUN_DIGIT+money/100,10,26);emit(scene,SUN_DIGIT+(money/10)%10,18,26);emit(scene,SUN_DIGIT+money%10,26,26);
    emit(scene,DIGIT+g.kills,239,28);emit(scene,DIGIT+target(g),254,28);
}
void pvz_scene(const Game& g,Scene& scene){
    scene.count=0;
    scene.background=g.mode==PVZ_TITLE?1u:(g.mode==PVZ_SELECT||g.mode==PVZ_HELP)?2u:g.level?3u:0u;
    for(auto& s:scene.objects)s={0,0,0,0};
    if(g.mode==PVZ_TITLE||g.mode==PVZ_SELECT)return;
    if(g.mode==PVZ_HELP){
        emit(scene,MENU_PANEL,70,36);emit(scene,HELP_TEXT,80,84);emit(scene,BACK_BUTTON,90,157);return;
    }
    if(g.mode==PVZ_DAVE){
        int x=g.cutscene<24?-80+int(g.cutscene)*4:16;
        emit(scene,DAVE+(g.cutscene/5)%4,x,110);
        emit(scene,DAVE_TEXT+g.dialog,112,68);counters(g,scene);return;
    }
    if(g.mode==PVZ_READY){emit(scene,READY_WORD+g.cutscene/20,110,95);counters(g,scene);return;}
    unsigned anim=(g.age/4)%8;
    for(unsigned c=0;c<rows(g)*9;++c)if(g.plants[c].hp)emit(scene,PLANT+anim,center(c%9)-15,top(g,c/9)+5);
    // Modal UI hides projectiles (logic is frozen), reserving descriptor and
    // scanline budgets for the tombstone and buttons even in a crowded scene.
    if(g.mode==PVZ_PLAY)for(const auto& b:g.bullets)if(b.active)emit(scene,PEA,b.x-4,top(g,unsigned(b.row))+12);
    for(const auto& z:g.zombies)if(z.hp>0)emit(scene,(z.eating?EAT:WALK)+anim,z.x/256-15,top(g,unsigned(z.row))-15);
    for(const auto& s:g.suns)if(s.life)emit(scene,SUN,s.x,s.y);
    if(g.sun<100)emit(scene,CARD_DIM,40,2);
    if(g.selected)emit(scene,CARD_SELECT,40,2);
    counters(g,scene);
    if(g.mode){
        emit(scene,MENU_PANEL,70,36);
        emit(scene,g.mode==PVZ_WIN?WIN:g.mode==PVZ_LOSE?LOSE:PAUSE,70,67);
        if(g.mode==PVZ_PAUSE){emit(scene,RESUME_BUTTON,90,101);emit(scene,RESTART_BUTTON,90,129);}
        else emit(scene,g.mode==PVZ_WIN&&g.level==0?NEXT_BUTTON:AGAIN_BUTTON,90,125);
        emit(scene,MENU_BUTTON,90,157);
    }
}
uint32_t pvz_hash(const Game& g){
    const uint32_t* p=reinterpret_cast<const uint32_t*>(&g);uint32_t hash=2166136261u;
    for(unsigned i=0;i<sizeof(g)/4;++i)hash=(hash^p[i])*16777619u;
    return hash;
}
uint32_t pvz_metrics(const Game& g){
    unsigned plants=0;for(const auto& p:g.plants)if(p.hp)++plants;
    return g.kills|(g.spawned<<8)|(g.mode<<16)|(g.level<<20)|(plants<<24);
}
