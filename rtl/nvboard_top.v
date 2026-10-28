// NVBoard is a simulator front end, NOT an FPGA: clk/reset are driven by C++.
module nvboard_top #(
    parameter PVZ_MODE = 0,
    parameter TETRIS_MODE = 0
) (
    input clk,
    input reset,
    input PS2_CLK,
    input PS2_DAT,
    input [4:0] buttons,
    input [8:0] scripted_keys,
    input [31:0] mouse_packet,
    input [19:0] mouse_position,
    input mouse_valid,
    output mouse_ready,
    output VGA_HSYNC,
    output VGA_VSYNC,
    output VGA_BLANK_N,
    output [7:0] VGA_R,
    output [7:0] VGA_G,
    output [7:0] VGA_B,
    output halted,
    output [2:0] halt_reason,
    output [31:0] debug_pc,
    output [8:0] debug_keys,
    output [31:0] video_frames,
    output [31:0] game_frames,
    output [31:0] game_state,
    output [31:0] game_level
);
    soc_top #(.WITH_VIDEO(1), .WITH_PVZ(PVZ_MODE), .WITH_TETRIS(TETRIS_MODE)) u_soc (
        .clk(clk), .reset(reset), .ps2_clk(PS2_CLK), .ps2_dat(PS2_DAT),
        .buttons(buttons), .scripted_keys(scripted_keys),
        .mouse_packet(mouse_packet), .mouse_valid(mouse_valid), .mouse_ready(mouse_ready),
        .mouse_position(mouse_position),
        .vga_hsync(VGA_HSYNC), .vga_vsync(VGA_VSYNC), .vga_valid(VGA_BLANK_N),
        .vga_red(VGA_R), .vga_green(VGA_G), .vga_blue(VGA_B),
        .halted(halted), .halt_reason(halt_reason), .debug_pc(debug_pc), .debug_keys(debug_keys),
        .video_frames(video_frames), .game_frames(game_frames),
        .game_state(game_state), .game_level(game_level), .test_done(), .test_status()
    );
endmodule
