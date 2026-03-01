`timescale 1 ns / 1 ps

module ps2_axi_v1_0 #
(
    parameter integer C_S00_AXI_DATA_WIDTH = 32,
    parameter integer C_S00_AXI_ADDR_WIDTH = 4,   // 16-byte range (4 registers)
    parameter integer FIFO_ADDR_WIDTH      = 6    // 64-entry RX FIFO
)
(
    // PS/2 physical pins
    inout  wire        ps2d,
    inout  wire        ps2c,

    // AXI4-Lite slave port
    input  wire                              s00_axi_aclk,
    input  wire                              s00_axi_aresetn,
    input  wire [C_S00_AXI_ADDR_WIDTH-1:0]  s00_axi_awaddr,
    input  wire [2:0]                        s00_axi_awprot,
    input  wire                              s00_axi_awvalid,
    output wire                              s00_axi_awready,
    input  wire [C_S00_AXI_DATA_WIDTH-1:0]  s00_axi_wdata,
    input  wire [(C_S00_AXI_DATA_WIDTH/8)-1:0] s00_axi_wstrb,
    input  wire                              s00_axi_wvalid,
    output wire                              s00_axi_wready,
    output wire [1:0]                        s00_axi_bresp,
    output wire                              s00_axi_bvalid,
    input  wire                              s00_axi_bready,
    input  wire [C_S00_AXI_ADDR_WIDTH-1:0]  s00_axi_araddr,
    input  wire [2:0]                        s00_axi_arprot,
    input  wire                              s00_axi_arvalid,
    output wire                              s00_axi_arready,
    output wire [C_S00_AXI_DATA_WIDTH-1:0]  s00_axi_rdata,
    output wire [1:0]                        s00_axi_rresp,
    output wire                              s00_axi_rvalid,
    input  wire                              s00_axi_rready
);

    // ---------------------------------------------------------------
    // Internal signals between AXI slave and ps2_top
    // ---------------------------------------------------------------
    wire        ps2_wr;
    wire        ps2_rd;
    wire [7:0]  ps2_tx_data;
    wire [7:0]  ps2_rx_data;
    wire        ps2_tx_idle;
    wire        ps2_rx_empty;

    wire reset_sys = ~s00_axi_aresetn;

    // ---------------------------------------------------------------
    // Explicit IOBUF primitives for ps2c and ps2d
    // Prevents Vivado from synthesizing inout as driven output in
    // block design hierarchy. T=1 -> high-impedance (input mode).
    // ---------------------------------------------------------------
    wire ps2c_in, ps2c_out_o, ps2c_t;
    wire ps2d_in, ps2d_out_o, ps2d_t;

    IOBUF iobuf_ps2c (
        .IO (ps2c),
        .I  (ps2c_out_o),
        .O  (ps2c_in),
        .T  (ps2c_t)
    );
    IOBUF iobuf_ps2d (
        .IO (ps2d),
        .I  (ps2d_out_o),
        .O  (ps2d_in),
        .T  (ps2d_t)
    );

    // ---------------------------------------------------------------
    // AXI4-Lite slave
    // ---------------------------------------------------------------
    ps2_axi_v1_0_S00_AXI # (
        .C_S_AXI_DATA_WIDTH (C_S00_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH (C_S00_AXI_ADDR_WIDTH)
    ) axi_slave_inst (
        .S_AXI_ACLK    (s00_axi_aclk),
        .S_AXI_ARESETN (s00_axi_aresetn),
        .S_AXI_AWADDR  (s00_axi_awaddr),
        .S_AXI_AWPROT  (s00_axi_awprot),
        .S_AXI_AWVALID (s00_axi_awvalid),
        .S_AXI_AWREADY (s00_axi_awready),
        .S_AXI_WDATA   (s00_axi_wdata),
        .S_AXI_WSTRB   (s00_axi_wstrb),
        .S_AXI_WVALID  (s00_axi_wvalid),
        .S_AXI_WREADY  (s00_axi_wready),
        .S_AXI_BRESP   (s00_axi_bresp),
        .S_AXI_BVALID  (s00_axi_bvalid),
        .S_AXI_BREADY  (s00_axi_bready),
        .S_AXI_ARADDR  (s00_axi_araddr),
        .S_AXI_ARPROT  (s00_axi_arprot),
        .S_AXI_ARVALID (s00_axi_arvalid),
        .S_AXI_ARREADY (s00_axi_arready),
        .S_AXI_RDATA   (s00_axi_rdata),
        .S_AXI_RRESP   (s00_axi_rresp),
        .S_AXI_RVALID  (s00_axi_rvalid),
        .S_AXI_RREADY  (s00_axi_rready),
        .ps2_wr        (ps2_wr),
        .ps2_rd        (ps2_rd),
        .ps2_tx_data   (ps2_tx_data),
        .ps2_rx_data   (ps2_rx_data),
        .ps2_tx_idle   (ps2_tx_idle),
        .ps2_rx_empty  (ps2_rx_empty)
    );

    // ---------------------------------------------------------------
    // PS/2 controller (TX + RX + FIFO)
    // ---------------------------------------------------------------
    ps2_top # (
        .W_SIZE (FIFO_ADDR_WIDTH)
    ) ps2_inst (
        .clk             (s00_axi_aclk),
        .reset           (reset_sys),
        .wr_ps2          (ps2_wr),
        .rd_ps2_packet   (ps2_rd),
        .ps2_tx_data     (ps2_tx_data),
        .ps2_rx_data     (ps2_rx_data),
        .ps2_tx_idle     (ps2_tx_idle),
        .ps2_rx_buf_empty(ps2_rx_empty),
        .ps2c_in         (ps2c_in),
        .ps2c_out_o      (ps2c_out_o),
        .ps2c_t          (ps2c_t),
        .ps2d_in         (ps2d_in),
        .ps2d_out_o      (ps2d_out_o),
        .ps2d_t          (ps2d_t)
    );

endmodule
