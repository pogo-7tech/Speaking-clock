/* ============================================================
 * ntp_task.c
 * ------------------------------------------------------------
 * This is the middleman task. It spends most of its life
 * asleep, waiting for the key_task to poke it via a semaphore.
 * When it wakes up, it does the heavy lifting of fetching the
 * time from the internet and formatting it.
 *
 * The flow is pretty straightforward:
 *   1. Call ntp_get_time() which does the UDP/DNS stuff to
 *      talk to the NTP pool and get the raw timestamp.
 *   2. The ntp_get_time() function also handles the math to
 *      convert UTC to Indian Standard Time (IST).
 *   3. Once we have a nicely formatted time_msg_t struct,
 *      we shove it onto a queue so the speech task can pick
 *      it up and "say" it.
 *
 * IPC primitives used:
 *     - xSemaphoreTake: to wait for the user trigger
 *     - xQueueSend: to pass the result to the next task
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "system.h"
#include "time_msg.h"

/* ------------------------------------------------------------
 * ntp_task
 * ------------------------------------------------------------
 * The main loop for fetching and processing network time.
 * ---------------------------------------------------------- */
void ntp_task(void *pv)
{
    /* Boilerplate to keep the compiler from complaining about
     * the unused pv parameter that FreeRTOS tasks require. */
    (void)pv;

    /* This struct will hold our parsed time data to send 
     * down the pipeline to the speech task. */
    time_msg_t msg;

    /* Infinite loop for the task lifecycle */
    for (;;)
    {
        /* Block forever until the key task wakes us up.
         * portMAX_DELAY means we will literally wait until the end
         * of time (or until the board loses power) for that semaphore. */
        xSemaphoreTake(time_request_sem, portMAX_DELAY);

        /* Got the signal! Let the user know we're on it. */
        uart_puts("[NTP ] request received, contacting pool.ntp.org...\r\n");

        /* Go out to the network and fetch the time. 
         * rc will be 0 if everything worked out. */
        int rc = ntp_get_time(&msg);
        
        if (rc != 0)
        {
            /* Something went wrong (probably DNS failed or UDP timeout).
             * We still need to tell the speech task so it can say
             * "network error" instead of just hanging silently. */
            uart_puts("[NTP ] failed to retrieve time.\r\n");
            
            /* Flag the message as an error and zero out the time 
             * so we don't accidentally speak garbage data. */
            msg.status = TIME_STATUS_NET_FAIL;
            msg.hour = msg.minute = msg.second = 0;
        }
        else
        {
            /* Success! We got the time and it's already converted to IST.
             * Print it to the console as a nice formatted string so I can
             * verify it against my laptop's clock. */
            uart_printf("[NTP ] IST = %02u:%02u:%02u  (%04u-%02u-%02u)\r\n",
                        msg.hour, msg.minute, msg.second,
                        msg.year, msg.month, msg.day);
        }

        /* Finally, post the parsed time struct to the speech queue.
         * I'm using a short 500ms timeout here as a safety measure. 
         * If the speech task is hung or running too slowly and the 
         * queue is full, I'd rather log a warning and drop the message 
         * than have this NTP task get stuck blocking forever. */
        if (xQueueSend(time_queue, &msg, pdMS_TO_TICKS(500)) != pdPASS)
        {
            /* We couldn't put the message in the queue before the timeout */
            uart_puts("[NTP ] WARNING: speech queue full, dropping time message\r\n");
        }
    }
}
