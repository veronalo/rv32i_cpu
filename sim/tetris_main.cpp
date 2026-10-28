#include "Vnvboard_top.h"
#include <verilated.h>
#include <nvboard.h>
#include <SDL.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <csignal>
#include <vector>
#include "../sw/tetris/demo.h"
// Production driver does NOT include/link the game logic or reference renderer.
void nvboard_bind_all_pins(Vnvboard_top*);
void read_event();
static volatile std::sig_atomic_t running=1;
static void stop(int){running=0;}
int main(int argc,char**argv){
    bool gui=true,demo=false,ps2=false;unsigned limit=0;double fps=30;
    const char* logpath=nullptr;const char* shot=nullptr;
    for(int i=1;i<argc;++i){
        if(!std::strcmp(argv[i],"--headless"))gui=false;
        else if(!std::strcmp(argv[i],"--demo"))demo=true;
        else if(!std::strcmp(argv[i],"--ps2-demo"))ps2=true;
        else if(!std::strcmp(argv[i],"--frames")&&i+1<argc)limit=unsigned(std::strtoul(argv[++i],nullptr,10));
        else if(!std::strcmp(argv[i],"--fps")&&i+1<argc){
            char* end=nullptr;const char* value=argv[++i];fps=std::strtod(value,&end);
            if(end==value||*end||!std::isfinite(fps)||fps<0||fps>1000)return 2;
        }else if(!std::strcmp(argv[i],"--log")&&i+1<argc)logpath=argv[++i];
        else if(!std::strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
        else if(argv[i][0]!='+')return 2;
    }
    if((ps2&&!gui)||(ps2&&demo))return 2;
    if(!gui&&!limit)limit=300;
    FILE* log=logpath?std::fopen(logpath,"w"):nullptr;if(logpath&&!log)return 2;
    VerilatedContext context;context.threads(1);context.commandArgs(argc,argv);
    Vnvboard_top top{&context};top.reset=1;top.PS2_CLK=top.PS2_DAT=1;top.buttons=top.scripted_keys=0;
    top.mouse_packet=top.mouse_position=0;top.mouse_valid=0;
    auto tick=[&](){top.clk=0;top.eval();top.clk=1;top.eval();context.timeInc(1);};
    tick();tick();tick();top.reset=0;
    if(gui){nvboard_bind_all_pins(&top);nvboard_init();}
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    std::vector<unsigned char> image(shot?640*480*3:0),completed(image.size());
    unsigned pixel=0,last=0,input_frame=~0u,target=0;uint16_t sent=0;uint64_t cycles=0;
    int result=0;auto begin=std::chrono::steady_clock::now(),deadline=begin;
    std::puts("Tetris on RV32I: Enter=start/pause; A/D or arrows=move; W/Up=rotate; Z=reverse; S/Down=soft drop; Space=hard drop; R=restart.");
    while(running&&!context.gotFinish()){
        if(top.game_frames!=input_frame){
            input_frame=top.game_frames;uint16_t desired=tetris_demo_keys(input_frame);
            if(demo)top.scripted_keys=desired;
            if(ps2){
                const SDL_Scancode codes[9]={SDL_SCANCODE_SPACE,SDL_SCANCODE_Z,SDL_SCANCODE_TAB,
                    SDL_SCANCODE_RETURN,SDL_SCANCODE_W,SDL_SCANCODE_DOWN,SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_R};
                // W contributes both ROTATE and DROP in the shared keyboard.
                if(desired&16)desired|=1;
                for(unsigned i=0;i<9;++i){
                    // Do not send an extra Space for the W alias bit.
                    bool was=(sent&(1u<<i))!=0,now=(desired&(1u<<i))!=0;
                    if(i==0){was=was&&!(sent&16);now=now&&!(desired&16);}
                    if(was==now)continue;
                    SDL_Event e{};e.type=now?SDL_KEYDOWN:SDL_KEYUP;
                    e.key.keysym.scancode=codes[i];e.key.state=now?SDL_PRESSED:SDL_RELEASED;
                    SDL_PushEvent(&e);
                }
                sent=desired;read_event();
            }
        }
        tick();++cycles;
        if(shot&&top.VGA_BLANK_N){
            image[pixel++]=top.VGA_R;image[pixel++]=top.VGA_G;image[pixel++]=top.VGA_B;
            if(pixel==image.size()){completed.swap(image);pixel=0;}
        }
        if(gui)nvboard_update();
        if(top.halted){std::fprintf(stderr,"HALT reason=%u pc=%08x\n",top.halt_reason,top.debug_pc);result=1;break;}
        if(top.game_frames!=last){
            last=top.game_frames;
            if(log&&(!limit||last<=limit))std::fprintf(log,"%u %08x %08x\n",last,top.game_state,top.game_level);
            if(last%60==0){std::printf("frame=%u mode=%u lines=%u pieces=%u\n",last,top.game_level>>24,top.game_level&4095,(top.game_level>>12)&4095);std::fflush(stdout);}
            if(limit&&last>=limit&&!target)target=top.video_frames+2;
            if(fps>0){
                deadline+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1/fps));
                auto now=std::chrono::steady_clock::now();if(now<deadline)std::this_thread::sleep_until(deadline);else deadline=now;
            }
        }
        if(target&&top.video_frames>=target)break;
        if(limit&&cycles>uint64_t(limit+30)*420000*4){result=1;std::fputs("TIMEOUT\n",stderr);break;}
    }
    if(shot){FILE* fp=std::fopen(shot,"wb");if(!fp)return 2;std::fprintf(fp,"P6\n640 480\n255\n");std::fwrite(completed.data(),1,completed.size(),fp);std::fclose(fp);}
    if(log)std::fclose(log);
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::printf("%s: %u game frames, %llu cycles, %.3fs, %.2f fps\n",result?"FAIL":"DONE",last,(unsigned long long)cycles,seconds,last/seconds);
    if(gui)nvboard_quit();top.final();return result;
}
