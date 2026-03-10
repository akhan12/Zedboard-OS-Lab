// ============================================================
// test_pattern_gen.sv
//
// Solid-colour test pattern generator — BRAM write port version.
//
// Runs in axi_clk domain. When enabled, triggered by each edge
// of buf_sel_axi (one swap per VGA line), walks wr_addr 0-1279
// writing one pixel per word into the ping-pong BRAMs.
//
// ping_pong_bram.sv asserts wr_en_a = wr_en_b = tpg_wr_en in
// TPG mode, so both BRAMs receive identical data and the VGA
// always reads a valid pattern regardless of buf_sel state.
//
// colour_sel encoding:
//   00 = RED   (0xF00)
//   01 = GREEN (0x0F0)
//   10 = BLUE  (0x00F)
//   11 = RED   (0xF00)
//
// One pixel per 32-bit word: RGB444 in [11:0], [31:12] = 0.
// ============================================================
`timescale 1ns/1ps

module test_pattern_gen
#(parameter CD = 12)
(
    input  logic        clk,           // axi_clk
    input  logic        reset,
    input  logic        enable,
    input  logic [1:0]  colour_sel,
    input  logic        buf_sel,       // synchronised buf_sel_axi

    output logic [31:0] wr_data,       // pixel word: RGB444 in [11:0]
    output logic [10:0] wr_addr,       // 0-1279
    output logic        wr_en
);

    localparam WORDS_PER_LINE = 1280;

    // ---------------------------------------------------------------
    // Colour lookup
    // ---------------------------------------------------------------
    logic [11:0] colour;
    always_comb
        case (colour_sel)
            2'b00:   colour = 12'hF00;
            2'b01:   colour = 12'h0F0;
            2'b10:   colour = 12'h00F;
            default: colour = 12'hF00;
        endcase

    // One pixel per word: RGB444 in [11:0], upper bits zero
    assign wr_data = {20'h0, colour};

    // ---------------------------------------------------------------
    // Edge detect on buf_sel → trigger one line write per swap
    // ---------------------------------------------------------------
    logic buf_sel_prev;
    always_ff @(posedge clk or posedge reset)
        if (reset) buf_sel_prev <= 1'b0;
        else       buf_sel_prev <= buf_sel;

    wire buf_sel_edge = buf_sel ^ buf_sel_prev;

    // ---------------------------------------------------------------
    // Write address counter
    // ---------------------------------------------------------------
    logic        running;
    logic [10:0] addr_cnt;

    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            running  <= 1'b0;
            addr_cnt <= 11'h0;
            wr_en    <= 1'b0;
            wr_addr  <= 11'h0;
        end else begin
            wr_en <= 1'b0;
            if (enable && buf_sel_edge && !running) begin
                running  <= 1'b1;
                addr_cnt <= 11'h0;
            end else if (running) begin
                wr_en    <= 1'b1;
                wr_addr  <= addr_cnt;
                addr_cnt <= addr_cnt + 1'b1;
                if (addr_cnt == WORDS_PER_LINE - 1)
                    running <= 1'b0;
            end
        end
    end

endmodule
