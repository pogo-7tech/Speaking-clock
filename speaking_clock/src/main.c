/* ============================================================
 * main.c
 * ------------------------------------------------------------
 * Entry point of the network-synchronised speaking clock.
 * This is the core file where everything gets tied together.
 * I decided to keep the main function as clean as possible,
 * mostly just initializing the hardware and setting up the
 * RTOS tasks before handing over control.
 *
 * Here's the sequence of events:
 *   1. Initialize the MCU hardware (clocks, UART for console).
 *   2. Bring up the lwIP network stack (handled in a separate task
 *      so it doesn't block the scheduler).
 *   3. Create the FreeRTOS primitives (semaphore for triggering,
 *      queue for passing the time data).
 *   4. Create the three main application tasks (key, ntp, speech).
 *   5. Start the FreeRTOS scheduler to begin execution.
 *
 * I assigned task priorities based on their time-criticality:
 *      - network_bringup: 3 (highest, needs to finish first)
 *      - ntp_task:        2 (medium, handles network packets)
 *      - key_task:        1 (low, mostly just waiting for user input)
 *      - speech_task:     1 (low, just processes the queue)
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "system.h"
#include "time_msg.h"

/* -------- Forward declarations of the tasks --------------- */
/* I put the actual task implementations in separate files to keep
 * things organized, so I need to declare them here. */
extern void key_task    (void *pv);
extern void ntp_task    (void *pv);
extern void speech_task (void *pv);

/* -------- Shared RTOS handles ----------------------------- */
/* These handles need to be global so the different tasks can
 * access them for inter-process communication (IPC). */
SemaphoreHandle_t time_request_sem = NULL;
QueueHandle_t     time_queue       = NULL;

/* ------------------------------------------------------------
 * main()
 * ---------------------------------------------------------- */
/* Forward declaration for the network bring-up task */
static void network_bringup_task(void *pv);

int main(void)
{
    /* First things first: get the basic hardware running.
     * system_init configures the clocks and sets up the UART
     * so we can at least print debug messages to the console. */
    system_init();

    /* Print a nice welcome banner so I know the board actually booted */
    uart_puts("\r\n");
    uart_puts("================================================\r\n");
    uart_puts("  Network-Synchronised Speaking Clock (STM32F4)\r\n");
    uart_puts("  FreeRTOS + lwIP + NTP + QEMU\r\n");
    uart_puts("================================================\r\n");

    /* ---- Inter-task communication primitives ------------- */
    /* Create the binary semaphore used by the key task to wake up
     * the NTP task. It starts empty, which is what we want. */
    time_request_sem = xSemaphoreCreateBinary();
    configASSERT(time_request_sem != NULL); /* Crash early if we run out of heap */

    /* Create the queue that passes the parsed time from the NTP task
     * to the speech task. A depth of 4 is plenty since we don't expect
     * a backlog of time requests. */
    time_queue = xQueueCreate(4 /* depth */, sizeof(time_msg_t));
    configASSERT(time_queue != NULL);

    /* ---- Tasks ------------------------------------------- */
    BaseType_t ok;

    /* Create the task that waits for the user to press 't'.
     * I gave it priority 1 and a small stack since it just blocks on UART. */
    ok = xTaskCreate(key_task,    "KEY",    256, NULL, 1, NULL);
    configASSERT(ok == pdPASS);

    /* Create the NTP task. Needs a slightly bigger stack for networking.
     * Priority 2 so it can preempt the key task when triggered. */
    ok = xTaskCreate(ntp_task,    "NTP",    512, NULL, 2, NULL);
    configASSERT(ok == pdPASS);

    /* Create the speech task that parses the time into TTS tokens. */
    ok = xTaskCreate(speech_task, "SPEECH", 512, NULL, 1, NULL);
    configASSERT(ok == pdPASS);

    /* ---- Network Initialization Task --------------------- */
    /* I ran into a nasty bug where lwIP would freeze if I initialized it
     * before starting the scheduler. So I created this temporary task
     * with highest priority just to bring up the network, after which
     * it deletes itself. */
    ok = xTaskCreate(network_bringup_task, "NETUP", 1024, NULL, 3, NULL);
    configASSERT(ok == pdPASS);

    /* ---- Hand over to the kernel ------------------------- */
    /* Once this is called, main() effectively ends and the FreeRTOS
     * tasks take over the CPU. */
    uart_puts("[MAIN ] starting scheduler\r\n");
    vTaskStartScheduler();
    
    /* If we get here, something went terribly wrong (usually ran out of
     * heap space while creating the idle task). */
    uart_puts("[MAIN ] scheduler returned unexpectedly\r\n");

    /* Infinite loop to catch the error */
    while (1) { }
    return 0;
}

/* ============================================================
 *  FreeRTOS hook functions
 * ------------------------------------------------------------
 * These are called by the kernel when something goes wrong.
 * I implemented them to print an error and halt so I don't
 * waste hours debugging silent failures.
 * ============================================================ */

/* Called if FreeRTOS runs out of heap memory during a dynamic allocation */
void vApplicationMallocFailedHook(void)
{
    uart_puts("[FATAL] malloc failed\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) { } /* Spin forever so I can attach a debugger */
}

/* Called if a task blows past its allocated stack space */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcName)
{
    (void)xTask; /* Suppress unused parameter warning */
    uart_printf("[FATAL] stack overflow in task %s\r\n", pcName);
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

/* Optional hooks for idle and tick. Not using them right now, 
 * but FreeRTOS expects them to be defined. */
void vApplicationIdleHook(void)     { /* do nothing */ }
void vApplicationTickHook(void)     { /* do nothing */ }

/* ------------------------------------------------------------
 * network_bringup_task
 * ------------------------------------------------------------
 * This is the temporary task I created to initialize lwIP. 
 * It runs exactly once at system startup, gets an IP address,
 * and then commits suicide to free up its resources.
 * ---------------------------------------------------------- */
static void network_bringup_task(void *pv)
{
    (void)pv;
    
    /* Call the actual lwIP initialization sequence */
    network_init();
    
    /* We're done setting up the network, so delete this task 
     * passing NULL means "delete myself" */
    vTaskDelete(NULL);
}
