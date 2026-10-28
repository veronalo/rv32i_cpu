#include "Vnvboard_top.h"
#include <verilated.h>
#include <nvboard.h>
#include <SDL.h>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include "game_input.h"

void nvboard_bind_all_pins(Vnvboard_top *);
void read_event(); // NVBoard dispatcher; used only for deterministic scripted input.
static volatile std::sig_atomic_t running = 1;
static void stop(int) { running = 0; }

int main(int argc, char **argv) {
    VerilatedContext context;
    // Match the single-thread model. This Verilator otherwise creates an idle
    // pool sized to all host CPUs, even when the generated model uses one.
    context.threads(1);
    context.commandArgs(argc, argv);
    bool gui = true, demo = false, ps2_demo = false, repeat_demo = false, verify = false;
    uint32_t limit = 0;
    double target_fps = MARIO_DISPLAY_FPS;
    std::string screenshot, logpath, frame_times;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") gui = false;
        else if (arg == "--demo") demo = true;
        else if (arg == "--ps2-demo") ps2_demo = true;
        else if (arg == "--ps2-repeat-demo") { ps2_demo = true; repeat_demo = true; }
        else if (arg == "--verify") verify = true;
        else if (arg == "--frames" && i+1 < argc) limit = std::stoul(argv[++i]);
        else if (arg == "--screenshot" && i+1 < argc) screenshot = argv[++i];
        else if (arg == "--log" && i+1 < argc) logpath = argv[++i];
        else if (arg == "--frame-times" && i+1 < argc) frame_times = argv[++i];
        else if (arg == "--fps" && i+1 < argc) {
            char *end = nullptr;
            target_fps = std::strtod(argv[++i], &end);
            if (!end || end == argv[i] || *end || !std::isfinite(target_fps) || target_fps < 0 || target_fps > 1000) {
                std::fprintf(stderr,"--fps must be a number in 0..1000 (0 disables pacing)\n"); return 2;
            }
        }
        else if (arg[0] != '+') { std::fprintf(stderr,"Unknown option: %s\n",argv[i]); return 2; }
    }
    if (!gui && !limit) limit = 300;
    if (ps2_demo && !gui) { std::fprintf(stderr,"--ps2-demo needs NVBoard (SDL dummy is allowed)\n");return 2; }
    Vnvboard_top top{&context};
    top.PS2_CLK = top.PS2_DAT = 1;
    top.buttons = top.scripted_keys = 0;
    // Virtual mouse is opt-in for PvZ only; Mario keeps PS/2 controls.
    top.mouse_packet = 0; top.mouse_valid = 0; top.mouse_position = 0;
    top.reset = 1;
    auto tick = [&]() {
        top.clk=0; top.eval();
        top.clk=1; top.eval();
        context.timeInc(1);
    };
    for (int i=0;i<4;++i) tick();
    top.reset=0;
    if (gui) { nvboard_bind_all_pins(&top); nvboard_init(); }
    std::signal(SIGINT,stop);
    std::signal(SIGTERM,stop);
    FILE *log = logpath.empty() ? nullptr : std::fopen(logpath.c_str(),"w");
    if (!logpath.empty() && !log) { std::perror("log"); return 2; }
    FILE *timing = frame_times.empty() ? nullptr : std::fopen(frame_times.c_str(),"w");
    if (!frame_times.empty() && !timing) { std::perror("frame-times"); if(log)std::fclose(log); return 2; }
    if (timing) std::fprintf(timing,"vga_frame,elapsed_ms,frame_ms,game_frame,cycles,monotonic_ms\n");
    constexpr unsigned image_bytes = 640*480*3;
    const bool capture = !screenshot.empty();
    std::vector<unsigned char> image(capture ? image_bytes : 0), completed(image.size());
    unsigned pixel = 0, presented = 0;
    uint32_t prior_game=0;
    uint32_t drain_until_video=0;
    unsigned min_y=255, max_y=0, min_x=65535, max_x=0, max_scroll=0;
    bool gameplay=false;
    uint64_t cycles=0;
    uint16_t sent_keys=0;
    uint32_t input_frame=~uint32_t(0);
    auto begin=std::chrono::steady_clock::now();
    const auto frame_period=std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(target_fps > 0 ? 1.0/target_fps : 0));
    auto next_game_deadline=begin+frame_period;
    auto prior_video_time=begin;
    uint32_t prior_video=top.video_frames;
    std::puts("RV32I Mario: WASD=move, W/space=jump, Enter=confirm/start/pause, Z=run/fire, R=reset; Ctrl+C=quit.");
    std::printf("Game timing: %u logic updates/display frame; target %u display fps / %u logic ticks per second.\n",
        unsigned(MARIO_UPDATES_PER_FRAME), unsigned(MARIO_DISPLAY_FPS),
        unsigned(MARIO_UPDATES_PER_FRAME * MARIO_DISPLAY_FPS));
    if (target_fps > 0) std::printf("Host game presentation cap: %.3f fps (no RTL cycles skipped; no catch-up bursts)\n",target_fps);
    int result=0;
    while (running && !context.gotFinish()) {
        // Scripted inputs change only at game-frame boundaries, not per pixel.
        bool input_changed = top.game_frames != input_frame;
        if (demo && input_changed) top.scripted_keys=demo_keys(top.game_frames);
        if (ps2_demo && input_changed) {
            uint16_t desired=demo_keys(top.game_frames);
            const SDL_Scancode codes[9]={SDL_SCANCODE_SPACE,SDL_SCANCODE_Z,SDL_SCANCODE_TAB,
                SDL_SCANCODE_RETURN,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,
                SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_R};
            for(unsigned i=0;i<9;++i)if((desired^sent_keys)&(1<<i)){
                SDL_Event event{};event.type=(desired&(1<<i))?SDL_KEYDOWN:SDL_KEYUP;
                event.key.keysym.scancode=codes[i];event.key.state=(desired&(1<<i))?SDL_PRESSED:SDL_RELEASED;
                SDL_PushEvent(&event);
            }
            sent_keys=desired;
            // Reproduce holding a real, gameplay-unmapped Q key: an initial
            // press, OS-style repeat keydowns, then a release. The rest of
            // the game input remains identical to --ps2-demo.
            if (repeat_demo && top.game_frames >= 220 && top.game_frames <= 300) {
                SDL_Event event{};
                event.type=top.game_frames == 300 ? SDL_KEYUP : SDL_KEYDOWN;
                event.key.state=event.type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
                event.key.keysym.scancode=SDL_SCANCODE_Q;
                event.key.repeat=top.game_frames > 220 && top.game_frames < 300;
                SDL_PushEvent(&event);
            }
            // Do not leave scripted events waiting for NVBoard's wall-clock
            // UI polling cadence. PS/2 serial delivery still uses real pins
            // and simulated clock cycles, but no longer depends on host speed.
            read_event();
        }
        input_frame=top.game_frames;
        tick(); ++cycles;
        if (top.VGA_BLANK_N) {
            if (capture) {
                image[pixel++]=top.VGA_R;image[pixel++]=top.VGA_G;image[pixel++]=top.VGA_B;
            } else pixel+=3;
            if (pixel==image_bytes) {
                if (capture) completed.swap(image); // No full-frame copy.
                pixel=0; ++presented;
            }
        }
        if (gui) nvboard_update(); // Exactly once per pixel-clock cycle.
        if (timing && top.video_frames!=prior_video) {
            auto now=std::chrono::steady_clock::now();
            if (timing) {
                double elapsed_ms=std::chrono::duration<double,std::milli>(now-begin).count();
                double frame_ms=std::chrono::duration<double,std::milli>(now-prior_video_time).count();
                double monotonic_ms=std::chrono::duration<double,std::milli>(now.time_since_epoch()).count();
                std::fprintf(timing,"%u,%.3f,%.3f,%u,%llu,%.3f\n",top.video_frames,elapsed_ms,frame_ms,
                             top.game_frames,(unsigned long long)cycles,monotonic_ms);
            }
            prior_video=top.video_frames; prior_video_time=now;
        }
        if (top.halted) {
            std::fprintf(stderr,"CPU HALTED reason=%u pc=%08x\n",top.halt_reason,top.debug_pc);
            result=1;break;
        }
        if (top.game_frames!=prior_game) {
            if (target_fps > 0) {
                auto now=std::chrono::steady_clock::now();
                if (now < next_game_deadline) std::this_thread::sleep_until(next_game_deadline);
                // Pace actual new game states, not duplicate VGA scans when
                // logic misses a scan boundary. Never skip simulated cycles.
                // Rebase after a slow frame rather than burst-catching up.
                next_game_deadline=std::chrono::steady_clock::now()+frame_period;
            }
            prior_game=top.game_frames;
            unsigned mode=top.game_state&255,x=(top.game_state>>8)&255,
                     y=(top.game_state>>16)&255,page=top.game_state>>24;
            if (log) std::fprintf(log,"%u %08x %08x\n",prior_game,top.game_state,top.game_level);
            if (prior_game%30==0 || prior_game==1) {
                std::printf("game=%u vga=%u mode=%u player=%u:%u,%u scroll=%u keys=%03x pc=%08x\n",
                    prior_game,top.video_frames,mode,page,x,y,top.game_level&255,top.debug_keys,top.debug_pc);
                std::fflush(stdout);
            }
            if (mode==1 && uint64_t(prior_game)*MARIO_UPDATES_PER_FRAME>220 && y>0) {
                gameplay=true;unsigned wx=page*256+x;
                if(wx<min_x)min_x=wx;if(wx>max_x)max_x=wx;
                if(y<min_y)min_y=y;if(y>max_y)max_y=y;
                if((top.game_level&255)>max_scroll)max_scroll=top.game_level&255;
            }
        }
        // Run one more full video frame so screenshot includes the submitted state.
        if (limit && top.game_frames>=limit) {
            // Logic may miss a VGA boundary; video_frames is not game_frames+N.
            // Drain relative to the actual final submission, not its number.
            if (!drain_until_video) drain_until_video=top.video_frames+2;
            if (top.video_frames>=drain_until_video) break;
        }
        if (limit && cycles>uint64_t(limit+30)*420000*5) {
            std::fprintf(stderr,"Game timed out pc=%08x game=%u\n",top.debug_pc,top.game_frames);
            result=1;break;
        }
    }
    if (!screenshot.empty()) {
        FILE *fp=std::fopen(screenshot.c_str(),"wb");
        if (!fp) { std::perror("screenshot"); result=1; }
        else {std::fprintf(fp,"P6\n640 480\n255\n");std::fwrite(completed.data(),1,completed.size(),fp);std::fclose(fp);}
    }
    if (verify && (!gameplay || max_x-min_x<20 || max_y-min_y<10 || max_scroll==0 || !presented)) {
        std::fprintf(stderr,"FAIL gameplay=%d x=%u..%u y=%u..%u scroll=%u presented=%u\n",
            gameplay,min_x,max_x,min_y,max_y,max_scroll,presented);result=1;
    }
    double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::printf("%s: %u game frames, %u VGA frames, %llu cycles, %.2fs, %.2f game fps\n",
        result?"FAIL":"DONE",top.game_frames,top.video_frames,(unsigned long long)cycles,seconds,top.game_frames/seconds);
    std::printf("Average logic throughput: %.2f ticks/s (includes startup and final video drain).\n",
        top.game_frames*double(MARIO_UPDATES_PER_FRAME)/seconds);
    if(log)std::fclose(log);
    if(timing)std::fclose(timing);
    if(gui)nvboard_quit();
    top.final();return result;
}
