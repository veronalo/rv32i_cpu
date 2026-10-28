`timescale 1ns/1ps
module mouse_tb;
    reg clk=0,reset=1,valid=0,acknowledge=0;
    reg [31:0] packet=0;
    wire ready;
    wire [31:0] rdata;
    mouse_mmio dut(.*);
    always #5 clk=~clk;
    initial begin
        @(negedge clk);reset=0;
        if(!ready||rdata!==0)$fatal(1,"reset");
        packet=32'h8002283e;valid=1;
        @(negedge clk);valid=0;
        if(ready||rdata!==packet)$fatal(1,"capture");
        packet=32'h80045078;valid=1;
        repeat(8)@(negedge clk);
        if(rdata!==32'h8002283e)$fatal(1,"must hold until ACK");
        acknowledge=1;
        @(negedge clk);valid=0;acknowledge=0;#1;
        if(ready||rdata!==packet)$fatal(1,"ACK + new transfer");
        acknowledge=1;
        @(negedge clk);acknowledge=0;
        if(!ready||rdata!==0)$fatal(1,"consumed");
        reset=1;valid=1;
        @(negedge clk);reset=0;valid=0;
        if(rdata!==0)$fatal(1,"reset precedence");
        $display("PASS: mouse latch, backpressure, ACK, concurrent replacement, reset");$finish;
    end
endmodule
