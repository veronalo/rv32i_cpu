`include "rv32i_defs.vh"
module imm_gen (
    input      [31:0] instruction,
    input      [2:0]  imm_type,
    output reg [31:0] immediate
);
    // opcode低7位不参与立即数拼接，保留完整指令端口方便阅读。
    wire unused_opcode = &{1'b0, instruction[6:0]};
    always @(*) begin
        case (imm_type)
            `IMM_I: immediate = {{20{instruction[31]}}, instruction[31:20]};
            `IMM_S: immediate = {{20{instruction[31]}}, instruction[31:25], instruction[11:7]};
            `IMM_B: immediate = {{19{instruction[31]}}, instruction[31], instruction[7],
                                instruction[30:25], instruction[11:8], 1'b0};
            `IMM_U: immediate = {instruction[31:12], 12'b0};
            `IMM_J: immediate = {{11{instruction[31]}}, instruction[31], instruction[19:12],
                                instruction[20], instruction[30:21], 1'b0};
            default: immediate = 32'b0;
        endcase
    end
endmodule
