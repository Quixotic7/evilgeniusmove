/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: FIZZ's kit (mockups/line.html's helpers, ui.h) drawn with the pixel kit (px.h): every call lands on the
 * 120 x 120 grid in the C64's colours, so a screen ported call for call from FIZZ comes out chunky. The coordinates
 * stay FIZZ's 240-space floats here (the screens pass them); px.h's own calls take 120-space integers.
 * Text by size: 17 px and under the 4 x 6 font, to 27 the 8 x 8, to 44 the 8 x 8 doubled, to 70 tripled, above 4x. */
#include <math.h>
#define FZ_MATH_IMPL                    /* (this unit holds the drawing's own math) */
#include "fz_math.h"                   /* (the drawing's own sinf .. atan2f: the flash diet) */

#include "ui.h"
#include "px.h"

const qd_rgb UI_K[4] = {UI_K1, UI_K2, UI_K3, UI_K4};

/* n as decimal into out (12 bytes are enough): no stdio in the screens */
char *ui_itoa(char *out, int n)
{
    char tmp[12];
    int i = 0, k = 0;
    unsigned u = n < 0 ? 0u - (unsigned)n : (unsigned)n;
    do {
        tmp[i++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u);
    if (n < 0)
        out[k++] = '-';
    while (i)
        out[k++] = tmp[--i];
    out[k] = 0;
    return out;
}

/* s (0: nothing) after d's end: d */
char *ui_cat(char *d, const char *s)
{
    char *e = d;
    while (*e)
        e++;
    while (s && *s)
        *e++ = *s++;
    *e = 0;
    return d;
}

float ui_ease(float x) { return x < 0.f ? 0.f : x > 1.f ? 1.f : x * x * (3.f - 2.f * x); }

/* 240-space to the grid */
static int G(float v) { return (int)floorf(v * 0.5f + 0.25f); }

static void text_style(float size, int *font, int *sc)
{
    if (size <= 17.5f)
        *font = PX_FS, *sc = 1;
    else if (size <= 27.5f)
        *font = PX_FB, *sc = 1;
    else if (size <= 44.5f)
        *font = PX_FB, *sc = 2;
    else if (size <= 70.f)
        *font = PX_FB, *sc = 3;
    else
        *font = PX_FB, *sc = 4;
}

/* T: s with its baseline at y; returns the advance (240-space) */
float ui_T(const char *s, float x, float y, float size, int weight, float ls, qd_rgb col, int align)
{
    int font, sc, w, gx, top;
    (void)weight;
    (void)ls;
    text_style(size, &font, &sc);
    w = px_text_w(s, font, sc);
    gx = G(x);
    if (align == QD_CENTER)
        gx -= (w + 1) / 2;
    else if (align == QD_RIGHT)
        gx -= w;
    top = G(y) - (font == PX_FB ? 7 : 5) * sc;
    px_text(gx, top, s, col, font, sc);
    return (float)(2 * (w + sc));
}

float ui_TW(const char *s, float size, int weight, float ls)
{
    int font, sc;
    (void)weight;
    (void)ls;
    text_style(size, &font, &sc);
    return (float)(2 * (px_text_w(s, font, sc) + sc));
}

void ui_lab(const char *s, float x, float y, qd_rgb col, int align) { ui_T(s, x, y, 12.f, 600, 0.5f, col, align); }

void ui_P(const float *pts, int npts, qd_rgb col, float lw, int close, qd_rgb fill)
{
    int p[64], i, n = npts > 32 ? 32 : npts;
    (void)lw;
    for (i = 0; i < n; i++) {
        p[2 * i] = G(pts[2 * i]);
        p[2 * i + 1] = G(pts[2 * i + 1]);
    }
    px_poly(p, n, close, col, fill);
}

void ui_O(float x, float y, float r, qd_rgb col, float lw, qd_rgb fill)
{
    (void)lw;
    px_circle(G(x), G(y), (int)(r * 0.5f + 0.3f), col, fill);
}

/* RR: a box (corners cut when its radius shows at 120); a stroke of 2.5 px or more is 2 pixels */
void ui_RR(float x, float y, float w, float h, float r, qd_rgb col, float lw, qd_rgb fill)
{
    const int x0 = G(x), y0 = G(y), gw = G(x + w) - x0, gh = G(y + h) - y0;
    if (gw <= 0 || gh <= 0)
        return;
    if (r >= 3.f && gw >= 4 && gh >= 4)
        px_rbox(x0, y0, gw, gh, col, fill);
    else {
        px_rect(x0, y0, gw, gh, fill);
        if (col != QD_NONE)
            px_box(x0, y0, gw, gh, col);
    }
    if (col != QD_NONE && lw >= 2.5f && gw > 4 && gh > 4)
        px_box(x0 + 1, y0 + 1, gw - 2, gh - 2, col);
}

/* the frame round the whole screen: a step selected (UI_STEP_FRAME), 2 px */
void ui_step_frame(void)
{
    px_box(0, 0, 120, 120, UI_STEP_FRAME);
    px_box(1, 1, 118, 118, UI_STEP_FRAME);
}

/* the track's number: reverse video, the C64's way (the number dark in a block of the colour) */
void ui_box(float x, float y, int n, qd_rgb col)
{
    char s[12];
    const int gx = G(x), gy = G(y);
    px_rect(gx, gy, 8, 8, col);
    px_ctext(gx + 4, gy + 1, ui_itoa(s, n), UI_BG, PX_FS, 1);
}

uint8_t UI_TRACK_KIND[UI_NTRACK] = {255, 255, 255, 255, 255, 255,
#if UI_NTRACK > 6
                                     255, 255,
#endif
};
int ui_track_kind(int n) { return n >= 1 && n <= UI_NTRACK && UI_TRACK_KIND[n - 1] < 7 ? UI_TRACK_KIND[n - 1] : -1; }

/* a small bitmap: rows of '#' (lit) / '.', centred at (cx, cy), scale s (1: 1 pixel a cell) */
static void bits(const char *const *rows, int nr, int cx, int cy, int s, qd_rgb c)
{
    int r, i, w = 0;
    while (rows[0][w])
        w++;
    for (r = 0; r < nr; r++)
        for (i = 0; i < w; i++)
            if (rows[r][i] == '#')
                px_rect(cx - (w * s) / 2 + i * s, cy - (nr * s) / 2 + r * s, s, s, c);
}

/* the kinds' icons (7 x 7): BASS a low wave, KEYS keys, PLUCK a spike, PAD a swell, LEAD a square, DRUMS a drum, FX a
 * sparkle */
void ui_kind_icon(int kind, float cx, float cy, float s, qd_rgb col)
{
    static const char *const K[7][7] = {
        {".......", "##.....", "..#...#", "...#.#.", "....#..", ".......", "......."},
        {"#######", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#######", "#.#.#.#", "#######"},
        {".#.....", ".#.....", "#.#....", "#..#...", "#...##.", "#.....#", "......."},
        {".......", "..###..", ".#...#.", "#.....#", "#.....#", ".......", "......."},
        {"###.###", "#.#.#..", "#.#.#..", "#.#.#..", "#.###..", ".......", "......."},
        {"#.....#", ".#...#.", "..#.#..", "#######", "#.....#", "#.....#", "#######"},
        {"...#...", "...#...", ".#.#.#.", "###.###", ".#.#.#.", "...#...", "...#..."}};
    if (kind < 0 || kind > 6)
        return;
    bits(K[kind], 7, G(cx), G(cy), s >= 1.6f ? 2 : 1, col);
}

float ui_head_x(int n) { return !n ? 8.f : ui_track_kind(n) >= 0 ? 42.f : 26.f; }

/* the head: [n] kind left ......... right, the 4 x 6 font on a line at the top; a dotted rule under it */
void ui_head(const char *left, const char *right, int n, qd_rgb lcol, qd_rgb rcol)
{
    float x = 8.f;
    if (n) {
        ui_box(4.f, 4.f, n, UI_K3);
        x = 26.f;
        if (ui_track_kind(n) >= 0) {
            ui_kind_icon(ui_track_kind(n), 31.f, 11.f, 1.f, UI_PK_COL[ui_track_kind(n)]);
            x = 42.f;
        }
    }
    ui_T(left, x, 18.f, 12.f, 600, 0.8f, lcol, QD_LEFT);
    if (right)
        ui_T(right, 232.f, 18.f, 12.f, 500, 0.8f, rcol, QD_RIGHT);
}

/* a keybed: nw white keys from the white note lo, the black keys over them */
void ui_keybed(float x0, float y0, float w, float h, int lo, int nw, const qd_rgb *fill)
{
    const int gx = G(x0), gy = G(y0), gw = G(x0 + w) - gx, gh = G(y0 + h) - gy;
    const int kw = gw / nw > 1 ? gw / nw : 2, bh = gh * 3 / 5;
    int i, n;
#define UI_BLACK(n) ((0x54au >> ((n) % 12)) & 1u)   /* C# D# F# G# A# */
    for (i = 0, n = lo; i < nw; n++) {
        qd_rgb f;
        if (UI_BLACK(n))
            continue;
        f = fill && n >= 0 && n < 128 ? fill[n] : QD_NONE;
        px_rect(gx + i * kw, gy, kw - 1, gh, f != QD_NONE ? f : UI_G2);
        i++;
    }
    for (i = 0, n = lo; i < nw; n++) {
        qd_rgb f;
        if (!UI_BLACK(n)) {
            i++;
            continue;
        }
        f = fill && n >= 0 && n < 128 && fill[n] != QD_NONE ? fill[n] : UI_BG;
        px_rect(gx + i * kw - kw / 2, gy, kw - 1 > 1 ? kw - 1 : 1, bh, f);
        if (f == UI_BG)
            px_box(gx + i * kw - kw / 2, gy, kw - 1 > 1 ? kw - 1 : 1, bh, UI_G1);
    }
#undef UI_BLACK
}

void ui_keyboard(float x0, float y0, float w, float h, int lo, const qd_rgb *fill)
{
    ui_keybed(x0, y0, w, h, lo, 14, fill);
}

const char *const UI_FIELD_NAME[UI_NFIELD] = {"NOTE", "VEL", "LEN", "MICRO", "RATCH", "CHNC", "COND", "TRIG",
                                              "FUNC", "SLIDE", "SOUND"};
/* a colour a mode, none of them red (red is time); the C64's: NOTE light blue, VEL light green, LEN cyan, MICRO purple,
 * RATCH light red, CHNC yellow, COND orange, TRIG white, FUNC light grey, SLIDE brown-orange, SOUND green */
const qd_rgb UI_MODE_COL[UI_NFIELD] = {PX_LBLUE, PX_LGREEN, PX_CYAN, PX_PURPLE, PX_LRED,
                                       PX_YELLOW, PX_ORANGE, PX_WHITE, PX_LGREY, UI_LOCK_COL, PX_GREEN};

void ui_head_track(const char *left, int n, const char *name, qd_rgb lcol)
{
    const float w = ui_TW(name, 12.f, 500, 0.8f);
    const int kd = name[0] ? ui_track_kind(n) : -1;
    ui_T(left, 8.f, 18.f, 12.f, 600, 0.8f, lcol, QD_LEFT);
    ui_T(name, 232.f, 18.f, 12.f, 500, 0.8f, UI_G1, QD_RIGHT);
    if (kd >= 0)
        ui_kind_icon(kd, 232.f - w - 9.f, 13.f, 1.f, UI_PK_COL[kd]);
    ui_box(232.f - w - 22.f - (kd >= 0 ? 15.f : 0.f), 6.f, n, UI_G1);
}

int ui_fn_icon(int f)
{
    static const int IC[7] = {UI_IC_NONE, UI_IC_RESET, UI_IC_LEFT, UI_IC_RIGHT, UI_IC_FLIP, UI_IC_DICE, UI_IC_DICE_FN};
    return f >= UI_FN_LEAP ? UI_IC_LEAP : f >= 7 ? UI_IC_JUMP : f > 0 ? IC[f] : UI_IC_NONE;
}

const qd_rgb UI_LFO_COL[3] = {PX_CYAN, PX_YELLOW, PX_PURPLE};
const qd_rgb UI_TRACK_PAL[8] = {PX_LBLUE, PX_LGREEN, PX_WHITE, PX_LRED, PX_PURPLE, PX_YELLOW, PX_CYAN, PX_ORANGE};

/* the black keys' strip: as FIZZ's (3 | 2 | 3 | 2, B11 apart), on the grid */
float ui_strip_x(int k)
{
    static const float X[11] = {4.f, 24.f, 44.f, 68.f, 88.f, 112.f, 132.f, 152.f, 176.f, 196.f, 222.f};
    return X[k < 0 ? 0 : k > 10 ? 10 : k];
}

/* ---------------------------------------------------------------- the icons: 7 x 7 pixel glyphs, as the C64's were */
static const char *const IC_BITS[UI_IC_NOTE][7] = {
    {".......", ".......", ".......", ".......", ".......", ".......", "......."},   /* NONE */
    {"#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".#...#.", "#.....#"},   /* CLEAR */
    {"####...", "#..#...", "#.####.", "#.#..#.", "###..#.", "..#..#.", "..####."},   /* COPY */
    {"...#...", "...#...", ".#.#.#.", "..###..", "#..#..#", "#.....#", "#######"},   /* PASTE */
    {"..#....", ".##....", "#######", ".##...#", "..#...#", "......#", "..####."},   /* UNDO */
    {"#.....#", ".#...#.", "..#.#..", "...#...", ".##.##.", "#..#..#", ".##.##."},   /* CUT */
    {"..#....", ".#.....", "#######", ".#.....", "..#....", ".......", "......."},   /* BACK */
    {".#.....", ".##....", ".###...", ".####..", ".###...", ".##....", ".#....."},   /* PLAY */
    {".......", ".#####.", ".#####.", ".#####.", ".#####.", ".#####.", "......."},   /* STOP */
    {".......", ".......", "..###..", "..###..", "..###..", ".......", "......."},   /* DOT */
    {"#######", "#.....#", ".......", "#..#..#", "#..#..#", ".......", "......."},   /* TRI */
    {"..###.#", ".#...##", "#...###", "#......", "#.....#", ".#...#.", "..###.."},   /* RESET */
    {"......#", "......#", "....###", "....#..", "..###..", "..#....", "###...."},   /* SCALE */
    {"#...#.#", "#...#.#", "#...#.#", "#...#.#", "#...#.#", "#######", "......."},   /* SWING */
    {"..###..", ".#####.", "#######", "#######", "#######", ".#####.", "..###.."},   /* REC */
    {"....##.", "...##..", "..##...", ".######", "...##..", "..##...", ".##...."},   /* NOW */
    {"......#", "......#", "....#.#", "....#.#", "..#.#.#", "..#.#.#", "#.#.#.#"},   /* RAMP */
    {"......#", ".......", "....#..", ".......", "..#....", ".......", "#......"},   /* STRUM */
    {"..###..", "..#.#..", "..#.#..", "..#.#..", "..#.#..", "..#.#..", "..###.."},   /* KEY */
    {"..#....", ".##....", "#######", ".##....", "..#....", ".......", "......."},   /* LEFT */
    {"....#..", "....##.", "#######", "....##.", "....#..", ".......", "......."},   /* RIGHT */
    {"....#..", "#######", "....#..", ".......", "..#....", "#######", "..#...."},   /* FLIP */
    {"#######", "#.....#", "#.#...#", "#..#..#", "#...#.#", "#.....#", "#######"},   /* DICE */
    {"#######", "#.....#", "#.#.#.#", "#.....#", "#.#.#.#", "#.....#", "#######"},   /* DICE_FN */
    {"..###..", ".#...#.", "#.....#", "#.....#", ".....#.", "...##..", "#.#...."},   /* JUMP */
    {".......", ".##.##.", "#..#..#", ".......", "#..#..#", ".......", "......."},   /* LEAP */
    {".#.#.#.", "#######", ".#...#.", "##.#.##", ".#...#.", "#######", ".#.#.#."},   /* GEAR */
    {"##.##.#", "##.##.#", "##.##.#", "##.##.#", "##.##.#", "##.##.#", "##.##.#"},   /* VIEWS */
    {"#######", ".......", "#######", "#######", ".......", "#######", "......."},   /* TRACK */
    {"###.###", "#.#.#.#", "###.###", ".......", "#####..", "......#", "#####.."},   /* FOLLOW */
};
static const char *const NOTE_BITS[7][7] = {
    {".......", ".......", ".......", ".......", ".###...", "#...#..", ".###..."},   /* 1/1 */
    {"....#..", "....#..", "....#..", "....#..", ".####..", "#...#..", ".###..."},   /* 1/2 */
    {"....#..", "....#..", "....#..", "....#..", ".####..", "#####..", ".###..."},   /* 1/4 */
    {"....##.", "....#.#", "....#..", "....#..", ".####..", "#####..", ".###..."},   /* 1/8 */
    {"....##.", "....#.#", "....##.", "....#.#", ".####..", "#####..", ".###..."},   /* 1/16 */
    {"....###", "....###", "....#.#", "....#..", ".####..", "#####..", ".###..."},   /* 1/32 */
    {"....###", "....###", "....###", "....#.#", ".####..", "#####..", ".###..."},   /* 1/64 */
};

FZ_COLD void ui_icon(int id, float cx, float cy, float s, qd_rgb c)
{
    const int sc = s >= 1.6f ? 2 : 1;
    if (id > UI_IC_NONE && id < UI_IC_NOTE)
        bits(IC_BITS[id], 7, G(cx), G(cy), sc, c);
    else if (id >= UI_IC_NOTE && id < UI_IC_NOTE + 7)
        bits(NOTE_BITS[id - UI_IC_NOTE], 7, G(cx), G(cy), sc, c);
}

/* the eleven black keys where they are on the panel, each with an icon or a number: lit a block of its colour (the
 * icon dark in it), dim dark blue, the others outlined; a key with nothing a speck */
void ui_keymap(const ui_kkey_t k[11], float y, float h)
{
    int i;
    char n[12];
    for (i = 0; i < 11; i++) {
        const int x = G(ui_strip_x(i)), w = i == 10 ? 8 : 9, gy = G(y), gh = G(y + h) - gy, cx = x + w / 2,
                  cy = gy + gh / 2;
        const qd_rgb c = k[i].col ? k[i].col : UI_K3;
        const qd_rgb ic = k[i].lit ? UI_BG : k[i].dim ? UI_G3 : c;
        if (k[i].icon == UI_IC_NONE && !k[i].num && !k[i].lit) {
            px_pset(cx, cy, UI_G3);
            continue;
        }
        if (k[i].lit)
            px_rect(x, gy, w, gh, c);
        else
            px_box(x, gy, w, gh, k[i].dim ? UI_G3 : c);
        if (k[i].num)
            px_ctext(cx + 1, cy - 2, ui_itoa(n, k[i].num), ic, PX_FS, 1);
        else if (k[i].icon != UI_IC_NONE)
            ui_icon(k[i].icon, (float)(2 * cx + 1), (float)(2 * cy + 1), 1.f, ic);
        if (k[i].mark)
            px_hline(x + 1, gy + gh + 1, w - 2, UI_G1);
    }
}

/* a lock: a filled chip, 4 x 3 pixels */
void ui_lock(float x, float y, qd_rgb col, float s)
{
    px_rect(G(x), G(y + 1.5f * s), s >= 1.6f ? 6 : 4, s >= 1.6f ? 4 : 3, col);
}

/* a value as a chip: filled in col with dark text (locked here), or outlined in col (locked somewhere) */
float ui_chip(const char *txt, float cx, float y, float size, float maxw, qd_rgb col, int filled)
{
    int font, sc, w, x0, top, h;
    text_style(size, &font, &sc);
    w = px_text_w(txt, font, sc);
    if (2 * w + 8 > (int)maxw && (font != PX_FS || sc > 1)) {
        font = PX_FS;
        sc = 1;
        w = px_text_w(txt, font, sc);
    }
    h = (font == PX_FB ? 7 : 5) * sc + 4;
    x0 = G(cx) - (w + 1) / 2 - 2;
    top = G(y) - (font == PX_FB ? 7 : 5) * sc - 2;
    if (filled)
        px_rbox(x0, top, w + 5, h, col, col);
    else
        px_rbox(x0, top, w + 5, h, col, QD_NONE);
    px_text(x0 + 3, top + 2, txt, filled ? UI_BG : col, font, sc);
    return (float)(2 * (w + 5));
}

/* a little knob: a ring of 5 and its pointer at v over 270 degrees */
void ui_knob_mark(float x, float y, float v, qd_rgb col)
{
    const float a = UI_PI * 0.75f + v * UI_PI * 1.5f;
    const int gx = G(x), gy = G(y);
    px_circle(gx, gy, 3, col, QD_NONE);
    px_line(gx, gy, gx + (int)lroundf(cosf(a) * 2.6f), gy + (int)lroundf(sinf(a) * 2.6f), col);
}

float ui_keyboard_x(float x0, float w, int lo, int n, int *black)
{
    static const unsigned char WI[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6};
    static const unsigned char ISB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    const float kw = w / 14.f;
    const unsigned s = (unsigned)n % 12u, l = (unsigned)lo;
    const int wi = WI[s] - WI[l % 12u] + (int)((unsigned)n / 12u - l / 12u) * 7;
    if (black)
        *black = ISB[s];
    return ISB[s] ? x0 + (float)wi * kw + kw : x0 + (float)wi * kw + kw / 2.f;
}

/* a W key while it plays notes (the keys, REC): boxy, in the keys' colour. st: 0 out of the scale (dark blue), 1 in it
 * (an outline), 2 a root (half-toned), +4 sounding (filled); bits 3.. a value's hint, filled from the foot */
void ui_rec_key(float x, float y, float w, float h, int st)
{
    const int gx = G(x), gy = G(y), gw = G(x + w) - gx, gh = G(y + h) - gy;
    if (st & 4) {
        px_rect(gx, gy, gw, gh, UI_REC_KEY);
        return;
    }
    if ((st & 3) == 2)
        px_checker(gx + 1, gy + 1, gw - 2, gh - 2, UI_REC_KEY, 0);
    px_box(gx, gy, gw, gh, st & 3 ? UI_REC_KEY : UI_G3);
    if (st >> 3) {
        const int fh = (gh - 2) * (st >> 3) / 31;
        px_checker(gx + 1, gy + gh - 1 - fh, gw - 2, fh, UI_REC_KEY, 1);
    }
}

/* the page's step row (16 in fours): a trig filled, the selected outlined white, red the recording's cursor, the
 * playhead white */
void ui_page_row(float y, uint16_t trig, uint16_t sel, int red, int play)
{
    int i;
    const int gy = G(y);
    for (i = 0; i < 16; i++) {
        const int sx = 7 + i * 6 + (i / 4) * 2;
        const int s = (sel >> i) & 1u;
        const qd_rgb f = i == play ? UI_K3 : (trig >> i) & 1u ? UI_G2 : QD_NONE;
        px_rect(sx, gy, 5, 6, f);
        px_box(sx, gy, 5, 6, i == red ? PX_LRED : s ? UI_K3 : UI_G3);
    }
}

/* a B11 click's keys along a screen's foot: the sixteen W keys, the keyboard sign at the left */
void ui_rec_keys_foot(const uint8_t *kst, int clear)
{
    int i;
    if (clear)
        px_rect(0, 88, 120, 32, UI_BG);
    for (i = 0; i < 16; i++)
        ui_rec_key(20.f + (float)i * 13.f + (float)(i / 4) * 2.f, 184.f, 10.f, 42.f, kst[i]);
    ui_rec_kb_sign(4.f, 198.f);
}

void ui_rec_kb_sign(float x, float y)
{
    const int gx = G(x), gy = G(y);
    px_box(gx, gy, 7, 7, UI_REC_KEY);
    px_vline(gx + 2, gy + 1, 5, UI_REC_KEY);
    px_vline(gx + 4, gy + 1, 5, UI_REC_KEY);
    px_rect(gx + 2, gy, 1, 4, UI_REC_KEY);
    px_rect(gx + 4, gy, 1, 4, UI_REC_KEY);
}
