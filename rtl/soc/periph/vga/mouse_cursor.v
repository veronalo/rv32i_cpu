// Black-outline/warm-yellow arrow, 16x24 texels at 2x (32x48 pixels).
// Composited after the game renderer; no framebuffer, CPU work or sprite slot.
// position = {inside, y[8:0], x[9:0]}, hotspot at the arrow's top-left tip.
module mouse_cursor (
    input clk, input reset, input frame_end,
    input [19:0] position,
    input valid, input [9:0] x, input [8:0] y,
    input [23:0] rgb_in, output [23:0] rgb_out
);
    reg visible;
    reg [9:0] cursor_x;
    reg [8:0] cursor_y;
    // Snapshot at frame boundary so moving the mouse cannot split the arrow.
    always @(posedge clk) begin
        if (reset) begin visible <= 0; cursor_x <= 0; cursor_y <= 0; end
        else if (frame_end) begin
            visible <= position[19] && position[9:0] < 640 && position[18:10] < 480;
            cursor_x <= position[9:0];
            cursor_y <= position[18:10];
        end
    end
    wire [9:0] dx = x - cursor_x;
    wire [8:0] dy = y - cursor_y;
    reg [31:0] row_bits;
    // Two bits per texel: 0 transparent, 1 black, 2 yellow; bit 0 is left.
    always @(*) begin
        case (dy[5:1])
            5'd0: row_bits = 32'h00000001;
            5'd1: row_bits = 32'h00000005;
            5'd2: row_bits = 32'h00000019;
            5'd3: row_bits = 32'h00000069;
            5'd4: row_bits = 32'h000001a9;
            5'd5: row_bits = 32'h000006a9;
            5'd6: row_bits = 32'h00001aa9;
            5'd7: row_bits = 32'h00006aa9;
            5'd8: row_bits = 32'h0001aaa9;
            5'd9: row_bits = 32'h0006aaa9;
            5'd10: row_bits = 32'h001aaaa9;
            5'd11: row_bits = 32'h00556aa9;
            5'd12: row_bits = 32'h0001a6a9;
            5'd13: row_bits = 32'h000691a9;
            5'd14: row_bits = 32'h00069069;
            5'd15: row_bits = 32'h001a4019;
            5'd16: row_bits = 32'h001a4005;
            5'd17: row_bits = 32'h00050001;
            default: row_bits = 0;
        endcase
    end
    wire [1:0] texel = row_bits[{dx[4:1],1'b0} +: 2];
    wire hit = visible && valid && x >= cursor_x && y >= cursor_y && dx < 32 && dy < 48;
    assign rgb_out = !valid ? 24'b0 :
                     (hit && texel == 1) ? 24'h000000 :
                     (hit && texel == 2) ? 24'hffd65a : rgb_in;
endmodule
