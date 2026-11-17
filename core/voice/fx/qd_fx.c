/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius's effects (qd_fx.h): the retro set. The three sends: CHORUS (the PAL chorus: two taps swaying apart),
 * ECHO (a ping-pong tempo echo at half rate whose repeats are darkened, coarsened and wobbled by LOFI and WOW), ROOM
 * (a small reverb at half rate: four combs and two allpasses a side). Then the mix through CRUSH, ROBOT, WARP, GLITCH,
 * TV, then FIZZ's output (fz_out.c). The levels are the mix's (a voice's full scale near 24000). */
#include <stdint.h>
#include <string.h>

#include "qd_fx.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-macros"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define __attribute__(x)                /* (the buffers: zero-initialised, the link puts them in the pool) */
#include "fz_tables.h"
#include "fz_core.h"
#include "cr_ram.h"
#include "fz_voice.h"
#include "fz_out.c"
#undef __attribute__
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define QD_FX_POT_DEFAULT 724
#define N QD_FX_CTL
#define H (N / 2)                       /* a block at the half rate */
typedef char qd_fx_ctl_ok[QD_FX_CTL == CTL ? 1 : -1];

/* ---------------------------------------------------------------- the values --- */
typedef struct {
    const char *name;
    int16_t min, max, def;
    const char *const *list;
} fxv_t;
static const char *const L_OFFON[2] = {"OFF", "ON"};
static const char *const L_DMODE[2] = {"STEREO", "PING"};
static const char *const L_DIV[12] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4",
                                      "1/2T", "1/4D", "1/2"};
static const char *const L_RATIO[4] = {"1:1", "3:4", "2:3", "1:2"};
static const char *const L_XRATE[8] = {"44K", "22K", "15K", "11K", "7K", "5K", "4K", "3K"};
static const char *const L_RBWAVE[3] = {"SINE", "SQUARE", "TRI"};
static const char *const L_GLEN[4] = {"1/32", "1/16", "1/8", "1/4"};
static const char *const L_RMODE[1] = {"PARALLEL"};
static const fxv_t FXV[QD_NFXP] = {
    [QD_FXP_DTIME] = {"TIME", 1, 127, 40, 0}, [QD_FXP_DDIV] = {"TIME", 0, 11, 7, L_DIV},
    [QD_FXP_DSPREAD] = {"SPREAD", 0, 127, 0, 0}, [QD_FXP_DRATIO] = {"RATIO", 0, 3, 0, L_RATIO},
    [QD_FXP_DFDBK] = {"FDBK", 0, 127, 64, 0}, [QD_FXP_DSYNC] = {"SYNC", 0, 1, 1, L_OFFON},
    [QD_FXP_DMODE] = {"MODE", 0, 1, 1, L_DMODE}, [QD_FXP_DTONE] = {"LOFI", 0, 127, 50, 0},
    [QD_FXP_DWOW] = {"WOW", 0, 127, 20, 0},
    [QD_FXP_RSIZE] = {"SIZE", 0, 127, 70, 0}, [QD_FXP_RDAMP] = {"DAMP", 0, 127, 60, 0},
    [QD_FXP_RDECAY] = {"DECAY", 0, 127, 70, 0}, [QD_FXP_RPRE] = {"PRE", 0, 127, 12, 0},
    [QD_FXP_RMOD] = {"MOD", 0, 127, 20, 0}, [QD_FXP_RLOW] = {"LOW CUT", 0, 127, 20, 0},
    [QD_FXP_RWIDTH] = {"WIDTH", 0, 127, 100, 0},
    [QD_FXP_CWIDTH] = {"WIDTH", 0, 127, 127, 0}, [QD_FXP_CRATE] = {"RATE", 0, 127, 30, 0},
    [QD_FXP_CDEPTH] = {"DEPTH", 0, 127, 60, 0},
    [QD_FXP_RMODE] = {"MODE", 0, 0, 0, L_RMODE}, [QD_FXP_ORDER] = {"ORDER", 0, 23, 0, 0},
    [QD_FXP_RDRY] = {"DRY", 0, 127, 127, 0}, [QD_FXP_RSPARE1] = {"", 0, 127, 64, 0},
    [QD_FXP_RSPARE2] = {"", 0, 127, 64, 0},
    [QD_FXP_XBITS] = {"BITS", 1, 16, 6, 0}, [QD_FXP_XRATE] = {"RATE", 0, 7, 3, L_XRATE},
    [QD_FXP_XJIT] = {"JITTER", 0, 127, 0, 0}, [QD_FXP_XMIX] = {"MIX", 0, 127, 0, 0},
    [QD_FXP_RBFREQ] = {"FREQ", 0, 127, 60, 0}, [QD_FXP_RBWAVE] = {"WAVE", 0, 2, 0, L_RBWAVE},
    [QD_FXP_RBLFO] = {"WOBBLE", 0, 127, 10, 0}, [QD_FXP_RBMIX] = {"MIX", 0, 127, 0, 0},
    [QD_FXP_WTIME] = {"TIME", 0, 127, 40, 0}, [QD_FXP_WFB] = {"FDBK", 0, 127, 100, 0},
    [QD_FXP_WRATE] = {"SWEEP", 0, 127, 10, 0}, [QD_FXP_WMIX] = {"MIX", 0, 127, 0, 0},
    [QD_FXP_GLEN] = {"SLICE", 0, 3, 1, L_GLEN}, [QD_FXP_GCHNC] = {"CHANCE", 0, 127, 32, 0},
    [QD_FXP_GPTCH] = {"PITCH", 0, 24, 12, 0}, [QD_FXP_GMIX] = {"MIX", 0, 127, 0, 0},
    [QD_FXP_TVTONE] = {"TONE", 0, 127, 50, 0}, [QD_FXP_TVHUM] = {"HUM", 0, 127, 16, 0},
    [QD_FXP_TVDRV] = {"DRIVE", 0, 127, 24, 0}, [QD_FXP_TVMIX] = {"MIX", 0, 127, 0, 0},
};
static int16_t P[QD_NFXP];
static int fx_bpm = 120;

int qd_fx_get(int id) { return id >= 0 && id < QD_NFXP ? P[id] : 0; }
void qd_fx_set(int id, int v)
{
    if (id < 0 || id >= QD_NFXP)
        return;
    P[id] = (int16_t)(v < FXV[id].min ? FXV[id].min : v > FXV[id].max ? FXV[id].max : v);
}
void qd_fx_range(int id, int *min, int *max, int *def)
{
    const fxv_t z = {"", 0, 0, 0, 0}, *v = id >= 0 && id < QD_NFXP ? &FXV[id] : &z;
    if (min)
        *min = v->min;
    if (max)
        *max = v->max;
    if (def)
        *def = v->def;
}
const char *qd_fx_name(int id) { return id >= 0 && id < QD_NFXP ? FXV[id].name : ""; }

static char *itoa_(char *b, int n)
{
    char t[12];
    int i = 0, k = 0;
    unsigned u = n < 0 ? 0u - (unsigned)n : (unsigned)n;
    do
        t[i++] = (char)('0' + u % 10u);
    while ((u /= 10u) && i < 11);
    if (n < 0)
        b[k++] = '-';
    while (i)
        b[k++] = t[--i];
    b[k] = 0;
    return b;
}

/* ---------------------------------------------------------------- shared helpers --- */
static float ex2(float x)
{
    int i = (int)(x >= 0.f ? x : x - 1.f);
    float f = x - (float)i, r;
    union { float f; uint32_t u; } u;
    r = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    if (i < -126)
        return 0.f;
    if (i > 127)
        i = 127;
    u.f = r;
    u.u += (uint32_t)i << 23;
    return u.f;
}
static int32_t sat16(int32_t x) { return x < -32767 ? -32767 : x > 32767 ? 32767 : x; }
static int32_t tri_q15(uint32_t ph)                     /* a triangle, -32767..32767 */
{
    const int32_t u = (int32_t)(ph >> 17);
    return u < 16384 ? u * 4 - 32767 : 32767 - (u - 16384) * 4;
}
static uint32_t rng = 0x2545F491u;
static uint32_t rnd(void)
{
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}
/* a division's length in samples at the tempo */
static uint32_t div_samples(int div, uint32_t fs)
{
    static const uint16_t Q16THS[12] = {2, 3, 4, 5, 6, 8, 11, 12, 16, 21, 24, 32};   /* in 32nds of a quarter (x 8) */
    const uint32_t q = fs * 60u / (uint32_t)(fx_bpm > 0 ? fx_bpm : 120);           /* a quarter */
    return q * Q16THS[div < 0 ? 0 : div > 11 ? 11 : div] / 16u;
}

/* the five rooms' buses for the next block (qd_fx_rooms), and each room's tail (blocks left to run) */
static const int16_t (*rm_a)[N], (*rm_b)[N];
static uint8_t rm_live[5];

/* ---------------------------------------------------------------- the lines' storage ---
 * The FM-1's memory decides how the delay lines are kept (the same on every build, so RemoteFM1 sounds as the unit):
 * the short ones 16-bit, the long ones (ECHO, GLITCH) a byte a sample, companded: the byte is the sample's square
 * root (sign kept), so a quiet tail keeps its detail (a step is ~1 % of the level, not a fixed 1 / 256 of full
 * scale) and the loud end is coarser, as the tape-and-chip echoes of the time were */
static int8_t c8_put(int32_t v)                         /* v within +-32767 (seven integer steps: no square root) */
{
    const int32_t a = v < 0 ? -v : v;
    int lo = 0, hi = 127;
    while (lo < hi) {
        const int mid = (lo + hi + 1) >> 1;
        if (((int32_t)mid * mid * 8321) >> 12 <= a)
            lo = mid;
        else
            hi = mid - 1;
    }
    return (int8_t)(v < 0 ? -lo : lo);
}
static int32_t c8_get(int8_t c) { return c < 0 ? -(((int32_t)c * c * 8321) >> 12) : ((int32_t)c * c * 8321) >> 12; }

/* ---------------------------------------------------------------- CHORUS --- */
#define CH_LEN 1024u                    /* (the taps reach 690 samples back) */
static int16_t ch_buf[CH_LEN];
static uint32_t ch_w, ch_ph;
static void chorus(const int32_t *in, int32_t *l, int32_t *r)
{
    const float hz = 0.05f * ex2((float)P[QD_FXP_CRATE] * (8.f / 127.f));
    const uint32_t inc = (uint32_t)(hz / 44100.f * 4294967296.f);
    const int32_t depth = 8 + P[QD_FXP_CDEPTH] * 3;                 /* samples (Q0): to ~390 (8.8 ms) */
    const uint32_t off = (uint32_t)P[QD_FXP_CWIDTH] << 24;           /* the right tap's phase: up to half a turn */
    int i, any = 0;
    for (i = 0; i < N; i++)
        any |= in[i];
    for (i = 0; i < N; i++) {
        int k;
        ch_buf[ch_w & (CH_LEN - 1u)] = (int16_t)sat16(in[i]);
        for (k = 0; k < 2; k++) {
            const int32_t m = tri_q15(ch_ph + (k ? off : 0u));
            const int32_t d16 = (300 << 16) + (int32_t)(((int64_t)m * depth) << 1);   /* Q16 samples back */
            const uint32_t rp = ch_w - (uint32_t)(d16 >> 16), fr = (uint32_t)d16 & 0xFFFFu;
            const int32_t a = ch_buf[rp & (CH_LEN - 1u)], b = ch_buf[(rp - 1u) & (CH_LEN - 1u)];
            const int32_t y = a + (int32_t)(((int64_t)(b - a) * fr) >> 16);
            if (k)
                r[i] += y;
            else
                l[i] += y;
        }
        ch_w++;
        ch_ph += inc;
    }
    (void)any;
}

/* ---------------------------------------------------------------- ECHO (half rate) --- */
#define EC_LEN 12680u                   /* 0.575 s at 22.05 kHz, a side (a quarter note down to 104 BPM) */
static int8_t ec_buf[2][EC_LEN];        /* (companded: c8_put) */
static uint32_t ec_w;
static int32_t ec_lp[2], ec_hold[2], ec_prev[2];
static uint32_t ec_ph, ec_cnt;
static uint32_t echo_len(int side)
{
    uint32_t n;
    if (P[QD_FXP_DSYNC]) {
        static const uint8_t RAT[4][2] = {{1, 1}, {4, 3}, {3, 2}, {2, 1}};
        n = div_samples(P[QD_FXP_DDIV], 22050u);
        if (side)
            n = n * RAT[P[QD_FXP_DRATIO] & 3][0] / RAT[P[QD_FXP_DRATIO] & 3][1];
    } else {
        n = (uint32_t)(220 + P[QD_FXP_DTIME] * 98);                  /* 10 ms .. 0.57 s */
        if (side)
            n += n * (uint32_t)P[QD_FXP_DSPREAD] / 254u;
    }
    return n < 64u ? 64u : n > EC_LEN - 64u ? EC_LEN - 64u : n;
}
static void echo(const int32_t *in, int32_t *l, int32_t *r)
{
    const int32_t fb = P[QD_FXP_DFDBK] * 245;                         /* Q15: to 0.95 */
    const int lofi = P[QD_FXP_DTONE];
    const int32_t lpk = 32767 - lofi * 220;                          /* the repeats' low-pass (Q15) */
    const int bits = 16 - lofi * 9 / 127;                            /* .. their bits (16 .. 7) */
    const uint32_t hold = 1u + (uint32_t)lofi / 40u;                  /* .. their sample-and-hold (1 .. 4) */
    const uint32_t wow = (uint32_t)P[QD_FXP_DWOW] * 2u;               /* samples of wobble (to ~11 ms) */
    const int ping = P[QD_FXP_DMODE];
    const uint32_t len[2] = {echo_len(0), echo_len(1)};
    int i, s;
    for (i = 0; i < H; i++) {
        const int32_t x = (in[2 * i] + in[2 * i + 1]) >> 1;
        int32_t y[2];
        const int32_t wm = (int32_t)(((int64_t)tri_q15(ec_ph) * (int32_t)wow) >> 15);
        ec_ph += 136000u;                                            /* (~0.7 Hz at 22.05 kHz) */
        for (s = 0; s < 2; s++) {
            const uint32_t rp = ec_w + EC_LEN - len[s] + (uint32_t)wm;
            int32_t v = c8_get(ec_buf[s][rp % EC_LEN]) << 1;
            ec_lp[s] += (int32_t)(((int64_t)(v - ec_lp[s]) * lpk) >> 15);
            y[s] = ec_lp[s];
        }
        if (++ec_cnt >= hold) {                                      /* LOFI: the repeats coarser */
            ec_cnt = 0;
            for (s = 0; s < 2; s++)
                ec_hold[s] = bits < 16 ? (y[s] >> (16 - bits)) << (16 - bits) : y[s];
        }
        for (s = 0; s < 2; s++) {
            const int32_t back = (int32_t)(((int64_t)ec_hold[s ^ (ping ? 1 : 0)] * fb) >> 15);
            const int32_t inp = ping ? (s ? 0 : x) : x;
            ec_buf[s][ec_w % EC_LEN] = c8_put(sat16((inp + back) >> 1));
        }
        ec_w++;
        for (s = 0; s < 2; s++) {                                    /* up to the full rate: a step and its average */
            int32_t *o = s ? r : l;
            o[2 * i] += (ec_prev[s] + ec_hold[s]) >> 1;
            o[2 * i + 1] += ec_hold[s];
            ec_prev[s] = ec_hold[s];
        }
    }
}

/* ---------------------------------------------------------------- ROOM (half rate) --- */
static const uint16_t RC_LEN[4] = {558, 594, 638, 678};               /* Freeverb's combs at 22.05 kHz */
static const uint16_t RA_LEN[2] = {278, 220};
#define RC_MAX 1100u
#define RP_LEN 512u
static int16_t rc_buf[2][4][RC_MAX];
#define RA_LEN_MAX 300u
static int16_t ra_buf[2][2][RA_LEN_MAX + 1u];
static uint32_t rc_w[2][4], ra_w[2][2];
static int32_t rc_lp[2][4], rm_hp[2], rm_hx[2], rm_prev[2];
static int16_t rp_buf[RP_LEN];
static uint32_t rp_w, rm_ph;
static void room(const int32_t *in, int32_t *l, int32_t *r)
{
    const int32_t g = P[QD_FXP_RDECAY] >= 127 ? 32700 : 22000 + P[QD_FXP_RDECAY] * 82;   /* Q15 feedback */
    const int32_t damp = P[QD_FXP_RDAMP] * 200;                    /* Q15 */
    const uint32_t pre = (uint32_t)P[QD_FXP_RPRE] * 4u;             /* to ~23 ms */
    const int32_t sz = 20000 + P[QD_FXP_RSIZE] * 360;               /* Q15 length scale (0.61 .. 2.0) */
    const int32_t wid = P[QD_FXP_RWIDTH] * 258, hpk = 200 + P[QD_FXP_RLOW] * 40;
    const uint32_t mod = (uint32_t)P[QD_FXP_RMOD] / 16u;
    int i, s, k;
    for (i = 0; i < H; i++) {
        const int32_t x = (in[2 * i] + in[2 * i + 1]) >> 2;
        int32_t y[2], xd;
        rp_buf[rp_w & (RP_LEN - 1u)] = (int16_t)sat16(x);
        xd = rp_buf[(rp_w - pre) & (RP_LEN - 1u)];
        rp_w++;
        rm_ph += 120000u;
        for (s = 0; s < 2; s++) {
            int32_t acc = 0;
            for (k = 0; k < 4; k++) {
                uint32_t n = (uint32_t)(((int32_t)RC_LEN[k] + s * 23) * sz >> 15);
                int32_t v;
                if (k == 0 && mod)
                    n += (uint32_t)((tri_q15(rm_ph) * (int32_t)mod) >> 15) + mod;
                n = n < 32u ? 32u : n > RC_MAX - 1u ? RC_MAX - 1u : n;
                v = rc_buf[s][k][(rc_w[s][k] + RC_MAX - n) % RC_MAX];
                rc_lp[s][k] += (int32_t)(((int64_t)(v - rc_lp[s][k]) * (32767 - damp)) >> 15);
                rc_buf[s][k][rc_w[s][k] % RC_MAX] = (int16_t)sat16(xd + (int32_t)(((int64_t)rc_lp[s][k] * g) >> 15));
                rc_w[s][k]++;
                acc += v;
            }
            acc >>= 2;
            for (k = 0; k < 2; k++) {                                /* two allpasses (0.5) */
                const uint32_t n = RA_LEN[k] + (uint32_t)s * 11u;
                const int32_t bv = ra_buf[s][k][(ra_w[s][k] + RA_LEN_MAX + 1u - n) % (RA_LEN_MAX + 1u)];
                const int32_t o = bv - acc;
                ra_buf[s][k][ra_w[s][k] % (RA_LEN_MAX + 1u)] = (int16_t)sat16(acc + (bv >> 1));
                ra_w[s][k]++;
                acc = o;
            }
            rm_hx[s] = acc;                                          /* the low cut */
            rm_hp[s] += (int32_t)(((int64_t)(acc - rm_hp[s]) * hpk) >> 15);
            y[s] = acc - rm_hp[s];
        }
        {
            const int32_t m = (y[0] + y[1]) >> 1;
            const int32_t a = m + (int32_t)(((int64_t)(y[0] - m) * wid) >> 15),
                          b = m + (int32_t)(((int64_t)(y[1] - m) * wid) >> 15);
            l[2 * i] += (rm_prev[0] + a) >> 1, l[2 * i + 1] += a;
            r[2 * i] += (rm_prev[1] + b) >> 1, r[2 * i + 1] += b;
            rm_prev[0] = a, rm_prev[1] = b;
        }
    }
}

/* ---------------------------------------------------------------- CRUSH --- */
static uint32_t cr_cnt;
static int32_t cr_h[2];
static void crush(int32_t *l, int32_t *r)
{
    static const uint8_t DIV[8] = {1, 2, 3, 4, 6, 9, 11, 15};
    const int mix = 127, bits = P[QD_FXP_XBITS];        /* (a room is whole on its own bus: qd_fx_rooms) */
    const uint32_t div = DIV[P[QD_FXP_XRATE] & 7];
    int i;
    if (!mix)
        return;
    for (i = 0; i < N; i++) {
        if (cr_cnt == 0) {
            const int sh = 16 - (bits < 1 ? 1 : bits > 16 ? 16 : bits);
            cr_h[0] = (sat16(l[i]) >> sh) << sh;
            cr_h[1] = (sat16(r[i]) >> sh) << sh;
            cr_cnt = div + (P[QD_FXP_XJIT] ? (rnd() % (1u + (uint32_t)P[QD_FXP_XJIT] / 16u)) : 0u);
        }
        cr_cnt--;
        l[i] += (cr_h[0] - l[i]) * mix / 127;
        r[i] += (cr_h[1] - r[i]) * mix / 127;
    }
}

/* ---------------------------------------------------------------- ROBOT: a ring modulator --- */
static uint32_t rb_ph, rb_lph;
static void robot(int32_t *l, int32_t *r)
{
    const int mix = 127;
    float hz;
    uint32_t inc;
    int i;
    if (!mix)
        return;
    rb_lph += 2u * 4294967u * 3u;                                   /* (the wobble at ~3 Hz, a block a step) */
    hz = 20.f * ex2((float)P[QD_FXP_RBFREQ] * (6.6f / 127.f)) *
         ex2((float)tri_q15(rb_lph) * (float)P[QD_FXP_RBLFO] * (0.5f / 32768.f / 127.f));
    inc = (uint32_t)(hz / 44100.f * 4294967296.f);
    for (i = 0; i < N; i++) {
        int32_t c;
        rb_ph += inc;
        switch (P[QD_FXP_RBWAVE]) {
        case 1: c = rb_ph < 0x80000000u ? 32767 : -32767; break;
        case 2: c = tri_q15(rb_ph); break;
        default: {
            const int32_t t = tri_q15(rb_ph);                        /* a softened triangle: near a sine */
            c = (int32_t)(((int64_t)t * (49152 - (int32_t)(((int64_t)t * t) >> 15) / 2)) >> 15);
            break;
        }
        }
        {
            const int32_t a = (int32_t)(((int64_t)l[i] * c) >> 15), b = (int32_t)(((int64_t)r[i] * c) >> 15);
            l[i] += (a - l[i]) * mix / 127;
            r[i] += (b - r[i]) * mix / 127;
        }
    }
}

/* ---------------------------------------------------------------- WARP: a comb / flanger --- */
#define WP_LEN 512u
static int16_t wp_buf[2][WP_LEN];
static uint32_t wp_w, wp_ph;
static void warp(int32_t *l, int32_t *r)
{
    const int mix = 127;
    const int32_t fb = (P[QD_FXP_WFB] - 64) * 480;                  /* Q15: to +-0.94 */
    const int32_t base = 9 + P[QD_FXP_WTIME] * 3;                    /* samples (0.2 .. 8.8 ms) */
    const uint32_t inc = (uint32_t)P[QD_FXP_WRATE] * P[QD_FXP_WRATE] * 25u;
    int i, s;
    if (!mix)
        return;
    for (i = 0; i < N; i++) {
        wp_ph += inc;
        for (s = 0; s < 2; s++) {
            int32_t *x = s ? &r[i] : &l[i];
            const int32_t sw = inc ? (int32_t)((((int64_t)tri_q15(wp_ph + (s ? 0x40000000u : 0u)) + 32768) * base) >> 16)
                                   : base;
            const int32_t d = sw < 2 ? 2 : sw > (int32_t)WP_LEN - 2 ? (int32_t)WP_LEN - 2 : sw;
            const int32_t y = wp_buf[s][(wp_w - (uint32_t)d) & (WP_LEN - 1u)];
            wp_buf[s][wp_w & (WP_LEN - 1u)] = (int16_t)sat16(*x + (int32_t)(((int64_t)y * fb) >> 15));
            *x += (int32_t)(((int64_t)y * mix) / 127);              /* (the comb's notches: the dry and the delayed) */
        }
        wp_w++;
    }
}

/* ---------------------------------------------------------------- GLITCH: a beat-repeat --- */
#define GL_LEN 8192u                    /* at half rate (0.37 s) */
static int8_t gl_buf[GL_LEN];           /* (the two sides' sum, companded: c8_put; the caught slice comes back mono) */
static uint32_t gl_w, gl_cnt, gl_slice, gl_start, gl_pos, gl_step, gl_left;
static uint8_t gl_on, gl_half;
int qd_fx_glitch_on(float *pos)
{
    if (pos)
        *pos = gl_on && gl_slice ? (float)(gl_pos >> 16) / (float)gl_slice : 0.f;
    return gl_on;
}
static void glitch(int32_t *l, int32_t *r)
{
    static const uint8_t DIV[4] = {0, 2, 5, 8};                      /* (div_samples' 1/32 1/16 1/8 1/4) */
    const int mix = 127;
    int i;
    if (!mix) {
        gl_on = 0;
        return;
    }
    for (i = 0; i < N; i++) {
        if (!(gl_half ^= 1)) {                                       /* (the half rate: every other sample) */
            gl_buf[gl_w & (GL_LEN - 1u)] = c8_put(sat16((l[i] + r[i]) >> 2));
            gl_w++;
            if (gl_cnt == 0) {                                       /* a slice boundary: repeat the last one? */
                uint32_t n = div_samples(DIV[P[QD_FXP_GLEN] & 3], 22050u);
                n = n > GL_LEN - 16u ? GL_LEN - 16u : n < 32u ? 32u : n;
                gl_cnt = n;
                if (gl_on && gl_left) {
                    gl_left--;
                    gl_pos = 0;
                    gl_step = (uint32_t)((float)gl_step * ex2((float)(P[QD_FXP_GPTCH] - 12) * (1.f / 12.f)));
                    if (gl_step < 8192u)
                        gl_step = 8192u;
                } else if ((int)(rnd() % 128u) < P[QD_FXP_GCHNC]) {
                    gl_on = 1;
                    gl_left = 3;
                    gl_slice = n;
                    gl_start = gl_w - n;
                    gl_pos = 0;
                    gl_step = 65536u;
                } else {
                    gl_on = 0;
                }
            }
            gl_cnt--;
            if (gl_on) {
                gl_pos += gl_step;
                if ((gl_pos >> 16) >= gl_slice)
                    gl_pos = 0;
            }
        }
        if (gl_on) {
            const uint32_t k = (gl_start + (gl_pos >> 16)) & (GL_LEN - 1u);
            const int32_t a = c8_get(gl_buf[k]) << 1, b = a;
            l[i] += (a - l[i]) * mix / 127;
            r[i] += (b - r[i]) * mix / 127;
        }
    }
}

/* ---------------------------------------------------------------- TV: a small speaker, the hum, a little drive --- */
static int32_t tv_hp[2], tv_lp[2], tv_lp2[2];
static uint32_t tv_ph;
static void tv(int32_t *l, int32_t *r)
{
    const int mix = 127;
    const int32_t lpk = 3000 + P[QD_FXP_TVTONE] * 180;               /* Q15 low-pass (the speaker's top) */
    const int32_t hpk = 2400 - P[QD_FXP_TVTONE] * 12;                /* Q15 high-pass (its bottom) */
    const int32_t hum = P[QD_FXP_TVHUM] * 14;
    const int32_t drv = 4096 + P[QD_FXP_TVDRV] * 96;                 /* Q12 */
    int i, s;
    if (!mix)
        return;
    for (i = 0; i < N; i++) {
        int32_t h;
        tv_ph += 4870000u;                                           /* 50 Hz */
        h = tri_q15(tv_ph) + (tri_q15(tv_ph * 3u) >> 2);             /* the mains and its buzz */
        h = (int32_t)(((int64_t)h * hum) >> 15);
        for (s = 0; s < 2; s++) {
            int32_t *x = s ? &r[i] : &l[i], y = *x;
            tv_hp[s] += (int32_t)(((int64_t)(y - tv_hp[s]) * hpk) >> 15);
            y -= tv_hp[s];
            tv_lp[s] += (int32_t)(((int64_t)(y - tv_lp[s]) * lpk) >> 15);
            tv_lp2[s] += (int32_t)(((int64_t)(tv_lp[s] - tv_lp2[s]) * lpk) >> 15);
            y = (tv_lp2[s] * drv) >> 12;
            y = y > 24000 ? 24000 + (y - 24000) / 4 : y < -24000 ? -24000 + (y + 24000) / 4 : y;
            y += h;
            *x += (y - *x) * mix / 127;
        }
    }
}

/* ---------------------------------------------------------------- the chain --- */
void qd_fx_init(void)
{
    int i;
    memset(ch_buf, 0, sizeof ch_buf), ch_w = ch_ph = 0;
    memset(ec_buf, 0, sizeof ec_buf), ec_w = ec_ph = ec_cnt = 0;
    memset(ec_lp, 0, sizeof ec_lp), memset(ec_hold, 0, sizeof ec_hold), memset(ec_prev, 0, sizeof ec_prev);
    memset(rc_buf, 0, sizeof rc_buf), memset(ra_buf, 0, sizeof ra_buf), memset(rc_w, 0, sizeof rc_w);
    memset(ra_w, 0, sizeof ra_w), memset(rc_lp, 0, sizeof rc_lp), memset(rp_buf, 0, sizeof rp_buf), rp_w = 0;
    memset(rm_hp, 0, sizeof rm_hp), memset(rm_prev, 0, sizeof rm_prev);
    cr_cnt = 0, rb_ph = rb_lph = 0, memset(wp_buf, 0, sizeof wp_buf), wp_w = wp_ph = 0;
    memset(gl_buf, 0, sizeof gl_buf), gl_w = gl_cnt = gl_pos = 0, gl_on = 0;
    memset(tv_hp, 0, sizeof tv_hp), memset(tv_lp, 0, sizeof tv_lp), memset(tv_lp2, 0, sizeof tv_lp2), tv_ph = 0;
    memset(rm_hx, 0, sizeof rm_hx), rm_ph = 0, memset(cr_h, 0, sizeof cr_h);   /* (every run the same: a session's
                                                                                 * audio is a function of its input) */
    gl_slice = gl_start = gl_step = gl_left = 0, gl_half = 0;
    rng = 0x2545F491u;
    memset(fx_dc, 0, sizeof fx_dc);
    memset(rm_live, 0, sizeof rm_live), rm_a = rm_b = 0;
    for (i = 0; i < QD_NFXP; i++)
        P[i] = FXV[i].def;
    fx_bpm = 120;
    song.g[G_BPM] = 120;
    fx_master_now(fz_master_of_pot(QD_FX_POT_DEFAULT));
    fx_output_set(1u, 1);
}
void qd_fx_tempo(int bpm)
{
    fx_bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm;
    song.g[G_BPM] = (int16_t)fx_bpm;
}
void qd_fx_tempo_fx(int bpm) { fx_bpm = bpm < 20 ? 20 : bpm > 300 ? 300 : bpm; }
void qd_fx_master_pot(int pot)
{
    pot = pot < 0 ? 0 : pot > 1023 ? 1023 : pot;
    song.master_q12 = fz_master_of_pot((uint32_t)pot);
}
uint32_t qd_fx_master_q12(void) { return song.master_q12; }
int qd_fx_route_block(int pos) { return pos & 3; }
void qd_fx_chain_g(const int32_t g[3]) { (void)g; }
int qd_fx_route_move(int block, int dir) { (void)block, (void)dir; return 0; }

void qd_fx_rooms(const int16_t (*a)[QD_FX_CTL], const int16_t (*b)[QD_FX_CTL]) { rm_a = a, rm_b = b; }
void qd_fx_block(const int32_t *sc, const int32_t *sd, const int32_t *sr, int32_t *l, int32_t *r, int32_t *out)
{
    chorus(sc, l, r);
    echo(sd, l, r);
    room(sr, l, r);
    {   /* the five rooms, each on its own bus (the tracks' sends), its output added to the mix; a room with nothing
         * coming in (and its tail gone: 64 blocks, 46 ms) is not run */
        static void (*const ROOM[5])(int32_t *, int32_t *) = {crush, robot, warp, glitch, tv};
        int32_t tl[N], tr[N];
        int k, i;
        for (k = 0; k < 5; k++) {
            int32_t any = 0;
            for (i = 0; i < N; i++) {
                const int32_t v = ((rm_a ? rm_a[k][i] : 0) + (rm_b ? rm_b[k][i] : 0)) * 16;
                tl[i] = tr[i] = v;
                any |= v;
            }
            if (any)
                rm_live[k] = 64;
            else if (rm_live[k])
                rm_live[k]--;
            else {
                if (k == 3)
                    gl_on = 0;
                continue;
            }
            ROOM[k](tl, tr);
            for (i = 0; i < N; i++)
                l[i] += tl[i], r[i] += tr[i];
        }
        rm_a = rm_b = 0;                                /* (a block's buses: given again for the next) */
    }
    out_master(out, l, r, (const int32_t *)0, CTL);
}

const char *qd_fx_text(int id, int v, char *buf)
{
    buf[0] = 0;
    if (id < 0 || id >= QD_NFXP)
        return buf;
    if (FXV[id].list) {                                 /* (into buf: the view copies from it) */
        strcpy(buf, v >= FXV[id].min && v <= FXV[id].max ? FXV[id].list[v - FXV[id].min] : "?");
        return buf;
    }
    switch (id) {
    case QD_FXP_DTIME: {
        char t[12];
        strcpy(buf, itoa_(t, (220 + v * 98) * 10 / 221));
        strcat(buf, " MS");
        return buf;
    }
    case QD_FXP_RBFREQ: {
        char t[12];
        strcpy(buf, itoa_(t, (int)(20.f * ex2((float)v * (6.6f / 127.f)))));
        strcat(buf, " HZ");
        return buf;
    }
    case QD_FXP_GPTCH:
    case QD_FXP_WFB: {
        const int c = id == QD_FXP_GPTCH ? v - 12 : v - 64;
        if (c > 0)
            buf[0] = '+', buf[1] = 0;
        itoa_(buf + (c > 0), c);
        return buf;
    }
    case QD_FXP_RDECAY:
        if (v >= 127) {
            strcpy(buf, "HOLD");
            return buf;
        }
        return itoa_(buf, v);
    case QD_FXP_XBITS:
        return itoa_(buf, v);
    default:
        return itoa_(buf, v);
    }
}
int qd_fx_delay_ms(void) { return (int)(echo_len(0) * 1000u / 22050u); }
int qd_fx_chorus_hz100(void) { return (int)(100.f * 0.05f * ex2((float)P[QD_FXP_CRATE] * (8.f / 127.f))); }
int qd_fx_reverb_ms(void) { return P[QD_FXP_RDECAY] >= 127 ? -1 : 300 + P[QD_FXP_RDECAY] * 30; }
