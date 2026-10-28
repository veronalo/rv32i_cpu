#include "game.h"
#include "../generated/assets.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <deque>
#include "render.h"
int main(int argc,char**argv){
    bool headless=false,demo=false,mouse_demo=false,ui_demo=false,campaign=false;unsigned frames=0;const char*logpath=nullptr,*shot=nullptr;
    for(int i=1;i<argc;++i){
        if(!std::strcmp(argv[i],"--headless"))headless=true;
        else if(!std::strcmp(argv[i],"--demo"))demo=true;
        else if(!std::strcmp(argv[i],"--mouse-demo"))mouse_demo=true;
        else if(!std::strcmp(argv[i],"--ui-demo"))ui_demo=true;
        else if(!std::strcmp(argv[i],"--campaign-demo"))campaign=true;
        else if(!std::strcmp(argv[i],"--frames")&&i+1<argc)frames=std::strtoul(argv[++i],nullptr,10);
        else if(!std::strcmp(argv[i],"--log")&&i+1<argc)logpath=argv[++i];
        else if(!std::strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
        else return 2;
    }
    if(headless&&!frames)frames=campaign?3200:1500;
    if(unsigned(demo)+unsigned(mouse_demo)+unsigned(ui_demo)+unsigned(campaign)>1)return 2;
    FILE*log=logpath?std::fopen(logpath,"w"):nullptr;
    if(logpath&&!log)return 2;
    SDL_Window*window=nullptr;SDL_Renderer*renderer=nullptr;SDL_Texture*texture=nullptr;
    if(!headless){
        if(SDL_Init(SDL_INIT_VIDEO)!=0)return 2;
        SDL_StopTextInput();
        window=SDL_CreateWindow("PvZ-simplized - native preview",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,0);
        renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer){std::fprintf(stderr,"%s\n",SDL_GetError());return 2;}
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,240);
        if(!texture)return 2;
    }
    Game game;pvz_reset(game,true);Scene scene{};std::vector<uint32_t> pixels(320*240);
    unsigned frame=0;bool running=true;std::deque<uint32_t> clicks;
    while(running&&(!frames||frame<frames)){
        uint32_t start=SDL_GetTicks();uint16_t keys=0;uint32_t click=0;
        if(!headless){
            SDL_Event event;while(SDL_PollEvent(&event)){
                if(event.type==SDL_QUIT)running=false;
                if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_LOST)clicks.clear();
                if(event.type==SDL_MOUSEBUTTONDOWN&&(event.button.button==SDL_BUTTON_LEFT||event.button.button==SDL_BUTTON_RIGHT)){
                    int w,h;SDL_GetWindowSize(window,&w,&h);unsigned x,y;
                    if(pvz_map_pointer(event.button.x,event.button.y,0,0,w,h,x,y)&&clicks.size()<64)
                        clicks.push_back(pvz_click(x,y,event.button.button==SDL_BUTTON_RIGHT));
                }
            }
            const uint8_t*k=SDL_GetKeyboardState(nullptr);
            keys=(k[SDL_SCANCODE_SPACE]?1:0)|(k[SDL_SCANCODE_Z]?2:0)|(k[SDL_SCANCODE_RETURN]?8:0)|
                ((k[SDL_SCANCODE_A]||k[SDL_SCANCODE_LEFT])?64:0)|
                ((k[SDL_SCANCODE_D]||k[SDL_SCANCODE_RIGHT])?128:0)|(k[SDL_SCANCODE_R]?256:0);
            if(k[SDL_SCANCODE_ESCAPE])running=false;
        }
        if(demo)keys=pvz_demo_keys(frame);
        if(!clicks.empty()){click=clicks.front();clicks.pop_front();}
        if(mouse_demo){keys=0;click=pvz_demo_mouse(frame);}
        if(ui_demo){keys=0;click=pvz_ui_demo_mouse(frame);}
        if(campaign){keys=0;click=pvz_campaign_mouse(frame);}
        pvz_step(game,keys,click);++frame;
        if(log)std::fprintf(log,"%u %08x %08x\n",frame,pvz_hash(game),pvz_metrics(game));
        if(!headless){
            pvz_scene(game,scene);render(scene,pixels);
            SDL_UpdateTexture(texture,nullptr,pixels.data(),320*4);
            SDL_RenderClear(renderer);SDL_RenderCopy(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);
            uint32_t elapsed=SDL_GetTicks()-start;if(elapsed<33)SDL_Delay(33-elapsed);
        }
    }
    if(shot){
        pvz_scene(game,scene);render(scene,pixels);
        FILE*fp=std::fopen(shot,"wb");if(!fp)return 2;
        std::fprintf(fp,"P6\n640 480\n255\n");
        for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x){
            uint32_t c=pixels[(y/2)*320+x/2];unsigned char rgb[3]={uint8_t(c>>16),uint8_t(c>>8),uint8_t(c)};
            std::fwrite(rgb,1,3,fp);
        }
        std::fclose(fp);
    }
    std::printf("frames=%u mode=%u kills=%u spawned=%u sun=%u hash=%08x\n",frame,game.mode,game.kills,game.spawned,game.sun,pvz_hash(game));
    if(log)std::fclose(log);
    if(!headless){SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();}
}
