/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* PERFORM's master effects (design/WORKFLOW.md "FX mode and PERFORM"): beat repeat, the filter sweeps and the tape
 * moves, on the whole mix after the core's output. The UI sets what is wanted (pf_set); the audio side follows it at
 * its next block, crossfading in and out. The throws are the core's (app.h qd_app_t.thr), not here. */
#ifndef EGM_PERFORM_H
#define EGM_PERFORM_H
#include <stdint.h>

enum { PF_REP_N = 8 };                  /* 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T */
enum { PF_LP1, PF_LP2, PF_HP1, PF_HP2, PF_FILT_N };
enum { PF_STOP, PF_HALF, PF_REV, PF_SCRATCH, PF_TAPE_N };

/* what is wanted: the repeat (-1 none, 0..7), the filter (-1 none, PF_LP1..), the tape (-1 none, PF_STOP..), the tempo
 * (the repeat's length). A new repeat or tape value starts it again from now */
void pf_set(int repeat, int filter, int tape, int bpm);
void pf_process(int16_t *stereo, int frames);
/* the state as the audio side has it (tests): the source (0 live, 1 repeat, 2 tape), the filter's cutoffs (Hz) */
void pf_state(int *src, int *lp_hz, int *hp_hz);
/* the count-in (SEQ > TIME): beats clicks, spb samples apart (the first of every four accented), at level 0..100;
 * pf_countin_left: the beats still to click (0: done, -1: none running); cancel: stopped */
void pf_countin(int beats, int spb, int level);
int pf_countin_left(void);
void pf_countin_cancel(void);

#endif
