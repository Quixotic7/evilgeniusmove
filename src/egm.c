/* SPDX-License-Identifier: MPL-2.0 */
/* The Move's own interface over the EvilGenius core.
 *
 * The core keeps its whole instrument: the sequencer, the sounds, the locks, the undo, the stores. What is the Move's
 * is how it is laid out on this surface:
 *
 *   the 16 step buttons   the core's W1..W16: the steps (tap, hold, the latch), as on the FM-1. With Shift: the
 *                         shortcuts printed under them (SHIFT_STEP below)
 *   the top pad row       the page keys: on HOME the core's B1..B8 (the hold modes); in SOUND the eight groups; in FX
 *                         the tower's floors
 *   the 24 pads under it  notes of the selected track's scale (through the core's MIDI in: they play, record and
 *                         enter a held step's notes); a row up is a fourth up
 *   the 8 knobs           the open page's values. A SOUND or FX page here is up to eight of the core's values: one of
 *                         the core's four-knob pages, or two side by side. A turn goes through the core's own knob
 *                         (so it locks a held step, is undone, is clamped as there): the core's page is put where the
 *                         value lives for the turn
 *   the screen            pictures, no labels: a knob touched or turned fills the screen with its name and value, a page
 *                         entered with its name, the wheel touched with all eight
 *
 * The core's ui state (the latch, the page) says which of these modes is up, so the two never disagree. */
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "ctl.h"
#include "persist.h"
#include "qd_gfx.h"
#include "quad_voice.h"
#include "scale.h"
#include "ui.h"

#include "egm.h"
#include "flash.h"
#include "mono.h"
#include "screens.h"

/* ------------------------------------------------------------------ the Move's surface (schwung docs/API.md) --- */
enum { N_KNOB_TOUCH0 = 0, N_VOL_TOUCH = 8, N_JOG_TOUCH = 9, N_STEP0 = 16, N_PAD0 = 68 };
enum { CC_JOG_CLICK = 3, CC_JOG = 14, CC_TRACK4 = 40, CC_TRACK1 = 43, CC_SHIFT = 49, CC_MENU = 50, CC_BACK = 51,
       CC_CAPTURE = 52, CC_DOWN = 54, CC_UP = 55, CC_UNDO = 56, CC_LOOP = 58, CC_COPY = 60, CC_LEFT = 62, CC_RIGHT = 63,
       CC_KNOB1 = 71, CC_VOLUME = 79, CC_PLAY = 85, CC_REC = 86, CC_MUTE = 88, CC_SAMPLE = 118, CC_DELETE = 119 };
/* the LED palette's entries used (schwung src/shared/constants.mjs) */
enum { C_OFF = 0, C_GREY = 119, C_LGREY = 118, C_WHITE = 120, C_DKBLUE = 97, C_BLUE = 17, C_LTBLUE = 48, C_DKGREEN = 83,
       C_GREEN = 126, C_RED = 127, C_YELLOW = 8, C_DKYELLOW = 73 };

/* the FM-1's buttons by their printed names, in the core's QD_B_* order (app.h) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP };

enum { MV_IDLE, MV_HOME, MV_SOUND, MV_FX, MV_MIX, MV_CORE };
enum { OV_NONE, OV_SETUP, OV_TEMPO };  /* a page of the Move's own over whatever is up */

/* Shift + a step button: the Move's own shortcuts, by the icons printed under the steps (0: none there) */
enum { SS_NONE, SS_PROJECTS, SS_SETUP, SS_TEMPO, SS_METRO, SS_MIDIFX, SS_DOUBLE };
static const uint8_t SHIFT_STEP[16] = {
    SS_PROJECTS,                       /* 1  set overview: the projects */
    SS_SETUP,                          /* 2  setup: the settings, and the way out */
    SS_NONE, SS_NONE,
    SS_TEMPO,                          /* 5  tempo */
    SS_METRO,                          /* 6  metronome */
    SS_NONE, SS_NONE, SS_NONE, SS_NONE,
    SS_MIDIFX,                         /* 11 arp / repeat: the MIDI FX */
    SS_NONE, SS_NONE,
    SS_DOUBLE,                         /* 14 double loop: the track doubled */
    SS_NONE, SS_NONE,
};
static const char *const SETUP_ITEMS[] = { "SETTINGS", "EXIT", "CLOSE" };
enum { SETUP_SETTINGS, SETUP_EXIT, SETUP_CLOSE, SETUP_N };

#define KNOB_DIV 4                     /* Move encoder counts to one core detent (the FM-1's knobs click; these do not) */
#define STEP_VEL 100                   /* a step button's velocity (it has no sensor) */
#define NAME_MS 700u                   /* a page's name stays this long */
#define TURN_MS 700u                   /* a knob's pop-up stays this long after its last turn */
#define RIDE_MS 450u                   /* the lift's ride */
#define FRAME_MS 33u
#define PAD_OCT 2                      /* the bottom-left pad: the scale's root in this octave (C3 in C) */

/* ------------------------------------------------------------------ the pages --- */
typedef struct { const char *page; int8_t k; } ent_t;   /* a value: the core's page by name, its knob (4: ALGORITHM) */
typedef struct { const char *name; uint8_t group, n; ent_t e[8]; } spage_t;

#define P4(p) {p, 0}, {p, 1}, {p, 2}, {p, 3}
static const spage_t SND[] = {
    /* B1: playing and the amp */
    {"AMP", 0, 8, {{"AMP", 0}, {"AMP", 1}, {"AMP", 2}, {"CRUSH", 0}, {"CRUSH", 1}, {"PORT", 0}, {"PORT", 1}, {"PORT", 2}}},
    {"VOICE", 0, 6, {P4("VOICE"), {"VOICE", 4}, {"PORT", 3}}},
    {"MOD", 0, 3, {{"MOD", 0}, {"MOD", 1}, {"MOD", 2}}},
    /* B2: the chip's three oscillators */
    {"OSC 1", 1, 5, {P4("OSC 1"), {"OSC 1", 4}}},
    {"OSC 2", 1, 5, {P4("OSC 2"), {"OSC 2", 4}}},
    {"OSC 3", 1, 5, {P4("OSC 3"), {"OSC 3", 4}}},
    /* B3: their envelopes */
    {"ENV 1", 2, 4, {P4("ENV 1")}},
    {"ENV 2", 2, 5, {P4("ENV 2"), {"ENV 2", 4}}},
    {"ENV 3", 2, 5, {P4("ENV 3"), {"ENV 3", 4}}},
    /* B4: the filter with its envelope, the chip */
    {"FILTER", 3, 8, {P4("FILTER"), P4("F ENV")}},
    {"CHIP", 3, 4, {{"CHIP", 0}, {"CHIP", 1}, {"CHIP", 2}, {"FILTER", 4}}},
    /* B5: the voice */
    {"WORD", 4, 5, {P4("WORD"), {"WORD", 4}}},
    {"SAY", 4, 5, {P4("SAY"), {"SAY", 4}}},
    {"DIGI", 4, 4, {P4("DIGI")}},
    /* B6..B8: an LFO and its wave */
    {"LFO 1", 5, 8, {P4("LFO 1"), P4("LFO 1 WAVE")}},
    {"LFO 2", 6, 8, {P4("LFO 2"), P4("LFO 2 WAVE")}},
    {"LFO 3", 7, 8, {P4("LFO 3"), P4("LFO 3 WAVE")}},
};
#define NSND ((int)(sizeof SND / sizeof SND[0]))
static int8_t g_snd_core[NSND][8];     /* each value's page in the core (QD_SND_PAGES' index) */

/* the FX tower's floors, top first: one of the core's FX pages, or its two side by side (ctl_mix.c QD_FX_PAGES) */
typedef struct { int8_t a, b; } fpage_t;
static const fpage_t FXP[] = { {0, 1}, {2, -1}, {3, 4}, {5, 6}, {7, -1}, {8, -1}, {9, -1}, {10, -1}, {11, -1} };
#define NFX ((int)(sizeof FXP / sizeof FXP[0]))

/* ------------------------------------------------------------------ rings: one writer, one reader each --- */
#define RING 512u
typedef struct { uint8_t p[RING][4]; volatile uint32_t head, tail; } ring_t;

static int ring_push(ring_t *r, const uint8_t p[4])
{
    const uint32_t h = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
    if (h - __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE) >= RING)
        return 0;
    memcpy(r->p[h % RING], p, 4);
    __atomic_store_n(&r->head, h + 1, __ATOMIC_RELEASE);
    return 1;
}

static int ring_pop(ring_t *r, uint8_t p[4])
{
    const uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
    if (t == __atomic_load_n(&r->head, __ATOMIC_ACQUIRE))
        return 0;
    memcpy(p, r->p[t % RING], 4);
    __atomic_store_n(&r->tail, t + 1, __ATOMIC_RELEASE);
    return 1;
}

/* ------------------------------------------------------------------ state --- */
static qd_app_t A;
static pthread_mutex_t g_mu;
static ring_t g_in, g_led;             /* audio -> UI: the surface's packets; UI -> audio: LED packets */

static struct {
    int mode, over;
    int spage, fpage;                  /* the SOUND page, the FX floor shown */
    int menu_sel;
    uint32_t home_up_ms;               /* the core's HOME held for it (its settings): let go then (0: not) */
    int shift, vol_touch, quit;
    int knob_acc[10];                  /* knobs 1..8, the wheel, the volume */
    int pot;                           /* MASTER, 0..1023 */
    int touch;                         /* the control touched: 0..7 a knob, 8 the volume, 9 the wheel; -1 none */
    int touch_turned;
    int last_knob;                     /* the knob turned last (0..7) .. */
    uint32_t turn_ms;                  /* .. then; 0: none */
    int oct;                           /* the note pads' octave shift */
    int8_t pad_note[24];               /* the note each pad struck (-1 up) */
    int win_top;                       /* the core screen's window (MV_CORE): its first logical row */
    int scr;                           /* the core's screen now */
    char name[20];                     /* the name showing .. */
    uint32_t name_until;               /* .. until then */
    char ride_from[12], ride_to[12];   /* the lift's ride .. */
    int ride_up;
    uint32_t ride_ms;                  /* .. begun then (0 none) */
    int home_set;
    uint32_t now, frame_ms;
    int have_frame;
    uint8_t led_note[128], led_cc[128];        /* what the surface shows now (0xFF: unknown, send) */
} S;

static uint16_t g_lcd[QD_W * QD_H];
static uint8_t g_last[MONO_PACKED];

/* ------------------------------------------------------------------ the pages' values --- */
typedef struct { char name[16], val[16]; int frac, ok; } kinfo_t;   /* frac 0..1000, < 0 none */

static int snd_frac(int page, int k, int v)
{
    const int8_t *ids = qd_snd_ids(&A, page);
    int16_t lo, hi;
    if (!ids || ids[k] < 0 || !qd_quad_value_range(ids[k], &lo, &hi, 0) || hi <= lo)
        return -1;
    v = v < lo ? lo : v > hi ? hi : v;
    return (v - lo) * 1000 / (hi - lo);
}

/* the SOUND page's record with the core's editor put on one of the core's pages (and put back) */
static void snd_view(int core_page, ui_snd_t *v)
{
    const int keep = A.ui.snd_page;
    A.ui.snd_page = core_page;
    memset(v, 0, sizeof *v);
    qd_view_snd(&A, v);
    A.ui.snd_page = keep;
}

static void fx_view(int core_page, ui_fx_t *v)
{
    const int keep = A.ui.fx_page;
    A.ui.fx_page = core_page;
    memset(v, 0, sizeof *v);
    qd_view_fx(&A, v);
    A.ui.fx_page = keep;
}

/* an FX floor's value i: its page in the core and the knob there; 0: none */
static int fx_ent(int i, int *page, int *k)
{
    const fpage_t *f = &FXP[S.fpage];
    *page = i < 4 ? f->a : f->b;
    *k = i & 3;
    return i < 8 && *page >= 0;
}

static int mix_native(void) { return S.mode == MV_MIX && A.ui.mix_page % QD_MIX_NPAGE == 0; }

static void knob_info(int i, kinfo_t *o)
{
    memset(o, 0, sizeof *o);
    o->frac = -1;
    if (i < 0 || i > 7)
        return;
    if (S.mode == MV_SOUND) {
        const spage_t *sp = &SND[S.spage];
        ui_snd_t v;
        int k;
        if (i >= sp->n)
            return;
        k = sp->e[i].k;
        snd_view(g_snd_core[S.spage][i], &v);
        if (!v.has[k] || !v.lab[k])
            return;
        snprintf(o->name, sizeof o->name, "%s", v.lab[k]);
        snprintf(o->val, sizeof o->val, "%s", v.txt[k]);
        o->frac = snd_frac(g_snd_core[S.spage][i], k, v.v[k]);
        o->ok = 1;
    } else if (S.mode == MV_FX) {
        ui_fx_t v;
        int page, k;
        if (!fx_ent(i, &page, &k))
            return;
        fx_view(page, &v);
        if (!v.has[k] || !v.lab[k])
            return;
        snprintf(o->name, sizeof o->name, "%s", v.lab[k]);
        snprintf(o->val, sizeof o->val, "%s", v.txt[k]);
        o->frac = (int)(v.x[k] * 1000.f);
        o->ok = 1;
    } else if (mix_native()) {
        ui_mix_t v;
        if (i > 6)
            return;
        memset(&v, 0, sizeof v);
        qd_view_mix(&A, &v);
        if (i < 6) {
            snprintf(o->name, sizeof o->name, "TRACK %d", i + 1);
            snprintf(o->val, sizeof o->val, "%d", v.tr[i].level);
            o->frac = v.tr[i].level * 1000 / 127;
        } else {
            const int pan = v.tr[A.sel].pan;
            snprintf(o->name, sizeof o->name, "PAN %d", A.sel + 1);
            if (pan)
                snprintf(o->val, sizeof o->val, "%c%d", pan < 0 ? 'L' : 'R', pan < 0 ? -pan : pan);
            else
                snprintf(o->val, sizeof o->val, "C");
            o->frac = (pan + 64) * 1000 / 127;
        }
        o->ok = 1;
    } else {                                            /* the core's own page: its knobs' jobs by name, no value */
        static const int LBL[8] = { QD_LBL_KNOB0 + QD_K_1, QD_LBL_KNOB0 + QD_K_2, QD_LBL_KNOB0 + QD_K_3,
                                    QD_LBL_KNOB0 + QD_K_4, QD_LBL_KNOB0 + QD_K_ALGO, QD_LBL_KNOB0 + QD_K_PRESETS, -1, -1 };
        qd_labels_t lb;
        if (LBL[i] < 0)
            return;
        qd_ctl_labels(&A, &lb);
        if (!lb.s[LBL[i]][0])
            return;
        snprintf(o->name, sizeof o->name, "%s", lb.s[LBL[i]]);
        o->ok = 1;
    }
}

/* ------------------------------------------------------------------ names and rides --- */
static void show_name(const char *name)
{
    snprintf(S.name, sizeof S.name, "%s", name);
    S.name_until = S.now + NAME_MS;
}

static const char *fx_name(int fpage) { return QD_FX_PAGES[FXP[fpage].a].name; }

static void fx_go(int fpage)
{
    if (fpage < 0 || fpage >= NFX || fpage == S.fpage)
        return;
    snprintf(S.ride_from, sizeof S.ride_from, "%s", fx_name(S.fpage));
    snprintf(S.ride_to, sizeof S.ride_to, "%s", fx_name(fpage));
    S.ride_up = fpage < S.fpage;                        /* (the first floor is the top) */
    S.ride_ms = S.now ? S.now : 1;
    /* the core's own change of page, so its lift calls the floor in the villain's voice */
    qd_app_knob(&A, QD_K_SELECT, FXP[fpage].a - A.ui.fx_page);
    S.fpage = fpage;
    snprintf(S.name, sizeof S.name, "%s", fx_name(fpage));
    S.name_until = S.now + RIDE_MS + NAME_MS;
}

static void snd_go(int spage)
{
    if (spage < 0 || spage >= NSND)
        return;
    S.spage = spage;
    A.ui.snd_page = g_snd_core[spage][0];
    show_name(SND[spage].name);
}

/* a group's key: its first page, or its next when the group is already up */
static void snd_group(int g)
{
    int i, first = -1, next = -1;
    for (i = 0; i < NSND; i++)
        if (SND[i].group == g) {
            if (first < 0)
                first = i;
            if (next < 0 && i > S.spage && SND[S.spage].group == g)
                next = i;
        }
    if (first >= 0)
        snd_go(SND[S.spage].group == g && next >= 0 ? next : first);
}

/* ------------------------------------------------------------------ input --- */
static void btn(int b, int down) { qd_app_button(&A, b, down); }
static void tap(int b) { btn(b, 1); btn(b, 0); }
static void key_tap(int key) { qd_app_key(&A, key, 1, STEP_VEL); qd_app_key(&A, key, 0, 0); }

/* something of the core's that must not see SHIFT, while Shift is down here: it is let go there for the call */
static void unshift(int on)
{
    if (S.shift)
        btn(B_GLO, on ? 0 : 1);
}

static int detents(int acc_i, int delta, int div)
{
    int *acc = &S.knob_acc[acc_i], d;
    *acc += delta;
    d = *acc / div;
    *acc -= d * div;
    return d;
}

static void knob(int i, int delta)
{
    const int d = detents(i, delta, KNOB_DIV);
    S.touch_turned = 1;
    S.last_knob = i;
    S.turn_ms = S.now ? S.now : 1;
    if (!d || S.over)
        return;
    if (S.mode == MV_SOUND) {
        const spage_t *sp = &SND[S.spage];
        if (i < sp->n) {
            const int keep = A.ui.snd_page;
            A.ui.snd_page = g_snd_core[S.spage][i];
            qd_app_knob(&A, sp->e[i].k == 4 ? QD_K_ALGO : QD_K_1 + sp->e[i].k, d);
            A.ui.snd_page = keep;
        }
    } else if (S.mode == MV_FX) {
        int page, k;
        if (fx_ent(i, &page, &k)) {
            const int keep = A.ui.fx_page;
            A.ui.fx_page = page;
            qd_app_knob(&A, QD_K_1 + k, d);
            A.ui.fx_page = keep;
        }
    } else if (mix_native()) {
        if (i < 6) {                                    /* a fader a track: the core's LEVEL knob with that track selected */
            const int keep = A.sel;
            A.sel = i;
            qd_app_knob(&A, QD_K_1, d);
            A.sel = keep;
        } else if (i == 6)
            qd_app_knob(&A, QD_K_2, d);                 /* the selected track's pan */
    } else {
        static const int K[8] = { QD_K_1, QD_K_2, QD_K_3, QD_K_4, QD_K_ALGO, QD_K_PRESETS, -1, -1 };
        if (K[i] >= 0)
            qd_app_knob(&A, K[i], d);
    }
}

static void wheel(int delta)
{
    const int d = detents(8, delta, 1);
    if (!d)
        return;
    if (S.over == OV_SETUP) {
        S.menu_sel = S.menu_sel + d < 0 ? 0 : S.menu_sel + d >= SETUP_N ? SETUP_N - 1 : S.menu_sel + d;
    } else if (S.over == OV_TEMPO) {
        const int bpm = qd_seq_bpm(&A.seq) + d;
        qd_ctl_set_tempo(&A, bpm < 30 ? 30 : bpm > 300 ? 300 : bpm);
    } else if (S.shift && S.mode != MV_CORE) {          /* Shift: the track's sound */
        unshift(1);
        qd_app_knob(&A, QD_K_PRESETS, d);
        unshift(0);
    } else if (S.mode == MV_SOUND) {
        snd_go(S.spage + d < 0 ? 0 : S.spage + d >= NSND ? NSND - 1 : S.spage + d);
    } else if (S.mode == MV_FX) {
        fx_go(S.fpage + d < 0 ? 0 : S.fpage + d >= NFX ? NFX - 1 : S.fpage + d);
    } else if (mix_native()) {
        const int t = A.sel + d;
        qd_ctl_select_track(&A, t < 0 ? 0 : t >= QD_TRACKS ? QD_TRACKS - 1 : t);
    } else
        qd_app_knob(&A, QD_K_SELECT, d);
}

static void wheel_click(int down)
{
    if (down && S.shift && S.vol_touch) {               /* the way out that always works */
        S.quit = 1;
        return;
    }
    if (S.over == OV_SETUP) {
        if (!down)
            return;
        S.over = OV_NONE;
        if (S.menu_sel == SETUP_EXIT)
            S.quit = 1;
        else if (S.menu_sel == SETUP_SETTINGS) {        /* the core's global settings: its HOME, held */
            btn(B_HOME, 1);
            S.home_up_ms = S.now + QD_HOME_HOLD_MS + 60u;
        }
        return;
    }
    if (S.over == OV_TEMPO) {
        if (down)
            S.over = OV_NONE;
        return;
    }
    if (S.mode == MV_CORE)                              /* the core's own menus: yes / enter (its OCT+) */
        btn(B_OCTUP, down);
}

/* Shift + a step: the Move's shortcut there */
static void shift_step(int i)
{
    switch (SHIFT_STEP[i]) {
    case SS_SETUP: S.over = S.over == OV_SETUP ? OV_NONE : OV_SETUP; S.menu_sel = 0; break;
    case SS_TEMPO: S.over = S.over == OV_TEMPO ? OV_NONE : OV_TEMPO; break;
    case SS_METRO: key_tap(QD_BLACK_KEY[6]); break;     /* the core's SHIFT + B7 (Shift is down there too) */
    case SS_DOUBLE: key_tap(QD_BLACK_KEY[8]); break;    /* SHIFT + B9 */
    case SS_PROJECTS: tap(B_SAVE); break;               /* SHIFT + SAVE */
    case SS_MIDIFX: tap(B_ARP); break;                  /* SHIFT + ARP */
    default: break;
    }
}

/* a note pad's note now: the selected track's scale from its root, a row up a fourth up (chromatic: five semitones) */
static int pad_note(int idx)
{
    int scale, root;
    qd_track_key(&A.proj, qd_ctl_cpat(&A), A.sel, &scale, &root);
    return qd_scale_note(scale, root, PAD_OCT + S.oct, (idx / 8) * (qd_scale_size(scale) >= 12 ? 5 : 3) + idx % 8);
}

static int pad_is_root(int idx)
{
    int scale, root;
    qd_track_key(&A.proj, qd_ctl_cpat(&A), A.sel, &scale, &root);
    return (pad_note(idx) - root) % 12 == 0;
}

static void note_pad(int idx, int vel)
{
    if (vel > 0) {
        const int n = pad_note(idx);
        if (S.pad_note[idx] >= 0)
            qd_app_midi(&A, 0x80 | QD_MIDI_SEL, S.pad_note[idx], 0);
        S.pad_note[idx] = (int8_t)n;
        qd_app_midi(&A, 0x90 | QD_MIDI_SEL, n, vel);
    } else if (S.pad_note[idx] >= 0) {
        qd_app_midi(&A, 0x80 | QD_MIDI_SEL, S.pad_note[idx], 0);
        S.pad_note[idx] = -1;
    }
}

/* the top pad row: the page keys. Eight pads, so where the core has more keys than that the last pad holds the rest
 * and steps through them: at home B8, B9, B10 (TRIG, FUNC, SOUND), in FX the last two floors */
static void page_pad(int col, int down, int vel)
{
    static int8_t key_down[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    if (S.mode == MV_SOUND) {
        if (down)
            snd_group(col);
    } else if (S.mode == MV_FX) {
        if (down)
            fx_go(col == 7 && S.fpage == 7 ? 8 : col);
    } else if (down) {
        int b = col;
        if (col == 7 && S.mode == MV_HOME)
            b = A.ui.hmode == 7 ? 8 : A.ui.hmode == 8 ? 9 : 7;
        key_down[col] = (int8_t)b;
        qd_app_key(&A, QD_BLACK_KEY[b], 1, vel);
    } else if (key_down[col] >= 0) {
        qd_app_key(&A, QD_BLACK_KEY[key_down[col]], 0, 0);
        key_down[col] = -1;
    }
}

static void surface_note(int note, int vel)
{
    static uint16_t step_eaten;                         /* a step whose press was a shortcut: its release is no key */
    const int down = vel > 0;
    if (note <= N_JOG_TOUCH) {
        if (note == N_VOL_TOUCH)
            S.vol_touch = down;
        if (down)
            S.touch = note, S.touch_turned = 0;
        else if (S.touch == note)
            S.touch = -1;
        return;
    }
    if (note >= N_STEP0 && note < N_STEP0 + 16) {
        const int i = note - N_STEP0;
        if (down && S.shift) {
            step_eaten |= (uint16_t)(1u << i);
            shift_step(i);
        } else if (!down && (step_eaten >> i & 1u)) {
            step_eaten &= (uint16_t)~(1u << i);
        } else
            qd_app_key(&A, QD_WHITE_KEY[i], down, STEP_VEL);
        return;
    }
    if (note >= N_PAD0 && note < N_PAD0 + 32) {
        const int idx = note - N_PAD0;                  /* row 0 the bottom */
        if (idx >= 24)
            page_pad(idx - 24, down, vel);
        else
            note_pad(idx, vel);
    }
}

static int rel(int v) { return v < 64 ? v : v - 128; }                      /* a relative encoder's count */

static void surface_cc(int cc, int v)
{
    const int down = v > 0;
    if (cc >= CC_KNOB1 && cc < CC_KNOB1 + 8) {
        knob(cc - CC_KNOB1, rel(v));
        return;
    }
    switch (cc) {
    case CC_JOG: S.touch_turned = 1; wheel(rel(v)); break;
    case CC_VOLUME:
        S.pot += rel(v) * 8;
        S.pot = S.pot < 0 ? 0 : S.pot > 1023 ? 1023 : S.pot;
        qd_app_master(&A, S.pot);
        break;
    case CC_JOG_CLICK: wheel_click(down); break;
    case CC_SHIFT: S.shift = down; btn(B_GLO, down); break;
    case CC_MENU:
        if (S.over) { if (down) S.over = OV_NONE; }
        else btn(B_HOME, down);
        break;
    case CC_BACK: {                                     /* out of a page (HOME), or a menu's step back (OCT-) */
        static int sent = -1;
        if (down) {
            if (S.over) {
                S.over = OV_NONE;
                sent = -1;
                break;
            }
            sent = S.mode == MV_SOUND || S.mode == MV_FX || S.mode == MV_MIX || S.scr == QD_SCR_MFX ||
                   S.scr == QD_SCR_TOOLS || S.scr == QD_SCR_GEEK || S.scr == QD_SCR_OV ? B_HOME : B_OCTDN;
        }
        if (sent >= 0)
            btn(sent, down);
        break;
    }
    case CC_CAPTURE:                                    /* FX; again: the next floor. Shift: the MIDI FX */
        if (S.shift)
            btn(B_ARP, down);
        else if (S.mode == MV_FX) {
            if (down)
                fx_go((S.fpage + 1) % NFX);
        } else
            btn(B_FX, down);
        break;
    case CC_SAMPLE:                                     /* the sound editor. Shift: SAVE PRESET */
        if (S.shift) {
            if (down) { unshift(1); tap(B_SAVE); unshift(0); }
        } else
            btn(B_EDIT, down);
        break;
    case CC_MUTE: btn(B_ENV, down); break;              /* the mix; again: its sends, the FX mix */
    case CC_LOOP: btn(B_SEQ, down); break;
    case CC_COPY:                                       /* the pattern copied; Shift: pasted (SHIFT + B4 / B5) */
        if (down) {
            if (!S.shift) btn(B_GLO, 1);
            key_tap(QD_BLACK_KEY[S.shift ? 4 : 3]);
            if (!S.shift) btn(B_GLO, 0);
        }
        break;
    case CC_DELETE: qd_app_key(&A, QD_BLACK_KEY[10], down, STEP_VEL); break;   /* B11: clear (Shift: the track) */
    case CC_PLAY: btn(B_PLAY, down); break;
    case CC_REC:                                        /* armed: the pads' notes are recorded (stopped: a capture
                                                         * that starts with the first note); again: off */
        if (down) {
            if (A.ui.rec)
                qd_rec_toggle(&A);
            else
                qd_rec_arm_live(&A);
        }
        break;
    case CC_UP:                                         /* F2. Shift: the pads an octave up (the core's own page: its window) */
        if (!S.shift) btn(B_OCTUP, down);
        else if (down && S.mode == MV_CORE) { if (S.win_top > 0) S.win_top -= 28; }
        else if (down && S.oct < 4) S.oct++;
        break;
    case CC_DOWN:
        if (!S.shift) btn(B_OCTDN, down);
        else if (down && S.mode == MV_CORE) { if (S.win_top < 56) S.win_top += 28; }
        else if (down && S.oct > -2) S.oct--;
        break;
    case CC_LEFT:                                       /* the page of 16 steps the buttons show */
    case CC_RIGHT:
        if (down) {
            const int pg = A.ui.page[A.sel] + (cc == CC_RIGHT ? 1 : -1);
            char nm[12];
            qd_ctl_set_page(&A, pg);
            snprintf(nm, sizeof nm, "PAGE %d", A.ui.page[A.sel] + 1);
            show_name(nm);
        }
        break;
    case CC_UNDO:                                       /* SHIFT + F1 undo, SHIFT + F2 redo */
        if (down) {
            if (!S.shift) btn(B_GLO, 1);
            tap(S.shift ? B_OCTUP : B_OCTDN);
            if (!S.shift) btn(B_GLO, 0);
        }
        break;
    default:
        if (cc >= CC_TRACK4 && cc <= CC_TRACK1 && down) {   /* Shift: tracks 5 and 6 on the first two */
            const int n = CC_TRACK1 - cc;
            qd_ctl_select_track(&A, S.shift && n < 2 ? n + 4 : n);
        }
        break;
    }
}

static void input(const uint8_t p[4])
{
    const int cable = p[0] >> 4, st = p[1] & 0xF0;
    if (cable == 2) {                                   /* USB-A: MIDI in, as the FM-1's */
        if (p[1] >= 0x80)
            qd_app_midi(&A, p[1], p[2], p[3]);
        return;
    }
    if (cable != 0)
        return;
    if (st == 0x90) surface_note(p[2], p[3]);
    else if (st == 0x80) surface_note(p[2], 0);
    else if (st == 0xB0) surface_cc(p[2], p[3]);
}

/* ------------------------------------------------------------------ the mode: what the core has open --- */
static int mode_now(void)
{
    const qd_ui_t *u = &A.ui;
    if (qd_view_screen(&A) == QD_SCR_IDLE)
        return MV_IDLE;
    if (u->mode != QD_MODE_STEP)
        return MV_CORE;
    if (u->latch == QD_LATCH_SND)
        return qd_msnd_is(A.model.t[A.sel].patch) ? MV_CORE : MV_SOUND;   /* (a MIDI track's editor: the core's) */
    if (u->latch == QD_LATCH_FX)
        return MV_FX;
    if (u->latch == QD_LATCH_MIX)
        return MV_MIX;
    return u->latch == QD_LATCH_NONE ? MV_HOME : MV_CORE;
}

static const char *screen_name(int scr)                 /* the core's own pages' names */
{
    switch (scr) {
    case QD_SCR_KEYS: return "KEYS";
    case QD_SCR_MENU: return "SETTINGS";
    case QD_SCR_BROWSE: return "BROWSER";
    case QD_SCR_PRJ: return "PROJECTS";
    case QD_SCR_SAVE: return "SAVE";
    case QD_SCR_MFX: return "MIDI FX";
    case QD_SCR_GEEK: return "GEEK OUT";
    case QD_SCR_TOOLS: return "TOOLS";
    case QD_SCR_OV: return "OVERVIEW";
    default: return "";
    }
}

static void sync(void)
{
    const int m = mode_now(), scr = qd_view_screen(&A);
    if (m != S.mode) {
        S.mode = m;
        S.win_top = 0;
        S.turn_ms = 0;
        if (m == MV_SOUND) show_name(SND[S.spage].name);
        else if (m == MV_FX) show_name(fx_name(S.fpage));
        else if (m == MV_MIX) show_name("MIX");
        else if (m == MV_HOME && !S.home_set) {         /* out of the title, once: the room is home here */
            S.home_set = 1;
            if (A.ui.view != QD_VIEW_EDIT)
                qd_ctl_set_view(&A, QD_VIEW_EDIT);
        }
    }
    if (scr != S.scr) {
        S.scr = scr;
        if (m == MV_CORE && *screen_name(scr))
            show_name(screen_name(scr));
    }
    if (m == MV_SOUND)                                  /* the core's editor follows the page here */
        A.ui.snd_page = g_snd_core[S.spage][0];
    else if (m == MV_FX && A.ui.fx_page != FXP[S.fpage].a && A.ui.fx_page != FXP[S.fpage].b)
        A.ui.fx_page = FXP[S.fpage].a;
}

/* ------------------------------------------------------------------ LEDs --- */
static uint8_t shade(int level, int dim, int mid, int lit)
{
    return (uint8_t)(level == 0 ? C_OFF : level <= QD_LED_DIM ? dim : level <= QD_LED_ROOT ? mid : lit);
}

static void led_note(int note, uint8_t colour)
{
    const uint8_t p[4] = { 0x09, 0x90, (uint8_t)note, colour };
    if (S.led_note[note] != colour && ring_push(&g_led, p))
        S.led_note[note] = colour;
}

static void led_cc(int cc, uint8_t v)
{
    const uint8_t p[4] = { 0x0B, 0xB0, (uint8_t)cc, v };
    if (S.led_cc[cc] != v && ring_push(&g_led, p))
        S.led_cc[cc] = v;
}

static uint8_t white(int level) { return (uint8_t)(level / 2 < 6 ? 6 : level / 2 > 124 ? 124 : level / 2); }

static void leds(void)
{
    qd_leds_t L;
    int i;
    qd_app_leds(&A, &L);
    for (i = 0; i < 16; i++) {
        if (S.shift)                                    /* Shift: the steps that are shortcuts, the open one bright */
            led_note(N_STEP0 + i, (uint8_t)(!SHIFT_STEP[i] ? C_OFF
                                            : (SHIFT_STEP[i] == SS_SETUP && S.over == OV_SETUP) ||
                                              (SHIFT_STEP[i] == SS_TEMPO && S.over == OV_TEMPO) ? C_WHITE : C_LTBLUE));
        else
            led_note(N_STEP0 + i, shade(L.key[QD_WHITE_KEY[i]], C_GREY, C_LGREY, C_WHITE));
    }
    for (i = 0; i < 24; i++)                            /* the note pads: struck white, a root green, the rest dim */
        led_note(N_PAD0 + i, (uint8_t)(S.mode == MV_IDLE ? C_OFF : S.pad_note[i] >= 0 ? C_WHITE
                                                                 : pad_is_root(i) ? C_GREEN : C_GREY));
    for (i = 0; i < 8; i++) {                           /* the page keys */
        uint8_t c;
        if (S.mode == MV_SOUND)
            c = (uint8_t)(SND[S.spage].group == i ? C_WHITE : C_BLUE);
        else if (S.mode == MV_FX)
            c = (uint8_t)(S.fpage == i || (i == 7 && S.fpage == 8) ? C_WHITE : C_BLUE);
        else {
            int lv = L.key[QD_BLACK_KEY[i]];
            if (i == 7 && S.mode == MV_HOME) {          /* (the pad of three keys: the brightest of them) */
                if (L.key[QD_BLACK_KEY[8]] > lv) lv = L.key[QD_BLACK_KEY[8]];
                if (L.key[QD_BLACK_KEY[9]] > lv) lv = L.key[QD_BLACK_KEY[9]];
            }
            c = shade(lv, C_DKBLUE, C_BLUE, C_LTBLUE);
        }
        led_note(N_PAD0 + 24 + i, c);
    }
    led_cc(CC_SHIFT, white(L.button[B_GLO]));
    led_cc(CC_MENU, white(L.button[B_HOME]));
    led_cc(CC_BACK, (uint8_t)(S.over || (S.mode != MV_HOME && S.mode != MV_IDLE) ? 64 : 6));
    led_cc(CC_CAPTURE, (uint8_t)(S.mode == MV_FX ? 124 : 16));
    led_cc(CC_MUTE, (uint8_t)(S.mode == MV_MIX ? 124 : 16));
    led_cc(CC_DOWN, white(L.button[B_OCTDN]));
    led_cc(CC_UP, white(L.button[B_OCTUP]));
    led_cc(CC_LOOP, white(L.button[B_SEQ]));
    led_cc(CC_COPY, 16);
    led_cc(CC_DELETE, white(L.key[QD_BLACK_KEY[10]]));
    led_cc(CC_UNDO, 16);
    led_cc(CC_LEFT, 16);
    led_cc(CC_RIGHT, 16);
    led_cc(CC_PLAY, L.play_green ? C_GREEN : shade(L.button[B_PLAY], C_GREY, C_LGREY, C_WHITE));
    led_cc(CC_REC, A.ui.rec ? C_RED : C_OFF);
    led_cc(CC_SAMPLE, (uint8_t)(S.mode == MV_SOUND ? C_WHITE : C_GREY));
    for (i = 0; i < 4; i++)                             /* the track buttons: the selected one (5 and 6: the first two, dim) */
        led_cc(CC_TRACK1 - i, (uint8_t)(A.sel == i ? C_YELLOW : A.sel == i + 4 && i < 2 ? C_DKYELLOW : C_GREY));
}

/* ------------------------------------------------------------------ the API --- */
static int snd_page_index(const char *name)
{
    int p;
    for (p = 0; p < QD_SND_NPAGE; p++)
        if (!strcmp(QD_SND_PAGES[p].name, name))
            return p;
    fprintf(stderr, "egm: the core has no sound page \"%s\"\n", name);
    return 0;
}

void egm_init(const char *flash_path)
{
    pthread_mutexattr_t at;
    int i, j;
    pthread_mutexattr_init(&at);
#ifdef __linux__
    pthread_mutexattr_setprotocol(&at, PTHREAD_PRIO_INHERIT);
#endif
    pthread_mutex_init(&g_mu, &at);
    memset(&S, 0, sizeof S);
    memset(S.led_note, 0xFF, sizeof S.led_note);
    memset(S.led_cc, 0xFF, sizeof S.led_cc);
    memset(S.pad_note, -1, sizeof S.pad_note);
    S.touch = -1;
    S.pot = QD_MASTER_POT_DEFAULT;
    S.mode = MV_IDLE;
    S.scr = QD_SCR_IDLE;
    S.spage = 3;                                        /* OSC 1: the sound's first page */
    S.fpage = 1;                                        /* CHORUS */
    for (i = 0; i < NSND; i++)
        for (j = 0; j < SND[i].n; j++)
            g_snd_core[i][j] = (int8_t)snd_page_index(SND[i].e[j].page);
    qd_app_init(&A);
    qd_persist_attach(&A, flash_open(flash_path));
    qd_app_master(&A, S.pot);
    qd_app_tick(&A, 0);
}

void egm_shutdown(void)
{
    pthread_mutex_lock(&g_mu);
    qd_persist_flush(&A);
    pthread_mutex_unlock(&g_mu);
    flash_flush();
}

void egm_midi_in(const uint8_t pkt[4]) { ring_push(&g_in, pkt); }

void egm_audio(int16_t *stereo, int frames)
{
    pthread_mutex_lock(&g_mu);
    qd_app_audio(&A, stereo, (uint32_t)frames);
    pthread_mutex_unlock(&g_mu);
}

int egm_leds_take(uint8_t out[][4], int max)
{
    int n = 0;
    while (n < max && ring_pop(&g_led, out[n]))
        n++;
    return n;
}

int egm_ui_step(uint32_t now_ms)
{
    uint8_t p[4];
    const int have = __atomic_load_n(&g_in.head, __ATOMIC_ACQUIRE) != g_in.tail;
    S.now = now_ms;
    if (have)
        qd_app_input_prepare(&A);                       /* the undo's snapshot: outside the lock (app.h) */
    pthread_mutex_lock(&g_mu);
    while (ring_pop(&g_in, p)) {
        input(p);
        sync();
    }
    qd_app_tick_locked(&A, now_ms);
    if (S.home_up_ms && (int32_t)(now_ms - S.home_up_ms) >= 0) {
        S.home_up_ms = 0;
        btn(B_HOME, 0);
    }
    sync();
    pthread_mutex_unlock(&g_mu);
    qd_app_tick_free(&A);
    flash_poll(now_ms);
    leds();
    return S.quit;
}

static void core_frame(uint32_t now_ms)
{
    if (!S.have_frame || now_ms - S.frame_ms >= FRAME_MS) {
        qd_app_frame(&A, g_lcd);
        S.frame_ms = now_ms, S.have_frame = 1;
    }
}

static int osc_wave(int w)
{
    return w == QD_SW_OFF ? SCR_W_OFF : w == QD_SW_TRI ? SCR_W_TRI : w == QD_SW_SAW ? SCR_W_SAW
         : w == QD_SW_PUL ? SCR_W_PULSE : w == QD_SW_NOI ? SCR_W_NOISE : SCR_W_MIX;
}

static void page_draw(mono_t *m, uint32_t now_ms)
{
    if (S.mode == MV_SOUND) {
        const int page = g_snd_core[S.spage][0];
        ui_snd_t v;
        snd_view(page, &v);
        if (v.draw == UI_SD_OSC) {
            const int pw = snd_frac(page, 3, v.v[3]);
            scr_osc(m, osc_wave(v.v[0]), pw < 0 ? 500 : pw, now_ms);
            return;
        }
        if (v.draw == UI_SD_ENV) {
            scr_env(m, snd_frac(page, 0, v.v[0]), snd_frac(page, 1, v.v[1]), snd_frac(page, 2, v.v[2]),
                    snd_frac(page, 3, v.v[3]));
            return;
        }
        if (v.draw == UI_SD_FILTER) {
            const char c = v.txt[4][0];                 /* the mode by its name: OFF, LOW .., BAND .., HIGH .. */
            scr_filter(m, !strcmp(v.txt[4], "OFF") ? 0 : c == 'H' ? 3 : c == 'B' || c == 'N' ? 2 : 1,
                       snd_frac(page, 0, v.v[0]), snd_frac(page, 1, v.v[1]));
            return;
        }
        core_frame(now_ms);                             /* the core's own picture of the page, without its labels */
        scr_core_crop(m, g_lcd, 0, 14);
    } else if (S.mode == MV_FX) {
        core_frame(now_ms);
        scr_core_crop(m, g_lcd, 12, 22);                /* (its lift's shaft at the left: not shown) */
    } else if (mix_native()) {
        ui_mix_t v;
        int level[6], mute[6], meter[6], i;
        memset(&v, 0, sizeof v);
        qd_view_mix(&A, &v);
        for (i = 0; i < 6; i++)
            level[i] = v.tr[i].level, mute[i] = v.tr[i].mute, meter[i] = (int)(v.tr[i].meter * 1000.f);
        scr_mix(m, level, mute, meter, 6, A.sel);
    } else {
        core_frame(now_ms);
        if (S.mode == MV_HOME && S.scr == QD_SCR_EDIT)
            scr_core_crop(m, g_lcd, 0, 24);             /* the room, between its head and its foot */
        else if (S.mode == MV_MIX)
            scr_core_crop(m, g_lcd, 0, 14);
        else
            scr_core_crop(m, g_lcd, 0, S.win_top);
    }
}

int egm_frame(uint8_t packed[MONO_PACKED], uint32_t now_ms)
{
    const int pages = S.mode == MV_SOUND || S.mode == MV_FX || mix_native();
    const int turned = S.turn_ms && now_ms - S.turn_ms < TURN_MS;
    const int kn = S.touch >= 0 && S.touch < 8 ? S.touch : turned ? S.last_knob : -1;
    mono_t m;
    kinfo_t ki;
    int changed, done = 0;
    if (S.mode == MV_IDLE && !S.over) {
        scr_intro(&m, now_ms);
        done = 1;
    } else if (S.over == OV_SETUP) {
        scr_menu(&m, SETUP_ITEMS, SETUP_N, S.menu_sel);
        done = 1;
    } else if (S.over == OV_TEMPO) {
        char v[8];
        const int bpm = qd_seq_bpm(&A.seq);
        snprintf(v, sizeof v, "%d", bpm);
        scr_knob(&m, "TEMPO", v, (bpm - 30) * 1000 / 270);
        done = 1;
    } else if (A.ui.cf_on && A.ui.cf_big && A.ui.cf_big[0]) {   /* the core's word for what just happened (CLEARED, LIVE REC) */
        scr_page_name(&m, A.ui.cf_big);
        done = 1;
    } else if (S.ride_ms && now_ms - S.ride_ms < RIDE_MS) {
        scr_lift(&m, S.ride_from, S.ride_to, S.ride_up, (int)((now_ms - S.ride_ms) * 1000u / RIDE_MS));
        done = 1;
    } else if (kn >= 0) {
        knob_info(kn, &ki);
        /* a value known: while touched or just turned. Only a name: while touched and not yet turned (the core's own
         * picture shows the value then) */
        if (ki.ok && (ki.val[0] || (S.touch == kn && !S.touch_turned))) {
            scr_knob(&m, ki.name, ki.val, ki.frac);
            done = 1;
        }
    } else if (S.touch == N_JOG_TOUCH && pages) {       /* a finger on the wheel: every knob at once */
        static kinfo_t all[8];
        const char *lab[8], *val[8];
        int frac[8], i;
        for (i = 0; i < 8; i++) {
            knob_info(i, &all[i]);
            lab[i] = all[i].ok ? all[i].name : "";
            val[i] = all[i].val;
            frac[i] = all[i].frac;
        }
        scr_eight(&m, lab, val, frac, 8);
        done = 1;
    }
    if (!done && (int32_t)(S.name_until - now_ms) > 0 && S.name[0]) {
        scr_page_name(&m, S.name);
        done = 1;
    }
    if (!done)
        page_draw(&m, now_ms);
    mono_pack(&m, packed);
    changed = memcmp(packed, g_last, MONO_PACKED) != 0;
    memcpy(g_last, packed, MONO_PACKED);
    return changed;
}

void egm_state(int *track, int *screen, int *playing)
{
    *track = A.sel;
    *screen = qd_view_screen(&A);
    *playing = A.seq.playing;
}

const uint16_t *egm_core_lcd(void)
{
    qd_app_frame(&A, g_lcd);
    return g_lcd;
}
