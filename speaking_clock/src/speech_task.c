/* ============================================================
 * speech_task.c
 * ------------------------------------------------------------
 * This task is the final stage of our pipeline. It's the 
 * consumer of the time_queue. Whenever the NTP task pushes a 
 * new time message onto the queue, this task wakes up and 
 * translates that time into a stream of speech tokens.
 *
 * I decided to output these tokens over USART2 (which QEMU maps
 * to stdout). I wrote a Python script (host_bridge/tts_bridge.py)
 * that listens to this output and pipes the tokens into eSpeak
 * so we can actually hear the time being spoken!
 *
 * IPC primitive used:
 *    - xQueueReceive: blocks until there's a new time to speak
 * ============================================================ */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stdio.h>
#include <stdarg.h>

#include "system.h"
#include "speech_tokens.h"

/* ============================================================
 *  Low-level token emitters
 * ------------------------------------------------------------
 * These are little helper functions I wrote to make the main 
 * speech logic cleaner. They just format the output exactly
 * how the Python bridge expects it.
 * ========================================================== */

/* Emits a standard word token like "TOKEN HOURS" */
void speech_emit_word(const char *word)
{
    /* The bridge expects "TOKEN <word>\r\n" */
    uart_puts("TOKEN ");
    uart_puts(word);
    uart_puts("\r\n");
}

/* Emits a two-digit number token, padding with a leading zero
 * if necessary (so 5 becomes "05"). */
void speech_emit_num2(uint8_t n)
{
    /* I created a small buffer to format the number manually.
     * This avoids needing the heavy, full-featured sprintf() from 
     * the standard library, which would eat up our precious SRAM. */
    char buf[16];
    
    /* Manual modulo math to extract the tens and units digits.
     * Adding '0' converts the raw integer to its ASCII character. */
    buf[0] = '0' + (n / 10) % 10;
    buf[1] = '0' + (n     ) % 10;
    buf[2] = '\0'; /* Null terminate the string! */

    /* Output the formatted number token */
    uart_puts("TOKEN ");
    uart_puts(buf);
    uart_puts("\r\n");
}

/* Tells the Python bridge we're done speaking the sentence */
void speech_emit_end(void)
{
    uart_puts("END\r\n");
}

/* ============================================================
 *  speak_time
 * ------------------------------------------------------------
 * Assembles the full utterance piece by piece.
 * The goal is to say: "The time is HH hours MM minutes and SS seconds"
 * ========================================================== */
void speak_time(const time_msg_t *m)
{
    /* First, check if the NTP task actually succeeded. 
     * If the network is down, we don't want to speak garbage time. */
    if (m->status != TIME_STATUS_OK)
    {
        /* Just say "network error" so the user knows what happened */
        speech_emit_word(TOK_NETWORK);
        speech_emit_word(TOK_ERROR);
        speech_emit_end();
        return; /* Bail out early */
    }

    /* We have a valid time, let's start the sentence */
    speech_emit_word(TOK_THE);
    speech_emit_word(TOK_TIME);
    speech_emit_word(TOK_IS);

    /* Output the hours */
    speech_emit_num2(m->hour);
    speech_emit_word(TOK_HOURS);

    /* Output the minutes */
    speech_emit_num2(m->minute);
    speech_emit_word(TOK_MINUTES);

    /* Output the "and" before the seconds to make it sound natural */
    speech_emit_word(TOK_AND);

    /* Output the seconds */
    speech_emit_num2(m->second);
    speech_emit_word(TOK_SECONDS);

    /* Tell the bridge to flush the audio buffer and play the sound */
    speech_emit_end();
}

/* ============================================================
 *  speech_task
 * ------------------------------------------------------------
 * The main loop for the task. It just sits and waits on the queue.
 * ========================================================== */
void speech_task(void *pv)
{
    /* Keep the compiler happy about the unused parameter */
    (void)pv;

    /* A local copy of the message to receive the data into */
    time_msg_t msg;

    for (;;)
    {
        /* Block forever until a message arrives in the queue.
         * The queue automatically manages the blocking/unblocking
         * so we don't waste CPU cycles polling it. */
        if (xQueueReceive(time_queue, &msg, portMAX_DELAY) == pdPASS)
        {
            /* Log that we received the data, mostly for debugging */
            uart_puts("[SPK ] emitting speech tokens\r\n");
            
            /* Pass the message to the helper function to do the
             * actual token emission */
            speak_time(&msg);
        }
    }
}
