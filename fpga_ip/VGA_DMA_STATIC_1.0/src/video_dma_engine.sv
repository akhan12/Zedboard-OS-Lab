// ============================================================
// video_dma_engine.sv
// AXI4 Master DMA engine: reads 1280 words per line from DDR
// and writes them directly into the ping-pong BRAM.
// One 32-bit word per pixel: RGB444 in [11:0], [31:12] unused.
//
// Burst plan per line: 5 × 256-beat = 1280 beats.
//
// Triggering:
//   DMA is triggered by a 1-cycle pulse (line_swap_in) that is
//   generated in video_sys_720p by CDC'ing buf_sel from pix_clk.
//   Each pulse means VGA just finished a line and swapped buffers,
//   so the DMA should fill the newly freed BRAM with the next line.
//
// first_rdata debug register (slave reg 3):
//   Latches burst_addr at ISSUE_AR. Non-zero → AR channel active.
// ============================================================
`timescale 1ns/1ps

module video_dma_engine
#(
    parameter integer C_M_AXI_ADDR_WIDTH   = 32,
    parameter integer C_M_AXI_DATA_WIDTH   = 32,
    parameter integer C_M_AXI_ID_WIDTH     = 1,
    parameter integer C_M_AXI_ARUSER_WIDTH = 0,
    parameter integer C_M_AXI_RUSER_WIDTH  = 0
)
(
    // ---- AXI clock / reset ------------------------------------------
    input  wire                              M_AXI_ACLK,
    input  wire                              M_AXI_ARESETN,

    // ---- Control ----------------------------------------------------
    input  wire [C_M_AXI_ADDR_WIDTH-1:0]    frame_base_addr,
    input  wire                              dma_enable,
    input  wire                              line_swap_in,  // 1-cycle pulse per line swap (CDC'd)
    input  wire                              frame_sync_in, // 1-cycle pulse per frame (CDC'd)

    // ---- BRAM write port (axi_clk domain) ---------------------------
    output reg  [31:0]                       bram_wr_data,
    output reg  [10:0]                       bram_wr_addr,
    output reg                               bram_wr_en,

    // ---- AXI4 Read Address Channel ----------------------------------
    output wire [C_M_AXI_ID_WIDTH-1:0]      M_AXI_ARID,
    output reg  [C_M_AXI_ADDR_WIDTH-1:0]    M_AXI_ARADDR,
    output reg  [7:0]                        M_AXI_ARLEN,
    output wire [2:0]                        M_AXI_ARSIZE,
    output wire [1:0]                        M_AXI_ARBURST,
    output wire                              M_AXI_ARLOCK,
    output wire [3:0]                        M_AXI_ARCACHE,
    output wire [2:0]                        M_AXI_ARPROT,
    output wire [3:0]                        M_AXI_ARQOS,
    output wire [C_M_AXI_ARUSER_WIDTH-1:0]  M_AXI_ARUSER,
    output reg                               M_AXI_ARVALID,
    input  wire                              M_AXI_ARREADY,

    // ---- AXI4 Read Data Channel -------------------------------------
    input  wire [C_M_AXI_ID_WIDTH-1:0]      M_AXI_RID,
    input  wire [C_M_AXI_DATA_WIDTH-1:0]    M_AXI_RDATA,
    input  wire [1:0]                        M_AXI_RRESP,
    input  wire                              M_AXI_RLAST,
    input  wire [C_M_AXI_RUSER_WIDTH-1:0]   M_AXI_RUSER,
    input  wire                              M_AXI_RVALID,
    output wire                              M_AXI_RREADY,

    // ---- Status -----------------------------------------------------
    output wire                              dma_active,
    output wire                              dma_error,
    output reg  [31:0]                       first_rdata   // first ARADDR issued (debug)
);

    // ---------------------------------------------------------------
    // 720p constants
    // ---------------------------------------------------------------
    localparam WORDS_PER_LINE  = 1280;
    localparam LINE_BYTES      = WORDS_PER_LINE * 4;   // 5120 bytes
    localparam LINES_PER_FRAME = 720;

    // ---------------------------------------------------------------
    // AXI fixed signals
    // ---------------------------------------------------------------
    assign M_AXI_ARID    = {C_M_AXI_ID_WIDTH{1'b0}};
    assign M_AXI_ARSIZE  = 3'b010;    // 4 bytes per beat
    assign M_AXI_ARBURST = 2'b01;     // INCR
    assign M_AXI_ARLOCK  = 1'b0;
    assign M_AXI_ARCACHE = 4'b0011;   // Modifiable, Bufferable
    assign M_AXI_ARPROT  = 3'h0;
    assign M_AXI_ARQOS   = 4'h0;
    assign M_AXI_ARUSER  = {C_M_AXI_ARUSER_WIDTH{1'b0}};

    // Accept data whenever waiting for a DDR word
    assign M_AXI_RREADY  = (state_reg == RD_DATA);

    // ---------------------------------------------------------------
    // State machine
    // ---------------------------------------------------------------
    localparam [2:0]
        IDLE       = 3'd0,
        ISSUE_AR   = 3'd1,
        WAIT_AR    = 3'd2,
        RD_DATA    = 3'd3,
        NEXT_BURST = 3'd4,
        LINE_DONE  = 3'd5;

    reg [2:0] state_reg;

    // ---------------------------------------------------------------
    // Datapath registers
    // ---------------------------------------------------------------
    reg [C_M_AXI_ADDR_WIDTH-1:0] line_base_addr;
    reg [C_M_AXI_ADDR_WIDTH-1:0] burst_addr;
    reg [10:0] words_rem;      // DDR words remaining in current line
    reg [8:0]  beat_count;     // beats received in current burst (0-based)
    reg [7:0]  cur_burst_len;  // ARLEN for current burst
    reg [9:0]  line_count;     // lines fetched this frame (0-based)
    reg [10:0] bram_word_addr; // running BRAM write address (0-1279)
    reg        frame_started;  // 1 once frame_base_addr has been latched

    // ---------------------------------------------------------------
    // Next burst length
    // ---------------------------------------------------------------
    wire [7:0] next_burst_arlen = (words_rem >= 10'd256) ? 8'd255
                                                          : words_rem[7:0] - 8'd1;

    // ---------------------------------------------------------------
    // Fetch trigger
    // line_swap_in: 1-cycle pulse when VGA swaps buffers (= one line
    //   was just displayed, DMA should write the next one).
    // Also allow first fetch immediately on dma_enable without
    //   waiting for the first swap pulse (frame_started).
    // ---------------------------------------------------------------
    wire fetch_ok = dma_enable && frame_started && line_swap_in &&
                    (state_reg == IDLE);

    // ---------------------------------------------------------------
    // Frame / line address management
    // ---------------------------------------------------------------
    always @(posedge M_AXI_ACLK) begin
        if (!M_AXI_ARESETN) begin
            line_base_addr <= {C_M_AXI_ADDR_WIDTH{1'b0}};
            line_count     <= 10'h0;
            frame_started  <= 1'b0;
        end else begin
            if (frame_sync_in && dma_enable) begin
                line_base_addr <= frame_base_addr;
                line_count     <= 10'h0;
                frame_started  <= 1'b1;
            end else if (dma_enable && !frame_started) begin
                // First enable: latch immediately, don't wait for frame_sync
                line_base_addr <= frame_base_addr;
                line_count     <= 10'h0;
                frame_started  <= 1'b1;
            end else if (state_reg == LINE_DONE) begin
                line_base_addr <= line_base_addr + LINE_BYTES;
                line_count     <= (line_count == LINES_PER_FRAME-1) ? 10'h0
                                                                     : line_count + 1'b1;
            end

            if (!dma_enable)
                frame_started <= 1'b0;
        end
    end

    // ---------------------------------------------------------------
    // Main state machine + datapath
    // ---------------------------------------------------------------
    always @(posedge M_AXI_ACLK) begin
        if (!M_AXI_ARESETN) begin
            state_reg     <= IDLE;
            M_AXI_ARADDR  <= {C_M_AXI_ADDR_WIDTH{1'b0}};
            M_AXI_ARLEN   <= 8'h0;
            M_AXI_ARVALID <= 1'b0;
            burst_addr    <= {C_M_AXI_ADDR_WIDTH{1'b0}};
            words_rem     <= 11'h0;
            beat_count    <= 9'h0;
            cur_burst_len <= 8'h0;
            bram_word_addr<= 11'h0;
            bram_wr_data  <= 32'h0;
            bram_wr_addr  <= 11'h0;
            bram_wr_en    <= 1'b0;
            first_rdata   <= 32'h0;
        end else begin
            // Default: deassert one-cycle strobes
            M_AXI_ARVALID <= 1'b0;
            bram_wr_en    <= 1'b0;

            case (state_reg)

                // -----------------------------------------------------
                IDLE: begin
                    bram_word_addr <= 11'h0;
                    if (fetch_ok) begin
                        burst_addr <= line_base_addr;
                        words_rem  <= WORDS_PER_LINE[10:0];
                        state_reg  <= ISSUE_AR;
                    end
                end

                // -----------------------------------------------------
                ISSUE_AR: begin
                    cur_burst_len <= next_burst_arlen;
                    M_AXI_ARADDR  <= burst_addr;
                    M_AXI_ARLEN   <= next_burst_arlen;
                    M_AXI_ARVALID <= 1'b1;
                    beat_count    <= 9'h0;
                    if (first_rdata == 32'h0)
                        first_rdata <= burst_addr;
                    state_reg     <= WAIT_AR;
                end

                // -----------------------------------------------------
                WAIT_AR: begin
                    M_AXI_ARVALID <= 1'b1;
                    if (M_AXI_ARREADY) begin
                        M_AXI_ARVALID <= 1'b0;
                        state_reg     <= RD_DATA;
                    end
                end

                // -----------------------------------------------------
                // RD_DATA: accept each beat from AXI R channel and
                // write it directly to the BRAM (no pixel unpacking
                // needed — vga_sync demuxes the two pixels per word).
                RD_DATA: begin
                    if (M_AXI_RVALID) begin
                        bram_wr_data   <= M_AXI_RDATA;
                        bram_wr_addr   <= bram_word_addr;
                        bram_wr_en     <= 1'b1;
                        bram_word_addr <= bram_word_addr + 1'b1;
                        beat_count     <= beat_count + 1'b1;

                        if (beat_count == cur_burst_len) begin
                            if (words_rem == ({3'b000, cur_burst_len} + 11'd1))
                                state_reg <= LINE_DONE;
                            else begin
                                words_rem <= words_rem - ({3'b000, cur_burst_len} + 11'd1);
                                state_reg <= NEXT_BURST;
                            end
                        end
                    end
                end

                // -----------------------------------------------------
                NEXT_BURST: begin
                    burst_addr <= burst_addr + ({{22{1'b0}}, cur_burst_len, 2'b00} + 32'd4);
                    state_reg  <= ISSUE_AR;
                end

                // -----------------------------------------------------
                LINE_DONE: begin
                    state_reg <= IDLE;
                end

                default: state_reg <= IDLE;

            endcase
        end
    end

    // ---------------------------------------------------------------
    // Status outputs
    // ---------------------------------------------------------------
    assign dma_active = (state_reg != IDLE);
    assign dma_error  = 1'b0;

endmodule
