`timescale 1ns/1ps
// 检查4MiB存储器的起点、旧边界、高地址、最后一个字和越界保护。
module memory_tb;
    localparam integer SIZE = 4 * 1024 * 1024;
    reg clk = 0;
    reg selected = 1;
    reg [31:0] offset = 0, wdata = 0, address = 0;
    reg [3:0] wstrb = 0;
    wire [31:0] rdata, rom_data;
    dmem ram (.clk(clk), .selected(selected), .offset(offset),
              .wdata(wdata), .wstrb(wstrb), .rdata(rdata));
    imem rom (.address(address), .rdata(rom_data));
    always #5 clk = ~clk;

    task store(input [31:0] addr, input [31:0] data, input [3:0] mask);
        @(negedge clk);
        offset = addr; wdata = data; wstrb = mask;
        @(negedge clk);
        wstrb = 0;
    endtask
    task check_ram(input [31:0] addr, input [31:0] expected);
        offset = addr;
        #1;
        if (rdata !== expected)
            $fatal(1, "RAM offset=%h got=%h expected=%h", addr, rdata, expected);
    endtask

    initial begin
        #1; // 等待ROM initial清零完成，再注入边界测试字节。
        if ($size(rom.bytes) != SIZE || $size(ram.words) != SIZE / 4)
            $fatal(1, "Memory defaults must both be 4MiB");
        rom.bytes[0] = 8'h11;
        rom.bytes[1] = 8'h22;
        rom.bytes[2] = 8'h33;
        rom.bytes[3] = 8'h44;
        rom.bytes[SIZE-4] = 8'hef;
        rom.bytes[SIZE-3] = 8'hcd;
        rom.bytes[SIZE-2] = 8'hab;
        rom.bytes[SIZE-1] = 8'h89;
        address = 0;
        #1;
        if (rom_data !== 32'h44332211) $fatal(1, "ROM first word");
        address = SIZE - 4;
        #1;
        if (rom_data !== 32'h89abcdef) $fatal(1, "ROM last word");
        address = SIZE;
        #1;
        if (rom_data !== 0) $fatal(1, "ROM out-of-range read");

        store(0, 32'h01020304, 4'b1111);
        store(32'h00010000, 32'ha5a55a5a, 4'b1111);
        store(32'h00200000, 32'h12345678, 4'b1111);
        store(SIZE-4, 32'h89abcdef, 4'b1111);
        check_ram(0, 32'h01020304);
        check_ram(32'h00010000, 32'ha5a55a5a);
        check_ram(32'h00200000, 32'h12345678);
        check_ram(SIZE-4, 32'h89abcdef);
        store(SIZE-4, 32'h76540000, 4'b1100);
        check_ram(SIZE-4, 32'h7654cdef);
        store(SIZE, 32'hdeadbeef, 4'b1111); // 不得回绕覆盖RAM起点。
        check_ram(SIZE, 0);
        check_ram(0, 32'h01020304);
        selected = 0;
        check_ram(SIZE-4, 0);
        $display("PASS: 4MiB ROM/RAM, first/last words, high addresses, byte masks, bounds");
        $finish;
    end
endmodule
