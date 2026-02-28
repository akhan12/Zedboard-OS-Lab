/*********************************************************************
 * @file chu_io_map.h
 *
 * @brief address map for video AXI IP
 *
 * Update VIDEO_AXI_BASE to match the base address assigned in the
 * Vivado Address Editor for the video_axi_v1_0 peripheral.
 *********************************************************************/

#ifndef _CHU_IO_MAP_H_INCLUDED
#define _CHU_IO_MAP_H_INCLUDED

// -------------------------------------------------------------------
// Set this to match your Vivado Address Editor assignment
// -------------------------------------------------------------------
#define VIDEO_AXI_BASE  0x40000000

// -------------------------------------------------------------------
// Video slot base addresses
// Each slot occupies 0x10000 bytes (16384 x 32-bit words)
// Matches chu_video_controller: slot select = video_addr[16:14]
//                                           = axi_awaddr[18:16]
// -------------------------------------------------------------------
#define get_video_slot_addr(slot)  ((uint32_t)(VIDEO_AXI_BASE + (slot) * 0x10000))

// Video slot numbers
#define V0_SYNC    0
#define V1_MOUSE   1
#define V2_OSD     2
#define V3_GHOST   3
#define V4_USER4   4
#define V5_USER5   5
#define V6_GRAY    6
#define V7_BAR     7

// -------------------------------------------------------------------
// Frame buffer base address
// video_addr[20]=1 -> axi_awaddr[22]=1 -> offset 0x400000 from base
// -------------------------------------------------------------------
#define VIDEO_FRAME_BASE  ((uint32_t)(VIDEO_AXI_BASE + 0x400000))

#endif // _CHU_IO_MAP_H_INCLUDED
