/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the mixer and the effects (FIZZ's ui_tracks.c, redrawn on the 120 x 120 grid; the models are FIZZ's).
 *   MIX     six lift shafts: the platform the track's LEVEL, the pan a dial on top, the meter climbing beside it, the
 *           ECHO and ROOM sends two little bars under it; the selected track in the knobs' colours; a muted one
 *           crossed, a soloed one's number yellow. SENDS (ENV again): CHORUS, ECHO, ROOM as three bars a track
 *   EFFECTS the retro rooms (qd_fx.h), a drawing each, KNOB 1..4 along the foot:
 *           SENDS   the Evil Genius's machine, the track's sends on its parts (fx_sends)
 *           CHORUS  two waves swaying apart       ECHO / 2  bouncing balls, the repeats darkening (LOFI)
 *           ROOM / 2  the room as big as SIZE     CRUSH  the staircase   ROBOT  the antenna sparking
 *           WARP    the comb's teeth             GLITCH  a slice caught and repeated   TV  the set, its scanlines
 * A held step (or the selection) frames the screen in purple: the knobs lock on it. */
#include <string.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"

static const qd_rgb FK[4] = {UI_K1, UI_K2, UI_K3, UI_K4};

static void panel_head(const char *title, int npg, int ipg, int nsel)
{
    const int x = px_text(4, 4, title, UI_K3, PX_FS, 1);
    int k;
    for (k = 0; k < npg && npg > 1; k++)
        px_rect(x + 3 + k * 4, 6, 2, 2, k == ipg ? UI_K3 : UI_G2);
    if (nsel > 0) {
        char n[12];
        ui_itoa(n, nsel);
        px_rect(104, 2, 12, 9, UI_STEP_FRAME);
        px_ctext(110, 4, n, UI_BG, PX_FS, 1);
    }
    px_dots(0, 12, 120, UI_G3, 2);
}

static void step_frame(int on)
{
    if (on) {
        px_box(0, 0, 120, 120, UI_STEP_FRAME);
        px_box(1, 1, 118, 118, UI_STEP_FRAME);
    }
}

/* ---------------------------------------------------------------- the mix: lift shafts */
/* FX MIX: the selected track's sends into the five rooms as five lifts, the car as high as the send (how much of
 * the track goes through that room); 0: none, its shaft dark. KNOB 1..4 and PRESETS (the pink P) */
static void mix_rooms(const float *room, const char txt[5][8], const uint8_t *lk, int last, int x0, int pitch)
{
    static const char *const NM[5] = {"CRUSH", "ROBOT", "WARP", "GLITCH", "TV"};
    static const char *const KB[5] = {"1", "2", "3", "4", "P"};
    int k;
    for (k = 0; k < 5; k++) {
        const int x = x0 + k * pitch, top = 26, bot = 86;
        const qd_rgb kc = k < 4 ? FK[k] : UI_KALG;
        const int on = room[k] > 0.f, fy = bot - (int)((float)(bot - top) * room[k]);
        px_rect(x - 3, 15, 7, 7, kc);                   /* the knob's badge */
        px_ctext(x + 1, 16, KB[k], UI_BG, PX_FS, 1);
        px_vline(x - 5, top, bot - top + 3, on ? UI_G1 : UI_G3);   /* the shaft */
        px_vline(x + 5, top, bot - top + 3, on ? UI_G1 : UI_G3);
        if (on)
            px_checker(x - 4, fy + 3, 9, bot - fy, kc, 0);
        px_rect(x - 4, fy, 9, 2, on ? kc : UI_G2);      /* the car */
        px_rect(x - 4, fy + 2, 9, 1, on ? PX_ORANGE : UI_G3);
        px_hline(x - 7, bot + 3, 15, UI_G2);
        px_ctext(x + 1, 93, NM[k], last == k ? kc : UI_G1, PX_FS, 1);
        if (last == k)
            px_hline(x - 8, 99, 17, kc);
if (lk[k] == 1)                                 /* a lock: a chip (filled here, outlined somewhere) */
            px_rect(x - 10, 102, 21, 11, kc);
        else if (lk[k] == 2)
            px_box(x - 10, 102, 21, 11, kc);
        px_ctext(x + 1, 105, on || lk[k] == 1 ? txt[k] : "OFF", lk[k] == 1 ? UI_BG : on ? kc : UI_G2, PX_FS, 1);
    }
}

void ui_mix_draw(const ui_mix_t *v)
{
    static const char *const PG[3] = {"MIX", "SENDS", "FX MIX"};
    int i, k;
    panel_head(PG[v->page % 3], 3, v->page % 3, v->nsel);
    if (v->page == 2) {
        char n[2] = {(char)('1' + v->sel), 0};
        px_rect(94, 2, 8, 8, UI_K4);                    /* (the track these sends are) */
        px_ctext(98, 3, n, UI_BG, PX_FS, 1);
        mix_rooms(v->room, v->room_txt, v->room_lk, v->last, 14, 23);
        step_frame(v->step);
        return;
    }
    for (i = 0; i < UI_NTRACK; i++) {
        const int x = 10 + i * 20, sel = i == v->sel, mute = v->tr[i].mute == 1, solo = v->tr[i].mute == 2;
        const qd_rgb col = mute ? UI_G3 : sel ? UI_K3 : UI_G1;
        char n[4] = {(char)('1' + i), 0};
        if (ui_track_kind(i + 1) >= 0)
            ui_kind_icon(ui_track_kind(i + 1), (float)(2 * x), 34.f, 1.f, mute ? UI_G3 : UI_PK_COL[ui_track_kind(i + 1)]);
        if (!v->page) {
            const int top = 30, bot = 88, fy = bot - (bot - top) * v->tr[i].level / 127;
            const float a = -UI_PI * 0.5f + (float)v->tr[i].pan / 64.f * 2.2f;
            px_circle(x, 24, 3, sel ? UI_K2 : col, QD_NONE);   /* PAN */
            px_line(x, 24, x + (int)(fz_cosf(a) * 3.f + 0.5f), 24 + (int)(fz_sinf(a) * 3.f + 0.5f), sel ? UI_K2 : col);
            px_vline(x - 3, top, bot - top, UI_G3);     /* the shaft */
            px_vline(x + 3, top, bot - top, UI_G3);
            if (!mute && v->tr[i].meter > 0.f) {         /* the meter, beside it */
                const int h = (int)((float)(bot - top) * (v->tr[i].meter > 1.f ? 1.f : v->tr[i].meter));
                px_rect(x + 5, bot - h, 2, h, h > (bot - top) * 9 / 10 ? PX_LRED : UI_G2);
            }
            px_rect(x - 5, fy - 1, 11, 3, mute ? UI_G3 : sel ? UI_K1 : PX_YELLOW);   /* the platform: LEVEL */
            px_rect(x - 5, fy + 2, 11, 1, mute ? UI_G3 : PX_ORANGE);
            if (sel && v->locked[0])
                px_box(x - 6, fy - 2, 13, 5, FK[0]);
            for (k = 0; k < 2; k++) {                   /* the sends: ECHO, ROOM */
                const int y = 93 + k * 3, f = (k ? v->tr[i].reverb : v->tr[i].delay) * 14 / 127;
                px_hline(x - 7, y, 15, UI_G3);
                if (f > 0)
                    px_hline(x - 7, y, f, mute ? UI_G3 : sel ? FK[2 + k] : UI_G2);
                if (sel && v->locked[2 + k])
                    px_rect(x + 8, y - 1, 2, 3, FK[2 + k]);
            }
        } else {
            for (k = 0; k < 4; k++) {                   /* CHORUS, ECHO, ROOM, DRY: four bars */
                const int bx = x - 8 + k * 4, top = 24, bot = 88;
                const int val = k == 0 ? v->tr[i].chorus : k == 1 ? v->tr[i].delay : k == 2 ? v->tr[i].reverb : v->tr[i].dry;
                const int h = (bot - top) * val / 127;
                px_box(bx, top, 3, bot - top, UI_G3);
                if (h > 0)
                    px_rect(bx, bot - h, 3, h, mute ? UI_G3 : sel ? FK[k] : k == 3 ? UI_G2 : UI_G1);
                if (sel && k < 3 && v->locked[k])
                    px_box(bx - 1, bot - h - 1, 5, 3, FK[k]);
            }
        }
        if (mute) {
            px_line(x - 3, 101, x + 3, 107, UI_K4);
            px_line(x + 3, 101, x - 3, 107, UI_K4);
        } else {
            px_ctext(x + 1, 101, n, solo ? UI_KSEL : sel ? UI_K3 : UI_G1, PX_FS, 1);
        }
        if (sel || solo)
            px_box(x - 5, 99, 11, 10, solo ? UI_KSEL : UI_K3);
    }
    if (v->page) {
        static const char *const S[4] = {"CHORUS", "ECHO", "ROOM", "DRY"};
        for (k = 0; k < 4; k++)
            px_ctext(17 + k * 29, 112, S[k], FK[k], PX_FS, 1);
    } else {                                            /* (the four knobs named, the turned one bright) */
        static const char *const L[4] = {"LEVEL", "PAN", "ECHO", "ROOM"};
        for (k = 0; k < 4; k++)
            px_ctext(17 + k * 29, 112, L[k], v->last == k || v->last < 0 ? FK[k] : UI_G2, PX_FS, 1);
    }
    for (k = 0; k < 4; k++)                             /* the locks as chips in the head */
        if (v->locked[k] || v->lockany[k]) {
            if (v->locked[k])
                px_rect(44 + k * 14, 3, 12, 7, FK[k]);
            else
                px_box(44 + k * 14, 3, 12, 7, FK[k]);
            px_ctext(50 + k * 14, 4, v->ltxt[k], v->locked[k] ? UI_BG : FK[k], PX_FS, 1);
        }
    step_frame(v->step);
}

/* ---------------------------------------------------------------- the FX tower: the lift, the rooms
 * The effects are the floors of a tower, B1 (SENDS, the villain's control room) at the top, the signal going down to
 * the TV at the bottom. The shaft is at the left: a window a floor (lit: the effect is heard), the agent in the car at
 * the page's floor. A change of floor rides the lift: the car moves, the new room slides in past the old one's place.
 * A room: x 10..118, y 14..88; KNOB 1..4 along the foot. */
#define FX_X 10
#define FX_W 108
#define FX_FLOOR 86

static uint32_t fx_h(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}

static void fx_girder(void)
{
    px_rect(FX_X, FX_FLOOR, FX_W, 1, PX_LBLUE);
    px_rect(FX_X, FX_FLOOR + 1, FX_W, 1, PX_BLUE);
}

static void fx_shaft(const ui_fx_t *v)
{
    const int n = v->nfloor > 0 ? v->nfloor : 1;
    int k, cy = 15 + v->floor * 8;
    px_vdots(0, 14, 75, UI_G2, 2);
    px_vdots(8, 14, 75, UI_G2, 2);
    if (v->ride_from >= 0 && v->ride < 1.f)
        cy = 15 + v->ride_from * 8 + (int)((float)((v->floor - v->ride_from) * 8) * v->ride);
    for (k = 0; k < n; k++) {
        const int y = 15 + k * 8;
        px_rect(3, y + 2, 3, 3, (v->floor_on >> k) & 1 ? PX_LGREEN : PX_DGREY);
        px_hline(1, y + 7, 7, UI_G2);
    }
    px_rect(1, cy, 7, 7, PX_BLACK);                     /* the car, the agent in it */
    px_box(1, cy, 7, 7, PX_YELLOW);
    px_pset(4, cy + 1, PX_LRED);
    px_rect(3, cy + 2, 3, 2, PX_LBLUE);
    px_pset(3, cy + 4, PX_LGREY), px_pset(5, cy + 4, PX_LGREY);
}

/* SENDS: the Evil Genius's whole machine, the track's four sends on its parts (no words on it):
 *   ROOM (KNOB 1)    the spark between the pylons: taller and busier with the send, none at 0
 *   CHORUS (KNOB 2)  the left monitor: a wave, its height the send (a flat line at 0)
 *   ECHO (KNOB 3)    the right monitor: a hit and its repeats, as many and as tall as the send
 *   DRY (KNOB 4)     the spark under its feet
 * each in its knob's colour; a part locked on the held step has a filled mark by it, one locked on some step an
 * outlined one. The knob turned last names its send and the value in a small plate by its part, for a moment (the
 * view lets v->last go after it). The machine's legs are left out: the page's row is at the foot. */
#define SM_X 14                                         /* the machine's corner (GENIUS_BIG's row 1 at SM_Y) */
#define SM_Y 10
static uint32_t sm_hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15, h *= 0x2C1B3C6Du;
    return h ^ h >> 12;
}
/* a spark from x0 to x1 about y: its corners up and down by amp, new ones every tick */
static void sm_spark(int x0, int x1, int y, int amp, int tick, qd_rgb c)
{
    int x, px0 = x0, py0 = y, k = 0;
    for (x = x0 + 4; x <= x1; x += 4, k++) {
        const int last = x + 4 > x1;
        const int yy = last ? y : y + (int)(sm_hash((uint32_t)k, (uint32_t)tick) % (uint32_t)(2 * amp + 1)) - amp;
        px_line(px0, py0, last ? x1 : x, yy, c);
        px0 = last ? x1 : x, py0 = yy;
    }
}
static void fx_sends(const ui_fx_t *v)
{
    static const char *const NM[4] = {"ROOM", "CHORUS", "ECHO", "DRY"};
    static const int MARK[4][2] = {{SM_X + 50, SM_Y - 1}, {SM_X + 4, SM_Y + 46}, {SM_X + 82, SM_Y + 46}, {SM_X + 28, SM_Y + 79}};
    static const int PLATE[4][2] = {{SM_X + 50, SM_Y + 10}, {SM_X + 22, SM_Y + 28}, {SM_X + 78, SM_Y + 28}, {SM_X + 50, SM_Y + 64}};
    const int tick = (int)(v->t * 14.f);
    int k, i;
    px_sprite_part(SM_X, SM_Y, PXS_GENIUS_BIG, 0, 1, 100, 76);       /* the pylons to the computer's underside */
    ui_genius_bubbles(SM_X, SM_Y - 1, v->t);
    if (v->beat >= 0.f && v->beat < 0.15f)                          /* it laughs at the track's notes */
        px_sprite(SM_X + 33, SM_Y + 27, PXS_GENIUS_HA, 0);
    px_rect(SM_X + 37, SM_Y, 33, 9, PX_BLACK);                      /* (its own sparks and screens out) */
    px_rect(SM_X + 9, SM_Y + 43, 12, 8, PX_BLACK);
    px_rect(SM_X + 86, SM_Y + 43, 12, 8, PX_BLACK);
    if (v->has[0] && v->x[0] > 0.f) {                               /* ROOM: the pylons' spark, two strands */
        sm_spark(SM_X + 37, SM_X + 70, SM_Y + 4, 1 + (int)(v->x[0] * 3.9f), tick, FK[0]);
        sm_spark(SM_X + 37, SM_X + 70, SM_Y + 5, 1 + (int)(v->x[0] * 3.9f), tick + 3, PX_WHITE);
    }
    if (v->has[1]) {                                                /* CHORUS: the left monitor's wave */
        int prev = SM_Y + 47;
        for (i = 0; i < 12; i++) {
            const int y = SM_Y + 47 - (int)(fz_sinf((float)i * 1.1f + v->t * 5.f) * v->x[1] * 3.6f);
            px_line(SM_X + 9 + (i ? i - 1 : 0), prev, SM_X + 9 + i, y, v->x[1] > 0.f ? FK[1] : UI_G3);
            prev = y;
        }
    }
    if (v->has[2]) {                                                /* ECHO: the right monitor: the hit, its repeats */
        const int n = v->x[2] > 0.f ? 1 + (int)(v->x[2] * 3.9f) : 0;
        px_rect(SM_X + 86, SM_Y + 44, 2, 7, v->x[2] > 0.f ? FK[2] : UI_G3);
        for (i = 0; i < n; i++) {
            const int h = (int)(6.f * v->x[2]) - i;
            if (h > 0)
                px_rect(SM_X + 89 + i * 3 - (i == 3), SM_Y + 51 - h, 2, h, FK[2]);
        }
    }
    if (v->has[3] && v->x[3] > 0.f) {                               /* DRY: the spark under its feet */
        sm_spark(SM_X + 30, SM_X + 68, SM_Y + 77, 1 + (int)(v->x[3] * 1.9f), tick + 7, FK[3]);
        sm_spark(SM_X + 30, SM_X + 68, SM_Y + 78, 1 + (int)(v->x[3] * 1.9f), tick + 11, PX_WHITE);
    }
    for (k = 0; k < 4; k++) {
        if (!v->has[k])
            continue;
        if (v->locked[k])                                           /* a lock: a mark by the part */
            px_rect(MARK[k][0], MARK[k][1], 3, 3, FK[k]);
        else if (v->lockany[k])
            px_box(MARK[k][0], MARK[k][1], 3, 3, FK[k]);
        if (v->last == k) {                                         /* the knob being turned: its plate */
            const int w = px_text_w(NM[k], PX_FS, 1) + 3 + px_text_w(v->txt[k], PX_FS, 1) + 5;
            const int x = PLATE[k][0] - w / 2, y = PLATE[k][1];
            px_rbox(x, y, w, 9, FK[k], UI_BG);
            px_text(px_text(x + 3, y + 2, NM[k], UI_G1, PX_FS, 1) + 3, y + 2, v->txt[k], FK[k], PX_FS, 1);
        }
    }
}

/* CHORUS: the agent and his two doubles, apart by WIDTH, drifting by DEPTH at RATE; the two waves over them */
static void fx_chorus(const ui_fx_t *v)
{
    static const int RUN[4] = {PXS_AGENT_RUN1, PXS_AGENT_RUN2, PXS_AGENT_RUN3, PXS_AGENT_RUN2};
    const int on = v->x[3] > 0.f;
    int x, k;
    for (k = 0; k < 2; k++) {
        int prev = -1;
        for (x = FX_X + 2; x < FX_X + FX_W - 2; x++) {
            const float sway = fz_sinf(v->t * 2.f * v->chorus_hz + (float)k * 2.f) * v->depth * 6.f * (k ? v->cwidth : 0.3f);
            const int y = 34 - (int)(fz_sinf((float)x * 0.18f + sway) * 12.f);
            if (prev >= 0)
                px_line(x - 1, prev, x, y, k ? FK[1] : FK[0]);
            prev = y;
        }
    }
    fx_girder();
    for (k = 0; k < 3; k++) {
        const int o = k == 0 ? -1 : k == 1 ? 1 : 0;      /* (the doubles first, the agent over them) */
        const float drift = fz_sinf(v->t * UI_TAU * v->chorus_hz * 0.5f + (float)k * 2.1f) * v->depth * 5.f;
        const int cx = 64 + o * (6 + (int)(v->cwidth * 26.f)) + (o ? (int)drift : 0);
        const int spr = RUN[((int)(v->t * 8.f) + k) & 3];
        if (o)
            px_sprite_tint(cx - 4, FX_FLOOR - 16, spr, 0, on ? FK[k] : UI_G2);   /* (k 0, 1: the doubles; the
                                                         * colour from the table: the unit's ROM-address check reads
                                                         * a folded difference of two colours as an address) */
        else
            px_sprite(cx - 4, FX_FLOOR - 16, spr, 0);
    }
}

/* ECHO: the orb bouncing across the room, a bounce a repeat, each lower by FEEDBACK (LOFI: the later ones coarser) */
static void fx_echo(const ui_fx_t *v)
{
    const float fb = v->fdbk > 0.05f ? v->fdbk : 0.05f, lofi = v->tone;
    const int on = v->x[3] > 0.f, pitch = 15;
    float u;
    int k, n = 0, hh[7];
    for (k = 0; k < 7; k++) {
        hh[k] = (int)(56.f * fz_powf(fb, (float)k));
        if (hh[k] < 3)
            break;
        n++;
    }
    fx_girder();
    for (k = 0; k < n; k++) {                           /* each bounce's arc, dotted */
        const int x0 = FX_X + 3 + k * pitch, st = lofi > 0.4f && k > 1 ? 3 : 2;
        int i;
        for (i = 0; i <= pitch; i += st) {
            const float f = (float)i / (float)pitch;
            px_pset(x0 + i, FX_FLOOR - 1 - (int)((float)hh[k] * 4.f * f * (1.f - f)),
                    !on ? UI_G2 : v->ping ? (k & 1 ? FK[2] : FK[1]) : k < 3 ? FK[0] : UI_G1);
        }
    }
    u = fz_fmodf(v->t / (v->delay_s > 0.06f ? v->delay_s : 0.06f), (float)(n > 0 ? n : 1));
    k = (int)u;
    if (n > 0 && k < n) {
        const float f = u - (float)k;
        px_sprite(FX_X + 3 + k * pitch + (int)(f * (float)pitch) - 3,
                  FX_FLOOR - 7 - (int)((float)hh[k] * 4.f * f * (1.f - f)), PXS_ORB, 0);
    }
}

static void fx_echo2(const ui_fx_t *v)
{
    int k, x;
    px_text(FX_X + 2, 18, v->sync ? "SYNC" : "FREE", FK[0], PX_FS, 1);
    px_text(FX_X + 34, 18, v->ping ? "PING-PONG" : "STEREO", FK[1], PX_FS, 1);
    for (k = 0; k < 2; k++)                             /* the two sides' repeats */
        for (x = 0; x < 6; x++)
            if (!v->ping || ((x + k) & 1))
                px_sprite_tint(FX_X + 4 + x * 16 + k * 8, 34 + k * 14, PXS_ORB, 0, x < 2 ? FK[k] : UI_G2);
    for (x = FX_X + 2; x < FX_X + FX_W - 2; x++)        /* WOW: the line wobbling */
        px_pset(x, 74 + (int)(fz_sinf((float)x * 0.12f + v->t * 2.f) * v->wow * 5.f), FK[3]);
}

/* ROOM: the room as big as SIZE, the agent calling out in it (his call's rings to the walls), DECAY under it */
static void fx_room(const ui_fx_t *v)
{
    const int w = 30 + (int)(v->size * 72.f), h = 22 + (int)(v->size * 44.f), x = 64 - w / 2, y = 82 - h;
    const int rmax = (w < h * 2 ? w / 2 : h) - 4;
    int k;
    for (k = 0; k < 3; k++)                             /* the walls and their echoes inward, darker with DAMP */
        px_box(x + k * 3, y + k * 2, w - k * 6, h - k * 2, k == 0 ? FK[0] : v->damp > 0.5f && k > 1 ? UI_G3 : UI_G2);
    px_sprite(60, y + h - 17, PXS_AGENT_STAND, 0);
    for (k = 0; k < 3 && rmax > 6; k++) {               /* the call: arcs growing from his head */
        const int r = 5 + ((int)(v->t * 14.f) + k * (rmax / 3 + 1)) % rmax;
        const int cx = 64, cy = y + h - 14;
        int a;
        for (a = -5; a <= 5; a++) {
            const float an = (float)a * 0.16f;
            const int dx = (int)(fz_cosf(an) * (float)r), dyy = (int)(fz_sinf(an) * (float)r);
            if (cy + dyy > y + 1 && cy + dyy < y + h - 1) {
                px_pset(cx + 5 + dx, cy + dyy, r > rmax * 2 / 3 ? UI_G2 : FK[1]);
                px_pset(cx - 5 - dx, cy + dyy, r > rmax * 2 / 3 ? UI_G2 : FK[1]);
            }
        }
    }
    px_hline(x, 85, (int)((float)w * (v->decay < 0.f ? 1.f : v->decay)), FK[2]);
}

static void fx_room2(const ui_fx_t *v)
{
    int x;
    const int gap = (int)(v->pre * 30.f);
    px_rect(FX_X + 2, 30, 3, 40, UI_K3);                /* the dry hit, the gap, the tail */
    for (x = 0; x < 96 - gap; x++) {
        const int h = (int)(38.f * fz_expf(-(float)x * 0.04f));
        px_vline(FX_X + 6 + gap + x, 70 - h + (int)(fz_sinf((float)x * 0.5f + v->t * 3.f) * v->mod * 3.f), h + 1,
                 x & 1 ? UI_G2 : FK[1]);
    }
    px_line(FX_X + 2, 84, FX_X + 2 + (int)(v->low * 30.f), 76, FK[2]);   /* LOW CUT: a slope */
}

/* CRUSH: the wave stamped into stairs (BITS the levels, RATE the treads), the press that does it at the right */
static void fx_crush(const ui_fx_t *v)
{
    const int levels = 2 + (int)(v->x[0] * 14.f), step = 2 + (int)(v->x[1] * 12.f);
    const int on = v->x[3] > 0.f;
    const float beat = fz_fmodf(v->t * 2.f, 1.f);
    const int drop = on ? (int)((beat < 0.2f ? beat * 5.f : 1.f - (beat - 0.2f) * 1.25f) * 38.f) : 0;
    int x;
    for (x = FX_X + 2; x < FX_X + 86; x += step) {
        const float s = fz_sinf((float)(x - FX_X - 2) * 0.075f);
        const int q = (int)(s * (float)levels / 2.f);
        px_rect(x, 50 - q * 30 / (levels / 2 > 0 ? levels / 2 : 1), step, 2, on ? FK[0] : UI_G2);
    }
    fx_girder();
    px_rect(104, 14, 4, 8 + drop, UI_G1);               /* the press: its ram, its head */
    px_rect(96, 22 + drop, 20, 8, on ? PX_YELLOW : UI_G2);
    for (x = 0; x < 5; x++)
        px_rect(97 + x * 4, 24 + drop, 2, 4, PX_BLACK);
    if (drop > 34)                                      /* (what was under it) */
        px_rect(98, FX_FLOOR - 3, 16, 3, PX_LGREY);
    else
        px_sprite(101, FX_FLOOR - 12, PXS_ROBOT1, 0);
}

/* ROBOT: the sentry speaking (a ring modulator is its voice): its eye and arm spark at the carrier's rate */
static void fx_robot(const ui_fx_t *v)
{
    const int on = v->x[3] > 0.f, spark = on && ((int)(v->t * (4.f + v->x[0] * 20.f)) & 1);
    int x, prev = -1;
    fx_girder();
    px_sprite_sc(FX_X + 4, FX_FLOOR - 42, spark ? PXS_SENTRY2 : PXS_SENTRY1, 1, 3, on ? QD_NONE : UI_G2);
    if (spark) {
        px_rect(FX_X + 39, 66, 2, 2, PX_YELLOW);
        px_rect(FX_X + 42, 63, 2, 2, PX_WHITE);
        px_rect(FX_X + 44, 67, 2, 2, PX_YELLOW);
    }
    for (x = 0; x < 58; x++) {                          /* the ring: the carrier times the voice */
        const float c = fz_sinf((float)x * (0.3f + v->x[0] * 2.f) + (on ? v->t * 6.f : 0.f));
        const int y = 40 - (int)(fz_sinf((float)x * 0.11f) * c * 22.f);
        if (prev >= 0)
            px_line(FX_X + 48 + x - 1, prev, FX_X + 48 + x, y, on ? FK[0] : UI_G2);
        prev = y;
    }
}

/* WARP: the teleporter. The comb's teeth are its beams (TIME how many, FEEDBACK how tall); the agent on the pad
 * comes apart in bands that climb at RATE */
static void fx_warp(const ui_fx_t *v)
{
    const float teeth = 3.f + v->x[0] * 8.f, fb = v->x[1] * 2.f - 1.f;
    const int on = v->x[3] > 0.f, ph = (int)(v->t * (2.f + v->x[2] * 14.f));
    int x, r;
    for (x = FX_X + 2; x < FX_X + FX_W - 2; x += 2) {
        const float p = (float)(x - FX_X) / (float)FX_W * teeth * UI_PI + v->t * v->x[2] * 4.f;
        const float g = fz_cosf(p) * (fb >= 0.f ? 1.f : -1.f);
        const int h = 6 + (int)((g * 0.5f + 0.5f) * (18.f + (fb < 0.f ? -fb : fb) * 44.f));
        px_vdots(x, FX_FLOOR - 2 - h, h, on ? PX_BLUE : UI_G2, 2);
    }
    px_sprite_sc(50, FX_FLOOR - 50, PXS_AGENT_STAND, 0, 3, QD_NONE);
    for (r = 0; r < 48 && on; r++)                      /* the bands taken out of him */
        if (((r + ph) % 12) < 1 + (int)(v->x[3] * 4.f))
            px_hline(48, FX_FLOOR - 50 + r, 32, (r + ph) % 12 == 0 ? FK[0] : PX_BLACK);
    px_rect(44, FX_FLOOR - 2, 40, 2, on ? PX_YELLOW : UI_G2);   /* the pad */
    px_rect(44, 15, 40, 2, on ? PX_YELLOW : UI_G2);
}

/* GLITCH: the picture breaks: the agent cut in bands thrown sideways, blocks of rubbish over the room, more with
 * CHANCE, longer with SLICE */
static void fx_glitch(const ui_fx_t *v)
{
    const int on = v->x[3] > 0.f, tick = (int)(v->t * 7.f);
    const int amt = on ? 2 + (int)(v->x[1] * 14.f) : 0;
    int b, k;
    fx_girder();
    for (b = 0; b < 8; b++) {                           /* the agent, 3x, a band at a time */
        const uint32_t h = fx_h((uint32_t)b, (uint32_t)tick);
        const int off = amt && (h & 3u) == 0u ? (int)((h >> 4) % (uint32_t)(2 * amt + 1)) - amt : 0;
        qd_save();
        qd_clip_rect(0.f, (float)(2 * (FX_FLOOR - 48 + b * 6)), 240.f, 12.f);
        px_sprite_sc(50 + off, FX_FLOOR - 48, PXS_AGENT_STAND, 0, 3, off ? (h & 16u ? FK[0] : FK[3]) : QD_NONE);
        qd_restore();
    }
    for (k = 0; k < amt; k++) {                         /* the rubbish */
        const uint32_t h = fx_h((uint32_t)k + 57u, (uint32_t)(tick / (1 + (int)(v->x[0] * 3.f))));
        const int x = FX_X + 2 + (int)(h % 96u), y = 18 + (int)((h >> 8) % 60u);
        px_checker(x, y, 4 + (int)((h >> 16) & 7u), 4, (h >> 20) & 1u ? FK[1] : FK[2], (int)(h >> 21) & 1);
    }
}

static void fx_tv(const ui_fx_t *v)
{
    const int on = v->x[3] > 0.f, roll = (int)(v->t * 10.f * v->x[1]) % 30;
    int y;
    px_rbox(14, 16, 82, 62, UI_G1, PX_DGREY);
    px_rect(20, 20, 64, 52, PX_BLUE);
    for (y = 21; y < 72; y += 2)
        px_hline(20, y, 64, on ? PX_LBLUE : UI_G3);
    if (on && v->x[1] > 0.05f)                          /* the hum: a bar rolling up the screen */
        px_rect(20, 70 - roll - 4, 64, 3, PX_LBLUE);
    px_rect(24, 28, 28, 18, PX_BLUE);
    px_text(26, 30, "READY.", PX_WHITE, PX_FS, 1);
    if (on && ((int)(v->t * 2.f) & 1))
        px_rect(26, 38, 4, 5, PX_WHITE);
    px_rect(88, 22, 6, 6, PX_BLACK);                    /* its knobs, its legs */
    px_rect(88, 34, 6, 2, UI_G2);
    px_rect(88, 40, 6, 2, UI_G2);
    px_rect(34, 78, 4, 6, UI_G2);
    px_rect(72, 78, 4, 6, UI_G2);
    {
        int k;
        for (k = 0; k < 3; k++)                         /* DRIVE: the speaker's grille glowing */
            px_rect(101 + k * 5, 70 - (int)(v->x[2] * 30.f) * (k + 1) / 3, 3, (int)(v->x[2] * 30.f) * (k + 1) / 3 + 1, FK[2]);
    }
}

void ui_fx_draw(const ui_fx_t *v)
{
    int k, dy = 0;
    panel_head(v->name ? v->name : "", v->npg, v->ipg, v->nsel);
    if (v->track >= 1 && v->track <= UI_NTRACK) {       /* (every page has a send of this track's) */
        char n[2] = {(char)('0' + v->track), 0};
        px_rect(94, 2, 8, 8, UI_K4);
        px_ctext(98, 3, n, UI_BG, PX_FS, 1);
    }
    if (v->ride_from >= 0 && v->ride < 1.f)             /* the lift's ride: the room slides in */
        dy = (int)((1.f - v->ride) * 76.f) * (v->floor > v->ride_from ? 1 : -1);
    qd_save();
    qd_clip_rect((float)(2 * (FX_X - 1)), 26.f, (float)(2 * (FX_W + 2)), 152.f);
    if (dy) {
        qd_translate(0.f, (float)(2 * dy));
        px_rect(FX_X, dy > 0 ? 9 : 91, FX_W, 3, UI_G2);  /* (the slab between the floors) */
    }
    switch (v->draw) {
    case UI_FXD_DELAY: fx_echo(v); break;
    case UI_FXD_DELAY2: fx_echo2(v); break;
    case UI_FXD_REVERB: fx_room(v); break;
    case UI_FXD_REVERB2: fx_room2(v); break;
    case UI_FXD_CHORUS: fx_chorus(v); break;
    case UI_FXD_OVER: fx_sends(v); break;
    case UI_FXD_CRUSH: fx_crush(v); break;
    case UI_FXD_ROBOT: fx_robot(v); break;
    case UI_FXD_WARP: fx_warp(v); break;
    case UI_FXD_GLITCH: fx_glitch(v); break;
    case UI_FXD_TV: fx_tv(v); break;
    case UI_FXD_MIXES: break;                            /* (drawn below, over the foot too) */
    default: break;
    }
    qd_restore();
    fx_shaft(v);
    if (v->draw == UI_FXD_OVER) {                       /* SENDS: the page's row (or the keys) at the foot */
        if (v->keys)
            ui_rec_keys_foot(v->kst, 0);
        else if (v->lane)
            ui_steps_row((const ui_lane_t *)v->lane, 180.f, 58.f);
    }
    if (v->draw == UI_FXD_MIXES)                        /* FX MIX: the five rooms' lifts, their names and values */
        mix_rooms(v->room, v->room_txt, v->room_lk, v->last, 21, 22);
    for (k = 0; k < 4 && v->draw != UI_FXD_OVER && v->draw != UI_FXD_MIXES; k++) {   /* KNOB 1..4 along the foot */
        const int cx = 15 + k * 30;
        const int f = px_text_w(v->txt[k], PX_FB, 1) > 27 ? PX_FS : PX_FB;
        if (!v->has[k])
            continue;
        px_ctext(cx, 93, v->lab[k] ? v->lab[k] : "", v->dim[k] ? UI_G3 : UI_G1, PX_FS, 1);
        if (v->locked[k] || v->lockany[k]) {
            if (v->locked[k])
                px_rect(cx - 14, 102, 28, 13, FK[k]);
            else
                px_box(cx - 14, 102, 28, 13, FK[k]);
        }
        px_ctext(cx, f == PX_FB ? 105 : 107, v->txt[k], v->locked[k] ? UI_BG : v->dim[k] ? UI_G3 : FK[k], f, 1);
        if (v->last == k)
            px_hline(cx - 9, 99, 19, FK[k]);
    }
    step_frame(v->step);
}
