#include "xil_exception.h"
#include "xil_mmu.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xscugic.h"
#include "xstatus.h"
#include "xttcps.h"
#include "xuartps.h"

#include "type.h"

// ---------------------------------------------------------------------------
// Timer constants
// ---------------------------------------------------------------------------
#if defined(PLATFORM_ZYNQ)
enum { NUM_DEVICES = 9U };
#else
enum { NUM_DEVICES = 12U };
#endif

#define TICK_TIMER_FREQ_HZ 100
#define TTC_TICK_DEVICE_ID XPAR_XTTCPS_1_DEVICE_ID
#define TTC_TICK_INTR_ID   XPAR_XTTCPS_1_INTR

// ---------------------------------------------------------------------------
// UART constants
// ---------------------------------------------------------------------------
#define UART_DEVICE_ID  XPAR_XUARTPS_0_DEVICE_ID
#define UART_INT_IRQ_ID XPAR_XUARTPS_1_INTR

enum { SBUFSIZE = 128 };

// ---------------------------------------------------------------------------
// Producer-consumer buffer
// ---------------------------------------------------------------------------
enum { PRSIZE = 2 };

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef struct {
  u32 OutputHz;
  XInterval Interval;
  u8 Prescaler;
  u16 Options;
} TmrCntrSetup;

typedef struct uart {
  char inbuf[SBUFSIZE + 20];
  int inhead, intail;
  struct semaphore indata, uline;
  char outbuf[SBUFSIZE + 20];
  int outhead, outtail;
  struct semaphore outroom;
  int txon;
} UART;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static XTtcPs TtcPsInst[NUM_DEVICES];

static TmrCntrSetup SettingsTable[NUM_DEVICES] = {
    {200, 0, 0, 0}, // PWM timer counter
    {1, 0, 0, 0},   // Tick timer counter
};

static volatile u8 ErrorCount;
static volatile u32 TickCount;

XScuGic InterruptController;
XUartPs Uart_Ps;
UART uart_buffer;
u8 u_buffer[100];

volatile int TotalReceivedCount;
volatile int TotalSentCount;
int TotalErrorCount;

char prbuf[PRSIZE];
int head, tail;
struct semaphore full, empty, mutex;

extern PROC *running;
extern PROC *readyQueue;
extern PROC *pauseList;
extern int color;
extern PROC *getproc();
extern void kbd_task();

// ---------------------------------------------------------------------------
// Interrupt system
// ---------------------------------------------------------------------------
static int SetupInterruptSystem(u16 IntcDeviceID, XScuGic *IntcInstancePtr) {
  int Status;
  XScuGic_Config *IntcConfig;

  IntcConfig = XScuGic_LookupConfig(IntcDeviceID);
  if (NULL == IntcConfig)
    return XST_FAILURE;

  Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
                                 IntcConfig->CpuBaseAddress);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                               (Xil_ExceptionHandler)XScuGic_InterruptHandler,
                               IntcInstancePtr);
  Xil_ExceptionEnable();
  return XST_SUCCESS;
}

// ---------------------------------------------------------------------------
// Tick handler
// ---------------------------------------------------------------------------
static void TickHandler(void *CallBackRef, u32 StatusEvent) {
  PROC *temp = 0;
  PROC *p;

  if (!(XTTCPS_IXR_INTERVAL_MASK & StatusEvent)) {
    ErrorCount++;
    return;
  }

  TickCount++;
  while ((p = dequeue(&pauseList))) {
    p->pause--;
    if (p->pause == 0) {
      p->status = READY;
      enqueue(&readyQueue, p);
    } else {
      enqueue(&temp, p);
    }
  }
  pauseList = temp;
}

// ---------------------------------------------------------------------------
// Timer setup
// ---------------------------------------------------------------------------
int SetupTimer(int DeviceID) {
  int Status;
  XTtcPs_Config *Config;
  XTtcPs *Timer = &TtcPsInst[DeviceID];
  TmrCntrSetup *TimerSetup = &SettingsTable[DeviceID];

  Config = XTtcPs_LookupConfig(DeviceID);
  if (NULL == Config)
    return XST_FAILURE;

  Status = XTtcPs_CfgInitialize(Timer, Config, Config->BaseAddress);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  XTtcPs_SetOptions(Timer, TimerSetup->Options);
  XTtcPs_CalcIntervalFromFreq(Timer, TimerSetup->OutputHz,
                              &TimerSetup->Interval, &TimerSetup->Prescaler);
  XTtcPs_SetInterval(Timer, TimerSetup->Interval);
  XTtcPs_SetPrescaler(Timer, TimerSetup->Prescaler);
  return XST_SUCCESS;
}

int SetupTicker(void) {
  int Status;
  XTtcPs *TtcPsTick;
  TmrCntrSetup *TimerSetup = &SettingsTable[TTC_TICK_DEVICE_ID];

  TimerSetup->Options |=
      (XTTCPS_OPTION_INTERVAL_MODE | XTTCPS_OPTION_WAVE_DISABLE);

  Status = SetupTimer(TTC_TICK_DEVICE_ID);
  if (Status != XST_SUCCESS)
    return Status;

  TtcPsTick = &TtcPsInst[TTC_TICK_DEVICE_ID];

  Status =
      XScuGic_Connect(&InterruptController, TTC_TICK_INTR_ID,
                      (Xil_ExceptionHandler)XTtcPs_InterruptHandler, TtcPsTick);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  XTtcPs_SetStatusHandler(&TtcPsInst[TTC_TICK_DEVICE_ID],
                          &TtcPsInst[TTC_TICK_DEVICE_ID],
                          (XTtcPs_StatusHandler)TickHandler);

  XScuGic_Enable(&InterruptController, TTC_TICK_INTR_ID);
  XTtcPs_EnableInterrupts(TtcPsTick, XTTCPS_IXR_INTERVAL_MASK);
  XTtcPs_Start(TtcPsTick);
  return Status;
}

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------
void Handler(void *CallBackRef, u32 Event, unsigned int EventData);

int uart_init() {
  int Status;
  XUartPs_Config *Config;

  Config = XUartPs_LookupConfig(UART_DEVICE_ID);
  if (NULL == Config)
    return XST_FAILURE;

  Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  Status =
      XScuGic_Connect(&InterruptController, UART_INT_IRQ_ID,
                      (Xil_ExceptionHandler)XUartPs_InterruptHandler, &Uart_Ps);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  XUartPs_SetHandler(&Uart_Ps, (XUartPs_Handler)Handler, &Uart_Ps);

  u32 IntrMask = XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING |
                 XUARTPS_IXR_OVER | XUARTPS_IXR_RXFULL | XUARTPS_IXR_RXOVR;
  XUartPs_SetInterruptMask(&Uart_Ps, IntrMask);

  UART *up = &uart_buffer;
  up->inhead = up->intail = 0;
  up->indata.value = 0;
  up->indata.queue = 0;
  up->uline.value = 0;
  up->uline.queue = 0;
  up->outhead = up->outtail = 0;
  up->outroom.value = SBUFSIZE;
  up->outroom.queue = 0;
  up->txon = 0;

  XUartPs_SetFifoThreshold(&Uart_Ps, 1);
  XScuGic_Enable(&InterruptController, UART_INT_IRQ_ID);
  XUartPs_SetRecvTimeout(&Uart_Ps, 8);
  return XST_SUCCESS;
}

void uputc(char c) { outbyte(c); }

char ugetc(UART *up) {
  char c;
  P(&up->indata);
  c = up->inbuf[up->intail++];
  up->intail %= SBUFSIZE;
  return c;
}

int ugets(char *s) {
  char c;
  char *p = s;
  while ((c = ugetc(&uart_buffer)) != '\r') {
    if (c == '\b' || c == 0x7f) {
      if (p > s) {
        p--;
        uputc('\b');
        uputc(' ');
        uputc('\b');
      }
    } else {
      *p++ = c;
      uputc(c);
    }
  }
  *p = 0;
  uputc('\r');
  return p - s;
}

void Handler(void *CallBackRef, u32 Event, unsigned int EventData) {
  uint32_t single_line = 0;

  if (Event == XUARTPS_EVENT_SENT_DATA) {
    TotalSentCount = EventData;
  }

  if (Event == XUARTPS_EVENT_RECV_DATA || Event == XUARTPS_EVENT_RECV_TOUT) {
    if (EventData != 0) {
      for (unsigned int i = 0; i < EventData; i++) {
        int idx = (uart_buffer.inhead + i) % SBUFSIZE;
        if (uart_buffer.inbuf[idx] == '\r' && single_line == 0) {
          V_int(&uart_buffer.uline);
          single_line++;
        }
        V_int(&uart_buffer.indata);
      }
      uart_buffer.inhead = (uart_buffer.inhead + EventData) % SBUFSIZE;
      XUartPs_Recv(&Uart_Ps, &uart_buffer.inbuf[uart_buffer.inhead], 1u);
    }
  }

  if (Event == XUARTPS_EVENT_RECV_ERROR ||
      Event == XUARTPS_EVENT_PARE_FRAME_BRKE ||
      Event == XUARTPS_EVENT_RECV_ORERR) {
    TotalReceivedCount = EventData;
    TotalErrorCount++;
  }
}

// ---------------------------------------------------------------------------
// OS tasks
// ---------------------------------------------------------------------------
int pause(int t) {
  running->status = PAUSE;
  running->pause = t;
  enterList(&pauseList, running);
  tswitch();
}

int delayed1_task() {
  int t = 5;
  printf("delayed1_task %d start\n", running->pid);
  while (1) {
    pause(t);
    color = CYAN;
    printf("proc%d run once in %d seconds\n", running->pid, t);
  }
}

int delayed2_task() {
  int t = 7;
  printf("delayed2_task %d start\n", running->pid);
  while (1) {
    pause(t);
    color = RED;
    printf("proc%d run once in %d seconds\n", running->pid, t);
  }
}

int producer() {
  char *cp;
  char line[128];
  int oldcolor;
  color = YELLOW;
  while (1) {
    ugets(line);
    cp = line;
    while (*cp) {
      P(&empty);
      P(&mutex);
      prbuf[head++] = *cp++;
      head %= PRSIZE;
      V(&mutex);
      kprintf("Producer %d V(full=%d) ", running->pid, full.value);
      if (full.value < 0) {
        kprintf("full.queue=");
        printlist(full.queue);
      } else {
        printf("\n");
      }
      V(&full);
    }
    color = oldcolor;
  }
}

int consumer() {
  int oldcolor;
  char c;
  while (1) {
	oldcolor=color;
	color = GREEN;
    P(&full);
    P(&mutex);
    c = prbuf[tail++];
    tail %= PRSIZE;
    V(&mutex);
    kprintf("Consumer %d V(empty=%d) ", running->pid, empty.value);
    if (empty.value < 0) {
      kprintf("empty.queue=");
      printlist(empty.queue);
    } else {
      printf("\n");
    }
    kprintf("c = %c\n", c);
    V(&empty);
    color = oldcolor;
  }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
  int Status;

  // Mark the DMA frame buffer region as strongly-ordered (non-cacheable,
  // non-bufferable). Xil_SetTlbAttributes covers 1 MB per call (Cortex-A9
  // first-level MMU sections). The frame buffer is 4 MB (0x1FB00000-0x1FEFFFFF).
  // This ensures CPU pixel writes reach DDR before the DMA engine fetches them.
  // Region must match VIDEO_FRAME_BASE / VIDEO_FB in vga_core.h and lscript.ld.
  Xil_SetTlbAttributes(0x1FB00000U, NORM_NONCACHE);
  Xil_SetTlbAttributes(0x1FC00000U, NORM_NONCACHE);
  Xil_SetTlbAttributes(0x1FD00000U, NORM_NONCACHE);
  Xil_SetTlbAttributes(0x1FE00000U, NORM_NONCACHE);

  color = WHITE;
  fbuf_init();

  Status = SetupInterruptSystem(XPAR_SCUGIC_SINGLE_DEVICE_ID, &InterruptController);
  if (Status != XST_SUCCESS)
    return XST_FAILURE;

  Status = SetupTicker();
  if (Status != XST_SUCCESS)
    return Status;

  /* -----------------------------------------------------------------------
   * Boot banner — drawn once at the top of the framebuffer console.
   * ASCII art is 7 lines; one blank line follows so shell output starts
   * at row 8 (out of 45), leaving the logo permanently visible above.
   * ----------------------------------------------------------------------- */
  color = CYAN;
  kprintf(" ______          _  ___  ____\n");
  kprintf("|___  /  ___  __| |/ _ \\/ ___|\n");
  kprintf("   / /  / _ \\/ _` | | | \\___ \\\n");
  kprintf("  / /__|  __/ (_| | |_| |___) |\n");
  kprintf(" /_____|\\___|\\__,_|\\___/|____/\n");
  color = WHITE;
  kprintf(" Zynq Embedded OS  |  Zedboard  |  1280x720 DMA VGA\n");
  color = CYAN;
  kprintf(" --------------------------------------------------------\n");
  color = WHITE;
  kprintf("\n");
  uart_init();

  head = tail = 0;
  full.value = 0;  full.queue = 0;
  empty.value = PRSIZE; empty.queue = 0;
  mutex.value = 1; mutex.queue = 0;

  init();

  kprintf("P0 kfork tasks\n");
  kfork((int)delayed1_task, 1);
  kfork((int)delayed2_task, 1);
  kfork((int)producer, 2);
  kfork((int)consumer, 2);
  kfork((int)producer, 3);
  kfork((int)consumer, 3);
  kfork((int)kbd_task, 1);

  printQ(readyQueue);
  XUartPs_Recv(&Uart_Ps, uart_buffer.inbuf, 1u);
  color = YELLOW;
  printf("enter line from UART0 to activate producer/consumer tasks\n");
  unlock();
  while (1) {
    if (readyQueue)
      tswitch();
  }
}
