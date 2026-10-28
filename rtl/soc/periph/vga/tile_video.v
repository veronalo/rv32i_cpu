// Generic 40x30 character/color-tile display, 16x16 pixels per tile.
// CPU uploads font/palette once and submits double-buffered tile maps.
module tile_video (
    input clk, input reset, input selected,
    input [15:0] address, input [31:0] wdata, input [3:0] wstrb,
    output [31:0] rdata,
    output hsync, output vsync, output valid,
    output [7:0] red, output [7:0] green, output [7:0] blue,
    output reg [31:0] frame_count
);
    // 0..12bf: 1200 tiles, each a 32-bit write (low 16 bits used).
    // Tile: background[15:12], foreground[11:8], character[7:0].
    // 2000: submit/pending; 3000..33ff: 128 glyphs x 8 row bytes;
    // 3400..343f: 16 RGB888 palette words. Font is 5x7, scaled 2x.
    reg [15:0] tiles [0:2399];
    reg [7:0] font [0:1023];
    reg [23:0] palette [0:15];
    reg front_bank, pending;
    wire [9:0] h,v,x;
    wire [8:0] y;
    wire frame_end;
    vga_timing u_timing(.clk(clk),.reset(reset),.h(h),.v(v),.hsync(hsync),
        .vsync(vsync),.valid(valid),.frame_end(frame_end),.x(x),.y(y));
    integer i,lane;
    initial begin
        for(i=0;i<2400;i=i+1)tiles[i]=0;
        for(i=0;i<1024;i=i+1)font[i]=0;
        for(i=0;i<16;i=i+1)palette[i]=0;
    end
    always @(posedge clk) begin
        if(reset)begin front_bank<=0;pending<=0;frame_count<=0;end
        else begin
            if(frame_end)begin
                frame_count<=frame_count+1;
                if(pending)begin front_bank<=!front_bank;pending<=0;end
            end
            if(selected && wstrb==4'b1111)begin
                if(address<16'h12c0)
                    tiles[(front_bank?0:1200)+32'(address[15:2])]<=wdata[15:0];
                if(address==16'h2000)pending<=1;
                if(address>=16'h3400 && address<16'h3440)palette[address[5:2]]<=wdata[23:0];
            end
            if(selected && address>=16'h3000 && address<16'h3400)
                for(lane=0;lane<4;lane=lane+1)
                    if(wstrb[lane])font[{address[9:2],2'b00}+10'(lane)]<=wdata[lane*8+:8];
        end
    end
    assign rdata=(address==16'h2000)?{31'b0,pending}:32'b0;
    wire [15:0] tile=tiles[(front_bank?1200:0)+32'(y[8:4])*40+32'(x[9:4])];
    wire [3:0] px=x[3:0],py=y[3:0];
    wire [3:0] glyph_x=(px-4'd3)>>1;
    wire [3:0] glyph_y=(py-4'd1)>>1;
    wire [7:0] row_bits=font[{tile[6:0],3'b0}+10'(glyph_y)];
    wire ink=px>=3 && px<13 && py>=1 && py<15 && row_bits[4-glyph_x];
    reg [23:0] rgb;
    always @(*)begin
        rgb=0;
        if(valid)begin
            rgb=palette[tile[15:12]];
            if(tile[7:0]==8'h80)begin // solid beveled block
                rgb=palette[tile[11:8]];
                if(px==0 || py==0)rgb=24'hffffff;
                if(px==15 || py==15)rgb=(palette[tile[11:8]]>>1)&24'h7f7f7f;
            end else if(tile[7:0]==8'h81)begin // ghost landing outline
                if(px==1 || px==14 || py==1 || py==14)rgb=palette[tile[11:8]];
            end else if(ink)rgb=palette[tile[11:8]];
        end
    end
    assign {red,green,blue}=rgb;
    wire [19:0] unused_timing={h,v};
endmodule
