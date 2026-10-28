`include "rv32i_defs.vh"
module opcode_decoder (
    input      [31:0] instruction,
    output     [4:0]  rs1_addr,
    output     [4:0]  rs2_addr,
    output     [4:0]  rd_addr,
    output reg [2:0]  imm_type,
    output reg [3:0]  alu_op,
    output reg [1:0]  alu_src_a,
    output reg        alu_src_b_imm,
    output reg [1:0]  wb_sel,
    output reg        reg_wen,
    output reg        mem_read,
    output reg        mem_write,
    output     [2:0]  mem_type,
    output reg [2:0]  branch_type,
    output reg        is_jal,
    output reg        is_jalr,
    output reg        is_ecall,
    output reg        is_ebreak,
    output reg        illegal
);
    wire [6:0] opcode = instruction[6:0];
    wire [2:0] funct3 = instruction[14:12];
    wire [6:0] funct7 = instruction[31:25];
    assign rs1_addr = instruction[19:15];
    assign rs2_addr = instruction[24:20];
    assign rd_addr = instruction[11:7];
    assign mem_type = funct3;

    always @(*) begin
        // 默认无副作用，所有组合输出均赋值，避免推断锁存器。
        imm_type = `IMM_I;
        alu_op = `ALU_ADD;
        alu_src_a = `SRC_A_RS1;
        alu_src_b_imm = 1'b0;
        wb_sel = `WB_ALU;
        reg_wen = 1'b0;
        mem_read = 1'b0;
        mem_write = 1'b0;
        branch_type = `BR_NONE;
        is_jal = 1'b0;
        is_jalr = 1'b0;
        is_ecall = 1'b0;
        is_ebreak = 1'b0;
        illegal = 1'b0;

        case (opcode)
            7'b0110111: begin // LUI: 0 + U-immediate
                imm_type = `IMM_U;
                alu_src_a = `SRC_A_ZERO;
                alu_src_b_imm = 1'b1;
                reg_wen = 1'b1;
            end
            7'b0010111: begin // AUIPC: PC + U-immediate
                imm_type = `IMM_U;
                alu_src_a = `SRC_A_PC;
                alu_src_b_imm = 1'b1;
                reg_wen = 1'b1;
            end
            7'b1101111: begin // JAL
                imm_type = `IMM_J;
                is_jal = 1'b1;
                wb_sel = `WB_PC4;
                reg_wen = 1'b1;
            end
            7'b1100111: begin // JALR，只有funct3=000合法
                is_jalr = 1'b1;
                alu_src_b_imm = 1'b1;
                wb_sel = `WB_PC4;
                reg_wen = 1'b1;
                illegal = (funct3 != 3'b000);
            end
            7'b1100011: begin // 六种条件分支
                imm_type = `IMM_B;
                case (funct3)
                    3'b000: branch_type = `BR_EQ;
                    3'b001: branch_type = `BR_NE;
                    3'b100: branch_type = `BR_LT;
                    3'b101: branch_type = `BR_GE;
                    3'b110: branch_type = `BR_LTU;
                    3'b111: branch_type = `BR_GEU;
                    default: illegal = 1'b1;
                endcase
            end
            7'b0000011: begin // LB LH LW LBU LHU
                alu_src_b_imm = 1'b1;
                mem_read = 1'b1;
                wb_sel = `WB_MEM;
                reg_wen = 1'b1;
                case (funct3)
                    3'b000, 3'b001, 3'b010, 3'b100, 3'b101: ;
                    default: illegal = 1'b1;
                endcase
            end
            7'b0100011: begin // SB SH SW
                imm_type = `IMM_S;
                alu_src_b_imm = 1'b1;
                mem_write = 1'b1;
                case (funct3)
                    3'b000, 3'b001, 3'b010: ;
                    default: illegal = 1'b1;
                endcase
            end
            7'b0010011: begin // 九种OP-IMM
                alu_src_b_imm = 1'b1;
                reg_wen = 1'b1;
                case (funct3)
                    3'b000: alu_op = `ALU_ADD;
                    3'b010: alu_op = `ALU_SLT;
                    3'b011: alu_op = `ALU_SLTU;
                    3'b100: alu_op = `ALU_XOR;
                    3'b110: alu_op = `ALU_OR;
                    3'b111: alu_op = `ALU_AND;
                    3'b001: begin
                        alu_op = `ALU_SLL;
                        illegal = (funct7 != 7'b0000000);
                    end
                    3'b101: begin
                        alu_op = instruction[30] ? `ALU_SRA : `ALU_SRL;
                        illegal = (funct7 != 7'b0000000 && funct7 != 7'b0100000);
                    end
                    default: illegal = 1'b1;
                endcase
            end
            7'b0110011: begin // 十种OP，明确拒绝M扩展等其他编码
                reg_wen = 1'b1;
                case ({funct7, funct3})
                    10'b0000000_000: alu_op = `ALU_ADD;
                    10'b0100000_000: alu_op = `ALU_SUB;
                    10'b0000000_001: alu_op = `ALU_SLL;
                    10'b0000000_010: alu_op = `ALU_SLT;
                    10'b0000000_011: alu_op = `ALU_SLTU;
                    10'b0000000_100: alu_op = `ALU_XOR;
                    10'b0000000_101: alu_op = `ALU_SRL;
                    10'b0100000_101: alu_op = `ALU_SRA;
                    10'b0000000_110: alu_op = `ALU_OR;
                    10'b0000000_111: alu_op = `ALU_AND;
                    default: illegal = 1'b1;
                endcase
            end
            7'b0001111: begin
                // 单核、顺序访存、无缓存：基础FENCE按NOP处理。
                // FENCE.I属于Zifencei，不在本项目RV32I范围内。
                illegal = (funct3 != 3'b000);
            end
            7'b1110011: begin
                case (instruction)
                    32'h00000073: is_ecall = 1'b1;
                    32'h00100073: is_ebreak = 1'b1;
                    default: illegal = 1'b1; // 不支持CSR和特权指令
                endcase
            end
            default: illegal = 1'b1;
        endcase
    end
endmodule
