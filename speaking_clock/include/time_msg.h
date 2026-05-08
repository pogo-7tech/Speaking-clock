/* ============================================================
 * time_msg.h
 * ------------------------------------------------------------
 * Shared data structure passed through the FreeRTOS queue
 * from ntp_task  ->  speech_task.
 *
 * Fields are already normalised to the local wall-clock
 * (Indian Standard Time, UTC+5:30).
 * ============================================================ */

#ifndef TIME_MSG_H
#define TIME_MSG_H

#include <stdint.h>

typedef enum {
    TIME_STATUS_OK        = 0,   /* Valid reading from NTP server          */
    TIME_STATUS_NET_FAIL  = 1,   /* lwIP / socket / DNS error              */
    TIME_STATUS_NTP_FAIL  = 2    /* Packet received but invalid / KoD etc. */
} time_status_t;

typedef struct {
    uint8_t        hour;     /* 0 .. 23 in IST   */
    uint8_t        minute;   /* 0 .. 59          */
    uint8_t        second;   /* 0 .. 59          */
    uint8_t        day;      /* 1 .. 31          */
    uint8_t        month;    /* 1 .. 12          */
    uint16_t       year;     /* e.g. 2026        */
    time_status_t  status;
} time_msg_t;

#endif /* TIME_MSG_H */
