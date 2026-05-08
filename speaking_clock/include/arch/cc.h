/* ============================================================
 * arch/cc.h  -  lwIP compiler / platform abstraction
 * ------------------------------------------------------------
 * Target : arm-none-eabi-gcc, little-endian, 32-bit
 * ============================================================ */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Integer types -------------------------------------------- */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

/* printf format specifiers -------------------------------- */
#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

/* Byte order (ARM is usually configured little-endian) ----- */
#define BYTE_ORDER LITTLE_ENDIAN

/* Structure packing (GCC) --------------------------------- */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Diagnostic macros --------------------------------------- */
extern void uart_printf(const char *fmt, ...);

#define LWIP_PLATFORM_DIAG(x)   do { uart_printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { uart_printf("ASSERT %s\r\n", x); \
                                     while (1) { } } while (0)

#endif /* LWIP_ARCH_CC_H */
