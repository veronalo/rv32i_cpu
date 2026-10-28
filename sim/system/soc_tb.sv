`timescale 1ns/1ps
module soc_tb;
    reg clk = 0;
    reg reset = 1;
    wire halted, test_done;
    wire [2:0] halt_reason;
    wire [31:0] test_status;
    reg [31:0] expected = 0;
    reg unused_expected_arg;
    soc_top dut (.clk(clk), .reset(reset), .halted(halted), .halt_reason(halt_reason),
        .test_done(test_done), .test_status(test_status), .ps2_clk(1'b1), .ps2_dat(1'b1),
        .buttons(5'b0), .scripted_keys(9'b0), .mouse_packet(32'b0), .mouse_valid(1'b0), .mouse_ready(),
        .mouse_position(20'b0));
    always #5 clk = ~clk;

    // 仅用于仿真观察，不增加CPU对外的硬件端口。
    task dump_gpr;
        begin
            $display("--- RTL final registers (hex / signed decimal) ---");
            $display("PC = 0x%08h  halt_reason=%0d", dut.imem_addr, halt_reason);
            for (integer i = 0; i < 32; i = i + 1)
                $display("x%02d = 0x%08h  %0d", i, dut.u_core.u_gpr.regs[i],
                         $signed(dut.u_core.u_gpr.regs[i]));
            $display("RESULT a1/x11 = %0d (0x%08h), MMIO = %0d, expected = %0d",
                     $signed(dut.u_core.u_gpr.regs[11]), dut.u_core.u_gpr.regs[11],
                     $signed(test_status), $signed(expected));
        end
    endtask
    initial begin
        unused_expected_arg = $value$plusargs("expected=%d", expected);
        if ($test$plusargs("trace")) begin
            $dumpfile("build/soc.vcd");
            $dumpvars(0, soc_tb);
        end
        repeat (2) @(negedge clk);
        reset = 0;
        // 等待稳定的下降沿读取状态，避免与RTL上升沿更新发生竞态。
        for (integer cycle = 0; cycle < 100000; cycle = cycle + 1) begin
            @(negedge clk);
            if (halted) begin
                dump_gpr();
                if (halt_reason !== 3'd1 || test_done !== 1'b1 || test_status !== expected ||
                    dut.u_core.u_gpr.regs[10] !== 32'b0 ||
                    dut.u_core.u_gpr.regs[11] !== expected)
                    $fatal(1, "C program failed: reason=%0d done=%b status=%0d pc=%h",
                           halt_reason, test_done, test_status, dut.imem_addr);
                $display("PASS: bare-metal C program, %0d cycles, result=%0d, exit=0",
                         cycle + 1, $signed(test_status));
                $finish;
            end
        end
        dump_gpr();
        $fatal(1, "C program timed out at pc=%h", dut.imem_addr);
    end
endmodule
