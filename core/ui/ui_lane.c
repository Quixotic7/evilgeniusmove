/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the step view (FIZZ's ui_lane.c, redrawn on the 120 x 120 grid; the model, ui_lane_t, is FIZZ's).
 *
 * The frame: the head (the track's box, its name, the mode in its colour, the pattern), the eight pages as segments
 * under it (each over its own start..end; the edited one white and thick, the loop grey, out of it dark blue; the
 * playhead's place red), the sixteen columns in fours (dark blue shafts; a held one white; outside the page's range a
 * dotted shaft), the beats as dots over the columns, the red playhead, the default named at the foot.
 *
 * The lanes, a step that overrides the mode in the mode's colour, on the track's default grey, the playhead's white; a
 * step not playing half-toned (the C64's checker), the playing one solid:
 *   NOTE  the piano roll           VEL   bars up from the floor       LEN   bars over the steps, a row a step
 *   MICRO flags off the grid       RATCH the hits as a ladder          CHNC  ten lights up the shaft
 *   COND  A:B rings, names stacked TRIG  NOTE a block, LOCK a chip, TIE a hook     SOUND the pool number
 *   FUNC  the function's icon      SLIDE a ramp to the next step
 * A trigless LOCK step: a small ring. */
#include <string.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"

#define TOP 17                                          /* the shafts' top and bottom */
#define BOT 104
#define CW 6                                            /* a shaft's width */

static int cx_(int i) { return 4 + i * 7 + (i / 4); }    /* a shaft's left (in fours, a pixel between) */
#define XEND (cx_(15) + CW)

static qd_rgb def_col = UI_G1;

static void def_line(int y) { px_dots(cx_(0), y, XEND - cx_(0), def_col, 2); }

static int ramp_vel(int v, int ramp, int h, int n)
{
    int x = v;
    if (n > 1) {
        if (ramp == 1)
            x = v * (h + 1) / n;
        else if (ramp == 2)
            x = v * (n - h) / n;
        else if (ramp == 3)
            x = h ? v * 5 / 8 : v;
    }
    return x < 1 ? 1 : x > 127 ? 127 : x;
}

static qd_rgb ccol(const ui_lane_t *v, const ui_lcol_t *c)
{
    return c->lit ? UI_K3 : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
}

/* a bar: solid while it plays, else outlined and half-toned */
static void bar(int x, int y, int w, int h, qd_rgb col, int lit)
{
    if (h < 1 || w < 1)
        return;
    if (lit || w < 3 || h < 3) {
        px_rect(x, y, w, h, col);
        return;
    }
    px_box(x, y, w, h, col);
    px_checker(x + 1, y + 1, w - 2, h - 2, col, 0);
}

static void lock_ring(int cx, int y, const ui_lcol_t *c)
{
    px_circle(cx, y, 1, c->held ? UI_K3 : UI_G1, QD_NONE);
}

/* ---------------------------------------------------------------- NOTE: the piano roll */
static void l_note(const ui_lane_t *v)
{
    const int span = v->span > 0 ? v->span : 16;
    int i, j, r;
#define NY(n) (BOT - 4 - ((int)(n) - v->lo) * (BOT - TOP - 8) / span)
    for (j = 0; j < v->def_nnote && j < 6; j++) {
        const int y = NY(v->def_note[j]);
        if (y >= TOP + 1 && y <= BOT - 2)
            def_line(y);
    }
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx_(i) + 3, (TOP + BOT) / 2, c);
            continue;
        }
        {
            const int x0 = cx_(i) + c->micro * 7 / 24;
            const int nr = c->ratch > 1 ? c->ratch : 1;
            const qd_rgb col = c->lit ? UI_K3 : c->ovr ? UI_MODE_COL[v->mode] : UI_G1;
            int w = (int)(c->len * 7.f) - 1;
            if (!(w < 118 - x0))
                w = 118 - x0;
            if (w < 2)
                w = 2;
            for (j = 0; j < c->nnote && j < 6; j++) {
                const int y = NY(c->note[j]);
                if (y < TOP || y > BOT - 2)
                    continue;
                if (nr > 1) {
                    for (r = 0; r < nr; r++) {
                        const int pw = w / nr - 1;
                        px_rect(x0 + r * w / nr, y, pw < 1 ? 1 : pw, 2, col);
                    }
                } else {
                    px_rect(x0, y, w, 2, col);
                }
                if (c->held && !c->lit)
                    px_box(x0 - 1, y - 1, w + 2, 4, UI_K3);
            }
        }
    }
#undef NY
    for (i = 0; i < 16; i++) {                          /* under a column: its other overrides, a pixel each */
        const ui_lcol_t *c = &v->col[i];
        int x = cx_(i);
        if (!c->in_range || !c->on)
            continue;
        if (c->chance != v->def_chance)
            px_pset(x++, BOT + 2, UI_MODE_COL[UI_F_CHNC]);
        if (strcmp(c->cond, v->def_cond) != 0)
            px_pset(x++, BOT + 2, UI_MODE_COL[UI_F_COND]);
        if (c->sound)
            px_pset(x++, BOT + 2, UI_MODE_COL[UI_F_SOUND]);
        if (c->nlock)
            px_pset(x, BOT + 2, UI_LOCK_COL);
    }
}

/* ---------------------------------------------------------------- SLIDE: a ramp from the step towards the next */
static void l_slide(const ui_lane_t *v)
{
    const int y = 62;
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i) + 2;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->port >= 2) {
            const int w = 2 + 5 * c->ptime / 127;
            px_line(x, y + 3, x + w, y - 5, col);
            if (c->port == 3)
                px_hline(x, y + 5, w + 2, col);
            px_rect(x, y + 2, 2, 2, col);
        } else if (c->port == 1) {
            px_box(x - 1, y + 1, 4, 4, col);
        } else {
            px_rect(x, y + 2, 2, 2, col);
        }
    }
}

/* ---------------------------------------------------------------- VEL: bars up from the floor */
static void l_vel(const ui_lane_t *v)
{
    const int floor = BOT - 1, H = BOT - TOP - 6;
    int i;
    def_line(floor - v->def_vel * H / 127);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        int h;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx_(i) + 3, floor - 3, c);
            continue;
        }
        h = c->vel * H / 127;
        if (h < 3)
            h = 3;
        bar(cx_(i) + 1, floor - h, 4, h, ccol(v, c), c->lit);
        px_hline(cx_(i), floor - h, CW, ccol(v, c));    /* a lift's platform on top */
    }
}

/* ---------------------------------------------------------------- LEN: bars over the steps, a row a step */
static void len_bar(int x, int y, float len, qd_rgb col, int lit, int ghost)
{
    int w = (int)(len * 7.f) - 1, cut = 0;
    if (!(len < 64.f) || x + w > XEND)
        w = XEND - x, cut = 1;
    if (w < 2)
        w = 2;
    if (ghost) {
        px_dots(x, y + 1, w, col, 2);
        return;
    }
    bar(x, y, w, 3, col, lit);
    if (cut) {
        px_pset(XEND + 1, y, col);
        px_pset(XEND + 2, y + 1, col);
        px_pset(XEND + 1, y + 2, col);
    }
}

static void l_len(const ui_lane_t *v)
{
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i), y = TOP + 3 + i * 5;
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(x + 3, y + 1, c);
            continue;
        }
        if (c->ovr)
            len_bar(x, y, v->def_len, UI_G2, 0, 1);
        len_bar(x, y, c->len, ccol(v, c), c->lit, 0);
    }
}

/* ---------------------------------------------------------------- MICRO: the start flags off the grid */
static void l_micro(const ui_lane_t *v)
{
    const int top = 36, foot = BOT - 4;
    int i;
    for (i = 0; i < 16; i++) {
        const int gx = cx_(i) + 2;
        if (!v->col[i].in_range)
            continue;
        px_vline(gx, foot - 1, 3, UI_G2);
        px_vdots(gx + v->def_micro * 7 / 24, top + 4, foot - top - 4, v->def_hot ? def_col : UI_G3, 3);
    }
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int gx = cx_(i) + 2, sx = gx + c->micro * 7 / 24;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(gx, foot - 5, c);
            continue;
        }
        px_vline(sx, top, foot - top, col);
        px_rect(sx + 1, top, 3, 2, col);                /* the flag */
        px_pset(sx + 1, top + 2, col);
        if (c->micro)
            px_hline(gx < sx ? gx : sx, foot, (sx > gx ? sx - gx : gx - sx) + 1, col);
        px_pset(gx, foot + 1, col);
    }
}

/* ---------------------------------------------------------------- RATCH: the hits as a ladder */
static void l_ratch(const ui_lane_t *v)
{
    const int foot = BOT - 4, sp = 8;
    int i, h;
    def_line(foot - ((v->def_ratch < 1 ? 1 : v->def_ratch) - 1) * sp - sp / 2);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int n = c->ratch < 1 ? 1 : c->ratch > 8 ? 8 : c->ratch;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            lock_ring(cx_(i) + 3, foot, c);
            continue;
        }
        for (h = 0; h < n; h++) {
            const int w = 2 + 4 * ramp_vel(127, c->ramp, h, n) / 127;
            px_rect(cx_(i) + 3 - w / 2, foot - h * sp, w, 2, col);
        }
    }
}

/* ---------------------------------------------------------------- CHNC: ten lights up the shaft */
static void l_chnc(const ui_lane_t *v)
{
    const int foot = BOT - 4, sp = 8;
    int i, d;
    def_line(foot - ((v->def_chance + 5) / 10) * sp + sp / 2 + 1);
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int nl = (c->chance + 5) / 10;
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        for (d = 0; d < 10; d++) {
            const int y = foot - d * sp;
            if (d < nl)
                px_rect(cx_(i) + 1, y - 1, 4, 3, col);
            else
                px_pset(cx_(i) + 2, y, UI_G3);
        }
    }
}

/* ---------------------------------------------------------------- COND */
static int parse_ab(const char *s, int *a, int *b)
{
    if (s[0] >= '1' && s[0] <= '9' && s[1] == ':' && s[2] >= '1' && s[2] <= '9' && !s[3]) {
        *a = s[0] - '0', *b = s[2] - '0';
        return 1;
    }
    return 0;
}

static void l_cond(const ui_lane_t *v)
{
    const int foot = BOT - 5;
    int i, a, b, k;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i);
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (!c->cond[0]) {
            px_rect(x + 2, foot, 2, 2, col);
        } else if (parse_ab(c->cond, &a, &b)) {
            for (k = 0; k < b; k++) {
                if (k == a - 1)
                    px_rect(x + 1, foot - k * 6 - 1, 4, 4, col);
                else
                    px_box(x + 1, foot - k * 6 - 1, 4, 4, col);
            }
        } else {
            const int n = (int)strlen(c->cond);
            char ch[2];
            ch[1] = 0;
            for (k = 0; k < n; k++) {
                ch[0] = c->cond[k];
                px_text(x + 1, foot - (n - 1 - k) * 7 - 3, ch, col, PX_FS, 1);
            }
        }
    }
}

/* ---------------------------------------------------------------- TRIG */
static void l_trig(const ui_lane_t *v)
{
    const int y = 60;
    int i;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i);
        const qd_rgb col = ccol(v, c);
        if (!c->in_range || !c->on)
            continue;
        if (c->type == 2) {
            px_rect(x + 1, y, 4, 3, col);
        } else if (c->type == 3) {                      /* a hook from the step before */
            px_hline(x - 4, y - 4, 6, col);
            px_pset(x - 5, y - 3, col);
            px_pset(x + 2, y - 3, col);
            px_rect(x + 2, y, 2, 2, col);
        } else {
            px_rect(x + 1, y - 1, 4, 4, col);
        }
    }
}

/* ---------------------------------------------------------------- SOUND: a dot a step, the pool number */
static void l_sound(const ui_lane_t *v)
{
    const int y = 50;
    char s[12];
    int i, k;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i);
        const qd_rgb col = c->lit ? UI_K3 : c->sound ? UI_MODE_COL[UI_F_SOUND] : UI_G1;
        char ch[2];
        if (!c->in_range || !c->on)
            continue;
        if (!c->sound) {
            px_box(x + 1, y, 4, 4, col);
            continue;
        }
        px_rect(x + 1, y, 4, 4, col);
        ui_itoa(s, c->sound);
        ch[1] = 0;
        for (k = 0; s[k]; k++) {
            ch[0] = s[k];
            px_text(x + 1, y + 8 + k * 7, ch, col, PX_FS, 1);
        }
    }
}

/* ---------------------------------------------------------------- FUNC: an icon a step */
static void l_func(const ui_lane_t *v)
{
    const int y = 50;
    char s[12];
    int i, k;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int x = cx_(i);
        const qd_rgb col = c->lit ? UI_K3 : c->func ? UI_MODE_COL[UI_F_FUNC] : UI_G2;
        char ch[2];
        if (!c->in_range || !c->on)
            continue;
        if (!c->func) {
            px_pset(x + 3, y + 3, UI_G2);
            continue;
        }
        ui_icon(ui_fn_icon(c->func), (float)(2 * (x + 3) + 1), (float)(2 * (y + 3) + 1), 1.f, col);
        if (c->func >= UI_FN_JUMP) {
            if (c->func >= UI_FN_LEAP) {
                const int n = c->func - UI_FN_LEAP - 15;
                s[0] = n < 0 ? '-' : '+';
                ui_itoa(s + 1, n < 0 ? -n : n);
            } else {
                ui_itoa(s, c->func - UI_FN_JUMP + 1);
            }
            ch[1] = 0;
            for (k = 0; s[k]; k++) {
                ch[0] = s[k];
                px_text(x + 1, y + 10 + k * 7, ch, col, PX_FS, 1);
            }
        }
    }
}

/* ---------------------------------------------------------------- the lane */
static void lane(const ui_lane_t *v)
{
    const int ph = v->pos >= 0.f ? (int)v->pos : -1;
    int i;
    for (i = 0; i < 16; i++) {                          /* the shafts */
        const ui_lcol_t *c = &v->col[i];
        if (!c->in_range) {
            px_vdots(cx_(i), TOP, BOT - TOP, UI_G3, 3);
            px_vdots(cx_(i) + CW - 1, TOP, BOT - TOP, UI_G3, 3);
            continue;
        }
        px_box(cx_(i), TOP, CW, BOT - TOP, c->held ? UI_K3 : UI_G3);
    }
    switch (v->mode) {
    case UI_F_VEL: l_vel(v); break;
    case UI_F_LEN: l_len(v); break;
    case UI_F_MICRO: l_micro(v); break;
    case UI_F_RATCH: l_ratch(v); break;
    case UI_F_CHNC: l_chnc(v); break;
    case UI_F_COND: l_cond(v); break;
    case UI_F_TRIG: l_trig(v); break;
    case UI_F_SOUND: l_sound(v); break;
    case UI_F_FUNC: l_func(v); break;
    case UI_F_SLIDE: l_slide(v); break;
    default: l_note(v); break;
    }
    if (ph >= 0 && ph < 16)                             /* the playhead */
        px_vline(cx_(ph) + (int)((v->pos - (float)ph) * 7.f), TOP - 2, BOT - TOP + 4, PX_LRED);
    for (i = 0; i < 16; i += 4)                         /* the beats */
        px_rect(cx_(i) + 2, TOP - 3, 2, 1, i ? UI_G2 : UI_G1);
}

/* ---------------------------------------------------------------- STEPS: the track's eight pages */
/* The eight pages as rows of sixteen cells in fours, the page edited unfolded (tall cells with the notes as dark bars,
 * the fill's height its velocity), the others folded. Own notes white, the track's cyan, a ghost (LOCK) a purple ring,
 * off an outline; the pages out of the loop half-toned; the playhead a red marker over the open row, a red tick under a
 * folded cell; the held / selected outlined white */
#define OV_X0 10
#define OV_TOP 16
#define OV_FOLD 10
#define OV_OPEN 31
static int ov_x(int s) { return OV_X0 + s * 6 + (s / 4) * 2; }
#define OV_W 5

qd_rgb ui_steps_fill(uint8_t c)
{
    if (!(c & UI_C_RANGE) || !(c & UI_C_ON) || (c & UI_C_LOCK))
        return QD_NONE;
    return (c & UI_C_OWN) ? UI_STEP_OWN : UI_STEP_TRACK;
}
qd_rgb ui_steps_ink(uint8_t c) { return (c & UI_C_OWN) ? PX_DGREY : PX_BLUE; }

static int steps_row(const ui_lane_t *v, int p, int y, float g, int open)
{
    static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
    const qd_rgb mc = UI_MODE_COL[v->mode];
    const int ph = v->play_step;
    const int rowh = OV_FOLD + (int)((float)(open - OV_FOLD) * g), ch = rowh - 2 - (int)(6.f * g);
    const int in = (v->loop >> p) & 1, ed = p == v->page, op = g >= 0.5f;
    const int cy = y + (g > 0.f ? (int)(5.f * g) : 0);
    int i, j;
    px_text(2, op ? cy + 2 : cy + ch / 2 - 2, NUM[p], ed ? UI_K3 : in ? UI_G1 : UI_G3, PX_FS, 1);
    for (i = 0; i < 16; i++) {
        const uint8_t c = v->cell[p][i];
        const int abs = p * 16 + i, x = ov_x(i);
        const int on = (c & UI_C_ON) != 0, lock = (c & UI_C_LOCK) != 0;
        const qd_rgb fill = ui_steps_fill(c);
        const ui_lcol_t *lc = op && ed ? &v->col[i] : 0;
        const int pop = abs == v->flash_step && v->flash_age >= 0.f && v->flash_age < 0.3f && on;
        if (!(c & UI_C_RANGE)) {
            px_pset(x + 2, cy + ch / 2, UI_G3);
            continue;
        }
        if (fill != QD_NONE) {
            const int vh = lc ? ch * (lc->vel < 15 ? 15 : lc->vel) / 127 : ch;
            if (vh < ch)
                px_box(x, cy, OV_W, ch, UI_G2);
            if (in || ed)
                px_rect(x, cy + ch - vh, OV_W, vh, pop ? UI_K3 : fill);
            else
                px_checker(x, cy + ch - vh, OV_W, vh, fill, 0);
            if (lc && lc->nnote) {                      /* the notes as dark bars (red as they sound) */
                const int span = v->span > 0 ? v->span : 16;
                for (j = 0; j < lc->nnote && j < 6; j++) {
                    const int ny = cy + ch - 3 - ((int)lc->note[j] - v->lo) * (ch - 5) / span;
                    if (ny < cy + 1 || ny > cy + ch - 2)
                        continue;
                    px_hline(x + 1, ny, OV_W - 2, pop ? PX_LRED : ui_steps_ink(c));
                }
            }
        } else if (on && lock) {
            px_box(x, cy, OV_W, ch, UI_STEP_GHOST);
            px_pset(x + 2, cy + ch / 2, UI_STEP_GHOST);
        } else {
            px_box(x, cy, OV_W, ch, op ? UI_G2 : UI_G3);
        }
        if (on && (c & UI_C_OVR) && v->mode != UI_F_NOTE)
            px_hline(x + 1, cy + ch + 1, OV_W - 2, mc);
        if (c & UI_C_TIE)
            px_hline(x - 2, cy + 1, 3, UI_MODE_COL[UI_F_TRIG]);
        if (c & UI_C_FUNC)
            px_pset(x + OV_W - 1, cy, UI_MODE_COL[UI_F_FUNC]);
        if (abs == ph && !op)
            px_hline(x, cy + ch, OV_W, PX_LRED);
        if (c & UI_C_HELD)
            px_box(x - 1, cy - 1, OV_W + 2, ch + 2, UI_K3);
    }
    if (op && ph >= p * 16 && ph < p * 16 + 16) {       /* the marker over the open row */
        const int hx = ov_x(ph % 16) + 2;
        px_hline(hx - 1, cy - 4, 3, PX_LRED);
        px_pset(hx, cy - 3, PX_LRED);
    }
    return rowh;
}

static void steps_grid(const ui_lane_t *v)
{
    int y = OV_TOP, p;
    for (p = 0; p < 8; p++)
        y += steps_row(v, p, y, v->grow[p] < 0.f ? 0.f : v->grow[p] > 1.f ? 1.f : v->grow[p], OV_OPEN);
}

/* EDIT's row: the page edited alone, unfolded, at y (240-space, as FIZZ's callers give it) */
void ui_steps_row(const ui_lane_t *v, float y, float open)
{
    const int p = v->page >= 0 && v->page < 8 ? v->page : 0;
    steps_row(v, p, (int)(y * 0.5f), 1.f, (int)(open * 0.5f));
}

static void def_name(const ui_lane_t *v)
{
    const int sw = v->mode == UI_F_VEL || v->mode == UI_F_LEN || v->mode == UI_F_MICRO || v->mode == UI_F_RATCH ||
                   v->mode == UI_F_CHNC;
    int x = cx_(0);
    if (v->mode == UI_F_NOTE || !v->def_label[0])
        return;
    if (sw) {
        px_dots(x, 111, 6, def_col, 2);
        x += 8;
    }
    px_text(x, 109, v->def_label, def_col, PX_FS, 1);
}

void ui_lane_draw(const ui_lane_t *v)
{
    const int mode = v->mode >= 0 && v->mode < UI_NFIELD ? v->mode : UI_F_NOTE;
    ui_lane_t m;
    int mx, p;

    m = *v;
    m.mode = mode;
    v = &m;
    def_col = v->def_hot ? UI_MODE_COL[mode] : UI_G1;

    ui_head(v->name, v->pattern, v->track, UI_K3, UI_G1);
    mx = (int)(ui_head_x(v->track) * 0.5f) + px_text_w(v->name, PX_FS, 1) + 5;
    mx = px_text(mx, 4, UI_FIELD_NAME[mode], UI_MODE_COL[mode], PX_FS, 1);

    for (p = 0; p < 8 && !v->steps; p++) {              /* the pages: each over its start..end */
        const int x = 5 + p * 14, W = 11;
        const int s = v->pstart[p] > 15 ? 15 : v->pstart[p], e = v->pend[p] < s ? s : v->pend[p] > 15 ? 15 : v->pend[p];
        const int xs = x + s * W / 16, xe = x + (e + 1) * W / 16;
        const int in = (v->loop >> p) & 1, ed = p == v->page;
        if (s > 0 || e < 15)
            px_dots(x, 12, W, UI_G3, 2);
        px_rect(xs, ed ? 11 : 12, xe - xs, ed ? 2 : 1, ed ? UI_K3 : in ? UI_G1 : UI_G3);
        if (p == v->play_page) {
            if (ed && v->pos >= 0.f)
                px_vline(x + (int)(v->pos / 16.f * (float)W), 10, 4, PX_LRED);
            else
                px_rect((xs + xe) / 2 - 1, 12, 3, 1, PX_LRED);
        }
    }

    if (v->slide != 0.f && !v->steps) {                 /* a page change: the lane slides in */
        qd_save();
        qd_translate((float)(2 * (int)(v->slide * 20.f)), 0.f);
        lane(v);
        qd_restore();
    } else if (v->steps) {
        steps_grid(v);
    } else {
        lane(v);
    }
    if (!v->steps)
        def_name(v);
    if (v->latch)
        mx = px_text(mx + 4, 4, "LATCH", UI_K3, PX_FS, 1);
    if (!v->follow) {                                   /* page follow off: an arrow into a bar, crossed */
        const int x = 116 - px_text_w(v->pattern ? v->pattern : "", PX_FS, 1) - 9, y = 5;
        px_hline(x - 3, y + 1, 4, UI_G1);
        px_vline(x + 2, y - 2, 6, UI_G1);
        px_line(x - 3, y + 3, x + 4, y - 2, UI_K4);
    }
}
