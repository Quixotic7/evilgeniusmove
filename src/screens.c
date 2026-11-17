/* SPDX-License-Identifier: MPL-2.0 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "belt_art.h"

#include "screens.h"

/* ------------------------------------------------------------------ the brain in the jar --- */
static void brain_shape(mono_t *m, int grow, int on)
{
    static const int lobes[7][3] = { {47, 19, 6}, {54, 13, 6}, {64, 11, 6}, {74, 13, 6}, {81, 19, 6}, {45, 26, 5}, {83, 26, 5} };
    int i;
    mono_disc(m, 64, 22, 21 + grow, 12 + grow, on);
    mono_disc(m, 64, 33, 15 + grow, 9 + grow, on);      /* the jaw's part */
    for (i = 0; i < 7; i++)
        mono_disc(m, lobes[i][0], lobes[i][1], lobes[i][2] + grow, lobes[i][2] + grow, on);
}

static void bolt(mono_t *m, const int *p, int n)
{
    int i;
    for (i = 0; i + 1 < n; i++)
        mono_line(m, p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], 1);
}

void scr_intro(mono_t *m, uint32_t ms)
{
    static const int arc[] = { 45, 4, 50, 1, 55, 4, 60, 0, 64, 4, 68, 0, 73, 4, 78, 1, 83, 4 };
    static const int l1[] = { 32, 10, 23, 14, 28, 16, 17, 21 }, l2[] = { 33, 21, 25, 24, 29, 26, 19, 31 };
    static const int r1[] = { 96, 10, 105, 14, 100, 16, 111, 21 }, r2[] = { 95, 21, 103, 24, 99, 26, 109, 31 };
    static const int stars[8][2] = { {7, 5}, {14, 24}, {25, 3}, {103, 4}, {119, 11}, {123, 27}, {3, 36}, {113, 30} };
    const int jaw = (int)((ms / 170u) & 1u);            /* the laugh: the jaw drops and comes back */
    const int flick = (int)((ms / 90u) % 5u);           /* the lightning: one frame in five dark */
    const int wave = (int)((ms / 120u) % 8u);
    int i, x;
    mono_clear(m);
    /* the jar: its liquid a half tone with bubbles rising, its glass a line, its lid and the two electrodes */
    mono_dither(m, 37, 8, 54, 38, 2);
    mono_box(m, 36, 7, 56, 40, 1);
    mono_pset(m, 36, 7, 0); mono_pset(m, 91, 7, 0);
    mono_pset(m, 37, 8, 1); mono_pset(m, 90, 8, 1);
    mono_rect(m, 50, 4, 28, 3, 1);
    mono_rect(m, 56, 2, 16, 2, 1);
    mono_line(m, 51, 5, 46, 3, 1);
    mono_line(m, 77, 5, 82, 3, 1);
    mono_disc(m, 44, 3, 2, 2, 1);
    mono_disc(m, 84, 3, 2, 2, 1);
    if (flick != 0)
        bolt(m, arc, 9);
    for (i = 0; i < 4; i++) {
        const int by = 44 - (int)((ms / 70u + (uint32_t)i * 13u) % 34u);
        mono_box(m, i < 2 ? 39 + i * 3 : 85 + (i - 2) * 3, by, 3, 3, 1);
    }
    /* the brain: a black edge against the liquid, then white, its folds */
    brain_shape(m, 2, 0);
    brain_shape(m, 0, 1);
    mono_line(m, 64, 6, 64, 15, 0);
    mono_line(m, 54, 8, 52, 11, 0); mono_line(m, 52, 11, 55, 14, 0);
    mono_line(m, 74, 8, 76, 11, 0); mono_line(m, 76, 11, 73, 14, 0);
    mono_line(m, 44, 17, 47, 20, 0); mono_line(m, 84, 17, 81, 20, 0);
    mono_line(m, 59, 9, 60, 13, 0); mono_line(m, 69, 9, 68, 13, 0);
    /* the brows, low and mean, over eyes squeezed shut with laughing */
    mono_line(m, 49, 15, 59, 18, 0); mono_line(m, 49, 16, 59, 19, 0);
    mono_line(m, 79, 15, 69, 18, 0); mono_line(m, 79, 16, 69, 19, 0);
    mono_line(m, 51, 23, 54, 20, 0); mono_line(m, 54, 20, 57, 20, 0); mono_line(m, 57, 20, 60, 23, 0);
    mono_line(m, 52, 23, 54, 21, 0); mono_line(m, 57, 21, 59, 23, 0);
    mono_line(m, 68, 23, 71, 20, 0); mono_line(m, 71, 20, 74, 20, 0); mono_line(m, 74, 20, 77, 23, 0);
    mono_line(m, 69, 23, 71, 21, 0); mono_line(m, 74, 21, 76, 23, 0);
    /* the mouth: a grin as wide as the brain, two rows of teeth */
    for (x = 48; x <= 80; x++) {
        const int d = (x - 64) * (x - 64);              /* 0 .. 256 */
        const int top = 24 + 3 * (256 - d) / 256, bot = 26 + (13 + jaw) * (256 - d) / 256;
        if (bot > top)
            mono_rect(m, x, top, 1, bot - top + 1, 0);
        if (x >= 51 && x <= 77 && (x - 51) % 5 != 4) {
            mono_rect(m, x, top + 1, 1, 3, 1);          /* an upper tooth */
            if (x >= 55 && x <= 73 && bot - top > 9)
                mono_rect(m, x, bot - 3, 1, 3, 1);      /* a lower one */
        }
    }
    /* the SID chip computer under it: its lamps chasing, its pins */
    mono_rect(m, 33, 47, 62, 8, 1);
    mono_rect(m, 37, 49, 26, 4, 0);
    mono_rect(m, 66, 49, 14, 4, 0);
    mono_rect(m, 83, 49, 8, 4, 0);
    for (i = 0; i < 6; i++)
        if ((int)((ms / 140u) % 7u) != i)
            mono_rect(m, 38 + i * 4, 50, 3, 2, 1);
    mono_rect(m, 67, 50, 5, 2, 1);
    mono_rect(m, 74, 50, 5, 2, 1);
    if ((ms / 400u) & 1u)
        mono_rect(m, 85, 50, 4, 2, 1);
    for (i = 0; i < 7; i++)
        mono_rect(m, 37 + i * 9, 55, 3, 6, 1);
    mono_rect(m, 2, 53, 31, 2, 1);
    mono_rect(m, 95, 53, 31, 2, 1);
    mono_rect(m, 8, 55, 2, 5, 1);
    mono_rect(m, 118, 55, 2, 5, 1);
    /* the two monitors: a scope, the meters */
    mono_box(m, 7, 32, 24, 16, 1);
    mono_box(m, 8, 33, 22, 14, 1);
    for (i = 0; i < 18; i++) {
        static const int W[8] = { 0, -3, -4, -2, 1, 3, 4, 2 };
        mono_pset(m, 10 + i, 40 + W[(i + wave) & 7], 1);
        mono_pset(m, 10 + i, 41 + W[(i + wave) & 7], 1);
    }
    mono_rect(m, 17, 48, 4, 3, 1);
    mono_rect(m, 13, 51, 12, 2, 1);
    mono_box(m, 97, 32, 24, 16, 1);
    mono_box(m, 98, 33, 22, 14, 1);
    for (i = 0; i < 5; i++) {
        const int h = 2 + (int)((ms / 110u + (uint32_t)i * 3u) % 8u);
        mono_rect(m, 101 + i * 4, 45 - h, 2, h, 1);
    }
    mono_rect(m, 107, 48, 4, 3, 1);
    mono_rect(m, 103, 51, 12, 2, 1);
    if (flick != 1) { bolt(m, l1, 4); bolt(m, r2, 4); }
    if (flick != 3) { bolt(m, l2, 4); bolt(m, r1, 4); }
    for (i = 0; i < 8; i++)
        if ((int)((ms / 300u + (uint32_t)i) % 7u) != 0)
            mono_pset(m, stars[i][0], stars[i][1], 1);
}

/* ------------------------------------------------------------------ the pop-ups --- */
void scr_page_name(mono_t *m, const char *name)
{
    char a[9] = "", b[9] = "";
    const size_t n = strlen(name);
    mono_clear(m);
    if (n <= 8) {
        mono_ctext(m, 64, 24, name, MONO_FB, 2, 1);
        return;
    }
    {   /* two lines: broken at the last space that fits, else at eight characters */
        size_t cut = 8, i;
        for (i = 0; i <= 8 && i < n; i++)
            if (name[i] == ' ')
                cut = i;
        memcpy(a, name, cut);
        a[cut] = 0;
        strncpy(b, name + cut + (name[cut] == ' '), 8);
        b[8] = 0;
    }
    mono_ctext(m, 64, 14, a, MONO_FB, 2, 1);
    mono_ctext(m, 64, 34, b, MONO_FB, 2, 1);
}

void scr_knob(mono_t *m, const char *name, const char *value, int frac)
{
    mono_clear(m);
    mono_text(m, 4, 3, name, MONO_FB, 1, 1);
    if (value && *value) {
        const int sc = mono_text_w(value, MONO_FB, 3) <= 120 ? 3 : mono_text_w(value, MONO_FB, 2) <= 124 ? 2 : 1;
        mono_text(m, 4, 19 + (3 - sc) * 4, value, MONO_FB, sc, 1);
    }
    if (frac >= 0) {
        mono_box(m, 4, 51, 120, 9, 1);
        mono_rect(m, 4, 51, frac > 1000 ? 120 : frac * 120 / 1000, 9, 1);
    }
}

/* ------------------------------------------------------------------ the core's screen, a part of it --- */
void scr_core_crop(mono_t *m, const uint16_t *lcd_be, int left, int top)
{
    const int w = 120 - left > MONO_W ? MONO_W : 120 - left, x0 = (MONO_W - w) / 2;
    int x, y;
    mono_clear(m);
    for (y = 0; y < MONO_H; y++) {
        const int ly = top + y;
        if (ly < 0 || ly >= 120)
            continue;
        for (x = 0; x < w; x++) {
            const uint16_t be = lcd_be[(ly * 2) * 240 + (left + x) * 2], c = (uint16_t)((be >> 8) | (be << 8));
            const unsigned r = ((c >> 11) & 31u) * 255u / 31u, g = ((c >> 5) & 63u) * 255u / 63u, b = (c & 31u) * 255u / 31u;
            if ((r * 299u + g * 587u + b * 114u) / 1000u >= 48u)
                m->px[y][x0 + x] = 1;
        }
    }
}

/* ------------------------------------------------------------------ the sound's pictures --- */
static void thick(mono_t *m, int x0, int y0, int x1, int y1)
{
    mono_line(m, x0, y0, x1, y1, 1);
    mono_line(m, x0, y0 + 1, x1, y1 + 1, 1);
    if (x0 == x1)
        mono_line(m, x0 + 1, y0, x1 + 1, y1, 1);
}

void scr_osc(mono_t *m, int wave, int pw, uint32_t ms)
{
    const int top = 4, bot = 58, per = 52;
    int c, x;
    mono_clear(m);
    if (wave == SCR_W_OFF) {
        mono_dots_h(m, 0, 31, MONO_W, 3);
        return;
    }
    if (wave == SCR_W_NOISE) {                          /* held steps of a shift register's bits */
        uint32_t r = 0x7FFFF8u ^ (ms / 120u) * 2654435761u;
        int py = 31;
        for (x = 0; x < MONO_W; x += 4) {
            int y;
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            y = top + (int)(r % (uint32_t)(bot - top));
            thick(m, x, py, x, y);
            thick(m, x, y, x + 4, y);
            py = y;
        }
        return;
    }
    for (c = -1; c < 3; c++) {
        const int x0 = 6 + c * per;
        if (wave == SCR_W_TRI) {
            thick(m, x0, bot, x0 + per / 2, top);
            thick(m, x0 + per / 2, top, x0 + per, bot);
        } else if (wave == SCR_W_SAW) {
            thick(m, x0, bot, x0 + per, top);
            thick(m, x0 + per, top, x0 + per, bot);
        } else {                                        /* the pulse, as wide as PW */
            int hi = per * pw / 1000;
            hi = hi < 3 ? 3 : hi > per - 3 ? per - 3 : hi;
            thick(m, x0, bot, x0, top);
            thick(m, x0, top, x0 + hi, top);
            thick(m, x0 + hi, top, x0 + hi, bot);
            thick(m, x0 + hi, bot, x0 + per, bot);
            if (wave == SCR_W_MIX)                      /* a combined wave: the saw's slope drawn in the pulse */
                mono_line(m, x0, bot, x0 + hi, top + 6, 1);
        }
    }
}

void scr_env(mono_t *m, int a, int d, int s, int r)
{
    const int base = 58, peak = 5;
    const int xa = 4 + 4 + a * 26 / 1000, xd = xa + 4 + d * 26 / 1000, xs = xd + 26, xr = xs + 4 + r * 30 / 1000;
    const int ys = base - (base - peak) * s / 1000;
    int x, y;
    mono_clear(m);
    for (x = 4; x <= xr; x++) {                         /* the half tone under the line */
        int yl;
        if (x <= xa) yl = base - (base - peak) * (x - 4) / (xa - 4);
        else if (x <= xd) yl = peak + (ys - peak) * (x - xa) / (xd - xa);
        else if (x <= xs) yl = ys;
        else yl = ys + (base - ys) * (x - xs) / (xr - xs);
        for (y = yl + 2; y <= base; y++)
            if (((x + y) & 1) == 0)
                mono_pset(m, x, y, 1);
    }
    thick(m, 4, base, xa, peak);
    thick(m, xa, peak, xd, ys);
    thick(m, xd, ys, xs, ys);
    thick(m, xs, ys, xr, base);
    mono_rect(m, xa - 2, peak - 2, 5, 5, 1);
    mono_rect(m, xd - 2, ys - 2, 5, 5, 1);
    mono_rect(m, xs - 2, ys - 2, 5, 5, 1);
    mono_rect(m, xr - 2, base - 3, 5, 5, 1);
    mono_dots_h(m, 0, 61, MONO_W, 2);
}

void scr_filter(mono_t *m, int kind, int cutoff, int reso)
{
    const int mid = 30, bot = 60, xc = 12 + cutoff * 104 / 1000, peak = mid - 4 - reso * 22 / 1000;
    int x, y;
    mono_clear(m);
    mono_dots_h(m, 0, bot + 2, MONO_W, 2);
    if (kind == 0) {                                    /* off: the sound passes untouched */
        thick(m, 0, mid, MONO_W - 1, mid);
        return;
    }
    for (x = 0; x < MONO_W; x++) {                      /* the level at each frequency, a half tone under it */
        const int d = x - xc;                           /* past the cutoff: positive */
        int lv;
        if (kind == 1)
            lv = d < -10 ? mid : d <= 0 ? mid + (peak - mid) * (d + 10) / 10 : peak + (bot - peak) * d / 22;
        else if (kind == 3)
            lv = d > 10 ? mid : d >= 0 ? mid + (peak - mid) * (10 - d) / 10 : peak + (bot - peak) * -d / 22;
        else
            lv = peak + (bot - peak) * (d < 0 ? -d : d) / 26;
        lv = lv > bot ? bot : lv;
        mono_rect(m, x, lv, 1, 2, 1);
        for (y = lv + 3; y <= bot; y++)
            if (((x + y) & 1) == 0)
                mono_pset(m, x, y, 1);
    }
    mono_dots_v(m, xc, 2, bot - 2, 3);                  /* the cutoff */
}

/* ------------------------------------------------------------------ the mix --- */
void scr_mix(mono_t *m, const int *level, const int *mute, const int *meter, int n, int sel)
{
    int i, y;
    mono_clear(m);
    for (i = 0; i < n; i++) {
        const int x = n > 6 ? 8 + i * 15 : 10 + i * 19, cap = 50 - level[i] * 44 / 127;   /* (eight: closer) */
        char num[2] = { (char)('1' + i), 0 };
        if (mute[i] == 1)
            mono_dots_v(m, x, 4, 49, 2);
        else
            mono_rect(m, x, 4, 1, 49, 1);
        if (i == sel)
            mono_rect(m, x - 5, cap - 1, 11, 5, 1);
        else {
            mono_rect(m, x - 4, cap, 9, 3, 0);
            mono_box(m, x - 4, cap, 9, 3, 1);
        }
        for (y = 0; y < meter[i] * 48 / 1000; y++)      /* its level now, beside the fader */
            mono_pset(m, x + 7, 52 - y, 1);
        mono_text(m, x - 1, 57, mute[i] == 1 ? "M" : num, MONO_FS, 1, 1);
    }
}

/* ------------------------------------------------------------------ every knob at once --- */
void scr_eight(mono_t *m, const char *const *lab, const char *const *val, const int *frac, int n)
{
    int i;
    mono_clear(m);
    mono_dots_h(m, 0, 31, MONO_W, 2);
    for (i = 0; i < n && i < 8; i++) {
        const int x = (i % 4) * 32 + 1, y = (i / 4) * 33;
        char l[8], v[5];
        if (!lab[i] || !lab[i][0])
            continue;
        snprintf(l, sizeof l, "%s", lab[i]);
        snprintf(v, sizeof v, "%s", val[i] ? val[i] : "");
        mono_text(m, x, y + 2, l, MONO_FS, 1, 1);
        if (strlen(v) > 3)                              /* (four letters fill the column: the small font, a gap kept) */
            mono_text(m, x, y + 13, v, MONO_FS, 1, 1);
        else
            mono_text(m, x, y + 11, v, MONO_FB, 1, 1);
        if (frac[i] >= 0)
            mono_rect(m, x, y + 23, 1 + frac[i] * 27 / 1000, 2, 1);
    }
}

/* ------------------------------------------------------------------ the lift --- */
void scr_lift(mono_t *m, const char *from, const char *to, int up, int t)
{
    /* the car stays in the middle; the floors pass it: the one left slides away, the one arrived at slides in */
    const int travel = 46 * t / 1000, dir = up ? 1 : -1;
    const int y_from = 56 + dir * travel, y_to = 56 - dir * (46 - travel);
    int i;
    mono_clear(m);
    mono_rect(m, 44, 0, 1, MONO_H, 1);
    mono_rect(m, 84, 0, 1, MONO_H, 1);
    mono_rect(m, 64, 0, 1, 16, 1);
    mono_box(m, 47, 16, 35, 30, 1);
    mono_box(m, 48, 17, 33, 28, 1);
    /* the agent, standing */
    mono_rect(m, 62, 26, 4, 3, 1);
    mono_rect(m, 61, 29, 6, 7, 1);
    mono_rect(m, 59, 30, 2, 4, 1);
    mono_rect(m, 67, 30, 2, 4, 1);
    mono_rect(m, 61, 36, 2, 7, 1);
    mono_rect(m, 65, 36, 2, 7, 1);
    for (i = 0; i < 3; i++) {                           /* the wind of it, on the side it is leaving */
        const int x = 52 + i * 12, len = 6 + (int)((unsigned)(t / 60 + i) % 3u) * 3;
        mono_dots_v(m, x, up ? 48 : 14 - len, len, 2);
    }
    mono_rect(m, 0, y_from, 44, 2, 1);
    mono_rect(m, 85, y_from, 43, 2, 1);
    mono_text(m, 2, y_from - 8, from, MONO_FS, 1, 1);
    mono_rect(m, 0, y_to, 44, 2, 1);
    mono_rect(m, 85, y_to, 43, 2, 1);
    mono_text(m, 2, y_to - 8, to, MONO_FS, 1, 1);
    /* the arrow */
    for (i = 0; i < 5; i++)
        mono_rect(m, 106 - i, up ? 26 + i : 37 - i, 1 + 2 * i, 1, 1);
    mono_rect(m, 105, up ? 31 : 26, 3, 7, 1);
}

/* ------------------------------------------------------------------ a menu --- */
void scr_menu(mono_t *m, const char *const *items, int n, int sel)
{
    int i, first = sel - 1;
    if (first > n - 3)
        first = n - 3;
    if (first < 0)
        first = 0;
    mono_clear(m);
    for (i = 0; i < 3 && first + i < n; i++) {
        const int y = 3 + i * 21, on = first + i == sel;
        if (on)
            mono_rect(m, 0, y - 2, MONO_W, 18, 1);
        mono_text(m, 0, y, items[first + i], MONO_FB, 2, !on);
    }
}

/* ------------------------------------------------------------------ the room --- */
static void sprite(mono_t *m, int x, int y, const char *const *rows, int n)   /* '#' lit; x its left, y its top */
{
    int i, j;
    for (j = 0; j < n; j++)
        for (i = 0; rows[j][i]; i++)
            if (rows[j][i] == '#')
                mono_pset(m, x + i, y + j, 1);
}

static void agent(mono_t *m, int x, int y, int pose)    /* x his middle, y his feet's row */
{
    static const char *const A[10] = { "..##...", "..##...", "..###..", ".####.#", "#.##...", "..##...", "..###..",
                                       ".#..#..", "#....#.", "#....##" };
    static const char *const B[10] = { "..##...", "..##...", ".###...", "#.###..", "..##.#.", "..##...", "..##...",
                                       "..##...", ".##....", "..##..." };
    static const char *const J[8] = { "..##..#", "#.##.#.", ".####..", "..##...", "..##...", ".####..", "#....#.",
                                      "#......" };
    if (pose == 2)
        sprite(m, x - 3, y - 9, J, 8);
    else
        sprite(m, x - 3, y - 10, pose ? B : A, 10);
}

static void robot(mono_t *m, int x, int y, int blink)   /* x its left, y its feet's row */
{
    mono_pset(m, x + 3, y - 7, 1);
    mono_rect(m, x + 1, y - 6, 5, 3, 1);
    if (!blink) {
        mono_pset(m, x + 2, y - 5, 0);
        mono_pset(m, x + 4, y - 5, 0);
    }
    mono_rect(m, x + 2, y - 3, 3, 2, 1);
    mono_rect(m, x + 1, y - 1, 5, 1, 1);
}

void scr_room(mono_t *m, const scr_room_t *r, uint32_t ms)
{
    int i;
    mono_clear(m);
    /* the lift: the tracks, the one you are on solid, a pulse as each plays (none: the floor runs the full width) */
    if (r->ntrack)
        mono_dots_v(m, 10, 0, MONO_H, 2);
    for (i = 0; i < r->ntrack; i++) {
        const int y = 3 + i * 10;
        if (i == r->sel)
            mono_rect(m, 1, y, 7, 7, 1);
        else if (r->flash >> i & 1)
            mono_rect(m, 2, y + 1, 5, 5, 1);
        else
            mono_box(m, 3, y + 2, 3, 3, 1);
        if (r->mute >> i & 1) {                         /* not heard: struck through */
            mono_line(m, 1, y, 7, y + 6, i != r->sel);
            mono_line(m, 7, y, 1, y + 6, i != r->sel);
        }
    }
    /* the floor, the notes as platforms, a robot on a step that holds locks */
    {
        const int x0 = r->ntrack ? 11 : 0;
        mono_rect(m, x0, SCR_RM_FLOOR, MONO_W - x0, 2, 1);
        if (r->npage <= 0)
            mono_dots_h(m, x0, SCR_RM_FLOOR + 4, MONO_W - x0, 2);
        for (i = 0; i < r->npage && i < 16; i++) {      /* the pages: shown solid, playing blinking, the loop's dotted */
            const int x = 2 + i * 8;
            if (i == r->page_shown)
                mono_rect(m, x, SCR_RM_FLOOR + 4, 6, 2, 1);
            else if (i == r->page_playing && (ms / 250u) & 1u)
                mono_rect(m, x, SCR_RM_FLOOR + 4, 6, 1, 1);
            else if (r->page_loop >> i & 1u)
                mono_dots_h(m, x, SCR_RM_FLOOR + 4, 6, 2);
        }
    }
    for (i = 0; i < 16; i++) {
        const int x = SCR_RM_X(i);
        if (r->col[i].on) {
            mono_rect(m, x, r->col[i].y, r->col[i].w, 2, 1);
            if (r->col[i].held)
                mono_box(m, x - 2, r->col[i].y - 2, r->col[i].w + 4, 6, 1);
        } else if (r->col[i].held)
            mono_box(m, x - 1, SCR_RM_FLOOR - 5, 8, 5, 1);
        if (r->col[i].guard)
            robot(m, x, r->col[i].on ? r->col[i].y : SCR_RM_FLOOR, (int)((ms / 700u + (uint32_t)i) % 5u) == 0);
    }
    if (r->agent_x >= 0)
        agent(m, r->agent_x, r->agent_y, r->agent_pose);
}

/* ------------------------------------------------------------------ the FX floors --- */
static unsigned hash(unsigned x)
{
    x ^= x >> 16, x *= 0x7feb352du, x ^= x >> 15, x *= 0x846ca68bu, x ^= x >> 16;
    return x;
}

static void plot(mono_t *m, int x, int y, int *py, int w)   /* a curve's next point, joined to its last */
{
    if (x > 0)
        mono_line(m, x - 1, *py, x, y, 1);
    if (w > 1 && x > 0)
        mono_line(m, x - 1, *py + 1, x, y + 1, 1);
    *py = y;
}

void scr_fx(mono_t *m, int kind, const int *v, uint32_t ms)
{
    const float t = (float)ms / 1000.f;
    int x, y, i, py = 30;
#define V(k) (v[k] < 0 ? 0 : v[k])
    mono_clear(m);
    switch (kind) {
    case SCR_FX_SENDS:                                  /* eight levels: the track's three sends, the five rooms */
        for (i = 0; i < 8; i++) {
            const int bx = 5 + i * 15 + (i >= 3 ? 5 : 0), h = V(i) * 52 / 1000;
            mono_box(m, bx, 5, 9, 54, 1);
            mono_rect(m, bx, 59 - h - 1, 9, h + 1, 1);
        }
        mono_dots_v(m, 51, 2, 60, 3);
        break;
    case SCR_FX_CHORUS: {                               /* rate, depth, width: three voices drifting apart */
        const float rate = 0.6f + (float)V(0) / 250.f, amp = 3.f + (float)V(1) / 70.f, gap = 2.f + (float)V(2) / 60.f;
        for (i = -1; i <= 1; i++) {
            py = 30;
            for (x = 0; x < MONO_W; x++) {
                y = 31 + (int)((float)i * gap + amp * sinf((float)x * 0.11f + t * rate * (1.f + 0.23f * (float)i) + (float)i * 1.3f));
                if (i == 0 || (x & 1))
                    plot(m, x, y, &py, i == 0 ? 2 : 1);
                else
                    py = y;
            }
        }
        break;
    }
    case SCR_FX_ECHO: {                                 /* time, spread, feedback: the echoes, each fainter */
        const int gap = 7 + V(0) * 22 / 1000;
        float h = 50.f;
        const float fb = 0.25f + (float)V(2) / 1400.f;
        mono_rect(m, 2, 4, 2, 56, 1);
        mono_rect(m, 124, 4, 2, 56, 1);
        for (i = 0, x = 10; x < 120 && h >= 2.f; i++, x += gap, h *= fb) {
            const int off = (i & 1) ? V(1) * 6 / 1000 : 0;
            mono_rect(m, x, 32 - (int)h / 2 + off, i == 0 ? 4 : 2, (int)h, 1);
        }
        {   /* the sound itself, bouncing between the walls at the delay's time */
            const float ph = fmodf(t * 1000.f / (float)(120 + V(0)), 2.f);
            mono_disc(m, 8 + (int)((ph < 1.f ? ph : 2.f - ph) * 112.f), 61, 2, 2, 1);
        }
        break;
    }
    case SCR_FX_ROOM: {                                 /* size, damp, decay: a room, its reflections */
        const int w = 30 + V(0) * 90 / 1000, hh = 16 + V(0) * 42 / 1000, n = 1 + V(2) * 6 / 1000;
        const int x0 = 64 - w / 2, y0 = 32 - hh / 2;
        mono_box(m, x0, y0, w, hh, 1);
        mono_box(m, x0 + 1, y0 + 1, w - 2, hh - 2, 1);
        for (i = 1; i <= n; i++) {
            const int in = i * 4 + (int)(fmodf(t * 3.f, 4.f));
            const int step = 2 + i * V(1) / 500;
            if (w - 2 * in < 4 || hh - 2 * in < 4)
                break;
            mono_dots_h(m, x0 + in, y0 + in, w - 2 * in, step);
            mono_dots_h(m, x0 + in, y0 + hh - 1 - in, w - 2 * in, step);
            mono_dots_v(m, x0 + in, y0 + in, hh - 2 * in, step);
            mono_dots_v(m, x0 + w - 1 - in, y0 + in, hh - 2 * in, step);
        }
        mono_line(m, 0, 0, x0, y0, 1);
        mono_line(m, 127, 0, x0 + w - 1, y0, 1);
        mono_line(m, 0, 63, x0, y0 + hh - 1, 1);
        mono_line(m, 127, 63, x0 + w - 1, y0 + hh - 1, 1);
        break;
    }
    case SCR_FX_CRUSH: {                                /* bits, rate: a sine as stairs */
        const int levels = 2 + V(0) * 14 / 1000, hold = 1 + (1000 - V(1)) * 12 / 1000;
        for (x = 0; x < MONO_W; x++) {
            const int xs = x / hold * hold;
            const float s = sinf((float)xs * 0.075f + t * 2.f);
            y = 31 - (int)(floorf(s * (float)levels / 2.f + 0.5f) * 52.f / (float)levels);
            y = y < 3 ? 3 : y > 60 ? 60 : y;
            plot(m, x, y, &py, 2);
        }
        break;
    }
    case SCR_FX_ROBOT: {                                /* frequency: the sound multiplied by a carrier */
        const float car = 0.25f + (float)V(0) / 900.f;
        for (x = 0; x < MONO_W; x++) {
            y = 31 + (int)(26.f * sinf((float)x * 0.05f + t) * sinf((float)x * car + t * 6.f));
            plot(m, x, y, &py, 1);
        }
        for (x = 0; x < MONO_W; x += 2) {               /* its envelope, dotted */
            const int e = (int)(26.f * fabsf(sinf((float)x * 0.05f + t)));
            mono_pset(m, x, 31 - e, 1);
            mono_pset(m, x, 31 + e, 1);
        }
        break;
    }
    case SCR_FX_WARP: {                                 /* time, feedback, rate: tape that slows and speeds */
        const float dep = 1.f + (float)V(0) / 250.f, rate = 0.5f + (float)V(2) / 300.f;
        for (x = 0; x < MONO_W; x++) {
            y = 31 + (int)(22.f * sinf((float)x * 0.09f + dep * sinf((float)x * 0.025f + t * rate)));
            plot(m, x, y, &py, 2);
        }
        break;
    }
    case SCR_FX_GLITCH: {                               /* length, chance: the sound cut up and thrown about */
        const int bw = 5 + V(0) * 22 / 1000;
        const unsigned slice = ms / 180u;
        for (x = 0; x < MONO_W; x++) {
            const unsigned h = hash((unsigned)(x / bw) * 31u + slice);
            const int hit = (int)(h % 1000u) < 150 + V(1) * 7 / 10;
            const int xs = hit ? (x % bw) + (int)((h >> 10) % 100u) : x;
            y = 31 + (int)(20.f * sinf((float)xs * 0.12f + t * 2.f)) + (hit ? (int)((h >> 20) % 17u) - 8 : 0);
            y = y < 2 ? 2 : y > 61 ? 61 : y;
            if (x % bw == 0)
                py = y;
            plot(m, x % bw == 0 ? 0 : x, y, &py, 1);
            if (hit && x % bw == 0)
                mono_dots_v(m, x, 2, 60, 3);
        }
        break;
    }
    default: {                                          /* TV: a set, its hum bar rolling, snow by the drive */
        const int bar = (int)((ms / 40u) % 52u);
        mono_box(m, 14, 2, 100, 56, 1);
        mono_box(m, 15, 3, 98, 54, 1);
        mono_pset(m, 14, 2, 0); mono_pset(m, 113, 2, 0); mono_pset(m, 14, 57, 0); mono_pset(m, 113, 57, 0);
        for (y = 6; y < 55; y += 3)
            mono_dots_h(m, 18, y, 92, 2 + (1000 - V(0)) * 3 / 1000);
        mono_rect(m, 17, 5 + bar, 94, 2 + V(1) * 6 / 1000, 1);
        for (i = 0; i < 20 + V(2) * 160 / 1000; i++) {
            const unsigned h = hash((unsigned)i * 97u + ms / 60u);
            mono_pset(m, 18 + (int)(h % 92u), 6 + (int)((h >> 8) % 49u), (int)(h >> 20 & 1u));
        }
        mono_rect(m, 40, 59, 3, 4, 1);
        mono_rect(m, 85, 59, 3, 4, 1);
        break;
    }
    }
#undef V
}

/* ------------------------------------------------------------------ an LFO, a voice's word --- */
void scr_lfo(mono_t *m, int shape, int speed, int depth, uint32_t ms)
{
    const float cycles = 1.f + (float)speed / 200.f, amp = 4.f + (float)depth * 24.f / 1000.f;
    const float now = fmodf((float)ms / 1000.f * (0.3f + (float)speed / 400.f), 1.f);
    int x, py = 31, dot_y = 31;
    mono_clear(m);
    mono_dots_h(m, 0, 31, MONO_W, 4);
    for (x = 0; x < MONO_W; x++) {
        const float ph = fmodf((float)x / (float)MONO_W * cycles, 1.f);
        float s;
        int y;
        switch (shape) {
        case 1: s = ph < 0.5f ? 4.f * ph - 1.f : 3.f - 4.f * ph; break;
        case 2: s = 1.f - 2.f * ph; break;
        case 3: s = ph < 0.5f ? 1.f : -1.f; break;
        case 4: s = (float)(hash((unsigned)((float)x / (float)MONO_W * cycles * 4.f) + 7u) % 2001u) / 1000.f - 1.f; break;
        default: s = sinf(ph * 6.2832f); break;
        }
        y = 31 - (int)(s * amp);
        plot(m, x, y, &py, 2);
        if (x == (int)(now * (float)MONO_W))
            dot_y = y;
    }
    mono_disc(m, (int)(now * (float)MONO_W), dot_y, 3, 3, 1);
}

void scr_word(mono_t *m, const uint8_t *nib, uint32_t n, int start, int end)
{
    int x;
    mono_clear(m);
    for (x = 0; x < MONO_W; x++) {
        const uint32_t i0 = (uint32_t)((uint64_t)x * n / MONO_W), i1 = (uint32_t)((uint64_t)(x + 1) * n / MONO_W);
        int lo = 15, hi = 0;
        uint32_t i;
        for (i = i0; i < i1 || i == i0; i++) {
            const int s = i < n ? (nib[i / 2] >> (i & 1 ? 0 : 4)) & 15 : 8;
            lo = s < lo ? s : lo;
            hi = s > hi ? s : hi;
        }
        mono_rect(m, x, 58 - hi * 54 / 15, 1, (hi - lo) * 54 / 15 + 1, 1);
    }
    if (start >= 0)
        mono_dots_v(m, start * 127 / 1000, 0, MONO_H, 2);
    if (end >= 0)
        mono_dots_v(m, end * 127 / 1000, 0, MONO_H, 2);
}

void scr_randomize(mono_t *m, uint32_t ms, int pct)
{
    scr_intro(m, ms);
    mono_rect(m, 0, 56, MONO_W, 8, 0);
    mono_box(m, 4, 57, 120, 7, 1);
    mono_rect(m, 4, 57, (pct < 0 ? 0 : pct > 100 ? 100 : pct) * 120 / 100, 7, 1);
}

void scr_pages(mono_t *m, const char *name, const char *value, int npage, unsigned loop, int shown, int playing, uint32_t ms)
{
    int p;
    scr_knob(m, name, value, -1);
    for (p = 0; p < 16; p++) {
        const int x = 4 + p * 7 + p / 4 * 2;
        if (p >= npage)
            mono_dots_h(m, x, 59, 6, 2);
        else if (p == playing && (ms / 250u) & 1u)
            mono_box(m, x, 51, 6, 9, 1), mono_rect(m, x + 2, 54, 2, 3, 1);
        else if (!(loop >> p & 1u))
            mono_box(m, x, 51, 6, 9, 1);
        else
            mono_rect(m, x, 51, 6, 9, 1);
        if (p == shown)
            mono_rect(m, x + 1, 47, 4, 2, 1);
    }
}

/* ------------------------------------------------------------------ Session's clips --- */
void scr_session(mono_t *m, const uint8_t cell[4][8], int n, int first, int sel_col, int sel_row, const char *title,
                 uint32_t ms)
{
    const int blink = (int)((ms / 250u) & 1u);
    char rows[28];
    int r, c, k;
    mono_clear(m);
    mono_text(m, 1, 1, title, MONO_FS, 1, 1);
    snprintf(rows, sizeof rows, "%d-%d", first + 1, first + 4);
    mono_text(m, MONO_W - 1 - mono_text_w(rows, MONO_FS, 1), 1, rows, MONO_FS, 1, 1);
    for (r = 0; r < 4; r++)
        for (c = 0; c < n && c < 8; c++) {
            const int x = c * 16 + 1, y = 11 + r * 13, w = 14, h = 11, st = cell[r][c];
            const int solid = st == SCR_CL_PLAYING || ((st == SCR_CL_QUEUED || st == SCR_CL_STOPPING) && blink);
            if (st == SCR_CL_EMPTY)
                mono_pset(m, x + 7, y + 5, 1);
            else if (solid) {
                mono_rect(m, x, y, w, h, 1);
                if (st != SCR_CL_STOPPING)
                    for (k = 0; k < 7; k++)             /* the play mark */
                        mono_rect(m, x + 5, y + 2 + k, k < 4 ? k + 1 : 7 - k, 1, 0);
            } else
                mono_box(m, x, y, w, h, 1);
            if (c == sel_col && r == sel_row) {
                mono_box(m, x - 1, y - 1, w + 2, h + 2, 1);
                if (solid)
                    mono_box(m, x, y, w, h, 0);
            }
        }
}

/* ------------------------------------------------------------------ Session's MIX --- */
/* (meter: each track's level now, 0..1000, a 2-pixel bar beside its column; 0: none drawn) */
void scr_mix_cols(mono_t *m, const char *name, const int *v, const int *mute, const int *meter, int n, int sel, int pan)
{
    int i;
    mono_clear(m);
    mono_text(m, 1, 1, name, MONO_FS, 1, 1);
    for (i = 0; i < n && i < 8; i++) {
        const int x = i * 16 + 1, top = 10, h = 42, w = 11;
        const int mh = meter ? (meter[i] < 0 ? 0 : meter[i] > 1000 ? 1000 : meter[i]) * h / 1000 : 0;
        if (mh > 0)                                     /* the meter: from the column's foot up */
            mono_rect(m, x + w + 1, top + h - mh, 2, mh, 1);
        char num[2] = { (char)('1' + i), 0 };
        if (pan) {                                      /* the middle line, the mark left or right of it */
            const int mx = x + w / 2, px = mx + (v[i] < -64 ? -64 : v[i] > 63 ? 63 : v[i]) * (w / 2) / 64;
            if (mute[i])
                mono_dots_v(m, mx, top, h, 2);
            else
                mono_rect(m, mx, top, 1, h, 1);
            mono_rect(m, px - 1, top + h / 2 - 3, 3, 7, 1);
        } else {
            const int fill = (v[i] < 0 ? 0 : v[i] > 127 ? 127 : v[i]) * (h - 2) / 127;
            if (mute[i])
                mono_dither(m, x, top, w, h, 2);
            mono_box(m, x, top, w, h, 1);
            if (fill > 0)
                mono_rect(m, x + 1, top + h - 1 - fill, w - 2, fill, 1);
        }
        if (i == sel)
            mono_rect(m, x, 55, w, 9, 1);
        mono_ctext(m, x + w / 2, 57, mute[i] ? "M" : num, MONO_FS, 1, i == sel ? 0 : 1);
    }
}

/* ------------------------------------------------------------------ Session's page belt --- */
/* after the board's frames: the pages' boxes ride a belt into the brain machine at the right; the chosen one large in
 * the middle (x 37..66), its neighbours small (36 apart), its name under the belt. Moving (t < 1000, from the page
 * before): every box small, sliding, the rollers turning with them */
static void belt_icon(mono_t *m, int k, int cx, int cy)
{
    const belt_icon_t *ic = &BELT_ICON[k];
    int x, y;
    for (y = 0; y < ic->h; y++)
        for (x = 0; x < ic->w; x++)
            if (ic->row[y] >> (31 - x) & 1u)
                mono_pset(m, cx - ic->w / 2 + x, cy - ic->h / 2 + y, 1);
}

void scr_belt(mono_t *m, const char *const *names, int n, int sel, int from, int t)
{
    const int moving = t >= 0 && t < 1000;
    const int v1000 = moving ? from * 1000 + (sel - from) * t : sel * 1000;   /* (the belt's place, in thousandths) */
    int k, x, y;
    mono_clear(m);
    for (k = 0; k < n; k++) {                           /* the boxes */
        const int cx = 52 + (int)((int64_t)(k * 1000 - v1000) * 36 / 1000);
        const int big = !moving && k == sel;
        const int w = big ? 30 : 22, h = big ? 28 : 20, bx = cx - w / 2, by = 36 - h;
        if (bx > BELT_MX || bx + w < 0)
            continue;
        mono_rect(m, bx, by, w, h, 0);
        mono_box(m, bx, by, w, h, 1);
        if (big)
            mono_box(m, bx + 1, by + 1, w - 2, h - 2, 1);
        belt_icon(m, k, cx, by + h / 2);
    }
    mono_line(m, 0, 36, BELT_MX, 36, 1);                /* the belt, its rollers turning with it */
    mono_line(m, 0, 41, BELT_MX, 41, 1);
    {
        const int ph = (int)(((int64_t)v1000 * 36 / 1000) % 12);
        for (x = -12; x < BELT_MX; x += 12) {
            const int rx = x + 9 - ph;
            if (rx >= 1 && rx < BELT_MX - 1) {
                mono_pset(m, rx, 38, 1), mono_pset(m, rx, 40, 1);
                mono_line(m, rx - 1, 39, rx + 1, 39, 1);
            }
            if (rx - 7 >= 0 && rx - 7 < BELT_MX)
                mono_pset(m, rx - 7, 37, 1);
            if (rx - 5 >= 0 && rx - 5 < BELT_MX)
                mono_pset(m, rx - 5, 37, 1);
        }
    }
    mono_rect(m, BELT_MX, 0, BELT_MW, 42, 0);           /* the brain machine */
    for (y = 0; y < 42; y++)
        for (x = 0; x < BELT_MW; x++)
            if (BELT_MACHINE[y] >> (31 - x) & 1u)
                mono_pset(m, BELT_MX + x, y, 1);
    if (!moving) {                                      /* the name, bold */
        const int w = mono_text_w(names[sel], MONO_FB, 1);
        mono_text(m, 52 - w / 2, 50, names[sel], MONO_FB, 1, 1);
        mono_text(m, 53 - w / 2, 50, names[sel], MONO_FB, 1, 1);
    }
}

/* ------------------------------------------------------------------ Session's ARRANGE --- */
/* the song, eight positions from first (1-based on screen): a box each with its scene (0: END, the boxes after it
 * empty), its repeats under it; the playing one (0..7, -1 none) filled; the song's progress (0..1000, < 0 none) a
 * line under them */
void scr_song(mono_t *m, int first, const int *scene, const int *rep, int play, int prog)
{
    int i, ended = 0;
    char t[32];
    mono_clear(m);
    snprintf(t, sizeof t, "SONG %d-%d", first + 1, first + 8);
    mono_text(m, 1, 1, t, MONO_FS, 1, 1);
    for (i = 0; i < 8; i++) {
        const int x = i * 16 + 1, y = 12, w = 14, h = 18;
        char s[6];
        if (ended)
            continue;
        if (!scene[i]) {                                /* the song's end */
            mono_ctext(m, x + w / 2, y + 6, "END", MONO_FS, 1, 1);
            ended = 1;
            continue;
        }
        if (i == play)
            mono_rect(m, x, y, w, h, 1);
        else
            mono_box(m, x, y, w, h, 1);
        snprintf(s, sizeof s, "%d", scene[i]);
        mono_ctext(m, x + w / 2, y + 6, s, MONO_FB, 1, i == play ? 0 : 1);
        snprintf(s, sizeof s, "x%d", rep[i]);
        mono_ctext(m, x + w / 2, y + h + 4, s, MONO_FS, 1, 1);
    }
    mono_dots_h(m, 1, 50, 126, 2);
    if (prog >= 0)
        mono_rect(m, 1, 49, (prog > 1000 ? 1000 : prog) * 126 / 1000, 3, 1);
}

/* ------------------------------------------------------------------ MIDI FX (Session 2) --- */
/* the selected track's group: the head (MIDI FX, the group, the track), the rail of eight slots (each its effect's
 * short name; empty: a dotted box; bypassed: crossed; the selected inverted), the selected effect's name and page,
 * the foot: knobs 1-4's labels and values. g 0: the track is OFF (how to give it a group) */
static const char *const MFX_SHORT[11] = { "", "ARP", "CHN", "RPT", "HRM", "TRN", "SCL", "RND", "CHD", "MNO", "SEL" };
static const char *const MFX_LONG[11] = { "EMPTY", "ARP", "CHANCE", "REPEAT", "HARM", "TRANSPOSE", "SCALER", "RANDOM",
                                          "CHORD", "MONO", "SELECTOR" };
void scr_mfx(mono_t *m, int g, int track, const uint8_t *type, const uint8_t *byp, int sel, int page, int npages,
             const char *const *lab, const char (*val)[12], const int *has)
{
    char t[24];
    int i;
    mono_clear(m);
    snprintf(t, sizeof t, g ? "MIDI FX  G%d" : "MIDI FX  OFF", g);
    mono_text(m, 1, 1, t, MONO_FS, 1, 1);
    snprintf(t, sizeof t, "T%d", track);
    mono_text(m, 127 - mono_text_w(t, MONO_FS, 1), 1, t, MONO_FS, 1, 1);
    if (!g) {
        mono_ctext(m, 64, 24, "NO GROUP", MONO_FB, 1, 1);
        mono_ctext(m, 64, 40, "HOLD BUTTON 2 + WHEEL", MONO_FS, 1, 1);
        return;
    }
    for (i = 0; i < 8; i++) {                           /* the rail */
        const int x = i * 16, y = 9, w = 15, h = 11, ty = type[i] < 11 ? type[i] : 0;
        if (i == sel)
            mono_rect(m, x, y, w, h, 1);
        else if (ty)
            mono_box(m, x, y, w, h, 1);
        else {
            mono_dots_h(m, x, y, w, 2), mono_dots_h(m, x, y + h - 1, w, 2);
            mono_dots_v(m, x, y, h, 2), mono_dots_v(m, x + w - 1, y, h, 2);
        }
        if (ty)
            mono_ctext(m, x + w / 2, y + 3, MFX_SHORT[ty], MONO_FS, 1, i == sel ? 0 : 1);
        if (ty && byp[i])                               /* (bypassed: crossed) */
            mono_line(m, x + 1, y + h - 2, x + w - 2, y + 1, i == sel ? 0 : 1);
    }
    {
        const int ty = type[sel & 7] < 11 ? type[sel & 7] : 0;
        snprintf(t, sizeof t, "%d %s%s", sel + 1, MFX_LONG[ty], ty && byp[sel & 7] ? " (BYP)" : "");
        mono_text(m, 1, 23, t, MONO_FB, 1, 1);
        if (npages > 1) {
            snprintf(t, sizeof t, "%d/%d", page + 1, npages);
            mono_text(m, 127 - mono_text_w(t, MONO_FS, 1), 24, t, MONO_FS, 1, 1);
        }
    }
    mono_dots_h(m, 0, 33, 128, 2);
    if (!(type[sel & 7] < 11 ? type[sel & 7] : 0)) {    /* an empty slot: how to fill it */
        mono_text(m, 1, 38, "KNOB 1: AN EFFECT", MONO_FS, 1, 1);
        mono_text(m, 1, 48, "PAD + WHEEL: MOVE", MONO_FS, 1, 1);
        mono_text(m, 1, 56, "KNOB 6: THE GROUP", MONO_FS, 1, 1);
        return;
    }
    for (i = 0; i < 4; i++) {                           /* the foot: knobs 1-4 */
        const int x = i * 32 + 1;
        if (!has[i] || !lab[i])
            continue;
        snprintf(t, sizeof t, "%.5s", lab[i]);
        mono_text(m, x, 37, t, MONO_FS, 1, 1);
        snprintf(t, sizeof t, "%.5s", val[i]);
        mono_text(m, x, 47, t, MONO_FB, 1, 1);
    }
}

/* ------------------------------------------------------------------ PROJECTS --- */
/* the open project: its number of n, its name large, its colour; how the pads work, small */
void scr_projects(mono_t *m, int num, int n, const char *name, int colour, int used)
{
    char t[24];
    mono_clear(m);
    snprintf(t, sizeof t, "PROJECT %d/%d", num, n);
    mono_text(m, 1, 1, t, MONO_FS, 1, 1);
    snprintf(t, sizeof t, "%d USED", used);
    mono_text(m, 127 - mono_text_w(t, MONO_FS, 1), 1, t, MONO_FS, 1, 1);
    mono_text(m, 1, 14, name && name[0] ? name : "UNNAMED", MONO_FB, mono_text_w(name && name[0] ? name : "UNNAMED", MONO_FB, 2) <= 126 ? 2 : 1, 1);
    if (colour)
        snprintf(t, sizeof t, "COLOUR: TRACK %d'S", colour);
    else
        snprintf(t, sizeof t, "COLOUR: NONE");
    mono_text(m, 1, 34, t, MONO_FS, 1, 1);
    mono_dots_h(m, 0, 44, 128, 2);
    mono_text(m, 1, 48, "TAP: OPEN / NEW", MONO_FS, 1, 1);
    mono_text(m, 1, 56, "COPY, DELETE + PAD", MONO_FS, 1, 1);
}
