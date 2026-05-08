/* ============================================================
 * stm32.ld  -  linker script for STM32F405 (QEMU netduinoplus2)
 * ------------------------------------------------------------
 *   FLASH : 1 MiB @ 0x08000000
 *   SRAM  : 128 KiB @ 0x20000000  (combined SRAM1 + SRAM2)
 *
 * The stack grows downward from the end of SRAM.  The heap
 * grows upward from _end (provided for newlib's sbrk).
 * ============================================================ */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4096K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Top of stack = end of SRAM                                   */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);

/* Minimum guaranteed heap / stack sizes ----------------------- */
_Min_Heap_Size  = 0x2000;    /*  8 KiB  */
_Min_Stack_Size = 0x1000;    /*  4 KiB  */

SECTIONS
{
    /* ---- Vector table at the very top of FLASH ---------- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* ---- Code ------------------------------------------- */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ---- Read-only data --------------------------------- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM         : { __exidx_start = .;  *(.ARM.exidx*)
                      __exidx_end   = .; } > FLASH

    /* ---- Pre-init, init, fini arrays -------------------- */
    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT(.fini_array.*)))
        KEEP (*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } > FLASH

    /* ---- Initialised data (FLASH -> SRAM at reset) ------ */
    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT> FLASH

    /* ---- Zero-initialised data -------------------------- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM

    /* ---- Heap + stack sanity check ---------------------- */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end  = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } > SRAM

    /DISCARD/ : { libc.a ( * )  libm.a ( * )  libgcc.a ( * ) }
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
