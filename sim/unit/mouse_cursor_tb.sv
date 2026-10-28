`timescale 1ns/1ps
module mouse_cursor_tb;
    reg clk=0,reset=1,frame_end=0,valid=1;
    reg [19:0] position=0;
    reg [9:0] x=0;
    reg [8:0] y=0;
    reg [23:0] rgb_in=24'h3278ab;
    wire [23:0] rgb_out;
    mouse_cursor dut(.*);
    task tick;
        begin #1;clk=1;#1;clk=0;#1;end
    endtask
    task expect_rgb(input [23:0] expected);
        begin #1;if(rgb_out!==expected)$fatal(1,"cursor (%0d,%0d): %h != %h",x,y,rgb_out,expected);end
    endtask
    task latch_position(input integer px,input integer py,input bit enabled);
        begin position={enabled,9'(py),10'(px)};frame_end=1;tick();frame_end=0;end
    endtask
    initial begin
        tick();reset=0;expect_rgb(rgb_in);
        position={1'b1,9'd200,10'd300};tick();x=300;y=200;
        expect_rgb(rgb_in); // no movement until frame boundary
        frame_end=1;tick();frame_end=0;expect_rgb(24'h000000);
        x=302;y=204;expect_rgb(24'hffd65a); // yellow interior at 2x
        x=303;y=205;expect_rgb(24'hffd65a); // same expanded texel
        x=304;y=204;expect_rgb(24'h000000); // black border
        x=306;expect_rgb(rgb_in); // transparent exterior
        x=299;expect_rgb(rgb_in);y=199;expect_rgb(rgb_in);
        x=300;y=246;expect_rgb(rgb_in); // transparent trailing rows
        valid=0;expect_rgb(0);valid=1;
        latch_position(639,479,1);x=639;y=479;expect_rgb(0);
        x=0;y=0;expect_rgb(rgb_in); // right/bottom clipping never wraps
        latch_position(640,480,1);x=640;y=480;expect_rgb(rgb_in);
        latch_position(5,5,0);x=5;y=5;expect_rgb(rgb_in);
        latch_position(0,0,1);x=0;y=0;expect_rgb(0);
        reset=1;tick();expect_rgb(rgb_in);
        $display("PASS: RTL cursor border/fill/transparency, blanking, frame latch, clipping, hide/reset");
        $finish;
    end
endmodule
