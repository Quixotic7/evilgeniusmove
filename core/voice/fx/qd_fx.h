/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius's effects: the retro set, eight rooms (qd_fx.c), then FIZZ's output (fz_out.c: the trim, the DC blocker,
 * the look-ahead limiter, MASTER).
 *   the three sends (each track's, lockable on a step, as FIZZ's):  CHORUS (the PAL chorus), ECHO (a tempo echo whose
 *                                                                    repeats darken and crumble), ROOM (a small reverb)
 *   then on the whole mix, in order:                                 CRUSH (bits, sample rate), ROBOT (a ring
 *                                                                    modulator), WARP (a comb / flanger), GLITCH (a
 *                                                                    beat-repeat in tempo), TV (a small speaker, the
 *                                                                    mains' hum, a little drive)
 * One instance, one control block (32 frames) a call. Integer DSP but for the coefficients a block works out: two runs
 * give the same samples. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QD_FX_CTL 32

void qd_fx_init(void);
void qd_fx_tempo_fx(int bpm);
void qd_fx_tempo(int bpm);
void qd_fx_master_pot(int pot);
uint32_t qd_fx_master_q12(void);
/* the effects' parameters (global: no sound or pattern load touches them; the SET record keeps them) */
enum {
    /* ECHO (the delay send): TIME free (1..127: 10 ms .. 1 s), DIV synced, SPREAD free / RATIO synced (the right side
     * later), FEEDBACK, SYNC, MODE (STEREO / PING-PONG), LOFI (the repeats darker and coarser), WOW */
    QD_FXP_DTIME, QD_FXP_DDIV, QD_FXP_DSPREAD, QD_FXP_DRATIO, QD_FXP_DFDBK, QD_FXP_DSYNC, QD_FXP_DMODE,
    QD_FXP_DTONE, QD_FXP_DWOW,
    /* ROOM (the reverb send): SIZE, DAMP, DECAY (127: held), PRE-DELAY, MOD, LOW CUT, WIDTH */
    QD_FXP_RSIZE, QD_FXP_RDAMP, QD_FXP_RDECAY, QD_FXP_RPRE, QD_FXP_RMOD, QD_FXP_RLOW, QD_FXP_RWIDTH,
    /* CHORUS (the chorus send): WIDTH, RATE, DEPTH */
    QD_FXP_CWIDTH, QD_FXP_CRATE, QD_FXP_CDEPTH,
    /* (FIZZ's routing: one path here, PARALLEL; kept as values for the stored set) */
    QD_FXP_RMODE, QD_FXP_ORDER, QD_FXP_RDRY, QD_FXP_RSPARE1, QD_FXP_RSPARE2,
    /* the rooms on the mix: each MIX 0 is off */
    QD_FXP_XBITS, QD_FXP_XRATE, QD_FXP_XJIT, QD_FXP_XMIX,          /* CRUSH: bits 1..16, the rate, jitter, mix */
    QD_FXP_RBFREQ, QD_FXP_RBWAVE, QD_FXP_RBLFO, QD_FXP_RBMIX,      /* ROBOT: the carrier, its wave, its wobble, mix */
    QD_FXP_WTIME, QD_FXP_WFB, QD_FXP_WRATE, QD_FXP_WMIX,           /* WARP: the comb's time, feedback (+-), sweep, mix */
    QD_FXP_GLEN, QD_FXP_GCHNC, QD_FXP_GPTCH, QD_FXP_GMIX,          /* GLITCH: the slice, chance, pitch a repeat, mix */
    QD_FXP_TVTONE, QD_FXP_TVHUM, QD_FXP_TVDRV, QD_FXP_TVMIX,       /* TV: the speaker's tone, the hum, drive, mix */
    QD_NFXP
};
enum { QD_RT_CHORUS, QD_RT_DELAY, QD_RT_REVERB, QD_RT_FILTER, QD_RT_N };
int qd_fx_route_block(int pos);
void qd_fx_chain_g(const int32_t g[3]);
int qd_fx_route_move(int block, int dir);
int qd_fx_get(int id);
void qd_fx_set(int id, int v);
void qd_fx_range(int id, int *min, int *max, int *def);
/* EvilGenius: the five rooms (CRUSH ROBOT WARP GLITCH TV) each work on their own bus now, the tracks' sends into
 * it (mix.h room), not on the whole mix: a and b are the two halves' buses for the next qd_fx_block (five rows of
 * QD_FX_CTL samples each, >> 4; 0: none). A room's output is added to the mix whole (its MIX value is no longer
 * read: how much of a track goes through a room is that track's send) */
void qd_fx_rooms(const int16_t (*a)[QD_FX_CTL], const int16_t (*b)[QD_FX_CTL]);
const char *qd_fx_text(int id, int v, char *buf);   /* buf 16 bytes */
const char *qd_fx_name(int id);         /* a value's label ("BITS", "FREQ") */
int qd_fx_delay_ms(void);
int qd_fx_chorus_hz100(void);
int qd_fx_reverb_ms(void);
/* what the screens draw: the glitch repeating (and how far into its slice, 0..1), the crush's held level */
int qd_fx_glitch_on(float *pos);
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out);

#ifdef __cplusplus
}
#endif
