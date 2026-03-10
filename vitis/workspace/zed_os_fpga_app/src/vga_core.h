#ifndef VGA_CORE_H
#define VGA_CORE_H

// AXI4-Lite config slave of VGA_DMA_STATIC_1.0 (matches Vivado Address Editor).
#define VGA_DMA_CFG_BASE  0x43C00000U

// Frame buffer: DDR region reserved for DMA VGA controller (VGA_DMA_STATIC_1.0).
// Must match VIDEO_FB origin in lscript.ld.
#define VIDEO_FRAME_BASE  0x1FB00000U

// Frame dimensions (1280x720 @ 60 Hz, driven by VGA_DMA_STATIC_1.0)
#define HMAX 1280
#define VMAX 720

// Color indices (match vid.c / type.h)
#define BLUE 0
#define GREEN 1
#define RED 2
#define CYAN 3
#define YELLOW 4
#define PURPLE 5
#define WHITE 6

// Write one pixel word to the DDR frame buffer.
// offset is a word (pixel) index; each pixel is one 32-bit word.
// Pixel format: bits [11:0] = RGB444 (R=[11:8], G=[7:4], B=[3:0]), bits [31:12] unused.
#define fb_write(offset, data)                                                 \
  (*(volatile unsigned int *)(VIDEO_FRAME_BASE + 4U * (unsigned int)(offset)) = (data))

int fbuf_init(void);
void setpix(int x, int y);
void clrpix(int x, int y);
int kputc(char c);
int kprints(char *s);
int kprintf(char *fmt, ...);

#endif // VGA_CORE_H
