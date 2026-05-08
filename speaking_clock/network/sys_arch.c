/* ============================================================
 * sys_arch.c  -  lwIP port glue for FreeRTOS
 * ------------------------------------------------------------
 * This file is the "glue" layer between the lwIP network stack
 * and the FreeRTOS operating system. lwIP is designed to be OS-
 * agnostic, so it calls generic functions like `sys_sem_new` 
 * and relies on us to implement them using the actual RTOS 
 * primitives (like `xSemaphoreCreateCounting`).
 *
 * To save flash and RAM, I only implemented the specific 
 * primitives that our subset of lwIP (UDP, DHCP, sockets) 
 * actually uses. Everything else is left out.
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/stats.h"
#include "lwip/debug.h"
#include "arch/sys_arch.h"

/* ============================================================
 *  Init
 * ------------------------------------------------------------
 * Called by lwIP during startup to initialize the OS layer.
 * ========================================================== */
void sys_init(void) 
{ 
    /* Nothing to do here! We initialize FreeRTOS in main() 
     * before we even start the network task. */ 
}

/* ============================================================
 *  Time
 * ------------------------------------------------------------
 * lwIP needs to know the current system time for timeouts
 * and DHCP lease renewals.
 * ========================================================== */
u32_t sys_now(void)
{
    /* Grab the raw tick count from FreeRTOS and convert it 
     * into milliseconds, which is what lwIP expects. */
    return (u32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* Alias for sys_now, some older parts of lwIP still use this */
u32_t sys_jiffies(void) { return sys_now(); }

/* ============================================================
 *  Semaphores
 * ------------------------------------------------------------
 * lwIP uses semaphores to block threads while waiting for 
 * network events (like a packet arriving or a timeout).
 * ========================================================== */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
    /* Create a counting semaphore. The max value is 0xFFFF (65535),
     * which is practically infinity for our purposes. The initial 
     * count is passed in by lwIP. */
    *sem = xSemaphoreCreateCounting(0xFFFF, count);
    
    /* Let lwIP know if we ran out of heap memory */
    if (*sem == NULL) { return ERR_MEM; }
    return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
    /* Clean up the semaphore and set the pointer to NULL to 
     * prevent dangling references. */
    if (sem && *sem) { 
        vSemaphoreDelete(*sem); 
        *sem = NULL; 
    }
}

void sys_sem_signal(sys_sem_t *sem) 
{ 
    /* Wake up anyone waiting on this semaphore */
    xSemaphoreGive(*sem); 
}

u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout_ms)
{
    /* Record when we started waiting so we can calculate how 
     * long it took if we successfully get the semaphore. */
    TickType_t start = xTaskGetTickCount();
    
    /* Convert the millisecond timeout into RTOS ticks. 
     * A timeout of 0 means "wait forever". */
    TickType_t to = (timeout_ms == 0) ? portMAX_DELAY 
                                      : pdMS_TO_TICKS(timeout_ms);

    /* Try to take the semaphore */
    if (xSemaphoreTake(*sem, to) == pdTRUE)
    {
        /* We got it! Return the number of milliseconds we waited. */
        return (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
    }
    
    /* We timed out */
    return SYS_ARCH_TIMEOUT;
}

int sys_sem_valid(sys_sem_t *sem) { return (*sem != NULL); }
void sys_sem_set_invalid(sys_sem_t *sem) { *sem = NULL; }

/* ============================================================
 *  Mutexes
 * ------------------------------------------------------------
 * Used by lwIP to protect shared resources (like the memory pool)
 * from concurrent access by different tasks.
 * ========================================================== */
err_t sys_mutex_new(sys_mutex_t *m)
{
    /* We use FreeRTOS mutexes because they support priority 
     * inheritance, which prevents nasty priority inversion bugs. */
    *m = xSemaphoreCreateMutex();
    return (*m != NULL) ? ERR_OK : ERR_MEM;
}

void  sys_mutex_free   (sys_mutex_t *m) { vSemaphoreDelete(*m); *m = NULL; }
void  sys_mutex_lock   (sys_mutex_t *m) { xSemaphoreTake(*m, portMAX_DELAY); }
void  sys_mutex_unlock (sys_mutex_t *m) { xSemaphoreGive(*m); }
int   sys_mutex_valid  (sys_mutex_t *m) { return (*m != NULL); }
void  sys_mutex_set_invalid(sys_mutex_t *m) { *m = NULL; }

/* ============================================================
 *  Mailboxes
 * ------------------------------------------------------------
 * Used to pass messages (usually pointers to packets) between 
 * the interrupt handler and the main tcpip_thread.
 * ========================================================== */
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
    /* A mailbox is just a standard FreeRTOS queue where each 
     * item is a single void pointer. */
    *mbox = xQueueCreate(size, sizeof(void *));
    return (*mbox != NULL) ? ERR_OK : ERR_MEM;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
    if (mbox && *mbox) { vQueueDelete(*mbox); *mbox = NULL; }
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    /* Block forever until there is space in the queue to post 
     * the message. */
    while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE) { }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    /* Try to post the message, but fail immediately (timeout 0)
     * if the queue is full. */
    return (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE) ? ERR_OK : ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
    /* Special version for calling from inside hardware interrupts 
     * (like the Ethernet RX interrupt). Uses the FromISR variant
     * of the FreeRTOS API. */
    BaseType_t hpw = pdFALSE;
    BaseType_t ok  = xQueueSendToBackFromISR(*mbox, &msg, &hpw);
    
    /* If waking the task caused a higher priority task to become 
     * ready, request a context switch so it runs immediately. */
    portYIELD_FROM_ISR(hpw);
    
    return (ok == pdTRUE) ? ERR_OK : ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout_ms)
{
    /* If lwIP passes NULL, it means it just wants to wait for a message
     * but doesn't actually care about the message content. We use a 
     * local dummy pointer so we don't crash when passing it to FreeRTOS. */
    void *dummy;
    if (msg == NULL) { msg = &dummy; }

    TickType_t start = xTaskGetTickCount();
    TickType_t to    = (timeout_ms == 0) ? portMAX_DELAY
                                         : pdMS_TO_TICKS(timeout_ms);

    if (xQueueReceive(*mbox, msg, to) == pdTRUE)
    {
        /* Success! Return the time we spent waiting. */
        return (xTaskGetTickCount() - start) * portTICK_PERIOD_MS;
    }
    return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
    /* Same dummy pointer trick as above */
    void *dummy;
    if (msg == NULL) { msg = &dummy; }
    
    /* Don't block at all. Just grab it if it's there. */
    return (xQueueReceive(*mbox, msg, 0) == pdTRUE) ? 0 : SYS_MBOX_EMPTY;
}

int  sys_mbox_valid  (sys_mbox_t *mbox) { return (*mbox != NULL); }
void sys_mbox_set_invalid(sys_mbox_t *mbox) { *mbox = NULL; }

/* ============================================================
 *  Thread creation
 * ------------------------------------------------------------
 * How lwIP creates its internal background threads.
 * ========================================================== */
sys_thread_t sys_thread_new(const char *name,
                            lwip_thread_fn thread,
                            void *arg,
                            int  stacksize,
                            int  prio)
{
    /* This maps perfectly 1-to-1 with FreeRTOS's xTaskCreate. */
    TaskHandle_t h = NULL;
    xTaskCreate((TaskFunction_t)thread, name,
                stacksize, arg, (UBaseType_t)prio, &h);
    return h;
}

/* ============================================================
 *  Critical-section protection
 * ------------------------------------------------------------
 * Used by lwIP for very short blocks of code that absolutely 
 * cannot be interrupted.
 * ========================================================== */
sys_prot_t sys_arch_protect(void)
{
    /* Disable all maskable interrupts globally */
    taskENTER_CRITICAL();
    return 0;
}

void sys_arch_unprotect(sys_prot_t p) 
{ 
    /* Re-enable interrupts */
    (void)p; 
    taskEXIT_CRITICAL(); 
}
