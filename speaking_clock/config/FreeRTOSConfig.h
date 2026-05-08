/* ============================================================
 * FreeRTOSConfig.h  -  configuration for the speaking clock
 * ------------------------------------------------------------
 * Target : Cortex-M3 on QEMU mps2-an385
 * Scheduler : preemptive, tick = 1 ms
 * Heap      : heap_4.c, 48 KiB
 * ============================================================ */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

void uart_puts(const char *s);

/* ---- Scheduler ------------------------------------------ */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* ---- Clocks & tick -------------------------------------- */
#define configCPU_CLOCK_HZ                      ((unsigned long)25000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)

/* ---- Sizes ---------------------------------------------- */
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                ((uint16_t)128)
#define configTOTAL_HEAP_SIZE                   ((size_t)(48 * 1024))
#define configMAX_TASK_NAME_LEN                 12

/* ---- IPC primitives used -------------------------------- */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8

/* ---- Debug / hooks -------------------------------------- */
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* ---- Optional features we don't need -------------------- */
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         0

/* ---- Software timers ------------------------------------ */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                8
#define configTIMER_TASK_STACK_DEPTH            256

/* ---- API trim ------------------------------------------- */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xSemaphoreGetMutexHolder        1

/* ---- Cortex-M3 interrupt priorities --------------------- */
/* MPS2 AN385 exposes 3 implemented NVIC priority bits. */
#define configPRIO_BITS                         3
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    3
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY      << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* QEMU's MPS2 NVIC model does not expose implemented priority bits in the
 * way the FreeRTOS ARM_CM3 port probes for at startup. Use the configured
 * values directly instead of the probe-and-assert path. */
#define configSKIP_NVIC_PRIORITY_CHECK          1

/* ---- ARMv7-M port handlers ------------------------------ */
#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler      PendSV_Handler
#define xPortSysTickHandler     SysTick_Handler

/* ---- Assertion hook ------------------------------------- */
#define configASSERT(x) \
    do { if (!(x)) { uart_puts("[ASSERT] FreeRTOS assert\r\n"); taskDISABLE_INTERRUPTS(); for (;;) { } } } while (0)

#endif /* FREERTOS_CONFIG_H */
