#include "xparameters.h"
#include "xuartps.h"
#include "xil_printf.h"
#include "xstatus.h"
#include "xil_exception.h"
#include "xttcps.h"
#include "xscugic.h"

#include "type.h"

#define  SSIZE 1024
#define printf xil_printf
#define kprintf xil_printf

void stub_printf( const char8 *ctrl1, ...)
{
	va_list argp;

	va_start(argp, ctrl1);

	va_end(argp);
}

/* XILINX */
/****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur.
* This function is application specific since the actual system may or may not
* have an interrupt controller.  The TTC could be directly connected to a
* processor without an interrupt controller.  The user should modify this
* function to fit the application.
*
* @param	IntcDeviceID is the unique ID of the interrupt controller
* @param	IntcInstacePtr is a pointer to the interrupt controller
*		instance.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/

/************************** Variable Definitions *****************************/

/************************** Constant Definitions *****************************/
#if defined (PLATFORM_ZYNQ)
#define NUM_DEVICES    9U
#else
#define NUM_DEVICES    12U
#endif

/**************************** Type Definitions *******************************/
typedef struct {
	u32 OutputHz;	/* Output frequency */
	XInterval Interval;	/* Interval value */
	u8 Prescaler;	/* Prescaler value */
	u16 Options;	/* Option settings */
} TmrCntrSetup;

#define	TICK_TIMER_FREQ_HZ	100  /* Tick timer counter's output frequency */
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define TTC_TICK_INTR_ID	XPAR_XTTCPS_1_INTR
#define TTC_TICK_DEVICE_ID	XPAR_XTTCPS_1_DEVICE_ID

#define UART_DEVICE_ID		XPAR_XUARTPS_0_DEVICE_ID
#define INTC_DEVICE_ID		XPAR_SCUGIC_SINGLE_DEVICE_ID
#define UART_INT_IRQ_ID		XPAR_XUARTPS_1_INTR

static XTtcPs TtcPsInst[NUM_DEVICES];	/* Number of available timer counters */

static TmrCntrSetup SettingsTable[NUM_DEVICES] = {
	{200, 0, 0, 0}, /* PWM timer counter initial setup, only output freq */
	{1, 0, 0, 0},	/* Ticker timer counter initial setup, only output freq */
};

static volatile u8 ErrorCount;		/* Errors seen at interrupt time */
static volatile u32 TickCount;		/* Ticker interrupts between PWM change */

XScuGic InterruptController;  /* Interrupt controller instance */

static int SetupInterruptSystem(u16 IntcDeviceID,
				    XScuGic *IntcInstancePtr)
{
	int Status;
	XScuGic_Config *IntcConfig; /* The configuration parameters of the
					interrupt controller */

	/*
	 * Initialize the interrupt controller driver
	 */
	IntcConfig = XScuGic_LookupConfig(IntcDeviceID);
	if (NULL == IntcConfig) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig,
					IntcConfig->CpuBaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the ARM processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler) XScuGic_InterruptHandler,
			IntcInstancePtr);

	/*
	 * Enable interrupts in the ARM
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

/***************************************************************************/
/**
*
* This function is the handler which handles the periodic tick interrupt.
* It updates its count, and set a flag to signal PWM timer counter to
* update its duty cycle.
*
* This handler provides an example of how to handle data for the TTC and
* is application specific.
*
* @param	CallBackRef contains a callback reference from the driver, in
*		this case it is the instance pointer for the TTC driver.
*
* @return	None.
*
* @note		None.
*
*****************************************************************************/
static void TickHandler(void *CallBackRef, u32 StatusEvent)
{
	PROC *temp =  0;
      PROC *p;
	if (0 != (XTTCPS_IXR_INTERVAL_MASK & StatusEvent)) {
		TickCount++;

      while ( (p = dequeue(&pauseList)) ){
        p->pause--;
        if (p->pause == 0){
          p->status = READY;
          enqueue(&readyQueue, p); // Wake up ready process
        }
        else{
          enqueue(&temp, p); // Requeue if not ready
        }
      }
      pauseList = temp; // Update pause list
	}
	else {
		/*
		 * The Interval event should be the only one enabled. If it is
		 * not it is an error
		 */
		ErrorCount++;
	}
}

/****************************************************************************/
/**
*
* This function sets up a timer counter device, using the information in its
* setup structure.
*  . initialize device
*  . set options
*  . set interval and prescaler value for given output frequency.
*
* @param	DeviceID is the unique ID for the device.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/
int SetupTimer(int DeviceID)
{
	int Status;
	XTtcPs_Config *Config;
	XTtcPs *Timer;
	TmrCntrSetup *TimerSetup;

	TimerSetup = &SettingsTable[DeviceID];

	Timer = &(TtcPsInst[DeviceID]);

	/*
	 * Look up the configuration based on the device identifier
	 */
	Config = XTtcPs_LookupConfig(DeviceID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	/*
	 * Initialize the device
	 */
	Status = XTtcPs_CfgInitialize(Timer, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set the options
	 */
	XTtcPs_SetOptions(Timer, TimerSetup->Options);

	/*
	 * Timer frequency is preset in the TimerSetup structure,
	 * however, the value is not reflected in its other fields, such as
	 * IntervalValue and PrescalerValue. The following call will map the
	 * frequency to the interval and prescaler values.
	 */
	XTtcPs_CalcIntervalFromFreq(Timer, TimerSetup->OutputHz,
		&(TimerSetup->Interval), &(TimerSetup->Prescaler));

	/*
	 * Set the interval and prescale
	 */
	XTtcPs_SetInterval(Timer, TimerSetup->Interval);
	XTtcPs_SetPrescaler(Timer, TimerSetup->Prescaler);

	return XST_SUCCESS;
}

/****************************************************************************/
/**
*
* This function sets up the Ticker timer.
*
* @param	None
*
* @return	XST_SUCCESS if everything sets up well, XST_FAILURE otherwise.
*
* @note		None
*
*****************************************************************************/
int SetupTicker(void)
{
	int Status;
	TmrCntrSetup *TimerSetup;
	XTtcPs *TtcPsTick;

	TimerSetup = &(SettingsTable[TTC_TICK_DEVICE_ID]);

	/*
	 * Set up appropriate options for Ticker: interval mode without
	 * waveform output.
	 */
	TimerSetup->Options |= (XTTCPS_OPTION_INTERVAL_MODE |
					      XTTCPS_OPTION_WAVE_DISABLE);

	/*
	 * Calling the timer setup routine
	 *  . initialize device
	 *  . set options
	 */
	Status = SetupTimer(TTC_TICK_DEVICE_ID);
	if(Status != XST_SUCCESS) {
		return Status;
	}

	TtcPsTick = &(TtcPsInst[TTC_TICK_DEVICE_ID]);

	/*
	 * Connect to the interrupt controller
	 */
	Status = XScuGic_Connect(&InterruptController, TTC_TICK_INTR_ID,
		(Xil_ExceptionHandler)XTtcPs_InterruptHandler, (void *)TtcPsTick);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	XTtcPs_SetStatusHandler(&(TtcPsInst[TTC_TICK_DEVICE_ID]), &(TtcPsInst[TTC_TICK_DEVICE_ID]),
		              (XTtcPs_StatusHandler)TickHandler);

	/*
	 * Enable the interrupt for the Timer counter
	 */
	XScuGic_Enable(&InterruptController, TTC_TICK_INTR_ID);

	/*
	 * Enable the interrupts for the tick timer/counter
	 * We only care about the interval timeout.
	 */
	XTtcPs_EnableInterrupts(TtcPsTick, XTTCPS_IXR_INTERVAL_MASK);

	/*
	 * Start the tick timer/counter
	 */
	XTtcPs_Start(TtcPsTick);

	return Status;
}



/* END XILINX */

extern PROC *getproc();


char kgetc(){
	return (char)XUartPs_RecvByte(STDOUT_BASEADDRESS);
}
//int kprintf(char *fmt, ...);

// Pause a process for t seconds using the timer
int pause(int t) {
  running->status = PAUSE;
  running->pause = t;
  enterList(&pauseList, running);
  tswitch();
}

XUartPs Uart_Ps;		/* The instance of the UART Driver */
#define UART_DEVICE_ID                  XPAR_XUARTPS_0_DEVICE_ID

volatile int TotalReceivedCount;
volatile int TotalSentCount;
int TotalErrorCount;

#define SBUFSIZE 128

typedef struct uart{
  char inbuf[SBUFSIZE+20];
  int  inhead, intail;
  struct semaphore indata, uline;
  char outbuf[SBUFSIZE+20];
  int  outhead, outtail;
  struct semaphore outroom;
  int txon; // 1=TX interrupt is on
}UART;

UART uart_buffer;

u8 u_buffer[100];

void Handler(void *CallBackRef, u32 Event, unsigned int EventData);

int uart_init()
{
	XUartPs_Config *Config;
	int Status;
	/*
	 * Initialize the UART driver so that it's ready to use
	 * Look up the configuration in the config table and then initialize it.
	 */
	Config = XUartPs_LookupConfig(UART_DEVICE_ID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}



	Status = XScuGic_Connect(&InterruptController, UART_INT_IRQ_ID,
				  (Xil_ExceptionHandler) XUartPs_InterruptHandler,
				  (void *) &Uart_Ps);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Enable the interrupt for the device */
	XScuGic_Enable(&InterruptController, UART_INT_IRQ_ID);

	XUartPs_SetHandler(&Uart_Ps, (XUartPs_Handler)Handler, &Uart_Ps);

  u32   IntrMask =
  XUARTPS_IXR_TOUT | XUARTPS_IXR_PARITY | XUARTPS_IXR_FRAMING |
  XUARTPS_IXR_OVER  | XUARTPS_IXR_RXFULL |
  XUARTPS_IXR_RXOVR;

  XUartPs_SetInterruptMask(&Uart_Ps, IntrMask);

	/*
	 * Enable the interrupt for the Timer counter
	 */
	/* Buffer initialization */
	UART *up = &uart_buffer;
    up->inhead = up->intail = 0;
    up->indata.value = 0; up->indata.queue = 0;
    up->uline.value = 0;  up->uline.queue = 0;

    up->outhead = up->outtail = 0;
    up->outroom.value = SBUFSIZE; up->outroom.queue = 0;

    up->txon = 0;

	XScuGic_Enable(&InterruptController, UART_INT_IRQ_ID);
	XUartPs_SetRecvTimeout(&Uart_Ps, 8);
}

void uputc(char*s){
	outbyte(*s);
}

char ugetc(UART *up)
{
  char c;
  //  while(up->indata <= 0); // loop until up->data > 0
  // replace while loop with sleep
  P(&up->indata);
  //lock();
    c = up->inbuf[up->intail++];
    up->intail %= SBUFSIZE;
    //unlock();
  return c;
}

int ugets(char *s)
{
  //kprintf("in ugets() of UART%d", up->n);
  while ((*s = (char)ugetc(&uart_buffer)) != '\r'){
    uputc(*s);
    s++;
  }
 *s = 0;
}

int do_rx(UART *up)
{
  char c;
  // do we need this?
  // while(!(*(up->base + UFR) & 0x40));
 // while(XUartPs_RecvByte(&Uart_Ps, u_buffer, 1)!=0)
  //{

	  up->inbuf[up->inhead++] = u_buffer[0];
	  up->inhead %= SBUFSIZE;

	  V(&up->indata);
	   XUartPs_Recv(&Uart_Ps, u_buffer, 1);
	  if (c=='\r'){
		V(&up->uline);
	  }

  //}

}

uint32_t last_index = 0;
void Handler(void *CallBackRef, u32 Event, unsigned int EventData)
{
	uint32_t single_line = 0;
	/* All of the data has been sent */
	if (Event == XUARTPS_EVENT_SENT_DATA) {
		TotalSentCount = EventData;
	}

	/* All of the data has been received */
	if (Event == XUARTPS_EVENT_RECV_DATA) {
		for(int i = uart_buffer.inhead; i<uart_buffer.inhead + EventData; i++){

				  if (uart_buffer.inbuf[i]=='\r' && single_line==0){
					V_int(&(uart_buffer.uline));
					single_line++;
				  }
		}
		V_int(&(uart_buffer.indata));
		uart_buffer.inhead += EventData;
		uart_buffer.inhead %= SBUFSIZE;
		XUartPs_Recv(&Uart_Ps, &uart_buffer.inbuf[uart_buffer.inhead], 1u);
	}

	/*
	 * Data was received, but not the expected number of bytes, a
	 * timeout just indicates the data stopped for 8 character times
	 */
	if (Event == XUARTPS_EVENT_RECV_TOUT) {
		if(EventData!=0){
		for(int i = uart_buffer.inhead; i<uart_buffer.inhead + EventData; i++){

				  if (uart_buffer.inbuf[i]=='\r' && single_line==0){
					V_int(&(uart_buffer.uline));
					single_line++;
				  }
		}
		V_int(&(uart_buffer.indata));
		uart_buffer.inhead += EventData;
		uart_buffer.inhead %= SBUFSIZE;
		XUartPs_Recv(&Uart_Ps, &uart_buffer.inbuf[uart_buffer.inhead], 1u);
		}
	}

	/*
	 * Data was received with an error, keep the data but determine
	 * what kind of errors occurred
	 */
	if (Event == XUARTPS_EVENT_RECV_ERROR) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}

	/*
	 * Data was received with an parity or frame or break error, keep the data
	 * but determine what kind of errors occurred. Specific to Zynq Ultrascale+
	 * MP.
	 */
	if (Event == XUARTPS_EVENT_PARE_FRAME_BRKE) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}

	/*
	 * Data was received with an overrun error, keep the data but determine
	 * what kind of errors occurred. Specific to Zynq Ultrascale+ MP.
	 */
	if (Event == XUARTPS_EVENT_RECV_ORERR) {
		TotalReceivedCount = EventData;
		TotalErrorCount++;
	}
}



#define PRSIZE 2  // Size of producer-consumer buffer
char prbuf[PRSIZE]; // Shared circular buffer
int head, tail;     // Head/tail indexes
struct semaphore full, empty, mutex; // Synchronization primitives

// Two timer tasks showing different pause durations
int timer1_task() {
  int t = 5;
  printf("timer1_task %d start\n", running->pid);
  while(1){
    pause(t);
    printf("proc%d run once in %d seconds\n", running->pid, t);
  }
}

int timer2_task() {
  int t = 7;
  printf("timer2_task %d start\n", running->pid);
  while(1){
    pause(t);
    printf("proc%d run once in %d seconds\n", running->pid, t);
  }
}

// Producer process: produces characters from UART input
int producer() {
  char c, *cp; 

  char line[128];

  while(1){
    ugets(line); // Read line from UART

    cp = line;
    while(*cp){
      P(&empty);     // Wait for empty slot
      P(&mutex);     // Enter critical section
        prbuf[head++] = *cp++;
        head %= PRSIZE; // Circular buffer wrap
      V(&mutex);     // Exit critical section
      kprintf("Producer %d V(full=%d) ", running->pid, full.value);
      if (full.value<0) {
         kprintf("full.queue="); 
         printlist(full.queue);
      } else {
        printf("\n");
      }
      V(&full);      // Signal data availability
    }
  }
}

// Consumer process: consumes characters from the buffer
int consumer() {
  char c; 
  while(1){
    P(&full);     // Wait for available data
    P(&mutex);    // Enter critical section
      c = prbuf[tail++];
      tail %= PRSIZE;  // Circular buffer wrap
    V(&mutex);    // Exit critical section
    kprintf("Consumer %d V(empty=%d) ", running->pid, empty.value);
    if (empty.value<0) {
       kprintf("empty.queue="); 
       printlist(empty.queue);
    } else {
      printf("\n");
    }
    kprintf("c = %c\n", c);
    V(&empty);   // Signal empty slot

  }
}
// Entry point
int main()
{
	//TODO refactor this code
	int Status;

	/*
	 * Make sure the interrupts are disabled, in case this is being run
	 * again after a failure.
	 */

	/*
	 * Connect the Intc to the interrupt subsystem such that interrupts can
	 * occur. This function is application specific.
	 */
	Status = SetupInterruptSystem(INTC_DEVICE_ID, &InterruptController);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Set up the Ticker timer
	 */
	Status = SetupTicker();
	if (Status != XST_SUCCESS) {
		return Status;
	}


   uart_init();
   XUartPs_Recv(&Uart_Ps, u_buffer, 1);
   printf("Welcome to WANIX in Arm\n");
   // Initialize semaphores for producer-consumer
   head = tail = 0;
   full.value = 0;       full.queue = 0;
   empty.value = PRSIZE; empty.queue = 0;
   mutex.value = 1;      mutex.queue = 0;

   init();

   kprintf("P0 kfork tasks\n");
   kfork((int)timer1_task, 1);
   kfork((int)timer2_task, 1);
   kfork((int)producer,   2);
   kfork((int)consumer,   2);
   kfork((int)producer,   3);
   kfork((int)consumer,   3);
   printQ(readyQueue);
   uint32_t count = 0;
   uint32_t recv;
   recv = XUartPs_Recv(&Uart_Ps, uart_buffer.inbuf, 1u);
   printf("enter line from UART0 to activate producer/consumer tasks\n");
   unlock();
   while(1){
     if (readyQueue)
        tswitch();
   }
}
