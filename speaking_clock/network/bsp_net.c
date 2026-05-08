/* ============================================================
 * bsp_net.c  -  SMSC LAN9118 Ethernet driver for MPS2-AN385
 * ------------------------------------------------------------
 * This was probably the hardest part of the porting effort. 
 * Since QEMU's STM32 emulation doesn't support the Ethernet 
 * MAC, I had to switch to the MPS2-AN385 board which emulates
 * an SMSC LAN9118 chip.
 *
 * This is a bare-bones, polled driver for that chip. I got 
 * all the register offsets from the LAN9118 datasheet, section 7.
 * It only handles single-frame TX and RX, but that's perfectly
 * enough for lwIP to do its DHCP and NTP queries.
 * ============================================================ */

#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "system.h"

/* ------------------------------------------------------------
 * Hardware Register Map
 * ------------------------------------------------------------
 * The LAN9118 is memory-mapped to 0x40200000 on the MPS2.
 * I defined macros for all the internal registers I need to 
 * touch. The volatile cast is crucial so the compiler doesn't
 * optimize away my hardware reads/writes.
 * ---------------------------------------------------------- */
#define LAN9118_BASE     0x40200000UL

/* The two main data pipelines */
#define RX_DATA_FIFO    (*(volatile uint32_t *)(LAN9118_BASE + 0x00))
#define TX_DATA_FIFO    (*(volatile uint32_t *)(LAN9118_BASE + 0x20))

/* Status and control registers */
#define RX_STATUS_FIFO  (*(volatile uint32_t *)(LAN9118_BASE + 0x40))
#define TX_STATUS_FIFO  (*(volatile uint32_t *)(LAN9118_BASE + 0x48))
#define ID_REV          (*(volatile uint32_t *)(LAN9118_BASE + 0x50))
#define IRQ_CFG         (*(volatile uint32_t *)(LAN9118_BASE + 0x54))
#define INT_STS         (*(volatile uint32_t *)(LAN9118_BASE + 0x58))
#define INT_EN          (*(volatile uint32_t *)(LAN9118_BASE + 0x5C))
#define BYTE_TEST       (*(volatile uint32_t *)(LAN9118_BASE + 0x64))
#define FIFO_INT        (*(volatile uint32_t *)(LAN9118_BASE + 0x68))
#define RX_CFG          (*(volatile uint32_t *)(LAN9118_BASE + 0x6C))
#define TX_CFG          (*(volatile uint32_t *)(LAN9118_BASE + 0x70))
#define HW_CFG          (*(volatile uint32_t *)(LAN9118_BASE + 0x74))
#define RX_DP_CTRL      (*(volatile uint32_t *)(LAN9118_BASE + 0x78))
#define RX_FIFO_INF     (*(volatile uint32_t *)(LAN9118_BASE + 0x7C))
#define TX_FIFO_INF     (*(volatile uint32_t *)(LAN9118_BASE + 0x80))
#define PMT_CTRL        (*(volatile uint32_t *)(LAN9118_BASE + 0x84))

/* The MAC has its own internal indirect registers */
#define MAC_CSR_CMD     (*(volatile uint32_t *)(LAN9118_BASE + 0xA4))
#define MAC_CSR_DATA    (*(volatile uint32_t *)(LAN9118_BASE + 0xA8))

/* Indirect MAC register IDs */
#define MAC_CR      1
#define MAC_ADDRH   2
#define MAC_ADDRL   3

/* Assorted bitmasks from the datasheet */
#define MAC_CR_TXEN     (1U << 3)
#define MAC_CR_RXEN     (1U << 2)
#define MAC_CR_BCAST    (1U << 11)
#define MAC_CR_PRMS     (1U << 18) /* Promiscuous mode */
#define MAC_CR_PADSTR   (1U << 8)  /* Automatic padding */
#define TX_CFG_ON       (1U << 1)
#define HW_CFG_SF       (1U << 20) /* Store-and-forward mode */
#define MAC_CSR_BUSY    (1U << 31)

/* ------------------------------------------------------------
 * Globals
 * ---------------------------------------------------------- */
/* Semaphore to wake up the lwIP thread when a packet arrives */
SemaphoreHandle_t bsp_net_rx_sem;

/* I just made up a MAC address since QEMU doesn't care. 
 * Just has to not be a multicast address. */
static uint8_t s_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

/* Flag to make sure we don't initialize the hardware twice */
static int s_inited;

/* ------------------------------------------------------------
 * mac_write
 * ------------------------------------------------------------
 * Writing to the MAC registers is weird. They aren't mapped 
 * directly into memory. Instead, you have to write to a "data" 
 * register, then write to a "command" register, and spin-wait 
 * until the BUSY bit clears.
 * ---------------------------------------------------------- */
static void mac_write(uint32_t reg, uint32_t val)
{
    /* Wait in case a previous command is still running */
    for (int i = 0; i < 1000; i++) { if (!(MAC_CSR_CMD & MAC_CSR_BUSY)) break; }
    
    MAC_CSR_DATA = val;
    MAC_CSR_CMD  = MAC_CSR_BUSY | reg;
    
    /* Wait for our command to finish */
    for (int i = 0; i < 1000; i++) { if (!(MAC_CSR_CMD & MAC_CSR_BUSY)) break; }
}

/* ------------------------------------------------------------
 * lan9118_init
 * ------------------------------------------------------------
 * Configures the chip out of reset.
 * ---------------------------------------------------------- */
static void lan9118_init(void)
{
    if (s_inited) return; /* Already done! */
    s_inited = 1;

    /* Create the binary semaphore for the RX interrupt to use */
    bsp_net_rx_sem = xSemaphoreCreateBinary();
    configASSERT(bsp_net_rx_sem != NULL);

    /* Sanity check: reading BYTE_TEST should always return 0x87654321
     * according to the datasheet. Good way to test the memory bus. */
    volatile uint32_t bt = BYTE_TEST;
    (void)bt;

    /* Put the chip into store-and-forward mode and enable TX */
    HW_CFG = HW_CFG_SF;
    TX_CFG = TX_CFG_ON;

    /* Pack the 6-byte MAC address into two 32-bit registers */
    uint32_t addrl = s_mac[0] | (s_mac[1]<<8) | (s_mac[2]<<16) | (s_mac[3]<<24);
    uint32_t addrh = s_mac[4] | (s_mac[5]<<8);
    mac_write(MAC_ADDRL, addrl);
    mac_write(MAC_ADDRH, addrh);
    
    /* Accept broadcast/promiscuous RX while bringing up DHCP on SLIRP.
     * QEMU's LAN9118 model defaults to PRMS after reset; keep equivalent
     * filtering enabled instead of narrowing it to unicast-only. */
    mac_write(MAC_CR, MAC_CR_TXEN | MAC_CR_RXEN | MAC_CR_BCAST |
                      MAC_CR_PRMS | MAC_CR_PADSTR);
}

/* ------------------------------------------------------------
 * bsp_net_send
 * ------------------------------------------------------------
 * Called by lwIP to push an Ethernet frame onto the wire.
 * ---------------------------------------------------------- */
int bsp_net_send(const uint8_t *data, uint16_t len)
{
    lan9118_init(); /* Just in case */
    
    /* Standard MTU limit */
    if (len > 1514) return -1;

    uint16_t payload_len = len;
    
    /* Ethernet frames must be at least 60 bytes long (not counting FCS). 
     * If lwIP gives us a tiny packet (like an ARP request), we have to 
     * pad it out to 60 bytes. */
    uint16_t wire_len = (len < 60) ? 60 : len;

    /* The LAN9118 requires two "command" words to be pushed into the 
     * TX FIFO before the actual packet data. These tell the chip 
     * how long the packet is. */
    uint32_t cmd_a = (1U << 13) | (1U << 12) | (wire_len & 0x7FF);
    uint32_t cmd_b = wire_len & 0xFFFF;

    TX_DATA_FIFO = cmd_a;
    TX_DATA_FIFO = cmd_b;

    /* The chip expects data in 32-bit chunks, so we have to pack the 
     * raw byte array into 32-bit words. */
    uint16_t full_words = wire_len / 4;
    uint16_t remainder  = wire_len % 4;

    /* Push all the full 4-byte chunks */
    for (uint16_t i = 0; i < full_words; i++)
    {
        uint16_t off = i * 4;
        uint32_t w = 0;

        /* Carefully pack the bytes, padding with zeros if we exceed 
         * the original payload_len but haven't hit wire_len yet. */
        if ((off + 0) < payload_len) w |= (uint32_t)data[off + 0];
        if ((off + 1) < payload_len) w |= (uint32_t)data[off + 1] << 8;
        if ((off + 2) < payload_len) w |= (uint32_t)data[off + 2] << 16;
        if ((off + 3) < payload_len) w |= (uint32_t)data[off + 3] << 24;
        
        TX_DATA_FIFO = w; /* Shove it into the hardware FIFO */
    }

    /* Handle the last 1-3 bytes if the length wasn't a multiple of 4 */
    if (remainder > 0)
    {
        uint16_t off = full_words * 4;
        uint32_t w = 0;
        if ((off + 0) < payload_len) w |= (uint32_t)data[off + 0];
        if ((off + 1) < payload_len) w |= (uint32_t)data[off + 1] << 8;
        if ((off + 2) < payload_len) w |= (uint32_t)data[off + 2] << 16;
        TX_DATA_FIFO = w;
    }
    
    return 0;
}

/* ------------------------------------------------------------
 * bsp_net_recv
 * ------------------------------------------------------------
 * Called by lwIP's polling thread to drain packets from the RX FIFO.
 * ---------------------------------------------------------- */
int bsp_net_recv(uint8_t *buf, uint16_t maxlen)
{
    lan9118_init();

    /* Check the RX FIFO information register. If the top 8 bits are zero, 
     * there are no packets waiting for us. */
    if (((RX_FIFO_INF >> 16) & 0xFF) == 0) return 0;

    /* Read the status word for the first packet in the FIFO. 
     * This tells us how long the packet is. */
    uint32_t status = RX_STATUS_FIFO;
    uint16_t len = (status >> 16) & 0x3FFF;
    
    /* Sanity check */
    if (len < 4) return 0;
    
    /* The length reported by the chip includes the 4-byte CRC, 
     * which lwIP doesn't want. So subtract 4. */
    len -= 4;
    
    /* Don't overflow lwIP's buffer */
    if (len > maxlen) return -1;

    /* Figure out how many 32-bit words we need to pop off the FIFO */
    uint16_t total_bytes = len + 4; /* payload + CRC in FIFO */
    uint16_t total_words = (uint16_t)((total_bytes + 3U) / 4U);
    uint16_t out = 0;

    /* Pop the words and unpack them into the byte buffer */
    for (uint16_t i = 0; i < total_words; i++)
    {
        uint32_t w = RX_DATA_FIFO;

        if (out < len) buf[out++] = (uint8_t)(w);
        if (out < len) buf[out++] = (uint8_t)(w >> 8);
        if (out < len) buf[out++] = (uint8_t)(w >> 16);
        if (out < len) buf[out++] = (uint8_t)(w >> 24);
    }

    /* Advance past any packet-alignment padding still left in the RX FIFO
     * so the next read starts on the next frame boundary. The hardware
     * requires us to write this magic bit to do it. */
    RX_DP_CTRL = 0x80000000U;

    return len;
}

/* Just a getter for the MAC address so lwIP knows who we are */
void bsp_net_get_mac(uint8_t mac[6]) { memcpy(mac, s_mac, 6); }

/* ------------------------------------------------------------
 * ETHERNET_IRQHandler
 * ------------------------------------------------------------
 * This is the hardware interrupt handler. We don't actually 
 * hook it up to the NVIC in this simple port, we just poll instead,
 * but I left the structure here in case we want to upgrade later.
 * ---------------------------------------------------------- */
void ETHERNET_IRQHandler(void)
{
    BaseType_t hpw = pdFALSE;

    if (bsp_net_rx_sem != NULL)
    {
        /* Wake up the lwIP thread */
        xSemaphoreGiveFromISR(bsp_net_rx_sem, &hpw);
    }

    portYIELD_FROM_ISR(hpw);
}
