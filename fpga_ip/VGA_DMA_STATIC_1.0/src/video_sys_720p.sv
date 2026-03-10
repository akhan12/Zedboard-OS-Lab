// ============================================================
// video_sys_720p.sv
// Top-level video pipeline for 720p ping-pong BRAM architecture.
//
// Architecture:
//
//   [DDR via AXI Master]    [Test Pattern Gen]
//          |                       |
//          | bram_wr_*             | tpg_wr_*
//          v                       v
//                ping_pong_bram
//                (axi_clk write / pix_clk read)
//                       |
//                  rd_data [31:0]
//                       |
//                 vga_sync_720p
//                (demux x[0], hsync/vsync/rgb)
//
// CDC:
//   buf_sel (pix_clk) → 2-FF sync → buf_sel_axi (axi_clk)
//   Edge detect on buf_sel_axi → line_swap pulse → DMA trigger
//   frame_toggle (pix_clk) → 2-FF sync → frame_sync pulse → DMA re-sync
// ============================================================
`timescale 1ns/1ps

module video_sys_720p
#(
    parameter integer C_M_AXI_ADDR_WIDTH   = 32,
    parameter integer C_M_AXI_DATA_WIDTH   = 32,
    parameter integer C_M_AXI_ID_WIDTH     = 1,
    parameter integer C_M_AXI_ARUSER_WIDTH = 0,
    parameter integer C_M_AXI_RUSER_WIDTH  = 0,
    parameter integer CD                   = 12
)
(
    // ---- Clocks / reset ------------------------------------------
    input  logic            axi_clk,
    input  logic            pix_clk,
    input  logic            axi_resetn,

    // ---- Control from AXI slave registers ------------------------
    input  logic [31:0]     frame_base_addr,
    input  logic            dma_enable,
    input  logic            use_test_pattern,
    input  logic [1:0]      tp_colour_sel,

    // ---- Status to AXI slave -------------------------------------
    output logic            dma_active,
    output logic            dma_error,
    output wire  [31:0]     first_rdata,

    // ---- AXI4 Read Address Channel -------------------------------
    output wire [C_M_AXI_ID_WIDTH-1:0]      M_AXI_ARID,
    output wire [C_M_AXI_ADDR_WIDTH-1:0]    M_AXI_ARADDR,
    output wire [7:0]                        M_AXI_ARLEN,
    output wire [2:0]                        M_AXI_ARSIZE,
    output wire [1:0]                        M_AXI_ARBURST,
    output wire                              M_AXI_ARLOCK,
    output wire [3:0]                        M_AXI_ARCACHE,
    output wire [2:0]                        M_AXI_ARPROT,
    output wire [3:0]                        M_AXI_ARQOS,
    output wire [C_M_AXI_ARUSER_WIDTH-1:0]  M_AXI_ARUSER,
    output wire                              M_AXI_ARVALID,
    input  wire                              M_AXI_ARREADY,

    // ---- AXI4 Read Data Channel ----------------------------------
    input  wire [C_M_AXI_ID_WIDTH-1:0]      M_AXI_RID,
    input  wire [C_M_AXI_DATA_WIDTH-1:0]    M_AXI_RDATA,
    input  wire [1:0]                        M_AXI_RRESP,
    input  wire                              M_AXI_RLAST,
    input  wire [C_M_AXI_RUSER_WIDTH-1:0]   M_AXI_RUSER,
    input  wire                              M_AXI_RVALID,
    output wire                              M_AXI_RREADY,

    // ---- VGA / HDMI outputs --------------------------------------
    output logic            hsync,
    output logic            vsync,
    output logic [CD-1:0]   rgb
);

    wire reset = ~axi_resetn;

    // ---------------------------------------------------------------
    // buf_sel CDC: pix_clk → axi_clk
    // vga_sync_720p toggles buf_sel at end of each active line.
    // 2-FF synchroniser + edge detector → 1-cycle line_swap_axi pulse.
    // ---------------------------------------------------------------
    wire buf_sel_pix;   // from vga_sync_720p

    reg buf_s1, buf_s2, buf_s3;
    always @(posedge axi_clk or posedge reset) begin
        if (reset) begin
            buf_s1 <= 1'b0;
            buf_s2 <= 1'b0;
            buf_s3 <= 1'b0;
        end else begin
            buf_s1 <= buf_sel_pix;
            buf_s2 <= buf_s1;
            buf_s3 <= buf_s2;
        end
    end

    wire buf_sel_axi  = buf_s2;               // stable in axi_clk
    wire line_swap_axi = buf_s2 ^ buf_s3;     // 1-cycle pulse per line swap

    // ---------------------------------------------------------------
    // frame_sync CDC: pix_clk toggle → axi_clk 1-cycle pulse
    // Toggled at (x==0, y==0) in vga_sync scan counter.
    // ---------------------------------------------------------------
    localparam HT = 1650;
    localparam VT = 750;

    reg [10:0] scan_h, scan_v;
    always @(posedge pix_clk or posedge reset) begin
        if (reset) begin
            scan_h <= 11'd0;
            scan_v <= 11'd0;
        end else begin
            if (scan_h == HT-1) begin
                scan_h <= 11'd0;
                scan_v <= (scan_v == VT-1) ? 11'd0 : scan_v + 1'b1;
            end else
                scan_h <= scan_h + 1'b1;
        end
    end

    reg frame_toggle_pix;
    always @(posedge pix_clk or posedge reset)
        if (reset) frame_toggle_pix <= 1'b0;
        else if (scan_h == 11'd0 && scan_v == 11'd0)
            frame_toggle_pix <= ~frame_toggle_pix;

    reg fsync_s1, fsync_s2, fsync_s3;
    always @(posedge axi_clk or posedge reset) begin
        if (reset) begin
            fsync_s1 <= 1'b0;
            fsync_s2 <= 1'b0;
            fsync_s3 <= 1'b0;
        end else begin
            fsync_s1 <= frame_toggle_pix;
            fsync_s2 <= fsync_s1;
            fsync_s3 <= fsync_s2;
        end
    end

    wire frame_sync_axi = fsync_s2 ^ fsync_s3;

    // ---------------------------------------------------------------
    // Ping-pong BRAM
    // ---------------------------------------------------------------
    wire [10:0] rd_addr;
    wire [31:0] rd_data;

    wire [31:0] dma_wr_data;
    wire [10:0] dma_wr_addr;
    wire        dma_wr_en;

    wire [31:0] tpg_wr_data;
    wire [10:0] tpg_wr_addr;
    wire        tpg_wr_en;

    // ping_pong_bram handles the TPG/DMA mux internally via separate wr_en ports
    // and use_test_pattern. Addr/data are muxed here since both share one port.
    wire [31:0] bram_wr_data = use_test_pattern ? tpg_wr_data : dma_wr_data;
    wire [10:0] bram_wr_addr = use_test_pattern ? tpg_wr_addr : dma_wr_addr;

    ping_pong_bram pp_bram (
        .axi_clk         (axi_clk),
        .wr_data         (bram_wr_data),
        .wr_addr         (bram_wr_addr),
        .dma_wr_en       (dma_wr_en),
        .tpg_wr_en       (tpg_wr_en),
        .buf_sel_axi     (buf_sel_axi),
        .use_test_pattern(use_test_pattern),
        .pix_clk         (pix_clk),
        .rd_addr         (rd_addr),
        .buf_sel_pix     (buf_sel_pix),
        .rd_data         (rd_data)
    );

    // ---------------------------------------------------------------
    // DMA engine
    // ---------------------------------------------------------------
    video_dma_engine #(
        .C_M_AXI_ADDR_WIDTH   (C_M_AXI_ADDR_WIDTH),
        .C_M_AXI_DATA_WIDTH   (C_M_AXI_DATA_WIDTH),
        .C_M_AXI_ID_WIDTH     (C_M_AXI_ID_WIDTH),
        .C_M_AXI_ARUSER_WIDTH (C_M_AXI_ARUSER_WIDTH),
        .C_M_AXI_RUSER_WIDTH  (C_M_AXI_RUSER_WIDTH)
    ) dma_eng (
        .M_AXI_ACLK       (axi_clk),
        .M_AXI_ARESETN    (axi_resetn),
        .frame_base_addr  (frame_base_addr),
        .dma_enable       (dma_enable & ~use_test_pattern),
        .line_swap_in     (line_swap_axi),
        .frame_sync_in    (frame_sync_axi),
        .bram_wr_data     (dma_wr_data),
        .bram_wr_addr     (dma_wr_addr),
        .bram_wr_en       (dma_wr_en),
        .M_AXI_ARID       (M_AXI_ARID),
        .M_AXI_ARADDR     (M_AXI_ARADDR),
        .M_AXI_ARLEN      (M_AXI_ARLEN),
        .M_AXI_ARSIZE     (M_AXI_ARSIZE),
        .M_AXI_ARBURST    (M_AXI_ARBURST),
        .M_AXI_ARLOCK     (M_AXI_ARLOCK),
        .M_AXI_ARCACHE    (M_AXI_ARCACHE),
        .M_AXI_ARPROT     (M_AXI_ARPROT),
        .M_AXI_ARQOS      (M_AXI_ARQOS),
        .M_AXI_ARUSER     (M_AXI_ARUSER),
        .M_AXI_ARVALID    (M_AXI_ARVALID),
        .M_AXI_ARREADY    (M_AXI_ARREADY),
        .M_AXI_RID        (M_AXI_RID),
        .M_AXI_RDATA      (M_AXI_RDATA),
        .M_AXI_RRESP      (M_AXI_RRESP),
        .M_AXI_RLAST      (M_AXI_RLAST),
        .M_AXI_RUSER      (M_AXI_RUSER),
        .M_AXI_RVALID     (M_AXI_RVALID),
        .M_AXI_RREADY     (M_AXI_RREADY),
        .dma_active       (dma_active),
        .dma_error        (dma_error),
        .first_rdata      (first_rdata)
    );

    // ---------------------------------------------------------------
    // Test pattern generator
    // ---------------------------------------------------------------
    test_pattern_gen #(.CD(CD)) tpg (
        .clk        (axi_clk),
        .reset      (reset),
        .enable     (use_test_pattern),
        .colour_sel (tp_colour_sel),
        .buf_sel    (buf_sel_axi),
        .wr_data    (tpg_wr_data),
        .wr_addr    (tpg_wr_addr),
        .wr_en      (tpg_wr_en)
    );

    // ---------------------------------------------------------------
    // VGA sync controller
    // ---------------------------------------------------------------
    vga_sync_720p #(.CD(CD)) vga_unit (
        .clk     (pix_clk),
        .reset   (reset),
        .rd_addr (rd_addr),
        .rd_data (rd_data),
        .buf_sel (buf_sel_pix),
        .hsync   (hsync),
        .vsync   (vsync),
        .rgb     (rgb)
    );

endmodule
