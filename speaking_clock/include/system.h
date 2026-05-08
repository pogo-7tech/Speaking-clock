/* ============================================================
 * system.h
 * ------------------------------------------------------------
 * Project-wide declarations:
 *   - system / peripheral / network init hooks
 *   - shared RTOS handles (semaphore + queue)
 *   - minimal UART I/O the firmware uses as its "console"
 * ============================================================ */

#ifndef SYSTEM_H
#define SYSTEM_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

/* ---- Init hooks implemented in main.c / network / BSP ---- */
void system_init(void);       /* clocks, UART, NVIC                */
void network_init(void);      /* lwIP core + netif + DHCP start    */

/* ---- Shared RTOS objects --------------------------------- */
extern SemaphoreHandle_t time_request_sem;   /* key  -> ntp   */
extern QueueHandle_t     time_queue;         /* ntp  -> speech */

/* ---- UART bsp ------------------------------------------- */
void     uart_init(void);
void     uart_putc(char c);
int      uart_getc_blocking(void);   /* blocks current task   */
void     uart_puts(const char *s);
void     uart_printf(const char *fmt, ...);

/* ---- NTP client ----------------------------------------- */
#include "time_msg.h"
int ntp_get_time(time_msg_t *out);   /* 0 on success, -1 on fail */

#endif /* SYSTEM_H */
