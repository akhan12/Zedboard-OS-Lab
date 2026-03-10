/*********************************************************************
 * @file chu_io_map.h
 *
 * @brief Address map for VGA_DMA_STATIC_v1_0 AXI peripherals.
 *
 * Update each BASE address to match the Vivado Address Editor
 * assignment for your specific design.
 *********************************************************************/

#ifndef _CHU_IO_MAP_H_INCLUDED
#define _CHU_IO_MAP_H_INCLUDED

// -------------------------------------------------------------------
// VGA_DMA_STATIC_v1_0 — Static 720p DMA VGA controller
//
// VGA_DMA_STATIC_CFG_BASE : AXI4-Lite slave (S00_AXI) — config registers
//   Connected to PS GP0 port. 3 x 32-bit registers, 32-byte window.
//   Update to match Vivado Address Editor.
//
//   Reg 0 (0x00): frame_base_addr[31:0]   R/W  DDR byte address of frame buffer
//   Reg 1 (0x04): control                 R/W  [0]=dma_enable
//                                               [1]=use_test_pattern
//                                               [3:2]=tp_colour_sel
//   Reg 2 (0x08): status                  R    [0]=dma_active
//                                               [1]=dma_error
//
// VGA_DMA_STATIC_FB_BASE : frame buffer start address in PS DDR3
//   The CPU writes pixels here as plain uint32_t stores.
//   The DMA master reads from this address (HP0 port, 150 MHz).
//   Pixel packing: [31:20]=pixel_a[11:0], [19:8]=pixel_b[11:0], [7:0]=0x00
//   Two RGB444 pixels per 32-bit word.
//   Frame size: 1280x720 / 2 pixels per word = 460800 words = 1843200 bytes
//   Must be 4-byte aligned and within the 512 MB DDR window.
//   Typical choice: 0x1E000000 (last ~32 MB of 512 MB DDR).
//   Update to match your linker / device tree reservation.
// -------------------------------------------------------------------
#define VGA_DMA_STATIC_CFG_BASE  0x43C00000   // placeholder — update from Address Editor
#define VGA_DMA_STATIC_FB_BASE   0x1FB00000   // placeholder — update to match reservation

#endif // _CHU_IO_MAP_H_INCLUDED
