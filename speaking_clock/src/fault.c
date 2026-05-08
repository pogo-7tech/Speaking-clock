#include "system.h"
#include "FreeRTOS.h"
#include "task.h"

static void trap(const char *name)
{
    uart_puts("[FAULT] ");
    uart_puts(name);
    uart_puts("\r\n");
    taskDISABLE_INTERRUPTS();
    for (;;) { }
}

void Default_Handler(void)   { trap("Default_Handler"); }
void HardFault_Handler(void) { trap("HardFault_Handler"); }
void MemManage_Handler(void) { trap("MemManage_Handler"); }
void BusFault_Handler(void)  { trap("BusFault_Handler"); }
void UsageFault_Handler(void){ trap("UsageFault_Handler"); }
