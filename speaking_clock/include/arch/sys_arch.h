/* ============================================================
 * arch/sys_arch.h  -  lwIP <-> FreeRTOS mapping
 * ------------------------------------------------------------
 * All the sys_xxx primitives lwIP expects are implemented
 * in network/sys_arch.c using FreeRTOS semaphores and
 * queues.
 * ============================================================ */

#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"
#include "task.h"

/* lwIP "opaque" handles ----------------------------------- */
typedef SemaphoreHandle_t  sys_sem_t;
typedef SemaphoreHandle_t  sys_mutex_t;
typedef QueueHandle_t      sys_mbox_t;
typedef TaskHandle_t       sys_thread_t;

/* "Invalid" marker values --------------------------------- */
#define SYS_MBOX_NULL    ((QueueHandle_t)NULL)
#define SYS_SEM_NULL     ((SemaphoreHandle_t)NULL)
#define SYS_ARCH_TIMEOUT 0xFFFFFFFFU

/* sys_prot_t is used by SYS_ARCH_DECL_PROTECT, which we
 * implement as a critical-section wrapper.                 */
typedef UBaseType_t sys_prot_t;

#endif /* LWIP_ARCH_SYS_ARCH_H */
