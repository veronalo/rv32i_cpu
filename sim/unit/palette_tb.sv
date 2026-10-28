// Independent golden mapping from the original palette case implementation.
module palette_tb;
    mario_video dut (
        .clk(1'b0), .reset(1'b1), .selected(1'b0), .address(16'b0),
        .wdata(32'b0), .wstrb(4'b0), .rdata(), .hsync(), .vsync(),
        .valid(), .red(), .green(), .blue(), .frame_count()
    );
    function [23:0] expected(input [5:0] index);
        case (index)
             0:expected=24'h7c7c7c; 1:expected=24'h0000fc; 2:expected=24'h0000bc; 3:expected=24'h4428bc;
             4:expected=24'h940084; 5:expected=24'ha80020; 6:expected=24'ha81000; 7:expected=24'h881400;
             8:expected=24'h503000; 9:expected=24'h007800;10:expected=24'h006800;11:expected=24'h005800;
            12:expected=24'h004058;13,14,15:expected=0;
            16:expected=24'hbcbcbc;17:expected=24'h0078f8;18:expected=24'h0058f8;19:expected=24'h6844fc;
            20:expected=24'hd800cc;21:expected=24'he40058;22:expected=24'hf83800;23:expected=24'he45c10;
            24:expected=24'hac7c00;25:expected=24'h00b800;26:expected=24'h00a800;27:expected=24'h00a844;
            28:expected=24'h008888;29,30,31:expected=0;
            32:expected=24'hf8f8f8;33:expected=24'h3cbcfc;34:expected=24'h6888fc;35:expected=24'h9878f8;
            36:expected=24'hf878f8;37:expected=24'hf85898;38:expected=24'hf87858;39:expected=24'hfca044;
            40:expected=24'hf8b800;41:expected=24'hb8f818;42:expected=24'h58d854;43:expected=24'h58f898;
            44:expected=24'h00e8d8;45:expected=24'h787878;46,47:expected=0;
            48:expected=24'hfcfcfc;49:expected=24'ha4e4fc;50:expected=24'hb8b8f8;51:expected=24'hd8b8f8;
            52:expected=24'hf8b8f8;53:expected=24'hf8a4c0;54:expected=24'hf0d0b0;55:expected=24'hfce0a8;
            56:expected=24'hf8d878;57:expected=24'hd8f878;58:expected=24'hb8f8b8;59:expected=24'hb8f8d8;
            60:expected=24'h00fcfc;61:expected=24'hf8d8f8;default:expected=0;
        endcase
    endfunction
    integer i;
    initial begin
        #1;
        for (i=0; i<64; i=i+1)
            if (dut.rgb(6'(i)) !== expected(6'(i)))
                $fatal(1, "palette index %0d differs from golden mapping", i);
        $display("PASS: all 64 palette entries match original mapping");
        $finish;
    end
endmodule
