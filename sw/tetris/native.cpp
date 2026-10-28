#include "render.h"
#include "demo.h"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
int main(int argc,char** argv){
    bool headless=false,demo=false,running=true;unsigned limit=0;
    const char* logpath=nullptr;const char* shot=nullptr;
    for(int i=1;i<argc;++i){
        if(!std::strcmp(argv[i],"--headless"))headless=true;
        else if(!std::strcmp(argv[i],"--demo"))demo=true;
        else if(!std::strcmp(argv[i],"--frames")&&i+1<argc)limit=unsigned(std::strtoul(argv[++i],nullptr,10));
        else if(!std::strcmp(argv[i],"--log")&&i+1<argc)logpath=argv[++i];
        else if(!std::strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
        else return 2;
    }
    if(headless&&!limit)limit=300;
    FILE* log=logpath?std::fopen(logpath,"w"):nullptr;if(logpath&&!log)return 2;
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* texture=nullptr;
    if(!headless){
        if(SDL_Init(SDL_INIT_VIDEO)<0)return 2;
        window=SDL_CreateWindow("RV32I Tetris - native preview",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,0);
        if(window)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        if(renderer)texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,640,480);
        if(!texture){std::fprintf(stderr,"SDL: %s\n",SDL_GetError());SDL_Quit();return 2;}
    }
    Tetris game;tetris_reset(game);TetrisScene scene;
    std::vector<uint32_t> pixels(640*480);
    unsigned frame=0;
    while(running&&(!limit||frame<limit)){
        uint32_t start=SDL_GetTicks();uint16_t keys=0;
        if(!headless){
            SDL_Event e;while(SDL_PollEvent(&e))if(e.type==SDL_QUIT)running=false;
            const auto* k=SDL_GetKeyboardState(nullptr);
            keys=uint16_t((k[SDL_SCANCODE_SPACE]?TK_DROP:0)|(k[SDL_SCANCODE_Z]?TK_CCW:0)|
                (k[SDL_SCANCODE_RETURN]?TK_ENTER:0)|((k[SDL_SCANCODE_W]||k[SDL_SCANCODE_UP])?TK_ROTATE:0)|
                ((k[SDL_SCANCODE_S]||k[SDL_SCANCODE_DOWN])?TK_DOWN:0)|
                ((k[SDL_SCANCODE_A]||k[SDL_SCANCODE_LEFT])?TK_LEFT:0)|
                ((k[SDL_SCANCODE_D]||k[SDL_SCANCODE_RIGHT])?TK_RIGHT:0)|(k[SDL_SCANCODE_R]?TK_RESET:0));
            // W aliases both UP and JUMP; the physical Up arrow only sets UP.
            if(k[SDL_SCANCODE_W])keys|=TK_DROP;
            if(k[SDL_SCANCODE_ESCAPE])running=false;
        }
        if(demo)keys=tetris_demo_keys(frame);
        tetris_step(game,keys);++frame;
        uint32_t metrics=tetris_metrics(game);
        if(log)std::fprintf(log,"%u %08x %08x\n",frame,tetris_hash(game),metrics);
        if(!headless){
            tetris_scene(game,scene);
            for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x)pixels[y*640+x]=0xff000000u|tetris_pixel(scene,x,y);
            SDL_UpdateTexture(texture,nullptr,pixels.data(),640*4);SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer,texture,nullptr,nullptr);SDL_RenderPresent(renderer);
            uint32_t elapsed=SDL_GetTicks()-start;if(elapsed<33)SDL_Delay(33-elapsed);
        }
    }
    if(shot){
        tetris_scene(game,scene);FILE* fp=std::fopen(shot,"wb");if(!fp)return 2;
        std::fprintf(fp,"P6\n640 480\n255\n");
        for(unsigned y=0;y<480;++y)for(unsigned x=0;x<640;++x){
            uint32_t color=tetris_pixel(scene,x,y);unsigned char rgb[3]={uint8_t(color>>16),uint8_t(color>>8),uint8_t(color)};
            std::fwrite(rgb,1,3,fp);
        }
        std::fclose(fp);
    }
    std::printf("frames=%u mode=%u score=%u lines=%u locks=%u hash=%08x\n",frame,game.mode,game.score,game.lines,game.locks,tetris_hash(game));
    if(log)std::fclose(log);
    if(!headless){SDL_DestroyTexture(texture);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();}
    return 0;
}
