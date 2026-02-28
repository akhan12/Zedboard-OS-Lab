`timescale 1 ns / 1 ps

// AXI4-Lite slave for video system
// Address map (byte-addressed, 23-bit):
//   bit[22]     = 1 -> frame buffer  (bits[22:2] = 21-bit word addr, [21] is frame select)
//   bit[22]     = 0 -> video slot registers
//     bits[20:18] = slot select (3-bit)
//     bits[17:2]  = 14-bit register address within slot
// C driver: word address n -> byte address = base + 4*n

module video_axi_v1_0_S00_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 23
)
(
    // AXI4-Lite slave signals
    input  wire                              S_AXI_ACLK,
    input  wire                              S_AXI_ARESETN,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0]    S_AXI_AWADDR,
    input  wire [2:0]                        S_AXI_AWPROT,
    input  wire                              S_AXI_AWVALID,
    output wire                              S_AXI_AWREADY,
    input  wire [C_S_AXI_DATA_WIDTH-1:0]    S_AXI_WDATA,
    input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] S_AXI_WSTRB,
    input  wire                              S_AXI_WVALID,
    output wire                              S_AXI_WREADY,
    output wire [1:0]                        S_AXI_BRESP,
    output wire                              S_AXI_BVALID,
    input  wire                              S_AXI_BREADY,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0]    S_AXI_ARADDR,
    input  wire [2:0]                        S_AXI_ARPROT,
    input  wire                              S_AXI_ARVALID,
    output wire                              S_AXI_ARREADY,
    output wire [C_S_AXI_DATA_WIDTH-1:0]    S_AXI_RDATA,
    output wire [1:0]                        S_AXI_RRESP,
    output wire                              S_AXI_RVALID,
    input  wire                              S_AXI_RREADY,

    // Video bus outputs (to video_sys_daisy)
    output wire                              video_cs,
    output wire                              video_wr,
    output wire [20:0]                       video_addr,
    output wire [31:0]                       video_wr_data
);

    // ---------------------------------------------------------------
    // AXI write channel handshake registers
    // ---------------------------------------------------------------
    reg  axi_awready_reg;
    reg  axi_wready_reg;
    reg  axi_bvalid_reg;
    reg  [C_S_AXI_ADDR_WIDTH-1:0] axi_awaddr_reg;

    // ---------------------------------------------------------------
    // AXI read channel handshake registers (write-only peripheral;
    // reads always return 0)
    // ---------------------------------------------------------------
    reg  axi_arready_reg;
    reg  axi_rvalid_reg;

    // write address ready
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_awready_reg <= 1'b0;
        else if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID)
            axi_awready_reg <= 1'b1;
        else
            axi_awready_reg <= 1'b0;
    end

    // latch write address
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_awaddr_reg <= 0;
        else if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID)
            axi_awaddr_reg <= S_AXI_AWADDR;
    end

    // write data ready
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_wready_reg <= 1'b0;
        else if (~axi_wready_reg && S_AXI_WVALID && S_AXI_AWVALID)
            axi_wready_reg <= 1'b1;
        else
            axi_wready_reg <= 1'b0;
    end

    // write response
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_bvalid_reg <= 1'b0;
        else if (axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID && ~axi_bvalid_reg)
            axi_bvalid_reg <= 1'b1;
        else if (S_AXI_BREADY && axi_bvalid_reg)
            axi_bvalid_reg <= 1'b0;
    end

    // read address ready
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_arready_reg <= 1'b0;
        else if (~axi_arready_reg && S_AXI_ARVALID)
            axi_arready_reg <= 1'b1;
        else
            axi_arready_reg <= 1'b0;
    end

    // read data valid (returns 0; video system is write-only)
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_rvalid_reg <= 1'b0;
        else if (axi_arready_reg && S_AXI_ARVALID && ~axi_rvalid_reg)
            axi_rvalid_reg <= 1'b1;
        else if (axi_rvalid_reg && S_AXI_RREADY)
            axi_rvalid_reg <= 1'b0;
    end

    // ---------------------------------------------------------------
    // AXI output assignments
    // ---------------------------------------------------------------
    assign S_AXI_AWREADY = axi_awready_reg;
    assign S_AXI_WREADY  = axi_wready_reg;
    assign S_AXI_BRESP   = 2'b00;  // OKAY
    assign S_AXI_BVALID  = axi_bvalid_reg;
    assign S_AXI_ARREADY = axi_arready_reg;
    assign S_AXI_RDATA   = 32'b0;  // write-only peripheral
    assign S_AXI_RRESP   = 2'b00;
    assign S_AXI_RVALID  = axi_rvalid_reg;

    // ---------------------------------------------------------------
    // Video bus decode — drop byte bits [1:0], pass [22:2] as video_addr
    //   video_addr[20]   = axi_awaddr[22]          frame buffer select
    //   video_addr[16:14]= axi_awaddr[18:16]        slot select (stride 0x10000 bytes)
    //   video_addr[13:0] = axi_awaddr[15:2]         register address within slot
    // C: io_write(base, offset, data) -> byte addr = base + 4*offset -> video_addr = offset
    // ---------------------------------------------------------------
    wire wr_en = axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID;

    assign video_cs      = wr_en;
    assign video_wr      = wr_en;
    assign video_addr    = axi_awaddr_reg[22:2]; // drop byte bits [1:0] -> 21-bit word addr
    assign video_wr_data = S_AXI_WDATA;

endmodule
