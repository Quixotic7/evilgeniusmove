/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: a held step, a mode's track default, and the EDIT view (FIZZ's ui_held.c, redrawn on the 120 x 120
 * grid; the models are FIZZ's). One picture a mode, in the spirit of the 1984 action-puzzle games:
 *   NOTE   the chord's name big over a keybed, its notes lit in their knobs' colours
 *   VEL    the agent's somersault: as high as the velocity (it jumps as the step plays); the ramp's hops beside it
 *   LEN    a lift platform over the steps, as long as the note (INF: off the edge); TIE a chain from the note before
 *   MICRO  the agent standing off the grid's flag by the offset; STRUM a staircase of platforms
 *   RATCH  a patrol robot firing its hits, each bolt as long as its hit's velocity
 *   CHNC / COND  the security panel: thirty lights lit by the chance; the condition big, its passes as blocks
 *   SOUND  a floppy disk with the sound's name on its label, its neighbours peeking
 *   TRIG   three terminals: NOTE, LOCK, TIE (the step's lit); its MIDI FX group under them
 *   FUNC   the function's icon big, its name; JUMP's target page as a row
 *   SLIDE  two platforms joined by a lift's ramp, as long as the TIME
 * A held step (or the latch's selection) is framed in purple; a track's default is not. */
#include <string.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"

typedef struct {
    qd_rgb k, k2, wh;                   /* the mode's colour (grey when the step plays the default), the other mode's,
                                         * white (grey for an empty step) */
    int empty, nobig;                   /* an empty step; the value is in the head (EDIT): no big value in the body */
} pal_t;

static char *cat(char *d, const char *s)
{
    char *e = d;
    while (*e)
        e++;
    while (*s)
        *e++ = *s++;
    *e = 0;
    return d;
}

static int other_mode(int f)
{
    switch (f) {
    case UI_F_VEL: return UI_F_RATCH;
    case UI_F_LEN: return UI_F_TRIG;
    case UI_F_MICRO: return UI_F_NOTE;
    case UI_F_COND: return UI_F_CHNC;
    case UI_F_CHNC: return UI_F_COND;
    default: return f;
    }
}

static int held_pal(const ui_held_t *v, pal_t *p)
{
    const int f = v->field >= 0 && v->field < UI_NFIELD ? v->field : UI_F_NOTE;
    const int lit = v->is_default || v->ovr;
    p->empty = v->type == 0 && f != UI_F_TRIG && !v->is_default;
    p->k = p->empty ? UI_G2 : v->is_default ? (f == UI_F_VEL ? UI_MODE_COL[UI_F_VEL] : UI_TRACK_COL)
         : lit ? UI_MODE_COL[f] : UI_G1;
    p->k2 = p->empty ? UI_G2 : UI_MODE_COL[other_mode(f)];
    p->wh = p->empty ? UI_G1 : UI_K3;
    p->nobig = 0;
    return f;
}

/* the big value: the 8 x 8 font doubled (or single, or the small one, as it fits) */
static void big_val(const char *s, int x, int y, int maxw, qd_rgb c, int right)
{
    int sc = 2, f = PX_FB, w = px_text_w(s, f, sc);
    if (w > maxw)
        sc = 1, w = px_text_w(s, f, sc);
    if (w > maxw)
        f = PX_FS, w = px_text_w(s, f, sc);
    px_text(right ? x - w : x, y, s, c, f, sc);
}

/* ---------------------------------------------------------------- the pictures (each in the area top..bot) */
static void f_note(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    static const qd_rgb SLOT[6] = {UI_KSEL, UI_K1, UI_K2, UI_K3, UI_K4, UI_KALG};
    qd_rgb fill[128];
    int i, k;
    const int kh = (bot - top) * 2 / 5, ky = bot - kh - 1;
    for (i = 0; i < 128; i++)
        fill[i] = QD_NONE;
    for (k = 0; k < 6; k++)
        if (v->slot_note[k] && v->slot_note[k] < 128)
            fill[v->slot_note[k]] = p->empty ? UI_G1 : SLOT[k];
    for (i = 0; i < v->nnote && i < 6; i++)
        if (fill[v->note[i]] == QD_NONE)
            fill[v->note[i]] = p->empty ? UI_G1 : p->k;
    {   /* the keybed: two octaves from the C at or under the lowest note (the entry octave's C) */
        int lo = 12 * (v->entry_oct + 1);
        if (v->nnote && v->note[0] < lo)
            lo = v->note[0] / 12 * 12;
        ui_keybed(8.f, (float)(2 * ky), 224.f, (float)(2 * kh), lo, 14, fill);
    }
    if (!p->nobig && !p->empty) {
        char s[16];
        s[0] = 0;
        if (v->root[0]) {
            cat(cat(s, v->root), v->qual);
            big_val(s, 60 - px_text_w(s, PX_FB, 2) / 2, top + (ky - top - 14) / 2, 112, p->k, 0);
        } else if (v->names[0]) {
            px_ctext(60, top + (ky - top) / 2 - 3, v->names, p->k, PX_FS, 1);
        }
    } else if (p->nobig && v->root[0] && !p->empty) {
        char s[16];
        s[0] = 0;
        cat(cat(s, v->root), v->qual);
        big_val(s, 60 - px_text_w(s, PX_FB, 2) / 2, top + (ky - top - 14) / 2, 112, p->k, 0);
    }
    if (p->empty)
        px_ctext(60, top + (ky - top) / 2 - 3, "EMPTY STEP", UI_G1, PX_FS, 1);
}

static void girders(int y)
{
    int x;
    for (x = 4; x < 116; x += 8)
        px_sprite(x, y, PXS_FLOOR, 0);
}

static void f_vel(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int ground = bot - 4, room = ground - top - 34;
    const int h = v->vel * room / 127;
    int i;
    float u = -1.f;                                     /* the jump's progress while the step plays */
    girders(ground);
    for (i = 0; i <= 40; i++) {                         /* the arc: a parabola from x 22 to 98, its apex h up */
        const float t = (float)i / 40.f;
        const int x = 22 + (int)(76.f * t), y = ground - 1 - (int)(4.f * (float)h * t * (1.f - t));
        if (i % 2 == 0)
            px_pset(x, y, p->k);
    }
    if (v->play >= 0.f && v->play < 0.7f)
        u = v->play / 0.7f;
    if (p->empty) {
        px_sprite_sc(51, ground - 32, PXS_AGENT_STAND, 0, 2, UI_G2);
    } else if (u >= 0.f) {
        const int x = 22 + (int)(76.f * u) - 9, y = ground - 1 - (int)(4.f * (float)h * u * (1.f - u)) - 22;
        px_sprite_sc(x, u < 0.15f || u > 0.85f ? y - 10 : y, u < 0.15f || u > 0.85f ? PXS_AGENT_RUN2 : PXS_AGENT_FLIP,
                     0, 2, QD_NONE);
    } else {
        px_sprite_sc(51, ground - 1 - h - 22, PXS_AGENT_FLIP, 0, 2, QD_NONE);
        px_sprite(8, ground - 16, PXS_AGENT_STAND, 0);
    }
    if (v->ratch > 1 && v->ramp) {                      /* the ramp: the hits' hops in the other mode's colour */
        for (i = 0; i < v->ratch && i < 8; i++) {
            const int hv = v->ramp == 1 ? (i + 1) * 8 / v->ratch : v->ramp == 2 ? (v->ratch - i) * 8 / v->ratch : i ? 5 : 8;
            px_rect(104 + (i % 4) * 3, ground - 2 - hv - (i / 4) * 10, 2, hv, p->k2);
        }
    }
    if (!p->nobig) {
        char s[12];
        big_val(ui_itoa(s, v->vel), 116, top + 1, 60, p->k, 1);
    }
}

static void f_len(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int y = (top + bot) / 2 + 8, x0 = 10, sw = 12;   /* a step 12 px: eight in sight */
    int i, w;
    for (i = 0; i <= 8; i++)                            /* the ruler */
        px_vline(x0 + i * sw, y + 6, i % 4 ? 2 : 4, i % 4 ? UI_G2 : UI_G1);
    px_hline(x0, y + 6, 8 * sw + 1, UI_G2);
    if (v->has_prev) {                                  /* the note before, ghosted */
        const int px0 = x0 + (int)(v->prev_at * (float)sw), pw = (int)(v->prev_len * (float)sw);
        if (px0 + pw > 0)
            px_dots(px0 < 0 ? 0 : px0, y - 6, pw, UI_G2, 2);
    }
    if (v->next_at > 0.f)
        px_dots(x0 + (int)(v->next_at * (float)sw), y - 6, (int)(v->next_len * (float)sw), UI_G2, 2);
    w = v->len_steps < 0.f ? 120 : (int)(v->len_steps * (float)sw);
    if (w < 3)
        w = 3;
    for (i = 0; i < w && x0 + i < 118; i += 8)          /* the platform */
        px_sprite(x0 + i, y - 2, PXS_LIFT, 0);
    if (x0 + w < 118)
        px_rect(x0 + w, y - 2, 1, 3, UI_BG);
    px_rect(x0, y - 3, w < 108 ? w : 108, 1, p->k);
    if (v->len_steps < 0.f) {                           /* INF: off the edge */
        px_pset(116, y - 2, p->k);
        px_pset(117, y - 1, p->k);
        px_pset(116, y, p->k);
    }
    px_sprite_sc(x0 - 2, y - 34, p->empty ? PXS_AGENT_STAND : PXS_AGENT_RUN1, 0, 2, p->empty ? UI_G2 : QD_NONE);
    if (v->tie)                                         /* TIE: a chain from the note before */
        for (i = 0; i < 4; i++)
            px_box(x0 - 9 + i * 2, y - 2 + (i & 1), 2, 2, p->k2);
    if (!p->nobig)
        big_val(v->len_name ? v->len_name : "", 116, top + 1, 70, p->k, 1);
}

static void f_micro(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int y = bot - 14, gx = 50, k = 2;             /* the grid at x 50, a 24th 2 px (-12 .. +36) */
    const int sx = gx + v->micro * k;
    int i;
    for (i = -12; i <= 30; i++)                         /* the tape measure */
        px_vline(gx + i * k, y + 2, i % 24 == 0 ? 5 : i % 6 == 0 ? 3 : 1, i % 24 == 0 ? UI_G1 : UI_G2);
    px_hline(gx - 24, y + 2, 85, UI_G2);
    px_vline(gx, y - 22, 24, UI_G1);                    /* the flag on the grid */
    px_rect(gx + 1, y - 22, 4, 3, UI_G1);
    px_sprite_sc(sx - 9, y - 30, p->empty ? PXS_AGENT_STAND : PXS_AGENT_RUN2, v->micro < 0, 2, p->empty ? UI_G2 : QD_NONE);
    if (v->micro)
        px_hline(gx < sx ? gx : sx, y + 8, (sx > gx ? sx - gx : gx - sx) + 1, p->k);
    if (v->walk > -12.f)                                /* the slow playhead */
        px_vline(gx + (int)(v->walk * (float)k), y - 4, 6, PX_LRED);
    if (v->strum)                                       /* STRUM: a staircase of platforms */
        for (i = 0; i < 4; i++)
            px_rect(gx + 30 + i * (2 + v->strum / 8), y - 4 - i * 3, 5, 1, p->k2);
    if (!p->nobig) {
        char s[12];
        s[0] = 0;
        if (v->micro > 0)
            cat(s, "+");
        ui_itoa(s + strlen(s), v->micro);
        big_val(s, 116, top + 1, 60, p->k, 1);
    }
}

static void f_ratch(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int n = v->count < 1 ? 1 : v->count > 8 ? 8 : v->count, ground = bot - 4;
    const int fired = (int)(v->ball * (float)n * 1.2f);
    int i;
    girders(ground);
    px_sprite_sc(2, ground - 24, fired < n ? PXS_ROBOT2 : PXS_ROBOT1, 0, 2, QD_NONE);
    for (i = 0; i < n; i++) {                           /* the bolts, stacked, each as long as its hit's velocity */
        const int vel = v->ramp == 1 ? (i + 1) * 127 / n : v->ramp == 2 ? (n - i) * 127 / n : v->ramp == 3 && i ? 80 : 127;
        const int y = ground - 12 - i * 6, len = 8 + vel * 80 / 127;
        const qd_rgb c = i < fired || v->ball < 0.f ? p->k : UI_G2;
        int x;
        for (x = 0; x < len; x += 3)
            px_rect(26 + x, y + ((x / 3) & 1), 2, 1, c);
        px_rect(26 + len, y - 1, 2, 3, c);
    }
    if (!p->nobig) {
        char s[12] = "\xc3\x97";
        ui_itoa(s + 2, n);
        big_val(s, 116, top + 1, 50, p->k, 1);
    }
}

/* the security panel: thirty lights (6 x 5) lit by the chance, in an order the shuffle sets */
static void lights(const ui_held_t *v, qd_rgb c, int x0, int y0)
{
    const int lit = (v->chance * 30 + 50) / 100;
    int i;
    px_box(x0 - 2, y0 - 2, 6 * 7 + 3, 5 * 6 + 3, UI_G2);
    for (i = 0; i < 30; i++) {
        const int k = (int)((uint32_t)(i * 7 + (int)(v->shuffle % 30u)) % 30u);
        const int on = k < lit;
        if (on)
            px_rect(x0 + (i % 6) * 7, y0 + (i / 6) * 6, 5, 4, c);
        else
            px_box(x0 + (i % 6) * 7, y0 + (i / 6) * 6, 5, 4, UI_G3);
    }
}

static void passes(const ui_held_t *v, qd_rgb c, int cx, int y)
{
    int k;
    if (v->ab_b <= 0)
        return;
    for (k = 0; k < v->ab_b; k++) {
        const int x = cx - v->ab_b * 6 / 2 + k * 6;
        if (k == v->ab_a - 1)
            px_rect(x, y, 5, 5, c);
        else
            px_box(x, y, 5, 5, c);
        if (k == v->pass)
            px_hline(x, y + 7, 5, PX_LRED);
    }
}

static void f_cond(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    char s[12];
    const int chance_big = v->field == UI_F_CHNC;
    const qd_rgb cc = chance_big ? p->k : p->k2, kc = chance_big ? p->k2 : p->k;
    const int mid = (top + bot) / 2;
    ui_itoa(s, v->chance);
    cat(s, "%");
    if (chance_big) {
        big_val(s, 60 - px_text_w(s, PX_FB, 2) / 2, top + 2, 112, cc, 0);
        lights(v, cc, 40, mid - 6);
        px_ctext(60, bot - 9, v->cond_name, kc, PX_FS, 1);
        passes(v, kc, 60, bot - 2 > mid + 26 ? bot - 2 - 0 : bot - 2);
    } else {
        big_val(v->cond_name, 60 - px_text_w(v->cond_name, PX_FB, 2) / 2, top + 2, 112, kc, 0);
        passes(v, kc, 60, top + 22);
        if (v->cond_text)
            px_ctext(60, top + 32, v->cond_text, UI_G1, PX_FS, 1);
        lights(v, cc, 40, bot - 34);
        px_ctext(60, bot - 6, s, cc, PX_FS, 1);
    }
}

/* a 5.25" floppy: the sleeve, the hub, the read slot, the label with the name */
static void floppy(int x, int y, int w, const char *name, const char *num, qd_rgb col, qd_rgb ncol)
{
    const int h = w;
    px_rect(x, y, w, h, PX_DGREY);
    px_box(x, y, w, h, col);
    px_circle(x + w / 2, y + h / 2 + 2, w / 7 + 1, PX_BLACK, PX_BLACK);
    px_circle(x + w / 2, y + h / 2 + 2, w / 14 + 1, col, QD_NONE);
    px_rect(x + w / 2 - 2, y + h - 2 - h / 4, 4, h / 4, PX_BLACK);
    px_rect(x + 3, y + 3, w - 6, 9, PX_WHITE);
    px_rect(x + w - 6, y + h / 2 - 1, 2, 3, PX_BLACK);
    px_text(x + 5, y + 5, name, PX_BLACK, PX_FS, 1);
    if (num)
        px_text(x + 4, y + h - 8, num, ncol, PX_FS, 1);
}

static void f_sound(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    char n[12];
    const int w = 44, mid = (top + bot) / 2;
    int i;
    if (v->sound_prev)
        px_ctext(60, top + 1, v->sound_prev, UI_G2, PX_FS, 1);
    if (v->sound_next)
        px_ctext(60, bot - 13, v->sound_next, UI_G2, PX_FS, 1);
    floppy(60 - w / 2, mid - w / 2 - 3, w, v->sound_cur ? v->sound_cur : "TRACK", v->sound ? ui_itoa(n, v->sound) : 0,
           p->k, p->k);
    for (i = 0; i < 16; i++) {                          /* the page's sound locks */
        const int x = 12 + i * 6 + (i / 4) * 2;
        if (v->page_sound[i])
            px_rect(x, bot - 4, 4, 2, i == v->held_col ? UI_K3 : UI_MODE_COL[UI_F_SOUND]);
        else
            px_pset(x + 1, bot - 3, i == v->held_col ? UI_K3 : UI_G3);
    }
}

static void terminal(int x, int y, const char *word, int lit, int kind, qd_rgb c)
{
    px_sprite_tint(x, y, PXS_TERMINAL, 0, lit ? c : UI_G2);
    if (lit)
        px_rect(x + 3, y + 3, 8, 3, c);
    if (kind == 1)
        px_rect(x + 5, y + 2, 4, 3, lit ? UI_BG : UI_G2);
    px_ctext(x + 7, y + 15, word, lit ? c : UI_G1, PX_FS, 1);
}

static void f_trig(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    static const char *const W[3] = {"NOTE", "LOCK", "TIE"};
    int i;
    (void)bot;
    for (i = 0; i < 3; i++)
        terminal(14 + i * 34, top + 6, W[i], v->type == i + 1, i, p->k);
    {
        char s[24] = "MIDI FX ";
        char n[12];
        if (v->mfx_grp)
            cat(s, ui_itoa(n, v->mfx_grp));
        else
            cat(s, "OFF");
        if (v->mfx_track)
            cat(s, " TRACK");
        px_ctext(60, top + 34, s, v->mfx_grp ? UI_MODE_COL[UI_F_TRIG] : UI_G1, PX_FS, 1);
        for (i = 0; i < 8; i++)
            if (v->mfx_slot[i])
                px_rect(36 + i * 6, top + 42, 4, 3, (v->mfx_lit >> i) & 1u ? UI_K3 : UI_G1);
    }
}

/* FUNC (FIZZ beta.18's redraw, on the pixel grid): the page's 16 steps as a row, the held step filled; the function
 * as the playhead's route over it: RESTART back to step 1, REVERSE / FORWARD the flow turning, FLIP both, JUMP an arc
 * to its target (another page: the target's row, its number at the left), LEAP the hops, the random ones dotted arcs
 * under a die; NONE the flow running on. The name centred over it */
static int fn_cx(int i) { return 12 + i * 6 + (i / 4) * 2; }   /* a cell's centre */
static void fn_head(int x, int y, qd_rgb c)             /* an arrowhead pointing down at x, y */
{
    px_line(x, y, x - 2, y - 3, c);
    px_line(x, y, x + 2, y - 3, c);
}
static void fn_arc(int a, int b, int h, qd_rgb c, int dash, int ry)   /* dash 1: dotted; 2: a hop, no arrowhead */
{
    const int x0 = fn_cx(a), x1 = fn_cx(b), n = x1 > x0 ? x1 - x0 : x0 - x1, d = x1 > x0 ? 1 : -1;
    int k, py = ry - 2;
    for (k = 1; k <= n; k++) {
        const int y = ry - 2 - h * 4 * k * (n - k) / (n * n > 0 ? n * n : 1);
        if (dash != 1 || (k & 1))
            px_line(x0 + d * (k - 1), py, x0 + d * k, y, c);
        py = y;
    }
    if (dash != 2)
        fn_head(x1, ry - 2, c);
}
static void fn_flow(int s, int dir, qd_rgb c, int dash, int ry)   /* the flow turning at s, running on dir */
{
    const int x = fn_cx(s), y = ry - 12, xe = x + dir * 30, x1 = xe < 8 ? 8 : xe > 112 ? 112 : xe;
    px_vline(x, y + 2, 9, c);
    px_pset(x + dir, y + 1, c);
    if (dash)
        px_dots(x1 < x ? x1 : x + 2, y, x1 < x ? x - x1 - 1 : x1 - x - 1, c, 2);
    else
        px_hline(x1 < x ? x1 : x + 2, y, x1 < x ? x - x1 - 1 : x1 - x - 1, c);
    px_line(x1, y, x1 - dir * 3, y - 2, c);
    px_line(x1, y, x1 - dir * 3, y + 2, c);
}
static void f_func(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int f = v->func, s = v->is_default ? -1 : v->held_col, ry = bot - 10;
    const int other = f >= UI_FN_JUMP && f < UI_FN_LEAP && v->jump_page != (v->step - 1) / 16;
    const qd_rgb c = p->k;
    const uint8_t *trig = other ? v->jump_trig : v->page_trig;
    int i;
    if (v->func_name[0] && !p->nobig)
        px_ctext(60, top + 4, v->func_name, c, PX_FB, 1);
    for (i = 0; i < 16; i++) {                          /* the row */
        const int x = fn_cx(i) - 2;
        const int me = !other && i == s;
        const int tgt = (f >= UI_FN_JUMP && f < UI_FN_LEAP && i == v->jump_step) || (f == 1 && i == 0);
        if (me)
            px_rect(x, ry, 5, 7, c);
        else if (trig[i])
            px_rect(x, ry, 5, 7, UI_G2);
        px_box(x, ry, 5, 7, tgt ? c : me ? c : UI_G3);
    }
    if (other) {
        char n[12];
        px_text(3, ry + 1, ui_itoa(n, v->jump_page + 1), UI_G1, PX_FS, 1);
    }
    if (s < 0 && !other)
        return;
    if (f == 0) {                                       /* NONE: the flow runs on */
        fn_flow(s, 1, UI_G2, 1, ry);
    } else if (f == 1) {                                /* RESTART: up, back along the top, down into step 1 */
        const int x0 = fn_cx(s), xt = fn_cx(0), y = ry - 16;
        px_vline(x0, y + 1, 14, c);
        px_hline(xt, y, x0 - xt + 1, c);
        px_vline(xt, y + 1, 14, c);
        fn_head(xt, ry - 2, c);
    } else if (f == 2 || f == 3) {                      /* REVERSE / FORWARD */
        fn_flow(s, f == 2 ? -1 : 1, c, 0, ry);
    } else if (f == 4) {                                /* FLIP: the other way, whichever */
        fn_flow(s, -1, c, 0, ry);
        fn_flow(s, 1, c, 0, ry);
    } else if (f == 5 || f == 6) {                      /* RANDOM JUMP / RANDOM FUNC: dotted arcs, the die */
        static const int8_t D[3] = {-5, 4, 9};
        for (i = 0; i < 3; i++) {
            const int t = ((s + D[i]) % 16 + 16) % 16;
            fn_arc(s, t, 4 + (t > s ? t - s : s - t), f == 5 ? c : UI_G2, 1, ry);
        }
        ui_icon(f == 5 ? UI_IC_DICE : UI_IC_DICE_FN, 120.f, (float)(2 * (ry - 26)), 1.f, c);
    } else if (f < UI_FN_LEAP) {                        /* JUMP: an arc to the target */
        if (other) {
            const int x1 = fn_cx(v->jump_step);
            px_line(9, ry - 22, x1, ry - 22, c);
            px_vline(x1, ry - 22, 20, c);
            fn_head(x1, ry - 2, c);
        } else {
            const int d = v->jump_step > s ? v->jump_step - s : s - v->jump_step;
            fn_arc(s, v->jump_step, 8 + d, c, 0, ry);
        }
    } else {                                            /* LEAP n: n hops (many: one arc) */
        const int nn = f - UI_FN_LEAP - 15, an = nn < 0 ? -nn : nn, dir = nn < 0 ? -1 : 1;
        if (an <= 6)
            for (i = 0; i < an; i++)
                fn_arc(((s + dir * i) % 16 + 16) % 16, ((s + dir * (i + 1)) % 16 + 16) % 16, 5, c, i + 1 < an ? 2 : 0,
                       ry);
        else
            fn_arc(s, ((s + nn) % 16 + 16) % 16, 8 + an, c, 0, ry);
    }
}

static void f_slide(const ui_held_t *v, const pal_t *p, int top, int bot)
{
    const int y1 = bot - 10, y2 = top + 14, x1 = 10, x2 = 70;
    const int tw = 8 + v->ptime * 40 / 127;
    int i;
    for (i = 0; i < 4; i++)
        px_sprite(x1 + i * 8, y1, PXS_LIFT, 0);
    for (i = 0; i < 5; i++)
        px_sprite(x2 + i * 8, y2, PXS_LIFT, 0);
    if (v->port >= 1) {                                 /* the ramp: from the first platform's end, TIME long */
        px_line(x1 + 32, y1, x1 + 32 + tw, y2 + 1, v->port ? p->k : UI_G2);
        px_line(x1 + 32, y1 + 1, x1 + 32 + tw, y2 + 2, v->port ? p->k : UI_G2);
    }
    if (v->port == 2)                                   /* LEGATO: the note held on under it */
        px_dots(x1 + 32, y1 + 3, tw + 4, p->k2, 2);
    px_sprite(x1 + 2, y1 - 16, p->empty ? PXS_AGENT_STAND : PXS_AGENT_RUN1, 0);
    {
        char s[24] = "TIME ", n[12];
        cat(s, ui_itoa(n, v->ptime));
        px_text(x2, y2 + 6, s, UI_G1, PX_FS, 1);
    }
}

static void body(const ui_held_t *v, const pal_t *p, int f, int top, int bot)
{
    switch (f) {
    case UI_F_NOTE: f_note(v, p, top, bot); break;
    case UI_F_VEL: f_vel(v, p, top, bot); break;
    case UI_F_LEN: f_len(v, p, top, bot); break;
    case UI_F_MICRO: f_micro(v, p, top, bot); break;
    case UI_F_RATCH: f_ratch(v, p, top, bot); break;
    case UI_F_COND:
    case UI_F_CHNC: f_cond(v, p, top, bot); break;
    case UI_F_SOUND: f_sound(v, p, top, bot); break;
    case UI_F_TRIG: f_trig(v, p, top, bot); break;
    case UI_F_FUNC: f_func(v, p, top, bot); break;
    case UI_F_SLIDE: f_slide(v, p, top, bot); break;
    default: break;
    }
}

/* ---------------------------------------------------------------- the head */
static void head_left(const ui_held_t *v, int f, char *left)
{
    char n[12];
    left[0] = 0;
    if (v->is_default) {
        cat(left, "TRACK DEFAULT");
    } else {
        cat(cat(left, "STEP "), ui_itoa(n, v->step));
        if (v->nheld > 1)
            cat(cat(left, "+"), ui_itoa(n, v->nheld - 1));
    }
    cat(cat(left, " \xc2\xb7 "), UI_FIELD_NAME[f]);
}

static int frame_and_head(const ui_held_t *v, pal_t *p, int edit)
{
    const int f = held_pal(v, p);
    const int lit = v->is_default || v->ovr;
    char left[48];
    if (!v->is_default && !v->follow) {                 /* a held step: the purple frame, 2 px */
        px_box(0, 0, 120, 120, UI_STEP_FRAME);
        px_box(1, 1, 118, 118, UI_STEP_FRAME);
    }
    if (v->row)
        return f;
    head_left(v, f, left);
    {
        const int x = px_text(4, 4, left, v->is_default ? UI_TRACK_COL : UI_K3, PX_FS, 1);
        if ((!lit && !p->empty) || (p->empty && (f == UI_F_COND || f == UI_F_CHNC))) {
            const char *tag = p->empty ? "EMPTY" : "DEFAULT";
            if (edit)
                px_text(x + 4, 4, tag, UI_G1, PX_FS, 1);
            else
                px_rtext(115, 4, tag, UI_G1, PX_FS, 1);
        }
    }
    return f;
}

static void reset_icon(const ui_held_t *v, int y)
{
    ui_icon(UI_IC_RESET, 224.f, (float)(2 * y), 1.f, v->reset == 1 ? UI_K3 : UI_G3);
}

void ui_held_draw(const ui_held_t *v)
{
    pal_t p;
    const int f = frame_and_head(v, &p, 0);
    p.nobig = v->band;                                  /* (the knob band over the top: its value there) */
    body(v, &p, f, 12, 116);
    if (v->is_default && v->reset)
        reset_icon(v, 6);
}

/* ---------------------------------------------------------------- the EDIT view: the picture in the top area (13..88),
 * the value at the head's right, the page's row at the foot */
static const char *const PORT_NAMES[3] = {"OFF", "ON", "LEGATO"};
static void head_val(const ui_held_t *v, int f, char *val)
{
    static const char *const TRIGS[4] = {"", "NOTE", "LOCK", "TIE"};
    char n[12];
    val[0] = 0;
    switch (f) {
    case UI_F_NOTE:
        if (v->root[0])
            cat(cat(val, v->root), v->qual);
        break;
    case UI_F_VEL: cat(val, ui_itoa(n, v->vel)); break;
    case UI_F_LEN: cat(val, v->len_name ? v->len_name : ""); break;
    case UI_F_MICRO: cat(cat(val, v->micro > 0 ? "+" : ""), ui_itoa(n, v->micro)); break;
    case UI_F_RATCH: cat(cat(val, "\xc3\x97"), ui_itoa(n, v->count < 1 ? 1 : v->count > 8 ? 8 : v->count)); break;
    case UI_F_TRIG: cat(val, TRIGS[v->type >= 1 && v->type <= 3 ? v->type : 0]); break;
    case UI_F_SOUND: cat(val, v->sound_cur ? v->sound_cur : "TRACK SOUND"); break;
    case UI_F_FUNC: cat(val, v->func_name); break;
    case UI_F_SLIDE: cat(val, PORT_NAMES[v->port < 0 ? 0 : v->port > 2 ? 2 : v->port]); break;
    default: break;
    }
}

#include "ui_room.c"                    /* EvilGenius: THE ROOM (built here: the source lists stay) */

void ui_edit_draw(const ui_held_t *v, const ui_lane_t *lane)
{
    pal_t p, hp;
    const ui_held_t *hv = v->head ? v->head : v;
    char val[40], left[48];
    int f;
    if (v->room) {                                      /* EvilGenius: nothing held: THE ROOM over the page's row */
        ui_room_draw(lane);
        ui_steps_row(lane, 180.f, 58.f);
        return;
    }
    f = frame_and_head(hv, &hp, 1);
    p = hp;
    if (hv != v)
        held_pal(v, &p);
    p.nobig = f != UI_F_COND && f != UI_F_CHNC && f != UI_F_TRIG;
    qd_save();
    qd_clip_rect(0.f, 22.f, 240.f, 158.f);
    if (v->dim)
        qd_alpha(qd_get_alpha() * 0.6f);
    body(v, &p, f, 13, 88);
    qd_restore();
    head_val(hv, f, val);
    if (hp.empty) {
        val[0] = 0;
        cat(val, "EMPTY STEP");
    }
    if (hv->row) {                                      /* the page's step row in the head's place, the value under it */
        const int pg = lane->page >= 0 && lane->page < 8 ? lane->page : 0;
        uint16_t trig = 0, sel = 0;
        int i;
        for (i = 0; i < 16; i++) {
            trig |= (uint16_t)(((lane->cell[pg][i] & UI_C_ON) != 0) << i);
            sel |= (uint16_t)(((lane->cell[pg][i] & UI_C_HELD) != 0) << i);
        }
        ui_page_row(6.f, trig, sel, -1, lane->play_step >= 0 && lane->play_step / 16 == pg ? lane->play_step % 16 : -1);
        if (val[0] && f != UI_F_COND && f != UI_F_CHNC)
            px_rtext(115, 11, val, hp.empty ? UI_G1 : hp.k, PX_FS, 1);
    } else if (val[0] && p.nobig) {
        head_left(hv, f, left);
        {
            const int room = 112 - px_text_w(left, PX_FS, 1) - 6;
            if (px_text_w(val, PX_FB, 1) <= room)
                px_rtext(115, 2, val, hp.empty ? UI_G1 : hp.k, PX_FB, 1);
            else if (px_text_w(val, PX_FS, 1) <= room)
                px_rtext(115, 4, val, hp.empty ? UI_G1 : hp.k, PX_FS, 1);
        }
    }
    if (f == UI_F_NOTE && v->scale_name && v->scale_name[0] && !hv->row)
        px_text(4, 12, v->scale_name, UI_G1, PX_FS, 1);
    if (v->is_default && v->reset)
        reset_icon(v, 18);
    ui_steps_row(lane, 180.f, 58.f);
}
