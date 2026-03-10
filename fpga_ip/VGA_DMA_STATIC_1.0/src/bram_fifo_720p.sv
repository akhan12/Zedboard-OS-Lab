// ============================================================
// bram_fifo_720p.sv
// Xilinx dual-clock BRAM FIFO for 720p line buffer.
//
// Width  : DW bits (default 13 = 12-bit colour + 1 frame_start)
// Depth  : 2048 words  (covers one 1280-pixel line + margin)
// Mode   : First-Word Fall-Through (FWFT / show-ahead)
//
// Uses FIFO_DUALCLOCK_MACRO exactly like bram_fifo_fpro.sv but
// with a larger depth (18Kb → need to use 36Kb primitive).
// Xilinx 36Kb BRAM FIFO at 13 bits wide gives 2048 locations.
// ============================================================
`timescale 1ns/1ps

module bram_fifo_720p
#(parameter DW = 13)   // data width (colour + frame_start)
(
    input  logic            reset,      // synchronous reset (active high)
    // read port  (74.25 MHz pixel clock domain)
    input  logic            clk_rd,
    output logic [DW-1:0]   rd_data,
    input  logic            rd_ack,
    output logic            empty,
    output logic            almost_empty,
    output logic [10:0]     rdcount,
    // write port (AXI / DMA clock domain)
    input  logic            clk_wr,
    input  logic [DW-1:0]   wr_data,
    input  logic            wr_en,
    output logic            full,
    output logic            almost_full,
    output logic [10:0]     wrcount
);

    // Threshold: flag almost_full when 128 slots remain
    // so upstream has time to react (same cushion as original).
    localparam ALMOST_FULL_OFFSET  = 11'h080;
    localparam ALMOST_EMPTY_OFFSET = 11'h080;

    FIFO_DUALCLOCK_MACRO #(
        .ALMOST_FULL_OFFSET  (ALMOST_FULL_OFFSET),
        .ALMOST_EMPTY_OFFSET (ALMOST_EMPTY_OFFSET),
        .DATA_WIDTH          (DW),
        .DEVICE              ("7SERIES"),
        .FIFO_SIZE           ("36Kb"),   // 2048 locations at ≤18-bit width
        .FIRST_WORD_FALL_THROUGH ("TRUE")
    ) fifo_inst (
        .RST          (reset),
        // read side
        .RDCLK        (clk_rd),
        .DO           (rd_data),
        .RDEN         (rd_ack),         // consume one word per cycle when high
        .EMPTY        (empty),
        .ALMOSTEMPTY  (almost_empty),
        .RDCOUNT      (rdcount),
        // write side
        .WRCLK        (clk_wr),
        .DI           (wr_data),
        .WREN         (wr_en),
        .FULL         (full),
        .ALMOSTFULL   (almost_full),
        .WRCOUNT      (wrcount),
        // unused
        .RDERR        (),
        .WRERR        ()
    );

endmodule
