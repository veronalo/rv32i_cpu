module imem #(
    parameter integer ROM_BYTES = 4 * 1024 * 1024,
    parameter MEM_FILE = ""
) (
    input      [31:0] address,
    output     [31:0] rdata
);
    // 字节镜像，小端序。+rom=xxx.hex可在仿真时选择文件。
    reg [7:0] bytes [0:ROM_BYTES-1];
    reg [1023:0] rom_file;
    integer i;
    initial begin
        for (i = 0; i < ROM_BYTES; i = i + 1)
            bytes[i] = 8'b0;
        if (MEM_FILE != "")
            $readmemh(MEM_FILE, bytes);
        // 动态文件名仅用于仿真；综合时使用MEM_FILE参数。
        // synthesis translate_off
        if ($value$plusargs("rom=%s", rom_file))
            $readmemh(rom_file, bytes);
        // synthesis translate_on
    end
    assign rdata = (address <= ROM_BYTES - 4) ?
                   {bytes[address + 3], bytes[address + 2], bytes[address + 1], bytes[address]} : 32'b0;
endmodule
