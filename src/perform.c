/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* PERFORM's master effects (perform.h). The mix is written into a ring of about six seconds all the time; a repeat
 * loops the last 1/4 .. 1/32T of it, the tape reads it at its own rate (STOP slows to nothing, HALF half speed, REV
 * backwards, SCRATCH to and fro); either crossfades in and out over a few milliseconds. The filter is a low-pass and a
 * high-pass in series (state-variable, TPT), their cutoffs gliding to each pad's (a slide along the pads sweeps), open
 * again when let go, then out of the way. */
#include <math.h>
#include <string.h>

#include "perform.h"

#define SR 44100.0
#define RB (1u << 18)                   /* the ring: 262144 frames, 5.9 s */
#define XF 220                          /* the crossfade: 5 ms */
#define LP_OPEN 18000.0
#define HP_OPEN 20.0

static float g_ring[2][RB];
static uint32_t g_w;                    /* the next frame written */

/* wanted (the UI's), and as the audio side last took it */
static volatile int w_rep = -1, w_filt = -1, w_tape = -1, w_bpm = 120, w_gen;
static int a_rep = -1, a_tape = -1, a_gen = -1;

static int src;                         /* 0 live, 1 repeat, 2 tape */
static double x_live = 1.0;             /* the live share (1: live only) */
static uint32_t rep_start, rep_len, rep_ph;
static double tp_pos, tp_rate, tp_p0;   /* the tape's read head, its rate; SCRATCH's centre */
static uint32_t tp_n;                   /* frames since the tape began */
static double lp_hz = LP_OPEN, hp_hz = HP_OPEN, lp_t = LP_OPEN, hp_t = HP_OPEN;
static float lp_s[2][2], hp_s[2][2];    /* the filters' states (ic1, ic2) per channel */

static const double REP_BEATS[PF_REP_N] = { 1.0, 2.0 / 3, 0.5, 1.0 / 3, 0.25, 1.0 / 6, 0.125, 1.0 / 12 };
static const double F_LP[PF_FILT_N] = { 1200.0, 300.0, LP_OPEN, LP_OPEN };
static const double F_HP[PF_FILT_N] = { HP_OPEN, HP_OPEN, 300.0, 1500.0 };

void pf_set(int repeat, int filter, int tape, int bpm)
{
    if (repeat != w_rep || tape != w_tape)
        w_gen++;
    w_rep = repeat, w_filt = filter, w_tape = tape, w_bpm = bpm > 0 ? bpm : 120;
}

void pf_state(int *s, int *lp, int *hp)
{
    *s = src;
    *lp = (int)lp_hz;
    *hp = (int)hp_hz;
}

static float ring_at(int c, double pos)                 /* (linear between frames; pos counts frames, wraps) */
{
    const double f = floor(pos);
    const uint32_t i = (uint32_t)(int64_t)f & (RB - 1);
    const float a = g_ring[c][i], b = g_ring[c][(i + 1) & (RB - 1)], t = (float)(pos - f);
    return a + (b - a) * t;
}

static void take_wanted(void)
{
    const int gen = w_gen, rep = w_rep, tape = w_tape;
    if (gen == a_gen)
        return;
    a_gen = gen;
    if (tape >= 0 && (tape != a_tape || src != 2)) {    /* (the tape over the repeat: the newer wins in the UI) */
        src = 2;
        tp_n = 0;
        tp_pos = (double)g_w - 1;
        tp_rate = tape == PF_REV ? -1.0 : 1.0;
        tp_p0 = (double)g_w - 1 - 0.12 * SR;            /* (SCRATCH swings about a point just behind) */
    } else if (tape < 0 && rep >= 0 && (rep != a_rep || src != 1)) {
        double len = 60.0 / w_bpm * REP_BEATS[rep] * SR;
        rep_len = len < 64 ? 64u : (uint32_t)len;
        rep_start = g_w - rep_len;
        rep_ph = 0;
        src = 1;
    } else if (tape < 0 && rep < 0)
        src = 0;
    a_rep = rep, a_tape = tape;
}

static void svf_coef(double hz, float *g, float *k)
{
    *g = (float)tan(M_PI * (hz < 10 ? 10 : hz > 20000 ? 20000 : hz) / SR);
    *k = 0.8f;                                          /* (Q about 1.25: a little edge) */
}

void pf_process(int16_t *io, int frames)
{
    const double gl = 1.0 - exp(-1.0 / (0.12 * SR)) , xs = 1.0 / XF;
    int n, c;
    float lg = 0, lk = 0, hg = 0, hk = 0;
    int filt_on;
    take_wanted();
    {
        const int f = w_filt;
        lp_t = f >= 0 ? F_LP[f] : LP_OPEN;
        hp_t = f >= 0 ? F_HP[f] : HP_OPEN;
    }
    for (n = 0; n < frames; n++) {
        float in[2], out[2];
        in[0] = io[2 * n] / 32768.0f, in[1] = io[2 * n + 1] / 32768.0f;
        g_ring[0][g_w & (RB - 1)] = in[0], g_ring[1][g_w & (RB - 1)] = in[1];
        g_w++;
        x_live += src ? -xs : xs;                       /* (the crossfade to and from the ring) */
        x_live = x_live < 0 ? 0 : x_live > 1 ? 1 : x_live;
        if (x_live >= 1.0) {
            out[0] = in[0], out[1] = in[1];
        } else {
            float r[2];
            if (src == 1) {                             /* the repeat: the slice again, its ends faded */
                const uint32_t i = (rep_start + rep_ph) & (RB - 1);
                const uint32_t e = rep_len - rep_ph;
                const float env = rep_ph < 32 ? rep_ph / 32.0f : e < 32 ? e / 32.0f : 1.0f;
                r[0] = g_ring[0][i] * env, r[1] = g_ring[1][i] * env;
                if (++rep_ph >= rep_len)
                    rep_ph = 0;
            } else if (src == 2) {                      /* the tape */
                const int m = a_tape;
                if (m == PF_STOP)
                    tp_rate = tp_rate > 0 ? tp_rate - 1.0 / (0.7 * SR) : 0;
                else if (m == PF_HALF)
                    tp_rate += (0.5 - tp_rate) * (1.0 / (0.08 * SR));
                if (m == PF_SCRATCH)
                    tp_pos = tp_p0 + 0.1 * SR * sin(2 * M_PI * 2.2 * tp_n / SR);
                else
                    tp_pos += tp_rate;
                tp_n++;
                r[0] = ring_at(0, tp_pos), r[1] = ring_at(1, tp_pos);
            } else {                                    /* (fading back to live: the ring's last place held) */
                r[0] = r[1] = 0;
            }
            out[0] = (float)(in[0] * x_live + r[0] * (1 - x_live));
            out[1] = (float)(in[1] * x_live + r[1] * (1 - x_live));
        }
        if ((n & 31) == 0) {                            /* the cutoffs glide (a sweep), the coefficients each 32 */
            lp_hz = exp(log(lp_hz) + (log(lp_t) - log(lp_hz)) * gl * 32);
            hp_hz = exp(log(hp_hz) + (log(hp_t) - log(hp_hz)) * gl * 32);
            svf_coef(lp_hz, &lg, &lk);
            svf_coef(hp_hz, &hg, &hk);
        }
        filt_on = lp_hz < LP_OPEN - 50 || hp_hz > HP_OPEN + 2;
        for (c = 0; c < 2; c++) {
            float x = out[c];
            if (filt_on) {
                {   /* the low-pass */
                    float *s = lp_s[c];
                    const float a1 = 1.0f / (1.0f + lg * (lg + lk)), a2 = lg * a1, a3 = lg * a2;
                    const float v3 = x - s[1], v1 = a1 * s[0] + a2 * v3, v2 = s[1] + a2 * s[0] + a3 * v3;
                    s[0] = 2 * v1 - s[0], s[1] = 2 * v2 - s[1];
                    x = v2;
                }
                {   /* the high-pass */
                    float *s = hp_s[c];
                    const float a1 = 1.0f / (1.0f + hg * (hg + hk)), a2 = hg * a1, a3 = hg * a2;
                    const float v3 = x - s[1], v1 = a1 * s[0] + a2 * v3, v2 = s[1] + a2 * s[0] + a3 * v3;
                    s[0] = 2 * v1 - s[0], s[1] = 2 * v2 - s[1];
                    x = x - hk * v1 - v2;
                }
            } else
                lp_s[c][0] = lp_s[c][1] = hp_s[c][0] = hp_s[c][1] = 0;
            x = x > 1 ? 1 : x < -1 ? -1 : x;
            io[2 * n + c] = (int16_t)(x * 32767.0f);
        }
    }
}
