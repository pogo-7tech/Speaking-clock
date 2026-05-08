/* ============================================================
 * speech_tokens.h
 * ------------------------------------------------------------
 * The firmware does NOT run a TTS engine.  It emits a tiny
 * text-based token protocol on the UART.  A Python bridge
 * running on the host reads these tokens and feeds them to a
 * speech synthesiser (eSpeak).
 *
 * Every token line has the form
 *        TOKEN <word_or_number>\n
 * and the whole utterance is terminated with
 *        END\n
 *
 * Example utterance for 14:32:05
 *        TOKEN THE
 *        TOKEN TIME
 *        TOKEN IS
 *        TOKEN 14
 *        TOKEN HOURS
 *        TOKEN 32
 *        TOKEN MINUTES
 *        TOKEN AND
 *        TOKEN 05
 *        TOKEN SECONDS
 *        END
 * ============================================================ */

#ifndef SPEECH_TOKENS_H
#define SPEECH_TOKENS_H

#include "time_msg.h"

/* ---- Fixed-vocabulary words -------------------------------- */
#define TOK_THE       "THE"
#define TOK_TIME      "TIME"
#define TOK_IS        "IS"
#define TOK_HOURS     "HOURS"
#define TOK_MINUTES   "MINUTES"
#define TOK_SECONDS   "SECONDS"
#define TOK_AND       "AND"
#define TOK_ERROR     "ERROR"
#define TOK_NETWORK   "NETWORK"

/* ---- Emit helpers ------------------------------------------ */
/*
 * Send a single fixed-vocabulary word.
 *      speech_emit_word("HOURS");
 */
void speech_emit_word(const char *word);

/*
 * Send a zero-padded 2-digit number (00 .. 99).
 *      speech_emit_num2(7)  ->  "TOKEN 07\n"
 */
void speech_emit_num2(uint8_t n);

/* Marker that ends the current utterance. */
void speech_emit_end(void);

/*
 * Top-level helper used by the speech task: produces the full
 * "The time is HH hours MM minutes and SS seconds" utterance.
 */
void speak_time(const time_msg_t *msg);

#endif /* SPEECH_TOKENS_H */
