/*****************************************************************//**
 * @file vga_dma_static_core.cpp
 *
 * @brief Implementation of VgaDmaStaticCore driver.
 *
 * Hardware: VGA_DMA_STATIC_v1_0 IP, static 720p (1280x720 @ 60 Hz).
 *
 * DDR pixel format (1 pixel per 32-bit word):
 *   bits [11:0]  = RGB444  (R=[11:8], G=[7:4], B=[3:0])
 *   bits [31:12] = 0x00000 (unused)
 *********************************************************************/

#include "vga_dma_static_core.h"

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------
VgaDmaStaticCore::VgaDmaStaticCore(uint32_t cfg_base_addr, uint32_t fb_base_addr)
    : cfg_(static_cast<uintptr_t>(cfg_base_addr)),
      fb_(reinterpret_cast<volatile uint32_t *>(fb_base_addr))
{
    // Write frame buffer address into hardware register
    io_write(cfg_, VGA_DMA_REG_FB_ADDR, fb_base_addr);
    // Leave control register at reset value (DMA and TPG both off)
    io_write(cfg_, VGA_DMA_REG_CTRL, 0);
}

// -------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------
void VgaDmaStaticCore::enable_dma()
{
    // Enable DMA, disable test pattern
    io_write(cfg_, VGA_DMA_REG_CTRL, VGA_DMA_CTRL_DMA_EN);
}

void VgaDmaStaticCore::enable_test_pattern(uint8_t colour_sel)
{
    uint32_t ctrl = VGA_DMA_CTRL_TP_EN;
    ctrl |= (uint32_t)(colour_sel & 0x3) << 2;   // tp_colour_sel → bits [3:2]
    io_write(cfg_, VGA_DMA_REG_CTRL, ctrl);
}

void VgaDmaStaticCore::disable()
{
    io_write(cfg_, VGA_DMA_REG_CTRL, 0);
}

// -------------------------------------------------------------------
// Status
// -------------------------------------------------------------------
uint32_t VgaDmaStaticCore::status() const
{
    return io_read(cfg_, VGA_DMA_REG_STATUS);
}

bool VgaDmaStaticCore::dma_active() const
{
    return (status() & VGA_DMA_STATUS_ACTIVE) != 0;
}

bool VgaDmaStaticCore::dma_error() const
{
    return (status() & VGA_DMA_STATUS_ERROR) != 0;
}

uint32_t VgaDmaStaticCore::first_rdata() const
{
    return io_read(cfg_, VGA_DMA_REG_FIRST_RD);
}

// -------------------------------------------------------------------
// Pixel helpers
// -------------------------------------------------------------------
void VgaDmaStaticCore::set_pixel(int x, int y, uint16_t colour)
{
    if (x < 0 || x >= (int)VGA_DMA_WIDTH || y < 0 || y >= (int)VGA_DMA_HEIGHT)
        return;
    fb_[y * (int)VGA_DMA_WIDTH + x] = colour & 0xFFF;
}

void VgaDmaStaticCore::fill(uint16_t colour)
{
    uint32_t word = colour & 0xFFF;
    volatile uint32_t *p = fb_;
    for (uint32_t i = 0; i < VGA_DMA_FRAME_WORDS; i++)
        p[i] = word;
}

void VgaDmaStaticCore::fill_rect(int x0, int y0, int x1, int y1, uint16_t colour)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= (int)VGA_DMA_WIDTH)  x1 = (int)VGA_DMA_WIDTH  - 1;
    if (y1 >= (int)VGA_DMA_HEIGHT) y1 = (int)VGA_DMA_HEIGHT - 1;
    if (x0 > x1 || y0 > y1) return;

    uint32_t word = colour & 0xFFF;
    for (int y = y0; y <= y1; y++) {
        volatile uint32_t *row = fb_ + y * (int)VGA_DMA_WIDTH;
        for (int x = x0; x <= x1; x++)
            row[x] = word;
    }
}

void VgaDmaStaticCore::checkerboard(int sq_size, uint16_t c0, uint16_t c1)
{
    uint32_t w0 = c0 & 0xFFF;
    uint32_t w1 = c1 & 0xFFF;
    for (int y = 0; y < (int)VGA_DMA_HEIGHT; y++) {
        volatile uint32_t *row = fb_ + y * (int)VGA_DMA_WIDTH;
        int sq_row = y / sq_size;
        for (int x = 0; x < (int)VGA_DMA_WIDTH; x++) {
            int sq_col = x / sq_size;
            row[x] = ((sq_col + sq_row) & 1) ? w1 : w0;
        }
    }
}

void VgaDmaStaticCore::colour_bars()
{
    static const uint16_t bars[8] = {
        COLOR_WHITE,
        COLOR_YELLOW,
        COLOR_CYAN,
        COLOR_GREEN,
        COLOR_MAGENTA,
        COLOR_RED,
        COLOR_BLUE,
        COLOR_BLACK
    };

    int bar_h = (int)VGA_DMA_HEIGHT / 8;   // 90 lines per bar

    for (int y = 0; y < (int)VGA_DMA_HEIGHT; y++) {
        int bar_idx = y / bar_h;
        if (bar_idx > 7) bar_idx = 7;
        uint32_t word = bars[bar_idx] & 0xFFF;
        volatile uint32_t *row = fb_ + y * (int)VGA_DMA_WIDTH;
        for (int x = 0; x < (int)VGA_DMA_WIDTH; x++)
            row[x] = word;
    }
}

void VgaDmaStaticCore::colour_ramp(uint16_t c0, uint16_t c1)
{
    int r0 = (c0 >> 8) & 0xF,  g0 = (c0 >> 4) & 0xF,  b0 = c0 & 0xF;
    int r1 = (c1 >> 8) & 0xF,  g1 = (c1 >> 4) & 0xF,  b1 = c1 & 0xF;
    int w  = (int)VGA_DMA_WIDTH;

    for (int y = 0; y < (int)VGA_DMA_HEIGHT; y++) {
        volatile uint32_t *row = fb_ + y * w;
        for (int x = 0; x < w; x++) {
            int r = r0 + (r1 - r0) * x / (w - 1);
            int g = g0 + (g1 - g0) * x / (w - 1);
            int b = b0 + (b1 - b0) * x / (w - 1);
            row[x] = (uint32_t)RGB444(r & 0xF, g & 0xF, b & 0xF);
        }
    }
}
