`ifndef RV32I_DEFS_VH
`define RV32I_DEFS_VH

// 内部控制信号编码；这些不是指令本身的编码。
`define IMM_I 3'd0
`define IMM_S 3'd1
`define IMM_B 3'd2
`define IMM_U 3'd3
`define IMM_J 3'd4

`define ALU_ADD  4'd0
`define ALU_SUB  4'd1
`define ALU_SLL  4'd2
`define ALU_SLT  4'd3
`define ALU_SLTU 4'd4
`define ALU_XOR  4'd5
`define ALU_SRL  4'd6
`define ALU_SRA  4'd7
`define ALU_OR   4'd8
`define ALU_AND  4'd9

`define SRC_A_RS1  2'd0
`define SRC_A_PC   2'd1
`define SRC_A_ZERO 2'd2
`define WB_ALU 2'd0
`define WB_MEM 2'd1
`define WB_PC4 2'd2

`define BR_NONE 3'd0
`define BR_EQ   3'd1
`define BR_NE   3'd2
`define BR_LT   3'd3
`define BR_GE   3'd4
`define BR_LTU  3'd5
`define BR_GEU  3'd6

// 访存类型沿用指令的funct3，便于对照指令手册。
`define MEM_BYTE  3'b000
`define MEM_HALF  3'b001
`define MEM_WORD  3'b010
`define MEM_BYTEU 3'b100
`define MEM_HALFU 3'b101

// 教学运行环境的停止原因，不是RISC-V的mcause编码。
`define HALT_NONE       3'd0
`define HALT_EBREAK     3'd1
`define HALT_ECALL      3'd2
`define HALT_ILLEGAL    3'd3
`define HALT_INST_ALIGN 3'd4
`define HALT_DATA_ALIGN 3'd5
`endif
