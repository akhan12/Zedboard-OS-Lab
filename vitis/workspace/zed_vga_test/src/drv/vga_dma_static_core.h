/*****************************************************************//**
 * @file vga_dma_static_core.h
 *
 * @brief Driver for VGA_DMA_STATIC_v1_0 — static 720p DMA video IP.
 *
 * Hardware configuration (fixed at synthesis):
 *   Resolution  : 1280 x 720 @ 60 Hz (720p)
 *   Pixel clock : 74.25 MHz (external, from MMCM)
 *   AXI clock   : 150 MHz (HP0 port)
 *   Pixel depth : 12-bit RGB444
 *
 * Register map (AXI4-Lite slave, byte addresses):
 *   0x00  frame_base_addr [31:0]   R/W  DDR byte address of frame buffer start
 *   0x04  control         [31:0]   R/W  [0]=dma_enable
 *                                        [1]=use_test_pattern
 *                                        [3:2]=tp_colour_sel (00=RED 01=GREEN 10=BLUE)
 *   0x08  status          [31:0]   R    [0]=dma_active  [1]=dma_error
 *   0x0C  first_rdata     [31:0]   R    first ARADDR issued by DMA (debug)
 *                                        non-zero = AR channel reached interconnect
 *
 * DDR pixel format (1 pixel per 32-bit word):
 *   bits [11:0]  = RGB444  (R=[11:8], G=[7:4], B=[3:0])
 *   bits [31:12] = 0x00000 (unused)
 *
 * Frame buffer layout:
 *   1280 words per line  (1 word per pixel)
 *   720 lines per frame
 *   Total: 921600 words = 3686400 bytes ≈ 3.52 MB
 *********************************************************************/

#ifndef _VGA_DMA_STATIC_CORE_H_INCLUDED
#define _VGA_DMA_STATIC_CORE_H_INCLUDED

#include <inttypes.h>
#include "chu_io_rw.h"

// -------------------------------------------------------------------
// Register word offsets
// -------------------------------------------------------------------
#define VGA_DMA_REG_FB_ADDR   0   // frame_base_addr  R/W
#define VGA_DMA_REG_CTRL      1   // control          R/W
#define VGA_DMA_REG_STATUS    2   // status           R
#define VGA_DMA_REG_FIRST_RD  3   // first DDR word read by DMA (debug) R

// Control register bit masks
#define VGA_DMA_CTRL_DMA_EN   (1u << 0)   // dma_enable
#define VGA_DMA_CTRL_TP_EN    (1u << 1)   // use_test_pattern
#define VGA_DMA_CTRL_TP_SEL0  (1u << 2)   // tp_colour_sel[0]
#define VGA_DMA_CTRL_TP_SEL1  (1u << 3)   // tp_colour_sel[1]
#define VGA_DMA_CTRL_TP_MASK  (3u << 2)   // tp_colour_sel mask

// Status register bit masks
#define VGA_DMA_STATUS_ACTIVE (1u << 0)   // dma_active
#define VGA_DMA_STATUS_ERROR  (1u << 1)   // dma_error

// Test pattern colour select values (bits [3:2] of control register)
#define VGA_DMA_TP_RED        0u   // solid red   (12'hF00)
#define VGA_DMA_TP_GREEN      1u   // solid green (12'h0F0)
#define VGA_DMA_TP_BLUE       2u   // solid blue  (12'h00F)

// Frame dimensions
#define VGA_DMA_WIDTH         1280u
#define VGA_DMA_HEIGHT        720u
#define VGA_DMA_WORDS_PER_LINE VGA_DMA_WIDTH           // 1280 (1 word per pixel)
#define VGA_DMA_FRAME_WORDS   (VGA_DMA_WORDS_PER_LINE * VGA_DMA_HEIGHT)  // 921600

class VgaDmaStaticCore {
public:
    // ------------------------------------------------------------------
    // Constructor
    // @param cfg_base_addr  AXI4-Lite slave base address (S00_AXI)
    // @param fb_base_addr   DDR frame buffer base address
    // ------------------------------------------------------------------
    VgaDmaStaticCore(uint32_t cfg_base_addr, uint32_t fb_base_addr);

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    /**
     * Enable DMA mode: DMA reads pixels from DDR frame buffer.
     * Disables test pattern generator.
     */
    void enable_dma();

    /**
     * Enable test pattern mode: hardware generates a solid-colour pattern.
     * Disables DMA.
     * @param colour_sel  VGA_DMA_TP_RED (0), VGA_DMA_TP_GREEN (1), or VGA_DMA_TP_BLUE (2)
     */
    void enable_test_pattern(uint8_t colour_sel = VGA_DMA_TP_RED);

    /**
     * Disable both DMA and test pattern output.
     * Video output will show the last data remaining in the line buffer.
     */
    void disable();

    // ------------------------------------------------------------------
    // Status
    // ------------------------------------------------------------------

    /**
     * @return true if the DMA engine is currently fetching from DDR
     */
    bool dma_active() const;

    /**
     * @return true if an AXI read error was detected
     */
    bool dma_error() const;

    /**
     * @return raw status register value
     */
    uint32_t status() const;

    /**
     * @return first ARADDR issued by the DMA engine (debug).
     *         Non-zero confirms the AXI master AR channel is active.
     *         Expected value: VGA_DMA_STATIC_FB_BASE when DMA first starts.
     *         Resets to 0 on hardware reset.
     */
    uint32_t first_rdata() const;

    // ------------------------------------------------------------------
    // Pixel helpers
    //
    // Pixel colour format: 0x00000RGB  (12-bit RGB444)
    //   R = bits [11:8], G = bits [7:4], B = bits [3:0]
    //   Values 0x000 (black) … 0xFFF (white)
    //
    // Convenience macros below construct RGB444 from 4-bit components.
    // ------------------------------------------------------------------

    /**
     * Write a single pixel to the DDR frame buffer.
     * @param x      column (0 … 1279)
     * @param y      row    (0 … 719)
     * @param colour RGB444 colour value (12-bit, lower nibble per channel)
     */
    void set_pixel(int x, int y, uint16_t colour);

    /**
     * Fill the entire frame with a single colour.
     * @param colour RGB444 colour value
     */
    void fill(uint16_t colour);

    /**
     * Fill a rectangular region.
     * @param x0, y0  top-left corner (inclusive)
     * @param x1, y1  bottom-right corner (inclusive)
     * @param colour  RGB444 colour value
     */
    void fill_rect(int x0, int y0, int x1, int y1, uint16_t colour);

    /**
     * Draw a checkerboard pattern.
     * @param sq_size  pixel size of each square (should be even)
     * @param c0, c1   RGB444 colours for the two alternating squares
     */
    void checkerboard(int sq_size, uint16_t c0, uint16_t c1);

    /**
     * Draw horizontal colour bars (8 bars, top to bottom).
     * Colours: white, yellow, cyan, green, magenta, red, blue, black.
     */
    void colour_bars();

    /**
     * Draw a smooth horizontal colour ramp from colour c0 (left) to c1 (right).
     * Each row is identical; the transition samples each channel independently.
     * @param c0  left-edge RGB444 colour
     * @param c1  right-edge RGB444 colour
     */
    void colour_ramp(uint16_t c0, uint16_t c1);

    // ------------------------------------------------------------------
    // Frame buffer direct access
    // Returns a pointer to the DDR frame buffer for direct pixel writes.
    // ------------------------------------------------------------------
    volatile uint32_t* frame_buf() const { return fb_; }

private:
    uintptr_t         cfg_;   // AXI4-Lite config base address
    volatile uint32_t *fb_;   // DDR frame buffer pointer


};

// -------------------------------------------------------------------
// Colour construction helpers (12-bit RGB444, 4 bits per channel)
// -------------------------------------------------------------------
#define RGB444(r, g, b)  ((uint16_t)(((r) & 0xF) << 8 | ((g) & 0xF) << 4 | ((b) & 0xF)))

// Common colours
#define COLOR_BLACK    RGB444(0x0, 0x0, 0x0)
#define COLOR_WHITE    RGB444(0xF, 0xF, 0xF)
#define COLOR_RED      RGB444(0xF, 0x0, 0x0)
#define COLOR_GREEN    RGB444(0x0, 0xF, 0x0)
#define COLOR_BLUE     RGB444(0x0, 0x0, 0xF)
#define COLOR_YELLOW   RGB444(0xF, 0xF, 0x0)
#define COLOR_CYAN     RGB444(0x0, 0xF, 0xF)
#define COLOR_MAGENTA  RGB444(0xF, 0x0, 0xF)
#define COLOR_GRAY     RGB444(0x8, 0x8, 0x8)

#endif // _VGA_DMA_STATIC_CORE_H_INCLUDED
