/* ============================================================
 * ntp_client.c
 * ------------------------------------------------------------
 * This file contains my custom NTPv4 client. I didn't want to 
 * pull in a massive third-party NTP library just to get the time, 
 * so I wrote this minimal client directly on top of the lwIP 
 * sockets API.
 *
 * How it works:
 *   1. Does a DNS lookup to find the IP of the NTP pool server.
 *   2. Crafts a tiny 48-byte UDP packet (NTP version 4, Client mode).
 *   3. Sends it to port 123 and waits up to 3 seconds for a reply.
 *   4. Extracts the 64-bit "Transmit Timestamp" from the server's reply.
 *   5. Does some nasty math to convert the timestamp into a human-readable
 *      date/time in Indian Standard Time (IST).
 *
 * I found the NTP packet layout in RFC 5905, section 7.3. I put a 
 * little diagram here so I don't forget where the bytes are:
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |LI | VN  |Mode |    Stratum    |     Poll      |   Precision   |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                         Root Delay                            |
 *  +---------------------------------------------------------------+
 *  |                       Root Dispersion                         |
 *  +---------------------------------------------------------------+
 *  |                     Reference Identifier                      |
 *  +---------------------------------------------------------------+
 *  |                    Reference Timestamp (8)                    |
 *  +---------------------------------------------------------------+
 *  |                    Originate  Timestamp (8)                   |
 *  +---------------------------------------------------------------+
 *  |                    Receive   Timestamp (8)                    |
 *  +---------------------------------------------------------------+
 *  |                    Transmit  Timestamp (8)   <-- we read this |
 *  +---------------------------------------------------------------+
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"

#include <string.h>
#include <stdint.h>

#include "system.h"
#include "time_msg.h"

/* ------------------------------------------------------------
 * Constants and Configuration
 * ---------------------------------------------------------- */
#define NTP_PORT                123
#define NTP_PKT_LEN             48

/* NTP starts its epoch in 1900, but Unix starts in 1970. 
 * This is the exact number of seconds between those two dates. */
#define NTP_EPOCH_OFFSET        2208988800UL   
#define IST_OFFSET_SEC          (5*3600 + 30*60) /* 5 hours, 30 minutes */

/* I'm using the Indian NTP pool as the primary since I'm targeting IST.
 * If that fails, I fall back to the official NPL India time server. */
#define NTP_PRIMARY_HOST        "in.pool.ntp.org"
#define NTP_FALLBACK_HOST       "time.nplindia.org"

#define RX_TIMEOUT_MS           3000 /* 3 seconds is plenty of time */

/* ------------------------------------------------------------
 *  unix_to_ymdhms
 * ------------------------------------------------------------
 * This function converts a raw Unix timestamp (seconds since 1970)
 * into a broken-down struct (year, month, day, hour, min, sec).
 *
 * I definitely didn't invent this algorithm myself! It's from 
 * Howard Hinnant's "date" paper. It's magic, but it works perfectly
 * and avoids expensive loops or giant lookup tables.
 * ---------------------------------------------------------- */
static void unix_to_ymdhms(uint32_t unix_s,
                           uint16_t *year, uint8_t *mon, uint8_t *mday,
                           uint8_t  *hour, uint8_t *min,  uint8_t *sec)
{
    /* First, split the total seconds into whole days and remainder seconds */
    uint32_t days = unix_s / 86400UL;
    uint32_t secs = unix_s % 86400UL;

    /* Extract the time of day from the remainder seconds */
    *hour = secs / 3600;           secs %= 3600;
    *min  = secs / 60;
    *sec  = secs % 60;

    /* Now the crazy math to convert days-since-1970 into a calendar date.
     * I don't fully understand every line, but it correctly handles leap
     * years and the weird 400-year cycle rules. */
    int32_t z = (int32_t)days + 719468;           /* offset from 0000-03-01 */
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);
    uint32_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int32_t  y   = (int32_t)yoe + era * 400;
    uint32_t doy = doe - (365*yoe + yoe/4 - yoe/100);
    uint32_t mp  = (5*doy + 2)/153;
    uint32_t d   = doy - (153*mp + 2)/5 + 1;
    uint32_t m   = mp < 10 ? mp + 3 : mp - 9;
    
    /* Adjust the year if we're in Jan/Feb (because the math algorithm
     * shifts the start of the year to March 1st internally) */
    y += (m <= 2);

    /* Write the results back to the caller's pointers */
    *year = (uint16_t)y;
    *mon  = (uint8_t)m;
    *mday = (uint8_t)d;
}

/* ------------------------------------------------------------
 *  ntp_query
 * ------------------------------------------------------------
 * Performs the actual network transaction with a specific server.
 * Returns 0 on success, or -1 if anything goes wrong.
 * ---------------------------------------------------------- */
static int ntp_query(const char *host, uint32_t *out_unix_utc)
{
    int                s;
    struct addrinfo    hints, *res = NULL;
    struct sockaddr_in server;
    uint8_t            pkt[NTP_PKT_LEN];
    int                rc = -1; /* Assume failure until we succeed */

    /* ----- 1. DNS Resolution ------------------------------ */
    /* Set up the hints to tell getaddrinfo we want IPv4 UDP */
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    /* Do the lookup */
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || res == NULL)
    {
        uart_printf("[NTP ] DNS failed for %s\r\n", host);
        return -1;
    }

    /* Copy the resolved IP address into our server struct */
    memcpy(&server, res->ai_addr, sizeof server);
    server.sin_port = htons(NTP_PORT);
    freeaddrinfo(res); /* Clean up the linked list */

    /* Print out the IP we resolved so I can verify DNS is actually working */
    uart_printf("[NTP ] server %s -> %s\r\n",
                host, inet_ntoa(server.sin_addr));

    /* ----- 2. Create Socket & Set Timeout ----------------- */
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0)
    {
        uart_puts("[NTP ] socket() failed\r\n");
        return -1;
    }

    /* I don't want the task to hang forever if the packet gets lost,
     * so I set a 3 second receive timeout on the socket. */
    struct timeval tv = { .tv_sec = RX_TIMEOUT_MS/1000, .tv_usec = 0 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    /* ----- 3. Build Client Request Packet ----------------- */
    /* Start with a clean slate of 48 zero bytes */
    memset(pkt, 0, sizeof pkt);

    /* Set the very first byte:
     * LI (Leap Indicator) = 0 (no warning)
     * VN (Version Number) = 4
     * Mode = 3 (Client)
     * 00 100 011 in binary is 0x23 */
    pkt[0] = (0 << 6) | (4 << 3) | 3;

    /* ----- 4. Send and Receive ---------------------------- */
    /* Fire it off to the server */
    if (sendto(s, pkt, NTP_PKT_LEN, 0,
               (struct sockaddr *)&server, sizeof server) != NTP_PKT_LEN)
    {
        uart_puts("[NTP ] sendto() failed\r\n");
        goto done; /* Use goto to jump to the cleanup code at the end */
    }

    /* Wait for the reply. This will block until data arrives or we timeout. */
    int n = recv(s, pkt, sizeof pkt, 0);
    if (n != NTP_PKT_LEN)
    {
        uart_printf("[NTP ] recv() = %d (expected %d)\r\n", n, NTP_PKT_LEN);
        goto done;
    }

    /* ----- 5. Sanity-Check the Reply ---------------------- */
    /* We need to make sure the server didn't send us a Kiss-o'-Death (KoD)
     * packet telling us to go away, and that it's actually a server response. */
    uint8_t mode = pkt[0] & 0x07;
    uint8_t stratum = pkt[1];
    
    if (mode != 4 /* Not a server response */ || stratum == 0 /* KoD packet */)
    {
        uart_printf("[NTP ] bad reply mode=%u strat=%u\r\n", mode, stratum);
        goto done;
    }

    /* ----- 6. Extract the Timestamp ----------------------- */
    /* The Transmit Timestamp starts at byte 40.
     * It's sent in big-endian network byte order, so I have to shift and OR 
     * the 4 bytes together to reconstruct the 32-bit integer.
     * I'm totally ignoring the fractional seconds (bytes 44-47) because 
     * I don't care about millisecond precision for a speaking clock. */
    uint32_t secs_1900 =
          ((uint32_t)pkt[40] << 24)
        | ((uint32_t)pkt[41] << 16)
        | ((uint32_t)pkt[42] <<  8)
        | ((uint32_t)pkt[43]      );

    /* Quick sanity check: if the time is before 1970, something is very wrong */
    if (secs_1900 < NTP_EPOCH_OFFSET)
    {
        uart_puts("[NTP ] timestamp pre-1970, dropping\r\n");
        goto done;
    }

    /* Convert from the NTP epoch (1900) to the Unix epoch (1970) */
    *out_unix_utc = secs_1900 - NTP_EPOCH_OFFSET;
    
    /* We made it! Flag as success. */
    rc = 0;

done:
    /* Always close the socket before returning, otherwise we'll leak them
     * and eventually run out of memory. */
    closesocket(s);
    return rc;
}

/* ------------------------------------------------------------
 *  ntp_get_time
 * ------------------------------------------------------------
 * Public API called by the ntp_task. Wraps the query logic
 * and handles fallbacks and timezone conversion.
 * ---------------------------------------------------------- */
int ntp_get_time(time_msg_t *out)
{
    uint32_t unix_utc = 0;

    /* First, try to hit the primary NTP pool */
    if (ntp_query(NTP_PRIMARY_HOST, &unix_utc) != 0)
    {
        /* If the pool fails (maybe UDP packet dropped), try the fallback */
        uart_puts("[NTP ] primary pool failed, trying fallback\r\n");
        if (ntp_query(NTP_FALLBACK_HOST, &unix_utc) != 0)
        {
            /* Both failed. Tell the caller it's a network error. */
            out->status = TIME_STATUS_NET_FAIL;
            return -1;
        }
    }

    /* We have the UTC time. Now apply the timezone offset. 
     * India doesn't observe Daylight Saving Time, so this is just
     * a hardcoded constant addition. Nice and easy! */
    uint32_t unix_ist = unix_utc + IST_OFFSET_SEC;

    /* Break the raw seconds down into the human-readable struct */
    unix_to_ymdhms(unix_ist,
                   &out->year, &out->month, &out->day,
                   &out->hour, &out->minute, &out->second);
                   
    out->status = TIME_STATUS_OK;
    return 0;
}
