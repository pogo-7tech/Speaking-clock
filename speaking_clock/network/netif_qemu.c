/* ============================================================
 * netif_qemu.c
 * ------------------------------------------------------------
 * Minimal lwIP netif driver that talks to the virtio-net
 * device exposed by QEMU (machine netduinoplus2 / mps2-an385).
 *
 * The low-level frame TX/RX is delegated to a BSP layer
 * (virtio_net_xxx()) that is outside the scope of this
 * assignment - a production port would plug in the vendor
 * HAL driver.  In this file we only implement the lwIP netif
 * boilerplate:
 *
 *   - link-level output wrapper            (low_level_output)
 *   - link-level input  wrapper            (low_level_input)
 *   - a FreeRTOS "rx thread" that pumps incoming frames into
 *     lwIP via tcpip_input()
 *   - the init function that lwip_init.c installs.
 *
 * Replace the stubs marked BSP with real virtio-net
 * calls when porting.
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"

#include <string.h>
#include "system.h"

#define IFNAME0 'q'
#define IFNAME1 'e'

/* ------------------------------------------------------------
 *  BSP hooks - implemented in network/bsp_net.c
 * ---------------------------------------------------------- */
extern int  bsp_net_send    (const uint8_t *data, uint16_t len);
extern int  bsp_net_recv    (uint8_t *buf, uint16_t maxlen);
extern void bsp_net_get_mac (uint8_t mac[6]);
extern SemaphoreHandle_t bsp_net_rx_sem;      /* given by ISR  */

/* ------------------------------------------------------------
 *  low_level_output  - hand an lwIP pbuf to the NIC
 * ---------------------------------------------------------- */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    (void)netif;

    /* Copy the (possibly chained) pbuf into a linear buffer. */
    static uint8_t tx_buf[1600];

    if (p->tot_len > sizeof tx_buf) { return ERR_IF; }
    pbuf_copy_partial(p, tx_buf, p->tot_len, 0);

    if (bsp_net_send(tx_buf, p->tot_len) != 0) { return ERR_IF; }

    LINK_STATS_INC(link.xmit);
    return ERR_OK;
}

/* ------------------------------------------------------------
 *  low_level_input  - pull one frame from the NIC into a pbuf
 * ---------------------------------------------------------- */
static struct pbuf *low_level_input(void)
{
    static uint8_t rx_buf[1600];

    int n = bsp_net_recv(rx_buf, sizeof rx_buf);
    if (n <= 0) { return NULL; }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)n, PBUF_POOL);
    if (p == NULL)
    {
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
        return NULL;
    }
    pbuf_take(p, rx_buf, (u16_t)n);
    LINK_STATS_INC(link.recv);
    return p;
}

/* ------------------------------------------------------------
 *  RX thread - prefer IRQ wakeups but fall back to polling
 * ---------------------------------------------------------- */
static void netif_rx_thread(void *arg)
{
    struct netif *netif = (struct netif *)arg;

    for (;;)
    {
        if (bsp_net_rx_sem != NULL)
        {
            (void)xSemaphoreTake(bsp_net_rx_sem, pdMS_TO_TICKS(20));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        for (;;)
        {
            struct pbuf *p = low_level_input();
            if (p == NULL) { break; }

            if (netif->input(p, netif) != ERR_OK) { pbuf_free(p); }
        }
    }
}

/* ------------------------------------------------------------
 *  Public init - referenced from lwip_init.c
 * ---------------------------------------------------------- */
err_t qemu_netif_init(struct netif *netif)
{
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

    netif->output     = etharp_output;
#if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
#endif
    netif->linkoutput = low_level_output;

    netif->mtu        = 1500;
    netif->hwaddr_len = 6;
    netif->flags      = NETIF_FLAG_BROADCAST
                      | NETIF_FLAG_ETHARP
                      | NETIF_FLAG_LINK_UP
                      | NETIF_FLAG_IGMP;

    bsp_net_get_mac(netif->hwaddr);

    /* Start the RX helper task. Polling keeps RX alive even if the
     * emulated NIC interrupt is not wired or enabled yet. */
    if (xTaskCreate(netif_rx_thread, "netif-rx",
                    512, netif, tskIDLE_PRIORITY + 3, NULL) != pdPASS)
    {
        return ERR_MEM;
    }

    return ERR_OK;
}
