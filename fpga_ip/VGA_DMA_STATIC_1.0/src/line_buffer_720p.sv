// ============================================================
// line_buffer_720p.sv
// Dual-clock AXI-Stream FIFO bridge for 720p.
//
// Write side : AXI / DMA clock (150 MHz, HP0 port)
// Read  side : pixel clock     (74.25 MHz)
//
// Depth  : 2048 words  (one 1280-pixel line + headroom)
// Width  : CD+1 bits   (colour[CD-1:0] + frame_start[0])
//
// Stream handshake:
//   si_valid / si_ready  – write (DMA → FIFO)
//   so_valid / so_ready  – read  (FIFO → vga_sync_720p)
// ============================================================
module line_buffer_720p
#(parameter CD = 12)
(
    input  logic            clk_stream_in,   // DMA clock
    input  logic            clk_stream_out,  // pixel clock
    input  logic            reset,
    // stream in (sink)
    input  logic [CD:0]     si_data,         // {colour, frame_start}
    input  logic            si_valid,
    output logic            si_ready,
    // stream out (source)
    output logic [CD:0]     so_data,
    output logic            so_valid,
    input  logic            so_ready,
    // occupancy (write-side clock domain) for DMA trigger
    output logic [10:0]     wrcount
);

    localparam DW = CD + 1;

    logic almost_full, empty;
    logic [10:0] rdcount;

    bram_fifo_720p #(.DW(DW)) fifo_unit (
        .reset        (reset),
        // read side
        .clk_rd       (clk_stream_out),
        .rd_data      (so_data),
        .rd_ack       (so_ready),
        .empty        (empty),
        .almost_empty (),
        .rdcount      (rdcount),
        // write side
        .clk_wr       (clk_stream_in),
        .wr_data      (si_data),
        .wr_en        (si_valid & ~almost_full),
        .full         (),
        .almost_full  (almost_full),
        .wrcount      (wrcount)
    );

    // Back-pressure: stop filling when FIFO is almost full
    assign si_ready = ~almost_full;
    assign so_valid = ~empty;

endmodule
