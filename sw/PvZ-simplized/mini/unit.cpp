#include "game.h"
#include "../generated/assets.h"
#include <cassert>
#include <cstdio>
int main(){
    const auto* atlas=reinterpret_cast<const uint8_t*>(pvz_atlas);
    for(unsigned n=0;n<10;++n){
        const auto& glyph=pvz_assets[SUN_DIGIT+n];unsigned opaque=0;
        for(unsigned i=0;i<unsigned(glyph.w)*glyph.h;++i)if(auto index=atlas[glyph.offset+i]){
            ++opaque;uint32_t rgb=pvz_palette[index];
            assert((rgb>>16)<48&&((rgb>>8)&255)<48&&(rgb&255)<48);
        }
        assert(opaque>0); // dark text must not become transparent palette index 0
    }
    unsigned x,y;
    assert(pvz_map_pointer(639,979,0,500,640,480,x,y)&&x==319&&y==239);
    assert(!pvz_map_pointer(640,600,0,500,640,480,x,y));
    assert(!pvz_map_pointer(0,499,0,500,640,480,x,y));
    assert(!pvz_map_pointer(0,0,0,0,0,0,x,y));
    Game mouse;pvz_reset(mouse);
    pvz_step(mouse,0,pvz_click(62,130));assert(mouse.sun==300&&!mouse.plants[1].hp);
    pvz_step(mouse,0,pvz_click(52,20));assert(mouse.selected);
    pvz_step(mouse,0,pvz_click(62,130));assert(mouse.sun==200&&mouse.plants[1].hp==300&&!mouse.selected);
    pvz_step(mouse,0,pvz_click(52,20));pvz_step(mouse,0,pvz_click(62,130));
    assert(mouse.sun==200&&mouse.selected); // occupied: no charge, keep selection
    pvz_step(mouse,0,pvz_click(10,180));assert(mouse.sun==200&&mouse.selected); // inactive row
    pvz_step(mouse,0,pvz_click(0,0,true));assert(!mouse.selected);
    mouse.suns[0]={90,120,100};pvz_step(mouse,0,pvz_click(52,20));
    pvz_step(mouse,0,pvz_click(99,129));assert(mouse.sun==225&&mouse.selected&&!mouse.plants[2].hp);
    pvz_step(mouse,0,pvz_click(285,20));assert(mouse.mode==3);
    auto frozen=mouse.age;pvz_step(mouse,0,pvz_click(62,130));assert(mouse.age==frozen);
    pvz_step(mouse,0,pvz_click(285,20));assert(mouse.mode==0);
    mouse.sun=25;mouse.selected=0;pvz_step(mouse,0,pvz_click(52,20));assert(!mouse.selected);
    pvz_reset(mouse,true);for(unsigned i=0;i<1800;++i)pvz_step(mouse,0,pvz_demo_mouse(i));
    assert(mouse.mode==1&&mouse.kills==6&&mouse.sun>0);
    pvz_step(mouse,0,pvz_click(160,135));assert(mouse.mode==PVZ_DAVE&&mouse.level==1&&mouse.sun==600);
    for(unsigned i=0;i<12;++i)pvz_step(mouse,0);
    pvz_step(mouse,0,pvz_click(200,100));assert(mouse.dialog==1&&mouse.age==0);
    pvz_step(mouse,0,pvz_click(0,0,true));assert(mouse.mode==PVZ_READY);
    for(unsigned i=0;i<60;++i)pvz_step(mouse,0);
    assert(mouse.mode==PVZ_PLAY&&mouse.age==0&&mouse.spawned==0);
    Game ui;Scene ui_scene;
    pvz_reset(ui,true);pvz_scene(ui,ui_scene);assert(ui.mode==PVZ_TITLE&&ui_scene.background==1&&ui_scene.count==0);
    pvz_step(ui,0,pvz_click(160,220));assert(ui.mode==PVZ_SELECT&&ui.age==0);
    pvz_scene(ui,ui_scene);assert(ui_scene.background==2&&ui_scene.count==0);
    pvz_step(ui,0,pvz_click(220,110));assert(ui.mode==PVZ_HELP);
    pvz_step(ui,0,pvz_click(160,167));assert(ui.mode==PVZ_SELECT);
    pvz_step(ui,0,pvz_click(225,60));assert(ui.mode==PVZ_PLAY&&ui.age==0);
    pvz_step(ui,0,pvz_click(285,20));assert(ui.mode==PVZ_PAUSE);
    pvz_step(ui,0,pvz_click(160,139));assert(ui.mode==PVZ_PLAY&&ui.age==0);
    pvz_step(ui,0,pvz_click(285,20));pvz_step(ui,0,pvz_click(160,167));assert(ui.mode==PVZ_SELECT);
    pvz_step(ui,0,pvz_click(225,60));assert(ui.mode==PVZ_PLAY&&ui.sun==300);
    ui.selected=1;pvz_scene(ui,ui_scene);
    for(const auto& s:ui_scene.objects)assert(!(s.texture>>31)||(s.texture&0x3ffff)!=pvz_assets[CURSOR].offset);
    // Crowded pause still leaves room for every modal button.
    for(auto& p:ui.plants)p={300,10};for(auto& z:ui.zombies)z={200*256,270,0};
    for(auto& b:ui.bullets)b={150,1};for(auto& s:ui.suns)s={100,100,10};
    ui.mode=PVZ_PAUSE;pvz_scene(ui,ui_scene);assert(ui_scene.count<PVZ_OBJECTS);
    assert((ui_scene.objects[ui_scene.count-1].texture&0x3ffff)==pvz_assets[MENU_BUTTON].offset);
    pvz_reset(ui,true);for(unsigned i=0;i<120;++i)pvz_step(ui,0,pvz_ui_demo_mouse(i));
    assert(ui.mode==PVZ_PAUSE&&ui.plants[1].hp==300&&ui.sun==200);
    Game g;pvz_reset(g);pvz_step(g,1);assert(g.sun==200&&g.plants[1].hp==300);
    pvz_step(g,1);assert(g.sun==200); // held jump does not buy a second plant
    pvz_step(g,0);pvz_step(g,128);pvz_step(g,0);pvz_step(g,1);
    assert(g.sun==100&&g.plants[2].hp==300);
    auto age=g.age;pvz_step(g,8);pvz_step(g,0);assert(g.mode==3&&g.age==age);
    pvz_step(g,8);assert(g.mode==0);pvz_step(g,0);
    pvz_step(g,256);assert(g.age==0&&g.sun==300);pvz_step(g,256);assert(g.age==1);
    pvz_reset(g,true);for(unsigned i=0;i<1800;++i)pvz_step(g,pvz_demo_keys(i));
    assert(g.mode==1&&g.kills==6&&g.spawned==6);
    pvz_step(g,8);pvz_step(g,8);assert(g.mode==PVZ_DAVE&&g.level==1&&g.age==0);
    pvz_reset(g);for(unsigned i=0;i<1200;++i)pvz_step(g,0);assert(g.mode==2);
    pvz_reset(g);g.plants[1].hp=300;g.plants[1].cooldown=100;
    g.zombies[0]={62*256,270,0};pvz_step(g,0);
    assert(g.plants[1].hp==297&&g.zombies[0].eating==1);
    pvz_reset(g);g.zombies[0]={70*256,20,0};g.bullets[0]={59,1};pvz_step(g,0);
    assert(g.kills==1&&g.zombies[0].hp==0);
    Scene scene;pvz_scene(g,scene);assert(scene.count<=PVZ_OBJECTS);
    for(const auto& s:scene.objects)if(s.texture>>31)
        assert((s.texture&0x3ffff)+(s.size&255)*((s.size>>8)&255)<=sizeof(pvz_atlas));
    pvz_reset(g,false,1);
    for(unsigned row=0;row<3;++row){pvz_step(g,0,pvz_click(52,20));pvz_step(g,0,pvz_click(62,86+row*44));}
    assert(g.sun==300&&g.plants[1].hp==300&&g.plants[10].hp==300&&g.plants[19].hp==300);
    pvz_reset(g,false,1);g.zombies[0]={70*256,20,0,1};g.bullets[0]={59,1,0};pvz_step(g,0);
    assert(g.kills==0&&g.zombies[0].hp==20); // rows do not cross-hit
    g.zombies[0].row=0;g.bullets[0]={59,1,0};pvz_step(g,0);assert(g.kills==1);
    pvz_reset(g,false,1);g.plants[10].hp=300;g.zombies[0]={62*256,270,0,2};pvz_step(g,0);
    assert(g.plants[10].hp==300&&!g.zombies[0].eating);
    g.plants[19].hp=300;pvz_step(g,0);assert(g.plants[19].hp==297&&g.plants[10].hp==300);
    pvz_step(g,0,pvz_click(285,20));pvz_step(g,0,pvz_click(160,139));assert(g.level==1&&g.sun==600&&g.age==0);
    pvz_reset(g,true);for(unsigned i=0;i<3200;++i)pvz_step(g,0,pvz_campaign_mouse(i));
    assert(g.mode==PVZ_WIN&&g.level==1&&g.kills==9&&g.spawned==9);
    std::puts("PASS: three-lane planting/collision/eating, level-preserving restart, Dave/Ready freeze, two-level victory");
    std::puts("PASS: mouse mapping/card/plant/sun/cancel/menu/retry/full win; keyboard, damage/eating, scene bounds");
}
