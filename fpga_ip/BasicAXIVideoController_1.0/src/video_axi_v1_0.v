`timescale 1 ns / 1 ps

module video_axi_v1_0 #
(
    parameter integer C_S00_AXI_DATA_WIDTH = 32,
    parameter integer C_S00_AXI_ADDR_WIDTH = 23
)
(
    // User ports
    output wire        hsync,
    output wire        vsync,
    output wire [11:0] rgb,

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
    // Internal signals
    // ---------------------------------------------------------------
    wire        clk_100M, clk_25M;
    wire        locked;
    wire        reset_sys;

    // video bus
    wire        video_cs, video_wr;
    wire [20:0] video_addr;
    wire [31:0] video_wr_data;

    // ---------------------------------------------------------------
    // MMCM: 100 MHz in -> 100 MHz + 25 MHz out
    // reset tied to AXI reset (active high for MMCM)
    // ---------------------------------------------------------------
    mmcm_fpro clk_mmcm_unit (
        .clk_in_100M (s00_axi_aclk),
        .clk_100M    (clk_100M),
        .clk_25M     (clk_25M),
        .clk_40M     (),
        .clk_67M     (),
        .reset       (~s00_axi_aresetn),
        .locked      (locked)
    );

    // Hold video system in reset until AXI reset deasserts AND MMCM locked
    assign reset_sys = ~s00_axi_aresetn | ~locked;

    // ---------------------------------------------------------------
    // AXI4-Lite slave / address decoder
    // ---------------------------------------------------------------
    video_axi_v1_0_S00_AXI # (
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
        .video_cs      (video_cs),
        .video_wr      (video_wr),
        .video_addr    (video_addr),
        .video_wr_data (video_wr_data)
    );

    // ---------------------------------------------------------------
    // Video pipeline: frame buffer -> bar -> sync
    // ---------------------------------------------------------------
    video_sys_daisy video_sys_inst (
        .clk_sys      (clk_100M),
        .clk_25M      (clk_25M),
        .reset_sys    (reset_sys),
        .video_cs     (video_cs),
        .video_wr     (video_wr),
        .video_addr   (video_addr),
        .video_wr_data(video_wr_data),
        .hsync        (hsync),
        .vsync        (vsync),
        .rgb          (rgb)
    );

endmodule
