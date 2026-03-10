// ============================================================
// ping_pong_bram.sv
//
// Two 1280x32 dual-port BRAMs for ping-pong line buffering.
// One 32-bit word per pixel (no packing). RGB444 in [11:0].
//
// Write port : axi_clk domain  (DMA or TPG)
// Read  port : pix_clk domain  (vga_sync_720p)
//
// buf_sel_axi (axi_clk domain):
//   0 → DMA/TPG writes BRAM_A,  VGA reads BRAM_B
//   1 → DMA/TPG writes BRAM_B,  VGA reads BRAM_A
//
// buf_sel_pix (pix_clk domain):
//   0 → rd_data = BRAM_B output
//   1 → rd_data = BRAM_A output
//
// TPG mode: both BRAMs written simultaneously (identical data).
// ============================================================
`timescale 1ns/1ps

module ping_pong_bram
(
    // ---- Write port (axi_clk domain) ----------------------------
    input  logic        axi_clk,

    input  logic [31:0] wr_data,          // pixel word (RGB444 in [11:0])
    input  logic [10:0] wr_addr,          // 0-1279
    input  logic        dma_wr_en,        // from DMA engine
    input  logic        tpg_wr_en,        // from test pattern gen
    input  logic        buf_sel_axi,      // synchronised buf_sel
    input  logic        use_test_pattern, // selects TPG vs DMA

    // ---- Read port (pix_clk domain) -----------------------------
    input  logic        pix_clk,
    input  logic [10:0] rd_addr,          // 0-1279, from vga_sync (x_reg)
    input  logic        buf_sel_pix,      // direct buf_sel from vga_sync

    output logic [31:0] rd_data           // registered output (1-cycle latency)
);

    // ---------------------------------------------------------------
    // Write enable generation
    // ---------------------------------------------------------------
    logic wr_en_a, wr_en_b;

    always_comb begin
        if (use_test_pattern) begin
            // TPG writes both BRAMs simultaneously
            wr_en_a = tpg_wr_en;
            wr_en_b = tpg_wr_en;
        end else begin
            // DMA writes only the inactive buffer
            wr_en_a = dma_wr_en & ~buf_sel_axi;
            wr_en_b = dma_wr_en &  buf_sel_axi;
        end
    end

    // ---------------------------------------------------------------
    // Inferred dual-port BRAMs (Vivado maps to RAMB36E1 SDP)
    // ---------------------------------------------------------------
    (* ram_style = "block" *) logic [31:0] bram_a [0:1279];
    (* ram_style = "block" *) logic [31:0] bram_b [0:1279];

    logic [31:0] rd_data_a, rd_data_b;

    // BRAM_A write (axi_clk) / read (pix_clk)
    always_ff @(posedge axi_clk)
        if (wr_en_a) bram_a[wr_addr] <= wr_data;

    always_ff @(posedge pix_clk)
        rd_data_a <= bram_a[rd_addr];

    // BRAM_B write (axi_clk) / read (pix_clk)
    always_ff @(posedge axi_clk)
        if (wr_en_b) bram_b[wr_addr] <= wr_data;

    always_ff @(posedge pix_clk)
        rd_data_b <= bram_b[rd_addr];

    // ---------------------------------------------------------------
    // Read mux (pix_clk domain)
    // buf_sel_pix=0 → VGA reads BRAM_B (DMA was writing BRAM_A)
    // buf_sel_pix=1 → VGA reads BRAM_A (DMA was writing BRAM_B)
    // ---------------------------------------------------------------
    always_ff @(posedge pix_clk)
        rd_data <= buf_sel_pix ? rd_data_a : rd_data_b;

endmodule
