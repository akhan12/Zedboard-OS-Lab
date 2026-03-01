// vga_core.c
// VGA framebuffer driver for Zedboard custom AXI video IP.
// API mirrors vid.c (K.C. Wang EOS) for drop-in OS integration.
// Hardware: 640x480, pixel writes via AXI at VIDEO_FRAME_BASE.

#include "vga_core.h"

#include "font0" // defines: unsigned char fonts0[]

static char *tab = "0123456789ABCDEF";
static unsigned char cursor;
static unsigned char *font;
static int row, col;
int color; // global, set by caller before drawing (BLUE/GREEN/RED/etc.)

// Pixel color table indexed by color constants
static unsigned int color_table[] = {
    0x00FF0000, // BLUE   (00BBGGRR format)
    0x0000FF00, // GREEN
    0x000000FF, // RED
    0x00FFFF00, // CYAN
    0x0000FFFF, // YELLOW
    0x00FF00FF, // PURPLE
    0x00FFFFFF, // WHITE
};

int fbuf_init(void) {
  int x;
  font = fonts0;
  cursor = 128; // cursor glyph index in font0
  row = 0;
  col = 0;
  color = WHITE;

  // Clear screen: write black to every pixel
  for (x = 0; x < HMAX * VMAX; x++)
    fb_write(x, 0x00000000);

  return 0;
}

void clrpix(int x, int y) { fb_write(y * HMAX + x, 0x00000000); }

void setpix(int x, int y) {
  unsigned int c =
      (color >= 0 && color <= WHITE) ? color_table[color] : 0x00FFFFFF;
  fb_write(y * HMAX + x, c);
}

static void dchar(unsigned char c, int x, int y) {
  int r, bit;
  unsigned char *caddress = font + c * 16;
  unsigned char byte;
  for (r = 0; r < 16; r++) {
    byte = *(caddress + r);
    for (bit = 0; bit < 8; bit++) {
      clrpix(x + bit, y + r);
      if (byte & (1 << bit))
        setpix(x + bit, y + r);
    }
  }
}

static void undchar(unsigned char c, int x, int y) {
  int r, bit;
  unsigned char *caddress = font + c * 16;
  unsigned char byte;
  for (r = 0; r < 16; r++) {
    byte = *(caddress + r);
    for (bit = 0; bit < 8; bit++) {
      if (byte & (1 << bit))
        clrpix(x + bit, y + r);
    }
  }
}

static void scroll(void) {
  int i;
  for (i = 0; i < HMAX * VMAX - HMAX * 16; i++)
    fb_write(
        i, *(volatile unsigned int *)(VIDEO_FRAME_BASE + 4 * (i + HMAX * 16)));
  // blank the last row
  for (i = HMAX * VMAX - HMAX * 16; i < HMAX * VMAX; i++)
    fb_write(i, 0x00000000);
}

static void kpchar(char c, int ro, int co) { dchar(c, co * 8, ro * 16); }

static void unkpchar(char c, int ro, int co) { undchar(c, co * 8, ro * 16); }

static void erasechar(void) {
  int r, bit;
  int x = col * 8;
  int y = row * 16;
  for (r = 0; r < 16; r++)
    for (bit = 0; bit < 8; bit++)
      clrpix(x + bit, y + r);
}

static void clrcursor(void) { erasechar(); }
static void putcursor(void) { kpchar(cursor, row, col); }

int kputc(char c) {
  clrcursor();
  if (c == '\r') {
    col = 0;
    putcursor();
    return 0;
  }
  if (c == '\n') {
    row++;
    if (row >= 25) {
      row = 24;
      scroll();
    }
    putcursor();
    return 0;
  }
  if (c == '\b') {
    if (col > 0) {
      col--;
      erasechar();
    }
    putcursor();
    return 0;
  }
  kpchar(c, row, col);
  col++;
  if (col >= 80) {
    col = 0;
    row++;
    if (row >= 25) {
      row = 24;
      scroll();
    }
  }
  putcursor();
  return 0;
}

int kprints(char *s) {
  while (*s)
    kputc(*s++);
  return 0;
}

static void krpx(int x) {
  char c;
  if (x) {
    c = tab[x % 16];
    krpx(x / 16);
    kputc(c);
  }
}

static void kprintx(int x) {
  kputc('0');
  kputc('x');
  if (x == 0)
    kputc('0');
  else
    krpx(x);
  kputc(' ');
}

static void krpu(int x) {
  char c;
  if (x) {
    c = tab[x % 10];
    krpu(x / 10);
    kputc(c);
  }
}

static void kprintu(int x) {
  if (x == 0)
    kputc('0');
  else
    krpu(x);
  kputc(' ');
}

static void kprinti(int x) {
  if (x < 0) {
    kputc('-');
    x = -x;
  }
  kprintu(x);
}

int kprintf(char *fmt, ...) {
  int *ip;
  char *cp = fmt;
  ip = (int *)&fmt + 1;

  while (*cp) {
    if (*cp != '%') {
      kputc(*cp);
      if (*cp == '\n')
        kputc('\r');
      cp++;
      continue;
    }
    cp++;
    switch (*cp) {
    case 'c':
      kputc((char)*ip);
      break;
    case 's':
      kprints((char *)*ip);
      break;
    case 'd':
      kprinti(*ip);
      break;
    case 'u':
      kprintu(*ip);
      break;
    case 'x':
      kprintx(*ip);
      break;
    }
    cp++;
    ip++;
  }
  return 0;
}
