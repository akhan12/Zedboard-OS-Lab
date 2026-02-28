/********************************************************************
 * @file video_test.cpp
 *
 * @brief test program for video_axi IP
 *
 * Tests:
 *   1. Solid color fill  - fills frame buffer with a single color
 *   2. Color bars        - enables bar core overlay
 *   3. Checkerboard      - draws alternating black/white squares
 *   4. Line drawing      - draws diagonal lines across the screen
 *
 * Sleep uses unistd.h sleep()/usleep() — no FPro timer required.
 ********************************************************************/

#include <stdint.h>
#include <unistd.h>       // sleep(), usleep()
#include "chu_io_map.h"
#include "chu_io_rw.h"
#include "vga_core.h"

// -------------------------------------------------------------------
// 9-bit color helpers (format: RRRGGGGBBB -> bits [8:6]=R [5:3]=G [2:0]=B)
// -------------------------------------------------------------------
#define COLOR_BLACK    0x000
#define COLOR_WHITE    0x1FF
#define COLOR_RED      0x1C0
#define COLOR_GREEN    0x038
#define COLOR_BLUE     0x007
#define COLOR_YELLOW   0x1F8
#define COLOR_CYAN     0x03F
#define COLOR_MAGENTA  0x1C7

// -------------------------------------------------------------------
// Test 1: fill entire frame buffer with one color
// -------------------------------------------------------------------
void test_solid_fill(FrameCore &frame, int color) {
   frame.clr_screen(color);
}

// -------------------------------------------------------------------
// Test 2: color bars via bar core
//   bypass(0) = disable bypass -> show bar pattern
//   bypass(1) = enable bypass  -> pass frame buffer through
// -------------------------------------------------------------------
void test_color_bars(GpvCore &bar) {
   bar.bypass(0);   // show bar pattern
}

// -------------------------------------------------------------------
// Test 3: checkerboard pattern (32x32 squares)
// -------------------------------------------------------------------
void test_checkerboard(FrameCore &frame) {
   for (int y = 0; y < 480; y++) {
      for (int x = 0; x < 640; x++) {
         int tile_x = x / 32;
         int tile_y = y / 32;
         int color = ((tile_x + tile_y) & 1) ? COLOR_WHITE : COLOR_BLACK;
         frame.wr_pix(x, y, color);
      }
   }
}

// -------------------------------------------------------------------
// Test 4: diagonal lines
// -------------------------------------------------------------------
void test_lines(FrameCore &frame) {
   frame.clr_screen(COLOR_BLACK);
   // diagonal from top-left to bottom-right
   frame.plot_line(0, 0, 639, 479, COLOR_RED);
   // diagonal from top-right to bottom-left
   frame.plot_line(639, 0, 0, 479, COLOR_GREEN);
   // horizontal centre line
   frame.plot_line(0, 240, 639, 240, COLOR_BLUE);
   // vertical centre line
   frame.plot_line(320, 0, 320, 479, COLOR_YELLOW);
}

// -------------------------------------------------------------------
// Main
// -------------------------------------------------------------------
int main() {
   // instantiate cores
   FrameCore frame(VIDEO_FRAME_BASE);
   GpvCore   bar(get_video_slot_addr(V7_BAR));

   // ensure bar bypass is on (show frame buffer by default)
   bar.bypass(1);

   // test 1: red fill
   test_solid_fill(frame, COLOR_RED);
   sleep(2);

   // test 2: green fill
   test_solid_fill(frame, COLOR_GREEN);
   sleep(2);

   // test 3: blue fill
   test_solid_fill(frame, COLOR_BLUE);
   sleep(2);

   // test 4: checkerboard
   test_checkerboard(frame);
   sleep(3);

   // test 5: lines
   test_lines(frame);
   sleep(3);

   // test 6: bar core overlay (disables frame buffer display)
   test_color_bars(bar);
   sleep(3);

   // restore frame buffer display
   bar.bypass(1);
   frame.clr_screen(COLOR_BLACK);

   return 0;
}
