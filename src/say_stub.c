/* SPDX-License-Identifier: MPL-2.0 */
/* The core's speech (core/voice/sam/say.h) with no speech synthesizer behind it: for a build made with NO_SAM=1.
 * SAM, the Software Automatic Mouth (core/voice/sam/), has no licence (the core's LICENSING.md), so a build meant to be
 * passed on can leave it out. What goes with it: a sound's SAY (its live word), and the villain's and the lift's
 * spoken lines. The stored words (VOX's WORD) are data in the core and still play. */
#include <stdint.h>

#include "say.h"

int qd_say_text(int seed, char *out) { (void)seed; out[0] = 0; return 0; }
uint32_t qd_say_key(const int16_t *p) { (void)p; return 0; }
int qd_say_default_voice(const int16_t *p) { (void)p; return 0; }
int qd_say_want(const int16_t *p) { (void)p; return 0; }
const uint8_t *qd_say_get(uint32_t key, uint32_t *len) { (void)key; if (len) *len = 0; return 0; }
int qd_say_touch(const int16_t *p) { (void)p; return 0; }
void qd_say_pass(void) {}
void qd_say_reset(void) {}
int qd_ann_say(const char *text, int voice, int level) { (void)text; (void)voice; (void)level; return 0; }
void qd_ann_stop(void) {}
int qd_ann_busy(void) { return 0; }
void qd_ann_mix(int32_t *l, int32_t *r, int n) { (void)l; (void)r; (void)n; }
