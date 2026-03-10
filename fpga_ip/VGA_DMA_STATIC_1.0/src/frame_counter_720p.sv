// ============================================================
// frame_counter_720p.sv
// Pixel x/y counter for 720p active area (1280 x 720).
// Identical interface to frame_counter.sv but fixed constants.
// inc is driven by si_ready feedback from the line buffer so
// the source never overruns the FIFO.
// ============================================================
module frame_counter_720p
(
    input  logic        clk,
    input  logic        reset,
    input  logic        inc,        // advance one pixel
    input  logic        sync_clr,   // synchronous clear to (0,0)
    output logic [10:0] hcount,     // 0 .. 1279
    output logic [10:0] vcount,     // 0 .. 719
    output logic        frame_start, // combinatorial: (x==0 && y==0)
    output logic        frame_end,   // combinatorial: (x==1279 && y==719)
    output logic        line_start,  // combinatorial: x==0
    output logic        line_end     // combinatorial: x==1279
);

    localparam HMAX = 1280;
    localparam VMAX = 720;

    logic [10:0] hc_reg, hc_next;
    logic [10:0] vc_reg, vc_next;

    always_ff @(posedge clk, posedge reset)
        if (reset) begin
            hc_reg <= 0;
            vc_reg <= 0;
        end else if (sync_clr) begin
            hc_reg <= 0;
            vc_reg <= 0;
        end else begin
            hc_reg <= hc_next;
            vc_reg <= vc_next;
        end

    // horizontal next-state
    always_comb
        if (inc)
            hc_next = (hc_reg == HMAX-1) ? 11'd0 : hc_reg + 1'b1;
        else
            hc_next = hc_reg;

    // vertical next-state
    always_comb
        if (inc && (hc_reg == HMAX-1))
            vc_next = (vc_reg == VMAX-1) ? 11'd0 : vc_reg + 1'b1;
        else
            vc_next = vc_reg;

    assign hcount      = hc_reg;
    assign vcount      = vc_reg;
    assign frame_start = (hc_reg == 0)        && (vc_reg == 0);
    assign frame_end   = (hc_reg == HMAX-1)   && (vc_reg == VMAX-1);
    assign line_start  = (hc_reg == 0);
    assign line_end    = (hc_reg == HMAX-1);

endmodule
