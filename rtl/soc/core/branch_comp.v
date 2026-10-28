`include "rv32i_defs.vh"
module branch_comp (
    input      [31:0] rs1_data,
    input      [31:0] rs2_data,
    input      [2:0]  branch_type,
    output reg       taken
);
    always @(*) begin
        case (branch_type)
            `BR_EQ:  taken = (rs1_data == rs2_data);
            `BR_NE:  taken = (rs1_data != rs2_data);
            `BR_LT:  taken = ($signed(rs1_data) < $signed(rs2_data));
            `BR_GE:  taken = ($signed(rs1_data) >= $signed(rs2_data));
            `BR_LTU: taken = (rs1_data < rs2_data);
            `BR_GEU: taken = (rs1_data >= rs2_data);
            default: taken = 1'b0;
        endcase
    end
endmodule
