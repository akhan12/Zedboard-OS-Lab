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

// Character cell shadow buffer: tracks what is displayed on screen.
// Used by scroll() to avoid reading back from write-only AXI framebuffer.
#define ROWS 30
#define COLS 80
static char cbuf[ROWS][COLS];
static int  colbuf[ROWS][COLS]; // per-cell colour index

// Pixel color table indexed by color constants.
// Hardware uses 9-bit packed format: RRRGGGGBBB ([8:6]=R [5:2]=G [1:0]=B)
static unsigned int color_table[] = {
    0x007, // BLUE   000 0000 111
    0x038, // GREEN  000 1110 000
    0x1C0, // RED    111 0000 000
    0x03F, // CYAN   000 1111 111
    0x1F8, // YELLOW 111 1110 000
    0x1C7, // PURPLE 111 0001 111
    0x1FF, // WHITE  111 1111 111
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

  // Clear character and colour shadow buffers
  for (x = 0; x < ROWS * COLS; x++) {
    ((char *)cbuf)[x] = 0;
    ((int *)colbuf)[x] = WHITE;
  }

  return 0;
}

void clrpix(int x, int y) { fb_write(y * HMAX + x, 0x00000000); }

void setpix(int x, int y) {
  unsigned int c =
      (color >= 0 && color <= WHITE) ? color_table[color] : 0x1FFU;
  fb_write(y * HMAX + x, c);
}

static void dchar_col(unsigned char c, int x, int y, int col_idx) {
  int r, bit;
  unsigned char *caddress = font + c * 16;
  unsigned char byte;
  unsigned int pix =
      (col_idx >= 0 && col_idx <= WHITE) ? color_table[col_idx] : 0x1FFU;
  for (r = 0; r < 16; r++) {
    byte = *(caddress + r);
    for (bit = 0; bit < 8; bit++) {
      clrpix(x + bit, y + r);
      if (byte & (1 << bit))
        fb_write((y + r) * HMAX + (x + bit), pix);
    }
  }
}

static void dchar(unsigned char c, int x, int y) {
  dchar_col(c, x, y, color);
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
  int r, c;
  // Shift character and colour shadows up one row
  for (r = 0; r < ROWS - 1; r++)
    for (c = 0; c < COLS; c++) {
      cbuf[r][c]   = cbuf[r + 1][c];
      colbuf[r][c] = colbuf[r + 1][c];
    }
  // Clear last row in shadows
  for (c = 0; c < COLS; c++) {
    cbuf[ROWS - 1][c]   = 0;
    colbuf[ROWS - 1][c] = WHITE;
  }
  // Redraw all rows using per-cell colours
  for (r = 0; r < ROWS; r++)
    for (c = 0; c < COLS; c++)
      dchar_col(cbuf[r][c], c * 8, r * 16, colbuf[r][c]);
}

static void kpchar(char c, int ro, int co) {
  cbuf[ro][co]   = c;
  colbuf[ro][co] = color;
  dchar_col(c, co * 8, ro * 16, color);
}

static void unkpchar(char c, int ro, int co) { undchar(c, co * 8, ro * 16); }

static void erasechar(void) {
  int r, bit;
  int x = col * 8;
  int y = row * 16;
  cbuf[row][col]   = 0;
  colbuf[row][col] = WHITE;
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
    if (row >= ROWS) {
      row = ROWS - 1;
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
    if (row >= ROWS) {
      row = ROWS - 1;
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
