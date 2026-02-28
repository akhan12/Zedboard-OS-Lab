// =================================================================
//  Video pipeline: frame buffer -> bar -> vga sync
//  Bus interface driven by AXI slave via chu_video_controller.
//  Frame buffer si_rgb = bright blue (default/bypass background).
// =================================================================

module video_sys_daisy
#(
   parameter CD               = 12,  // color depth
   parameter VRAM_DATA_WIDTH  = 9    // frame buffer RAM data width
)
(
   input  logic        clk_sys,
   input  logic        clk_25M,
   input  logic        reset_sys,
   // video bus (from AXI slave)
   input  logic        video_cs,
   input  logic        video_wr,
   input  logic [20:0] video_addr,
   input  logic [31:0] video_wr_data,
   // to VGA monitor
   output logic        vsync, hsync,
   output logic [11:0] rgb
);

   // -----------------------------------------------------------------
   // Signal declarations
   // -----------------------------------------------------------------
   // data stream between cores
   logic [CD-1:0] frame_rgb, bar_rgb;
   logic [CD:0]   line_data_in;

   // frame counter
   logic        inc, frame_start;
   logic [10:0] x, y;

   // 2-stage delay (compensates bar_src 2-cycle pipeline)
   logic frame_start_d1_reg, frame_start_d2_reg;
   logic inc_d1_reg, inc_d2_reg;

   // video controller decoded signals
   logic        frame_cs, frame_wr;
   logic [19:0] frame_addr;
   logic [31:0] frame_wr_data;
   logic [7:0]  slot_cs_array;
   logic [7:0]  slot_mem_wr_array;
   logic [13:0] slot_reg_addr_array [7:0];
   logic [31:0] slot_wr_data_array  [7:0];

   // -----------------------------------------------------------------
   // 2-stage delay line
   // -----------------------------------------------------------------
   always_ff @(posedge clk_sys) begin
      frame_start_d1_reg <= frame_start;
      frame_start_d2_reg <= frame_start_d1_reg;
      inc_d1_reg         <= inc;
      inc_d2_reg         <= inc_d1_reg;
   end

   // -----------------------------------------------------------------
   // Frame counter
   // -----------------------------------------------------------------
   frame_counter #(.HMAX(640), .VMAX(480)) frame_counter_unit (
      .clk        (clk_sys),
      .reset      (reset_sys),
      .sync_clr   (1'b0),
      .inc        (inc),
      .hcount     (x),
      .vcount     (y),
      .frame_start(frame_start),
      .frame_end  ()
   );

   // -----------------------------------------------------------------
   // Video bus address decoder
   // -----------------------------------------------------------------
   chu_video_controller ctrl_unit (
      .video_cs            (video_cs),
      .video_wr            (video_wr),
      .video_addr          (video_addr),
      .video_wr_data       (video_wr_data),
      .frame_cs            (frame_cs),
      .frame_wr            (frame_wr),
      .frame_addr          (frame_addr),
      .frame_wr_data       (frame_wr_data),
      .slot_cs_array       (slot_cs_array),
      .slot_mem_wr_array   (slot_mem_wr_array),
      .slot_reg_addr_array (slot_reg_addr_array),
      .slot_wr_data_array  (slot_wr_data_array)
   );

   // -----------------------------------------------------------------
   // Frame buffer core  (si_rgb = bright blue background)
   // bypass_reg resets to 0 -> shows frame buffer by default
   // -----------------------------------------------------------------
   chu_frame_buffer_core #(.CD(CD), .DW(VRAM_DATA_WIDTH)) frame_buf_unit (
      .clk     (clk_sys),
      .reset   (reset_sys),
      .x       (x),
      .y       (y),
      .cs      (frame_cs),
      .write   (frame_wr),
      .addr    (frame_addr),
      .wr_data (frame_wr_data),
      .si_rgb  (12'h00F),   // bright blue background when bypassed
      .so_rgb  (frame_rgb)
   );

   // -----------------------------------------------------------------
   // Bar core  (slot 7 = V7_BAR = index 7)
   // bypass_reg resets to 1 -> passes frame_rgb through by default
   // -----------------------------------------------------------------
   chu_vga_bar_core bar_unit (
      .clk     (clk_sys),
      .reset   (reset_sys),
      .x       (x),
      .y       (y),
      .cs      (slot_cs_array[7]),
      .write   (slot_mem_wr_array[7]),
      .addr    (slot_reg_addr_array[7]),
      .wr_data (slot_wr_data_array[7]),
      .si_rgb  (frame_rgb),
      .so_rgb  (bar_rgb)
   );

   // -----------------------------------------------------------------
   // Pack frame_start into stream LSB
   // -----------------------------------------------------------------
   assign line_data_in = {bar_rgb, frame_start_d2_reg};

   // -----------------------------------------------------------------
   // VGA sync core  (slot 0 = V0_SYNC = index 0)
   // -----------------------------------------------------------------
   chu_vga_sync_core #(.CD(CD)) sync_unit (
      .clk_sys  (clk_sys),
      .clk_25M  (clk_25M),
      .reset    (reset_sys),
      .cs       (slot_cs_array[0]),
      .write    (slot_mem_wr_array[0]),
      .addr     (slot_reg_addr_array[0]),
      .wr_data  (slot_wr_data_array[0]),
      .si_data  (line_data_in),
      .si_valid (inc_d2_reg),
      .si_ready (inc),
      .hsync    (hsync),
      .vsync    (vsync),
      .rgb      (rgb)
   );

endmodule
