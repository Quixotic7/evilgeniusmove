/* SPDX-License-Identifier: MPL-2.0 */
#include <stdio.h>
#include <string.h>

#include "screens.h"

/* ------------------------------------------------------------------ the brain in the jar --- */
static void brain_shape(mono_t *m, int grow, int on)
{
    static const int lobes[7][3] = { {50, 20, 5}, {57, 15, 6}, {64, 14, 6}, {71, 15, 6}, {78, 20, 5}, {47, 27, 4}, {81, 27, 4} };
    int i;
    mono_disc(m, 64, 24, 19 + grow, 12 + grow, on);
    for (i = 0; i < 7; i++)
        mono_disc(m, lobes[i][0], lobes[i][1], lobes[i][2] + grow, lobes[i][2] + grow, on);
    mono_rect(m, 56 - grow, 32, 16 + 2 * grow, 10 + grow, on);
}

static void bolt(mono_t *m, const int *p, int n)
{
    int i;
    for (i = 0; i + 1 < n; i++)
        mono_line(m, p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], 1);
}

void scr_intro(mono_t *m, uint32_t ms)
{
    static const int arc[] = { 48, 3, 53, 1, 58, 4, 63, 0, 68, 4, 73, 1, 78, 3, 81, 3 };
    static const int l1[] = { 31, 13, 22, 16, 27, 18, 17, 22 }, l2[] = { 33, 22, 25, 24, 29, 26, 20, 29 };
    static const int r1[] = { 97, 13, 106, 16, 101, 18, 111, 22 }, r2[] = { 95, 22, 103, 24, 99, 26, 108, 29 };
    static const int stars[7][2] = { {8, 6}, {14, 20}, {24, 3}, {104, 5}, {118, 12}, {122, 26}, {3, 38} };
    const int jaw = (int)((ms / 180u) & 1u);            /* the laugh: the jaw drops a pixel and comes back */
    const int flick = (int)((ms / 90u) % 5u);           /* the lightning: one frame in five dark */
    const int wave = (int)((ms / 120u) % 4u);
    int i;
    mono_clear(m);
    /* the jar: its liquid a half tone, its glass a line */
    mono_dither(m, 39, 9, 50, 36, 2);
    mono_box(m, 38, 8, 52, 38, 1);
    mono_pset(m, 38, 8, 0); mono_pset(m, 89, 8, 0);
    mono_rect(m, 52, 5, 24, 3, 1);
    mono_disc(m, 44, 4, 3, 3, 1);
    mono_disc(m, 84, 4, 3, 3, 1);
    mono_line(m, 46, 6, 53, 8, 1);
    mono_line(m, 82, 6, 75, 8, 1);
    if (flick != 0)
        bolt(m, arc, 8);
    /* the brain: a black edge against the liquid, then white */
    brain_shape(m, 2, 0);
    brain_shape(m, 0, 1);
    mono_line(m, 64, 8, 64, 17, 0);
    mono_line(m, 55, 10, 53, 13, 0); mono_line(m, 53, 13, 55, 16, 0);
    mono_line(m, 73, 10, 75, 13, 0); mono_line(m, 75, 13, 73, 16, 0);
    /* the eyes, shut with laughing */
    mono_line(m, 52, 23, 56, 19, 0); mono_line(m, 56, 19, 60, 23, 0);
    mono_line(m, 52, 22, 56, 18, 0); mono_line(m, 56, 18, 60, 22, 0);
    mono_line(m, 68, 23, 72, 19, 0); mono_line(m, 72, 19, 76, 23, 0);
    mono_line(m, 68, 22, 72, 18, 0); mono_line(m, 72, 18, 76, 22, 0);
    /* the mouth and its teeth */
    for (i = 0; i <= 11 + jaw; i++) {
        const int inset = i < 3 ? 3 - i : (i > 8 + jaw ? (i - 8 - jaw) * 2 : 0);
        mono_rect(m, 50 + inset, 27 + i, 28 - 2 * inset, 1, 0);
    }
    for (i = 0; i < 5; i++)
        mono_rect(m, 53 + i * 5, 29, 4, 3, 1);
    for (i = 0; i < 3; i++)
        mono_rect(m, 56 + i * 6, 36 + jaw, 4, 3, 1);
    /* the SID chip computer under it */
    mono_rect(m, 36, 46, 56, 8, 1);
    mono_rect(m, 40, 48, 22, 4, 0);
    mono_rect(m, 66, 48, 12, 4, 0);
    mono_rect(m, 82, 48, 6, 4, 0);
    for (i = 0; i < 5; i++)
        if ((int)((ms / 150u) % 6u) != i)
            mono_rect(m, 41 + i * 4, 49, 2, 2, 1);
    mono_rect(m, 67, 49, 4, 2, 1);
    mono_rect(m, 73, 49, 4, 2, 1);
    if ((ms / 400u) & 1u)
        mono_rect(m, 84, 49, 2, 2, 1);
    for (i = 0; i < 6; i++)
        mono_rect(m, 42 + i * 8 + (i > 2), 54, 3, 5, 1);
    mono_rect(m, 4, 52, 32, 2, 1);
    mono_rect(m, 92, 52, 32, 2, 1);
    /* the two monitors: a scope, the meters */
    mono_box(m, 9, 33, 22, 14, 1);
    for (i = 0; i < 16; i++) {
        static const int W[8] = { 0, -3, -4, -2, 1, 3, 4, 2 };
        mono_pset(m, 12 + i, 40 + W[(i + wave * 2) & 7], 1);
    }
    mono_rect(m, 18, 47, 4, 3, 1);
    mono_rect(m, 14, 50, 12, 2, 1);
    mono_box(m, 97, 33, 22, 14, 1);
    for (i = 0; i < 4; i++) {
        const int h = 2 + (int)((ms / 110u + (uint32_t)i * 3u) % 6u);
        mono_rect(m, 101 + i * 4, 44 - h, 2, h, 1);
    }
    mono_rect(m, 106, 47, 4, 3, 1);
    mono_rect(m, 102, 50, 12, 2, 1);
    if (flick != 1) { bolt(m, l1, 4); bolt(m, r2, 4); }
    if (flick != 3) { bolt(m, l2, 4); bolt(m, r1, 4); }
    for (i = 0; i < 7; i++)
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
        const int sc = mono_text_w(value, MONO_FB, 3) <= 120 ? 3 : 2;
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
        const int x = 10 + i * 19, cap = 50 - level[i] * 44 / 127;
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
