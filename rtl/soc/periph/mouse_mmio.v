// Simulation-fed virtual mouse. This is NOT a physical PS/2/USB controller.
// +0x20 reads a stable click packet; +0x24 write 1 acknowledges that packet.
module mouse_mmio (
    input clk, input reset,
    input [31:0] packet, input valid, output ready,
    input acknowledge, output [31:0] rdata
);
    reg pending;
    reg [18:0] latched_packet;
    assign ready = !pending || acknowledge;
    assign rdata = pending ? {1'b1,12'b0,latched_packet} : 32'b0;
    always @(posedge clk) begin
        if (reset) begin pending <= 0; latched_packet <= 0; end
        else begin
            if (acknowledge) pending <= 0;
            if (valid && ready) begin
                latched_packet <= packet[18:0];
                pending <= 1;
            end
        end
    end
endmodule
