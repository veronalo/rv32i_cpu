module vga_timing (
    input clk,
    input reset,
    output reg [9:0] h,
    output reg [9:0] v,
    output hsync,
    output vsync,
    output valid,
    output frame_end,
    output [9:0] x,
    output [8:0] y
);
    // 640x480, total800x525. Same active window as the existing NVBoard example.
    always @(posedge clk) begin
        if (reset) begin h <= 0; v <= 0; end
        else if (h == 799) begin
            h <= 0;
            v <= (v == 524) ? 10'd0 : v + 1'b1;
        end else h <= h + 1'b1;
    end
    assign hsync = (h >= 96);
    assign vsync = (v >= 2);
    assign valid = (h >= 144 && h < 784 && v >= 35 && v < 515);
    assign frame_end = (h == 799 && v == 524);
    assign x = valid ? h - 10'd144 : 10'd0;
    assign y = valid ? 9'(v - 10'd35) : 9'd0;
endmodule
