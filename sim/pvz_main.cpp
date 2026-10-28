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
#include "../sw/PvZ-simplized/mini/game.h"
#include "nvboard_mouse.h"
void nvboard_bind_all_pins(Vnvboard_top*);
void read_event();
static volatile std::sig_atomic_t running=1;
static void stop(int){running=0;}
int main(int argc,char**argv){
    bool gui=true,demo=false,ps2=false,verify=false,mouse_demo=false,mouse_sdl=false,ui_demo=false,campaign=false;unsigned limit=0;double fps=30;
    const char* logpath=nullptr,*shot=nullptr,*perfpath=nullptr;
    bool cursor_demo=false;
    for(int i=1;i<argc;++i){
        if(!std::strcmp(argv[i],"--headless"))gui=false;
        else if(!std::strcmp(argv[i],"--demo"))demo=true;
        else if(!std::strcmp(argv[i],"--ps2-demo"))ps2=true;
        else if(!std::strcmp(argv[i],"--mouse-demo"))mouse_demo=true;
        else if(!std::strcmp(argv[i],"--mouse-sdl-demo"))mouse_demo=mouse_sdl=true;
        else if(!std::strcmp(argv[i],"--ui-demo"))mouse_demo=ui_demo=true;
        else if(!std::strcmp(argv[i],"--campaign-demo"))mouse_demo=campaign=true;
        else if(!std::strcmp(argv[i],"--verify"))verify=true;
        else if(!std::strcmp(argv[i],"--cursor-demo"))cursor_demo=true;
        else if(!std::strcmp(argv[i],"--frames")&&i+1<argc)limit=std::strtoul(argv[++i],nullptr,10);
        else if(!std::strcmp(argv[i],"--fps")&&i+1<argc){
            char*end=nullptr;const char*value=argv[++i];fps=std::strtod(value,&end);
            if(end==value||*end||!std::isfinite(fps)||fps<0||fps>1000)return 2;
        }
        else if(!std::strcmp(argv[i],"--log")&&i+1<argc)logpath=argv[++i];
        else if(!std::strcmp(argv[i],"--perf-log")&&i+1<argc)perfpath=argv[++i];
        else if(!std::strcmp(argv[i],"--screenshot")&&i+1<argc)shot=argv[++i];
        else if(argv[i][0]!='+')return 2;
    }
    if((!gui&&(ps2||mouse_sdl))||((demo||ps2)&&mouse_demo))return 2;
    if(!gui&&!limit)limit=campaign?3200:1500;
    FILE*log=logpath?std::fopen(logpath,"w"):nullptr;if(logpath&&!log)return 2;
    FILE*perf=perfpath?std::fopen(perfpath,"w"):nullptr;if(perfpath&&!perf)return 2;
    if(perf)std::fputs("frame,level,mode,wall_seconds\n",perf);
    VerilatedContext context;context.threads(1);context.commandArgs(argc,argv);
    Vnvboard_top top{&context};top.reset=1;top.PS2_CLK=top.PS2_DAT=1;top.buttons=top.scripted_keys=0;
    top.mouse_valid=0;top.mouse_packet=0;top.mouse_position=0;
    auto tick=[&](){top.clk=0;top.eval();top.clk=1;top.eval();context.timeInc(1);};
    tick();tick();tick();tick();top.reset=0;
    // Fixed RTL cursor fixture, without desktop input or host-side drawing.
    if(cursor_demo)top.mouse_position=(1u<<19)|(200u<<10)|300u;
    if(gui){nvboard_bind_all_pins(&top);nvboard_init();rv32i_mouse_enable(true);}
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    auto begin=std::chrono::steady_clock::now(),deadline=begin;
    auto period=std::chrono::duration<double>(fps>0?1/fps:0);
    std::vector<unsigned char> image(shot?640*480*3:0),completed(image.size());
    unsigned pixel=0,last=0,input_frame=~0u,drain=0;uint16_t sent=0;bool won=false;int result=0;
    uint64_t cycles=0;
    std::puts("PvZ-simplized on RV32I: click seed card then lawn to plant; click sun to collect; right click=cancel; MENU=pause/retry. Keyboard controls remain available.");
    while(running&&!context.gotFinish()){
        if(top.game_frames!=input_frame){
            input_frame=top.game_frames;auto desired=pvz_demo_keys(input_frame);
            if(mouse_demo){
                uint32_t packet=campaign?pvz_campaign_mouse(input_frame):ui_demo?pvz_ui_demo_mouse(input_frame):pvz_demo_mouse(input_frame);
                if(packet){
                    if(mouse_sdl){
                        if(!rv32i_mouse_inject(packet)){std::fputs("FAIL: SDL mouse injection\n",stderr);result=1;break;}
                        read_event();
                    }else{
                        if(top.mouse_valid||!top.mouse_ready){result=1;std::fputs("Mouse demo backpressure\n",stderr);break;}
                        top.mouse_packet=packet;top.mouse_valid=1;
                    }
                }
            }
            if(demo)top.scripted_keys=desired;
            if(ps2){
                const SDL_Scancode codes[9]={SDL_SCANCODE_SPACE,SDL_SCANCODE_Z,SDL_SCANCODE_TAB,
                    SDL_SCANCODE_RETURN,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_R};
                for(unsigned i=0;i<9;++i)if((desired^sent)&(1<<i)){
                    SDL_Event e{};e.type=(desired&(1<<i))?SDL_KEYDOWN:SDL_KEYUP;
                    e.key.keysym.scancode=codes[i];e.key.state=(desired&(1<<i))?SDL_PRESSED:SDL_RELEASED;
                    SDL_PushEvent(&e);
                }
                sent=desired;read_event();
            }
        }
        // Poll the project queue in batches, not on every simulated clock.
        if(gui&&!cursor_demo&&!mouse_demo&&!demo&&!ps2&&!(cycles&4095))
            top.mouse_position=rv32i_mouse_position();
        if(gui&&(!mouse_demo||mouse_sdl)&&!top.mouse_valid&&!(cycles&4095)){
            uint32_t packet;
            if(rv32i_mouse_pop(packet)){top.mouse_packet=packet;top.mouse_valid=1;}
        }
        bool mouse_accepted=top.mouse_valid&&top.mouse_ready;
        tick();++cycles;
        if(mouse_accepted)top.mouse_valid=0;
        if(shot&&top.VGA_BLANK_N){
            image[pixel++]=top.VGA_R;image[pixel++]=top.VGA_G;image[pixel++]=top.VGA_B;
            if(pixel==image.size()){completed.swap(image);pixel=0;}
        }
        if(gui)nvboard_update();
        if(top.halted){std::fprintf(stderr,"HALT reason=%u pc=%08x\n",top.halt_reason,top.debug_pc);result=1;break;}
        if(top.game_frames!=last){
            last=top.game_frames;won|=((top.game_level>>16)&15)==1&&(!campaign||(top.game_level&(1u<<20)));
            if(log)std::fprintf(log,"%u %08x %08x\n",last,top.game_state,top.game_level);
            if(perf)std::fprintf(perf,"%u,%u,%u,%.9f\n",last,((top.game_level>>20)&1)+1,(top.game_level>>16)&15,
                std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count());
            if(last%60==0){unsigned level=(top.game_level>>20)&1;
                std::printf("frame=%u level=1-%u kills=%u/%u spawned=%u mode=%u hash=%08x\n",last,level+1,
                top.game_level&255,level?9:6,(top.game_level>>8)&255,(top.game_level>>16)&15,top.game_state);std::fflush(stdout);}
            if(fps>0){
                deadline+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(period);
                auto now=std::chrono::steady_clock::now();if(now<deadline)std::this_thread::sleep_until(deadline);
                else deadline=now;
            }
        }
        if(limit&&last>=limit){if(!drain)drain=top.video_frames+2;if(top.video_frames>=drain)break;}
        if(limit&&cycles>uint64_t(limit+30)*420000*5){result=1;std::fputs("TIMEOUT\n",stderr);break;}
    }
    if(verify&&!won){result=1;std::fputs("FAIL: deterministic demo did not win\n",stderr);}
    if(shot){FILE*fp=std::fopen(shot,"wb");if(!fp)return 2;std::fprintf(fp,"P6\n640 480\n255\n");std::fwrite(completed.data(),1,completed.size(),fp);std::fclose(fp);}
    auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::printf("%s: %u game frames, %u VGA frames, %llu cycles, %.3fs, %.2f fps\n",result?"FAIL":"DONE",
        last,top.video_frames,(unsigned long long)cycles,seconds,last/seconds);
    if(log)std::fclose(log);if(perf)std::fclose(perf);
    if(gui){rv32i_mouse_enable(false);nvboard_quit();}top.final();return result;
}
