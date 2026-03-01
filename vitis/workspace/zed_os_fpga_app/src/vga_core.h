#ifndef VGA_CORE_H
#define VGA_CORE_H

// Frame buffer base address (AXI video IP, matches Vivado Address Editor)
#define VIDEO_AXI_BASE 0x40000000
#define VIDEO_FRAME_BASE (VIDEO_AXI_BASE + 0x400000)

// Frame dimensions
#define HMAX 640
#define VMAX 480

// Color indices (match vid.c / type.h)
#define BLUE 0
#define GREEN 1
#define RED 2
#define CYAN 3
#define YELLOW 4
#define PURPLE 5
#define WHITE 6

// io_write: write 32-bit word to AXI-mapped frame buffer.
// offset is a word (pixel) offset; byte address = base + 4*offset.
#define fb_write(offset, data)                                                 \
  (*(volatile unsigned int *)(VIDEO_FRAME_BASE + 4 * (offset)) = (data))

int fbuf_init(void);
void setpix(int x, int y);
void clrpix(int x, int y);
int kputc(char c);
int kprints(char *s);
int kprintf(char *fmt, ...);

#endif // VGA_CORE_H
