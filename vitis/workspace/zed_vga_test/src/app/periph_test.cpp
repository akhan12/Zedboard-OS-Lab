/********************************************************************
 * @file periph_test.cpp
 *
 * @brief Peripheral test program for AXI IPs
 *
 * Tests are run sequentially. Comment out sections not yet wired up.
 *
 * Peripherals tested:
 *   - video_axi_v1_0 : frame buffer fills, checkerboard, lines, bar
 *   - ps2_axi_v1_0   : device init, keyboard echo, mouse tracking
 *
 * Sleep uses unistd.h sleep()/usleep() — no FPro timer required.
 ********************************************************************/

#include <stdint.h>
#include <unistd.h>       // sleep(), usleep()
#include "chu_io_map.h"
#include "chu_io_rw.h"
#include "vga_core.h"
#include "ps2_core.h"

// -------------------------------------------------------------------
// 9-bit color helpers  RRRGGGGBBB -> [8:6]=R [5:2]=G [1:0]=B
// -------------------------------------------------------------------
#define COLOR_BLACK    0x000
#define COLOR_WHITE    0x1FF
#define COLOR_RED      0x1C0
#define COLOR_GREEN    0x038
#define COLOR_BLUE     0x007
#define COLOR_YELLOW   0x1F8
#define COLOR_CYAN     0x03F
#define COLOR_MAGENTA  0x1C7

// ===================================================================
// VIDEO TESTS
// ===================================================================

void test_solid_fill(FrameCore &frame, int color) {
   frame.clr_screen(color);
}

void test_color_bars(GpvCore &bar) {
   bar.bypass(0);   // show bar pattern
}

void test_checkerboard(FrameCore &frame) {
   for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 640; x++) {
         int color = (((x / 32) + (y / 32)) & 1) ? COLOR_WHITE : COLOR_BLACK;
         frame.wr_pix(x, y, color);
      }
   }
}

void test_lines(FrameCore &frame) {
   frame.clr_screen(COLOR_BLACK);
   frame.plot_line(0,   0,   639, 479, COLOR_RED);
   frame.plot_line(639, 0,   0,   479, COLOR_GREEN);
   frame.plot_line(0,   240, 639, 240, COLOR_BLUE);
   frame.plot_line(320, 0,   320, 479, COLOR_YELLOW);
}

// ===================================================================
// PS/2 TESTS
// ===================================================================

// Wait up to timeout_ms for a byte; return -1 on timeout
static int ps2_wait_byte(Ps2Core &ps2, int timeout_ms) {
   int elapsed = 0;
   while (ps2.rx_fifo_empty()) {
      usleep(1000);
      if (++elapsed >= timeout_ms)
         return -1;
   }
   return ps2.rx_byte();
}

// Test 1: identify device type
void test_ps2_init(Ps2Core &ps2) {
   int result = ps2.init();
   // result: 1=keyboard, 2=mouse, negative=error
   // Check result in debugger or via LED/UART if available
   (void)result;
}

// Test 2: keyboard echo — read up to 20 characters
// Characters are translated to ASCII via get_kb_ch()
void test_ps2_keyboard(Ps2Core &ps2) {
   char ch;
   int  count = 0;
   while (count < 20) {
      if (ps2.get_kb_ch(&ch)) {
         // ch holds the ASCII code; use however your output allows
         // e.g. write to a frame buffer OSD, toggle an LED, etc.
         count++;
      }
   }
}


// ===================================================================
// Main
// ===================================================================
int main() {

   // --- Video cores ---
   FrameCore frame(VIDEO_FRAME_BASE);
   GpvCore   bar(get_video_slot_addr(V7_BAR));

   // --- PS/2 core ---
   Ps2Core   ps2(PS2_AXI_BASE);

   // ----------------------------------------------------------------
   // VIDEO: initialize frame buffer before display is visible
   // ----------------------------------------------------------------
//   bar.bypass(1);                     // pass frame buffer through (bar hidden)
//   frame.clr_screen(COLOR_BLACK);     // init RAM to avoid garbage on first display
//
//   // solid color fills
//   test_solid_fill(frame, COLOR_RED);    sleep(2);
//   test_solid_fill(frame, COLOR_GREEN);  sleep(2);
//   test_solid_fill(frame, COLOR_BLUE);   sleep(2);
//
//   // checkerboard
//   test_checkerboard(frame);             sleep(3);
//
//   // lines
//   test_lines(frame);                    sleep(3);
//
//   // bar overlay
//   test_color_bars(bar);                 sleep(3);   // bar.bypass(0)

   // restore frame buffer display
   frame.clr_screen(COLOR_BLUE);
   bar.bypass(1);
   sleep(1);

   // ----------------------------------------------------------------
   // PS/2: identify device, then run keyboard or mouse test
   // ----------------------------------------------------------------
   int device = ps2.init();

   if (device == 1) {
      // keyboard: read 20 key presses, inspect in debugger
      test_ps2_keyboard(ps2);
   }
   // device == 2: mouse (not tested yet)
   // device < 0: no device or init error — skip PS/2 tests

   return 0;
}
