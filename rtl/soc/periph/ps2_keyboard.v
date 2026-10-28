module ps2_keyboard (
    input clk,
    input reset,
    input ps2_clk,
    input ps2_dat,
    output [8:0] keys
);
    // bits: A jump, B run/fire, select, start, up, down, left, right, reset.
    reg [2:0] clk_sync, dat_sync;
    reg [10:0] packet;
    reg [3:0] count;
    reg [15:0] timeout_count;
    reg released, extended;
    reg [7:0] code;
    integer key_index;
    // Independent physical-key states are essential for W/space/X aliases:
    // holding W+space, then releasing W, must leave jump pressed.
    // W, space, X, Z, Tab, Backspace, Enter, Up, S, Down, A, Left, D, Right, R, keypad Enter.
    reg [15:0] pressed;
    assign keys = {pressed[14], pressed[12] | pressed[13], pressed[10] | pressed[11],
                   pressed[8] | pressed[9], pressed[0] | pressed[7], pressed[6] | pressed[15],
                   pressed[4] | pressed[5], pressed[3], pressed[0] | pressed[1] | pressed[2]};
    always @(posedge clk) begin
        if (reset) begin
            clk_sync <= 3'b111;
            dat_sync <= 3'b111;
            packet <= 0;
            count <= 0;
            timeout_count <= 0;
            released <= 0;
            extended <= 0;
            pressed <= 0;
        end else begin
            clk_sync <= {clk_sync[1:0], ps2_clk};
            dat_sync <= {dat_sync[1:0], ps2_dat};
            timeout_count <= timeout_count + 1'b1;
            if (&timeout_count) count <= 0;
            if (clk_sync[2:1] == 2'b10) begin
                timeout_count <= 0;
                if (count == 0) begin
                    if (!dat_sync[2]) begin packet[0] <= 0; count <= 1; end
                end else if (count == 10) begin
                    count <= 0;
                    if (dat_sync[2] && (^packet[9:1])) begin
                        code = packet[8:1];
                        if (code == 8'hf0) released <= 1;
                        else if (code == 8'he0) extended <= 1;
                        else begin
                            key_index = -1;
                            case (code)
                                8'h1d: if (!extended) key_index = 0; // W: up + jump
                                8'h29: if (!extended) key_index = 1; // Space: jump
                                8'h22: if (!extended) key_index = 2; // X: legacy jump
                                8'h1a: if (!extended) key_index = 3; // Z
                                8'h0d: if (!extended) key_index = 4; // Tab
                                8'h66: if (!extended) key_index = 5; // Backspace
                                8'h5a: key_index = extended ? 15 : 6; // Enter
                                8'h75: if (extended) key_index = 7;
                                8'h1b: if (!extended) key_index = 8; // S
                                8'h72: if (extended) key_index = 9;
                                8'h1c: if (!extended) key_index = 10; // A
                                8'h6b: if (extended) key_index = 11;
                                8'h23: if (!extended) key_index = 12; // D
                                8'h74: if (extended) key_index = 13;
                                8'h2d: if (!extended) key_index = 14; // R
                                default: ;
                            endcase
                            if (key_index >= 0) pressed[key_index] <= !released;
                            released <= 0;
                            extended <= 0;
                        end
                    end
                end else begin
                    packet[count] <= dat_sync[2];
                    count <= count + 1'b1;
                end
            end
        end
    end
endmodule
