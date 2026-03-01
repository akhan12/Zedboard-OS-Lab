// ps2_core.c
// PS/2 keyboard driver for Zedboard custom AXI PS/2 IP.
// API mirrors kbd.c (K.C. Wang EOS) for drop-in OS integration.
// Hardware: AXI PS/2 RX FIFO at PS2_AXI_BASE.

#include "ps2_core.h"
#include "type.h"

// Scan code set 2: scan code -> ASCII (lowercase)
static const unsigned char ltab[128] = {
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,  'q', '1',  0,    0,   0,  'z', 's', 'a', 'w', '2',  0,
  0,  'c', 'x', 'd', 'e', '4', '3',  0,    0,  ' ', 'v', 'f', 't', 'r', '5',  0,
  0,  'n', 'b', 'h', 'g', 'y', '6',  0,    0,   0,  'm', 'j', 'u', '7', '8',  0,
  0,  ',', 'k', 'i', 'o', '0', '9',  0,    0,  '.', '/', 'l', ';', 'p', '-',  0,
  0,   0,  '\'', 0,  '[', '=',  0,   0,    0,   0, '\r', ']',  0, '\\',  0,   0,
  0,   0,   0,   0,   0,   0,  '\b', 0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0
};

// Scan code set 2: scan code -> ASCII (uppercase / shifted)
static const unsigned char utab[128] = {
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,  'Q', '!',  0,    0,   0,  'Z', 'S', 'A', 'W', '@',  0,
  0,  'C', 'X', 'D', 'E', '$', '#',  0,    0,  ' ', 'V', 'F', 'T', 'R', '%',  0,
  0,  'N', 'B', 'H', 'G', 'Y', '^',  0,    0,   0,  'M', 'J', 'U', '&', '*',  0,
  0,  '<', 'K', 'I', 'O', ')', '(',  0,    0,  '>', '?', 'L', ':', 'P', '_',  0,
  0,   0,  '"',  0,  '{', '+',  0,   0,    0,   0, '\r', '}',  0,  '|',  0,   0,
  0,   0,   0,   0,   0,   0,  '\b', 0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0
};

#define LSHIFT  0x12
#define RSHIFT  0x59
#define LCTRL   0x14

#define KBD_BUFSIZE 128

typedef volatile struct kbd {
    char buf[KBD_BUFSIZE];
    int  head, tail;
    struct semaphore data, kline;
} KBD;

static volatile KBD kbd;

static int shifted = 0;
static int release = 0;
static int control = 0;

// Low-level RX FIFO helpers
static int ps2_rx_empty(void)
{
    return (int)((ps2_read(PS2_RD_DATA_REG) & PS2_RX_EMPT) >> 8);
}

static int ps2_rx_byte(void)
{
    uint32_t data;
    if (ps2_rx_empty())
        return -1;
    data = ps2_read(PS2_RD_DATA_REG) & PS2_RX_DATA;
    ps2_write(PS2_RM_RD_REG, 0);  // pop FIFO
    return (int)data;
}

int ps2_init(void)
{
    KBD *kp = (KBD *)&kbd;

    // Flush any stale bytes in RX FIFO
    while (!ps2_rx_empty())
        ps2_rx_byte();

    kp->head = kp->tail = 0;
    kp->data.value = 0;  kp->data.queue = 0;
    kp->kline.value = 0; kp->kline.queue = 0;

    shifted = 0;
    release = 0;
    control = 0;

    return 0;
}

// kbd_handler: call this from your interrupt dispatcher when the PS/2 IRQ fires.
// Mirrors kbd_handler2() from kbd.c (scan code set 2).
void kbd_handler(void)
{
    unsigned char scode, c;
    KBD *kp = (KBD *)&kbd;

    if (ps2_rx_empty())
        return;

    scode = (unsigned char)ps2_rx_byte();

    if (scode == 0xF0) {        // break code prefix — next byte is the released key
        release = 1;
        return;
    }

    if (release) {
        if (scode == LSHIFT || scode == RSHIFT)
            shifted = 0;
        if (scode == LCTRL)
            control = 0;
        release = 0;
        return;
    }

    // Make codes for modifier keys
    if (scode == LSHIFT || scode == RSHIFT) { shifted = 1; return; }
    if (scode == LCTRL)                      { control = 1; return; }

    // Control-C
    if (control && scode == 0x21) {
        kprintf("Control-C\n");
        control = 0;
        return;
    }

    // Control-D
    if (control && scode == 0x23) {
        c = 0x04;
        kp->buf[kp->head++] = c;
        kp->head %= KBD_BUFSIZE;
        V(&kp->data);
        return;
    }

    c = shifted ? utab[scode] : ltab[scode];
    if (c == 0)
        return;

    kp->buf[kp->head++] = c;
    kp->head %= KBD_BUFSIZE;

    V(&kp->data);
    if (c == '\r')
        V(&kp->kline);
}

int kgetc(void)
{
    char c;
    KBD *kp = (KBD *)&kbd;

    P(&kp->data);
    c = kp->buf[kp->tail++];
    kp->tail %= KBD_BUFSIZE;
    return c;
}

int kgets(char *s)
{
    char c;
    char *start = s;
    while ((c = (char)kgetc()) != '\r') {
        if (c == '\b') {
            if (s > start) s--;
            continue;
        }
        *s++ = c;
    }
    *s = 0;
    return s - start;
}
