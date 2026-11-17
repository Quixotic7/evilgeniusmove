/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The built-in demo (demo.h): one song in four patterns, the sounds made here on INIT.
 *
 * Dark minimal techno at 124 BPM in A minor, two bars a pattern, everything on the grid (no swing, no chance). The
 * drums are the pool's kit (sounds.h: KICK, SNARE, HAT), a sound lock a step, on track 1 only (QD_DEMO_DRUMS); EVIL's
 * second word is a lock on WORD, not a sound lock. The mix that goes with it is qd_demo_model.
 * Designed by rendering and measuring (the owner tunes it by ear). */
#include <string.h>

#include "demo.h"
#include "sounds.h"

typedef struct { uint8_t id; int16_t v; } kv;
typedef char kv_id_fits[QD_QUAD_NP <= 255 ? 1 : -1];
#define LF(k, f) QD_QP_LFO(k, QD_QL_##f)
#define END {255, 0}
#define FENV(d) {QD_QP_MOD(0, QD_QM_SRC), QD_MS_FENV}, {QD_QP_MOD(0, QD_QM_DST), QD_D_CUT}, {QD_QP_MOD(0, QD_QM_DEPTH), d}

enum { INTRO, GROOVE, BUILD, DROP };
enum { L8 = 0, L4 = 1, L2 = 2, L34 = 3, L1 = 4, L15 = 5, LS2 = 6, LS3 = 7, LS4 = 8, LS6 = 9, LS8 = 10 };

/* the bass: a pulse and a triangle under it, the filter snapping shut, driven */
static const kv BASS[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 38}, {QD_QP_W2, QD_SW_TRI}, {QD_QP_C2, 0},
                          {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 380}, {QD_QP_RESO, 9}, FENV(30), {QD_QP_FATK, 0},
                          {QD_QP_FDEC, 18}, {QD_QP_FSUS, 0}, {QD_QP_FREL, 10}, {QD_QP_EATK, 0}, {QD_QP_EDEC, 5},
                          {QD_QP_ESUS, 6}, {QD_QP_EREL, 1}, {QD_QP_DRIVE, 48}, {QD_QP_VMODE, 1}, {QD_QP_LEVEL, 104},
                          END};
/* the arp: a saw and a pulse apart, a resonant low-pass its envelope plucks (CUTOFF is each pattern's own, and the
 * BUILD's steps lock it); LFO 2 turns the pulse's width */
static const kv ARP[] = {{QD_QP_W1, QD_SW_SAW}, {QD_QP_W2, QD_SW_PUL}, {QD_QP_C2, 0}, {QD_QP_PW2, 44},
                         {QD_QP_DETUNE, 16}, {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 540}, {QD_QP_RESO, 11}, FENV(26),
                         {QD_QP_FATK, 0}, {QD_QP_FDEC, 22}, {QD_QP_FSUS, 0}, {QD_QP_FREL, 14}, {QD_QP_EATK, 0},
                         {QD_QP_EDEC, 6}, {QD_QP_ESUS, 7}, {QD_QP_EREL, 4}, {LF(1, DEST), QD_D_PW2},
                         {LF(1, WAVE), QD_LW_SINE}, {LF(1, TRIG), QD_LT_FREE}, {LF(1, MULT), 7},
                         {LF(1, DEPTH), 64 + 30}, {QD_QP_VMODE, 1}, {QD_QP_LEVEL, 112}, END};
/* the stab: a minor triad in one voice (the three oscillators), short */
static const kv STAB[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 30}, {QD_QP_W2, QD_SW_SAW}, {QD_QP_C2, 3},
                          {QD_QP_W3, QD_SW_PUL}, {QD_QP_C3, 7}, {QD_QP_PW3, 52}, {QD_QP_FTYPE, QD_QF_LPBP},
                          {QD_QP_FREQ, 1150}, {QD_QP_RESO, 7}, FENV(50), {QD_QP_FATK, 0}, {QD_QP_FDEC, 30},
                          {QD_QP_FSUS, 0}, {QD_QP_FREL, 20}, {QD_QP_EATK, 0}, {QD_QP_EDEC, 9}, {QD_QP_ESUS, 0},
                          {QD_QP_EREL, 6}, {QD_QP_VMODE, 1}, {QD_QP_LEVEL, 108}, END};
/* the drone: an octave and a fifth, low under the filter, slow in and out */
static const kv DRONE[] = {{QD_QP_W1, QD_SW_SAW}, {QD_QP_C1, -12}, {QD_QP_W2, QD_SW_PUL}, {QD_QP_C2, 0},
                           {QD_QP_PW2, 46}, {QD_QP_W3, QD_SW_SAW}, {QD_QP_C3, 7}, {QD_QP_DETUNE, 10},
                           {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 300}, {QD_QP_RESO, 6}, {LF(0, DEST), QD_D_CUT},
                           {LF(0, WAVE), QD_LW_SINE}, {LF(0, TRIG), QD_LT_FREE}, {LF(0, MULT), 7},
                           {LF(0, DEPTH), 64 + 22}, {QD_QP_EATK, 8}, {QD_QP_EDEC, 8}, {QD_QP_ESUS, 13},
                           {QD_QP_EREL, 9}, {QD_QP_VMODE, 1}, {QD_QP_LEVEL, 90}, END};
/* the riser (the BUILD's PAD): noise through a band-pass that a two-bar ramp opens, once, and swells */
static const kv RISER[] = {{QD_QP_W1, QD_SW_NOI}, {QD_QP_C1, 12}, {QD_QP_FTYPE, QD_QF_BP}, {QD_QP_FREQ, 500},
                           {QD_QP_RESO, 10}, {LF(0, DEST), QD_D_CUT}, {LF(0, WAVE), QD_LW_RAMP},
                           {LF(0, TRIG), QD_LT_ONE}, {LF(0, MULT), 7}, {LF(0, DEPTH), 64 + 56},
                           {LF(1, DEST), QD_D_LEVEL}, {LF(1, WAVE), QD_LW_RAMP}, {LF(1, TRIG), QD_LT_ONE},
                           {LF(1, MULT), 7}, {LF(1, DEPTH), 64 + 40}, {QD_QP_EATK, 9}, {QD_QP_EDEC, 8},
                           {QD_QP_ESUS, 15}, {QD_QP_EREL, 3}, {QD_QP_VMODE, 1}, {QD_QP_LEVEL, 112}, END};
/* the words: SAM's EVIL (the VOX factory sound), GENIUS a lock on a step */
static const kv EVIL[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_WORD, 42}, {QD_QP_EREL, 0}, {QD_QP_LEVEL, 127}, END};
#define WORD_GENIUS 43

static void patch(qd_tsound_t *s, const char *name, const kv *k)
{
    memset(s, 0, sizeof *s);
    qd_quad_preset_patch(0, s->patch);                  /* INIT */
    for (; k->id != 255; k++)
        s->patch[k->id] = k->v;
    strncpy(s->name, name, sizeof s->name - 1);
}

static void note(qd_pattern_t *p, int t, int s, int n, int vel, int len)
{
    const uint8_t x = (uint8_t)n;
    qd_step_on(p, t, s);
    qd_step_set_notes(p, t, s, &x, 1);
    qd_step_set_vel(p, t, s, vel);
    qd_step_set_len(p, t, s, len);
}

/* a drum: the pool's sound on its step of track 1, at the note it was made for, its level a lock */
static void drum(qd_pattern_t *p, int s, int snd, int vel, int level)
{
    note(p, 0, s, snd == QD_SND_KICK ? 33 : snd == QD_SND_SNARE ? 55 : 96, vel, snd == QD_SND_KICK ? L2 : L4);
    qd_step_set_sound(p, 0, s, snd);
    qd_lock_set(p, 0, s, QD_LK_LEVEL, level);
}

/* the drums' levels over the track's (locks): measured, the kick the loudest thing in the mix */
#define KICK_LEVEL 126
#define HAT_LEVEL 127
#define SNARE_LEVEL 112

void qd_demo_pattern(qd_pattern_t *p, int kind)
{
    static const uint8_t ARPN[32] = {57, 69, 64, 69, 60, 69, 64, 69, 57, 69, 64, 72, 60, 69, 64, 72,
                                     57, 69, 64, 69, 60, 69, 64, 69, 57, 69, 64, 72, 64, 72, 69, 76};
    static const int16_t CUT[4] = {300, 560, 260, 1000};
    int s, t;

    qd_pat_init(p);
    if (kind < 0 || kind >= QD_DEMO_N)
        return;
    p->bpm = 124;
    p->swing = 50;
    p->master = 32;
    for (t = 0; t < 5; t++)
        qd_track_set_length(p, t, 32);
    qd_tset_set_rate(&p->trk[5], QD_RATE_1_4);
    qd_track_set_length(p, 5, 8);

    /* DRUMS */
    if (kind == GROOVE || kind == DROP) {
        /* the hats: the off-beat eighths, and a syncopated line across them (3, 9, 11, and 31). The echo's dotted
         * eighth is three steps: the hats on 2 and 10 go into it hardest and answer on the empty 5 and 13 (and 8, 16
         * under the kick), 14 answers on the next bar's 1, so the echo plays the sixteenths the hat leaves out. The
         * DROP opens the hat on 6 and 14 */
        static const struct { uint8_t s, vel, echo; } HAT[] = {{2, 110, 100}, {3, 96, 50}, {6, 124, 40}, {9, 100, 60},
                                                               {10, 110, 100}, {11, 84, 0}, {14, 124, 70}};
        int b, k;
        for (s = 0; s < 32; s += 4)
            drum(p, s, QD_SND_KICK, 124, KICK_LEVEL);
        for (b = 0; b < 32; b += 16)
            for (k = 0; k < (int)(sizeof HAT / sizeof HAT[0]); k++) {
                const int open = kind == DROP && HAT[k].s % 8 == 6;
                drum(p, b + HAT[k].s, open ? QD_SND_OHAT : QD_SND_HAT, HAT[k].vel, HAT_LEVEL);
                qd_lock_set(p, 0, b + HAT[k].s, QD_LK_DELAY, HAT[k].echo);
            }
        drum(p, 31, QD_SND_HAT, 92, HAT_LEVEL);
        qd_lock_set(p, 0, 31, QD_LK_DELAY, 100);
    }
    if (kind == BUILD) {
        for (s = 0; s < 16; s += 4)                     /* bar 1: the kick, the snare coming in */
            drum(p, s, QD_SND_KICK, 124, KICK_LEVEL);
        drum(p, 10, QD_SND_SNARE, 60, SNARE_LEVEL);
        drum(p, 14, QD_SND_SNARE, 68, SNARE_LEVEL);
        for (s = 16; s < 24; s += 2)                    /* bar 2: eighths, sixteenths, then ratchets; a gap at the end */
            drum(p, s, QD_SND_SNARE, 72 + (s - 16) * 3, SNARE_LEVEL + (s - 16) / 2);
        for (s = 24; s < 28; s++)
            drum(p, s, QD_SND_SNARE, 96 + (s - 24) * 4, SNARE_LEVEL + (s - 16) / 2);
        for (s = 28; s < 31; s++) {
            drum(p, s, QD_SND_SNARE, 116 + (s - 28) * 4, SNARE_LEVEL + (s - 16) / 2);
            qd_step_set_ratchet(p, 0, s, s == 30 ? 3 : 2, QD_RAMP_UP);
        }
    }
    p->trk[0].limit = 3;

    /* BASS: the three sixteenths between the kicks */
    if (kind != INTRO)
        for (s = 0; s < (kind == BUILD ? 16 : 32); s += 4) {
            note(p, 1, s + 1, 33, 84, L2);
            note(p, 1, s + 2, 33, 104, L2);
            note(p, 1, s + 3, s == 28 ? 36 : 33, 112, L2);
        }
    p->trk[1].limit = 1;
    patch(&p->snd[1], "ROLL BASS", BASS);

    /* KEYS: the effects, the stab's three oscillators under a PAL arpeggio (LFO 1 on PITCH: a note a frame, 50 Hz).
     * The pool's first sounds are all a demo may lock (QD_DEMO_DRUMS), so each hit is the track's sound with its
     * LFO locked on the step: the wave is the chord, DEPTH how far and which way, SPEED the frames a note, ONE a
     * run that ends */
    patch(&p->snd[2], kind == BUILD ? "LIFT" : "ARP STAB", STAB);
    p->snd[2].patch[LF(0, DEST)] = QD_D_PITCH;
    p->snd[2].patch[LF(0, WAVE)] = QD_LW_AMIN;
    p->snd[2].patch[LF(0, TRIG)] = QD_LT_TRIG;
    p->snd[2].patch[LF(0, SPEED)] = 127;
    p->snd[2].patch[LF(0, DEPTH)] = 64 + 30;            /* (two octaves up) */
    if (kind == DROP) {
        /* (step, note, velocity, length, wave, trig, speed, depth) */
        static const struct { uint8_t s, n, vel, len, wave, trig, speed, depth; } FXH[] = {
            {0, 57, 112, LS2, QD_LW_AMIN, QD_LT_TRIG, 127, 64 + 30},    /* the chord on the one, buzzing */
            {6, 69, 92, L1, QD_LW_AOCT, QD_LT_TRIG, 111, 64 + 50},     /* octaves up, three frames a note */
            {14, 81, 100, LS2, QD_LW_AMIN, QD_LT_ONE, 103, 64 - 50},    /* a fall, three octaves down the chord */
            {16, 57, 104, LS2, QD_LW_AMIN7, QD_LT_TRIG, 127, 64 + 30},  /* the chord again, its seventh in */
            {22, 69, 92, L1, QD_LW_ADIM, QD_LT_TRIG, 111, 64 + 50},
            {30, 81, 104, LS2, QD_LW_A5TH, QD_LT_ONE, 111, 64 - 50},    /* the fall into the loop */
        };
        for (s = 0; s < (int)(sizeof FXH / sizeof FXH[0]); s++) {
            const int at = FXH[s].s;
            note(p, 2, at, FXH[s].n, FXH[s].vel, FXH[s].len);
            qd_lock_set(p, 2, at, LF(0, WAVE), FXH[s].wave);
            qd_lock_set(p, 2, at, LF(0, TRIG), FXH[s].trig);
            qd_lock_set(p, 2, at, LF(0, SPEED), FXH[s].speed);
            qd_lock_set(p, 2, at, LF(0, DEPTH), FXH[s].depth);
        }
    } else if (kind == BUILD) {                         /* the lift: fifths up three octaves into the drop */
        note(p, 2, 24, 45, 108, LS6);
        qd_lock_set(p, 2, 24, LF(0, WAVE), QD_LW_A5TH);
        qd_lock_set(p, 2, 24, LF(0, TRIG), QD_LT_ONE);
        qd_lock_set(p, 2, 24, LF(0, SPEED), 87);        /* (six frames a note: the six notes in 0.7 s) */
        qd_lock_set(p, 2, 24, LF(0, DEPTH), 64 + 50);
        qd_lock_set(p, 2, 24, QD_QP_EDEC, 11);
        qd_lock_set(p, 2, 24, QD_QP_ESUS, 10);
        qd_lock_set(p, 2, 24, QD_LK_LEVEL, 98);
    }
    p->trk[2].limit = 1;

    /* LEAD: the arp, the root on every beat */
    patch(&p->snd[3], "TRANCE ARP", ARP);
    p->snd[3].patch[QD_QP_FREQ] = CUT[kind];
    if (kind == DROP) {                                 /* the DROP: its pulse an octave up and back every frame */
        p->snd[3].patch[LF(0, DEST)] = QD_D_P2;
        p->snd[3].patch[LF(0, WAVE)] = QD_LW_AOCT;
        p->snd[3].patch[LF(0, TRIG)] = QD_LT_TRIG;
        p->snd[3].patch[LF(0, SPEED)] = 127;
        p->snd[3].patch[LF(0, DEPTH)] = 64 + 10;
    }
    for (s = 0; s < 32; s++) {
        note(p, 3, s, ARPN[s], s % 4 == 0 ? 118 : 82, L2);
        if (kind == BUILD) {                            /* the sweep: a lock a step */
            qd_lock_set(p, 3, s, QD_QP_FREQ, 260 + s * 44);
            qd_lock_set(p, 3, s, QD_QP_RESO, 9 + s / 6);
        } else if (kind == INTRO)
            qd_lock_set(p, 3, s, QD_QP_FREQ, 240 + s * 9);
    }
    p->trk[3].limit = 1;

    /* PLUCK: the words */
    patch(&p->snd[4], "EVIL", EVIL);
    if (kind == BUILD) {                                /* EVIL, EVIL, E-E-E.. */
        static const uint8_t AT[7] = {0, 16, 20, 24, 26, 28, 29};
        for (s = 0; s < 7; s++)
            note(p, 4, AT[s], 60 + (s > 3 ? s - 3 : 0), 112, s < 2 ? LS4 : L1);
    } else {                                            /* EVIL .. GENIUS; the DROP: GENIUS on the one */
        note(p, 4, 0, 60, kind == DROP ? 120 : 112, kind == DROP ? LS6 : LS4);
        note(p, 4, 16, 60, 112, kind == DROP ? LS4 : LS6);
        qd_lock_set(p, 4, kind == DROP ? 0 : 16, QD_QP_WORD, WORD_GENIUS);
    }
    p->trk[4].limit = 1;

    /* PAD: the drone, two bars a note; the BUILD's is the riser */
    patch(&p->snd[5], kind == BUILD ? "RISER" : "DRONE", kind == BUILD ? RISER : DRONE);
    note(p, 5, 0, kind == BUILD ? 60 : 45, 100, LS8);
    p->trk[5].limit = 1;
    p->used = 1;
}

/* the mix the song was made in: lo-fi on the drums and the drone (CRUSH, TV), the echo on the arp, the stab and
 * the words */
void qd_demo_model(qd_model_t *m)
{
    static const struct { uint8_t level, chorus, delay, reverb, crush, robot, tv; int8_t pan; } M[6] = {
        {108, 0, 0, 8, 34, 0, 18, 0},                   /* DRUMS */
        {110, 0, 0, 0, 20, 0, 0, 0},                    /* BASS */
        {106, 20, 84, 50, 0, 0, 30, -18},               /* KEYS */
        {110, 24, 44, 30, 0, 0, 0, 12},                 /* LEAD */
        {118, 0, 60, 40, 0, 36, 0, 0},                  /* PLUCK (the words) */
        {84, 30, 0, 56, 52, 0, 40, 0},                  /* PAD */
    };
    int t;
    for (t = 0; t < 6 && t < QD_NTRACK; t++) {
        qd_mix_track_t *x = &m->t[t].mix;
        x->level = M[t].level, x->pan = M[t].pan;
        x->chorus = M[t].chorus, x->delay = M[t].delay, x->reverb = M[t].reverb;
        x->room[0] = M[t].crush, x->room[1] = M[t].robot, x->room[4] = M[t].tv;
    }
}

const uint8_t QD_DEMO_DRUMS[QD_DEMO_N][QD_DEMO_NSTEP] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {18, 0, 22, 22, 18, 0, 22, 0, 18, 22, 22, 22, 18, 0, 22, 0, 18, 0, 22, 22, 18, 0, 22, 0, 18, 22, 22, 22, 18, 0, 22, 22},
    {18, 0, 0, 0, 18, 0, 0, 0, 18, 0, 19, 0, 18, 0, 19, 0, 19, 0, 19, 0, 19, 0, 19, 0, 19, 19, 19, 19, 19, 19, 19, 0},
    {18, 0, 22, 22, 18, 0, 23, 0, 18, 22, 22, 22, 18, 0, 23, 0, 18, 0, 22, 22, 18, 0, 23, 0, 18, 22, 22, 22, 18, 0, 23, 22},
};

void qd_demo_load(qd_project_t *pr)
{
    int k;
    pr->demo = 1;                                       /* (P2.1: the patterns' defaults are the demos) */
    for (k = 0; k < QD_DEMO_N; k++) {
#if QD_PAT_RAM < QD_NPAT
        if (!qd_proj_resident(pr, k)) {                 /* (out of RAM: its default is the demo now; a record in the
                                                         * store still wins when it is read in) */
            pr->pat_used[k] = 1;
            continue;
        }
#endif
        qd_demo_pattern(qd_proj_pat(pr, k), k);
    }
}
