module gpr (
    input         clk,
    input         reset,
    input  [4:0]  rs1_addr,
    input  [4:0]  rs2_addr,
    output [31:0] rs1_data,
    output [31:0] rs2_data,
    input         rd_wen,
    input  [4:0]  rd_addr,
    input  [31:0] rd_data
);
    reg [31:0] regs [0:31];
    integer i;

    // 双异步读端口；即使内部数组内容未知，读x0也必须为0。
    assign rs1_data = (rs1_addr == 0) ? 32'b0 : regs[rs1_addr];
    assign rs2_data = (rs2_addr == 0) ? 32'b0 : regs[rs2_addr];

    always @(posedge clk) begin
        if (reset) begin
            // ISA并不要求清零所有寄存器；这里为了教学和测试可重复性清零。
            for (i = 0; i < 32; i = i + 1)
                regs[i] <= 32'b0;
        end else if (rd_wen && rd_addr != 0) begin
            regs[rd_addr] <= rd_data;
        end
    end
endmodule
