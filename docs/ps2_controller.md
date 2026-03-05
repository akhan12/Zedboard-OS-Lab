# PS/2 Controller

## Overview

The PS/2 controller is a custom AXI4-Lite peripheral implemented on the Zynq PL. It handles the bidirectional PS/2 serial protocol, buffering received scan codes in a FIFO that the ARM processor reads by polling.

The design is adapted from Pong Chu's *FPGA Prototyping by SystemVerilog Examples*, ported from his custom FPro bus to AXI4-Lite.

---

## Hardware Architecture

```
PS/2 Keyboard
    │  ps2c (clock), ps2d (data)  — open-drain, pulled high
    ▼
ps2rx  (receiver FSM)
    │  rx_done_tick + 8-bit scan code
    ▼
FIFO (64-entry, 8-bit wide)
    │
ps2tx  (transmitter FSM)  ◄── CPU writes command bytes (e.g. reset 0xFF)
    │
    ▼
AXI Slave Wrapper (ps2_axi_v1_0)
    │  AXI4-Lite reads/writes
    ▼
ARM CPU (PS)
```

---

## PS/2 Protocol

PS/2 is a synchronous serial protocol. The keyboard drives an 11-bit frame on the `ps2d` line, clocked by the keyboard's own `ps2c` signal:

```
[ start(0) | D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | parity(odd) | stop(1) ]
```

Data is sampled on the **falling edge** of `ps2c`. The host (FPGA) can inhibit transmission by holding `ps2c` low.

---

## Receiver FSM (`ps2rx.sv`)

The receiver is a three-state FSMD (Finite State Machine with Datapath):

| State | Description |
|-------|-------------|
| `idle` | Waiting for falling edge on `ps2c` with `rx_en` asserted |
| `dps` | Shifting in 10 bits (8 data + parity + stop) on each falling edge |
| `load` | One extra clock to complete the final shift; asserts `rx_done_tick` |

**Clock filter:** `ps2c` passes through an 8-bit shift register before being used for edge detection. A falling edge is only recognised when the filter transitions from all-ones to all-zeros, rejecting glitches shorter than 8 system clock cycles (~320 ns at 25 MHz).

On `rx_done_tick`, the 8 data bits (`b_reg[8:1]`) are written into the FIFO automatically.

**Transmit interlock:** The receiver only operates when `rx_en` is high, which is gated by the transmitter's `tx_idle` signal. This prevents the receiver from misinterpreting bus contention during a host-to-device transmission.

---

## FIFO (`fifo.sv`, `fifo_ctrl.sv`)

The receive FIFO is 64 entries deep (parameterised by `W_SIZE = 6` address bits) and 8 bits wide. The CPU reads one byte at a time:

1. Read `PS2_RD_DATA_REG` — returns `{ TX_IDLE[9], RX_EMPTY[8], RX_DATA[7:0] }`
2. Write any value to `PS2_RM_RD_REG` — pops the front entry from the FIFO

The FIFO has no interrupt output wired to the GIC, so the CPU must poll `RX_EMPTY` to know when data is available.

---

## AXI Register Map

Base address: `0x40800000`

| Offset | Direction | Description |
|--------|-----------|-------------|
| `0` (`PS2_RD_DATA_REG`) | Read | `[9]` TX idle, `[8]` RX FIFO empty, `[7:0]` RX data byte |
| `2` (`PS2_WR_DATA_REG`) | Write | Send command byte to keyboard (e.g. `0xFF` = reset) |
| `3` (`PS2_RM_RD_REG`) | Write | Dummy write — pops front entry from RX FIFO |

---

## Software Interfaces

### C++ Driver (`Ps2Core` — `ps2_core.cpp`)

Used by the standalone test application (`zed_vga_test`). Supports both keyboard and mouse.

**Instantiation:**
```cpp
Ps2Core ps2(PS2_AXI_BASE);  // 0x40800000
```

**Initialisation — identifies the connected device:**
```cpp
int device = ps2.init();
// returns  1 = keyboard
//          2 = mouse (also sets mouse to stream mode)
//         -1 = no response / BAT failed
//         -2 = unknown device
//         -3 = mouse failed to enter stream mode
```

The C++ `init()` waits 2 full seconds after reset (rather than 200 ms) to accommodate USB-to-PS/2 adapters which are slower to respond.

**Keyboard input:**
```cpp
char ch;
if (ps2.get_kb_ch(&ch)) {
    // ch holds ASCII value of the pressed key
}
```

`get_kb_ch` consumes one or more bytes from the FIFO per call. It handles break codes (`0xF0`) internally — on a key release it reads the following scan code and discards it, so the caller only ever sees make-code characters. Shift state is tracked with a static flag.

The C++ scan code table is more complete than the C driver's — it includes function keys (F1–F12), Caps Lock, Num Lock, Escape, Tab, and backspace, mapping them to private byte values (`0x80`–`0xFB`).

**Mouse input:**
```cpp
int lbtn, rbtn, xmov, ymov;
if (ps2.get_mouse_activity(&lbtn, &rbtn, &xmov, &ymov)) {
    // lbtn/rbtn: 1 = pressed
    // xmov/ymov: signed 9-bit movement delta
}
```

Mouse packets are 3 bytes. The x/y deltas are 9-bit two's complement values — the sign bit is in byte 1 (`b1[4]` for X, `b1[5]` for Y) and is manually sign-extended to a full `int`.

**Low-level FIFO access:**
```cpp
ps2.rx_fifo_empty();   // 1 if no data waiting
ps2.rx_byte();         // read and pop one byte (-1 if empty)
ps2.tx_byte(0xff);     // send command byte to device
ps2.tx_idle();         // 1 if transmitter is free
```

---

### C Driver (`ps2_core.c`) — OS integration

**Initialisation (`ps2_init`):**

1. Flush any stale bytes from the FIFO
2. Send reset command (`0xFF`) to the keyboard
3. Wait 200 ms for the keyboard self-test (BAT) to complete
4. Expect `0xFA` (ACK) followed by `0xAA` (BAT pass)
5. If an extra `0x00` follows, a mouse is connected — return error

**Scan code decoding (`ps2_poll`):**

The PS/2 keyboard uses **scan code set 2**. Each keypress generates a make code; each key release generates a break code prefix `0xF0` followed by the same scan code.

The driver maintains three state flags:
- `shifted` — set when left or right shift is held
- `release` — set when `0xF0` break prefix is seen
- `control` — set when left Ctrl is held

On each call to `ps2_poll`, one byte is dequeued and processed:

```
0xF0 received  →  set release flag, return 0
release set    →  clear modifier if shift/ctrl, clear release flag, return 0
shift key      →  set shifted flag, return 0
ctrl key       →  set control flag, return 0
otherwise      →  look up ASCII in ltab[] or utab[], return character
```

Two lookup tables (`ltab[128]`, `utab[128]`) map scan codes to ASCII — `ltab` for unshifted, `utab` for shifted. Unmapped keys return 0.

**Blocking read (`kgetc`):**

```c
int kgetc(void) {
    int c;
    do {
        c = ps2_poll();
    } while (c <= 0);  // spin on empty FIFO or non-printable
    return c;
}
```

This is a busy-wait spin loop — the PS/2 controller has no interrupt line connected to the GIC, so the CPU cannot block and yield to another process while waiting for a keypress.

---

## Key Files

| File | Description |
|------|-------------|
| `fpga_ip/ps2_controller_1.0/src/ps2_top.sv` | Top-level: wires rx, tx, FIFO together |
| `fpga_ip/ps2_controller_1.0/src/ps2rx.sv` | Receiver FSMD with clock filter |
| `fpga_ip/ps2_controller_1.0/src/ps2tx.sv` | Transmitter (for host-to-device commands) |
| `fpga_ip/ps2_controller_1.0/src/fifo.sv` | Parameterised synchronous FIFO |
| `fpga_ip/ps2_controller_1.0/src/fifo_ctrl.sv` | FIFO read/write pointer control |
| `fpga_ip/ps2_controller_1.0/src/ps2_axi_v1_0.v` | AXI4-Lite slave wrapper |
| `vitis/workspace/zed_vga_test/src/drv/ps2_core.cpp` | C++ driver (Ps2Core — keyboard and mouse) |
| `vitis/workspace/zed_vga_test/src/drv/ps2_core.h` | C++ class declaration, register map |
| `vitis/workspace/zed_os_fpga_app/src/ps2_core.c` | C driver (init, poll, scan code decode) |
| `vitis/workspace/zed_os_fpga_app/src/ps2_core.h` | Base address, register offsets, prototypes |
