/*****************************************************************//**
 * @file dma_test.cpp
 *
 * @brief Test application for VGA_DMA_STATIC_v1_0 — 720p DMA video IP.
 *
 * Architecture: ping-pong BRAM line buffer.
 *   - DMA fills one BRAM while VGA reads the other.
 *   - Buffer swap happens at end of each active line (~17 µs).
 *   - No FIFO fill delay needed before enabling DMA.
 *   - CPU writes pixels to DDR frame buffer; DMA reads and writes
 *     into the BRAM line buffer every line.
 *
 * Demonstrates:
 *   1. Hardware test patterns (RED, GREEN, BLUE via IP colour selector)
 *   2. Software-drawn patterns in DDR frame buffer (DMA mode)
 *
 * Pixel format (RGB444, 12-bit):
 *   Use RGB444(r, g, b) macro with 4-bit (0-15) channel values.
 *   Pre-defined colours: COLOR_RED, COLOR_GREEN, COLOR_BLUE, etc.
 *
 * Build notes:
 *   Include path: ../drv/
 *   Source files: vga_dma_static_core.cpp
 *********************************************************************/

#include <stdint.h>
#include "xil_mmu.h"
#include "chu_io_map.h"
#include "vga_dma_static_core.h"

// Simple busy-wait delay (approximate, not calibrated)
static void delay_ms(volatile int ms)
{
    while (ms-- > 0)
        for (volatile int i = 0; i < 100000; i++);
}

int main()
{
    // -------------------------------------------------------------------
    // Mark frame buffer region as strongly-ordered (non-cacheable,
    // non-bufferable) so CPU writes go directly to DDR and are
    // immediately visible to the DMA engine on HP0.
    // One Xil_SetTlbAttributes call covers one 1 MB page; the frame
    // buffer is 3.52 MB so cover 4 consecutive 1 MB pages.
    // -------------------------------------------------------------------
    for (uint32_t i = 0; i < 4; i++)
        Xil_SetTlbAttributes(VGA_DMA_STATIC_FB_BASE + i * 0x100000u, 0xC02u);

    // -------------------------------------------------------------------
    // Instantiate driver — writes frame_base_addr to hardware register
    // -------------------------------------------------------------------
    VgaDmaStaticCore vga(VGA_DMA_STATIC_CFG_BASE, VGA_DMA_STATIC_FB_BASE);

    // -------------------------------------------------------------------
    // 1. Hardware test patterns — TPG writes directly into BRAM,
    //    no DDR access required.
    // -------------------------------------------------------------------
    vga.enable_test_pattern(VGA_DMA_TP_RED);
    delay_ms(1000);

    vga.enable_test_pattern(VGA_DMA_TP_GREEN);
    delay_ms(1000);

    vga.enable_test_pattern(VGA_DMA_TP_BLUE);
    delay_ms(1000);

    // -------------------------------------------------------------------
    // 2. DMA mode — CPU writes frame buffer in DDR, DMA streams it
    //    line-by-line into the ping-pong BRAM each line period.
    //
    // Fill the frame buffer first so the first displayed frame is valid,
    // then enable DMA. The DMA starts fetching on the next line swap
    // pulse — no explicit delay needed before drawing new content.
    // -------------------------------------------------------------------
    vga.fill(COLOR_GRAY);
    vga.enable_dma();

    // Debug: first_rdata latches the ARADDR of the first AXI AR transaction.
    // Poll briefly to let the DMA issue its first burst.
    delay_ms(50);
    volatile uint32_t dbg_addr   = vga.first_rdata();  // expect VGA_DMA_STATIC_FB_BASE
    volatile uint32_t dbg_status = vga.status();        // bit0 = dma_active
    (void)dbg_addr;
    (void)dbg_status;

    // Colour bars — visible immediately on next line fetch
    vga.colour_bars();
    delay_ms(2000);

    // Checkerboard (32x32 squares)
    vga.checkerboard(32, COLOR_WHITE, COLOR_BLACK);
    delay_ms(2000);

    // Colour ramp from red to blue
    vga.colour_ramp(COLOR_RED, COLOR_BLUE);
    delay_ms(2000);

    // Coloured quadrants
    vga.fill(COLOR_BLACK);
    vga.fill_rect(0,   0,   639,  359, COLOR_RED);
    vga.fill_rect(640, 0,   1279, 359, COLOR_GREEN);
    vga.fill_rect(0,   360, 639,  719, COLOR_BLUE);
    vga.fill_rect(640, 360, 1279, 719, COLOR_YELLOW);
    delay_ms(2000);

    // White border on black background
    vga.fill(COLOR_BLACK);
    vga.fill_rect(0,    0,   1279, 3,   COLOR_WHITE);  // top
    vga.fill_rect(0,    716, 1279, 719, COLOR_WHITE);  // bottom
    vga.fill_rect(0,    0,   3,    719, COLOR_WHITE);  // left
    vga.fill_rect(1276, 0,   1279, 719, COLOR_WHITE);  // right
    delay_ms(2000);

    // -------------------------------------------------------------------
    // 3. Status check
    // -------------------------------------------------------------------
    if (vga.dma_error()) {
        // AXI read error — fall back to red test pattern
        vga.enable_test_pattern(VGA_DMA_TP_RED);
    } else {
        vga.colour_bars();
    }

    while (1) {}

    return 0;
}

