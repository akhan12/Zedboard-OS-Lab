#ifndef PS2_CORE_H
#define PS2_CORE_H

#include <stdint.h>

// AXI PS/2 core base address (match your Vivado Address Editor assignment)
#define PS2_AXI_BASE  0x40010000

// Register word offsets
#define PS2_RD_DATA_REG  0   // read data/status
#define PS2_WR_DATA_REG  2   // write command byte
#define PS2_RM_RD_REG    3   // dummy write to pop RX FIFO

// Field masks for RD_DATA_REG
#define PS2_TX_IDLE  0x00000200  // bit 9: TX idle
#define PS2_RX_EMPT  0x00000100  // bit 8: RX FIFO empty
#define PS2_RX_DATA  0x000000ff  // bits 7..0: data byte

// io_read/io_write: word-offset AXI access (byte addr = base + 4*offset)
#define ps2_read(offset)        (*(volatile uint32_t *)(PS2_AXI_BASE + 4*(offset)))
#define ps2_write(offset, data) (*(volatile uint32_t *)(PS2_AXI_BASE + 4*(offset)) = (uint32_t)(data))

int  ps2_init(void);
void kbd_handler(void);
int  kgetc(void);
int  kgets(char *s);

#endif // PS2_CORE_H
