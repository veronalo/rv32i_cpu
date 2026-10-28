module mario_video (
    input clk,
    input reset,
    input selected,
    input [15:0] address,
    input [31:0] wdata,
    input [3:0] wstrb,
    output [31:0] rdata,
    output hsync,
    output vsync,
    output valid,
    output [7:0] red,
    output [7:0] green,
    output [7:0] blue,
    output reg [31:0] frame_count
);
    // CHR=0..1fff, back state=2000..2a03, submit/pending=2a04.
    // Game-generated state is double-buffered; swap only at a VGA frame boundary.
    reg [7:0] chr [0:8191];
    reg [7:0] state_mem [0:8191];
    reg front_bank, pending;
    wire [9:0] h, v, px;
    wire [8:0] py;
    wire frame_end;
    vga_timing u_timing (.clk(clk), .reset(reset), .h(h), .v(v), .hsync(hsync),
        .vsync(vsync), .valid(valid), .frame_end(frame_end), .x(px), .y(py));
    wire [31:0] unused_timing = {12'b0, h, v};
    integer k;
    initial begin
        for (k = 0; k < 8192; k = k + 1) begin chr[k] = 0; state_mem[k] = 0; end
    end
    integer lane, bus_byte, target;
    always @(posedge clk) begin
        if (reset) begin front_bank <= 0; pending <= 0; frame_count <= 0; end
        else begin
            if (frame_end) begin
                frame_count <= frame_count + 1;
                if (pending) begin front_bank <= !front_bank; pending <= 0; end
            end
            if (selected && |wstrb) begin
                for (lane = 0; lane < 4; lane = lane + 1) begin
                    bus_byte = {16'b0, address[15:2], 2'b00} + lane;
                    if (wstrb[lane]) begin
                        if (bus_byte < 8192) chr[bus_byte] <= wdata[lane*8 +: 8];
                        else if (bus_byte >= 8192 && bus_byte <= 16'h2a03) begin
                            target = (front_bank ? 0 : 4096) + bus_byte - 8192;
                            state_mem[target] <= wdata[lane*8 +: 8];
                        end
                    end
                end
                if (address[15:2] == 14'h0a81) pending <= 1;
            end
        end
    end
    assign rdata = (address[15:2] == 14'h0a81) ? {31'b0, pending} : 32'b0;

    // Fixed combinational ROM, not a priority search through 64 case items.
    // Keep all entries explicit, including the unused NES palette colors.
    reg [23:0] palette_rom [0:63];
    initial begin
        palette_rom[ 0]=24'h7c7c7c; palette_rom[ 1]=24'h0000fc;
        palette_rom[ 2]=24'h0000bc; palette_rom[ 3]=24'h4428bc;
        palette_rom[ 4]=24'h940084; palette_rom[ 5]=24'ha80020;
        palette_rom[ 6]=24'ha81000; palette_rom[ 7]=24'h881400;
        palette_rom[ 8]=24'h503000; palette_rom[ 9]=24'h007800;
        palette_rom[10]=24'h006800; palette_rom[11]=24'h005800;
        palette_rom[12]=24'h004058; palette_rom[13]=0;
        palette_rom[14]=0; palette_rom[15]=0;
        palette_rom[16]=24'hbcbcbc; palette_rom[17]=24'h0078f8;
        palette_rom[18]=24'h0058f8; palette_rom[19]=24'h6844fc;
        palette_rom[20]=24'hd800cc; palette_rom[21]=24'he40058;
        palette_rom[22]=24'hf83800; palette_rom[23]=24'he45c10;
        palette_rom[24]=24'hac7c00; palette_rom[25]=24'h00b800;
        palette_rom[26]=24'h00a800; palette_rom[27]=24'h00a844;
        palette_rom[28]=24'h008888; palette_rom[29]=0;
        palette_rom[30]=0; palette_rom[31]=0;
        palette_rom[32]=24'hf8f8f8; palette_rom[33]=24'h3cbcfc;
        palette_rom[34]=24'h6888fc; palette_rom[35]=24'h9878f8;
        palette_rom[36]=24'hf878f8; palette_rom[37]=24'hf85898;
        palette_rom[38]=24'hf87858; palette_rom[39]=24'hfca044;
        palette_rom[40]=24'hf8b800; palette_rom[41]=24'hb8f818;
        palette_rom[42]=24'h58d854; palette_rom[43]=24'h58f898;
        palette_rom[44]=24'h00e8d8; palette_rom[45]=24'h787878;
        palette_rom[46]=0; palette_rom[47]=0;
        palette_rom[48]=24'hfcfcfc; palette_rom[49]=24'ha4e4fc;
        palette_rom[50]=24'hb8b8f8; palette_rom[51]=24'hd8b8f8;
        palette_rom[52]=24'hf8b8f8; palette_rom[53]=24'hf8a4c0;
        palette_rom[54]=24'hf0d0b0; palette_rom[55]=24'hfce0a8;
        palette_rom[56]=24'hf8d878; palette_rom[57]=24'hd8f878;
        palette_rom[58]=24'hb8f8b8; palette_rom[59]=24'hb8f8d8;
        palette_rom[60]=24'h00fcfc; palette_rom[61]=24'hf8d8f8;
        palette_rom[62]=0; palette_rom[63]=0;
    end
    function [23:0] rgb(input [5:0] index);
        rgb = palette_rom[index];
    endfunction

    // Scanline sprite cache: evaluate 64 sprites ONCE per line, not per pixel.
    // 25th bit marks nontransparent. Front/back are independent priorities.
    reg [24:0] front_line [0:255];
    reg [24:0] behind_line [0:255];
    integer lb, ly, ls, li, lx, la, lt, lr, lc, lp, ld, j;
    reg [7:0] line_ctrl;
    always @(posedge clk) begin
        if (h == 143 && v >= 35 && v < 515) begin
            lb = front_bank ? 4096 : 0;
            ly = (int'(v)-35) >> 1;
            line_ctrl = state_mem[lb+'ha00];
            for (j=0;j<256;j=j+1) begin front_line[j]=0; behind_line[j]=0; end
            // First sprite zero, then 63..1: matches upstream coin-indicator priority.
            for (ls=64;ls>=0;ls=ls-1) begin
                li = ls == 64 ? 0 : ls;
                ld = lb+'h800+li*4;
                lx=int'(state_mem[ld+3]);la=int'(state_mem[ld+2]);
                lr=ly-int'(state_mem[ld])-1;
                lt=int'(state_mem[ld+1])+(line_ctrl[3]?256:0);
                if (lx<249 && state_mem[ld]<239 && lr>=0 && lr<8) begin
                    if ((la&128)!=0) lr=7-lr;
                    for(lc=0;lc<8;lc=lc+1) begin
                        lp=((int'(chr[lt*16+lr]) >> lc)&1) |
                           (((int'(chr[lt*16+lr+8]) >> lc)&1)<<1);
                        j=lx+(((la&64)!=0)?lc:7-lc);
                        if (lp!=0) begin
                            if ((la&32)!=0 && ls!=64)
                                behind_line[j]={1'b1,rgb(state_mem[lb+'h910+(la&3)*4+lp][5:0])};
                            if (((la&32)==0 && ls!=0) || (ls==64 && (lt&255)==255)) begin
                                if (!(li==0 && (lt&255)==255 && lr==5 && lc>3 && lc<6))
                                    front_line[j]={1'b1,rgb(state_mem[lb+'h910+(la&3)*4+lp][5:0])};
                            end
                        end
                    end
                end
            end
        end
    end

    integer base, x, y, world_x, nt_index, attr_index, shift, tile, row, column;
    integer palette_id, pi;
    reg [7:0] ctrl, mask, scroll_x, lo, hi;
    reg [23:0] pixel, bg, behind, front;
    reg bg_visible, behind_visible, front_visible;
    always @(*) begin
        base = front_bank ? 4096 : 0;
        ctrl = state_mem[base + 'ha00];
        mask = state_mem[base + 'ha01];
        scroll_x = state_mem[base + 'ha02];
        x=0; y=0; world_x=0; nt_index=0; attr_index=0; shift=0; tile=0; row=0; column=0;
        palette_id=0; pi=0;
        lo=0; hi=0; bg=0; behind=0; front=0;
        bg_visible=0; behind_visible=0; front_visible=0;
        pixel = 0;
        if (valid && px >= 64 && px < 576) begin
            x = (int'(px) - 64) >> 1;
            y = int'(py) >> 1;
            pixel = rgb(state_mem[base + 'h900][5:0]);
            if (mask[3]) begin
                world_x = (y < 32) ? x : (x + int'(scroll_x) + (ctrl[0] ? 256 : 0));
                world_x = world_x & 511;
                nt_index = ((world_x >> 8) << 10) + ((y >> 3) << 5) + ((world_x & 255) >> 3);
                attr_index = ((world_x >> 8) << 10) + 960 + ((y >> 5) << 3) + ((world_x & 255) >> 5);
                shift = ((y & 16) != 0 ? 4 : 0) + ((world_x & 16) != 0 ? 2 : 0);
                palette_id = (int'(state_mem[base + attr_index]) >> shift) & 3;
                tile = int'(state_mem[base + nt_index]) + (ctrl[4] ? 256 : 0);
                row = y & 7;
                column = 7 - (world_x & 7);
                lo = chr[tile * 16 + row]; hi = chr[tile * 16 + row + 8];
                pi = ((int'(lo) >> column) & 1) | (((int'(hi) >> column) & 1) << 1);
                bg_visible = (pi != 0);
                bg = rgb(state_mem[base + 'h900 + palette_id * 4 + pi][5:0]);
            end
            if (mask[4]) begin
                behind = behind_line[x][23:0]; behind_visible=behind_line[x][24];
                front = front_line[x][23:0]; front_visible=front_line[x][24];
            end
            if (behind_visible) pixel = behind;
            if (bg_visible) pixel = bg;
            if (front_visible) pixel = front;
        end
    end
    assign {red, green, blue} = pixel;
endmodule
