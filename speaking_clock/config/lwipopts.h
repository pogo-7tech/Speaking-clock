/* ============================================================
 * lwipopts.h  -  stack trim for the speaking clock
 * ------------------------------------------------------------
 * Per section 5 of the spec, only UDP / IP / DHCP are required.
 * TCP, PPP, SLIP, IPv6 etc. are turned off to save flash/RAM.
 * ============================================================ */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- OS mode -------------------------------------------- */
#define NO_SYS                          0
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1

/* ---- Protocols ------------------------------------------ */
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_UDP                        1
#define LWIP_TCP                        0     /* not needed        */
#define LWIP_ICMP                       1
#define LWIP_IGMP                       1

#define LWIP_DHCP                       1
#define LWIP_DNS                        1
#define LWIP_RAW                        0

/* ---- Memory budget (tune for MCU RAM) ------------------- */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (16 * 1024)

#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_UDP_PCB                6
#define MEMP_NUM_NETBUF                 4
#define MEMP_NUM_NETCONN                4
#define MEMP_NUM_SYS_TIMEOUT            10

#define PBUF_POOL_SIZE                  16
#define PBUF_POOL_BUFSIZE               1600

/* ---- Checksum offload ----------------------------------- */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1

/* ---- Threading (FreeRTOS) ------------------------------- */
#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          1024
#define TCPIP_THREAD_PRIO               3
#define TCPIP_MBOX_SIZE                 8

#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         4
#define DEFAULT_THREAD_STACKSIZE        512
#define DEFAULT_THREAD_PRIO             2

/* ---- Sockets API options -------------------------------- */
#define SO_REUSE                        1
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_COMPAT_SOCKETS             1

/* ---- DNS tuning ----------------------------------------- */
#define DNS_TABLE_SIZE                  4
#define DNS_MAX_NAME_LENGTH             128

/* ---- Random number for DNS query IDs -------------------- */
#include <stdlib.h>
#define LWIP_RAND() ((u32_t)rand())

/* ---- Turn off POSIX errno mapping (bare-metal) ---------- */
#define LWIP_PROVIDE_ERRNO              0
#define LWIP_SOCKET_SET_ERRNO           0

/* ---- Debug ---------------------------------------------- */
#define LWIP_DEBUG                      0
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define UDP_DEBUG                       LWIP_DBG_OFF
#define DNS_DEBUG                       LWIP_DBG_OFF

#endif /* LWIPOPTS_H */
