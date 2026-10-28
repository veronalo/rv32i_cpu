`include "rv32i_defs.vh"
module rv32i_core #(
    parameter [31:0] RESET_VECTOR = 32'h0000_0000,
    parameter WITH_CLOCK_ENABLE = 0
) (
    input             clk,
    input             reset,
    input             clock_enable,
    // 两个端口都要求本周期组合返回数据，没有ready/valid等待机制。
    output     [31:0] imem_addr,
    input      [31:0] imem_rdata,
    // address是原始字节地址；rdata必须是{address[31:2],2'b00}处的字。
    output     [31:0] dmem_addr,
    output     [31:0] dmem_wdata,
    output     [3:0]  dmem_wstrb,
    output            dmem_ren,
    input      [31:0] dmem_rdata,
    output reg        halted,
    output reg [2:0]  halt_reason,
    // 上升沿之后表示刚刚执行的指令；fault时valid=0。
    output reg        commit_valid,
    output reg [31:0] commit_pc,
    output reg [31:0] commit_inst,
    output reg        commit_rd_wen,
    output reg [4:0]  commit_rd,
    output reg [31:0] commit_rd_data
);
    wire [31:0] current_pc, immediate, rs1_data, rs2_data;
    wire [31:0] alu_result, load_data, lsu_wdata;
    wire [4:0] rs1_addr, rs2_addr, rd_addr;
    wire [2:0] imm_type, branch_type, mem_type;
    wire [3:0] alu_op, lsu_wstrb;
    wire [1:0] alu_src_a, wb_sel;
    wire alu_src_b_imm, reg_wen, mem_read, mem_write;
    wire is_jal, is_jalr, is_ecall, is_ebreak, illegal;
    wire branch_taken, data_misaligned;
    reg [31:0] operand_a, writeback_data, next_pc;
    reg [2:0] stop_reason;

    wire [31:0] pc_plus4 = current_pc + 32'd4;
    wire [31:0] operand_b = alu_src_b_imm ? immediate : rs2_data;
    wire active = !reset && !halted;
    wire enabled = !WITH_CLOCK_ENABLE || clock_enable;
    wire fault = illegal || (current_pc[1:0] != 0) ||
                 (next_pc[1:0] != 0) || ((mem_read || mem_write) && data_misaligned);
    wire execute_ok = active && enabled && !fault && !is_ecall && !is_ebreak;
    wire rd_write = execute_ok && reg_wen && (rd_addr != 0);

    assign imem_addr = current_pc;
    assign dmem_addr = alu_result;
    assign dmem_wdata = lsu_wdata;
    assign dmem_wstrb = (execute_ok && mem_write) ? lsu_wstrb : 4'b0000;
    // Keep the request asserted during a read wait state, but don't retire it.
    assign dmem_ren = active && !fault && !is_ecall && !is_ebreak && mem_read;

    pc #(.RESET_VECTOR(RESET_VECTOR)) u_pc (
        .clk(clk), .reset(reset), .enable(execute_ok),
        .next_pc(next_pc), .current_pc(current_pc)
    );
    opcode_decoder u_decoder (
        .instruction(imem_rdata),
        .rs1_addr(rs1_addr), .rs2_addr(rs2_addr), .rd_addr(rd_addr),
        .imm_type(imm_type), .alu_op(alu_op), .alu_src_a(alu_src_a),
        .alu_src_b_imm(alu_src_b_imm), .wb_sel(wb_sel), .reg_wen(reg_wen),
        .mem_read(mem_read), .mem_write(mem_write), .mem_type(mem_type),
        .branch_type(branch_type), .is_jal(is_jal), .is_jalr(is_jalr),
        .is_ecall(is_ecall), .is_ebreak(is_ebreak), .illegal(illegal)
    );
    imm_gen u_imm_gen (
        .instruction(imem_rdata), .imm_type(imm_type), .immediate(immediate)
    );
    gpr u_gpr (
        .clk(clk), .reset(reset), .rs1_addr(rs1_addr), .rs2_addr(rs2_addr),
        .rs1_data(rs1_data), .rs2_data(rs2_data),
        .rd_wen(rd_write), .rd_addr(rd_addr), .rd_data(writeback_data)
    );
    alu u_alu (
        .operand_a(operand_a), .operand_b(operand_b), .operation(alu_op), .result(alu_result)
    );
    branch_comp u_branch_comp (
        .rs1_data(rs1_data), .rs2_data(rs2_data), .branch_type(branch_type), .taken(branch_taken)
    );
    load_store_unit u_lsu (
        .address(alu_result), .store_data(rs2_data), .memory_rdata(dmem_rdata),
        .mem_type(mem_type), .load_data(load_data), .memory_wdata(lsu_wdata),
        .memory_wstrb(lsu_wstrb), .misaligned(data_misaligned)
    );

    always @(*) begin
        case (alu_src_a)
            `SRC_A_PC:   operand_a = current_pc;
            `SRC_A_ZERO: operand_a = 32'b0;
            default:     operand_a = rs1_data;
        endcase
        case (wb_sel)
            `WB_MEM: writeback_data = load_data;
            `WB_PC4: writeback_data = pc_plus4;
            default: writeback_data = alu_result;
        endcase

        next_pc = pc_plus4;
        if (is_jalr)
            next_pc = (rs1_data + immediate) & 32'hffff_fffe;
        else if (is_jal || branch_taken)
            next_pc = current_pc + immediate;

        // 无C扩展时指令地址必须4字节对齐；不跳转的分支不检查其目标。
        stop_reason = `HALT_NONE;
        if (current_pc[1:0] != 0)
            stop_reason = `HALT_INST_ALIGN;
        else if (illegal)
            stop_reason = `HALT_ILLEGAL;
        else if (next_pc[1:0] != 0)
            stop_reason = `HALT_INST_ALIGN;
        else if ((mem_read || mem_write) && data_misaligned)
            stop_reason = `HALT_DATA_ALIGN;
        else if (is_ebreak)
            stop_reason = `HALT_EBREAK;
        else if (is_ecall)
            stop_reason = `HALT_ECALL;
    end

    always @(posedge clk) begin
        if (reset) begin
            halted <= 1'b0;
            halt_reason <= `HALT_NONE;
            commit_valid <= 1'b0;
            commit_pc <= 32'b0;
            commit_inst <= 32'b0;
            commit_rd_wen <= 1'b0;
            commit_rd <= 5'b0;
            commit_rd_data <= 32'b0;
        end else begin
            commit_valid <= active && enabled && !fault;
            commit_rd_wen <= rd_write;
            if (active && enabled) begin
                commit_pc <= current_pc;
                commit_inst <= imem_rdata;
                commit_rd <= rd_addr;
                commit_rd_data <= writeback_data;
                if (stop_reason != `HALT_NONE) begin
                    halted <= 1'b1;
                    halt_reason <= stop_reason;
                end
            end
        end
    end
endmodule
