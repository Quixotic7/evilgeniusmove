/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: SAY, SAM speaking live. A patch's spoken word is its SAY word (1..999: a random word, FIZZ's name
 * generator's, the same for the same value on every build) or, with a voice other than SAM's own, its WORD; SAM
 * renders it in that voice (SPEED, PITCH, THROAT, MOUTH) into a cache of QD_SAY_SLOTS words, 4-bit at QD_VOX_FS like
 * the stored words, so VOX plays it the same way. SAM runs only in the app's free time (qd_say_want, never in the
 * audio); the voices only look the word up (qd_say_get): a word not rendered yet is the stored one (WORD), or silence
 * (a SAY word). */
#pragma once
#include <stdint.h>

#define QD_VOX_FS 6000u                 /* the words' rate (eg_vox_data.h VOX_FS) */
#define QD_SAY_MAX 999                  /* SAY's values: 1..999 */

int qd_say_text(int seed, char *out);   /* SAY's word for seed (out: 13 bytes); 0: none (seed 0, OFF) */
uint32_t qd_say_key(const int16_t *p);  /* the patch's live word and voice (0: none live: the stored word, or no word) */
int qd_say_default_voice(const int16_t *p);
int qd_say_want(const int16_t *p);      /* the app (not the audio): render the patch's live word if it isn't cached;
                                         * 1: it rendered one (SAM's time spent) */
const uint8_t *qd_say_get(uint32_t key, uint32_t *len);   /* the audio: the cached word (two samples a byte, the
                                                           * high nibble first) and its samples, or 0 */
int qd_say_touch(const int16_t *p);     /* the app: p's word is wanted (kept if it is rendered); 1: it is rendered */
void qd_say_pass(void);                 /* the app: a look at every word wanted begins (words wanted since stay) */
void qd_say_reset(void);                /* the cache empty (a new app) */

/* EvilGenius: THE VOICE, the villain speaking over the mix (the confirmations' words, his remarks). SAM renders the
 * line into its own 22050 Hz buffer (already 4-bit: its samples are 16 levels) and the audio plays it from there, each
 * sample twice: no memory of its own. A track's word that SAM must render (qd_say_want) cuts the line: the music first. */
enum { QD_ANN_VILLAIN, QD_ANN_ROBOT };  /* the voices: the villain's low one, the sentries' */
int qd_ann_say(const char *text, int voice, int level);   /* the app (not the audio): speak text (English, upper case)
                                                           * at level (0..100 %); 1: it speaks */
void qd_ann_stop(void);
int qd_ann_busy(void);
void qd_ann_mix(int32_t *l, int32_t *r, int n);   /* the audio: the line's next n samples added to the mix */
