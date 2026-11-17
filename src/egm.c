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
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "chord.h"
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
#include "perform.h"
#include "names.h"

void qd_view_edit(qd_app_t *a, ui_held_t *h, ui_lane_t *l);
void qd_view_browse(const qd_app_t *a, ui_browse_t *v);        /* .. the preset browser's */    /* view.c: the EDIT view's records (the room's lane) */

/* ------------------------------------------------------------------ the Move's surface (schwung docs/API.md) --- */
enum { N_KNOB_TOUCH0 = 0, N_VOL_TOUCH = 8, N_JOG_TOUCH = 9, N_STEP0 = 16, N_PAD0 = 68 };
enum { CC_JOG_CLICK = 3, CC_JOG = 14, CC_TRACK4 = 40, CC_TRACK1 = 43, CC_SHIFT = 49, CC_MENU = 50, CC_BACK = 51,
       CC_CAPTURE = 52, CC_DOWN = 54, CC_UP = 55, CC_UNDO = 56, CC_LOOP = 58, CC_COPY = 60, CC_LEFT = 62, CC_RIGHT = 63,
       CC_STEP_ICON0 = 16, CC_KNOB1 = 71, CC_VOLUME = 79, CC_PLAY = 85, CC_REC = 86, CC_MUTE = 88, CC_SAMPLE = 118, CC_DELETE = 119 };
/* the LED palette's entries used (schwung src/shared/constants.mjs) */
enum { C_OFF = 0, C_GREY = 119, C_LGREY = 118, C_WHITE = 120, C_DKBLUE = 97, C_BLUE = 17, C_LTBLUE = 48, C_DKGREEN = 83,
       C_GREEN = 126, C_RED = 127, C_YELLOW = 8, C_DKYELLOW = 73 };

/* the FM-1's buttons by their printed names, in the core's QD_B_* order (app.h) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP };

enum { MV_IDLE, MV_HOME, MV_SOUND, MV_FX, MV_MIX, MV_BROWSE, MV_CORE };
enum { OV_NONE, OV_SETUP, OV_TEMPO, OV_POWER, OV_PROJECTS };  /* a page of the Move's own over whatever is up */

/* Shift + a step button: the Move's own shortcuts, by the icons printed under the steps (0: none there) */
enum { SS_NONE, SS_PROJECTS, SS_SETUP, SS_TEMPO, SS_METRO, SS_MIDIFX, SS_NEWCLIP, SS_DOUBLE };
static const uint8_t SHIFT_STEP[16] = {
    SS_PROJECTS,                       /* 1  set overview: the projects */
    SS_SETUP,                          /* 2  setup: the settings, and the way out */
    SS_NONE, SS_NONE,
    SS_TEMPO,                          /* 5  tempo */
    SS_METRO,                          /* 6  metronome */
    SS_NONE, SS_NONE, SS_NONE, SS_NONE,
    SS_MIDIFX,                         /* 11 arp / repeat: the MIDI FX */
    SS_NONE, SS_NONE,
    SS_NEWCLIP,                        /* 14 new clip: the track's next empty slot chosen */
    SS_DOUBLE,                         /* 15 double loop: the clip doubled */
    SS_NONE,
};
static const char *const SETUP_ITEMS[] = { "SETTINGS", "EXIT", "CLOSE" };
enum { SETUP_SETTINGS, SETUP_EXIT, SETUP_CLOSE, SETUP_N };
static const char *const POWER_ITEMS[] = { "CANCEL", "POWEROFF" };

#define KNOB_DIV 4                     /* Move encoder counts to one core detent (the FM-1's knobs click; these do not) */
#define STEP_VEL 100                   /* a step button's velocity (it has no sensor) */
#define NAME_MS 700u                   /* a page's name stays this long */
#define TURN_MS 700u                   /* a knob's pop-up stays this long after its last turn */
#define RIDE_MS 450u                   /* the lift's ride */
#define FRAME_MS 33u
#define PAD_OCT 2                      /* the bottom-left pad: the scale's root in this octave (C3 in C) */
#define HOLD_MS 400u                   /* a track button down this long: held (its top row while held), not tapped */
#define DOUBLE_MS 350u                 /* Mute held: two taps on a track pad this close: solo */
#define RND_MS 1000u                   /* RND PAGE / RND SOUND held this long: shaken */
enum { SQ_TRACKS, SQ_SOUND, SQ_FX, SQ_TOOLS, SQ_N };
static const char *const SQ_NAME[SQ_N] = { "TRACKS", "SOUND", "FX", "TOOLS" };
/* Session's four modes (Menu: Sequence <-> Session): CLIPS (the pads the clips, 8 tracks x 4 slots), MIDI FX, FX on
 * the mix, MIX. A clip is a pattern's part of a track (the core's clips, seq.h): slot k is pattern k */
enum { SN_CLIPS, SN_MFX, SN_FX, SN_MIX, SN_SCENES, SN_ARRANGE, SN_SEQ, SN_N };   /* (the four track buttons', then the
                                                         * belt's) */
static const char *const SN_NAME[SN_N] = { "CLIPS", "MIDI FX", "FX", "MIX", "SCENES", "ARRANGE", "SEQ" };
#define NSLOT 16                       /* clip slots a track (A01..A16) */
#define LAUNCH_Q (A.proj.launch)       /* a clip launched or stopped: the project's launch quantize */
/* the launch quantize as Shift + the wheel turns it in CLIPS: now (a 16th), a beat, 1, 2, 4, 8 bars, the clip's end */
static const uint8_t LAUNCH_ORDER[] = { QD_LAUNCH_NOW, QD_LAUNCH_BEAT, QD_LAUNCH_BAR, QD_LAUNCH_BAR2, QD_LAUNCH_BAR4,
                                        QD_LAUNCH_BAR8, QD_LAUNCH_END };
static const uint8_t TRK_COL[8] = { 1, 3, 7, 11, 16, 17, 22, 25 };   /* the tracks' colours (the palette's) .. */
static const uint8_t TRK_DIM[8] = { 65, 69, 77, 85, 95, 97, 107, 113 };   /* .. dim */
/* TRIG / SOUND's page pads 1-6: the sound pages each cycles through (SND's indices), the first the default. Pads 7, 8:
 * RND PAGE and RND SOUND (held a second) */
static const int8_t SND_PAD[6][4] = {
    { 0, 1, -1, -1 },                  /* TRIG: AMP (with CRUSH and PORT), VOICE */
    { 3, 4, 5, 2 },                    /* OSC 1, 2, 3, MOD */
    { 11, 12, 13, -1 },                /* SPEECH: WORD, SAY, DIGI */
    { 9, 10, -1, -1 },                 /* FILTER (with its envelope), CHIP */
    { 6, 7, 8, -1 },                   /* ENV 1, 2, 3 */
    { 14, 15, 16, -1 },                /* LFO 1, 2, 3 */
};
/* the eight sends a held track pad's knobs turn, and the hold popups' ids (knob_info: 20 + one of these) */
enum { TS_VOL, TS_PAN, TS_CHORUS, TS_ECHO, TS_ROOM, TS_CRUSH, TS_ROBOT, TS_WARP, TS_GLITCH, TS_TV, TS_N };
static const char *const TS_NAME[TS_N] = { "VOLUME", "PAN", "CHORUS", "ECHO", "ROOM", "CRUSH", "ROBOT", "WARP", "GLITCH", "TV" };
#define KI_MICRO 10                    /* knob_info's ids past the eight knobs: a held step's micro and velocity .. */
#define KI_VEL 11
#define KI_MASTER 12                   /* .. the main volume */
#define KI_TRACK 20                    /* .. a held track pad's VOLUME .. TV (20 + TS_*) */

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

/* HOME: a step's values on the eight knobs (a step held: that step's; none held: the track's defaults). These are the
 * FM-1's hold modes, which there are picked with the black keys and turned on one knob; here each has its own. NOTE
 * needs none (the pads play the notes in) and SOUND is Shift + the wheel. Shift + a knob: the mode's second value
 * (LEN's tie, MICRO's strum, RATCH's ramp, CHANCE's condition, COND's chance); Shift + the last: the step function */
static const uint8_t HOME_MODE[8] = { UI_F_VEL, UI_F_LEN, UI_F_MICRO, UI_F_RATCH, UI_F_CHNC, UI_F_COND, UI_F_TRIG, UI_F_SLIDE };
static const char *const HOME_NAME[8] = { "VELOCITY", "LENGTH", "MICRO", "RATCHET", "CHANCE", "CONDITION", "TRIG", "SLIDE" };

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
    int shift, vol_touch, quit;        /* quit: 1 back to Move, 2 the power off */
    int mute_down, mute_used;          /* Mute held, and used on a track (its release is then no tap) */
    int loop_down;                     /* Shift + Loop held: the steps are the patterns (until Session) */
    /* page mode (Loop tapped: latched; Loop held: while held): the steps are the clip's pages, rows 3-4 the shown
     * page's start and end, the wheel its length */
    int loop_btn, page_latch, loop_used;
    uint32_t loop_ms;
    int pg_held;                       /* a page step held (-1 none) .. */
    uint32_t pg_ms, pg_tap_ms[16];     /* .. since then; each page's last tap (a double tap: loop only it) */
    int pg_used;
    unsigned pg_solo_restore;          /* the loop before a page was looped alone (0 none) */
    int pg_copied;                     /* Copy + a page: the page copied (-1 none) */
    int copy_down, delete_down;
    int pd_held;                       /* rows 3-4: a step of the page held (-1 none), its last tap .. */
    uint32_t pd_tap_ms[16];
    int pd_prev_start;                 /* (the start before a tap: a double tap sets the end, the start kept) */
    int pw_stage;                      /* the power button's SysEx: how far into it .. */
    uint8_t pw_cmd[3], pw_status;      /* .. its report, the last one's bits */
    int knob_acc[10];                  /* knobs 1..8, the wheel, the volume */
    int pot;                           /* MASTER, 0..1023 */
    int touch;                         /* the control touched: 0..7 a knob, 8 the volume, 9 the wheel; -1 none */
    int touch_turned;
    int last_knob;                     /* the knob turned last (0..7) .. */
    uint32_t turn_ms;                  /* .. then; 0: none */
    int oct;                           /* the note pads' octave shift */
    int8_t pad_note[32];               /* the note each pad struck (-1 up); 24..31: the top row played as keys */
    /* Sequence's four modes on the track buttons (design/WORKFLOW.md): 0 TRACKS, 1 TRIG / SOUND, 2 FX, 3 TOOLS. A
     * button brings its mode's top row; the active one tapped hides it (all four rows keys) until a button is tapped
     * again; held, its top row only while held */
    int seq_mode, top_hidden;
    int tb_down;                       /* the track button held (-1 none) .. */
    uint32_t tb_ms;                    /* .. since then */
    int tb_prev_mode, tb_prev_hidden;  /* .. and what was up before it */
    int tpad;                          /* a track pad held (TRACKS' top row, -1 none): Volume, the wheel, the knobs are its */
    int tpad_used;
    uint32_t mute_tap_ms[8];           /* Mute held: each track pad's last tap (a double tap solos) */
    int8_t grp_last[6];                /* TRIG / SOUND: the page each page pad showed last */
    /* Session (Menu): the track buttons are its four modes; CLIPS: the pads the clips, 8 tracks x 4 slots from srow,
     * the step buttons in pairs under each track (stop, mute) */
    int jog_touch;                     /* a finger on the wheel (whatever else is touched) */
    unsigned mk_touch, mk_turned;      /* Mute held: the knobs touched (bit n), and turned since (their volume, no mute) */
    /* PROJECTS (Shift + step 1): a project taken by Copy (-1), one armed by Delete (-1); the name ideas knob 1 turns */
    int pj_take, pj_del, pj_nidea, pj_idea;
    char pj_ideas[16][QD_NAME_MAX + 1];
    int mod_hold, mod_used;            /* TRIG / SOUND: ENV (4) or LFO (5) held (-1 none), a knob turned meanwhile .. */
    char mod_name[16], mod_val[16];    /* .. what the turn mapped (the knob's popup while held) */
    int session, sn_mode, srow;
    int sn_keep, seq_keep;             /* each view's mode when it was left (Menu brings it back) */
    int8_t tsel[8];                    /* a slot chosen on a track with no clip there (-1: the one playing or queued) */
    uint32_t cl_tap_ms[32];            /* each clip pad's last tap (two: its track stopped) */
    int cl_take;                       /* Copy / Delete + a clip: the one taken (track * NSLOT + slot; -1 none) .. */
    int cl_cut;                        /* .. by Delete: cut (kept aside, pasted where tapped next) */
    uint32_t left_ms;                  /* Left's last tap in CLIPS (two: back to the first slots) */
    int copy_used;                     /* Copy held and a clip tapped (released alone: the clip duplicated) */
    int mix_k;                         /* Session's MIX: what the knobs and the pad columns set (TS_*) */
    int mix_hide;                      /* .. its top row hidden (button 4 again): all four rows the columns */
    int8_t mxg_row[8];                 /* .. each column's pad held (its row; -1 none): the value glides to its height .. */
    uint8_t mxg_pres[8];               /* .. as fast as the pad is pressed (its velocity, then its aftertouch) */
    float mxg_v[8];
    uint32_t mxg_ms;
    int mfx_pad, mfx_dbl;
    uint32_t mfx_k1_ms;                /* MIDI FX: knob 1 last chose an effect then */              /* the MIDI FX page: a slot pad held (-1 none); a double tap's release to eat */
    uint32_t mfx_tap_ms[8];            /* .. each slot pad's last tap (two: the slot bypassed) */
    uint32_t mx_tap_ms[24];            /* .. each column pad's last tap (two: the default) */
    int undo_cur;                      /* (a clip operation's level: the pattern edited before it) */
    int proj_seen;                     /* the project the clips were last looked at in */
    /* Session's page belt (the wheel in CLIPS and MIX): open, the page chosen */
    int belt, belt_sel;
    int belt_from;                     /* .. the page it moved from, when (its motion) */
    uint32_t belt_ms;
    int seqp;                          /* SEQ: its subpage (TIME, REC, CLOCK) */
    /* FX's PERFORM (pad 8): the 24 pads below are performance pads; each held, latched (two taps), its last tap, when
     * it was last pressed (the newest of a kind wins), the one pressed last (the screen) */
    int perform;
    uint8_t pf_held[24], pf_latch[24];
    uint32_t pf_tap_ms[24], pf_t[24], pf_ctr;
    int pf_last;
    /* SCENES: the scene edited; ARRANGE: the positions shown (8 a page) */
    int scn, apage;
    /* the scene running (a scene launched, by hand, by a follow action or by the song; -1 none): from tick run_x to
     * run_end, then what follows it (run_next: decided already) */
    int run, run_next;
    int64_t run_x, run_end;
    int song_on, song_pos;             /* the song playing, its position */
    int64_t song_t0;                   /* .. the tick its first position began (its progress) */
    int was_playing;
    uint32_t rng;
    int rnd_pad;                       /* RND PAGE (6) / RND SOUND (7) held (-1 none) .. */
    uint32_t rnd_ms;                   /* .. since then */
    int rnd_done;
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

/* an FX floor's value i: its page in the core and the knob there; 0: none. Every effect's floor has the selected
 * track's send into it on knob 8 (its own values on 1..7: the first page's three, the second page's four); SENDS, all
 * sends, as it is */
static int fx_ent(int i, int *page, int *k)
{
    const fpage_t *f = &FXP[S.fpage];
    if (i < 0 || i >= 8)
        return 0;
    if (S.fpage == 0) {
        *page = i < 4 ? f->a : f->b;
        *k = i & 3;
    } else if (i < 3)
        *page = f->a, *k = i;
    else if (i < 7)
        *page = f->b, *k = i - 3;
    else
        *page = f->a, *k = 3;                           /* (the core's knob 4 on an effect page: its send) */
    return *page >= 0;
}

static int mix_native(void) { return S.mode == MV_MIX && A.ui.mix_page % QD_MIX_NPAGE == 0; }

static int mix_on(void);
static int mix_val(int t, int k);
static int scn_on(void);
static int arr_on(void);
static void scn_val(int i, char *v, int cap, int *frac, int brief);
static const char *const SCN_LAB[8];
static int sq_on(void);
static void sq_val(int i, char *v, int cap, int *frac);
static int seqp_lab_ok(int i);
static const char *seqp_lab(int i);
static void knob_info(int i, kinfo_t *o)
{
    memset(o, 0, sizeof *o);
    o->frac = -1;
    if (S.mute_down && i >= 0 && i < 8) {               /* Mute held: knob i track i's volume, once turned */
        if (i < QD_TRACKS && (S.mk_turned >> i & 1u)) {
            const int v = A.model.t[i].mix.level;
            snprintf(o->name, sizeof o->name, "T%d VOLUME", i + 1);
            snprintf(o->val, sizeof o->val, "%d", v);
            o->frac = v * 1000 / 127;
            o->ok = 1;
        }
        return;
    }
    if (S.over == OV_PROJECTS && (i == 0 || i == 1)) {  /* PROJECTS: the open one's name, colour */
        const int p = A.per.proj, c = A.per.pcol[p];
        snprintf(o->name, sizeof o->name, i ? "COLOUR" : "NAME");
        if (i)
            snprintf(o->val, sizeof o->val, c ? "T%d" : "NONE", c);
        else
            snprintf(o->val, sizeof o->val, "%.12s", A.per.pname[p]);
        o->ok = 1;
        return;
    }
    if (S.mode == MV_SOUND && S.mod_hold >= 0 && S.mod_used && S.mod_name[0]) {   /* ENV / LFO held: the mapping */
        snprintf(o->name, sizeof o->name, "%s", S.mod_name);
        snprintf(o->val, sizeof o->val, "%s", S.mod_val);
        o->ok = 1;
        return;
    }
    if (i >= 0 && i < 8 && sq_on()) {                   /* SEQ: the subpage's settings */
        if (seqp_lab_ok(i)) {
            snprintf(o->name, sizeof o->name, "%s", seqp_lab(i));
            sq_val(i, o->val, sizeof o->val, &o->frac);
            o->ok = 1;
        }
        return;
    }
    if (i >= 0 && i < 8 && scn_on()) {                  /* SCENES: the edited scene's eight */
        snprintf(o->name, sizeof o->name, "S%d %s", S.scn + 1, SCN_LAB[i]);
        scn_val(i, o->val, sizeof o->val, &o->frac, 0);
        o->ok = 1;
        return;
    }
    if (i >= 0 && i < 8 && arr_on()) {                  /* ARRANGE: a position's scene (Shift: its repeats) */
        const int p = S.apage * 8 + i;
        snprintf(o->name, sizeof o->name, "POS %d", p + 1);
        if (S.shift)
            snprintf(o->val, sizeof o->val, "x%d", A.proj.song[p][1]);
        else if (A.proj.song[p][0])
            snprintf(o->val, sizeof o->val, "SCENE %d", A.proj.song[p][0]);
        else
            snprintf(o->val, sizeof o->val, "END");
        o->ok = 1;
        return;
    }
    if (i >= 0 && i < 8 && i < QD_TRACKS && mix_on()) {   /* Session's MIX: knob i track i's value */
        const int v = mix_val(i, S.mix_k);
        snprintf(o->name, sizeof o->name, "T%d %s", i + 1, TS_NAME[S.mix_k]);
        if (S.mix_k == TS_PAN) {
            if (v)
                snprintf(o->val, sizeof o->val, "%c%d", v < 0 ? 'L' : 'R', v < 0 ? -v : v);
            else
                snprintf(o->val, sizeof o->val, "C");
            o->frac = (v + 64) * 1000 / 127;
        } else {
            snprintf(o->val, sizeof o->val, "%d", v);
            o->frac = v * 1000 / 127;
        }
        o->ok = 1;
        return;
    }
    if (i >= KI_TRACK && i < KI_TRACK + TS_N && S.tpad >= 0) {   /* a held track pad's level, pan, a send */
        const qd_mix_track_t *m = &A.model.t[S.tpad].mix;
        const int k = i - KI_TRACK;
        int v;
        snprintf(o->name, sizeof o->name, "T%d %s", S.tpad + 1, TS_NAME[k]);
        if (k == TS_PAN) {
            if (m->pan)
                snprintf(o->val, sizeof o->val, "%c%d", m->pan < 0 ? 'L' : 'R', m->pan < 0 ? -m->pan : m->pan);
            else
                snprintf(o->val, sizeof o->val, "C");
            o->frac = (m->pan + 64) * 1000 / 127;
        } else {
            v = k == TS_VOL ? m->level : k == TS_CHORUS ? m->chorus : k == TS_ECHO ? m->delay : k == TS_ROOM ? m->reverb
              : m->room[k - TS_CRUSH];
            snprintf(o->val, sizeof o->val, "%d", v);
            o->frac = v * 1000 / 127;
        }
        o->ok = 1;
        return;
    }
    if (i == KI_MASTER) {                               /* the main volume, as it is turned */
        snprintf(o->name, sizeof o->name, "VOLUME");
        snprintf(o->val, sizeof o->val, "%d%%", S.pot * 100 / 1023);
        o->frac = S.pot * 1000 / 1023;
        o->ok = 1;
        return;
    }
    if ((i == KI_MICRO || i == KI_VEL) && A.ui.nheld) {     /* a held step's micro timing (the wheel), velocity (Volume) */
        qd_rstep_t r;
        qd_step_resolve(qd_ctl_cpat(&A), A.sel, A.ui.held[0], &r);
        snprintf(o->name, sizeof o->name, "%s", i == KI_MICRO ? "MICRO" : "VELOCITY");
        if (i == KI_MICRO)
            snprintf(o->val, sizeof o->val, "%+d", r.micro);
        else {
            snprintf(o->val, sizeof o->val, "%d", r.vel);
            o->frac = r.vel * 1000 / 127;
        }
        o->ok = 1;
        return;
    }
    if (i == 8 && S.mode == MV_HOME && A.ui.nheld) {    /* the wheel with Shift, a step held: the step's own sound */
        qd_rstep_t r;
        qd_step_resolve(qd_ctl_cpat(&A), A.sel, A.ui.held[0], &r);
        snprintf(o->name, sizeof o->name, "STEP SOUND");
        snprintf(o->val, sizeof o->val, "%s", r.sound ? qd_sound_name(&A.proj, r.sound) : "TRACK'S");
        o->ok = 1;
        return;
    }
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
        if (i >= QD_TRACKS && (QD_TRACKS > 6 || i > 6))
            return;
        memset(&v, 0, sizeof v);
        qd_view_mix(&A, &v);
        if (i < QD_TRACKS) {
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
    } else if (S.mode == MV_HOME) {
        static const char *const TRIG[4] = { "OFF", "NOTE", "LOCK", "TIE" };
        const qd_pattern_t *P = qd_ctl_cpat(&A);
        qd_rstep_t r;
        if (A.ui.nheld)
            qd_step_resolve(P, A.sel, A.ui.held[0], &r);
        else {                                          /* the track's defaults: a step with no overrides */
            qd_step_t on;
            memset(&on, 0, sizeof on);
            on.flags = QD_SF_ON;
            qd_step_resolve_def(&P->trk[A.sel].def, &on, &r);
        }
        snprintf(o->name, sizeof o->name, "%s", S.shift && i == 7 ? "FUNCTION" : HOME_NAME[i]);
        switch (S.shift && i == 7 ? UI_F_FUNC : HOME_MODE[i]) {
        case UI_F_VEL: snprintf(o->val, sizeof o->val, "%d", r.vel); o->frac = r.vel * 1000 / 127; break;
        case UI_F_LEN: snprintf(o->val, sizeof o->val, "%s", qd_len_name(r.len)); break;
        case UI_F_MICRO: snprintf(o->val, sizeof o->val, "%+d", r.micro); break;
        case UI_F_RATCH: snprintf(o->val, sizeof o->val, "X%d", r.ratch < 1 ? 1 : r.ratch); o->frac = (r.ratch < 1 ? 0 : r.ratch - 1) * 1000 / 7; break;
        case UI_F_CHNC: snprintf(o->val, sizeof o->val, "%d%%", r.chance); o->frac = r.chance * 10; break;
        case UI_F_COND: qd_cond_name(r.cond, o->val); break;
        case UI_F_TRIG: snprintf(o->val, sizeof o->val, "%s", TRIG[r.type & 3]); break;
        case UI_F_FUNC: qd_fn_name(r.func, o->val); break;
        case UI_F_SLIDE: {                              /* the step's own lock of the sound's glide, else the sound's */
            static const char *const PORT[3] = { "OFF", "ON", "LEGATO" };
            int16_t pv = 0;
            if (A.ui.nheld && qd_lock_get(P, A.sel, A.ui.held[0], QD_QP_PORT, &pv))
                snprintf(o->val, sizeof o->val, "%s", PORT[pv < 0 ? 0 : pv > 2 ? 2 : pv]);
            else
                snprintf(o->val, sizeof o->val, "SOUND");
            break;
        }
        default: break;
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
    snprintf(S.name, sizeof S.name, "%.19s", name);    /* (S.name holds 19) */
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

/* TRIG / SOUND's page pad p (0..5) the page shown is on (-1 none) */
static int snd_pad_of(int spage)
{
    int p, j;
    for (p = 0; p < 6; p++)
        for (j = 0; j < 4; j++)
            if (SND_PAD[p][j] == spage)
                return p;
    return -1;
}

/* a page pad: the page it showed last; tapped again while its page is up, its next page (round) */
static void snd_pad(int p)
{
    int j, n = 0;
    while (n < 4 && SND_PAD[p][n] >= 0)
        n++;
    if (snd_pad_of(S.spage) == p) {
        for (j = 0; j < n && SND_PAD[p][j] != S.spage; j++)
            ;
        j = (j + 1) % n;
    } else
        for (j = 0; j < n && SND_PAD[p][j] != S.grp_last[p]; j++)
            ;
    if (j >= n)
        j = 0;
    S.grp_last[p] = SND_PAD[p][j];
    snd_go(SND_PAD[p][j]);
}

/* ------------------------------------------------------------------ page mode --- */
static int page_mode(void)
{
    return (S.page_latch || (S.loop_btn && !S.loop_down)) && (S.mode == MV_HOME || S.mode == MV_SOUND || S.mode == MV_FX) &&
           !(S.session && (S.sn_mode == SN_CLIPS || S.sn_mode >= SN_SCENES) && S.mode == MV_HOME);
}
static unsigned trk_loop(void) { unsigned m = qd_ctl_cpat(&A)->trk[A.sel].loop; return m ? m : 1u; }
static int trk_npage(void)            /* the clip's length in pages: up to its last page in the loop */
{
    const unsigned m = trk_loop();
    int p, n = 1;
    for (p = 0; p < QD_NPAGE; p++)
        if (m >> p & 1u)
            n = p + 1;
    return n;
}
static void set_loop(unsigned m)
{
    qd_undo_hint(&A, QD_UK_EDIT);
    qd_page_set_loop(qd_ctl_pat(&A), A.sel, (int)(m & QD_PMASK_ALL));
}
static void page_name(const char *what)
{
    char v[20];
    snprintf(v, sizeof v, "%s %d", what, A.ui.page[A.sel] + 1);
    show_name(v);
}

/* a page step: tap shows the page; held (HOLD_MS) its place in the loop toggles (muted / heard); two taps: it loops
 * alone (again: the loop back); held and another tapped: the loop runs from one to the other. Copy held: the page
 * copied (then: pasted where tapped); Delete held: the page cleared */
static void page_step(int i, int down)
{
    qd_pattern_t *P = qd_ctl_pat(&A);
    S.loop_used = 1;
    if (down) {
        if (S.copy_down) {
            qd_undo_hint(&A, QD_UK_PASTE);
            if (S.pg_copied < 0) {
                qd_clip_copy_page(&A.ui.clip, P, A.sel, i);
                S.pg_copied = i;
                show_name("COPIED");
            } else {
                qd_clip_paste_page(&A.ui.clip, P, A.seq.cur, A.sel, i);
                show_name("PASTED");
            }
            return;
        }
        if (S.delete_down) {
            qd_undo_hint(&A, QD_UK_CLEAR);
            qd_clip_clear_page(&A.ui.clip, P, A.seq.cur, A.sel, i);
            show_name("CLEARED");
            return;
        }
        if (S.pg_held >= 0 && S.pg_held != i) {         /* a range: from one held page to this one */
            const int lo = S.pg_held < i ? S.pg_held : i, hi = S.pg_held < i ? i : S.pg_held;
            set_loop(((1u << (hi + 1)) - 1u) & ~((1u << lo) - 1u));
            S.pg_used = 1;
            show_name("LOOP");
            return;
        }
        S.pg_held = i, S.pg_ms = S.now ? S.now : 1, S.pg_used = 0;
        return;
    }
    if (S.pg_held != i)
        return;
    S.pg_held = -1;
    if (S.pg_used)
        return;
    if (S.now - S.pg_ms >= HOLD_MS) {                   /* held: out of the loop / back in */
        unsigned m = trk_loop() ^ (1u << i);
        set_loop(m ? m : trk_loop());
        show_name((trk_loop() >> i & 1u) ? "PAGE ON" : "PAGE OFF");
        return;
    }
    if (S.pg_tap_ms[i] && S.now - S.pg_tap_ms[i] < DOUBLE_MS) {   /* two taps: this page alone (again: back) */
        if (S.pg_solo_restore && trk_loop() == (1u << i)) {
            set_loop(S.pg_solo_restore);
            S.pg_solo_restore = 0;
        } else {
            S.pg_solo_restore = trk_loop();
            set_loop(1u << i);
        }
        S.pg_tap_ms[i] = 0;
        return;
    }
    S.pg_tap_ms[i] = S.now ? S.now : 1;
    qd_ctl_set_page(&A, i);                             /* a tap: the page shown (its start and end on rows 3-4) */
    page_name("PAGE");
}

/* rows 3-4 in page mode: the shown page's 16 steps (row 3 steps 1-8, row 4 9-16): a tap its start, two taps its end,
 * one held and another tapped both */
static void page_pad(int col, int down, int vel);
static void range_pad(int k, int down)
{
    const int pg = A.ui.page[A.sel];
    const qd_tset_t *ts = &qd_ctl_cpat(&A)->trk[A.sel];
    int st = ts->pstart[pg], en = ts->pend[pg];
    S.loop_used = 1;
    if (!down) {
        if (S.pd_held == k)
            S.pd_held = -1;
        return;
    }
    if (S.pd_held >= 0 && S.pd_held != k) {
        st = S.pd_held < k ? S.pd_held : k, en = S.pd_held < k ? k : S.pd_held;
        S.pd_held = -1;
    } else if (S.pd_tap_ms[k] && S.now - S.pd_tap_ms[k] < DOUBLE_MS) {
        en = k, st = S.pd_prev_start > k ? k : S.pd_prev_start;
        S.pd_tap_ms[k] = 0;
    } else {
        S.pd_prev_start = st;
        st = k, en = en < k ? k : en;
        S.pd_held = k;
        S.pd_tap_ms[k] = S.now ? S.now : 1;
    }
    qd_undo_hint(&A, QD_UK_EDIT);
    qd_page_set_range(qd_ctl_pat(&A), A.sel, pg, st, en);
}

/* ------------------------------------------------------------------ Sequence's modes --- */
static void tap(int b);
/* the core put on mode m: HOME for TRACKS, its sound editor, its FX, its TOOLS (Loop, tapped) */
static void seq_go(int m)
{
    static const int MV[SQ_N] = { MV_HOME, MV_SOUND, MV_FX, -1 };
    if (m == SQ_TOOLS ? S.mode == MV_CORE && S.scr == QD_SCR_TOOLS : S.mode == MV[m])
        return;
    if (S.mode != MV_HOME && S.mode != MV_IDLE)
        tap(B_HOME);                                    /* (HOME closes any page; at home it would cycle the views) */
    if (m == SQ_SOUND)
        tap(B_EDIT);
    else if (m == SQ_FX)
        tap(B_FX);
    else if (m == SQ_TOOLS)
        tap(B_SEQ);
}

/* the top row shows its mode's keys (else the fourth row of notes): Mute held always shows the tracks */
static int top_row_on(void)
{
    if (S.mute_down || S.loop_down)
        return 1;
    if (S.mode != MV_HOME && S.mode != MV_SOUND && S.mode != MV_FX && !(S.mode == MV_CORE && S.scr == QD_SCR_TOOLS))
        return 1;                                       /* (the core's other pages: its black keys, always) */
    return !S.top_hidden;
}

static void track_button(int n, int down)
{
    if (down) {
        S.tb_down = n;
        S.tb_ms = S.now ? S.now : 1;
        S.tb_prev_mode = S.seq_mode;
        S.tb_prev_hidden = S.top_hidden;
        S.top_hidden = 0;                               /* its top row, at once (held: while held) */
        if (n != S.seq_mode) {
            S.seq_mode = n;
            seq_go(n);
            show_name(SQ_NAME[n]);
        }
        return;
    }
    if (S.tb_down != n)
        return;
    S.tb_down = -1;
    if (S.now - S.tb_ms >= HOLD_MS) {                   /* held: back to what was up */
        S.top_hidden = S.tb_prev_hidden;
        if (S.tb_prev_mode != S.seq_mode) {
            S.seq_mode = S.tb_prev_mode;
            seq_go(S.seq_mode);
        }
    } else if (S.tb_prev_mode == n)                     /* the active mode's button tapped: its top row hides / shows */
        S.top_hidden = !S.tb_prev_hidden;
    else
        S.top_hidden = 0;
}

/* ------------------------------------------------------------------ Session's clips --- */
static qd_pattern_t g_cut;             /* Delete + a clip: the part cut (its track 0), until pasted */

static int clip_has(int slot, int t)   /* track t's part of pattern slot has a trig */
{
    const qd_pattern_t *P = qd_proj_cpat(&A.proj, slot);
    int i;
    for (i = 0; i < QD_NSTEP; i++)
        if (qd_step_is_on(qd_step(P, t, i)))
            return 1;
    return 0;
}

/* the clip edited on track t: the slot chosen (empty), else the one queued, else the one playing */
static int edit_clip(int t)
{
    const int q = qd_seq_clip_queued(&A.seq, t);
    return S.tsel[t] >= 0 ? S.tsel[t] : q >= 0 ? q : qd_seq_clip(&A.seq, t);
}

static int clips_on(void) { return S.session && S.sn_mode == SN_CLIPS && S.mode == MV_HOME && !S.over; }

static void clip_say(const char *what, int n)
{
    char v[20];
    snprintf(v, sizeof v, "%s %d", what, n);
    show_name(v);
}

static void clip_launch(int t, int slot)
{
    qd_seq_launch(&A.seq, t, slot, LAUNCH_Q);
    S.tsel[t] = -1;
}

static void clip_stop(int t) { qd_seq_stop_track(&A.seq, t, LAUNCH_Q); }

/* a clip operation on pattern k: an undo level of its own (a level holds the pattern edited when its gesture opened:
 * k is made that one for it, the one edited put back after) */
static void clip_undo_begin(int k, int kind)
{
    S.undo_cur = A.seq.cur;
    qd_undo_close(&A);
    A.seq.cur = k;
    qd_undo_input(&A, 0);
    qd_undo_hint(&A, kind);
}
static void clip_undo_end(void)
{
    qd_undo_close(&A);
    A.seq.cur = S.undo_cur;
}

static int next_empty(int t, int from)              /* track t's first slot with no clip from slot from on (-1 none) */
{
    int k;
    for (k = from < 0 ? 0 : from; k < NSLOT; k++)
        if (!clip_has(k, t))
            return k;
    return -1;
}

static void launch_say(void)
{
    char v[20];
    snprintf(v, sizeof v, "LAUNCH %s", A.proj.launch == QD_LAUNCH_END ? "CLIP END" : qd_launch_name(A.proj.launch));
    show_name(v);
}

/* Copy tapped alone in CLIPS: the selected track's clip into its next empty slot, launched there */
static void clip_duplicate(void)
{
    const int t = A.sel, from = edit_clip(t), to = next_empty(t, from + 1);
    if (t >= QD_TRACKS || !clip_has(from, t))
        return;
    if (to < 0) {
        show_name("NO SLOT");
        return;
    }
    clip_undo_begin(to, QD_UK_PASTE);
    qd_track_copy(qd_proj_pat(&A.proj, to), t, qd_proj_cpat(&A.proj, from), t);
    clip_undo_end();
    clip_launch(t, to);
    if (to >= S.srow + 4)
        S.srow = to - 3;
    clip_say("DUPLICATE", to + 1);
}

/* Shift + step 14: the selected track's next empty slot chosen (the clip playing goes on; a step put there launches
 * the new one) */
static void new_clip(void)
{
    const int t = A.sel, k = next_empty(t, 0);
    if (t >= QD_TRACKS)
        return;
    if (k < 0) {
        show_name("NO SLOT");
        return;
    }
    S.tsel[t] = (int8_t)k;
    if (k >= S.srow + 4 || k < S.srow)
        S.srow = k > NSLOT - 4 ? NSLOT - 4 : k;
    clip_say("NEW CLIP", k + 1);
}

/* a clip pad (row 0 the bottom): tap launches the clip (playing: at the next bar), or on an empty slot chooses it and
 * stops the track; two taps stop the track; Shift: the row's scene (an empty slot stops its track, or with EMPTY SLOT
 * KEEP leaves it playing); Copy held: the
 * clip copied, then pasted where tapped; Delete held: cut, then pasted where tapped (not pasted: deleted) */
static void note_pad(int idx, int vel);
static void scene_launch(int k);
static void clip_pad(int idx, int down)
{
    const int t = idx % 8, slot = S.srow + 3 - idx / 8;
    if (!down) {
        if (S.pad_note[idx] >= 0)
            note_pad(idx, 0);                           /* (a key held from before) */
        return;
    }
    if (t >= QD_TRACKS || slot < 0 || slot >= NSLOT)
        return;
    if (S.copy_down || S.delete_down) {
        S.copy_used = 1;
        if (S.cl_take < 0) {
            S.cl_take = t * NSLOT + slot;
            S.cl_cut = S.delete_down;
            if (S.cl_cut) {
                qd_pat_init(&g_cut);
                qd_track_copy(&g_cut, 0, qd_proj_cpat(&A.proj, slot), t);
                clip_undo_begin(slot, QD_UK_CLEAR);
                qd_track_clear(qd_proj_pat(&A.proj, slot), t);
                clip_undo_end();
            }
            show_name(S.cl_cut ? "CUT" : "COPIED");
        } else {
            const int st = S.cl_take / NSLOT, ss = S.cl_take % NSLOT;
            clip_undo_begin(slot, QD_UK_PASTE);
            if (S.cl_cut)
                qd_track_copy(qd_proj_pat(&A.proj, slot), t, &g_cut, 0);
            else if (st != t || ss != slot)
                qd_track_copy(qd_proj_pat(&A.proj, slot), t, qd_proj_cpat(&A.proj, ss), st);
            clip_undo_end();
            if (S.cl_cut)                               /* (a cut pastes once; a copy as often as tapped) */
                S.cl_take = -1;
            show_name("PASTED");
        }
        return;
    }
    if (S.shift) {                                      /* the row: a scene (an empty slot: EMPTY SLOT's) */
        scene_launch(slot);
        return;
    }
    qd_ctl_select_track(&A, t);
    if (S.cl_tap_ms[idx] && S.now - S.cl_tap_ms[idx] < DOUBLE_MS) {   /* two taps: the track stopped */
        S.cl_tap_ms[idx] = 0;
        clip_stop(t);
        clip_say("STOP", t + 1);
        return;
    }
    S.cl_tap_ms[idx] = S.now ? S.now : 1;
    if (clip_has(slot, t)) {
        clip_launch(t, slot);
        clip_say("CLIP", slot + 1);
    } else {
        S.tsel[t] = (int8_t)slot;
        clip_stop(t);
        clip_say("EMPTY", slot + 1);
    }
}

/* CLIPS' step buttons, two under each track: the first stops it, the second mutes it (Shift: solo) */
static void clip_step(int i)
{
    const int t = i / 2;
    if (t >= QD_TRACKS)
        return;
    if (!(i & 1)) {
        clip_stop(t);
        clip_say("STOP", t + 1);
    } else if (S.shift)
        A.seq.solo ^= (uint8_t)(1u << t);
    else
        A.seq.mute ^= (uint8_t)(1u << t);
}

/* the rows of slots shown: by d (Plus / Minus 1, Left / Right 4) */
static void clip_rows(int d)
{
    const int r = S.srow + d < 0 ? 0 : S.srow + d > NSLOT - 4 ? NSLOT - 4 : S.srow + d;
    if (r != S.srow) {
        char v[16];
        S.srow = r;
        snprintf(v, sizeof v, "SLOTS %d-%d", r + 1, r + 4);
        show_name(v);
    }
}

static void tsound_sync(int k, int t, const qd_pattern_t *E)   /* pattern k's sound of track t made E's */
{
    qd_pattern_t *P;
    if (k < 0 || k >= QD_NPAT || E == qd_proj_cpat(&A.proj, k))
        return;
    P = qd_proj_pat(&A.proj, k);
    if (memcmp(&P->snd[t], &E->snd[t], sizeof P->snd[t]))
        P->snd[t] = E->snd[t];
    if (P->trk[t].mfx != E->trk[t].mfx)                 /* (the MIDI FX group too: the track's, not a clip's) */
        P->trk[t].mfx = E->trk[t].mfx;
}

/* MIDI FX opened on a track with no group: the first group no track uses is its own (else the first) */
static void mfx_auto_group(void)
{
    qd_pattern_t *P = qd_ctl_pat(&A);
    unsigned used = 0;
    int t, g = 1;
    if (A.sel >= QD_TRACKS || qd_mfx_tgroup(&A))
        return;
    for (t = 0; t < QD_TRACKS; t++)
        used |= 1u << (P->trk[t].mfx & 0x7f);
    while (g <= QD_MFX_NGROUP && (used >> g & 1u))
        g++;
    if (g > QD_MFX_NGROUP)
        g = 1;
    P->trk[A.sel].mfx = (uint8_t)g;
    {
        char v[32];
        snprintf(v, sizeof v, "T%d: GROUP %d", A.sel + 1, g);
        show_name(v);
    }
}

/* each look: the clips on in the core's sequencer; a chosen empty slot given a trig plays; the pattern edited is the
 * selected track's clip; the sound stays with the track (the sound editor writes into the clip edited: from there into
 * the clip each track plays and the one queued, and into a clip as it comes to be edited) */
static void clip_follow(void)
{
    int t, k;
    if (!A.seq.clips || A.per.proj != S.proj_seen) {   /* (at start, after anything that set the sequencer up again,
                                                         * and in another project: no empty slot chosen) */
        if (!A.seq.clips)
            qd_seq_clips(&A.seq, 1);
        memset(S.tsel, -1, sizeof S.tsel);
        S.proj_seen = A.per.proj;
    }
    for (t = 0; t < QD_TRACKS; t++)
        if (S.tsel[t] >= 0 && clip_has(S.tsel[t], t))
            clip_launch(t, S.tsel[t]);
    k = edit_clip(A.sel);
    if (k != A.seq.cur && k >= 0 && k < QD_NPAT) {
        const qd_pattern_t *O = qd_proj_cpat(&A.proj, A.seq.cur);
        for (t = 0; t < QD_TRACKS; t++)
            tsound_sync(k, t, O);
        A.seq.cur = k;
    }
    {
        const qd_pattern_t *E = qd_proj_cpat(&A.proj, A.seq.cur);
        for (t = 0; t < QD_TRACKS; t++) {
            tsound_sync(qd_seq_clip(&A.seq, t), t, E);
            tsound_sync(qd_seq_clip_queued(&A.seq, t), t, E);
        }
    }
}

/* ------------------------------------------------------------------ PERFORM --- */
/* FX's pad 8: the rows below are performance pads (design/WORKFLOW.md "FX mode and PERFORM"). Row 3 (from the top
 * row's: pads 17-24 here) throws CHORUS .. TV fully on (Sequence: the selected track's sends; Session: every track's);
 * row 2 beat repeat 1/4 .. 1/32T; row 1 the filter sweeps LP 1, LP 2, HP 1, HP 2 and the tape STOP, HALF, REV,
 * SCRATCH (on the whole mix: perform.c). Held: on while held; two taps: latched (a tap: off). Of a kind, the newest
 * pressed wins */
static const char *const PF_NAME[24] = {
    "LP 1", "LP 2", "HP 1", "HP 2", "TAPE STOP", "HALF SPEED", "REVERSE", "SCRATCH",
    "REPEAT 1/4", "REPEAT 1/4T", "REPEAT 1/8", "REPEAT 1/8T", "REPEAT 1/16", "REPEAT 1/16T", "REPEAT 1/32", "REPEAT 1/32T",
    "CHORUS", "ECHO", "ROOM", "CRUSH", "ROBOT", "WARP", "GLITCH", "TV" };

static int pf_on(int i) { return S.pf_held[i] || S.pf_latch[i]; }

static int pf_newest(int lo, int hi)                    /* the newest pressed of pads lo .. hi - 1 that is on (-1) */
{
    int i, b = -1;
    for (i = lo; i < hi; i++)
        if (pf_on(i) && (b < 0 || S.pf_t[i] > S.pf_t[b]))
            b = i;
    return b;
}

/* each look: the throws into the core (app.h thr), the rest to perform.c */
static void pf_sync(void)
{
    int t, k;
    uint8_t thr = 0;
    if (S.mode != MV_FX)                                /* (held pads let go elsewhere: not held any more) */
        memset(S.pf_held, 0, sizeof S.pf_held);
    for (k = 0; k < 8; k++)
        if (pf_on(16 + k))
            thr |= (uint8_t)(1u << k);
    for (t = 0; t < QD_NTRACK; t++)
        A.thr[t] = (uint8_t)(t < QD_TRACKS && (S.session || t == A.sel) ? thr : 0);
    {
        const int rp = pf_newest(8, 16), fl = pf_newest(0, 4), tp = pf_newest(4, 8);
        pf_set(rp < 0 ? -1 : rp - 8, fl, tp < 0 ? -1 : tp - 4, A.bpm);
    }
}

static void pf_pad(int idx, int down)
{
    if (idx < 0 || idx >= 24)
        return;
    if (!down) {
        S.pf_held[idx] = 0;
        return;
    }
    if (S.pf_latch[idx])                                /* latched: a tap lets it go (on while held) */
        S.pf_latch[idx] = 0;
    else if (S.pf_tap_ms[idx] && S.now - S.pf_tap_ms[idx] < DOUBLE_MS) {
        S.pf_latch[idx] = 1;                            /* two taps: latched */
        show_name("LATCHED");
    }
    else
        S.name_until = S.now;                           /* (a name showing gives way to the pad's picture) */
    S.pf_tap_ms[idx] = S.now ? S.now : 1;
    S.pf_held[idx] = 1;
    S.pf_t[idx] = ++S.pf_ctr;
    S.pf_last = idx;
}

static void pf_off(void)                                /* PERFORM left: everything let go */
{
    memset(S.pf_held, 0, sizeof S.pf_held);
    memset(S.pf_latch, 0, sizeof S.pf_latch);
    S.perform = 0;
}

/* ------------------------------------------------------------------ SCENES and ARRANGE --- */
/* A scene is slot k on every track (pattern k). Launched (Shift + a clip row, a step in SCENES or ARRANGE, the wheel
 * pressed in SCENES) it starts on every track at one point, its launch quantize's (its own, else the project's), at its
 * tempo (its own, else the project's), and runs REPEATS lengths of AFTER bars (AUTO: its longest clip); then its FOLLOW
 * action, CHANCE % of the time (else another length first). The song (ARRANGE) is positions of a scene and its repeats,
 * to the first END; while it plays, follow actions wait. What follows is decided a bar ahead and launched exactly at the
 * point (qd_seq_launch_at) */
#define BAR_TICKS (16 * (int64_t)QD_TPS)
static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
#define BELT_MS 180u                                    /* the belt's move from a page to the next (the board's frame) */
static const char *const SCN_FOLLOW[QD_NFOLLOW] = { "NONE", "NEXT", "PREVIOUS", "FIRST", "ANY", "OTHER" };
static const char *const SCN_FOLLOW3[QD_NFOLLOW] = { "OFF", "NXT", "PRV", "1ST", "ANY", "OTH" };   /* (the all-eight) */
#define NSCN_NAME 16                                    /* the names a scene can take (0: none, SCENE n) */
static const char *const SCN_NAME[NSCN_NAME] = { "", "INTRO", "VERSE", "PRE", "CHORUS", "BRIDGE", "BREAK", "BUILD", "DROP",
                                                 "SOLO", "OUTRO", "FILL", "A", "B", "C", "D" };
static const char *const SCN_NAME3[NSCN_NAME] = { "-", "INT", "VRS", "PRE", "CHO", "BRI", "BRK", "BLD", "DRP", "SOL", "OUT",
                                                  "FIL", "A", "B", "C", "D" };
#define NSCN_COL 9                                      /* its colour: none, or one of the tracks' eight */
/* a scene's launch quantize as its knob turns: the project's, then LAUNCH_ORDER's */
static const uint8_t SCN_LAUNCH[8] = { QD_SCENE_LAUNCH_PROJ, QD_LAUNCH_NOW, QD_LAUNCH_BEAT, QD_LAUNCH_BAR, QD_LAUNCH_BAR2,
                                       QD_LAUNCH_BAR4, QD_LAUNCH_BAR8, QD_LAUNCH_END };
static const char *const SCN_LAUNCH_NAME[8] = { "PROJECT", "NOW", "BEAT", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "CLIP END" };
static const char *const SCN_LAUNCH3[8] = { "PRJ", "NOW", "BT", "1B", "2B", "4B", "8B", "END" };

static int scn_on(void) { return S.session && S.sn_mode == SN_SCENES && S.mode == MV_HOME && !S.over; }
static int arr_on(void) { return S.session && S.sn_mode == SN_ARRANGE && S.mode == MV_HOME && !S.over; }
static int sq_on(void) { return S.session && S.sn_mode == SN_SEQ && S.mode == MV_HOME && !S.over; }

/* SEQ: the sequencer's settings on three subpages (the wheel): TIME (tempo, swing, launch quantize, the metronome and
 * its click), REC (the record mode, quantize, take, automation), CLOCK (clock out, CC out; a clock in is followed
 * whenever one comes). No count-in: the core has none yet */
enum { SQP_TIME, SQP_REC, SQP_CLOCK, SQP_N };
static const char *const SQP_NAME[SQP_N] = { "TIME", "REC", "CLOCK" };
static const char *const SQP_LAB[SQP_N][8] = {
    { "TEMPO", "SWING", "LAUNCH", "METRO", "CLICK", "", "", "" },
    { "MODE", "QUANT", "TAKE", "AUTO", "", "", "", "" },
    { "CLK OUT", "CC OUT", "", "", "", "", "", "" },
};
static void sq_knob(int i, int d)
{
    int k;
    if (S.seqp == SQP_TIME) {
        switch (i) {
        case 0: qd_ctl_set_tempo(&A, clamp(qd_seq_bpm(&A.seq) + d, QD_BPM_MIN, QD_BPM_MAX)); break;
        case 1: {                                       /* (the project's: every pattern's, as one) */
            const int sw = clamp(qd_proj_cpat(&A.proj, A.seq.cur)->swing + d, 50, 80);
            for (k = 0; k < QD_NSCENE; k++)
                qd_proj_pat(&A.proj, k)->swing = (uint8_t)sw;
            break;
        }
        case 2:
            for (k = 0; k < (int)sizeof LAUNCH_ORDER && LAUNCH_ORDER[k] != A.proj.launch; k++)
                ;
            A.proj.launch = LAUNCH_ORDER[clamp((k < (int)sizeof LAUNCH_ORDER ? k : 2) + d, 0, (int)sizeof LAUNCH_ORDER - 1)];
            break;
        case 3: A.metro = d > 0; break;
        case 4: A.metro_lvl = clamp(A.metro_lvl + d * 5, 0, 100); break;
        }
    } else if (S.seqp == SQP_REC) {
        switch (i) {
        case 0: A.proj.rec_mode = d > 0; break;
        case 1: A.proj.rec_quant = (uint8_t)clamp(A.proj.rec_quant + d * 5, 0, 100); break;
        case 2: A.proj.rec_dub = d > 0 ? QD_TAKE_REPLACE : QD_TAKE_OVERDUB; break;
        case 3: A.proj.rec_auto = d > 0; break;
        }
    } else {
        if (i == 0)
            A.ui.clk_out = d > 0;
        else if (i == 1)
            A.ui.cc_out = d > 0;
    }
}
static int seqp_lab_ok(int i) { return SQP_LAB[S.seqp][i][0] != 0; }
static const char *seqp_lab(int i) { return SQP_LAB[S.seqp][i]; }
static void sq_val(int i, char *v, int cap, int *frac)
{
    int k;
    *frac = -1;
    v[0] = 0;
    if (S.seqp == SQP_TIME) {
        switch (i) {
        case 0: {
            const int b = qd_seq_bpm(&A.seq);
            snprintf(v, (size_t)cap, "%d", b), *frac = (b - QD_BPM_MIN) * 1000 / (QD_BPM_MAX - QD_BPM_MIN);
            break;
        }
        case 1: {
            const int sw = qd_proj_cpat(&A.proj, A.seq.cur)->swing;
            snprintf(v, (size_t)cap, "%d", sw), *frac = (sw - 50) * 1000 / 30;
            break;
        }
        case 2:
            for (k = 0; k < 8 && SCN_LAUNCH[k] != A.proj.launch; k++)
                ;
            snprintf(v, (size_t)cap, "%s", SCN_LAUNCH3[k < 8 ? k : 3]);
            break;
        case 3: snprintf(v, (size_t)cap, A.metro ? "ON" : "OFF"); break;
        case 4: snprintf(v, (size_t)cap, "%d", A.metro_lvl), *frac = A.metro_lvl * 10; break;
        }
    } else if (S.seqp == SQP_REC) {
        switch (i) {
        case 0: snprintf(v, (size_t)cap, A.proj.rec_mode ? "STP" : "LIV"); break;
        case 1: snprintf(v, (size_t)cap, "%d", A.proj.rec_quant), *frac = A.proj.rec_quant * 10; break;
        case 2: snprintf(v, (size_t)cap, A.proj.rec_dub == QD_TAKE_REPLACE ? "REP" : "DUB"); break;
        case 3: snprintf(v, (size_t)cap, A.proj.rec_auto ? "ON" : "OFF"); break;
        }
    } else if (i < 2)
        snprintf(v, (size_t)cap, (i ? A.ui.cc_out : A.ui.clk_out) ? "ON" : "OFF");
}

static int scene_used(int k)                            /* a clip in slot k on any track */
{
    int t;
    for (t = 0; t < QD_TRACKS; t++)
        if (clip_has(k, t))
            return 1;
    return 0;
}

static uint8_t scene_col(int k, int dim)                /* its colour (none: grey) */
{
    const int c = k >= 0 && k < QD_NSCENE ? A.proj.scene[k].colour : 0;
    return c >= 1 && c <= 8 ? (dim ? TRK_DIM[c - 1] : TRK_COL[c - 1]) : dim ? C_GREY : C_LGREY;
}

static int64_t scene_len(int k)                         /* one length of scene k, ticks: AFTER bars, AUTO its longest
                                                         * clip in whole bars (at least one) */
{
    const qd_scene_t *c = &A.proj.scene[k];
    int64_t L;
    if (c->after)
        return c->after * BAR_TICKS;
    L = qd_pat_master_ticks(qd_proj_cpat(&A.proj, k));
    return L <= 0 ? BAR_TICKS : qd_div64(L + BAR_TICKS - 1, (int32_t)BAR_TICKS) * BAR_TICKS;
}

static int64_t scene_x(int k)                           /* where scene k would start now (stopped: at PLAY) */
{
    const int lq = A.proj.scene[k].launch == QD_SCENE_LAUNCH_PROJ ? A.proj.launch : A.proj.scene[k].launch;
    int t;
    if (!A.seq.playing)
        return 0;
    if (lq == QD_LAUNCH_END)                            /* (the clip end: the first playing track's) */
        for (t = 0; t < QD_TRACKS; t++)
            if (qd_seq_clip_on(&A.seq, t))
                return qd_seq_launch_tick(&A.seq, t, lq);
    return qd_seq_launch_tick(&A.seq, 0, lq == QD_LAUNCH_END ? QD_LAUNCH_BAR : lq);
}

/* scene k on every track from tick x, run for reps lengths */
static void scene_start(int k, int64_t x, int reps)
{
    int bpm, t;
    if (A.proj.scene[k].bpm && !A.proj.bpm)            /* (the project's tempo, kept before a scene first changes it) */
        A.proj.bpm = (uint16_t)qd_seq_bpm(&A.seq);
    bpm = A.proj.scene[k].bpm ? A.proj.scene[k].bpm : A.proj.bpm;
    for (t = 0; t < QD_TRACKS; t++)
        if (clip_has(k, t)) {
            qd_seq_launch_at(&A.seq, t, k, x);
            S.tsel[t] = -1;
        } else if (!A.proj.empty_keep)
            qd_seq_stop_track_at(&A.seq, t, x);
    if (bpm)
        qd_seq_tempo_at(&A.seq, bpm * 1000, x);
    S.run = k;
    S.run_x = x;
    S.run_end = x + scene_len(k) * (reps < 1 ? 1 : reps);
    S.run_next = 0;
}

static void scene_launch(int k)                         /* by hand: the song gives way */
{
    if (k < 0 || k >= QD_NSCENE)
        return;
    S.song_on = 0;
    scene_start(k, scene_x(k), A.proj.scene[k].repeats);
    clip_say("SCENE", k + 1);
}

static uint32_t rnd(uint32_t n)                         /* (deterministic: the tests' runs repeat) */
{
    S.rng = S.rng * 1664525u + 1013904223u;
    return n ? (S.rng >> 8) % n : 0;
}

static int follow_target(int k, int f)                  /* the scene a follow action goes to (-1: none to go to) */
{
    int used[QD_NSCENE], n = 0, i;
    for (i = 0; i < QD_NSCENE; i++)
        if (scene_used(i))
            used[n++] = i;
    if (!n)
        return -1;
    switch (f) {
    case QD_FOLLOW_NEXT:
        for (i = 1; i <= QD_NSCENE; i++)
            if (scene_used((k + i) % QD_NSCENE))
                return (k + i) % QD_NSCENE;
        return k;
    case QD_FOLLOW_PREV:
        for (i = 1; i <= QD_NSCENE; i++)
            if (scene_used((k - i + QD_NSCENE) % QD_NSCENE))
                return (k - i + QD_NSCENE) % QD_NSCENE;
        return k;
    case QD_FOLLOW_FIRST: return used[0];
    case QD_FOLLOW_ANY: return used[rnd((uint32_t)n)];
    case QD_FOLLOW_OTHER: {
        int o[QD_NSCENE], m = 0;
        for (i = 0; i < n; i++)
            if (used[i] != k)
                o[m++] = used[i];
        return m ? o[rnd((uint32_t)m)] : k;
    }
    default: return -1;
    }
}

static int song_end(void)                               /* the first END: the song's length in positions */
{
    int p;
    for (p = 0; p < QD_NSONG; p++)
        if (!A.proj.song[p][0])
            return p;
    return QD_NSONG;
}

static int64_t song_ticks(int from, int to)             /* positions from .. to - 1, ticks */
{
    int64_t n = 0;
    int p;
    for (p = from; p < to; p++)
        n += scene_len(A.proj.song[p][0] - 1) * A.proj.song[p][1];
    return n;
}

static void song_play(int from)                         /* the song from position from (stopped: PLAY too) */
{
    int k;
    int64_t x;
    if (from < 0 || from >= song_end()) {
        show_name("NO SONG");
        return;
    }
    k = A.proj.song[from][0] - 1;
    x = scene_x(k);
    S.song_on = 1;
    S.song_pos = from;
    S.song_t0 = x - song_ticks(0, from);
    scene_start(k, x, A.proj.song[from][1]);
    if (!A.seq.playing)
        tap(B_PLAY);
    show_name(from ? "SONG FROM" : "SONG");
}

static void song_stop(void)                             /* the song stops; the clips play on */
{
    S.song_on = 0;
    S.run = -1;
    show_name("SONG STOP");
}

static int song_shown_pos(void)                         /* the position playing now (its successor may be queued) */
{
    return !S.song_on && S.run < 0 ? -1 : A.seq.tick >= S.run_x ? S.song_pos : S.song_pos - 1;
}

/* each look: what follows the scene running, a bar before it ends */
static void scene_run(void)
{
    if (!A.seq.playing) {
        if (S.was_playing) {                            /* (stopped: the song and the follow actions end) */
            S.song_on = 0;
            S.run = -1;
        }
        S.was_playing = 0;
        return;
    }
    S.was_playing = 1;
    if (S.run < 0 || S.run_next || A.seq.tick < S.run_end - BAR_TICKS)
        return;
    if (S.song_on) {
        const int p = S.song_pos + 1;
        if (p >= song_end()) {                          /* the song's end: the clips play on */
            S.song_on = 0;
            S.run_next = 1;
            return;
        }
        S.song_pos = p;
        scene_start(A.proj.song[p][0] - 1, S.run_end, A.proj.song[p][1]);
        return;
    }
    {
        const qd_scene_t *c = &A.proj.scene[S.run];
        int nx;
        if (c->follow == QD_FOLLOW_NONE) {
            S.run_next = 1;
            return;
        }
        if (rnd(100) >= c->chance) {                    /* not this time: another length first */
            S.run_end += scene_len(S.run);
            return;
        }
        nx = follow_target(S.run, c->follow);
        if (nx < 0) {
            S.run_next = 1;
            return;
        }
        scene_start(nx, S.run_end, A.proj.scene[nx].repeats);
    }
}


/* SCENES' knob i: the edited scene's settings */
static void scn_knob(int i, int d)
{
    qd_scene_t *c = &A.proj.scene[S.scn];
    int k;
    switch (i) {
    case 0:                                             /* TEMPO: OFF below 30; up from OFF: the tempo now */
        if (!c->bpm)
            c->bpm = d > 0 ? (uint16_t)qd_seq_bpm(&A.seq) : 0;
        else
            c->bpm = c->bpm + d < QD_BPM_MIN ? 0 : (uint16_t)clamp(c->bpm + d, QD_BPM_MIN, QD_BPM_MAX);
        break;
    case 1: c->follow = (uint8_t)clamp(c->follow + d, 0, QD_NFOLLOW - 1); break;
    case 2: c->after = (uint8_t)clamp(c->after + d, 0, 64); break;
    case 3: c->chance = (uint8_t)clamp(c->chance + d, 0, 100); break;
    case 4: c->repeats = (uint8_t)clamp(c->repeats + d, 1, 16); break;
    case 5:
        for (k = 0; k < 8 && SCN_LAUNCH[k] != c->launch; k++)
            ;
        c->launch = SCN_LAUNCH[clamp((k < 8 ? k : 0) + d, 0, 7)];
        break;
    case 6: c->name = (uint8_t)clamp(c->name + d, 0, NSCN_NAME - 1); break;
    case 7: c->colour = (uint8_t)clamp(c->colour + d, 0, NSCN_COL - 1); break;
    }
}

static const char *const SCN_LAB[8] = { "TEMPO", "FOLLOW", "AFTER", "CHANCE", "REPEAT", "LAUNCH", "NAME", "COLOUR" };
static void scn_val(int i, char *v, int cap, int *frac, int brief)   /* knob i's value as shown (brief: the
                                                         * all-eight's three characters) */
{
    const qd_scene_t *c = &A.proj.scene[S.scn];
    int k;
    *frac = -1;
    switch (i) {
    case 0:
        if (c->bpm)
            snprintf(v, (size_t)cap, "%d", c->bpm), *frac = (c->bpm - QD_BPM_MIN) * 1000 / (QD_BPM_MAX - QD_BPM_MIN);
        else
            snprintf(v, (size_t)cap, "OFF");
        break;
    case 1: snprintf(v, (size_t)cap, "%s", (brief ? SCN_FOLLOW3 : SCN_FOLLOW)[c->follow % QD_NFOLLOW]); break;
    case 2:
        if (c->after)
            snprintf(v, (size_t)cap, brief ? "%d" : c->after > 1 ? "%d BARS" : "%d BAR", c->after), *frac = c->after * 1000 / 64;
        else
            snprintf(v, (size_t)cap, brief ? "AUT" : "AUTO");
        break;
    case 3: snprintf(v, (size_t)cap, "%d", c->chance), *frac = c->chance * 10; break;
    case 4: snprintf(v, (size_t)cap, "x%d", c->repeats), *frac = (c->repeats - 1) * 1000 / 15; break;
    case 5:
        for (k = 0; k < 8 && SCN_LAUNCH[k] != c->launch; k++)
            ;
        snprintf(v, (size_t)cap, "%s", (brief ? SCN_LAUNCH3 : SCN_LAUNCH_NAME)[k < 8 ? k : 0]);
        break;
    case 6:
        snprintf(v, (size_t)cap, "%s", brief ? SCN_NAME3[c->name % NSCN_NAME] : c->name ? SCN_NAME[c->name % NSCN_NAME] : "NONE");
        break;
    default:
        if (c->colour)
            snprintf(v, (size_t)cap, brief ? "T%d" : "TRACK %d'S", c->colour);
        else
            snprintf(v, (size_t)cap, brief ? "-" : "NONE");
        break;
    }
}

static void scene_say(void)                             /* the scene edited: its number and name */
{
    char v[20];
    const int nm = A.proj.scene[S.scn].name;
    if (nm)
        snprintf(v, sizeof v, "SCENE %d %s", S.scn + 1, SCN_NAME[nm % NSCN_NAME]);
    else
        snprintf(v, sizeof v, "SCENE %d", S.scn + 1);
    show_name(v);
}

/* ARRANGE's knob i: position apage * 8 + i's scene (Shift: its repeats) */
static void arr_knob(int i, int d)
{
    const int p = S.apage * 8 + i;
    if (p >= QD_NSONG)
        return;
    if (S.shift)
        A.proj.song[p][1] = (uint8_t)clamp(A.proj.song[p][1] + d, 1, 16);
    else
        A.proj.song[p][0] = (uint8_t)clamp(A.proj.song[p][0] + d, 0, QD_NSCENE);
}

/* SCENES' pads: the two lower rows the 16 scenes (1-8 above 9-16): tap edits it, Shift + tap launches it */
static void scn_pad(int idx, int down)
{
    const int k = idx < 8 ? idx + 8 : idx - 8;
    if (!down || idx >= 16)
        return;
    if (S.shift)
        scene_launch(k);
    else {
        S.scn = k;
        scene_say();
    }
}

/* ARRANGE's pads: the 32 positions (1-8 the top row); tap: the song from there */
static void arr_pad(int idx, int down)
{
    const int p = (3 - idx / 8) * 8 + idx % 8;
    if (down)
        song_play(p);
}

/* Session's track buttons: its four modes (tapped) */
static void sess_button(int n, int down)
{
    if (!down) {
        if (S.tb_down == n)
            S.tb_down = -1;
        return;
    }
    S.belt = 0;
    if (n == SN_MIX && mix_on()) {                      /* MIX again: its top row hidden / shown */
        S.mix_hide = !S.mix_hide;
        show_name(S.mix_hide ? "MIX: 4 ROWS" : "MIX");
        return;
    }
    S.tb_down = n;                                      /* (held: button 2 + the wheel, the MIDI FX group) */
    S.tb_ms = S.now ? S.now : 1;
    S.sn_mode = n;
    if (n == SN_CLIPS)
        seq_go(SQ_TRACKS);
    else if (n == SN_FX)
        seq_go(SQ_FX);
    else if (n == SN_MFX && !(S.mode == MV_CORE && S.scr == QD_SCR_MFX)) {
        seq_go(SQ_TRACKS);
        mfx_auto_group();
        tap(B_ARP);
    } else if (n == SN_MIX)                             /* (the Move's own: the core at home) */
        seq_go(SQ_TRACKS);
    show_name(SN_NAME[n]);
}

static void sess_toggle(void)                           /* Menu: Sequence <-> Session, each back in its own last mode */
{
    if (S.session)
        S.sn_keep = S.sn_mode;
    else
        S.seq_keep = S.seq_mode;
    S.session = !S.session;
    S.belt = 0;
    S.top_hidden = 0;
    S.page_latch = 0;
    if (S.session) {
        const int n = S.sn_keep;
        if (n < SN_SCENES) {                            /* (the track buttons' four: as tapped) */
            sess_button(n, 1);
            sess_button(n, 0);
        } else {
            S.sn_mode = n;
            seq_go(SQ_TRACKS);
        }
    } else {
        S.seq_mode = S.seq_keep;
        seq_go(S.seq_keep);
    }
    show_name(S.session ? "SESSION" : "SEQUENCE");
}

/* the belt's page chosen, entered: the track buttons' four as tapped; SCENES and ARRANGE (the core at home) */
static void belt_enter(void)
{
    const int n = S.belt_sel;
    S.belt = 0;
    if (n < SN_SCENES) {
        sess_button(n, 1);
        sess_button(n, 0);
        return;
    }
    S.sn_mode = n;
    seq_go(SQ_TRACKS);
    if (n == SN_SCENES)
        scene_say();
    else if (n == SN_SEQ)
        show_name(SQP_NAME[S.seqp]);
    else
        show_name(SN_NAME[n]);
}

/* ------------------------------------------------------------------ input --- */
static void btn(int b, int down) { qd_app_button(&A, b, down); }
static void tap(int b) { btn(b, 1); btn(b, 0); }

static int detents(int acc_i, int delta, int div)
{
    int *acc = &S.knob_acc[acc_i], d;
    *acc += delta;
    d = *acc / div;
    *acc -= d * div;
    return d;
}

/* a held track pad's VOLUME, PAN or a send (TS_*), d detents: through the core's own mix knobs and room sends, the
 * track put selected for the turn (no step is held: a step held would take a lock) */
static void track_mix_of(int t, int k, int d)       /* track t's level, pan or a send turned d detents */
{
    static const struct { int8_t page, knob; } MK[5] = { {0, QD_K_1}, {0, QD_K_2}, {1, QD_K_1}, {0, QD_K_3}, {0, QD_K_4} };
    const int keep_sel = A.sel, keep_page = A.ui.mix_page, keep_held = A.ui.nheld;
    if (t < 0 || t >= QD_TRACKS || k < 0 || k >= TS_N)
        return;
    A.sel = t;
    A.ui.nheld = 0;
    if (k < TS_CRUSH) {
        A.ui.mix_page = MK[k].page;
        qd_mix_knob(&A, MK[k].knob, d);
    } else
        qd_room_turn(&A, k - TS_CRUSH, d);
    A.sel = keep_sel;
    A.ui.mix_page = keep_page;
    A.ui.nheld = keep_held;
}

static void track_mix(int k, int d)                     /* a track pad held: its level, pan or a send */
{
    if (S.tpad < 0)
        return;
    track_mix_of(S.tpad, k, d);
    S.tpad_used = 1;
    S.last_knob = KI_TRACK + k;
    S.turn_ms = S.now ? S.now : 1;
}

/* ------------------------------------------------------------------ Session's MIX --- */
static int mix_on(void) { return S.session && S.sn_mode == SN_MIX && S.mode == MV_HOME && !S.over; }

static int mix_val(int t, int k)                    /* track t's value k: 0..127 (PAN: -64..63) */
{
    const qd_mix_track_t *m = &A.model.t[t].mix;
    return k == TS_VOL ? m->level : k == TS_PAN ? m->pan : k == TS_CHORUS ? m->chorus : k == TS_ECHO ? m->delay
         : k == TS_ROOM ? m->reverb : m->room[k - TS_CRUSH];
}

static void mix_set(int t, int k, int v)            /* .. set (the model's mix: the voices take it at the control rate) */
{
    qd_mix_track_t *m = &A.model.t[t].mix;
    if (k == TS_PAN)
        m->pan = (int8_t)(v < -64 ? -64 : v > 63 ? 63 : v);
    else {
        const uint8_t u = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        if (k == TS_VOL) m->level = u;
        else if (k == TS_CHORUS) m->chorus = u;
        else if (k == TS_ECHO) m->delay = u;
        else if (k == TS_ROOM) m->reverb = u;
        else m->room[k - TS_CRUSH] = u;
    }
}

/* the pads in MIX: the top row what is set (pad 8: WARP, GLITCH, TV in turn); the three rows below a column a track,
 * tapped at a height: the value there (a third, two thirds, full; PAN: left, centre, right; the lowest tapped again at
 * its height: 0), two taps: the default; Mute held: the track muted */
static int mix_rows(void) { return S.mix_hide ? 4 : 3; }   /* the rows of columns */
static int mix_target(int row)                          /* a row's value: the bottom 0 (PAN: left), the top 127 (right) */
{
    const int nr = mix_rows() - 1;
    return S.mix_k == TS_PAN ? -64 + (row * 127 + nr / 2) / nr : (row * 127 + nr / 2) / nr;
}

/* each look: a column's pad held glides its value to the pad's height, a light touch slowly (about 20 s across), a
 * hard press quickly (about 0.2 s) */
static void mix_glide(void)
{
    const uint32_t dt = S.mxg_ms ? S.now - S.mxg_ms : 0;
    int c;
    S.mxg_ms = S.now ? S.now : 1;
    if (!mix_on())
        memset(S.mxg_row, -1, sizeof S.mxg_row);
    for (c = 0; c < 8 && c < QD_TRACKS; c++) {
        float to, rate, step;
        if (S.mxg_row[c] < 0 || !dt)
            continue;
        to = (float)mix_target(S.mxg_row[c]);
        rate = 6.0f * powf(2.0f, S.mxg_pres[c] / 18.0f);   /* (values a second: 6 .. 768) */
        step = rate * (float)(dt > 100 ? 100 : dt) / 1000.0f;
        S.mxg_v[c] = S.mxg_v[c] < to ? (S.mxg_v[c] + step > to ? to : S.mxg_v[c] + step)
                                     : (S.mxg_v[c] - step < to ? to : S.mxg_v[c] - step);
        if ((int)lrintf(S.mxg_v[c]) != mix_val(c, S.mix_k))
            mix_set(c, S.mix_k, (int)lrintf(S.mxg_v[c]));
    }
}

static void mix_pressure(int idx, int p)                /* a column's pad pressed harder or lighter */
{
    const int c = idx % 8, row = idx / 8;
    if (idx >= 0 && idx < 32 && c < 8 && S.mxg_row[c] == row)
        S.mxg_pres[c] = (uint8_t)(p < 0 ? 0 : p > 127 ? 127 : p);
}

static void mix_pad_v(int idx, int down, int vel)
{
    const int c = idx % 8, row = idx / 8;
    if (!down) {
        if (S.pad_note[idx] >= 0)
            note_pad(idx, 0);
        if (c < 8 && S.mxg_row[c] == row)
            S.mxg_row[c] = -1;
        return;
    }
    if (row == 3 && !S.mix_hide) {
        S.mix_k = c < 7 ? c : S.mix_k >= TS_WARP && S.mix_k < TS_TV ? S.mix_k + 1 : TS_WARP;
        show_name(TS_NAME[S.mix_k]);
        return;
    }
    if (c >= QD_TRACKS)
        return;
    if (S.mute_down) {
        S.mute_used = 1;
        A.seq.mute ^= (uint8_t)(1u << c);
        return;
    }
    qd_ctl_select_track(&A, c);
    if (S.shift) {                                      /* Shift: straight to this pad's height */
        S.mxg_row[c] = -1;
        mix_set(c, S.mix_k, mix_target(row));
    } else if (S.mx_tap_ms[idx] && S.now - S.mx_tap_ms[idx] < DOUBLE_MS) {   /* two taps: the default, at once */
        S.mx_tap_ms[idx] = 0;
        S.mxg_row[c] = -1;
        mix_set(c, S.mix_k, S.mix_k == TS_VOL ? QD_MIX_LEVEL_DEFAULT : 0);
    } else {                                            /* held: the value glides to this pad's height */
        S.mx_tap_ms[idx] = S.now ? S.now : 1;
        S.mxg_row[c] = (int8_t)row;
        S.mxg_pres[c] = (uint8_t)(vel > 127 ? 127 : vel);
        S.mxg_v[c] = (float)mix_val(c, S.mix_k);
    }                                                   /* (no popup: the screen's bars show it; the knobs keep theirs) */
}

static int mix_lit(int t, int row)                  /* a column's pad lit: up to the value (PAN: the nearest pad) */
{
    const int v = mix_val(t, S.mix_k), nr = mix_rows() - 1;
    if (S.mix_k == TS_PAN)
        return row == ((v + 64) * nr + 63) / 127;
    return v > 0 && v * 2 * nr >= (2 * row - 1) * 127;
}

/* a held step's micro timing or velocity (the hold modes of the core's knob 1, as the home knobs) */
static void step_hold(int which, int d)
{
    const int keep = A.ui.hmode;
    A.ui.hmode = which == KI_MICRO ? UI_F_MICRO : UI_F_VEL;
    qd_app_knob(&A, QD_K_1, d);
    A.ui.hmode = keep;
    S.last_knob = which;
    S.turn_ms = S.now ? S.now : 1;
}

/* the mod matrix (design/WORKFLOW.md "Mod mapping"): ENV held: the envelope last chosen on its pad (ENV 1-3) is a
 * modulation slot's source, knob i's value its destination, the turn its depth (the core's B1 hold-to-map); LFO held:
 * the same with the LFO last chosen on its pad (a slot too: the LFO keeps its own destination; eight slots a sound).
 * The popup: ENV2>CUTOFF +40; a value nothing can modulate: NO MOD */
static void mod_map(int i, int d)
{
    const spage_t *sp = &SND[S.spage];
    const int env = S.mod_hold == 4, n = env ? S.grp_last[4] - 6 : S.grp_last[5] - 14;
    const int keep_pg = A.ui.snd_page, keep_lk = A.ui.snd_lk, keep_src = A.ui.mod_src;
    char v[24];
    int id, dest, dep = 0;
    S.mod_used = 1;
    if (n < 0 || n > 2)
        return;
    A.ui.snd_page = g_snd_core[S.spage][i];
    id = qd_snd_ids(&A, A.ui.snd_page)[sp->e[i].k == 4 ? 4 : sp->e[i].k];
    {
        const int src = (env ? QD_MS_ENV1 : QD_MS_LFO1) + n;   /* (a slot either way: the LFO keeps its own DEST) */
        A.ui.snd_lk = 4;
        A.ui.mod_src = src;
        A.ui.snd_lk_bad_ms = 0;
        qd_app_knob(&A, sp->e[i].k == 4 ? QD_K_ALGO : QD_K_1 + sp->e[i].k, d);
        dest = id >= 0 ? qd_quad_dest_of(id, 1) : -1;
        if (dest >= 0 && !A.ui.snd_lk_bad_ms) {
            const int slot = qd_snd_mod_slot_of(&A, src, dest);
            dep = slot >= 0 ? A.model.t[A.sel].patch[QD_QP_MOD(slot, QD_QM_DEPTH)] : 0;
        }
    }
    if (dest >= 0 && !A.ui.snd_lk_bad_ms) {
        snprintf(S.mod_name, sizeof S.mod_name, "%s%d>%.10s", env ? "ENV" : "LFO", n + 1, qd_quad_dest_name(dest));
        snprintf(S.mod_val, sizeof S.mod_val, "%+d", dep);
    } else {
        snprintf(S.mod_name, sizeof S.mod_name, "%s%d", env ? "ENV" : "LFO", n + 1);
        snprintf(S.mod_val, sizeof S.mod_val, "NO MOD");
    }
    (void)v;
    A.ui.snd_page = keep_pg, A.ui.snd_lk = keep_lk, A.ui.mod_src = keep_src;
}

static void pj_knob(int i, int d);
static void knob(int i, int delta)
{
    const int d = detents(i, delta, KNOB_DIV);
    if (d && i == 0 && S.mode == MV_CORE && S.scr == QD_SCR_MFX && S.mfx_pad < 0 && !S.over) {
        /* MIDI FX: knob 1 on an empty slot chooses its effect (as the slot held + knob 1), and goes on choosing while it
         * turns (a second's pause: the effect's own first value again) */
        const int g = qd_mfx_tgroup(&A), sl = A.ui.mfx_slot & 7;
        if (g && (A.proj.mfx[g - 1].slot[sl].type == QD_MFX_NONE || (S.mfx_k1_ms && S.now - S.mfx_k1_ms < 1000u))) {
            const int pg = A.ui.mfx_page;
            qd_app_key(&A, QD_BLACK_KEY[sl], 1, STEP_VEL);
            qd_app_knob(&A, QD_K_1, d);
            qd_app_key(&A, QD_BLACK_KEY[sl], 0, 0);
            A.ui.mfx_page = pg;
            S.mfx_k1_ms = S.now ? S.now : 1;
            S.last_knob = 0, S.turn_ms = S.now ? S.now : 1;
            return;
        }
    }
    S.touch_turned = 1;
    S.last_knob = i;
    S.turn_ms = S.now ? S.now : 1;
    if (d && S.over == OV_PROJECTS) {                   /* PROJECTS: the open one's name, colour */
        pj_knob(i, d);
        return;
    }
    if (d && S.mute_down && i < QD_TRACKS) {            /* Mute held: knob i track i's volume (touched alone: its mute) */
        S.mk_turned |= 1u << i;
        track_mix_of(i, TS_VOL, d);
        return;
    }
    if (!d || S.over || S.mute_down)
        return;
    if (S.tpad >= 0) {                                  /* a track pad held: knob i its send i */
        track_mix(TS_CHORUS + i, d);
        return;
    }
    if (mix_on()) {                                     /* Session's MIX: knob i track i's value (its pop-up) */
        track_mix_of(i, S.mix_k, d);
        return;
    }
    if (scn_on()) {                                     /* SCENES: the edited scene's eight */
        scn_knob(i, d);
        return;
    }
    if (arr_on()) {                                     /* ARRANGE: a position's scene (Shift: its repeats) */
        arr_knob(i, d);
        return;
    }
    if (sq_on()) {                                      /* SEQ: the subpage's settings */
        sq_knob(i, d);
        return;
    }
    if (S.mode == MV_SOUND) {
        const spage_t *sp = &SND[S.spage];
        if (i < sp->n && S.mod_hold >= 0) {             /* ENV / LFO held: knob i's value modulated by the one last
                                                         * chosen on that pad, the turn its depth */
            mod_map(i, d);
            return;
        }
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
        if (i < QD_TRACKS) {                            /* a fader a track: the core's LEVEL knob with that track selected */
            const int keep = A.sel;
            A.sel = i;
            qd_app_knob(&A, QD_K_1, d);
            A.sel = keep;
        } else if (i == 6)
            qd_app_knob(&A, QD_K_2, d);                 /* the selected track's pan (six tracks: knob 7) */
    } else if (S.mode == MV_HOME) {                     /* the core's hold mode put on this knob's for the turn */
        const int keep = A.ui.hmode, second = S.shift && i >= 1 && i <= 5;
        A.ui.hmode = S.shift && i == 7 ? UI_F_FUNC : HOME_MODE[i];
        qd_app_knob(&A, second ? QD_K_2 : QD_K_1, d);
        A.ui.hmode = keep;
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
    } else if (S.over == OV_POWER) {
        S.menu_sel = d > 0;
    } else if (S.over == OV_TEMPO) {
        const int bpm = qd_seq_bpm(&A.seq) + d;
        qd_ctl_set_tempo(&A, bpm < 30 ? 30 : bpm > 300 ? 300 : bpm);
    } else if (S.mode == MV_BROWSE) {                   /* the sounds: each one heard on the track as it comes up */
        qd_br_turn(&A, d);
    } else if (S.belt && S.session && S.mode == MV_HOME) {   /* the belt: the page chosen */
        const int b = clamp(S.belt_sel + d, 0, SN_N - 1);
        if (b != S.belt_sel)
            S.belt_from = S.belt_sel, S.belt_ms = S.now ? S.now : 1;
        S.belt_sel = b;
        S.name_until = S.now;                           /* (a name showing gives way to it) */
    } else if (clips_on() && S.shift) {                 /* CLIPS, Shift: the launch quantize */
        int i, k = 0;
        for (i = 0; i < (int)sizeof LAUNCH_ORDER; i++)
            if (LAUNCH_ORDER[i] == A.proj.launch)
                k = i;
        k = k + d < 0 ? 0 : k + d >= (int)sizeof LAUNCH_ORDER ? (int)sizeof LAUNCH_ORDER - 1 : k + d;
        A.proj.launch = LAUNCH_ORDER[k];
        launch_say();
    } else if (scn_on()) {                              /* SCENES: the scene edited */
        S.scn = clamp(S.scn + d, 0, QD_NSCENE - 1);
        scene_say();
    } else if (sq_on()) {                               /* SEQ: the subpage */
        S.seqp = clamp(S.seqp + (d < 0 ? -1 : 1), 0, SQP_N - 1);
        show_name(SQP_NAME[S.seqp]);
    } else if (arr_on()) {                              /* ARRANGE: the positions shown, eight a page */
        char v[20];
        S.apage = clamp(S.apage + (d < 0 ? -1 : 1), 0, QD_NSONG / 8 - 1);
        snprintf(v, sizeof v, "SONG %d-%d", S.apage * 8 + 1, S.apage * 8 + 8);
        show_name(v);
    } else if (mix_on()) {                              /* MIX: the wheel does nothing (Back leaves it) */
    } else if (clips_on()) {                            /* CLIPS: the page belt (Plus / Minus move the rows) */
        S.belt = 1;
        S.name_until = S.now;
        S.belt_from = S.sn_mode;
        S.belt_ms = S.now ? S.now : 1;
        S.belt_sel = clamp(S.sn_mode + (d < 0 ? -1 : 1), 0, SN_N - 1);
    } else if (page_mode()) {                           /* the clip's length in pages: longer, the new pages in the
                                                         * loop; shorter, the pages past it out (muted pages stay) */
        const int n0 = trk_npage(), n = n0 + d < 1 ? 1 : n0 + d > QD_NPAGE ? QD_NPAGE : n0 + d;
        unsigned m = trk_loop();
        if (n > n0)
            m |= ((1u << n) - 1u) & ~((1u << n0) - 1u);
        else
            m &= (1u << n) - 1u;
        set_loop(m ? m : 1u);
        S.loop_used = 1;
        {
            char v[20];
            snprintf(v, sizeof v, "%d PAGES", n);
            show_name(v);
        }
    } else if (S.mode == MV_CORE && S.scr == QD_SCR_MFX && !S.shift) {   /* MIDI FX: a slot held, it moves (the core's
                                                         * KNOB 2 with its B held); a step held, or Session's track button
                                                         * 2, the group (PRESETS: the steps' or the track's); else the
                                                         * slot's pages (SELECT) */
        qd_app_knob(&A, S.mfx_pad >= 0 ? QD_K_2 : A.ui.nheld || (S.session && S.tb_down == SN_MFX) ? QD_K_PRESETS
                                                                                                    : QD_K_SELECT, d);
    } else if (S.tpad >= 0) {                           /* a track pad held: its pan */
        track_mix(TS_PAN, d);
    } else if (!S.shift && S.mode == MV_HOME && A.ui.nheld) {   /* a step held: its micro timing */
        step_hold(KI_MICRO, d);
    } else if (S.shift && S.mode != MV_CORE) {
        if (A.ui.nheld) {                               /* Shift, a step held: that step's own sound (a lock) */
            qd_app_knob(&A, QD_K_PRESETS, d);
            S.last_knob = 8;
            S.turn_ms = S.now ? S.now : 1;
        } else
            qd_br_turn(&A, 0);                          /* Shift: the track's sound: the browser opens where it is */
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
    if (S.over == OV_POWER) {
        if (!down)
            return;
        S.over = OV_NONE;
        if (S.menu_sel == 1)
            S.quit = 2;
        return;
    }
    if (S.belt && S.session && S.mode == MV_HOME && !S.over) {   /* the belt: into the page chosen */
        if (down)
            belt_enter();
        return;
    }
    if (scn_on() && !S.shift) {                         /* SCENES: the edited scene launched */
        if (down)
            scene_launch(S.scn);
        return;
    }
    if (arr_on()) {                                     /* ARRANGE: the song from the start; again: stopped */
        if (down) {
            if (S.song_on)
                song_stop();
            else
                song_play(0);
        }
        return;
    }
    if (clips_on() && S.shift) {                        /* CLIPS, Shift: EMPTY SLOT (a scene over a track's empty slot
                                                         * stops it, or leaves it playing) */
        if (down) {
            A.proj.empty_keep ^= 1u;
            show_name(A.proj.empty_keep ? "EMPTY SLOT KEEP" : "EMPTY SLOT STOP");
        }
    } else if (S.mode == MV_BROWSE) {                   /* the sound shown: taken */
        if (down)
            qd_br_load(&A);
    } else if (S.mode == MV_CORE)                       /* the core's own menus: yes / enter (its OCT+) */
        btn(B_OCTUP, down);
}

/* ------------------------------------------------------------------ PROJECTS --- */
/* Shift + step 1: the 32 projects on the pads, 1-8 the top row. A tap loads one (an empty pad: a new project there);
 * Copy held + a project, then an empty pad: copied; Delete held + a project twice: deleted. Knob 1: the open project's
 * name (the core's ideas for it), knob 2: its colour (a track's). Back, or Shift + step 1 again: closed */
static int pj_at(int idx) { return (3 - idx / 8) * 8 + idx % 8; }

static void pj_say(const char *what, int p)
{
    char v[20];
    snprintf(v, sizeof v, "%s %d", what, p + 1);
    show_name(v);
}

static void pj_open(void)
{
    int kind[16];
    if (S.over == OV_PROJECTS) {
        S.over = OV_NONE;
        return;
    }
    S.over = OV_PROJECTS;
    S.pj_take = S.pj_del = -1;
    S.pj_nidea = qd_name_project_ideas(S.now ^ 0x5EED5u, A.per.pname[A.per.proj], S.pj_ideas, kind, 16);
    S.pj_idea = 0;
}

static void pj_changed(void)                            /* (another project open: the Move's own state of the last one) */
{
    S.run = -1, S.song_on = 0;
    S.pj_nidea = 0;
}

static void pj_pad(int idx, int down)
{
    const int p = pj_at(idx);
    if (!down || p < 0 || p >= QD_NPROJ)
        return;
    if (S.copy_down) {                                  /* Copy: the project taken, then where it goes (an empty pad) */
        S.copy_used = 1;
        if (S.pj_take < 0) {
            if (A.per.pexists[p])
                S.pj_take = p, pj_say("COPY", p);
        } else if (A.per.pexists[p])
            show_name("NOT EMPTY");
        else {
            if (qd_proj_copy(&A, S.pj_take, p))
                pj_say("COPIED TO", p);
            S.pj_take = -1;
        }
        return;
    }
    if (S.delete_down) {                                /* Delete: armed, then deleted on the same pad */
        if (!A.per.pexists[p])
            return;
        if (S.pj_del == p) {
            S.pj_del = -1;
            if (qd_proj_delete(&A, p)) {
                pj_say("DELETED", p);
                if (p == A.per.proj)
                    pj_changed();
            }
        } else {
            S.pj_del = p;
            show_name("DELETE? AGAIN");
        }
        return;
    }
    S.pj_del = -1;
    if (p == A.per.proj)
        show_name(A.per.pname[p][0] ? A.per.pname[p] : "THIS ONE");
    else if (A.per.pexists[p]) {
        if (qd_proj_load(&A, p)) {
            pj_changed();
            show_name(A.per.pname[p][0] ? A.per.pname[p] : "LOADED");
        }
    } else if (qd_proj_new(&A, p)) {
        pj_changed();
        pj_say("NEW PROJECT", p);
    }
}

static void pj_knob(int i, int d)
{
    const int p = A.per.proj;
    if (i == 0) {                                       /* the name: the ideas for it */
        if (!S.pj_nidea) {
            int kind[16];
            S.pj_nidea = qd_name_project_ideas(S.now ^ 0x5EED5u, A.per.pname[p], S.pj_ideas, kind, 16);
            S.pj_idea = 0;
        }
        if (S.pj_nidea > 0) {
            S.pj_idea = clamp(S.pj_idea + d, 0, S.pj_nidea - 1);
            qd_proj_name(&A, p, S.pj_ideas[S.pj_idea]);
        }
    } else if (i == 1)                                  /* its colour */
        qd_proj_colour(&A, p, clamp(A.per.pcol[p] + d, 0, 8));
}

/* Shift + a step: the Move's shortcut there */
static void shift_step(int i)
{
    if (SHIFT_STEP[i] == SS_MIDIFX)
        mfx_auto_group();
    switch (SHIFT_STEP[i]) {
    case SS_SETUP: S.over = S.over == OV_SETUP ? OV_NONE : OV_SETUP; S.menu_sel = 0; break;
    case SS_TEMPO: S.over = S.over == OV_TEMPO ? OV_NONE : OV_TEMPO; break;
    case SS_METRO: qd_shift_black(&A, 6); break;        /* (the core's SHIFT + B7, by name) */
    case SS_DOUBLE: qd_shift_black(&A, 8); break;       /* (SHIFT + B9) */
    case SS_NEWCLIP: new_clip(); break;
    case SS_PROJECTS: pj_open(); break;
    case SS_MIDIFX: tap(B_ARP); break;                  /* the MIDI FX page (the core's ARP, tapped) */
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

/* Mute held + a track's pad or knob: muted (again: heard); two taps, or Shift: soloed */
static void mute_press(int t)
{
    if (t < 0 || t >= QD_TRACKS)
        return;
    S.mute_used = 1;
    if (S.shift)                                        /* Shift + Mute + a track: solo (the Ableton gesture) */
        A.seq.solo ^= (uint8_t)(1u << t);
    else if (S.mute_tap_ms[t] && S.now - S.mute_tap_ms[t] < DOUBLE_MS) {   /* a double tap: solo (the first tap's mute
                                                         * taken back) */
        A.seq.mute ^= (uint8_t)(1u << t);
        A.seq.solo ^= (uint8_t)(1u << t);
        S.mute_tap_ms[t] = 0;
    } else {
        A.seq.mute ^= (uint8_t)(1u << t);
        S.mute_tap_ms[t] = S.now ? S.now : 1;
    }
}

/* a track's pad or button: select it; with Mute held: mute it (Shift too: solo) */
static void track_press(int t)
{
    if (t < 0 || t >= QD_TRACKS)
        return;
    if (S.mute_down)
        mute_press(t);
    else
        qd_ctl_select_track(&A, t);
}

/* the top pad row: the page keys. In SOUND the eight groups; in FX the floors (the last pad steps through the two
 * floors past the eighth); at home the six tracks; on the core's own pages its black keys B1..B8 */
static void page_pad(int col, int down, int vel)
{
    if (S.loop_down || S.mute_down) {                   /* Loop held: the core's bank keys; Mute held: the tracks */
        if (S.mute_down) {
            if (down)
                track_press(col);
        } else
            qd_app_key(&A, QD_BLACK_KEY[col], down, down ? vel : 0);
    } else if (S.mode == MV_SOUND) {                    /* TRIG / SOUND: six page pads, then RND PAGE, RND SOUND */
        if (col >= 6) {                                 /* held a second: shaken (egm_ui_step); a tap does nothing */
            if (down)
                S.rnd_pad = col, S.rnd_ms = S.now ? S.now : 1, S.rnd_done = 0;
            else if (S.rnd_pad == col)
                S.rnd_pad = -1;
        } else if (down && S.shift) {                   /* Shift: the dice, this page shaken at once */
            qd_snd_randomise(&A, g_snd_core[S.spage][0]);
            show_name("SHAKEN");
        } else if (col == 4 || col == 5) {              /* ENV, LFO: held + a knob, the mod matrix; a tap its page */
            if (down)
                S.mod_hold = col, S.mod_used = 0;
            else if (S.mod_hold == col) {
                S.mod_hold = -1;
                if (!S.mod_used)
                    snd_pad(col);
            }
        } else if (down)
            snd_pad(col);
    } else if (S.mode == MV_FX) {                       /* the floors; pad 7: WARP, GLITCH, TV in turn; pad 8: PERFORM
                                                         * (not built yet) */
        if (down && col < 6)
            fx_go(col);
        else if (down && col == 6)
            fx_go(S.fpage >= 6 && S.fpage < 8 ? S.fpage + 1 : 6);
        else if (down && col == 7) {                    /* PERFORM on / off */
            if (S.perform)
                pf_off();
            else
                S.perform = 1, S.pf_last = -1;
            show_name(S.perform ? "PERFORM" : "PERFORM OFF");
        }
    } else if (S.mode == MV_CORE && S.scr == QD_SCR_MFX) {   /* MIDI FX: the slots (the core's B1..B8: tap selects,
                                                         * again its next page; held, knob 1 its effect, the wheel moves
                                                         * it); two taps: the slot bypassed (the core's B10) */
        if (down && S.mfx_tap_ms[col] && S.now - S.mfx_tap_ms[col] < DOUBLE_MS) {
            S.mfx_tap_ms[col] = 0;
            S.mfx_dbl = col;
            qd_app_key(&A, QD_BLACK_KEY[9], 1, STEP_VEL);
            qd_app_key(&A, QD_BLACK_KEY[9], 0, 0);
            return;
        }
        if (!down && S.mfx_dbl == col) {
            S.mfx_dbl = -1;
            return;
        }
        if (down)
            S.mfx_tap_ms[col] = S.now ? S.now : 1;
        S.mfx_pad = down ? col : S.mfx_pad == col ? -1 : S.mfx_pad;
        {   /* (the selected slot tapped again: the core would turn its page; here the wheel turns pages, so the page
             * stays and two taps only bypass) */
            const int pg = A.ui.mfx_page, again = col == A.ui.mfx_slot;
            qd_app_key(&A, QD_BLACK_KEY[col], down, down ? vel : 0);
            if (again)
                A.ui.mfx_page = pg;
        }
    } else if (S.mode == MV_HOME) {                     /* TRACKS: select; held, Volume / the wheel / the knobs are its */
        if (down) {
            track_press(col);
            S.tpad = col < QD_TRACKS ? col : -1;
            S.tpad_used = 0;
        } else if (S.tpad == col)
            S.tpad = -1;
    } else
        qd_app_key(&A, QD_BLACK_KEY[col], down, down ? vel : 0);
}

/* the idle screen (the laughing brain): any press of a pad, a step or a button wakes it and does nothing else (its
 * release too); turns and touches leave it be. The core's own wake takes the press (as PLAY's at the title) */
static uint8_t g_eat_note[128], g_eat_cc[128];
static int idle_eat(int is_cc, int n, int down)
{
    uint8_t *eat = is_cc ? g_eat_cc : g_eat_note;
    if (n < 0 || n > 127)
        return 0;
    if (!down && eat[n]) {
        eat[n] = 0;
        return 1;
    }
    if (!down || S.mode != MV_IDLE || S.over)
        return 0;
    eat[n] = 1;
    tap(B_PLAY);                                        /* (at idle the core only wakes: playback does not start) */
    return 1;
}

static void surface_note(int note, int vel)
{
    static uint16_t step_eaten;                         /* a step whose press was a shortcut: its release is no key */
    const int down = vel > 0;
    if (note >= N_STEP0 && idle_eat(0, note, down))
        return;
    if (note <= N_JOG_TOUCH) {
        if (note < N_VOL_TOUCH && (S.mute_down || (S.mk_touch >> note & 1u))) {   /* Mute held, anywhere: knob n
                                                         * touched and let go mutes track n (two touches, or Shift:
                                                         * solos); turned instead, its volume */
            const unsigned b = 1u << (note - N_KNOB_TOUCH0);
            if (down) {
                S.mk_touch |= b, S.mk_turned &= ~b;
                S.touch = note, S.touch_turned = 0;
            } else {
                if ((S.mk_touch & b) && !(S.mk_turned & b))
                    mute_press(note - N_KNOB_TOUCH0);
                S.mk_touch &= ~b;
                if (S.touch == note)
                    S.touch = -1;
            }
            return;
        }
        if (note == N_VOL_TOUCH)
            S.vol_touch = down;
        if (note == N_JOG_TOUCH)                        /* (a finger on the wheel: the knobs keep the all-eight view) */
            S.jog_touch = down;
        if (down)
            S.touch = note, S.touch_turned = 0;
        else if (S.touch == note)
            S.touch = -1;
        return;
    }
    if (note >= N_STEP0 && note < N_STEP0 + 16) {
        const int i = note - N_STEP0;
        if (page_mode() && !S.shift) {                 /* page mode: the steps are the pages */
            page_step(i, down);
            return;
        }
        if ((scn_on() || arr_on() || sq_on()) && !S.shift) {   /* SCENES, ARRANGE, SEQ: the steps launch the scenes */
            if (down)
                scene_launch(i);
            return;
        }
        if ((clips_on() || mix_on()) && !S.shift) {   /* CLIPS, MIX: stop and mute, two under each track */
            if (down)
                clip_step(i);
            return;
        }
        if (down && ((S.shift && !S.loop_down) || S.mode == MV_BROWSE)) {   /* (Shift + Loop held: the steps are the
                                                         * patterns. In the sounds the steps do nothing: no slot picked
                                                         * by a stray finger) */
            step_eaten |= (uint16_t)(1u << i);
            if (S.shift)
                shift_step(i);
        } else if (!down && (step_eaten >> i & 1u)) {
            step_eaten &= (uint16_t)~(1u << i);
        } else
            qd_app_key(&A, QD_WHITE_KEY[i], down, STEP_VEL);
        return;
    }
    if (note >= N_PAD0 && note < N_PAD0 + 32) {
        const int idx = note - N_PAD0;                  /* row 0 the bottom */
        if (S.over == OV_PROJECTS) {                    /* PROJECTS: the 32 projects */
            pj_pad(idx, down);
            return;
        }
        if (clips_on()) {                               /* CLIPS: all four rows the clips */
            clip_pad(idx, down);
            return;
        }
        if (scn_on()) {                                 /* SCENES: the lower two rows the scenes */
            scn_pad(idx, down);
            return;
        }
        if (arr_on()) {                                 /* ARRANGE: the 32 positions */
            arr_pad(idx, down);
            return;
        }
        if (sq_on())                                    /* SEQ: the pads do nothing */
            return;
        if (mix_on()) {                                 /* MIX: the top row what is set, the rows below the tracks */
            mix_pad_v(idx, down, vel);
            return;
        }
        if (page_mode() && idx < 16) {                  /* rows 3-4: the shown page's start and end */
            range_pad(idx >= 8 ? idx - 8 : idx + 8, down);
            return;
        }
        if (page_mode() && idx < 24)
            return;
        if (S.mode == MV_FX && S.perform && idx < 24 && S.pad_note[idx] < 0) {   /* PERFORM: the performance pads */
            pf_pad(idx, down);
            return;
        }
        if (idx >= 24 && (top_row_on() || !down) && S.pad_note[idx] < 0)
            page_pad(idx - 24, down, vel);
        else
            note_pad(idx, vel);                         /* (the top row hidden: the fourth row of keys) */
    }
}

static int rel(int v) { return v < 64 ? v : v - 128; }                      /* a relative encoder's count */

static void surface_cc(int cc, int v)
{
    const int down = v > 0;
    if (cc != CC_JOG && cc != CC_VOLUME && !(cc >= CC_KNOB1 && cc < CC_KNOB1 + 8) && idle_eat(1, cc, down))
        return;
    if (cc >= CC_KNOB1 && cc < CC_KNOB1 + 8) {
        knob(cc - CC_KNOB1, rel(v));
        return;
    }
    switch (cc) {
    case CC_JOG: S.touch_turned = 1; wheel(rel(v)); break;
    case CC_VOLUME:
        if (S.tpad >= 0 || (S.mode == MV_HOME && A.ui.nheld && !S.shift)) {   /* a track pad held: its volume; a step
                                                         * held: its velocity (the master untouched) */
            const int d = detents(9, rel(v), KNOB_DIV);
            if (d && S.tpad >= 0)
                track_mix(TS_VOL, d);
            else if (d)
                step_hold(KI_VEL, d);
            break;
        }
        S.pot += rel(v) * 8;
        S.pot = S.pot < 0 ? 0 : S.pot > 1023 ? 1023 : S.pot;
        qd_app_master(&A, S.pot);
        S.last_knob = KI_MASTER;                        /* (its popup while it turns) */
        S.turn_ms = S.now ? S.now : 1;
        break;
    case CC_JOG_CLICK: wheel_click(down); break;
    case CC_SHIFT: S.shift = down; break;               /* the Move's own modifier: the core never sees a SHIFT (its SHIFT
                                                         * sticks on a quick press, and means other things on its pages);
                                                         * what Shift does here calls the core's functions by name */
    case CC_MENU: {                                     /* Sequence <-> Session. Shift: the core's HOME (again: its next
                                                         * view; held: its settings) */
        static int sent;
        if (down)
            sent = S.over ? 0 : S.shift ? 2 : 1;
        if (sent == 0) {
            if (down)
                S.over = OV_NONE;
        } else if (sent == 2)
            btn(B_HOME, down);
        else if (down)
            sess_toggle();
        break;
    }
    case CC_BACK: {                                     /* out of a page (HOME), or a menu's step back (OCT-) */
        static int sent = -1;
        if (down) {
            if (S.over) {
                S.over = OV_NONE;
                sent = -1;
                break;
            }
            if (S.mode == MV_BROWSE) {                  /* out of the sounds: the track's own sound back */
                qd_br_cancel(&A);
                sent = -1;
                break;
            }
            if (S.session && S.mode == MV_HOME && S.belt) {   /* the belt closed: the clips (SCENES, ARRANGE: left) */
                S.belt = 0;
                if (S.sn_mode >= SN_SCENES)
                    S.sn_mode = SN_CLIPS;
                sent = -1;
                break;
            }
            if (mix_on()) {                             /* MIX: back to CLIPS (the only way out but the buttons) */
                sess_button(SN_CLIPS, 1);
                sess_button(SN_CLIPS, 0);
                sent = -1;
                break;
            }
            if (scn_on() || arr_on() || sq_on()) {      /* SCENES, ARRANGE, SEQ: up to the belt */
                S.belt = 1;
                S.name_until = S.now;
                S.belt_sel = S.sn_mode;
                sent = -1;
                break;
            }
            /* out of whatever page is open: the core's HOME, which closes any of them. A settings menu steps back
             * a level first (its OCT-). At home there is nothing to go back from */
            sent = QD_LATCH_MENU(A.ui.latch) ? B_OCTDN : S.mode == MV_HOME || S.mode == MV_IDLE ? -1 : B_HOME;
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
            if (down)
                tap(B_SAVE);
        } else
            btn(B_EDIT, down);
        break;
    case CC_MUTE:                                       /* held: the top row is the tracks (tap: mute, double tap or
                                                         * Shift: solo). Tapped: the selected track muted / heard.
                                                         * Shift + Mute tapped: the mix (until Session has its MIX) */
        if (down)
            S.mute_down = 1, S.mute_used = 0;
        else {
            S.mute_down = 0;
            if (!S.mute_used) {
                if (S.shift)
                    tap(B_ENV);
                else
                    A.seq.mute ^= (uint8_t)(1u << A.sel);
            }
        }
        break;
    case CC_LOOP:                                       /* page mode: tapped latched (again: off), held while held.
                                                         * Shift + Loop held: the patterns (until Session) */
        if (down) {
            S.loop_btn = 1, S.loop_ms = S.now ? S.now : 1, S.loop_used = 0;
            if (S.shift) {
                S.loop_down = 1;
                btn(B_SEQ, 1);
            }
        } else {
            S.loop_btn = 0;
            if (S.loop_down) {
                S.loop_down = 0;
                btn(B_SEQ, 0);
            } else if (!S.loop_used && S.now - S.loop_ms < HOLD_MS) {
                S.page_latch = !S.page_latch;
                show_name(S.page_latch ? "PAGES" : "STEPS");
            }
            S.pg_held = S.pd_held = -1;
        }
        break;
    case CC_COPY:                                       /* the pattern copied; Shift: pasted (SHIFT + B4 / B5). Page
                                                         * mode: held, a page tapped is copied, then pasted */
        S.copy_down = down;
        if (clips_on() || (!down && S.cl_take >= 0)) {  /* CLIPS: held, a clip tapped is copied, then pasted; tapped
                                                         * alone, the selected clip duplicated into the next empty slot */
            if (down)
                S.copy_used = 0;
            else if (!S.copy_used && clips_on())
                clip_duplicate();
            if (!down && !S.delete_down)
                S.cl_take = -1;
        } else if (page_mode()) {
            if (!down)
                S.pg_copied = -1;
        } else if (down)
            qd_shift_black(&A, S.shift ? 4 : 3);
        break;
    case CC_DELETE:                                     /* the core's B11: clear. Shift: the track (pressed twice). Page
                                                         * mode: held, a page tapped is cleared */
        S.delete_down = down;
        if (clips_on() || (!down && S.cl_take >= 0)) {  /* CLIPS: held, a clip tapped is cut, then pasted (else gone) */
            if (!down && !S.copy_down)
                S.cl_take = -1;
            break;
        }
        if (page_mode())
            break;
        if (!S.shift)
            qd_app_key(&A, QD_BLACK_KEY[10], down, STEP_VEL);
        else if (down)
            qd_shift_black(&A, 10);
        break;
    case CC_PLAY:                                       /* Session: Shift + Play stops every clip at the next bar */
        if (S.session && S.shift) {
            if (down) {
                int t;
                for (t = 0; t < QD_TRACKS; t++)
                    clip_stop(t);
                show_name("STOP ALL");
            }
        } else
            btn(B_PLAY, down);
        break;
    case CC_REC:                                        /* armed: the pads' notes are recorded (stopped: a capture
                                                         * that starts with the first note); again: off */
        if (down) {
            if (A.ui.rec)
                qd_rec_toggle(&A);
            else
                qd_rec_arm_live(&A);
        }
        break;
    case CC_UP:                                         /* F2. Shift: the pads an octave up (the core's own page: its window).
                                                         * CLIPS: the slots a row up (Plus / Minus) */
        if (clips_on()) { if (down) clip_rows(-1); }
        else if (!S.shift) btn(B_OCTUP, down);
        else if (down && S.mode == MV_CORE) { if (S.win_top > 0) S.win_top -= 28; }
        else if (down && S.oct < 4) S.oct++;
        break;
    case CC_DOWN:
        if (clips_on()) { if (down) clip_rows(1); }
        else if (!S.shift) btn(B_OCTDN, down);
        else if (down && S.mode == MV_CORE) { if (S.win_top < 56) S.win_top += 28; }
        else if (down && S.oct > -2) S.oct--;
        break;
    case CC_LEFT:                                       /* the page of 16 steps the buttons show. CLIPS: the slots 4 rows
                                                         * on (Left twice: back to the first) */
    case CC_RIGHT:
        if (clips_on()) {
            if (down && cc == CC_LEFT && S.left_ms && S.now - S.left_ms < DOUBLE_MS)
                clip_rows(-NSLOT), S.left_ms = 0;
            else if (down) {
                clip_rows(cc == CC_RIGHT ? 4 : -4);
                if (cc == CC_LEFT)
                    S.left_ms = S.now ? S.now : 1;
            }
            break;
        }
        if (down) {
            const int pg = A.ui.page[A.sel] + (cc == CC_RIGHT ? 1 : -1);
            char nm[16];
            qd_ctl_set_page(&A, pg);
            snprintf(nm, sizeof nm, "PAGE %d", (A.ui.page[A.sel] + 1) % 100);
            show_name(nm);
        }
        break;
    case CC_UNDO:                                       /* SHIFT + F1 undo, SHIFT + F2 redo */
        if (down)
            qd_shift_undo(&A, S.shift ? 1 : -1);
        break;
    default:
        if (cc >= CC_TRACK4 && cc <= CC_TRACK1) {       /* Sequence's modes: TRACKS, TRIG / SOUND, FX, TOOLS; Session's:
                                                         * CLIPS, MIDI FX, FX, MIX */
            if (S.session)
                sess_button(CC_TRACK1 - cc, down);
            else
                track_button(CC_TRACK1 - cc, down);
        }
        break;
    }
}

/* The power button is a SysEx from the surface, F0 00 21 1D 01 01 3A <bits> <battery> 00 F7: a status report, sent on
 * every change; bit 3 of <bits> is the button held. Every report must be answered with F0 00 21 1D 01 01 39 02 F7, as
 * Move's own software does, or the surface reports the button once and then goes quiet (movy-host's finding:
 * schwung-movy host/surface_keys.c, whose matcher this is) */
static void power_feed(const uint8_t p[4])
{
    static const int16_t WANT[4][4] = { { 0x04, 0xF0, 0x00, 0x21 }, { 0x04, 0x1D, 0x01, 0x01 }, { 0x04, 0x3A, -1, -1 },
                                        { 0x06, 0x00, 0xF7, -1 } };
    static const uint8_t ACK[3][4] = { { 0x04, 0xF0, 0x00, 0x21 }, { 0x04, 0x1D, 0x01, 0x01 }, { 0x07, 0x39, 0x02, 0xF7 } };
    int i;
    for (i = 0; i < 4; i++) {
        const int got = i == 0 ? p[0] & 0x0F : p[i];
        if (WANT[S.pw_stage][i] >= 0 && got != WANT[S.pw_stage][i]) {
            S.pw_stage = 0;
            return;
        }
    }
    if (S.pw_stage == 2)
        memcpy(S.pw_cmd, p + 1, 3);
    if (++S.pw_stage < 4)
        return;
    S.pw_stage = 0;
    for (i = 0; i < 3; i++)
        ring_push(&g_led, ACK[i]);
    if ((S.pw_cmd[1] & 0x08) && !(S.pw_status & 0x08)) {    /* the button's bit going up: a press */
        S.over = OV_POWER;
        S.menu_sel = 0;
    }
    S.pw_status = S.pw_cmd[1];
}

static void input(const uint8_t p[4])
{
    const int cable = p[0] >> 4, st = p[1] & 0xF0;
    if (cable == 0 && ((p[0] & 0x0F) == 0x04 || (p[0] & 0x0F) == 0x06 || S.pw_stage))
        power_feed(p);
    if (cable == 2) {                                   /* USB-A: MIDI in, as the FM-1's */
        if (p[1] >= 0x80)
            qd_app_midi(&A, p[1], p[2], p[3]);
        return;
    }
    if (cable != 0)
        return;
    if (st == 0x90) surface_note(p[2], p[3]);
    else if (st == 0x80) surface_note(p[2], 0);
    else if (st == 0xA0 && p[2] >= N_PAD0 && p[2] < N_PAD0 + 32)   /* a pad's pressure (MIX: the glide's speed) */
        mix_pressure(p[2] - N_PAD0, p[3]);
    else if (st == 0xD0) {                              /* (the whole surface's pressure: every column held) */
        int c;
        for (c = 0; c < 8; c++)
            if (S.mxg_row[c] >= 0)
                S.mxg_pres[c] = p[2];
    }
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
    if (u->latch == QD_LATCH_BROWSE)
        return MV_BROWSE;
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
    const int m = mode_now(), scr = qd_view_screen(&A), moved = m != S.mode || scr != S.scr;
    A.ui.shift_stick = 0;                               /* (no SHIFT of the core's is ever left on) */
    A.ui.ekeys = 0;                                     /* the step buttons are always the steps here: the pads are the keys */
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
    if (S.tb_down < 0) {                                /* the mode follows the core when it moves by itself (Back,
                                                         * a page closed): the track buttons light what is up */
        if (m == MV_HOME) S.seq_mode = SQ_TRACKS;
        else if (m == MV_SOUND) S.seq_mode = SQ_SOUND;
        else if (m == MV_FX) S.seq_mode = SQ_FX;
        else if (m == MV_CORE && scr == QD_SCR_TOOLS) S.seq_mode = SQ_TOOLS;
        if (S.session && moved) {                       /* (Session's too, as the core moves: Back from FX is CLIPS) */
            if (m == MV_HOME && S.sn_mode != SN_MIX && S.sn_mode < SN_SCENES) S.sn_mode = SN_CLIPS;
            else if (m == MV_FX) S.sn_mode = SN_FX;
            else if (m == MV_MIX) S.sn_mode = SN_MIX;
            else if (m == MV_CORE && scr == QD_SCR_MFX) S.sn_mode = SN_MFX;
        }
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

/* page mode's row 2: a page's 16 steps' trigs, pad j steps 2j and 2j + 1 (both: the track's colour; one: dim; none:
 * off; the play head's: white). The page: a page step held, else the one playing, else the one shown */
static int preview_page(void)
{
    const int pp = qd_ctl_playing_page(&A, A.sel);
    return S.pg_held >= 0 ? S.pg_held : pp >= 0 ? pp : A.ui.page[A.sel];
}
static uint8_t preview_lit(int j)
{
    const qd_pattern_t *P = qd_ctl_cpat(&A);
    const int t = A.sel, pg = preview_page(), s0 = pg * QD_PAGE + 2 * j, tc = t < 8 ? t : 7;
    const int ph = qd_seq_clip(&A.seq, t) == A.seq.cur && qd_seq_clip_on(&A.seq, t) ? qd_seq_track_step(&A.seq, t) : -1;
    int n = 0, k;
    if (t >= QD_TRACKS || pg >= trk_npage())
        return C_OFF;
    if (A.seq.playing && (ph == s0 || ph == s0 + 1))
        return C_WHITE;
    for (k = 0; k < 2; k++)
        n += qd_step_type(P, t, s0 + k) != QD_TRIG_NONE;
    return (uint8_t)(n == 2 ? TRK_COL[tc] : n ? TRK_DIM[tc] : C_OFF);
}

/* scene k's LED: running green (about to: blinking), else its colour (no clip in it: off) */
static uint8_t scene_led(int k, int blink)
{
    if (k == S.run && A.seq.playing)
        return A.seq.tick >= S.run_x ? C_GREEN : blink ? C_GREEN : C_DKGREEN;
    return scene_used(k) ? scene_col(k, 0) : C_OFF;
}

static void leds(void)
{
    qd_leds_t L;
    int i;
    const int blink = (int)((S.now / 250u) & 1u), clips = clips_on(), mix = mix_on(), scn = scn_on(), arr = arr_on(),
              sq = sq_on();
    qd_app_leds(&A, &L);
    for (i = 0; i < 16; i++) {
        if ((scn || arr || sq) && !S.shift) {          /* SCENES, ARRANGE, SEQ: the steps launch the scenes */
            led_note(N_STEP0 + i, scene_led(i, blink));
            continue;
        }
        if ((clips || mix) && !S.shift) {               /* CLIPS, MIX: under each track its stop (the track's colour while it
                                                         * plays, blinking while it stops; grey stopped) and its mute (red,
                                                         * soloed yellow) */
            const int t = i / 2;
            led_note(N_STEP0 + i, (uint8_t)(t >= QD_TRACKS ? C_OFF
                                            : i & 1 ? ((A.seq.solo >> t & 1u) ? C_YELLOW : (A.seq.mute >> t & 1u) ? C_RED : C_GREY)
                                            : !qd_seq_clip_on(&A.seq, t) ? C_GREY
                                            : qd_seq_clip_queued(&A.seq, t) == -2 ? (blink ? TRK_COL[t] : C_OFF) : TRK_COL[t]));
        } else if (page_mode() && !S.shift) {                  /* the pages: shown white, playing green, in the loop blue,
                                                         * out of it dark blue, past the end grey */
            const int pp = qd_ctl_playing_page(&A, A.sel);
            led_note(N_STEP0 + i, (uint8_t)(i == A.ui.page[A.sel] ? C_WHITE : i == pp ? C_GREEN
                                            : i >= trk_npage() ? C_GREY : (trk_loop() >> i & 1u) ? C_BLUE : C_DKBLUE));
        } else if (S.mode == MV_BROWSE)
            led_note(N_STEP0 + i, C_OFF);
        else if (S.shift)                               /* Shift: the steps that are shortcuts, the open one bright */
            led_note(N_STEP0 + i, (uint8_t)(!SHIFT_STEP[i] ? C_OFF
                                            : (SHIFT_STEP[i] == SS_SETUP && S.over == OV_SETUP) ||
                                              (SHIFT_STEP[i] == SS_TEMPO && S.over == OV_TEMPO) ? C_WHITE : C_LTBLUE));
        else
            led_note(N_STEP0 + i, shade(L.key[QD_WHITE_KEY[i]], C_GREY, C_LGREY, C_WHITE));
    }
    for (i = 0; i < 32; i++) {                          /* the note pads: struck white, a root green, the rest dim (the
                                                         * top row too while it is hidden) */
        if (S.over == OV_PROJECTS) {                    /* PROJECTS: each in its colour (none: light grey), the open one
                                                         * white, empty dark; Copy's taken one, Delete's armed blinking */
            const int p = pj_at(i), c = p < QD_NPROJ ? A.per.pcol[p] : 0;
            led_note(N_PAD0 + i, (uint8_t)(p >= QD_NPROJ || !A.per.pexists[p] ? C_OFF
                                           : p == S.pj_del ? (blink ? C_RED : C_OFF)
                                           : p == S.pj_take ? (blink ? C_WHITE : C_OFF)
                                           : p == A.per.proj ? C_WHITE : c >= 1 && c <= 8 ? TRK_COL[c - 1] : C_LGREY));
            continue;
        }
        if (clips) {                                    /* CLIPS: a clip in its track's colour, playing green, queued
                                                         * green blinking, stopping its colour blinking; the clip edited
                                                         * white (an empty slot chosen: grey) */
            const int t = i % 8, slot = S.srow + 3 - i / 8;
            uint8_t c = C_OFF;
            if (t < QD_TRACKS) {
                const int has = clip_has(slot, t), q = qd_seq_clip_queued(&A.seq, t);
                const int on = qd_seq_clip_on(&A.seq, t) && qd_seq_clip(&A.seq, t) == slot && has;
                if (q == slot)
                    c = blink ? C_GREEN : C_DKGREEN;
                else if (on && q == -2)
                    c = blink ? TRK_COL[t] : C_OFF;
                else if (on)
                    c = A.seq.playing ? C_GREEN : TRK_COL[t];
                else if (t == A.sel && edit_clip(t) == slot)
                    c = has ? C_WHITE : C_GREY;
                else
                    c = has ? TRK_DIM[t] : C_OFF;
            }
            led_note(N_PAD0 + i, c);
            continue;
        }
        if (sq) {                                       /* SEQ: the pads dark */
            led_note(N_PAD0 + i, C_OFF);
            continue;
        }
        if (scn) {                                      /* SCENES: the lower rows the scenes, the one edited white */
            const int k = i < 8 ? i + 8 : i - 8;
            led_note(N_PAD0 + i, (uint8_t)(i >= 16 ? C_OFF : k == S.scn ? C_WHITE : scene_led(k, blink)));
            continue;
        }
        if (arr) {                                      /* ARRANGE: the positions in their scene's colours, the one
                                                         * playing green, the end red; the page shown brighter */
            const int p = (3 - i / 8) * 8 + i % 8, end = song_end(), sh = song_shown_pos();
            const int k = A.proj.song[p][0] - 1, onpage = p / 8 == S.apage;
            led_note(N_PAD0 + i, (uint8_t)(p > end ? C_OFF : p == end ? (onpage ? C_RED : C_OFF)
                                           : p == sh && A.seq.playing ? C_GREEN : scene_col(k, !onpage)));
            continue;
        }
        if (mix) {                                      /* MIX: what is set (the chosen white), the tracks' columns in
                                                         * their colours up to the value */
            const int c = i % 8, row = i / 8;
            led_note(N_PAD0 + i, (uint8_t)(row == 3 && !S.mix_hide ? (c == (S.mix_k >= TS_WARP ? 7 : S.mix_k) ? C_WHITE
                                                       : c == 0 ? C_LGREY : c == 1 ? C_YELLOW : C_BLUE)
                                           : c >= QD_TRACKS ? C_OFF : (A.seq.mute >> c & 1u) ? (mix_lit(c, row) ? C_RED : C_OFF)
                                           : mix_lit(c, row) ? TRK_COL[c] : row == 0 ? TRK_DIM[c] : C_OFF));
            continue;
        }
        if (S.mode == MV_FX && S.perform && i < 24) {   /* PERFORM: each kind its colour, on white (latched: blinking) */
            static const uint8_t IDLE[3] = { C_DKGREEN, C_DKYELLOW, C_BLUE }, ON[3] = { C_GREEN, C_YELLOW, C_LTBLUE };
            const int row = i / 8, tape = row == 0 && i >= 4;
            const int win = row == 2 ? pf_on(i) : row == 1 ? pf_newest(8, 16) == i : tape ? pf_newest(4, 8) == i
                          : pf_newest(0, 4) == i;
            led_note(N_PAD0 + i, (uint8_t)(win ? (S.pf_latch[i] && !S.pf_held[i] && blink ? (tape ? C_RED : ON[row]) : C_WHITE)
                                           : pf_on(i) ? (tape ? C_RED : ON[row]) : tape ? TRK_DIM[0] : IDLE[row]));
            continue;
        }
        if (i >= 24 && top_row_on())
            continue;
        if (page_mode() && i >= 16 && i < 24) {         /* row 2: a page's trigs, two steps a pad */
            led_note(N_PAD0 + i, preview_lit(i - 16));
            continue;
        }
        if (page_mode() && i < 24) {                    /* rows 3-4: the page's steps, its start and end white, the
                                                         * range blue */
            const qd_tset_t *ts = &qd_ctl_cpat(&A)->trk[A.sel];
            const int pg = A.ui.page[A.sel], k = i >= 8 ? i - 8 : i + 8;
            led_note(N_PAD0 + i, (uint8_t)(k == ts->pstart[pg] || k == ts->pend[pg] ? C_WHITE
                                           : k > ts->pstart[pg] && k < ts->pend[pg] ? C_BLUE : C_GREY));
            continue;
        }
        led_note(N_PAD0 + i, (uint8_t)(S.mode == MV_IDLE ? C_OFF : S.pad_note[i] >= 0 ? C_WHITE
                                                                 : pad_is_root(i) ? C_GREEN : C_GREY));
    }
    for (i = 0; i < 8 && top_row_on() && !clips && !mix && !scn && !arr && !sq && S.over != OV_PROJECTS; i++) {
        uint8_t c;
        if (S.mute_down)                                /* Mute held: the tracks, a muted one red, a soloed one yellow */
            c = (uint8_t)(i >= QD_TRACKS ? C_OFF : (A.seq.solo >> i & 1u) ? C_YELLOW : (A.seq.mute >> i & 1u) ? C_RED
                          : A.sel == i ? C_WHITE : C_BLUE);
        else if (S.mode == MV_SOUND)                    /* the page pads (ENV, LFO light blue), RND PAGE, RND SOUND */
            c = (uint8_t)(i == 6 ? (S.rnd_pad == 6 ? C_WHITE : C_DKYELLOW) : i == 7 ? (S.rnd_pad == 7 ? C_WHITE : C_RED)
                          : snd_pad_of(S.spage) == i ? C_WHITE : i >= 4 ? C_LTBLUE : C_LGREY);
        else if (S.mode == MV_FX)                       /* the floors; pad 7 WARP / GLITCH / TV; pad 8 PERFORM */
            c = (uint8_t)(i == 7 ? (S.perform ? C_WHITE : C_RED) : S.fpage == i || (i == 6 && S.fpage >= 6) ? C_WHITE : C_BLUE);
        else if (S.mode == MV_CORE && S.scr == QD_SCR_MFX) {   /* MIDI FX: the slots in the track's colour (bypassed:
                                                         * dim; empty: off), the selected one white (bypassed: light
                                                         * grey; empty: grey) */
            const int g = qd_mfx_tgroup(&A), tc = A.sel < 8 ? A.sel : 7;
            const qd_mfx_slot_t *sl = g > 0 && i < QD_MFX_NSLOT ? &A.proj.mfx[g - 1].slot[i] : 0;
            const int has = sl && sl->type != QD_MFX_NONE;
            c = (uint8_t)(i == A.ui.mfx_slot ? (!has ? C_GREY : sl->bypass ? C_LGREY : C_WHITE) : !has ? C_OFF
                          : sl->bypass ? TRK_DIM[tc] : TRK_COL[tc]);
        } else if (S.mode == MV_HOME)                   /* the tracks: the selected one, a flash as each plays */
            c = (uint8_t)(i >= QD_TRACKS ? C_OFF : (A.seq.mute >> i & 1u) ? C_RED : A.sel == i ? C_WHITE
                          : qd_ctl_trig_flash(&A, i) ? C_LTBLUE : C_BLUE);
        else
            c = shade(L.key[QD_BLACK_KEY[i]], C_DKBLUE, C_BLUE, C_LTBLUE);
        led_note(N_PAD0 + 24 + i, c);
    }
    for (i = 0; i < 16; i++)                            /* the icons under the steps (white, CC 16-31): Shift held, the
                                                         * shortcuts there lit, the one open bright */
        led_cc(CC_STEP_ICON0 + i, (uint8_t)(!S.shift || !SHIFT_STEP[i] || S.mode == MV_IDLE ? 0
                                            : (SHIFT_STEP[i] == SS_SETUP && S.over == OV_SETUP) ||
                                              (SHIFT_STEP[i] == SS_TEMPO && S.over == OV_TEMPO) ? 124 : 64));
    led_cc(CC_SHIFT, (uint8_t)(S.shift ? 124 : 16));
    led_cc(CC_MENU, (uint8_t)(S.session ? 124 : white(L.button[B_HOME])));
    led_cc(CC_BACK, (uint8_t)(S.over || (S.mode != MV_HOME && S.mode != MV_IDLE) ? 64 : 6));
    led_cc(CC_CAPTURE, (uint8_t)(S.mode == MV_FX ? 124 : 16));
    led_cc(CC_MUTE, (uint8_t)(S.mode == MV_MIX ? 124 : 16));
    led_cc(CC_DOWN, clips ? (uint8_t)(S.srow < NSLOT - 4 ? 124 : 16) : white(L.button[B_OCTDN]));
    led_cc(CC_UP, clips ? (uint8_t)(S.srow > 0 ? 124 : 16) : white(L.button[B_OCTUP]));
    led_cc(CC_LOOP, (uint8_t)(page_mode() ? 124 : S.loop_down ? 64 : 16));
    led_cc(CC_COPY, 16);
    led_cc(CC_DELETE, white(L.key[QD_BLACK_KEY[10]]));
    led_cc(CC_UNDO, 16);
    {   /* Left / Right: bright on the side the playhead is, when it plays a page other than the one shown */
        const int pp = qd_ctl_playing_page(&A, A.sel), pg = A.ui.page[A.sel];
        led_cc(CC_LEFT, (uint8_t)(clips ? (S.srow > 0 ? 124 : 16) : pp >= 0 && pp < pg ? 124 : 16));
        led_cc(CC_RIGHT, (uint8_t)(clips ? (S.srow < NSLOT - 4 ? 124 : 16) : pp >= 0 && pp > pg ? 124 : 16));
    }
    led_cc(CC_PLAY, L.play_green ? C_GREEN : shade(L.button[B_PLAY], C_GREY, C_LGREY, C_WHITE));
    led_cc(CC_REC, A.ui.rec ? C_RED : C_OFF);
    led_cc(CC_SAMPLE, (uint8_t)(S.mode == MV_SOUND ? C_WHITE : C_GREY));
    for (i = 0; i < 4; i++)                             /* the track buttons: Sequence's modes, the one up white (light
                                                         * grey while its top row is hidden), the others dim; Session's:
                                                         * the one up white, the others dark green */
        led_cc(CC_TRACK1 - i, (uint8_t)(S.mode == MV_IDLE ? C_OFF
                                        : S.session ? (S.sn_mode == i ? C_WHITE : C_DKGREEN)
                                        : S.seq_mode != i ? C_GREY : top_row_on() ? C_WHITE : C_LGREY));
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
    S.tb_down = S.tpad = S.rnd_pad = S.pg_held = S.pg_copied = S.pd_held = S.cl_take = S.mfx_pad = S.mfx_dbl = -1;
    memset(S.tsel, -1, sizeof S.tsel);
    memset(S.mxg_row, -1, sizeof S.mxg_row);
    S.mod_hold = -1;
    S.run = -1;
    S.rng = 0x45474D31u;                                /* ("EGM1") */
    for (i = 0; i < 6; i++)
        S.grp_last[i] = SND_PAD[i][0];
    for (i = 0; i < NSND; i++)
        for (j = 0; j < SND[i].n; j++)
            g_snd_core[i][j] = (int8_t)snd_page_index(SND[i].e[j].page);
    qd_app_init(&A);
    qd_persist_attach(&A, flash_open(flash_path));
    A.ui.follow = 0;                                    /* the step buttons stay on the page you chose (the core follows
                                                         * the playhead by default: a step under a finger would change) */
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
    pf_process(stereo, frames);                         /* PERFORM's repeat, filter, tape: on the whole mix */
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
    clip_follow();
    scene_run();
    pf_sync();
    mix_glide();
    qd_app_tick_locked(&A, now_ms);
    if (S.rnd_pad >= 0 && !S.rnd_done && S.mode == MV_SOUND && now_ms - S.rnd_ms >= RND_MS) {   /* held a second */
        qd_snd_randomise(&A, S.rnd_pad == 6 ? g_snd_core[S.spage][0] : -1);
        show_name(S.rnd_pad == 6 ? "PAGE SHAKEN" : "SOUND SHAKEN");
        S.rnd_done = 1;
    }
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

/* The room (the core's ui_room.c, laid out again for this screen): the page's notes as platforms by pitch, a robot on
 * a step that holds locks, the agent running the notes as the playhead and leaping between them */
static void room_draw(mono_t *m, uint32_t now_ms)
{
    static ui_held_t h;
    static ui_lane_t l;
    struct { float at, len; int x, y, i; } st[18];
    scr_room_t r;
    int i, n = 0, k = -1;
    qd_view_edit(&A, &h, &l);
    memset(&r, 0, sizeof r);
    r.ntrack = 0;                                       /* (no lift: the track buttons and the top row show the tracks) */
    r.sel = A.sel;
    {   /* the page rail: the clip's pages up to the last one in its loop */
        const qd_tset_t *ts = &qd_ctl_cpat(&A)->trk[A.sel];
        int p;
        r.page_loop = ts->loop ? ts->loop : 1u;
        for (p = 0; p < QD_NPAGE; p++)
            if (r.page_loop >> p & 1u)
                r.npage = p + 1;
        r.page_shown = A.ui.page[A.sel];
        r.page_playing = qd_ctl_playing_page(&A, A.sel);
    }
    r.mute = l.trk_mute;
    for (i = 0; i < QD_TRACKS; i++)
        if (l.trk_flash[i] >= 0.f && l.trk_flash[i] < 0.09f)
            r.flash |= (uint8_t)(1u << i);
    st[n].at = -0.01f, st[n].len = 0.f, st[n].x = 4, st[n].y = SCR_RM_FLOOR, st[n].i = -1, n++;   /* the door in */
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &l.col[i];
        const int noted = c->in_range && c->on && c->type != 2 && c->nnote > 0;
        r.col[i].held = c->held;
        r.col[i].guard = (uint8_t)(c->in_range && c->on && (c->nplock || c->type == 2));
        if (noted) {
            const int span = l.span > 0 ? l.span : 16;
            int top = c->note[0], j, y, w;
            for (j = 1; j < c->nnote && j < 6; j++)
                top = c->note[j] > top ? c->note[j] : top;
            y = SCR_RM_BOT - (top - l.lo) * (SCR_RM_BOT - SCR_RM_TOP) / span;
            y = y < SCR_RM_TOP ? SCR_RM_TOP : y > SCR_RM_BOT ? SCR_RM_BOT : y;
            w = c->inf ? 127 - SCR_RM_X(i) : (int)(c->len * 7.f);
            w = w < 5 ? 5 : w > 127 - SCR_RM_X(i) ? 127 - SCR_RM_X(i) : w;
            r.col[i].on = 1, r.col[i].y = (uint8_t)y, r.col[i].w = (uint8_t)w;
            st[n].at = (float)i + (float)c->micro / 24.f;
            st[n].len = c->len;
            st[n].x = SCR_RM_X(i) + 2 + c->micro * 7 / 24;
            st[n].y = y;
            st[n].i = i;
            n++;
        }
    }
    st[n].at = 16.f, st[n].len = 0.f, st[n].x = 125, st[n].y = SCR_RM_FLOOR, st[n].i = -1, n++;   /* .. and out */
    r.agent_x = -1;
    if (l.pos >= 0.f) {
        const float pos = l.pos;
        if (n == 2) {                                   /* an empty room: he runs the floor */
            r.agent_x = 14 + (int)(pos * 7.f);
            r.agent_y = SCR_RM_FLOOR;
            r.agent_pose = (int)(pos * 4.f) & 1;
        } else {
            float hold, t0;
            for (i = 0; i < n; i++)
                if (st[i].at <= pos)
                    k = i;
            k = k < 0 ? 0 : k > n - 2 ? n - 2 : k;
            hold = st[k].len < st[k + 1].at - st[k].at - 0.5f ? st[k].len : st[k + 1].at - st[k].at - 0.5f;
            if (hold < 0.f || st[k].i < 0)
                hold = 0.f;
            t0 = st[k].at + hold;
            if (pos < t0) {                             /* on the platform: running its gate */
                r.agent_x = st[k].x + (int)((pos - st[k].at) * 7.f);
                r.agent_y = st[k].y;
                r.agent_pose = (int)(pos * 4.f) & 1;
            } else {                                    /* the leap to the next */
                const float gap = st[k + 1].at - t0 > 0.05f ? st[k + 1].at - t0 : 0.05f;
                const float u = (pos - t0) / gap > 1.f ? 1.f : (pos - t0) / gap;
                const float x0 = (float)st[k].x + hold * 7.f;
                float J = 3.f + 2.f * gap;
                J = J > 9.f ? 9.f : J;
                r.agent_x = (int)(x0 + ((float)st[k + 1].x - x0) * u + 0.5f);
                r.agent_y = (int)((float)st[k].y + (float)(st[k + 1].y - st[k].y) * u - J * 4.f * u * (1.f - u) + 0.5f);
                r.agent_pose = 2;
            }
        }
    } else if (!l.playing) {                            /* stopped: he waits at the door */
        r.agent_x = 5, r.agent_y = SCR_RM_FLOOR, r.agent_pose = 0;
    }
    scr_room(m, &r, now_ms);
}

static void page_draw(mono_t *m, uint32_t now_ms)
{
    if (S.over == OV_PROJECTS) {                        /* PROJECTS: the open one */
        int k, used = 0;
        for (k = 0; k < QD_NPROJ; k++)
            used += A.per.pexists[k] != 0;
        scr_projects(m, A.per.proj + 1, QD_NPROJ, A.per.pname[A.per.proj], A.per.pcol[A.per.proj], used);
        return;
    }
    if (S.belt && S.session && S.mode == MV_HOME && !S.over) {   /* the belt: the pages, the chosen one large */
        const uint32_t dt = now_ms - S.belt_ms;
        scr_belt(m, SN_NAME, SN_N, S.belt_sel, S.belt_from, S.belt_ms && dt < BELT_MS ? (int)(dt * 1000u / BELT_MS) : -1);
        return;
    }
    if (scn_on()) {                                     /* SCENES: the edited scene's eight */
        static char val[8][16];
        const char *lab[8], *vp[8];
        int frac[8], i;
        for (i = 0; i < 8; i++) {
            scn_val(i, val[i], sizeof val[i], &frac[i], 1);
            lab[i] = SCN_LAB[i], vp[i] = val[i];
        }
        scr_eight(m, lab, vp, frac, 8);
        return;
    }
    if (S.mode == MV_CORE && S.scr == QD_SCR_MFX) {     /* MIDI FX: the Move's own picture of the core's page */
        static ui_mfx_t v;
        memset(&v, 0, sizeof v);
        qd_view_mfx(&A, &v);
        scr_mfx(m, v.g, v.track ? v.track : A.sel + 1, v.type, v.byp, v.sel, v.page, v.npages, v.lab,
                (const char (*)[12])v.val, v.has);
        return;
    }
    if (sq_on()) {                                      /* SEQ: the subpage's settings */
        static char val[8][16];
        const char *lab[8], *vp[8];
        int frac[8], i;
        for (i = 0; i < 8; i++) {
            sq_val(i, val[i], sizeof val[i], &frac[i]);
            lab[i] = SQP_LAB[S.seqp][i], vp[i] = val[i];
        }
        scr_eight(m, lab, vp, frac, 8);
        return;
    }
    if (arr_on()) {                                     /* ARRANGE: the song, the page of it shown */
        int sc[8], rp[8], i, play = -1, prog = -1;
        const int first = S.apage * 8, shown = song_shown_pos(), end = song_end();
        for (i = 0; i < 8; i++)
            sc[i] = A.proj.song[first + i][0], rp[i] = A.proj.song[first + i][1];
        if (S.song_on || (shown >= 0 && S.run >= 0 && A.seq.playing)) {
            const int64_t tot = song_ticks(0, end);
            if (shown >= first && shown < first + 8)
                play = shown - first;
            if (tot > 0 && S.song_on)
                prog = (int)((A.seq.tick - S.song_t0) * 1000 / tot);
        }
        scr_song(m, first, sc, rp, play, prog);
        return;
    }
    if (mix_on()) {                                     /* MIX: the tracks' values of what is set */
        ui_mix_t mv;
        int v[8], mute[8], meter[8], t;
        memset(&mv, 0, sizeof mv);
        qd_view_mix(&A, &mv);                           /* (the meters: each track's voices' level now) */
        for (t = 0; t < 8; t++) {
            v[t] = t < QD_TRACKS ? mix_val(t, S.mix_k) : 0;
            mute[t] = t < QD_TRACKS && (A.seq.mute >> t & 1u);
            meter[t] = t < QD_TRACKS ? (int)(mv.tr[t].meter * 1000.f) : 0;
        }
        scr_mix_cols(m, TS_NAME[S.mix_k], v, mute, meter, QD_TRACKS < 8 ? QD_TRACKS : 8, A.sel, S.mix_k == TS_PAN);
        return;
    }
    if (clips_on()) {                                   /* CLIPS: the slots shown, the selected clip named */
        uint8_t cell[4][8];
        char title[48];
        int r, t, ed = edit_clip(A.sel);
        memset(cell, 0, sizeof cell);
        for (r = 0; r < 4; r++)
            for (t = 0; t < QD_TRACKS && t < 8; t++) {
                const int slot = S.srow + r, q = qd_seq_clip_queued(&A.seq, t);
                const int has = clip_has(slot, t), on = qd_seq_clip_on(&A.seq, t) && qd_seq_clip(&A.seq, t) == slot;
                cell[r][t] = (uint8_t)(q == slot ? SCR_CL_QUEUED : on && has && q == -2 ? SCR_CL_STOPPING
                                       : on && has ? SCR_CL_PLAYING : has ? SCR_CL_CLIP : SCR_CL_EMPTY);
            }
        snprintf(title, sizeof title, "%d %s  CLIP %d", A.sel + 1, A.model.t[A.sel].name, ed + 1);
        scr_session(m, cell, QD_TRACKS < 8 ? QD_TRACKS : 8, S.srow, A.sel, ed - S.srow, title, now_ms);
        return;
    }
    if (page_mode()) {                                  /* the clip's pages */
        char v[20];
        snprintf(v, sizeof v, "PAGE %d", A.ui.page[A.sel] + 1);
        scr_pages(m, "LOOP", v, trk_npage(), trk_loop(), A.ui.page[A.sel], qd_ctl_playing_page(&A, A.sel), now_ms);
        return;
    }
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
        if (v.draw == UI_SD_LFO) {                      /* its shape is on the page beside it (the WAVE's name) */
            ui_snd_t w;
            const char *n;
            const int dep = snd_frac(page, 2, v.v[2]);
            snd_view(g_snd_core[S.spage][4], &w);
            n = w.txt[0];
            scr_lfo(m, !strncmp(n, "TRI", 3) ? 1 : !strncmp(n, "SAW", 3) || !strncmp(n, "RAMP", 4) ? 2
                       : !strncmp(n, "SQ", 2) || !strncmp(n, "PUL", 3) ? 3
                       : !strncmp(n, "RND", 3) || !strncmp(n, "RAN", 3) || !strncmp(n, "S&H", 3) || !strncmp(n, "NOI", 3) ? 4 : 0,
                    snd_frac(page, 0, v.v[0]), dep < 0 ? 500 : dep > 500 ? (dep - 500) * 2 : (500 - dep) * 2, now_ms);
            return;
        }
        if ((v.draw == UI_SD_WORD || v.draw == UI_SD_SAY || v.draw == UI_SD_DIGI) && v.wave && v.wave_n) {
            scr_word(m, v.wave, v.wave_n, v.draw == UI_SD_WORD ? snd_frac(page, 2, v.v[2]) : -1,
                     v.draw == UI_SD_WORD ? snd_frac(page, 3, v.v[3]) : -1);
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
        static const uint8_t KIND[9] = { SCR_FX_SENDS, SCR_FX_CHORUS, SCR_FX_ECHO, SCR_FX_ROOM, SCR_FX_CRUSH, SCR_FX_ROBOT,
                                         SCR_FX_WARP, SCR_FX_GLITCH, SCR_FX_TV };
        kinfo_t ki;
        int val[8], i;
        if (S.perform && S.pf_last >= 0 && pf_on(S.pf_last)) {   /* PERFORM: a throw its floor's picture, the rest
                                                         * their names */
            if (S.pf_last >= 16) {
                for (i = 0; i < 8; i++)
                    val[i] = -1;
                scr_fx(m, KIND[S.pf_last - 15], val, now_ms);
            } else
                scr_page_name(m, PF_NAME[S.pf_last]);
            return;
        }
        for (i = 0; i < 8; i++) {
            knob_info(i, &ki);
            val[i] = ki.ok ? ki.frac : -1;
        }
        scr_fx(m, KIND[S.fpage], val, now_ms);
    } else if (S.mode == MV_BROWSE) {                   /* the sound that would be taken, its place in the list */
        ui_browse_t v;
        char head[17];
        memset(&v, 0, sizeof v);
        qd_view_browse(&A, &v);
        snprintf(head, sizeof head, "%s %d/%d", v.title && !strcmp(v.title, "PRESETS") ? "SOUND" : v.title ? v.title : "", v.pos, v.n);
        scr_knob(m, head, v.e[2].ok && v.e[2].name ? v.e[2].name : "NONE", v.n > 1 ? (v.pos - 1) * 1000 / (v.n - 1) : -1);
    } else if (mix_native()) {
        ui_mix_t v;
        int level[QD_TRACKS], mute[QD_TRACKS], meter[QD_TRACKS], i;
        memset(&v, 0, sizeof v);
        qd_view_mix(&A, &v);
        for (i = 0; i < QD_TRACKS; i++)
            level[i] = v.tr[i].level, mute[i] = v.tr[i].mute, meter[i] = (int)(v.tr[i].meter * 1000.f);
        scr_mix(m, level, mute, meter, QD_TRACKS, A.sel);
    } else {
        if (S.mode == MV_HOME && A.ui.nheld) {          /* a step held: which, and the notes in it */
            const qd_pattern_t *P = qd_ctl_cpat(&A);
            char nm[16], notes[16] = "";
            qd_rstep_t r;
            int k;
            qd_step_resolve(P, A.sel, A.ui.held[0], &r);
            snprintf(nm, sizeof nm, A.ui.nheld > 1 ? "STEP %d +%d" : "STEP %d", A.ui.held[0] % QD_PAGE + 1, A.ui.nheld - 1);
            for (k = 0; k < r.nn && k < 3; k++) {
                char one[6];
                qd_note_name(r.notes[k], one);
                snprintf(notes + strlen(notes), sizeof notes - strlen(notes), k ? " %s" : "%s", one);
            }
            scr_knob(m, nm, r.type == QD_TRIG_NONE ? "EMPTY" : notes, -1);
            return;
        }
        if (S.mode == MV_HOME && S.scr == QD_SCR_EDIT) {
            room_draw(m, now_ms);
            return;
        }
        core_frame(now_ms);
        if (S.mode == MV_MIX)
            scr_core_crop(m, g_lcd, 0, 14);
        else
            scr_core_crop(m, g_lcd, 0, S.win_top);
    }
}

int egm_frame(uint8_t packed[MONO_PACKED], uint32_t now_ms)
{
    const int pages = S.mode == MV_SOUND || S.mode == MV_FX || mix_native();
    const int turned = S.turn_ms && now_ms - S.turn_ms < TURN_MS;
    const int kn = turned && S.last_knob >= KI_MICRO ? S.last_knob : S.touch >= 0 && S.touch < 8 ? S.touch
                 : turned ? S.last_knob : -1;
    mono_t m;
    kinfo_t ki;
    int changed, done = 0;
    if (S.mode == MV_IDLE && !S.over) {
        scr_intro(&m, now_ms);
        done = 1;
    } else if (S.over == OV_SETUP) {
        scr_menu(&m, SETUP_ITEMS, SETUP_N, S.menu_sel);
        done = 1;
    } else if (S.over == OV_POWER) {
        scr_menu(&m, POWER_ITEMS, 2, S.menu_sel);
        done = 1;
    } else if (S.rnd_pad >= 0 && !S.rnd_done && S.mode == MV_SOUND) {   /* RND held: the brain laughs, the bar fills */
        scr_randomize(&m, now_ms, (int)((now_ms - S.rnd_ms) * 100u / RND_MS));
        done = 1;
    } else if (S.loop_down && S.mode != MV_IDLE) {      /* Shift + Loop held: the pattern playing, the one queued */
        char v[12];
        const int cur = qd_seq_clip(&A.seq, A.sel), nx = qd_seq_clip_queued(&A.seq, A.sel);   /* (clips: the
                                                         * selected track's, a pattern a scene) */
        if (nx >= 0 && nx != cur)
            snprintf(v, sizeof v, "%c%02d>%c%02d", 'A' + cur / 16, cur % 16 + 1, 'A' + nx / 16, nx % 16 + 1);
        else
            snprintf(v, sizeof v, "%c%02d", 'A' + cur / 16, cur % 16 + 1);
        scr_knob(&m, "PATTERN", v, -1);
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
    } else if (S.jog_touch && pages) {                  /* a finger on the wheel, knobs turned or not: every knob at once */
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
    } else if (kn >= 0 && !S.jog_touch) {               /* (the wheel touched: the page's own view, no pop-up) */
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

void egm_describe(char *out, int cap)
{
    snprintf(out, (size_t)cap, "mode=%d over=%d track=%d screen=%d playing=%d bpm=%d pat=%d nheld=%d rec=%d spage=%d fpage=%d "
             "mute=%x solo=%x page=%d shift=%d sq=%s top=%s", S.mode, S.over, A.sel + 1, qd_view_screen(&A), A.seq.playing,
             qd_seq_bpm(&A.seq), A.seq.cur, A.ui.nheld, A.ui.rec, S.spage, S.fpage, A.seq.mute, A.seq.solo,
             A.ui.page[A.sel] + 1, S.shift, SQ_NAME[S.seq_mode], top_row_on() ? "on" : "hidden");
    snprintf(out + strlen(out), (size_t)cap - strlen(out), " bar=%d", (int)(A.seq.tick / (16 * (int64_t)QD_TPS)) + 1);
    {   /* PROJECTS: the open one, its name and colour; those used (bit p) */
        uint32_t used = 0;
        int k;
        for (k = 0; k < QD_NPROJ; k++)
            used |= (uint32_t)(A.per.pexists[k] != 0) << k;
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " proj=%d pname=%s pcol=%d projs=%x", A.per.proj + 1,
                 A.per.pname[A.per.proj][0] ? A.per.pname[A.per.proj] : "-", A.per.pcol[A.per.proj], used);
    }
    snprintf(out + strlen(out), (size_t)cap - strlen(out), " pagemode=%d loopmask=%x npage=%d level=%d", page_mode(),
             trk_loop(), trk_npage(), A.model.t[A.sel].mix.level);
    {   /* Session: the mode, the rows, each track's clip (x: stopped; >n queued, >x stopping) */
        int t;
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " session=%d sn=%s srow=%d launch=%d empty=%s edit=%d clips=",
                 S.session, SN_NAME[S.sn_mode], S.srow + 1, A.proj.launch, A.proj.empty_keep ? "keep" : "stop",
                 A.seq.cur + 1);
        for (t = 0; t < QD_TRACKS; t++) {
            const int q = qd_seq_clip_queued(&A.seq, t);
            snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%s%d", t ? "," : "", qd_seq_clip_on(&A.seq, t) ? "" : "x",
                     qd_seq_clip(&A.seq, t) + 1);
            if (q >= 0)
                snprintf(out + strlen(out), (size_t)cap - strlen(out), ">%d", q + 1);
            else if (q == -2)
                snprintf(out + strlen(out), (size_t)cap - strlen(out), ">x");
        }
        {   /* the selected track's MIDI FX group, its slot selected and page, each slot's effect (b: bypassed) */
            const int g = qd_mfx_tgroup(&A);
            int k;
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " mfxg=%d mfxs=%d mfxp=%d mfx=", g, A.ui.mfx_slot + 1,
                     A.ui.mfx_page + 1);
            for (k = 0; k < QD_MFX_NSLOT && g; k++)
                snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%d%s", k ? "," : "",
                         A.proj.mfx[g - 1].slot[k].type, A.proj.mfx[g - 1].slot[k].bypass ? "b" : "");
        }
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " row2=");   /* row 2's colours as sent */
        for (t = 0; t < 8; t++)
            snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%d", t ? "," : "", S.led_note[N_PAD0 + 16 + t]);
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " toprow=");   /* the top row's colours as sent */
        for (t = 0; t < 8; t++)
            snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%d", t ? "," : "", S.led_note[N_PAD0 + 24 + t]);
        {   /* SCENES: the scene edited, its settings; the scene running; the song (each position: scene x repeats) */
            const qd_scene_t *c = &A.proj.scene[S.scn];
            int p;
            snprintf(out + strlen(out), (size_t)cap - strlen(out),
                     " belt=%s scn=%d set=%d,%s,%d,%d,%d,%d,%d,%d run=%d song=%s:%d pos=", S.belt ? SN_NAME[S.belt_sel] : "-",
                     S.scn + 1, c->bpm, SCN_FOLLOW[c->follow % QD_NFOLLOW], c->after, c->chance, c->repeats,
                     c->launch == QD_SCENE_LAUNCH_PROJ ? -1 : c->launch, c->name, c->colour, S.run + 1, S.song_on ? "on" : "off",
                     song_shown_pos() + 1);
            for (p = 0; p < song_end(); p++)
                snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%dx%d", p ? "," : "", A.proj.song[p][0],
                         A.proj.song[p][1]);
        }
        snprintf(out + strlen(out), (size_t)cap - strlen(out),   /* SEQ: its subpage; swing, metronome, click, record
                                                         * mode, quantize, take, automation, clock out, CC out */
                 " seqp=%s seqv=%d,%d,%d,%d,%d,%d,%d,%d,%d", SQP_NAME[S.seqp], qd_proj_cpat(&A.proj, A.seq.cur)->swing, A.metro,
                 A.metro_lvl, A.proj.rec_mode, A.proj.rec_quant, A.proj.rec_dub, A.proj.rec_auto, A.ui.clk_out, A.ui.cc_out);
        {   /* the selected track's sound: its modulation slots (source:destination:depth) and LFOs (destination:depth) */
            const int16_t *pp = A.model.t[A.sel].patch;
            int k, first = 1;
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " mods=");
            for (k = 0; k < QD_NMOD; k++)
                if (pp[QD_QP_MOD(k, QD_QM_SRC)]) {
                    snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%d:%d:%d", first ? "" : ",",
                             pp[QD_QP_MOD(k, QD_QM_SRC)], pp[QD_QP_MOD(k, QD_QM_DST)], pp[QD_QP_MOD(k, QD_QM_DEPTH)]);
                    first = 0;
                }
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " lfos=%d:%d,%d:%d,%d:%d", pp[QD_QP_LFO(0, QD_QL_DEST)],
                     pp[QD_QP_LFO(0, QD_QL_DEPTH)] - 64, pp[QD_QP_LFO(1, QD_QL_DEST)], pp[QD_QP_LFO(1, QD_QL_DEPTH)] - 64,
                     pp[QD_QP_LFO(2, QD_QL_DEST)], pp[QD_QP_LFO(2, QD_QL_DEPTH)] - 64);
        }
        {   /* PERFORM: on, the throws (track 1's, the selected's), the audio side's source (0 live, 1 repeat, 2 tape)
             * and its filter's cutoffs */
            int src, lp, hp;
            pf_state(&src, &lp, &hp);
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " perform=%d thr=%x,%x pf=%d,%d,%d", S.perform, A.thr[0],
                     A.thr[A.sel], src, lp, hp);
        }
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " mixk=%s mixrows=%d mix=", TS_NAME[S.mix_k], mix_rows());
        for (t = 0; t < QD_TRACKS; t++)
            snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%d", t ? "," : "", mix_val(t, S.mix_k));
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " slots=");   /* each track's slots holding a clip */
        for (t = 0; t < QD_TRACKS; t++) {
            unsigned m = 0;
            int k;
            for (k = 0; k < NSLOT; k++)
                m |= (unsigned)clip_has(k, t) << k;
            snprintf(out + strlen(out), (size_t)cap - strlen(out), "%s%x", t ? "," : "", m);
        }
    }
    {   /* the page shown: its trigs, and those with notes */
        static ui_held_t h;
        static ui_lane_t l;
        unsigned on = 0, noted = 0;
        int i;
        qd_view_edit(&A, &h, &l);
        for (i = 0; i < 16; i++) {
            on |= (unsigned)(l.col[i].on != 0) << i;
            noted |= (unsigned)(l.col[i].on && l.col[i].nnote > 0 && l.col[i].in_range && l.col[i].type != 2) << i;
        }
        {
            const qd_pattern_t *P = qd_ctl_cpat(&A);
            int n = 0;
            for (i = 0; i < QD_NSTEP; i++)
                n += qd_step_is_on(qd_step(P, A.sel, i)) != 0;
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " steps=%d", n);
        }
        snprintf(out + strlen(out), (size_t)cap - strlen(out), " trigs=%04x noted=%04x lo=%d span=%d", on, noted, l.lo, l.span);
        {   /* for a QA pass: every track's steps, the selected track's sound, key and loop, what the core is showing */
            const qd_pattern_t *P = qd_ctl_cpat(&A);
            int t, sc, rt;
            qd_track_key(&A.proj, P, A.sel, &sc, &rt);
            snprintf(out + strlen(out), (size_t)cap - strlen(out), " sound=\"%s\" scale=%s root=%d len=%d latch=%d view=%d dirty=%d all=",
                     A.model.t[A.sel].name, qd_scale_short(sc), rt, qd_track_len(&P->trk[A.sel]), A.ui.latch, A.ui.view,
                     qd_persist_dirty(&A));
            for (t = 0; t < QD_TRACKS; t++) {
                int n = 0;
                for (i = 0; i < QD_NSTEP; i++)
                    n += qd_step_is_on(qd_step(P, t, i)) != 0;
                snprintf(out + strlen(out), (size_t)cap - strlen(out), t ? ",%d" : "%d", n);
            }
            if (A.ui.cf_on && A.ui.cf_big)
                snprintf(out + strlen(out), (size_t)cap - strlen(out), " says=\"%s\"", A.ui.cf_big);
        }
    }
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
