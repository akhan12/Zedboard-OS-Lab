module ps2_top
   #(parameter W_SIZE = 6)   // # address bits in FIFO buffer
   (
    input  logic clk, reset,
    input  logic wr_ps2, rd_ps2_packet,
    input  logic [7:0] ps2_tx_data,
    output logic [7:0] ps2_rx_data,
    output logic ps2_tx_idle, ps2_rx_buf_empty,
    // PS/2 split tristate signals (IOBUF sits in top wrapper)
    input  logic ps2c_in,  output logic ps2c_out_o, output logic ps2c_t,
    input  logic ps2d_in,  output logic ps2d_out_o, output logic ps2d_t
   );

   // declaration
   logic rx_idle, tx_idle, rx_done_tick;
   logic [7:0] rx_data;

   // body
   // instantiate ps2 transmitter
   ps2tx ps2_tx_unit (
      .clk(clk), .reset(reset),
      .wr_ps2(wr_ps2), .rx_idle(rx_idle),
      .din(ps2_tx_data), .tx_done_tick(),
      .tx_idle(tx_idle),
      .ps2c_in(ps2c_in), .ps2c_out_o(ps2c_out_o), .ps2c_t(ps2c_t),
      .ps2d_in(ps2d_in), .ps2d_out_o(ps2d_out_o), .ps2d_t(ps2d_t)
   );
   // instantiate ps2 receiver (rx already has plain inputs, no tristate)
   ps2rx ps2_rx_unit (
      .clk(clk), .reset(reset),
      .ps2d(ps2d_in), .ps2c(ps2c_in),
      .rx_en(tx_idle), .rx_idle(rx_idle),
      .rx_done_tick(rx_done_tick), .dout(rx_data)
   );
   // instantiate FIFO buffer
   fifo #(.DATA_WIDTH(8), .ADDR_WIDTH(W_SIZE)) fifo_unit
      (.clk(clk), .reset(reset), .rd(rd_ps2_packet),
       .wr(rx_done_tick), .w_data(rx_data), .empty(ps2_rx_buf_empty),
       .full(), .r_data(ps2_rx_data));
   //output 
   assign ps2_tx_idle = tx_idle;
endmodule

