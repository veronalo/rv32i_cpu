`timescale 1ns/1ps
module keyboard_tb;
    reg clk=0, reset=1, ps2_clk=1, ps2_dat=1;
    wire [8:0] keys;
    ps2_keyboard dut(.*);
    always #5 clk=~clk;
    task send_byte(input [7:0] code,input bad_parity);
        reg [10:0] bits;
        begin
            bits={1'b1,((~^code)^bad_parity),code,1'b0};
            for(integer i=0;i<11;i=i+1)begin
                ps2_dat=bits[i];repeat(8)@(negedge clk);
                ps2_clk=0;repeat(8)@(negedge clk);
                ps2_clk=1;repeat(8)@(negedge clk);
            end
            ps2_dat=1;repeat(8)@(negedge clk);
        end
    endtask
    initial begin
        repeat(4)@(negedge clk);reset=0;
        send_byte(8'h5a,0);if(keys!==9'h008)$fatal(1,"enter make");
        send_byte(8'hf0,0);send_byte(8'h5a,0);if(keys!==0)$fatal(1,"enter break");
        send_byte(8'he0,0);send_byte(8'h74,0);if(keys!==9'h080)$fatal(1,"right make");
        send_byte(8'h22,0);if(keys!==9'h081)$fatal(1,"right+jump");
        send_byte(8'he0,0);send_byte(8'hf0,0);send_byte(8'h74,0);
        if(keys!==9'h001)$fatal(1,"right break");
        send_byte(8'h1a,1);if(keys!==9'h001)$fatal(1,"bad parity accepted");
        send_byte(8'hf0,0);send_byte(8'h22,0);if(keys!==0)$fatal(1,"jump break");
        send_byte(8'h1d,0);if(keys!==9'h011)$fatal(1,"W up+jump");
        send_byte(8'h29,0);if(keys!==9'h011)$fatal(1,"W+space");
        send_byte(8'h23,0);if(keys!==9'h091)$fatal(1,"W+D+space");
        send_byte(8'hf0,0);send_byte(8'h1d,0);if(keys!==9'h081)$fatal(1,"release W must retain space jump");
        send_byte(8'hf0,0);send_byte(8'h29,0);if(keys!==9'h080)$fatal(1,"release space");
        send_byte(8'he0,0);send_byte(8'h74,0);if(keys!==9'h080)$fatal(1,"D+right alias");
        send_byte(8'hf0,0);send_byte(8'h23,0);if(keys!==9'h080)$fatal(1,"release D must retain arrow");
        send_byte(8'he0,0);send_byte(8'hf0,0);send_byte(8'h74,0);if(keys!==0)$fatal(1,"release arrow");
        send_byte(8'h1c,0);if(keys!==9'h040)$fatal(1,"A left");
        send_byte(8'h1b,0);if(keys!==9'h060)$fatal(1,"S down");
        send_byte(8'hf0,0);send_byte(8'h1c,0);send_byte(8'hf0,0);send_byte(8'h1b,0);
        if(keys!==0)$fatal(1,"release AS");
        send_byte(8'h5a,0);send_byte(8'he0,0);send_byte(8'h5a,0);
        send_byte(8'hf0,0);send_byte(8'h5a,0);if(keys!==9'h008)$fatal(1,"keypad Enter alias");
        send_byte(8'he0,0);send_byte(8'hf0,0);send_byte(8'h5a,0);if(keys!==0)$fatal(1,"keypad Enter break");
        $display("PASS: PS/2 WASD, W/space jump, Enter, aliases, make/break, extended arrows, bad parity");$finish;
    end
endmodule
