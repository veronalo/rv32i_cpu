module pc #(
    parameter [31:0] RESET_VECTOR = 32'h0000_0000
) (
    input             clk,
    input             reset,
    input             enable,
    input      [31:0] next_pc,
    output reg [31:0] current_pc
);
    // 同步高有效复位；停机时enable=0，保留出错/结束指令的地址。
    always @(posedge clk) begin
        if (reset)
            current_pc <= RESET_VECTOR;
        else if (enable)
            current_pc <= next_pc;
    end
endmodule
