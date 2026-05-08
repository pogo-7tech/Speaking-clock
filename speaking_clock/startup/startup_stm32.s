/* ============================================================
 * startup_stm32.s  -  minimal Cortex-M3 startup for mps2-an385
 * ------------------------------------------------------------
 *   - places the vector table at the start of FLASH
 *   - initialises SP from _estack
 *   - copies .data from FLASH -> SRAM
 *   - zero-fills .bss
 *   - calls SystemInit (optional) and main
 *
 * All interrupt vectors except reset/SVC/PendSV/SysTick are
 * aliased to a weak Default_Handler that spins forever.
 * ============================================================ */

    .syntax unified
    .cpu   cortex-m3
    .fpu   softvfp
    .thumb

/* -------- Symbols provided by the linker script ---------- */
    .word   _sidata          /* src  of .data in FLASH       */
    .word   _sdata           /* dest of .data in SRAM        */
    .word   _edata
    .word   _sbss
    .word   _ebss

/* ============================================================
 *  Reset_Handler
 * ========================================================== */
    .section  .text.Reset_Handler
    .weak     Reset_Handler
    .type     Reset_Handler, %function
Reset_Handler:
    ldr    sp, =_estack

    /* ---- Copy .data from FLASH to SRAM --------------- */
    ldr    r0, =_sdata
    ldr    r1, =_edata
    ldr    r2, =_sidata
    movs   r3, #0
    b      copy_data_cond
copy_data_loop:
    ldr    r4, [r2, r3]
    str    r4, [r0, r3]
    adds   r3, r3, #4
copy_data_cond:
    adds   r4, r0, r3
    cmp    r4, r1
    bcc    copy_data_loop

    /* ---- Zero .bss ----------------------------------- */
    ldr    r2, =_sbss
    ldr    r4, =_ebss
    movs   r3, #0
    b      zero_bss_cond
zero_bss_loop:
    str    r3, [r2]
    adds   r2, r2, #4
zero_bss_cond:
    cmp    r2, r4
    bcc    zero_bss_loop

    /* ---- Optional low-level setup (weak) ------------- */
    bl     SystemInit

    /* ---- C runtime: __libc_init_array then main ------ */
    bl     __libc_init_array
    bl     main

    /* If main ever returns, hang. */
hang:
    b      hang
    .size   Reset_Handler, .-Reset_Handler

/* ============================================================
 *  Default_Handler - weakly aliased by all unused vectors
 * ========================================================== */
    .section  .text.Default_Handler, "ax", %progbits
    .weak    Default_Handler
    .type    Default_Handler, %function
Default_Handler:
    b .
    .size    Default_Handler, .-Default_Handler

/* Weak, always-spinning SystemInit if no BSP supplies one. */
    .weak    SystemInit
    .type    SystemInit, %function
SystemInit:
    bx      lr
    .size   SystemInit, .-SystemInit

/* ============================================================
 *  Vector table
 * ========================================================== */
    .section  .isr_vector, "a", %progbits
    .type     g_pfnVectors, %object
    .size     g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    .word  _estack
    .word  Reset_Handler
    .word  NMI_Handler
    .word  HardFault_Handler
    .word  MemManage_Handler
    .word  BusFault_Handler
    .word  UsageFault_Handler
    .word  0
    .word  0
    .word  0
    .word  0
    .word  SVC_Handler           /* FreeRTOS                   */
    .word  DebugMon_Handler
    .word  0
    .word  PendSV_Handler        /* FreeRTOS                   */
    .word  SysTick_Handler       /* FreeRTOS                   */

    /* External interrupts.
     * For mps2-an385 the LAN9118 Ethernet IRQ is external IRQ 48.
     * We poll UART, so all other device vectors stay unpopulated. */
    .rept  48
    .word  Default_Handler
    .endr
    .word  ETHERNET_IRQHandler
    .rept  31
    .word  Default_Handler
    .endr

/* ============================================================
 *  Weak aliases for standard Cortex-M exception handlers
 * ========================================================== */
    .weak  NMI_Handler
    .thumb_set NMI_Handler, Default_Handler
    .weak  HardFault_Handler
    .thumb_set HardFault_Handler, Default_Handler
    .weak  MemManage_Handler
    .thumb_set MemManage_Handler, Default_Handler
    .weak  BusFault_Handler
    .thumb_set BusFault_Handler, Default_Handler
    .weak  UsageFault_Handler
    .thumb_set UsageFault_Handler, Default_Handler
    .weak  DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak  ETHERNET_IRQHandler
    .thumb_set ETHERNET_IRQHandler, Default_Handler
