#include "Vsoc_top.h"
#include <verilated.h>
#include <chrono>
#include <cstdio>

int main(int argc, char **argv) {
    VerilatedContext context;
    context.threads(1);
    context.commandArgs(argc, argv);
    Vsoc_top top{&context};
    top.reset=1;
    top.ps2_clk=top.ps2_dat=1;
    top.buttons=top.scripted_keys=0;
    top.mouse_packet=0;top.mouse_valid=0;top.mouse_position=0;
    auto tick=[&]() {top.clk=0;top.eval();top.clk=1;top.eval();context.timeInc(1);};
    tick();tick();top.reset=0;
    uint32_t phase=0;
    uint32_t video_checksum=0;
    uint64_t start_cycle=0;
    auto begin=std::chrono::steady_clock::now(), start_time=begin;
    std::puts("phase,cycles,host_ms,simulated_cycles_per_second");
    for(uint64_t cycles=1;cycles<=100000000; ++cycles) {
        tick();
        if(top.vga_valid)video_checksum=(video_checksum<<1)^(video_checksum>>31)^
            (uint32_t(top.vga_red)<<16)^(uint32_t(top.vga_green)<<8)^top.vga_blue;
        if(top.game_frames!=phase) {
            auto now=std::chrono::steady_clock::now();
            double seconds=std::chrono::duration<double>(now-start_time).count();
            if(phase)std::printf("%u,%llu,%.3f,%.0f\n",phase,
                (unsigned long long)(cycles-start_cycle),seconds*1000,
                (cycles-start_cycle)/seconds);
            phase=top.game_frames;start_cycle=cycles;start_time=now;
        }
        if(top.halted) {
            bool ok=top.halt_reason==1 && top.test_done && top.test_status==0;
            std::fprintf(stderr,"%s: cycles=%llu exit=%u reason=%u host_seconds=%.3f VGA_frames=%u video_checksum=%08x\n",
                ok?"PASS":"FAIL",(unsigned long long)cycles,top.test_status,
                top.halt_reason,std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count(),
                top.video_frames,video_checksum);
            top.final();return ok?0:1;
        }
    }
    std::fprintf(stderr,"FAIL: timeout pc=%08x phase=%u\n",top.debug_pc,phase);
    top.final();return 1;
}
