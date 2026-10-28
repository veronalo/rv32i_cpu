module dmem #(
    parameter integer RAM_BYTES = 4 * 1024 * 1024
) (
    input             clk,
    input             selected,
    input      [31:0] offset,
    input      [31:0] wdata,
    input      [3:0]  wstrb,
    output     [31:0] rdata
);
    // offset是相对于RAM基址的字节地址；忽略低两位，返回完整对齐字。
    reg [31:0] words [0:RAM_BYTES/4-1];
    wire [31:0] word_index = offset >> 2;
    wire in_range = selected && (offset < RAM_BYTES);
    assign rdata = in_range ? words[word_index] : 32'b0;
    // RAM不在复位时清零。裸机启动代码负责初始化.data和.bss。
    always @(posedge clk) begin
        if (in_range) begin
            if (wstrb[0]) words[word_index][7:0]   <= wdata[7:0];
            if (wstrb[1]) words[word_index][15:8]  <= wdata[15:8];
            if (wstrb[2]) words[word_index][23:16] <= wdata[23:16];
            if (wstrb[3]) words[word_index][31:24] <= wdata[31:24];
        end
    end
endmodule
