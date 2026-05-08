/* ============================================================
 * bsp.c  -  UART driver for MPS2-AN385 (CMSDK UART0)
 * ------------------------------------------------------------
 * This file handles the low-level hardware abstraction for the 
 * serial port. Because the stock QEMU didn't emulate the Ethernet 
 * peripheral on the standard STM32 boards, I had to port the 
 * project to the MPS2 board, which uses the CMSDK UART instead 
 * of the STM32 USART.
 *
 * The CMSDK UART is super simple. It lives at memory address 
 * 0x40004000 and just has a few registers for data and status.
 * I wrote these basic wrapper functions so the rest of the code 
 * doesn't need to care about the underlying hardware differences.
 * ============================================================ */
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "system.h"

/* ------------------------------------------------------------
 * Hardware Register Definitions
 * ------------------------------------------------------------
 * These are the hardcoded memory addresses for the UART registers 
 * according to the Cortex-M System Design Kit documentation. 
 * I cast them to volatile pointers so the compiler knows not to 
 * optimize away reads and writes to them.
 * ---------------------------------------------------------- */
#define UART0_BASE    0x40004000UL
#define UART_DATA    (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_STATE   (*(volatile uint32_t *)(UART0_BASE + 0x04))
#define UART_CTRL    (*(volatile uint32_t *)(UART0_BASE + 0x08))
#define UART_BAUDDIV (*(volatile uint32_t *)(UART0_BASE + 0x10))

/* Bitmasks for the status and control registers */
#define UART_STATE_TXFULL  (1U << 0) /* Transmit buffer full flag */
#define UART_STATE_RXFULL  (1U << 1) /* Receive buffer full flag */
#define UART_CTRL_TXEN     (1U << 0) /* Transmit enable bit */
#define UART_CTRL_RXEN     (1U << 1) /* Receive enable bit */

/* ------------------------------------------------------------
 * uart_init
 * ------------------------------------------------------------
 * Sets up the UART hardware before we can use it.
 * ---------------------------------------------------------- */
void uart_init(void)
{
    /* Set a generic baud rate divider. In QEMU, the baud rate 
     * doesn't actually matter much, but the hardware still 
     * expects this register to be set to a non-zero value. */
    UART_BAUDDIV = 16;
    
    /* Turn on both the transmitter and the receiver */
    UART_CTRL    = UART_CTRL_TXEN | UART_CTRL_RXEN;
}

/* ------------------------------------------------------------
 * uart_putc
 * ------------------------------------------------------------
 * Sends a single character out the serial port.
 * ---------------------------------------------------------- */
void uart_putc(char c)
{
    /* Spin-wait until the hardware transmit buffer has room. 
     * This is a blocking loop, but UART is so fast in QEMU that 
     * it rarely blocks for more than a fraction of a millisecond. */
    while (UART_STATE & UART_STATE_TXFULL) { }
    
    /* Shove the character into the data register to transmit it */
    UART_DATA = (uint32_t)(uint8_t)c;
}

/* ------------------------------------------------------------
 * uart_puts
 * ------------------------------------------------------------
 * Helper function to print an entire null-terminated string.
 * ---------------------------------------------------------- */
void uart_puts(const char *s) { 
    /* Loop until we hit the null terminator at the end of the string */
    while (*s) {
        uart_putc(*s++); 
    }
}

/* ------------------------------------------------------------
 * uart_printf
 * ------------------------------------------------------------
 * A miniature version of printf that outputs over the UART.
 * ---------------------------------------------------------- */
void uart_printf(const char *fmt, ...)
{
    /* 128 bytes should be plenty for our debug messages */
    char buf[128];
    
    /* Standard C variable argument list handling */
    va_list ap;
    va_start(ap, fmt);
    
    /* Format the string into our temporary buffer */
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    
    /* Output the formatted string */
    uart_puts(buf);
}

/* ------------------------------------------------------------
 * uart_getc_blocking
 * ------------------------------------------------------------
 * Waits for a character to be received and returns it.
 * ---------------------------------------------------------- */
int uart_getc_blocking(void)
{
    /* Instead of busy-waiting and burning 100% CPU, I used FreeRTOS's 
     * vTaskDelay to put the calling task to sleep for 10ms at a time 
     * while we wait for the user to press a key. This is much friendlier 
     * to the RTOS scheduler. */
    while (!(UART_STATE & UART_STATE_RXFULL)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    /* Grab the data from the register and mask off just the bottom 8 bits */
    return (int)(UART_DATA & 0xFF);
}

/* ------------------------------------------------------------
 * system_init
 * ------------------------------------------------------------
 * The master initialization hook called from main().
 * ---------------------------------------------------------- */
void system_init(void) { 
    /* For now, just initialize the UART. If we were on real hardware, 
     * we'd also initialize the clock tree, flash wait states, etc. here. */
    uart_init(); 
}
