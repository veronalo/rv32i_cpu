`include "rv32i_defs.vh"
module alu (
    input      [31:0] operand_a,
    input      [31:0] operand_b,
    input      [3:0]  operation,
    output reg [31:0] result
);
    always @(*) begin
        case (operation)
            `ALU_ADD:  result = operand_a + operand_b;
            `ALU_SUB:  result = operand_a - operand_b;
            `ALU_SLL:  result = operand_a << operand_b[4:0];
            `ALU_SLT:  result = {31'b0, ($signed(operand_a) < $signed(operand_b))};
            `ALU_SLTU: result = {31'b0, (operand_a < operand_b)};
            `ALU_XOR:  result = operand_a ^ operand_b;
            `ALU_SRL:  result = operand_a >> operand_b[4:0];
            `ALU_SRA:  result = $signed(operand_a) >>> operand_b[4:0];
            `ALU_OR:   result = operand_a | operand_b;
            `ALU_AND:  result = operand_a & operand_b;
            default:  result = 32'b0;
        endcase
    end
endmodule
