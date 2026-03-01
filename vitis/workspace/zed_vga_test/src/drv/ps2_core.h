/*****************************************************************//**
 * @file ps2_core.h
 *
 * @brief Access AXI ps2 core
 *
 * @author p chu
 * @version v1.0: initial release
 *********************************************************************/

#ifndef _PS2_H_INCLUDED
#define _PS2_H_INCLUDED

#include <stdint.h>
#include "chu_io_rw.h"

/**
 * ps2 core driver
 *  - transmit/receive raw byte stream to/from PS/2 device
 *  - initialize ps2 mouse or keyboard
 *  - get mouse movement/button activities
 *  - get keyboard char
 */
class Ps2Core {
public:
   /**
    * Register map
    */
   enum {
      RD_DATA_REG     = 0,  /**< read data/status register (word offset 0) */
      PS2_WR_DATA_REG = 2,  /**< 8-bit write data register (word offset 2) */
      RM_RD_DATA_REG  = 3   /**< remove read data from FIFO (word offset 3) */
   };

   /**
    * Field masks for RD_DATA_REG
    */
   enum {
      TX_IDLE_FIELD = 0x00000200,  /**< bit 9:  1 = TX idle, ready to send */
      RX_EMPT_FIELD = 0x00000100,  /**< bit 8:  1 = RX FIFO empty          */
      RX_DATA_FIELD = 0x000000ff   /**< bits 7..0: RX data byte             */
   };

   Ps2Core(uint32_t core_base_addr);
   ~Ps2Core();

   /**
    * Check whether the PS/2 RX FIFO is empty
    * @return 1 if empty, 0 otherwise
    */
   int rx_fifo_empty();

   /**
    * Check whether the PS/2 transmitter is idle
    * @return 1 if idle, 0 otherwise
    */
   int tx_idle();

   /**
    * Send an 8-bit command to the PS/2 device
    * @param cmd byte to transmit
    */
   void tx_byte(uint8_t cmd);

   /**
    * Read one byte from the RX FIFO and remove it
    * @return -1 if FIFO empty, otherwise the byte value
    */
   int rx_byte();

   /**
    * Reset and identify the PS/2 device
    * @return  1 = keyboard
    *          2 = mouse (set to stream mode)
    *         -1 = no response
    *         -2 = unknown device
    *         -3 = failure to set mouse stream mode
    */
   int init();

   /**
    * Get mouse movement and button state (3-byte packet)
    * @param lbtn  left button state (1=pressed)
    * @param rbtn  right button state (1=pressed)
    * @param xmov  signed x-axis movement
    * @param ymov  signed y-axis movement
    * @return 1 if packet available, 0 if no data
    */
   int get_mouse_activity(int *lbtn, int *rbtn, int *xmov, int *ymov);

   /**
    * Get a keyboard character from the scan code stream
    * @param ch  ASCII code of the pressed key
    * @return 1 if a character is available, 0 otherwise
    */
   int get_kb_ch(char *ch);

private:
   uint32_t base_addr;
};

#endif  // _PS2_H_INCLUDED
