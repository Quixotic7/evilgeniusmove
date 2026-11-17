/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* FIZZ's MIDI map (midi_map.h; research/FIZZ-MIDI-PLAN.md §2). Built inside app.c */
#include "midi_map.h"

/* CC -> the value it sets, + 1 (0: none). The common GM numbers first (the mixer, FREQ, RESO, PORT, the amp), then the
 * editor's pages in SELECT's order. Left alone (0): 0 / 32 bank, 1 / 2 the modulation sources (midi_play), 6 / 38 data
 * entry, 64, 66..69 the pedals, 96..101 RPN / NRPN, 120..127 the channel mode (120 / 123 release, midi_play). In flash */
#define M(id) ((uint8_t)((id) + 1))
#define LF(k, f) M(QD_QP_LFO((k), (f)))
static const uint8_t MM_CC[128] = {
    [7] = M(QD_LK_LEVEL), [10] = M(QD_LK_PAN), [91] = M(QD_LK_REVERB), [93] = M(QD_LK_CHORUS), [94] = M(QD_LK_DELAY),
    [95] = M(QD_MM_DRY), [74] = M(QD_QP_FREQ), [71] = M(QD_QP_RESO), [5] = M(QD_QP_PTIM), [65] = M(QD_QP_PORT),
    [73] = M(QD_QP_EATK), [75] = M(QD_QP_EDEC), [72] = M(QD_QP_EREL),
    /* PORT, VOICE (EvilGenius: SID TONE's values) */
    [9] = M(QD_QP_HR), [18] = M(QD_QP_VEL), [19] = M(QD_QP_PSLOPE),
    [20] = M(QD_QP_VMODE), [21] = M(QD_QP_UNI), [22] = M(QD_QP_UDET), [23] = M(QD_QP_USPR), [24] = M(QD_QP_VPRIO),
    /* OSC 1 .. 3: WAVE COARSE FINE PW MOD; DETUNE */
    [25] = M(QD_QP_W1), [26] = M(QD_QP_C1), [27] = M(QD_QP_F1), [28] = M(QD_QP_PW1), [29] = M(QD_QP_M1),
    [30] = M(QD_QP_W2), [31] = M(QD_QP_C2), [33] = M(QD_QP_F2), [34] = M(QD_QP_PW2), [35] = M(QD_QP_M2),
    [36] = M(QD_QP_W3), [37] = M(QD_QP_C3), [39] = M(QD_QP_F3), [40] = M(QD_QP_PW3), [41] = M(QD_QP_M3),
    [42] = M(QD_QP_DETUNE),
    /* ENV 1 (73 75 72 above, SUS), ENV 2, ENV 3 */
    [62] = M(QD_QP_ESUS), [43] = M(QD_QP_E2A), [44] = M(QD_QP_E2D), [45] = M(QD_QP_E2S), [46] = M(QD_QP_E2R),
    [47] = M(QD_QP_E2LINK), [48] = M(QD_QP_E3A), [3] = M(QD_QP_E3D), [12] = M(QD_QP_E3S), [13] = M(QD_QP_E3R),
    [14] = M(QD_QP_E3LINK),
    /* FILTER, F ENV, CHIP */
    [49] = M(QD_QP_FTYPE), [50] = M(QD_QP_FDEPTH), [51] = M(QD_QP_FROUTE), [52] = M(QD_QP_CHIP),
    [53] = M(QD_QP_FATK), [54] = M(QD_QP_FDEC), [55] = M(QD_QP_FSUS), [56] = M(QD_QP_FREL), [60] = M(QD_QP_FKTRK),
    /* AMP, CRUSH */
    [76] = M(QD_QP_LEVEL), [77] = M(QD_QP_DRIVE), [79] = M(QD_QP_BITS), [80] = M(QD_QP_SRR),
    /* VOX: WORD ROOT START END LOOP, DIGI */
    [15] = M(QD_QP_WORD), [16] = M(QD_QP_ROOT), [17] = M(QD_QP_VSTART), [57] = M(QD_QP_VEND), [58] = M(QD_QP_VLOOP),
    [59] = M(QD_QP_DBITS), [61] = M(QD_QP_DHOLD), [63] = M(QD_QP_DBOOST), [70] = M(QD_QP_DLVL),
    /* SAY, SAM's live speech: the word, SPEED PITCH THROAT MOUTH */
    [78] = M(QD_QP_SAY), [81] = M(QD_QP_SSPD), [116] = M(QD_QP_SPIT), [117] = M(QD_QP_STHR), [118] = M(QD_QP_SMTH),
    /* LFO 1 .. 3: SPEED MULT DEPTH DEST, WAVE PHASE TRIG FADE */
    [82] = LF(0, QD_QL_SPEED), [83] = LF(0, QD_QL_MULT), [84] = LF(0, QD_QL_DEPTH), [85] = LF(0, QD_QL_DEST),
    [86] = LF(0, QD_QL_WAVE), [87] = LF(0, QD_QL_PHASE), [88] = LF(0, QD_QL_TRIG), [89] = LF(0, QD_QL_FADE),
    [90] = LF(1, QD_QL_SPEED), [92] = LF(1, QD_QL_MULT), [102] = LF(1, QD_QL_DEPTH), [103] = LF(1, QD_QL_DEST),
    [104] = LF(1, QD_QL_WAVE), [105] = LF(1, QD_QL_PHASE), [106] = LF(1, QD_QL_TRIG), [107] = LF(1, QD_QL_FADE),
    [108] = LF(2, QD_QL_SPEED), [109] = LF(2, QD_QL_MULT), [110] = LF(2, QD_QL_DEPTH), [111] = LF(2, QD_QL_DEST),
    [112] = LF(2, QD_QL_WAVE), [113] = LF(2, QD_QL_PHASE), [114] = LF(2, QD_QL_TRIG), [115] = LF(2, QD_QL_FADE),
};
#undef LF
#undef M
typedef char mm_ids_fit[QD_MM_DRY + 1 <= 255 ? 1 : -1];   /* (every id + 1 a byte) */

int qd_midi_cc_id(int cc) { return cc >= 0 && cc < 128 ? MM_CC[cc] - 1 : -1; }
int qd_midi_id_cc(int id)
{
    int cc;
    if (id == QD_QP_RB2)
        id = QD_QP_RB1;
    for (cc = 0; id >= 0 && cc < 128; cc++)
        if (MM_CC[cc] == id + 1)
            return cc;
    return -1;
}

static int mm_dest(int id) { return id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 3 * 8 && (id - QD_QP_LFO0) % 8 == QD_QL_DEST; }
static int mm_mult(int id) { return id >= QD_QP_LFO0 && id < QD_QP_LFO0 + 3 * 8 && (id - QD_QP_LFO0) % 8 == QD_QL_MULT; }
/* the range the CC spreads over: the value's, or its list's places (the MACHINE's six in the menu order, the DESTs in
 * ChoralRoot's editor order), RATIO B's pair */
static int mm_range(int id, int *lo, int *hi)
{
    int16_t l, h;
    *lo = 0;
    if (id == QD_MM_DRY)
        *hi = 127;
    else if (id == QD_QP_RB1)
        *hi = QD_MM_NRB - 1;
    else if (id == QD_QP_FTYPE)
        *hi = 5;
    else if (mm_dest(id))
        *hi = qd_quad_ndest() - 1;
    else if (qd_lock_range(id, &l, &h))
        *lo = l, *hi = h;
    else
        return 0;
    return 1;
}
int qd_midi_cc_value(int id, int cc)
{
    int lo, hi, p;
    if (!mm_range(id, &lo, &hi))
        return 0;
    cc = cc < 0 ? 0 : cc > 127 ? 127 : cc;
    p = lo + (cc * (hi - lo) + 63) / 127;
    if (id == QD_QP_FTYPE) {                            /* (the machine's stored FTYPE: ctl_snd.c SND_MACH_FT) */
        const int m = QD_SND_MENU[p];
        return m ? m + 2 : QD_QF_LP12;
    }
    if (mm_mult(id))                                    /* (2026-11-14: MULT in the speed's order, as its knob) */
        return qd_quad_mult_at(p);
    if (mm_dest(id))
        return qd_quad_dest_step(qd_quad_dest_step(0, -qd_quad_ndest()), p);   /* (the order's first, on p places) */
    return p;
}
int qd_midi_value_cc(int id, int v)
{
    int lo, hi;
    if (!mm_range(id, &lo, &hi) || hi <= lo)
        return 0;
    if (id == QD_QP_FTYPE) {
        const int m = qd_snd_machine(v);
        for (v = 0; v < 5 && QD_SND_MENU[v] != m; v++)
            ;
    } else if (mm_dest(id)) {
        v = qd_quad_dest_pos(v);
    } else if (mm_mult(id)) {
        v = qd_quad_mult_pos(v);
    }
    v = v < lo ? lo : v > hi ? hi : v;
    return ((v - lo) * 127 + (hi - lo) / 2) / (hi - lo);
}

int qd_midi_cc_in(qd_app_t *a, int tracks, int cc, int v)
{
    const int id = qd_midi_cc_id(cc);
    int t, x;
    if (id < 0)
        return 0;
    x = qd_midi_cc_value(id, v);
    for (t = 0; t < QD_NTRACK; t++) {
        const int16_t *p = a->model.t[t].patch;
        int rec;
        if (!((tracks >> t) & 1) || qd_msnd_is(p))     /* (FIZZ MIDI (e): a MIDI track has no sound to set) */
            continue;
        if (id == QD_MM_DRY) {                          /* (the mixer's own: no lock, no take) */
            a->model.t[t].mix.dry = (uint8_t)x;
            continue;
        }
        rec = t == a->sel && (id >= QD_QUAD_NP || qd_quad_lockable(id)) && qd_auto_on(a);   /* (the knobs' rule) */
        if (id == QD_QP_FTYPE && qd_snd_machine(rec ? qd_auto_cur(a, id) : p[id]) == qd_snd_machine(x))
            continue;                                   /* (the same machine: LEGACY keeps its TYPE, as the knob) */
        if (id == QD_QP_RB1) {                          /* RATIO B: both halves of the pair */
            if (rec) {
                qd_auto_put(a, QD_QP_RB1, x / 19);
                qd_auto_put(a, QD_QP_RB2, x % 19);
            } else {
                qd_player_live_edit(&a->player, t, QD_QP_RB1, x / 19);
                qd_player_live_edit(&a->player, t, QD_QP_RB2, x % 19);
            }
            continue;
        }
        if (rec && qd_auto_put(a, id, x))
            continue;
        qd_player_live_edit(&a->player, t, id, x);
    }
    return 1;
}

/* ---------------------------------------------------------------- the remote (§3) */
static void rm_key(qd_app_t *a, int k, int down, int vel)
{
    if (down)
        a->rm_keys |= 1u << k;
    else
        a->rm_keys &= ~(1u << k);
    qd_app_key(a, k, down, vel);
}
static void rm_button(qd_app_t *a, int b, int down)
{
    if (down)
        a->rm_btns = (uint16_t)(a->rm_btns | 1u << b);
    else
        a->rm_btns = (uint16_t)(a->rm_btns & ~(1u << b));
    qd_app_button(a, b, down);
}
void qd_midi_remote(qd_app_t *a, int kind, int d1, int d2)
{
    if (kind == 0x80 || kind == 0x90) {                 /* the keys */
        const int k = d1 - QD_NOTE0, down = kind == 0x90 && d2;
        if (k < 0 || k >= QD_NKEY)
            return;
        if ((a->rm_keys >> k) & 1u)                     /* (struck again, or let go: up first) */
            rm_key(a, k, 0, 0);
        if (down)
            rm_key(a, k, 1, d2);
        return;
    }
    if (kind != 0xB0)
        return;
    if (d1 >= QD_RM_BTN0 && d1 < QD_RM_BTN0 + QD_NBTN) {   /* the buttons */
        const int b = d1 - QD_RM_BTN0, down = d2 >= 64;
        if (down != (int)((a->rm_btns >> b) & 1u))
            rm_button(a, b, down);
    } else if (d1 >= QD_RM_ENC0 && d1 < QD_RM_ENC0 + QD_NKNOB) {   /* the encoders, relative (binary offset) */
        if (d2 != 64)
            qd_app_knob(a, d1 - QD_RM_ENC0, d2 - 64);
    } else if (d1 >= QD_RM_FADER0 && d1 < QD_RM_FADER0 + 4) {   /* KNOB 1..4 by a fader's moves */
        /* A fader's move arrives as many 1-step messages a few ms apart: as detents they would look like a knob spun
         * fast, and qd_ctl_acc's acceleration (up to x8 on a 128-step value) would make the value jump and land in a
         * different place for the same throw. So no acceleration: the knob's last-detent clock is cleared first (as a
         * pause would: qd_knob_accel then turns as they come), and a full throw is 127 detents, whatever the speed */
        const int i = d1 - QD_RM_FADER0, last = a->rm_fader[i];
        a->rm_fader[i] = (uint8_t)(d2 + 1);
        if (last && d2 != last - 1) {
            a->ui.knob_ms[QD_K_1 + i] = 0;
            qd_app_knob(a, QD_K_1 + i, d2 - (last - 1));
        }
    } else if (d1 == 7) {                               /* MASTER */
        qd_app_master(a, (d2 * 1023 + 63) / 127);
    } else if (d1 == 123) {                             /* all notes off: what the remote holds, let go */
        int i;
        for (i = 0; i < QD_NKEY; i++)
            if ((a->rm_keys >> i) & 1u)
                rm_key(a, i, 0, 0);
        for (i = 0; i < QD_NBTN; i++)
            if ((a->rm_btns >> i) & 1u)
                rm_button(a, i, 0);
    }
}
