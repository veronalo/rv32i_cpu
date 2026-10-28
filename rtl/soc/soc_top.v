module soc_top #(
    parameter integer ROM_BYTES = 4 * 1024 * 1024,
    parameter integer RAM_BYTES = 4 * 1024 * 1024,
    parameter MEM_FILE = "",
    parameter WITH_VIDEO = 0,
    parameter WITH_PVZ = 0,
    parameter WITH_TETRIS = 0
) (
    input         clk,
    input         reset,
    input         ps2_clk,
    input         ps2_dat,
    input [4:0]   buttons,
    input [8:0]   scripted_keys,
    input [31:0]  mouse_packet,
    input [19:0]  mouse_position,
    input         mouse_valid,
    output        mouse_ready,
    output        vga_hsync,
    output        vga_vsync,
    output        vga_valid,
    output [7:0]  vga_red,
    output [7:0]  vga_green,
    output [7:0]  vga_blue,
    output [31:0] video_frames,
    output [31:0] debug_pc,
    output [8:0]  debug_keys,
    output reg [31:0] game_frames,
    output reg [31:0] game_state,
    output reg [31:0] game_level,
    output        halted,
    output [2:0]  halt_reason,
    output reg        test_done,
    output reg [31:0] test_status
);
    localparam [31:0] RAM_BASE = 32'h1000_0000;
    localparam [31:0] STATUS_ADDR = 32'h3000_0008;
    wire [31:0] imem_addr, imem_rdata;
    wire [31:0] dmem_addr, dmem_wdata, ram_rdata, data_rom_rdata;
    wire [3:0] dmem_wstrb;
    wire dmem_ren;
    wire ram_selected = (dmem_addr >= RAM_BASE) && ((dmem_addr - RAM_BASE) < RAM_BYTES);
    wire rom_selected = (dmem_addr < ROM_BYTES);
    wire video_selected = WITH_VIDEO && (WITH_PVZ ? dmem_addr[31:20] == 12'h200 : dmem_addr[31:16] == 16'h2000);
    wire io_selected = dmem_addr[31:8] == 24'h300000;
    wire [31:0] video_rdata;
    wire [8:0] keyboard_keys;
    // buttons: down/up/right/left/center => D/U/R/L/A.
    wire [8:0] button_keys = {1'b0, buttons[2], buttons[3], buttons[0], buttons[1], 3'b0, buttons[4]};
    wire [8:0] keys = keyboard_keys | scripted_keys | button_keys;
    assign debug_keys = keys;
    reg [31:0] io_rdata;
    wire [31:0] mouse_rdata;
    generate if (WITH_VIDEO && WITH_PVZ) begin : g_mouse
        mouse_mmio u_mouse(.clk(clk),.reset(reset),.packet(mouse_packet),.valid(mouse_valid),.ready(mouse_ready),
            .acknowledge(io_selected && dmem_addr[7:2]==6'd9 && dmem_wstrb==4'b1111 && dmem_wdata[0]),
            .rdata(mouse_rdata));
    end else begin : g_no_mouse
        assign mouse_rdata=0;
        assign mouse_ready=0;
    end endgenerate
    always @(*) begin
        case (dmem_addr[7:2])
            0: io_rdata = {23'b0, keys};
            2: io_rdata = test_status;
            3: io_rdata = video_frames;
            4: io_rdata = game_frames;
            5: io_rdata = game_state;
            6: io_rdata = game_level;
            8: io_rdata = mouse_rdata;
            default: io_rdata = 0;
        endcase
    end
    wire [31:0] dmem_rdata = ram_selected ? ram_rdata :
                              rom_selected ? data_rom_rdata :
                              video_selected ? video_rdata : io_selected ? io_rdata : 32'b0;
    assign debug_pc = imem_addr;
    generate if (WITH_VIDEO) begin : g_video
        ps2_keyboard u_keyboard (.clk(clk), .reset(reset), .ps2_clk(ps2_clk), .ps2_dat(ps2_dat), .keys(keyboard_keys));
        if (WITH_TETRIS) begin : g_tetris
            tile_video u_video(.clk(clk),.reset(reset),.selected(video_selected),
                .address(dmem_addr[15:0]),.wdata(dmem_wdata),.wstrb(dmem_wstrb),.rdata(video_rdata),
                .hsync(vga_hsync),.vsync(vga_vsync),.valid(vga_valid),
                .red(vga_red),.green(vga_green),.blue(vga_blue),.frame_count(video_frames));
        end else if (WITH_PVZ) begin : g_pvz
            pvz_video u_video(.clk(clk),.reset(reset),.selected(video_selected),
                .mouse_position(mouse_position),
                .address(dmem_addr[19:0]),.wdata(dmem_wdata),.wstrb(dmem_wstrb),.rdata(video_rdata),
                .hsync(vga_hsync),.vsync(vga_vsync),.valid(vga_valid),
                .red(vga_red),.green(vga_green),.blue(vga_blue),.frame_count(video_frames));
        end else begin : g_mario
        mario_video u_video (.clk(clk), .reset(reset), .selected(video_selected),
            .address(dmem_addr[15:0]), .wdata(dmem_wdata), .wstrb(dmem_wstrb), .rdata(video_rdata),
            .hsync(vga_hsync), .vsync(vga_vsync), .valid(vga_valid),
            .red(vga_red), .green(vga_green), .blue(vga_blue), .frame_count(video_frames));
        end
    end else begin : g_no_video
        assign keyboard_keys = 0;
        assign video_rdata = 0;
        assign vga_hsync = 1;
        assign vga_vsync = 1;
        assign vga_valid = 0;
        assign vga_red = 0;
        assign vga_green = 0;
        assign vga_blue = 0;
        assign video_frames = 0;
    end endgenerate

    // A pending presentation read waits for the display boundary. This prevents
    // burning CPU instructions in the bare-metal polling loop; no writes retire.
    wire video_wait = video_selected && dmem_ren && video_rdata[0];
    rv32i_core #(.WITH_CLOCK_ENABLE(1)) u_core (
        .clk(clk), .reset(reset),
        .clock_enable(!video_wait),
        .imem_addr(imem_addr), .imem_rdata(imem_rdata),
        .dmem_addr(dmem_addr), .dmem_wdata(dmem_wdata), .dmem_wstrb(dmem_wstrb),
        .dmem_ren(dmem_ren), .dmem_rdata(dmem_rdata),
        .halted(halted), .halt_reason(halt_reason),
        .commit_valid(), .commit_pc(), .commit_inst(), .commit_rd_wen(),
        .commit_rd(), .commit_rd_data()
    );
    imem #(.ROM_BYTES(ROM_BYTES), .MEM_FILE(MEM_FILE)) u_imem (
        .address(imem_addr), .rdata(imem_rdata)
    );
    // 数据端也能读取ROM中的.rodata和.data初值，编译后的C程序需要这个通道。
    // 两个ROM读口使用同一镜像；当前先用独立实例表达，后续可改成双读口ROM。
    imem #(.ROM_BYTES(ROM_BYTES), .MEM_FILE(MEM_FILE)) u_data_rom (
        .address({dmem_addr[31:2], 2'b00}), .rdata(data_rom_rdata)
    );
    dmem #(.RAM_BYTES(RAM_BYTES)) u_dmem (
        .clk(clk), .selected(ram_selected), .offset(dmem_addr - RAM_BASE),
        .wdata(dmem_wdata), .wstrb(dmem_wstrb), .rdata(ram_rdata)
    );
    // 一个最小测试MMIO寄存器：裸机启动代码写入main()的返回值。
    always @(posedge clk) begin
        if (reset) begin
            test_done <= 1'b0;
            test_status <= 32'b0;
            game_frames <= 0;
            game_state <= 0;
            game_level <= 0;
        end else if (dmem_addr == STATUS_ADDR && dmem_wstrb == 4'b1111) begin
            test_done <= 1'b1;
            test_status <= dmem_wdata;
        end
        if (!reset && io_selected && dmem_wstrb == 4'b1111) begin
            case (dmem_addr[7:2])
                4: game_frames <= dmem_wdata;
                5: game_state <= dmem_wdata;
                6: game_level <= dmem_wdata;
                default: begin end
            endcase
        end
    end
endmodule
