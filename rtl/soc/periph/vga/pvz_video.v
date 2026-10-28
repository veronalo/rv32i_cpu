// Indexed background + 64 transparent sprites. CPU submits rectangles only;
// all pixel selection, clipping and palette lookup are real RTL, not host DPI.
module pvz_video (
    input clk, input reset, input selected,
    input [19:0] mouse_position,
    input [19:0] address, input [31:0] wdata, input [3:0] wstrb,
    output [31:0] rdata,
    output hsync, output vsync, output valid,
    output [7:0] red, output [7:0] green, output [7:0] blue,
    output reg [31:0] frame_count
);
    reg [7:0] background [0:307199];
    reg [7:0] atlas [0:262143];
    reg [23:0] palette [0:255];
    reg [31:0] objects [0:511];
    reg front_bank, pending;
    reg [1:0] front_background, next_background;
    reg [6:0] front_count,next_count;
    wire [9:0] h,v,x;
    wire [8:0] y;
    wire frame_end;
    vga_timing u_timing(.clk(clk),.reset(reset),.h(h),.v(v),.hsync(hsync),
        .vsync(vsync),.valid(valid),.frame_end(frame_end),.x(x),.y(y));
    integer i,lane,bus_byte,index;
    initial begin
        for(i=0;i<307200;i=i+1)background[i]=0;
        for(i=0;i<262144;i=i+1)atlas[i]=0;
        for(i=0;i<256;i=i+1)palette[i]=0;
        for(i=0;i<512;i=i+1)objects[i]=0;
    end
    always @(posedge clk) begin
        if(reset)begin front_bank<=0;pending<=0;frame_count<=0;front_background<=0;next_background<=0;front_count<=64;next_count<=64;end
        else begin
            if(frame_end)begin
                frame_count<=frame_count+1;
                if(pending)begin front_bank<=!front_bank;front_background<=next_background;front_count<=next_count;pending<=0;end
            end
            if(selected && |wstrb)begin
                for(lane=0;lane<4;lane=lane+1)begin
                    bus_byte=32'({address[19:2],2'b00})+lane;
                    if(wstrb[lane])begin
                        if(bus_byte<76800)background[bus_byte]<=wdata[lane*8+:8];
                        if(bus_byte>=32'h60000 && bus_byte<32'h72c00)
                            background[bus_byte-32'h60000+76800]<=wdata[lane*8+:8];
                        if(bus_byte>=32'h80000 && bus_byte<32'h92c00)
                            background[bus_byte-32'h80000+153600]<=wdata[lane*8+:8];
                        if(bus_byte>=32'hc0000 && bus_byte<32'hd2c00)
                            background[bus_byte-32'hc0000+230400]<=wdata[lane*8+:8];
                        if(bus_byte>=20'h20000 && bus_byte<20'h40000)
                            atlas[bus_byte-32'h20000]<=wdata[lane*8+:8];
                        if(bus_byte>=32'ha0000 && bus_byte<32'hc0000)
                            atlas[bus_byte-32'ha0000+131072]<=wdata[lane*8+:8];
                    end
                end
                if(address>=20'h40000 && address<20'h40400 && wstrb==4'b1111)
                    palette[address[9:2]]<=wdata[23:0];
                if(address>=20'h41000 && address<20'h41400 && wstrb==4'b1111)begin
                    index=(front_bank?0:256)+((32'(address)-32'h41000)>>2);
                    objects[index]<=wdata;
                end
                if(address==20'h41400 && wstrb==4'b1111)pending<=1;
                if(address==20'h41404 && wstrb==4'b1111)
                    next_background<=wdata[1:0];
                if(address==20'h41408 && wstrb==4'b1111)
                    next_count<=(wdata<=64)?wdata[6:0]:7'd64;
            end
        end
    end
    assign rdata=(address==20'h41400)?{31'b0,pending}:32'b0;
    // Build each logical scanline during horizontal blanking, 32 pixels/cycle.
    // The second physical scanline reuses it. Only front_count valid-prefix
    // entries are scanned; empty slots incur no per-line descriptor reads.
    reg [7:0] line_buffer [0:319];
    reg [1:0] phase;
    integer logical_y, column, sprite_id, paint_x, sprite_x, sprite_width, row_offset;
    integer render_lane, dest_x, tex, base, sy, sh;
    reg [31:0] xy, texture, size;
    reg [7:0] sample_color;
    always @(posedge clk)begin
        if(reset)begin
            phase<=0;logical_y<=0;column<=0;sprite_id<=0;
            paint_x<=0;sprite_x<=0;sprite_width<=0;row_offset<=0;
            for(integer p=0;p<320;p=p+1)line_buffer[p]<=0;
        end else begin
            if(h==0 && v>=35 && v<515 && v[0])begin
                logical_y<=(32'(v)-35)>>1;column<=0;phase<=1;
            end else if(phase==1)begin
                for(render_lane=0;render_lane<32;render_lane=render_lane+1)
                    line_buffer[column+render_lane]<=background[32'(front_background)*76800+logical_y*320+column+render_lane];
                if(column==288)begin sprite_id<=0;phase<=(front_count==0)?0:2;end
                else column<=column+32;
            end else if(phase==2)begin
                base=(front_bank?256:0)+sprite_id*4;
                xy=objects[base];texture=objects[base+1];size=objects[base+2];
                sy=32'($signed(xy[31:16]));sh=32'(size[15:8]);
                if(texture[31] && logical_y>=sy && logical_y<sy+sh && size[7:0]!=0)begin
                    sprite_x<=32'($signed(xy[15:0]));sprite_width<=32'(size[7:0]);
                    row_offset<=32'(texture[17:0])+(logical_y-sy)*32'(size[7:0]);
                    paint_x<=0;phase<=3;
                end else if(sprite_id+1>=32'(front_count))phase<=0;
                else sprite_id<=sprite_id+1;
            end else if(phase==3)begin
                for(render_lane=0;render_lane<32;render_lane=render_lane+1)begin
                    dest_x=sprite_x+paint_x+render_lane;tex=row_offset+paint_x+render_lane;
                    if(paint_x+render_lane<sprite_width && dest_x>=0 && dest_x<320 && tex>=0 && tex<262144)begin
                        sample_color=atlas[tex];
                        if(sample_color!=0)line_buffer[dest_x]<=sample_color;
                    end
                end
                if(paint_x+32>=sprite_width)begin
                    if(sprite_id+1>=32'(front_count))phase<=0;
                    else begin sprite_id<=sprite_id+1;phase<=2;end
                end else paint_x<=paint_x+32;
            end
        end
    end
    wire [23:0] rgb=valid?palette[line_buffer[x[9:1]]]:24'b0;
`ifndef SYNTHESIS
    // The fixed runtime pack has sprite widths <=180 and at most 64 objects;
    // all legal scenes must finish before the active video window at h=144.
    always @(posedge clk)
        if(!reset && h==143 && v>=35 && v<515 && phase!=0)
            $fatal(1,"PvZ scanline deadline missed v=%0d phase=%0d",v,phase);
`endif
    wire [23:0] cursor_rgb;
    mouse_cursor u_cursor(.clk(clk),.reset(reset),.frame_end(frame_end),.position(mouse_position),
        .valid(valid),.x(x),.y(y),.rgb_in(rgb),.rgb_out(cursor_rgb));
    assign red=cursor_rgb[23:16];assign green=cursor_rgb[15:8];assign blue=cursor_rgb[7:0];
    wire [19:0] unused_timing={h,v};
endmodule
