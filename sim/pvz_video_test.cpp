// Standalone graphics-unit test; host game logic creates reference fixtures
// ONLY here. Production NVBoard driver does not link game.cpp or render.h.
#include "Vpvz_video.h"
#include <verilated.h>
#include "../sw/PvZ-simplized/mini/render.h"
#include <cstdio>
#include <cstdlib>
#include <string>
int main(){
    VerilatedContext context;context.threads(1);Vpvz_video top{&context};
    top.reset=1;top.selected=0;top.wstrb=0;top.address=0;top.wdata=0;top.mouse_position=0;
    auto tick=[&](){top.clk=0;top.eval();top.clk=1;top.eval();context.timeInc(1);};
    tick();tick();top.reset=0;
    auto write=[&](unsigned address,uint32_t value){top.selected=1;top.address=address;top.wdata=value;top.wstrb=15;tick();top.selected=0;top.wstrb=0;};
    auto upload=[&](const auto& data,unsigned address){for(unsigned i=0;i<sizeof(data)/4;++i)write(address+i*4,data[i]);};
    upload(pvz_background,0);upload(pvz_title,0x60000);upload(pvz_selector,0x80000);upload(pvz_level2,0xc0000);
    for(unsigned i=0;i<sizeof(pvz_atlas)/4;++i)write(i<32768?0x20000+i*4:0xa0000+(i-32768)*4,pvz_atlas[i]);
    upload(pvz_palette,0x40000);
    auto check=[&](const Game& game,const char* name,bool extra=false){
        Scene scene{};pvz_scene(game,scene);std::vector<uint32_t> expected(320*240);render(scene,expected);
        if(extra){Sprite duplicate=scene.objects[scene.count-1];scene.objects[scene.count++]=duplicate;render(scene,expected);}
        const auto* words=reinterpret_cast<const uint32_t*>(scene.objects);
        for(unsigned i=0;i<scene.count*4;++i)write(0x41000+i*4,words[i]);
        write(0x41404,scene.background);write(0x41408,scene.count);write(0x41400,1);
        unsigned boundary=top.frame_count;while(top.frame_count==boundary)tick();
        FILE* fp=std::fopen((std::string("build/pvz-video-tests/")+name+".ppm").c_str(),"wb");
        if(!fp)return false;std::fprintf(fp,"P6\n640 480\n255\n");
        unsigned pixel=0;
        while(pixel<640*480){
            tick();if(!top.valid)continue;
            uint32_t actual=(uint32_t(top.red)<<16)|(uint32_t(top.green)<<8)|top.blue;
            uint32_t wanted=expected[(pixel/640/2)*320+(pixel%640/2)]&0xffffffu;
            if(actual!=wanted){std::fprintf(stderr,"FAIL %s pixel %u: %06x != %06x\n",name,pixel,actual,wanted);std::fclose(fp);return false;}
            const unsigned char rgb[]={top.red,top.green,top.blue};std::fwrite(rgb,1,3,fp);++pixel;
        }
        std::fclose(fp);std::printf("PASS: %s (%u sprites), full RGB and scanline deadline\n",name,scene.count);return true;
    };
    Game game;
    for(unsigned i=0;i<4;++i){pvz_reset(game,false,1);game.mode=PVZ_DAVE;game.cutscene=20+i*5;game.dialog=i%2;
        std::string name="dave-"+std::to_string(i);if(!check(game,name.c_str()))return 1;}
    game.cutscene=5;if(!check(game,"dave-enter-clipped"))return 1;
    for(unsigned i=0;i<3;++i){game.mode=PVZ_READY;game.cutscene=i*20;
        std::string name="ready-"+std::to_string(i);if(!check(game,name.c_str()))return 1;}
    pvz_reset(game,false,1);game.sun=25;game.selected=1;
    for(auto& p:game.plants)p={300,10};
    for(auto& z:game.zombies)z={160*256,270,0,1};
    for(unsigned i=0;i<PVZ_BULLETS;++i)game.bullets[i]={50+int(i)*12,1,1};
    for(auto& s:game.suns)s={100,110,10};
    Scene scene;pvz_scene(game,scene);if(scene.count!=63)return 1;
    if(!check(game,"crowded-three-lanes"))return 1;
    if(!check(game,"full-64-sprites",true))return 1;
    game.mode=PVZ_PAUSE;if(!check(game,"crowded-pause"))return 1;
    pvz_reset(game,true);if(!check(game,"zero-count-clears-stale-sprites"))return 1;
    top.final();return 0;
}
