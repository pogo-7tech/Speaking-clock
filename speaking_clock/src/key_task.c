/* ============================================================
 * key_task.c
 * ------------------------------------------------------------
 * This task acts as the user interface for our speaking clock.
 * It waits for the user to press 't' on the QEMU console, and
 * when they do, it signals the NTP client task to go fetch
 * the current time.
 *
 * I used a binary semaphore (time_request_sem) for the IPC here
 * because it's the perfect fit: we just need to send a simple
 * "go!" signal, we don't need to pass any data.
 *
 * One important design choice: I used uart_getc_blocking() 
 * which puts this task into the Blocked state until a byte 
 * actually arrives on the USART2 RX line. This is much better
 * than busy-waiting in a loop, since it consumes zero CPU 
 * cycles while sitting idle waiting for the user.
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "system.h"

/* ------------------------------------------------------------
 * key_task
 * ------------------------------------------------------------
 * The main loop for monitoring keyboard input.
 * ---------------------------------------------------------- */
void key_task(void *pv)
{
    /* Suppress the unused parameter warning from the compiler.
     * We have to have the void* parameter because FreeRTOS 
     * requires this exact function signature for tasks. */
    (void)pv;

    /* Print a helpful prompt so the user knows what to do
     * once the system finishes booting. */
    uart_puts("\r\n[KEY ] Press 't' to announce the time.\r\n");

    /* Main task loop - runs forever */
    for (;;)
    {
        /* Block here until the user presses a key on the console.
         * The RTOS will schedule other tasks while we wait. */
        int c = uart_getc_blocking();
        
        /* Just in case we got a read error, loop back and try again */
        if (c < 0) { continue; }

        /* A little bit of user-friendliness: I convert uppercase 'T'
         * to lowercase 't' so it works even if caps lock is on. */
        if (c >= 'A' && c <= 'Z') { c += ('a' - 'A'); }

        /* Figure out what to do based on the key pressed */
        switch (c)
        {
        case 't':
            /* The user pressed 't', so we log it to the console */
            uart_puts("[KEY ] 't' pressed -> requesting NTP time...\r\n");
            
            /* Give the semaphore to wake up the NTP task!
             * If the user spams the 't' key and a request is already
             * in flight, the extra xSemaphoreGive() calls are just 
             * silently ignored because the semaphore is binary 
             * (max-count is 1). This acts as a natural debounce. */
            xSemaphoreGive(time_request_sem);
            break;

        case '\r':
        case '\n':
            /* The user just hit enter, probably by accident.
             * I just ignore line terminators so we don't clutter
             * the console with error messages. */
            break;

        default:
            /* If they pressed anything else, give them a hint 
             * about what they are supposed to do. Print the hex 
             * value just in case it's some weird control character. */
            uart_printf("[KEY ] unknown key 0x%02x (press 't')\r\n",
                        (unsigned)c);
            break;
        }
    }
}
