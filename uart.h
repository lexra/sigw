#ifndef __UART__
#define __UART__

//#define TTY_SERIAL                    "/dev/ttyUSB0"
#define TTY_SERIAL                    "/dev/ttyS1"

void tell_uart_thread_quit(void);
void *uart_thread(void *param);


#endif // 