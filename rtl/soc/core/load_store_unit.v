`include "rv32i_defs.vh"
module load_store_unit (
    input      [31:0] address,
    input      [31:0] store_data,
    input      [31:0] memory_rdata,
    input      [2:0]  mem_type,
    output reg [31:0] load_data,
    output     [31:0] memory_wdata,
    output reg [3:0]  memory_wstrb,
    output reg        misaligned
);
    // 总线返回包含address的“对齐32位字”，LSU选择其中的字节通道。
    wire [4:0] shift_bits = {address[1:0], 3'b000};
    wire [31:0] shifted = memory_rdata >> shift_bits;
    // 这里只用地址低位和移位后的低半字；高位由核心/SoC处理。
    wire unused_bits = &{1'b0, address[31:2], shifted[31:16]};
    assign memory_wdata = store_data << shift_bits;

    always @(*) begin
        load_data = 32'b0;
        memory_wstrb = 4'b0000;
        misaligned = 1'b0;
        case (mem_type)
            `MEM_BYTE: begin
                load_data = {{24{shifted[7]}}, shifted[7:0]};
                memory_wstrb = 4'b0001 << address[1:0];
            end
            `MEM_BYTEU: load_data = {24'b0, shifted[7:0]};
            `MEM_HALF: begin
                load_data = {{16{shifted[15]}}, shifted[15:0]};
                memory_wstrb = 4'b0011 << address[1:0];
                misaligned = address[0];
            end
            `MEM_HALFU: begin
                load_data = {16'b0, shifted[15:0]};
                misaligned = address[0];
            end
            `MEM_WORD: begin
                load_data = memory_rdata;
                memory_wstrb = 4'b1111;
                misaligned = (address[1:0] != 2'b00);
            end
            default: ; // 非法类型由decoder统一拒绝
        endcase
    end
endmodule
