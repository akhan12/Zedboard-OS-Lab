// ps2_core.c
// PS/2 keyboard driver for Zedboard custom AXI PS/2 IP.
// Polling mode — IP has no interrupt output.
// API mirrors kbd.c (K.C. Wang EOS) for drop-in OS integration.

#include "ps2_core.h"

// Spin-delay: ~1 ms per count at 666 MHz (2 cycles/iter, ~333k iters/ms)
static void ps2_delay_ms(int ms) {
  volatile int i;
  for (i = 0; i < ms * 333000; i++)
    ;
}

// Scan code set 2: scan code -> ASCII (lowercase)
static const unsigned char ltab[128] = {
    0, 0,   0,    0,   0,   0,   0,    0, 0, 0,   0,    0,   0,   0,    0,   0,
    0, 0,   0,    0,   0,   'q', '1',  0, 0, 0,   'z',  's', 'a', 'w',  '2', 0,
    0, 'c', 'x',  'd', 'e', '4', '3',  0, 0, ' ', 'v',  'f', 't', 'r',  '5', 0,
    0, 'n', 'b',  'h', 'g', 'y', '6',  0, 0, 0,   'm',  'j', 'u', '7',  '8', 0,
    0, ',', 'k',  'i', 'o', '0', '9',  0, 0, '.', '/',  'l', ';', 'p',  '-', 0,
    0, 0,   '\'', 0,   '[', '=', 0,    0, 0, 0,   '\r', ']', 0,   '\\', 0,   0,
    0, 0,   0,    0,   0,   0,   '\b', 0, 0, 0,   0,    0,   0,   0,    0,   0,
    0, 0,   0,    0,   0,   0,   0,    0, 0, 0,   0,    0,   0,   0,    0,   0};

// Scan code set 2: scan code -> ASCII (uppercase / shifted)
static const unsigned char utab[128] = {
    0, 0,   0,   0,   0,   0,   0,    0, 0, 0,   0,    0,   0,   0,   0,   0,
    0, 0,   0,   0,   0,   'Q', '!',  0, 0, 0,   'Z',  'S', 'A', 'W', '@', 0,
    0, 'C', 'X', 'D', 'E', '$', '#',  0, 0, ' ', 'V',  'F', 'T', 'R', '%', 0,
    0, 'N', 'B', 'H', 'G', 'Y', '^',  0, 0, 0,   'M',  'J', 'U', '&', '*', 0,
    0, '<', 'K', 'I', 'O', ')', '(',  0, 0, '>', '?',  'L', ':', 'P', '_', 0,
    0, 0,   '"', 0,   '{', '+', 0,    0, 0, 0,   '\r', '}', 0,   '|', 0,   0,
    0, 0,   0,   0,   0,   0,   '\b', 0, 0, 0,   0,    0,   0,   0,   0,   0,
    0, 0,   0,   0,   0,   0,   0,    0, 0, 0,   0,    0,   0,   0,   0,   0};

#define LSHIFT 0x12
#define RSHIFT 0x59
#define LCTRL 0x14

static int shifted = 0;
static int release = 0;
static int control = 0;

// Low-level RX FIFO helpers
static int ps2_rx_empty(void) {
  return (int)((ps2_read(PS2_RD_DATA_REG) & PS2_RX_EMPT) >> 8);
}

static unsigned char ps2_rx_byte(void) {
  unsigned char data = (unsigned char)(ps2_read(PS2_RD_DATA_REG) & PS2_RX_DATA);
  ps2_write(PS2_RM_RD_REG, 0); // pop FIFO
  return data;
}

// ps2_init: reset the PS/2 device and confirm it is a keyboard.
// Mirrors Ps2Core::init() from the C++ driver.
// Returns  1 = keyboard detected (success)
//         -1 = no ACK or BAT failed
int ps2_init(void) {
  int packet;

  // Flush stale bytes
  while (!ps2_rx_empty())
    ps2_rx_byte();

  shifted = 0;
  release = 0;
  control = 0;

  // Send reset command
  ps2_write(PS2_WR_DATA_REG, 0xff);
  ps2_delay_ms(200); // give device time to complete self-test

  // Expect 0xfa (ACK) then 0xaa (BAT pass)
  if (ps2_rx_byte() != 0xfa)
    return -1;
  if (ps2_rx_byte() != 0xaa)
    return -1;

  // If an extra 0x00 follows the device is a mouse — unexpected here
  packet = ps2_rx_empty() ? -1 : (int)ps2_rx_byte();
  if (packet == 0x00)
    return -1; // mouse connected, not a keyboard

  return 1; // keyboard confirmed
}

// ps2_poll: read one scan code from the FIFO and decode it into a char.
// Returns the ASCII character, 0 if no printable key yet, or -1 if FIFO empty.
int ps2_poll(void) {
  unsigned char scode, c;

  if (ps2_rx_empty())
    return -1;

  scode = ps2_rx_byte();

  if (scode == 0xF0) { // break code prefix
    release = 1;
    return 0;
  }

  if (release) {
    if (scode == LSHIFT || scode == RSHIFT)
      shifted = 0;
    if (scode == LCTRL)
      control = 0;
    release = 0;
    return 0;
  }

  if (scode == LSHIFT || scode == RSHIFT) {
    shifted = 1;
    return 0;
  }
  if (scode == LCTRL) {
    control = 1;
    return 0;
  }

  if (control && scode == 0x21) {
    control = 0;
    return 0x03;
  } // Ctrl-C (ETX)
  if (control && scode == 0x23) {
    return 0x04;
  } // Ctrl-D (EOT)

  c = shifted ? utab[scode] : ltab[scode];
  return (int)c; // 0 if not a printable key
}

// kgetc: block (spin) until a printable key is available, return its ASCII
// value.
int kgetc(void) {
  int c;
  do {
    c = ps2_poll();
  } while (c <= 0); // spin on empty FIFO (-1) or non-printable make code (0)
  return c;
}

int kgets(char *s) {
  char c;
  char *start = s;
  while ((c = (char)kgetc()) != '\r') {
    if (c == '\b') {
      if (s > start)
        s--;
      continue;
    }
    *s++ = c;
  }
  *s = 0;
  return s - start;
}