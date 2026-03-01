// kbd_task.c
// Keyboard input task: polls the PS/2 RX FIFO and prints characters to the
// VGA screen. Yields the CPU (tswitch) when no key is available so other
// tasks can run.

#include "type.h"
#include "ps2_core.h"
#include "vga_core.h"

extern PROC *running;
extern PROC *readyQueue;

// Event token used with ksleep/kwakeup — arbitrary unique value.
#define KBD_EVENT  0xBD

void kbd_task(void)
{
    int c;

    ps2_init();
    kprintf("kbd_task %d started\n", running->pid);

    while (1) {
        c = ps2_poll();

        if (c <= 0) {
            // No key available — yield so other tasks can run.
            tswitch();
            continue;
        }

        // Echo the character to the VGA screen.
        kputc((char)c);
    }
}