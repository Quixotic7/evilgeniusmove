/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: screen 1, idle (FIZZ's logo screen, redrawn): the Evil Genius's hall at 120 x 120.
 *   the Evil Genius  a SID chip's brain in a jar of green liquid on its computer, from the owner's picture (the
 *                 sprite GENIUS_BIG, tools/eg/gen_genius.py): the spark between its pylons and under its feet
 *                 comes and goes. It throws bolts at the floor, behind the agent, who is already past; it laughs
 *                 (GENIUS_HA over its brain, shaking) before its name comes, and playing, on each beat
 *   the title     EVIL / GENIUS in the BIG font at 2x. FIZZ's word phases are kept (ui_idle_word): off at first,
 *                 the letters drop in one by one onto a plate over the computer, the title holds, then it melts: each column
 *                 of each letter runs down into the floor on its own time, cooling as it goes
 *   the floor     a girder; the agent runs it end to end and back once a loop, somersaulting the two patrol
 *                 robots whenever he meets one; a lift rides its shaft at the right
 *   the scroller  under the floor, between two raster lines, a line of text crawling right to left, once a loop
 *   playing       the border flashes on each beat (the downbeat brighter), the monitors step with the beats
 * The loop (FZ_LOOP s) is seamless: everything moves whole cycles a loop. Deterministic: a pure function of the time
 * and the beats. */
#include <math.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"
#include "../seq/qd_div.h"                    /* (qd_ratio_f) */

#define FZ_LOOP 11.f

/* the word's timing (loop seconds), FIZZ's: the tests read the phases through ui_idle_word */
#define WD_GATHER 4.0f                                  /* the first letter drops .. */
#define WD_STAGGER 0.12f
#define WD_TRAVEL 0.42f
#define WD_SOLID0 4.50f
#define WD_SOLID1 4.62f
#define WD_PFADE1 4.70f                                 /* .. all of them landed by here */
#define WD_BREAK0 8.78f                                 /* the letters start to fall away .. */
#define WD_BREAK1 8.90f
#define WD_FIZZ 8.86f
#define WD_FSTAG 0.6f
#define WD_FADE 0.9f                                    /* .. all gone by 10.36 s */

static float clamp01(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x; }
static float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
static uint32_t ih(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return h;
}

void ui_idle_word(float t, float *solid, float *parts)
{
    const float u = fmodf(t, FZ_LOOP);
    float s = 0.f, p = 0.f;
    if (u >= WD_GATHER && u < WD_PFADE1) {              /* dropping in, then landed */
        s = ui_ease(seg(u, WD_SOLID0, WD_SOLID1));
        p = 1.f - ui_ease(seg(u, WD_SOLID1 - 0.06f, WD_PFADE1));
    } else if (u >= WD_PFADE1 && u < WD_BREAK0) {       /* the hold */
        s = 1.f;
    } else if (u >= WD_BREAK0 && u < WD_FIZZ + WD_FSTAG + WD_FADE) {   /* falling away */
        s = 1.f - seg(u, WD_BREAK0, WD_BREAK1);
        p = seg(u, WD_BREAK0, WD_BREAK0 + 0.08f) * (1.f - seg(u, WD_FIZZ + WD_FSTAG, WD_FIZZ + WD_FSTAG + WD_FADE));
    }
    if (solid)
        *solid = s;
    if (parts)
        *parts = p;
}

/* ---------------------------------------------------------------- the room's layout (120-space) */
#define MX 4                                            /* the machine (GENIUS_BIG, 100 x 88) */
#define MY 0
#define CX (MX + 50)                                    /* the room's middle (the shaft takes the right) */
#define JAR_X 25                                        /* in it: the jar (56 x 41), the laugh's place */
#define JAR_Y 20
#define JAR_W 56
#define JAR_H 41
#define HA_X 33
#define HA_Y 28
#define PLATE_Y 59                                      /* the title's plate, over the computer */
#define T1_Y 61                                         /* EVIL's top */
#define T2_Y 76                                         /* GENIUS's top */
#define FLOOR_Y 104
#define SHAFT_X 108                                     /* the lift's shaft: rails at 108 and 117 */
#define RUN_X0 2
#define RUN_X1 96                                       /* the agent's run (his left edge) */
#define NROBOT 2
static const int ROBOT_C[NROBOT] = {32, 76};            /* the robots' beats (their middles) */

/* the title's letters: their place, the time each lands (dropping in) */
static const char *const TITLE[2] = {"EVIL", "GENIUS"};
#define NLET 10
static void letter(int k, int *row, int *i)
{
    *row = k < 4 ? 0 : 1;
    *i = k < 4 ? k : k - 4;
}
static int letter_x(int row, int i)
{
    const int w = px_text_w(TITLE[row], PX_FB, 2);
    return CX - (w + 1) / 2 + i * 16;
}

/* how far a letter is above its place (dropping in), whole pixels; -1000: not shown */
static int letter_dy(float u, int k)
{
    float t;
    if (u < WD_GATHER)
        return -1000;
    if (u < WD_PFADE1) {                                /* dropping in: a fall from above the screen, landing */
        const float t0 = WD_GATHER + (WD_PFADE1 - WD_GATHER - 0.24f) * (float)k / (float)(NLET - 1);
        if (u < t0)
            return -1000;
        t = clamp01((u - t0) / 0.24f);
        return -(int)((1.f - t * t) * 70.f);
    }
    return 0;
}

/* the melt: every column of every letter starts down on its own moment (within MELT_SPREAD s of WD_BREAK0), faster
 * as it goes, into the floor; its colour cools on the way */
#define MELT_SPREAD 0.7f
static void melt_letter(float u, int k, int x, int y, int row)
{
    static const qd_rgb HOT[2][4] = {{PX_LRED, PX_RED, PX_BROWN, PX_BROWN}, {PX_WHITE, PX_LGREY, PX_GREY, PX_DGREY}};
    const int ch = TITLE[row][k < 4 ? k : k - 4];
    int c, r;
    for (c = 0; c < 8; c++) {
        const float t0 = WD_BREAK0 + MELT_SPREAD * (float)(ih((uint32_t)(k * 8 + c), 11u) & 255u) / 255.f;
        const float t = u > t0 ? u - t0 : 0.f;
        const int dy = (int)(120.f * t * t + 8.f * t), heat = t <= 0.f ? 0 : t < 0.25f ? 1 : t < 0.5f ? 2 : 3;
        int run = 0;
        for (r = 0; r <= 7; r++) {                      /* the column's runs of lit pixels, each a rect */
            const int lit = r < 7 && (px_glyph_row(PX_FB, ch, r) & (0x80 >> c));
            if (lit) {
                run++;
            } else if (run) {
                const int y0 = y + (r - run) * 2 + dy, y1 = y0 + run * 2 + (heat ? dy / 6 : 0);   /* (a drip under it) */
                if (y0 < FLOOR_Y)
                    px_rect(x + c * 2, y0, 2, (y1 > FLOOR_Y ? FLOOR_Y : y1) - y0, HOT[row][heat]);
                run = 0;
            }
        }
    }
}

static void draw_title(float u)
{
    int k;
    for (k = 0; k < NLET; k++) {
        int row, i, dy, x, y;
        char ch[2];
        letter(k, &row, &i);
        dy = letter_dy(u, k);
        if (dy == -1000)
            continue;
        x = letter_x(row, i);
        y = (row ? T2_Y : T1_Y) + dy;
        if (u >= WD_BREAK0) {
            melt_letter(u, k, x, y, row);
            continue;
        }
        if (y < PLATE_Y)                                /* dropping in: out from under the jar */
            continue;
        ch[0] = TITLE[row][i];
        ch[1] = 0;
        if (row == 0)
            px_text_sh(x, y, ch, PX_LRED, PX_RED, PX_FB, 2);
        else
            px_text_sh(x, y, ch, PX_WHITE, PX_DGREY, PX_FB, 2);
    }
}

/* ---------------------------------------------------------------- the Evil Genius */
/* The brain in its jar alone (56 x 41 at x, y: the machine's jar), laughing or not (its laugh shakes). The mascot
 * wherever it is big without its machine (the effects' SENDS, the scene of a word); small, it is PXS_GENIUS. */
/* The jar's liquid boiling, over the machine drawn at ox, oy (GENIUS_BIG's corner) and under the laugh: bubbles up
 * the liquid at the brain's two sides (each a ring, swaying, a white pop as it reaches the surface), and the
 * surface itself running along. Only where the picture is liquid: the brain is never crossed. */
void ui_genius_bubbles(int ox, int oy, float t)
{
    int i;
    for (i = 0; i < 8; i++) {
        const uint32_t h = ih((uint32_t)i, 9u);
        /* (whole climbs a loop of the idle screen, 3 to 6 of them: it loops without a seam) */
        const float rise = t * (25.f * (float)(3u + (h & 3u)) / FZ_LOOP) + (float)(h >> 8 & 31u);
        const int k = (int)rise % 25;                   /* 0 at the bottom .. 24 at the surface */
        const int x = ox + (i & 1 ? 74 : 29) + (int)(h >> 16 & 3u) + (((k >> 1) + i) & 1);
        const int y = oy + 56 - k;
        if (k >= 22) {                                  /* the pop */
            px_pset(x, y, PX_WHITE);
        } else if (i < 4) {                             /* a ring */
            px_pset(x, y - 1, PX_LGREEN), px_pset(x, y + 1, PX_LGREEN);
            px_pset(x - 1, y, PX_LGREEN), px_pset(x + 1, y, PX_LGREEN);
        } else {                                        /* a small one */
            px_pset(x, y, PX_LGREEN);
        }
    }
    for (i = 28; i < 78; i++)                           /* the surface: a ripple going along, clear of the brain */
        if ((i < 44 || i > 66) && ((i + (int)(t * 7.f)) % 7) < 2)
            px_pset(ox + i, oy + 30, PX_LGREEN);
}

void ui_genius(int x, int y, int laugh, float t)
{
    px_sprite_part(x, y, PXS_GENIUS_BIG, JAR_X, JAR_Y, JAR_W, JAR_H);
    ui_genius_bubbles(x - JAR_X, y - JAR_Y, t);
    if (laugh)
        px_sprite(x + HA_X - JAR_X, y + HA_Y - JAR_Y - ((int)(t * 12.f) & 1), PXS_GENIUS_HA, 0);
    px_rect(x + 2, y + JAR_H, JAR_W - 4, 2, PX_GREY);   /* a collar under it */
    px_rect(x + 4, y + JAR_H, 8, 1, PX_LGREY), px_rect(x + JAR_W - 12, y + JAR_H, 8, 1, PX_LGREY);
}

/* a bolt from x0, y0 to x1, y1: a zigzag, its corners moved by the seed */
static void bolt(int x0, int y0, int x1, int y1, int n, uint32_t seed, qd_rgb c)
{
    int k, px0 = x0, py0 = y0;
    for (k = 1; k <= n; k++) {
        int x = x0 + (x1 - x0) * k / n, y = y0 + (y1 - y0) * k / n;
        if (k < n)
            x += (int)(ih((uint32_t)k, seed) % 9u) - 4;
        px_line(px0, py0, x, y, c);
        px0 = x, py0 = y;
    }
}

static void draw_machine(int laugh, int flick, float t)
{
    px_sprite(MX, MY, PXS_GENIUS_BIG, 0);
    if (!(flick & 1))                                   /* the pylons' spark: there, gone */
        px_rect(MX + 37, MY + 1, 33, 9, PX_BLACK);
    if (flick & 2)                                      /* .. and the one under its feet */
        px_rect(MX + 33, MY + 83, 30, 5, PX_BLACK);
    ui_genius_bubbles(MX, MY, t);
    if (laugh)
        px_sprite(MX + HA_X, MY + HA_Y - ((int)(t * 12.f) & 1), PXS_GENIUS_HA, 0);
}

/* ---------------------------------------------------------------- the floor, the shaft, the agent, the robots */
static int tri_wave(float u, float lo, float hi)        /* lo .. hi .. lo once a loop */
{
    const float f = u / FZ_LOOP;
    const float g = f < 0.5f ? f * 2.f : 2.f - f * 2.f;
    return (int)(lo + (hi - lo) * g + 0.5f);
}

static void draw_floor(float u)
{
    int x;
    const int ly = 34 + (int)(30.f * (1.f - cosf(UI_TAU * 2.f * u / FZ_LOOP)) + 0.5f);   /* twice a loop */
    for (x = 0; x < SHAFT_X - 2; x += 8)
        px_sprite(x, FLOOR_Y, PXS_FLOOR, 0);
    px_vdots(SHAFT_X, 0, FLOOR_Y + 4, PX_DGREY, 2);      /* the shaft's rails */
    px_vdots(SHAFT_X + 9, 0, FLOOR_Y + 4, PX_DGREY, 2);
    px_vdots(SHAFT_X + 1, 1, FLOOR_Y + 3, PX_GREY, 2);
    px_vdots(SHAFT_X + 10, 1, FLOOR_Y + 3, PX_GREY, 2);
    px_sprite(SHAFT_X + 1, ly, PXS_LIFT, 0);
    px_rect(SHAFT_X + 1, FLOOR_Y, 8, 1, PX_DGREY);       /* the shaft's sill */
}

/* the bolts: one a BOLT_EVERY s while the title is away, from under the computer to where the agent was a moment
 * before (he is past it), lit for BOLT_ON s, a burst where it lands */
#define BOLT_EVERY 0.8f
#define BOLT_ON 0.24f
static void draw_bolts(float u)
{
    const int slot = (int)(u / BOLT_EVERY);
    const float u0 = (float)slot * BOLT_EVERY, f = u - u0;
    int tx;
    if (f >= BOLT_ON || (u0 + BOLT_ON > WD_GATHER - 0.6f && u0 < WD_FIZZ + WD_FSTAG + WD_FADE))
        return;
    tx = tri_wave(u0 > 0.6f ? u0 - 0.6f : u0 + FZ_LOOP - 0.6f, (float)RUN_X0, (float)RUN_X1) + 4;
    bolt(CX, MY + 86, tx, FLOOR_Y - 1, 3, (uint32_t)(slot * 7 + (int)(f * 30.f)), (int)(f * 30.f) & 1 ? PX_WHITE : PX_YELLOW);
    px_hline(tx - 3, FLOOR_Y - 1, 7, PX_YELLOW);        /* where it lands */
    px_pset(tx - 4, FLOOR_Y - 3, PX_WHITE), px_pset(tx + 4, FLOOR_Y - 3, PX_WHITE), px_pset(tx, FLOOR_Y - 4, PX_YELLOW);
}

static void draw_actors(float u)
{
    const int rw = px_sprite_w(PXS_ROBOT1), rh = px_sprite_h(PXS_ROBOT1);
    const int aw = px_sprite_w(PXS_AGENT_STAND), ah = px_sprite_h(PXS_AGENT_STAND);
    const float f = u / FZ_LOOP;
    const int dir = f < 0.5f ? 1 : -1;                  /* the agent: right on the way out, left back */
    const int ax = tri_wave(u, (float)RUN_X0, (float)RUN_X1), span = 13;
    int ay = FLOOR_Y - ah, sprite, k;
    static const int RUN[4] = {PXS_AGENT_RUN1, PXS_AGENT_RUN2, PXS_AGENT_RUN3, PXS_AGENT_RUN2};

    sprite = RUN[(int)(u * 10.f) % 4];
    for (k = 0; k < NROBOT; k++) {
        const float w = UI_TAU * (float)(2 + k) * u / FZ_LOOP + (float)k * 1.7f;
        const int rx = ROBOT_C[k] - rw / 2 + (int)floorf(9.f * sinf(w) + 0.5f);
        const int dx = (ax + aw / 2) - (rx + rw / 2);
        px_sprite(rx, FLOOR_Y - rh, ((int)(u * 4.f) + k) % 2 ? PXS_ROBOT2 : PXS_ROBOT1, cosf(w) < 0.f);
        if (dx > -span && dx < span) {                  /* over a robot: a somersault, its arc by the distance */
            const float q = (float)dx / (float)span;
            ay = FLOOR_Y - ah - (int)(15.f * (1.f - q * q) + 0.5f);
            sprite = dx > -span * 2 / 3 && dx < span * 2 / 3 ? PXS_AGENT_FLIP : PXS_AGENT_RUN2;
        }
    }
    if (ax <= RUN_X0 || ax >= RUN_X1)                   /* at the ends: standing, turning */
        sprite = PXS_AGENT_STAND;
    px_sprite(ax, ay, sprite, dir < 0);
}

/* ---------------------------------------------------------------- the scroller */
static const char SCROLL[] = "EVILGENIUS FOR THE FM-1 · SIX TRACKS · A SID CHIP PER VOICE · WORDS IN $D418 · "
                             "EIGHT ROOMS OF FX · ANY KEY WAKES ME";
static void draw_scroller(float u, int playing, long beat)
{
    const int w = px_text_w(SCROLL, PX_FS, 1);
    const int x = 120 - (int)((float)(w + 120) * (u / FZ_LOOP));
    static const qd_rgb BAR[4] = {PX_BLUE, PX_LBLUE, PX_CYAN, PX_LBLUE};
    px_hline(0, 109, 120, BAR[playing ? (int)(beat & 3) : 1]);
    px_hline(0, 119, 120, BAR[playing ? (int)((beat + 2) & 3) : 0]);
    {   /* only the letters on the screen: the whole line is 950 rectangles, and the unit's frame holds 2816 */
        const char *p = SCROLL;
        int cx = x;
        while (*p && cx < 120) {
            const int n = ((unsigned char)*p & 0xE0u) == 0xC0u ? 2 : 1;   /* (a letter, or the middle dot's two bytes) */
            if (cx > -4) {
                char ch[3] = {0, 0, 0};
                ch[0] = p[0];
                if (n == 2)
                    ch[1] = p[1];
                px_text(cx, 112, ch, PX_LGREY, PX_FS, 1);
            }
            p += n;
            cx += 4;
        }
    }
}

void ui_idle_draw(const ui_idle_t *v)
{
    const float t = v->t, u = fmodf(t, FZ_LOOP), bpm = (float)(v->bpm > 0 ? v->bpm : 120);
    float solid, parts, frac = 0.f;
    int talk, flick;

    ui_idle_word(t, &solid, &parts);
    if (v->playing)                                     /* the part of the beat gone */
        frac = qd_ratio_f(v->beat_num, v->beat_den > 0 ? v->beat_den : 1);
    (void)bpm;

    /* it laughs before its name comes and as the name lands, and again as the name melts */
    talk = (u >= WD_GATHER - 1.1f && u < WD_PFADE1 + 0.5f) || (u >= WD_BREAK0 - 0.2f && u < WD_BREAK0 + 1.1f);
    flick = (int)(u * 6.f);
    if (v->playing) {
        talk = frac < 0.25f;
        flick = (int)(v->beat & 0xffff);
    }
    draw_machine(talk, flick, t);
    draw_floor(u);
    if (solid > 0.f || parts > 0.f || (u >= WD_GATHER && u < WD_FIZZ + WD_FSTAG + WD_FADE)) {
        if (u < WD_BREAK0 + MELT_SPREAD) {              /* the plate the name is on, gone as the last column runs */
            px_rect(MX, PLATE_Y, 100, T2_Y + 16 - PLATE_Y, PX_BLACK);
            px_hline(MX, PLATE_Y, 100, PX_LBLUE);
        }
        draw_title(u);
    }
    draw_bolts(u);
    draw_actors(u);
    draw_scroller(u, v->playing, v->beat);

    if (v->playing && frac < 0.125f) {                  /* the border: a flash on each beat, the downbeat brighter */
        const qd_rgb c = v->beat % 4 == 0 ? PX_YELLOW : PX_LBLUE;
        px_rect(0, 0, 120, 1, c);
        px_rect(0, 119, 120, 1, c);
        px_rect(0, 0, 1, 120, c);
        px_rect(119, 0, 1, 120, c);
    }
}
