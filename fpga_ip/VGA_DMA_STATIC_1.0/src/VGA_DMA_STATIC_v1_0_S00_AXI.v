`timescale 1 ns / 1 ps

// ===========================================================================
// VGA_DMA_STATIC_v1_0_S00_AXI.v
//
// AXI4-Lite slave — configuration registers for the VGA DMA IP.
//
// Register map (byte address = reg_index * 4):
//
//   Reg 0  (0x00)  frame_base_addr [31:0]   R/W
//            DDR byte address of the start of the frame buffer.
//            Must be 4-byte aligned. Latched by DMA at each frame boundary.
//
//   Reg 1  (0x04)  control [31:0]            R/W
//            [0]   dma_enable        1 = enable DMA reads from DDR
//            [1]   use_test_pattern  1 = show test pattern (bypasses DMA)
//            [3:2] tp_colour_sel     00=RED  01=GREEN  10=BLUE  11=RED
//            [31:4] reserved
//
//   Reg 2  (0x08)  status [31:0]             R only (writes silently ignored)
//            [0]   dma_active        1 = DMA is currently fetching
//            [1]   dma_error         1 = AXI read error detected
//            [31:2] reserved
//
//   Reg 3–7        reserved for future use
// ===========================================================================

module VGA_DMA_STATIC_v1_0_S00_AXI #
(
	// Users to add parameters here

	// User parameters ends
	// Do not modify the parameters beyond this line

	// Width of S_AXI data bus
	parameter integer C_S_AXI_DATA_WIDTH	= 32,
	// Width of S_AXI address bus
	parameter integer C_S_AXI_ADDR_WIDTH	= 5
)
(
	// Users to add ports here
	output wire [31:0]  frame_base_addr,
	output wire         dma_enable,
	output wire         use_test_pattern,
	output wire [1:0]   tp_colour_sel,
	input  wire         dma_active,
	input  wire         dma_error,
	input  wire [31:0]  first_rdata,
	// User ports ends
	// Do not modify the ports beyond this line

	// Global Clock Signal
	input wire  S_AXI_ACLK,
	// Global Reset Signal. This Signal is Active LOW
	input wire  S_AXI_ARESETN,
	// Write address (issued by master, accepted by Slave)
	input wire [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_AWADDR,
	// Write channel Protection type
	input wire [2 : 0] S_AXI_AWPROT,
	// Write address valid
	input wire  S_AXI_AWVALID,
	// Write address ready
	output wire  S_AXI_AWREADY,
	// Write data
	input wire [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_WDATA,
	// Write strobes
	input wire [(C_S_AXI_DATA_WIDTH/8)-1 : 0] S_AXI_WSTRB,
	// Write valid
	input wire  S_AXI_WVALID,
	// Write ready
	output wire  S_AXI_WREADY,
	// Write response
	output wire [1 : 0] S_AXI_BRESP,
	// Write response valid
	output wire  S_AXI_BVALID,
	// Response ready
	input wire  S_AXI_BREADY,
	// Read address
	input wire [C_S_AXI_ADDR_WIDTH-1 : 0] S_AXI_ARADDR,
	// Protection type
	input wire [2 : 0] S_AXI_ARPROT,
	// Read address valid
	input wire  S_AXI_ARVALID,
	// Read address ready
	output wire  S_AXI_ARREADY,
	// Read data
	output wire [C_S_AXI_DATA_WIDTH-1 : 0] S_AXI_RDATA,
	// Read response
	output wire [1 : 0] S_AXI_RRESP,
	// Read valid
	output wire  S_AXI_RVALID,
	// Read ready
	input wire  S_AXI_RREADY
);

	// AXI4-Lite internal signals
	reg [C_S_AXI_ADDR_WIDTH-1 : 0] axi_awaddr;
	reg  axi_awready;
	reg  axi_wready;
	reg [1 : 0] axi_bresp;
	reg  axi_bvalid;
	reg [C_S_AXI_ADDR_WIDTH-1 : 0] axi_araddr;
	reg  axi_arready;
	reg [C_S_AXI_DATA_WIDTH-1 : 0] axi_rdata;
	reg [1 : 0] axi_rresp;
	reg  axi_rvalid;

	// ADDR_LSB = 2 for 32-bit, OPT_MEM_ADDR_BITS = 2 → 8 regs (3-bit index)
	localparam integer ADDR_LSB          = (C_S_AXI_DATA_WIDTH/32) + 1;
	localparam integer OPT_MEM_ADDR_BITS = 2;

	// Registers
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg0;  // frame_base_addr
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg1;  // control
	// reg2 = read-only status (no storage)
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg3;
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg4;
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg5;
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg6;
	reg [C_S_AXI_DATA_WIDTH-1:0] slv_reg7;

	wire slv_reg_rden;
	wire slv_reg_wren;
	reg [C_S_AXI_DATA_WIDTH-1:0] reg_data_out;
	integer byte_index;
	reg aw_en;

	// I/O assignments
	assign S_AXI_AWREADY = axi_awready;
	assign S_AXI_WREADY  = axi_wready;
	assign S_AXI_BRESP   = axi_bresp;
	assign S_AXI_BVALID  = axi_bvalid;
	assign S_AXI_ARREADY = axi_arready;
	assign S_AXI_RDATA   = axi_rdata;
	assign S_AXI_RRESP   = axi_rresp;
	assign S_AXI_RVALID  = axi_rvalid;

	// User register outputs
	assign frame_base_addr  = slv_reg0;
	assign dma_enable       = slv_reg1[0];
	assign use_test_pattern = slv_reg1[1];
	assign tp_colour_sel    = slv_reg1[3:2];

	// ---------------------------------------------------------------
	// Write address ready
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			axi_awready <= 1'b0;
			aw_en <= 1'b1;
		end else begin
			if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
				axi_awready <= 1'b1;
				aw_en <= 1'b0;
			end else if (S_AXI_BREADY && axi_bvalid) begin
				aw_en <= 1'b1;
				axi_awready <= 1'b0;
			end else
				axi_awready <= 1'b0;
		end
	end

	// ---------------------------------------------------------------
	// Write address latch
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0)
			axi_awaddr <= 0;
		else if (~axi_awready && S_AXI_AWVALID && S_AXI_WVALID && aw_en)
			axi_awaddr <= S_AXI_AWADDR;
	end

	// ---------------------------------------------------------------
	// Write data ready
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0)
			axi_wready <= 1'b0;
		else begin
			if (~axi_wready && S_AXI_WVALID && S_AXI_AWVALID && aw_en)
				axi_wready <= 1'b1;
			else
				axi_wready <= 1'b0;
		end
	end

	// ---------------------------------------------------------------
	// Register write
	// ---------------------------------------------------------------
	assign slv_reg_wren = axi_wready && S_AXI_WVALID && axi_awready && S_AXI_AWVALID;

	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			slv_reg0 <= 32'h0000_0000;
			slv_reg1 <= 32'h0000_0000;
			slv_reg3 <= 0;
			slv_reg4 <= 0;
			slv_reg5 <= 0;
			slv_reg6 <= 0;
			slv_reg7 <= 0;
		end else if (slv_reg_wren) begin
			case (axi_awaddr[ADDR_LSB+OPT_MEM_ADDR_BITS : ADDR_LSB])
				3'h0:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg0[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				3'h1:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg1[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				// 3'h2: read-only status — writes silently ignored
				3'h3:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg3[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				3'h4:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg4[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				3'h5:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg5[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				3'h6:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg6[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				3'h7:
					for (byte_index=0; byte_index<=(C_S_AXI_DATA_WIDTH/8-1); byte_index=byte_index+1)
						if (S_AXI_WSTRB[byte_index])
							slv_reg7[(byte_index*8) +: 8] <= S_AXI_WDATA[(byte_index*8) +: 8];
				default: ;
			endcase
		end
	end

	// ---------------------------------------------------------------
	// Write response
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			axi_bvalid <= 0;
			axi_bresp  <= 2'b0;
		end else begin
			if (axi_awready && S_AXI_AWVALID && ~axi_bvalid && axi_wready && S_AXI_WVALID) begin
				axi_bvalid <= 1'b1;
				axi_bresp  <= 2'b0;
			end else if (S_AXI_BREADY && axi_bvalid)
				axi_bvalid <= 1'b0;
		end
	end

	// ---------------------------------------------------------------
	// Read address ready
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			axi_arready <= 1'b0;
			axi_araddr  <= 32'b0;
		end else begin
			if (~axi_arready && S_AXI_ARVALID) begin
				axi_arready <= 1'b1;
				axi_araddr  <= S_AXI_ARADDR;
			end else
				axi_arready <= 1'b0;
		end
	end

	// ---------------------------------------------------------------
	// Read valid
	// ---------------------------------------------------------------
	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0) begin
			axi_rvalid <= 0;
			axi_rresp  <= 0;
		end else begin
			if (axi_arready && S_AXI_ARVALID && ~axi_rvalid) begin
				axi_rvalid <= 1'b1;
				axi_rresp  <= 2'b0;
			end else if (axi_rvalid && S_AXI_RREADY)
				axi_rvalid <= 1'b0;
		end
	end

	// ---------------------------------------------------------------
	// Register read mux (includes live status in reg2)
	// ---------------------------------------------------------------
	assign slv_reg_rden = axi_arready & S_AXI_ARVALID & ~axi_rvalid;

	always @(*) begin
		case (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS : ADDR_LSB])
			3'h0:    reg_data_out = slv_reg0;
			3'h1:    reg_data_out = slv_reg1;
			3'h2:    reg_data_out = {30'h0, dma_error, dma_active};
			3'h3:    reg_data_out = first_rdata;   // first DDR word read by DMA (debug)
			3'h4:    reg_data_out = slv_reg4;
			3'h5:    reg_data_out = slv_reg5;
			3'h6:    reg_data_out = slv_reg6;
			3'h7:    reg_data_out = slv_reg7;
			default: reg_data_out = 32'h0;
		endcase
	end

	always @(posedge S_AXI_ACLK) begin
		if (S_AXI_ARESETN == 1'b0)
			axi_rdata <= 0;
		else if (slv_reg_rden)
			axi_rdata <= reg_data_out;
	end

endmodule
