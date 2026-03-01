`timescale 1 ns / 1 ps

// AXI4-Lite slave for PS/2 core
// Address map (byte-addressed, 4-bit):
//   offset 0  (addr[1:0]=00): READ  -> {22'b0, tx_idle, rx_buf_empty, rx_data[7:0]}
//   offset 8  (addr[3:2]=10): WRITE -> send byte to PS/2 TX (ps2_tx_data = wdata[7:0])
//   offset 12 (addr[3:2]=11): WRITE -> pop one byte from RX FIFO (dummy write)
// C driver: io_read(base,0) / io_write(base,2,cmd) / io_write(base,3,0)

module ps2_axi_v1_0_S00_AXI #
(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 4
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

    // PS/2 core interface
    output wire        ps2_wr,        // write byte to TX
    output wire        ps2_rd,        // pop byte from RX FIFO
    output wire [7:0]  ps2_tx_data,   // byte to transmit
    input  wire [7:0]  ps2_rx_data,   // byte from RX FIFO
    input  wire        ps2_tx_idle,   // TX idle flag
    input  wire        ps2_rx_empty   // RX FIFO empty flag
);

    // ---------------------------------------------------------------
    // AXI write channel
    // ---------------------------------------------------------------
    reg  axi_awready_reg;
    reg  axi_wready_reg;
    reg  axi_bvalid_reg;
    reg  [C_S_AXI_ADDR_WIDTH-1:0] axi_awaddr_reg;

    // write address ready + latch
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN) begin
            axi_awready_reg <= 1'b0;
            axi_awaddr_reg  <= 0;
        end else if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID) begin
            axi_awready_reg <= 1'b1;
            axi_awaddr_reg  <= S_AXI_AWADDR;
        end else begin
            axi_awready_reg <= 1'b0;
        end
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

    // ---------------------------------------------------------------
    // AXI read channel
    // ---------------------------------------------------------------
    reg  axi_arready_reg;
    reg  axi_rvalid_reg;
    reg  [C_S_AXI_DATA_WIDTH-1:0] axi_rdata_reg;

    // read address ready
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_arready_reg <= 1'b0;
        else if (~axi_arready_reg && S_AXI_ARVALID)
            axi_arready_reg <= 1'b1;
        else
            axi_arready_reg <= 1'b0;
    end

    // read data: capture PS/2 status+data when address is accepted
    always @(posedge S_AXI_ACLK) begin
        if (~S_AXI_ARESETN)
            axi_rdata_reg <= 0;
        else if (~axi_arready_reg && S_AXI_ARVALID)
            axi_rdata_reg <= {22'b0, ps2_tx_idle, ps2_rx_empty, ps2_rx_data};
    end

    // read data valid
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
    assign S_AXI_BRESP   = 2'b00;
    assign S_AXI_BVALID  = axi_bvalid_reg;
    assign S_AXI_ARREADY = axi_arready_reg;
    assign S_AXI_RDATA   = axi_rdata_reg;
    assign S_AXI_RRESP   = 2'b00;
    assign S_AXI_RVALID  = axi_rvalid_reg;

    // ---------------------------------------------------------------
    // PS/2 decode
    //   wr_en pulses when both AW and W handshakes complete
    //   addr[3:2]: 2'b10 = write TX byte, 2'b11 = pop RX FIFO
    // ---------------------------------------------------------------
    wire wr_en = axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID;

    assign ps2_wr      = wr_en && (axi_awaddr_reg[3:2] == 2'b10);
    assign ps2_rd      = wr_en && (axi_awaddr_reg[3:2] == 2'b11);
    assign ps2_tx_data = S_AXI_WDATA[7:0];

endmodule
