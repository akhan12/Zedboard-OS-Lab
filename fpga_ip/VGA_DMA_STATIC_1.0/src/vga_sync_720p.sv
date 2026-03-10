// ============================================================
// vga_sync_720p.sv
// VGA/HDMI timing controller for 720p (1280x720 @ 60 Hz).
//
// Standard 720p timing:
//   Pixel clock : 74.25 MHz
//   Horizontal  : HD=1280  HF=110  HB=220  HR=40   HT=1650
//   Vertical    : VD=720   VF=5    VB=20   VR=5    VT=750
//
// Reads pixel data from ping_pong_bram via a registered read port
// (2-cycle latency: BRAM register + mux register in ping_pong_bram).
//
// Pipeline:
//   Cycle N  : rd_addr = x_reg        (issued to BRAM)
//   Cycle N+1: BRAM output register latches word
//   Cycle N+2: ping_pong_bram mux register → rd_data valid
//              y_del2 gates rgb output (video_on uses y_del2, x_del2)
// One word per pixel: rgb = rd_data[CD-1:0], no demux needed.
//
// buf_sel: toggles at end of each active line (x==HD-1, y<VD).
//   buf_sel=0: VGA reads BRAM_B, DMA/TPG writes BRAM_A
//   buf_sel=1: VGA reads BRAM_A, DMA/TPG writes BRAM_B
// ============================================================
`timescale 1ns/1ps

module vga_sync_720p
#(parameter CD = 12)
(
    input  logic            clk,           // 74.25 MHz pixel clock
    input  logic            reset,

    // ---- Ping-pong BRAM read interface --------------------------
    output logic [10:0]     rd_addr,       // pixel address to BRAM (x_reg)
    input  logic [31:0]     rd_data,       // registered output from ping_pong_bram (2-cycle latency)

    // ---- Ping-pong control --------------------------------------
    output logic            buf_sel,       // 0=VGA reads B / 1=VGA reads A

    // ---- HDMI/VGA outputs ---------------------------------------
    output logic            hsync,
    output logic            vsync,
    output logic [CD-1:0]   rgb
);

    // ---------------------------------------------------------------
    // 720p timing constants
    // ---------------------------------------------------------------
    localparam HD = 1280;
    localparam HF = 110;
    localparam HB = 220;
    localparam HR = 40;
    localparam HT = HD + HF + HB + HR;   // 1650

    localparam VD = 720;
    localparam VF = 5;
    localparam VB = 20;
    localparam VR = 5;
    localparam VT = VD + VF + VB + VR;   // 750

    // ---------------------------------------------------------------
    // Free-running scan counter
    // ---------------------------------------------------------------
    logic [10:0] x_reg, y_reg;

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            x_reg <= 11'd0;
            y_reg <= 11'd0;
        end else begin
            if (x_reg == HT-1) begin
                x_reg <= 11'd0;
                y_reg <= (y_reg == VT-1) ? 11'd0 : y_reg + 1'b1;
            end else
                x_reg <= x_reg + 1'b1;
        end
    end

    // ---------------------------------------------------------------
    // BRAM read address: x_reg issued now, data valid 2 cycles later.
    // ---------------------------------------------------------------
    assign rd_addr = x_reg;

    // ---------------------------------------------------------------
    // 2-cycle delayed x and y for video_on gating
    // ---------------------------------------------------------------
    logic [10:0] x_del1, x_del2;
    logic [10:0] y_del1, y_del2;

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            x_del1 <= 11'd0; x_del2 <= 11'd0;
            y_del1 <= 11'd0; y_del2 <= 11'd0;
        end else begin
            x_del1 <= x_reg;  x_del2 <= x_del1;
            y_del1 <= y_reg;  y_del2 <= y_del1;
        end
    end

    // One word per pixel — take low CD bits directly, no demux.
    logic [CD-1:0] pixel_colour;
    assign pixel_colour = rd_data[CD-1:0];

    logic video_on_del2;
    assign video_on_del2 = (x_del2 < HD) && (y_del2 < VD);

    // ---------------------------------------------------------------
    // Sync signals (generated from x_reg/y_reg, then delayed 2 cycles
    // to stay aligned with the pixel pipeline)
    // ---------------------------------------------------------------
    logic hsync_i, vsync_i;
    assign hsync_i = ~((x_reg >= (HD+HF)) && (x_reg < (HD+HF+HR)));
    assign vsync_i = ~((y_reg >= (VD+VF)) && (y_reg < (VD+VF+VR)));

    logic hsync_d1, vsync_d1;
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            hsync_d1 <= 1'b1; hsync <= 1'b1;
            vsync_d1 <= 1'b1; vsync <= 1'b1;
            rgb      <= {CD{1'b0}};
        end else begin
            hsync_d1 <= hsync_i;
            hsync    <= hsync_d1;
            vsync_d1 <= vsync_i;
            vsync    <= vsync_d1;
            rgb      <= video_on_del2 ? pixel_colour : {CD{1'b0}};
        end
    end

    // ---------------------------------------------------------------
    // buf_sel: toggle at end of each active line
    // Swap happens in blanking so DMA has full line period to fill
    // the newly freed BRAM before VGA reads it on the next line.
    // ---------------------------------------------------------------
    always_ff @(posedge clk or posedge reset) begin
        if (reset)
            buf_sel <= 1'b0;
        else if (x_reg == HD-1 && y_reg < VD)
            buf_sel <= ~buf_sel;
    end

endmodule
