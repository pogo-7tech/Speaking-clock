/* ============================================================
 * lwip_init.c
 * ------------------------------------------------------------
 * This file is responsible for bringing up the lwIP network 
 * stack. It's configured to run in NO_SYS=0 mode, meaning 
 * the core network stack runs inside its own dedicated 
 * FreeRTOS task called "tcpip_thread".
 *
 * Sequence of events:
 *   1. tcpip_init(): Creates the internal thread and the mailbox
 *      used for thread-safe API calls.
 *   2. netif_add(): Registers our custom QEMU LAN9118 driver 
 *      with the lwIP core.
 *   3. dhcp_start(): Triggers the DHCP state machine to go ask 
 *      the QEMU SLIRP router (usually 10.0.2.2) for an IP address.
 *
 * Once this function finishes, the rest of the app can safely 
 * use standard sockets (like in ntp_client.c).
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

#include "system.h"

/* The low-level init function defined over in netif_qemu.c */
err_t qemu_netif_init(struct netif *netif);

/* We only have one network interface (the emulated LAN9118 NIC), 
 * so a single global struct is fine here. */
static struct netif s_netif;

/* ------------------------------------------------------------
 * tcpip_init callback
 * ------------------------------------------------------------
 * This flag gets set when the internal tcpip_thread is fully 
 * initialized. We need this because tcpip_init() runs asynchronously.
 * ---------------------------------------------------------- */
static volatile int s_tcpip_ready = 0;

static void tcpip_init_done_cb(void *arg)
{
    /* Mark the flag as true so the main initialization can proceed */
    (void)arg;
    s_tcpip_ready = 1;
}

/* ------------------------------------------------------------
 *  wait_for_dhcp
 * ------------------------------------------------------------
 * A simple polling loop to wait until DHCP gives us an IP address.
 * ---------------------------------------------------------- */
static int wait_for_dhcp(uint32_t timeout_ms)
{
    /* Check every 100 milliseconds */
    const uint32_t step = 100;
    uint32_t waited = 0;

    while (waited < timeout_ms)
    {
        /* dhcp_supplied_address returns true if we got a valid lease */
        if (dhcp_supplied_address(&s_netif))
        {
            /* Awesome, we got an IP! Let's print it out so I can see it.
             * Usually it's 10.0.2.15 under QEMU user networking. */
            uart_printf("[NET ] DHCP bound: %s\r\n",
                        ip4addr_ntoa(netif_ip4_addr(&s_netif)));
            return 0; /* Success */
        }
        
        /* Yield to the scheduler while we wait, so other tasks can run */
        vTaskDelay(pdMS_TO_TICKS(step));
        waited += step;
    }
    
    /* If we break out of the loop, we timed out */
    return -1;
}

/* ------------------------------------------------------------
 *  network_init
 * ------------------------------------------------------------
 * The main public entry point, called by the network_bringup_task.
 * ---------------------------------------------------------- */
void network_init(void)
{
    /* We don't know our IP address yet, so initialize everything to zero.
     * DHCP will fill these in automatically later. */
    ip4_addr_t any;
    IP4_ADDR(&any, 0, 0, 0, 0);

    uart_puts("[NET ] starting lwIP...\r\n");

    /* 1. Start the core tcpip thread.
     *    Pass in our callback function so we know when it's done. */
    tcpip_init(tcpip_init_done_cb, NULL);
    
    /* Spin here until the callback fires. Yields 10ms at a time. */
    while (!s_tcpip_ready) { 
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    uart_puts("[NET ] TCP/IP thread ready\r\n");

    /* 2. Register our network interface with the lwIP core.
     *    - Passes the zeroed IP configs
     *    - Passes our BSP init function (qemu_netif_init)
     *    - Tells it to use tcpip_input for handing off received packets */
    netif_add(&s_netif,
              &any, &any, &any,        /* ip, netmask, gw     */
              NULL,                    /* private state       */
              qemu_netif_init,         /* BSP-specific init   */
              tcpip_input);            /* input hand-off      */

    /* Tell lwIP this is our primary network interface and bring it up */
    netif_set_default(&s_netif);
    netif_set_up(&s_netif);

    /* 3. Configure some default DNS servers just in case DHCP doesn't
     *    provide them. I'm using Google's public DNS (8.8.8.8) and 
     *    Cloudflare (1.1.1.1) because they're reliable. */
    ip_addr_t dns0, dns1;
    IP_ADDR4(&dns0, 8, 8, 8, 8);
    IP_ADDR4(&dns1, 1, 1, 1, 1);
    dns_setserver(0, &dns0);
    dns_setserver(1, &dns1);

    /* 4. Start the DHCP client state machine to go fetch an IP address. */
    if (dhcp_start(&s_netif) != ERR_OK)
    {
        /* If it fails to even start, something is seriously broken */
        uart_puts("[NET ] dhcp_start() failed\r\n");
        return;
    }

    /* 5. Wait for the DHCP process to complete.
     *    I gave it a generous 10 second timeout since QEMU can sometimes
     *    be slow to respond. */
    uart_puts("[NET ] waiting for DHCP...\r\n");
    if (wait_for_dhcp(10000 /* ms */) != 0)
    {
        /* DHCP timed out. We might still be able to route locally, 
         * so just print a warning and continue anyway. */
        uart_puts("[NET ] DHCP timed out - continuing anyway\r\n");
    }
    else
    {
        /* We got an IP! As a nice convenience, let's automatically trigger
         * the first NTP time sync so the user doesn't have to press 't'
         * immediately after booting. */
        uart_puts("[NET ] auto-triggering initial NTP sync\r\n");
        xSemaphoreGive(time_request_sem);
    }
}
