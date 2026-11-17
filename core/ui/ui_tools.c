/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: TOOLS, the agent's puzzle terminal (FIZZ's T1 screens, ui.h ui_tools_t, redrawn on the 120 x 120 grid;
 * the model is FIZZ's). Built through ui_layer.c (it includes this file). The head: the ten tools as 7 x 7 glyphs, the
 * chosen one in reverse video; a change pending (an orange light) or drawn only (the undo arrow crossed out); the
 * track's number in reverse video (PATTERN: a dotted box); the scope's picture. Under it one picture a tool, then the
 * eight pages and the lane at the foot (the edited page as it will be: solid kept, outlined new, raster-ghosted going):
 *   ROTATE    the sixteen steps as a ring of floor tiles, the agent running round it on the playing one; the offset
 *             big in KNOB 1's blue, its sweep inside the ring
 *   REVERSE   the steps as bars (their notes' heights) about the mirror
 *   RANDOM    a scrambled terminal (its hex the seed's), the die showing the seed; the four amounts as shafts in
 *             KNOB 1..4's colours (density, notes, velocity, chance)
 *   EUCLID    the robot's patrol: the ring, the hits its posts (KNOB 1's blue polygon), the robot on the playing
 *             position; hits big (KNOB 1), the length (KNOB 2), the rotation's mark (KNOB 3), the landing (KNOB 4)
 *   GRIDS     the pocket computer's map: nine rooms, the marker at X (KNOB 1) and Y (KNOB 2), chaos (KNOB 4) a cloud,
 *             the density (KNOB 3) a shaft; the three drums (PATTERN: the tracks they go to)
 *   HUMANIZE / QUANTIZE  the steps off their grid lines, the ghosts behind; the knobs' amounts as bars
 *   LENGTH    the eight pages, the steps big, halving and doubling
 *   STRETCH   two tapes (the loop as it was, as it is) and the pull between them; the factor or the pages it fits
 *   CLIP      punch cards: COPY, PASTE, CLEAR, what they act on
 * And the popup (a tool's name, the scope's takeover, a message, RANDOM's knob named). */
#include "../seq/tools.h"

static const char *const TL_NAME[QD_TL_N] = {"ROTATE", "REVERSE", "RANDOM", "EUCLID", "GRIDS", "HUMANIZE", "QUANTIZE",
                                                "LENGTH", "STRETCH", "CLIP"};
static const char *const TL_SCOPE[QD_TS_N] = {"SELECTED", "PAGE", "TRACK", "PATTERN"};

#define TL_TOP 13                                       /* the picture's area (120-space) */
#define TL_BOT 86
#define TL_LX(i) (4 + (i) * 7)                          /* the lane's (and the steps') columns: 6 wide, 7 apart */

static int tl_g(float v) { return (int)floorf(v * 0.5f + 0.25f); }   /* (a 240-space argument to the grid) */
static int tl_rnd(float v) { return (int)floorf(v + 0.5f); }

/* a small bitmap: rows of '#' (lit) / '.', top-left x, y, each cell sc x sc (runs of a row one rectangle) */
static void tl_bits(const char *const *rows, int nr, int x, int y, int sc, qd_rgb c)
{
    int r, i;
    for (r = 0; r < nr; r++)
        for (i = 0; rows[r][i]; i++)
            if (rows[r][i] == '#') {
                int n = 1;
                while (rows[r][i + n] == '#')
                    n++;
                px_rect(x + i * sc, y + r * sc, n * sc, sc, c);
                i += n - 1;
            }
}

/* a ghost (what goes): every other row, the C64's raster half tone */
static void tl_ghost(int x, int y, int w, int h, qd_rgb c)
{
    int j;
    for (j = 1; j < h; j += 2)
        px_hline(x, y + j, w, c);
    px_hline(x, y, w, c);
}

/* the division sign (the font has none): 5 wide, 5 tall at sc 1 */
static void tl_div(int x, int y, int sc, qd_rgb c)
{
    px_rect(x, y + 2 * sc, 5 * sc, sc, c);
    px_rect(x + 2 * sc, y, sc, sc, c);
    px_rect(x + 2 * sc, y + 4 * sc, sc, sc, c);
}

/* ---------------------------------------------------------------- the tools' glyphs, 7 x 7 */
static const char *const TL_ICON[QD_TL_N][7] = {
    {"..###.#", ".#...##", "#...###", "#......", "#.....#", ".#...#.", "..###.."},   /* ROTATE: a turning arrow */
    {"....#..", "#######", "....#..", ".......", "..#....", "#######", "..#...."},   /* REVERSE: arrows both ways */
    {"#######", "#.....#", "#.#...#", "#..#..#", "#...#.#", "#.....#", "#######"},   /* RANDOM: a die */
    {"...#...", "..#.#..", ".#...#.", ".#...#.", "#.....#", "#######", "......."},   /* EUCLID: a polygon of hits */
    {"#######", "#..#..#", "#..#.##", "#######", "#..#..#", "#..#..#", "#######"},   /* GRIDS: the map, the marker */
    {".......", ".#.....", "#.#..#.", "...#.#.", "....#..", ".......", "#######"},   /* HUMANIZE: a wobble, the grid */
    {"...#...", ".#.#...", ".#.#.#.", ".#.#.#.", ".#.#.#.", "#######", "......."},   /* QUANTIZE: bars on their lines */
    {"###.#.#", "#.#....", "#.#...#", "#.#....", "#.#...#", "###.#.#", "......."},   /* LENGTH: a page, a dotted one */
    {"#.....#", "#.....#", "#.#.#.#", "#######", "#.#.#.#", "#.....#", "#.....#"},   /* STRETCH: pulled apart */
    {".##.##.", "#.###.#", "#.....#", "#.###.#", "#.....#", "#.##..#", "#######"},   /* CLIP: a clipboard */
};
static void tl_icon(int i, int x, int y, qd_rgb c)        /* top-left x, y */
{
    if (i >= 0 && i < QD_TL_N)
        tl_bits(TL_ICON[i], 7, x, y, 1, c);
}

/* each tool as a little picture, centred at x, y (240-space, FIZZ's call) */
void ui_tools_icon(int i, float x, float y, int on)
{
    tl_icon(i, tl_g(x) - 3, tl_g(y) - 3, on ? UI_K3 : UI_G2);
}

/* the scope, 13 x 7 at sc 1 (centred at cx, cy): SELECTED a few steps picked out of a row, PAGE one block, TRACK a long
 * lane, PATTERN the lanes */
static void tl_scope_icon(int sc, int cx, int cy, qd_rgb col, int s)
{
    const int x0 = cx - 6 * s, y0 = cy - 3 * s;
    if (sc == QD_TS_SEL) {
        px_rect(x0, y0 + 2 * s, 3 * s, 3 * s, col);
        px_box(x0 + 5 * s, y0 + 2 * s, 3 * s, 3 * s, UI_G2);
        px_rect(x0 + 10 * s, y0 + 2 * s, 3 * s, 3 * s, col);
    } else if (sc == QD_TS_PAGE) {
        px_rect(x0 + 4 * s, y0, 5 * s, 7 * s, col);
        px_rect(x0, y0 + 3 * s, 3 * s, s, UI_G2);
        px_rect(x0 + 10 * s, y0 + 3 * s, 3 * s, s, UI_G2);
    } else if (sc == QD_TS_TRACK) {
        px_rect(x0, y0 + s, 13 * s, 5 * s, col);
    } else {
        static const uint8_t TR[4] = {0, 1, 3, 4};
        int k;
        for (k = 0; k < 4; k++)
            px_rect(x0, y0 + 2 * k * s, 13 * s, s, k == 1 ? col : UI_TRACK_PAL[TR[k]]);
    }
}
void ui_tools_scope_icon(int sc, float x, float y, qd_rgb col, float s)
{
    tl_scope_icon(sc, tl_g(x), tl_g(y), col, s >= 1.6f ? 2 : 1);
}

/* ---------------------------------------------------------------- the head */
static void tl_head(const ui_tools_t *v)
{
    int i;
    for (i = 0; i < QD_TL_N; i++) {                     /* (ten, 8 apart; the chosen in reverse video) */
        const int x = 1 + i * 8;
        if (i == v->tool) {
            px_rect(x - 1, 1, 9, 9, UI_K3);
            tl_icon(i, x, 2, UI_BG);
        } else {
            tl_icon(i, x, 2, UI_G2);
        }
    }
    if (v->nolive) {                                    /* drawn only: no undo (the arrow crossed out) */
        ui_icon(UI_IC_UNDO, 170.f, 11.f, 1.f, UI_KSEL);
        px_line(82, 9, 88, 2, UI_KSEL);
    } else if (v->pending) {                            /* a change pending (heard and seen): a light, beating */
        const int big = (int)(v->t * 4.f) & 1;
        px_rect(83, 4, 3, 3, UI_K4);
        if (big)
            px_box(82, 3, 5, 5, UI_K4);
    }
    if (v->scope != QD_TS_PATTERN) {                    /* the track's number, reverse video */
        char n[12];
        px_rect(91, 1, 9, 9, v->tcol);
        px_ctext(96, 3, ui_itoa(n, v->track), UI_BG, PX_FS, 1);
    } else {
        px_dots(91, 1, 9, UI_G1, 2);
        px_dots(91, 9, 9, UI_G1, 2);
        px_vdots(91, 3, 6, UI_G1, 2);
        px_vdots(99, 3, 6, UI_G1, 2);
    }
    tl_scope_icon(v->scope, 110, 5, UI_K3, 1);
    px_dots(0, 11, 120, UI_G3, 2);
}

/* ---------------------------------------------------------------- the foot: the pages, the lane */
static int tl_noteh(const ui_tools_t *v, int i, int span)   /* a step's note as a height 0..span */
{
    return v->note[i] && v->hi > v->lo ? span * (v->note[i] - v->lo) / (v->hi - v->lo) : 0;
}

/* the eight pages over the lane: the one shown tall in the track's colour, the looped ones grey, the others dark */
static void tl_pages(const ui_tools_t *v)
{
    int p;
    for (p = 0; p < 8; p++) {
        const int x = 4 + p * 14;
        if (p == v->page)
            px_rect(x, 89, 13, 4, v->tcol);
        else
            px_rect(x, 90, 13, 2, (v->loop >> p) & 1 ? UI_G2 : UI_G3);
    }
}

/* the lane: the page as it will be (kept solid, new outlined white, going a ghost), the playhead red under it, the
 * selected steps marked yellow over it */
static void tl_lane(const ui_tools_t *v, int heights)
{
    int i;
    for (i = 0; i < 16; i++) {
        const int x = TL_LX(i), h = heights ? 5 + tl_noteh(v, i, 7) : 12, y = 112 - h;
        if (!((v->inr >> i) & 1u))                      /* (a step that does not play: a speck) */
            px_rect(x + 2, 110, 2, 2, UI_G3);
        else if (v->on[i] && v->was[i])
            px_rect(x, y, 6, h, v->tcol);
        else if (v->on[i])
            px_box(x, y, 6, h, UI_K3);
        else if (v->was[i])
            tl_ghost(x, y, 6, h, v->tcol);
        else
            px_rect(x, 109, 6, 3, i % 4 ? UI_G3 : UI_G2);
        if ((v->sel >> i) & 1)
            px_rect(x + 1, 96, 4, 2, UI_KSEL);
    }
    if (v->play >= 0 && v->play < 16)
        px_rect(TL_LX(v->play) + 1, 114, 4, 2, PX_LRED);
}

/* ---------------------------------------------------------------- ROTATE: the ring of floor tiles */
#define TL_RCX 60
#define TL_RCY 55
static void tl_ell(int i, int n, int rx, int ry, int *x, int *y)   /* point i of n round an ellipse, from the top */
{
    const float a = -UI_PI / 2.f + (float)i * UI_TAU / (float)n;
    *x = TL_RCX + tl_rnd(cosf(a) * (float)rx);
    *y = TL_RCY + tl_rnd(sinf(a) * (float)ry);
}
static void tl_tile(int cx, int y, qd_rgb c)           /* a floor tile, 12 wide: the girder, its underside */
{
    px_rect(cx - 6, y, 12, 2, c);
    px_dots(cx - 5, y + 2, 11, c, 2);
}
static void tl_rotate(const ui_tools_t *v)
{
    char s[16];
    int i, x, y, off = v->rot;
    for (i = 0; i < 16; i++) {                          /* the tiles: a trig in the track's colour, the first white */
        tl_ell(i, 16, 46, 24, &x, &y);
        tl_tile(x, y - 1, v->on[i] ? v->tcol : i == 0 ? UI_K3 : UI_G2);
    }
    off = off > 16 ? 16 : off < -16 ? -16 : off;        /* (the sweep: at most a turn) */
    for (i = 0; i <= (off < 0 ? -off : off) * 6; i++) {
        tl_ell(off < 0 ? -i : i, 96, 31, 13, &x, &y);
        px_pset(x, y, UI_K1);
    }
    if (off) {                                          /* (its end: a block) */
        tl_ell(off * 6, 96, 31, 13, &x, &y);
        px_rect(x - 1, y - 1, 3, 3, UI_K1);
    }
    s[0] = v->rot > 0 ? '+' : 0, s[1] = 0;
    ui_itoa(s + (v->rot > 0), v->rot);
    px_ctext(TL_RCX, TL_RCY - 7, s, UI_K1, PX_FB, 2);
    if (v->play >= 0 && v->play < 16) {                 /* the agent on the playing tile, running round */
        const int p = v->play, fr = PXS_AGENT_RUN1 + (int)(v->t * 10.f) % 3;
        tl_ell(p, 16, 46, 24, &x, &y);
        px_sprite(x - 4, y - 17, fr, p >= 4 && p < 12);
    } else {                                            /* (stopped: waiting on the first) */
        tl_ell(0, 16, 46, 24, &x, &y);
        px_sprite(x - 4, y - 17, PXS_AGENT_STAND, 0);
    }
    tl_lane(v, 0);
}

/* ---------------------------------------------------------------- REVERSE: the bars about the mirror */
static void tl_reverse(const ui_tools_t *v)
{
    const qd_rgb mc = v->flip ? UI_K4 : UI_K3;
    const int cy = 59;
    int i;
    px_vdots(59, TL_TOP + 1, TL_BOT - TL_TOP - 1, mc, 2);
    px_sprite(49, 15, PXS_AGENT_STAND, 0);              /* (the agent and his reflection) */
    px_sprite(61, 15, PXS_AGENT_STAND, 1);
    px_rect(45, 31, 29, 2, UI_G2);
    px_dots(46, 33, 28, UI_G2, 2);
    for (i = 0; i < 16; i++) {
        const int x = TL_LX(i), h = 8 + tl_noteh(v, i, 26);
        if (v->on[i])
            px_rect(x, cy - h / 2, 6, h, v->tcol);
        else
            px_rect(x + 2, cy - 1, 2, 2, UI_G2);
    }
    tl_lane(v, 1);
}

/* ---------------------------------------------------------------- RANDOM: the scrambled terminal */
static void tl_shaft(int x, int y, int w, int h, int num, int den, qd_rgb col)   /* a meter filled num / den from the foot */
{
    int f = den > 0 ? (h - 2) * (num < 0 ? 0 : num > den ? den : num) / den : 0;
    px_box(x, y, w, h, UI_G3);
    if (f > 0)
        px_rect(x + 1, y + h - 1 - f, w - 2, f, col);
}
static void tl_random(const ui_tools_t *v)
{
    static const char HEX[] = "0123456789ABCDEF";
    static const uint8_t PIP[6][6] = {{4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8}};
    static const qd_rgb COL[4] = {UI_K1, UI_K2, UI_K3, UI_K4};
    static const int MAX[4] = {100, 12, 64, 100};
    const int face = (v->seed >> 3) % 6;
    int k, r, c;
    px_rbox(3, 14, 48, 38, UI_K3, PX_LGREY);            /* the terminal: its bezel, its screen */
    px_rect(6, 17, 42, 28, UI_BG);
    for (r = 0; r < 4; r++)                             /* (the scramble: the seed's hex) */
        for (c = 0; c < 10; c++) {
            const uint32_t h = (uint32_t)(v->seed + 1) * 2654435761u ^ (uint32_t)(r * 16 + c) * 40503u;
            char ch[2] = {HEX[(h >> 13) & 15u], 0};
            if (((h >> 7) & 7u) == 0u)
                continue;
            px_text(7 + c * 4, 18 + r * 7, ch, (h >> 21) & 3u ? PX_GREEN : PX_LGREEN, PX_FS, 1);
        }
    px_rect(20, 52, 14, 2, UI_G1);                      /* (its foot) */
    px_rect(14, 54, 26, 2, UI_G2);
    px_rbox(18, 60, 17, 17, UI_K3, UI_K3);             /* the die: the seed's face */
    for (k = 0; k <= face; k++)
        px_rect(20 + (PIP[face][k] % 3) * 5, 62 + (PIP[face][k] / 3) * 5, 3, 3, UI_BG);
    for (k = 0; k < 4; k++) {                           /* the amounts, what each sets under it */
        const int x = 57 + k * 16, cx = x + 5, y = 80;
        tl_shaft(x, TL_TOP + 2, 11, 60, v->r[k], MAX[k], COL[k]);
        if (k == 0) {                                   /* (trigs) */
            px_rect(cx - 5, y - 2, 3, 4, COL[0]);
            px_box(cx - 1, y - 2, 3, 4, UI_G3);
            px_rect(cx + 3, y - 2, 3, 4, COL[0]);
        } else if (k == 1) {                            /* (a note) */
            ui_icon(UI_IC_NOTE + 2, (float)(2 * cx + 1), (float)(2 * y - 1), 1.f, COL[1]);
        } else if (k == 2) {                            /* (a wedge) */
            int j;
            for (j = 0; j < 7; j++)
                px_vline(cx - 3 + j, y + 2 - j * 5 / 6, j * 5 / 6 + 1, COL[2]);
        } else {                                        /* (a pie) */
            px_circle(cx, y, 3, COL[3], QD_NONE);
            px_rect(cx, y - 3, 3, 4, COL[3]);
        }
    }
    tl_lane(v, 0);
}

/* ---------------------------------------------------------------- EUCLID: the robot's patrol */
#define TL_ECX 50
#define TL_ECY 51
#define TL_ER 28
static void tl_ring_xy(int i, int n, int r, int *x, int *y)
{
    const float a = -UI_PI / 2.f + (float)i * UI_TAU / (float)n;
    *x = TL_ECX + tl_rnd(cosf(a) * (float)r);
    *y = TL_ECY + tl_rnd(sinf(a) * (float)r);
}
static void tl_euclid(const ui_tools_t *v)
{
    const int n = v->len < 1 ? 1 : v->len > QD_NSTEP ? QD_NSTEP : v->len;
    int poly[2 * 64], i, np = 0, m, x, y;
    char s[12];
    for (i = 0; i < n; i++)                             /* the hits' polygon (its corners the posts) */
        if (qd_tl_euclid_hit(i, v->hits, n, v->erot) && np < 64) {
            tl_ring_xy(i, n, TL_ER, &poly[2 * np], &poly[2 * np + 1]);
            np++;
        }
    px_circle(TL_ECX, TL_ECY, TL_ER, UI_G3, QD_NONE);
    if (np > 2)
        px_poly(poly, np, 1, UI_K1, QD_NONE);
    else if (np == 2)
        px_line(poly[0], poly[1], poly[2], poly[3], UI_K1);
    for (i = 0; i < n; i++) {
        const int hit = qd_tl_euclid_hit(i, v->hits, n, v->erot);
        tl_ring_xy(i, n, TL_ER, &x, &y);
        if (hit)
            px_rect(x - (n > 32 ? 0 : 1), y - (n > 32 ? 0 : 1), n > 32 ? 1 : 3, n > 32 ? 1 : 3, UI_K1);
        else if (n <= 32)
            px_rect(x, y, 1, 1, i == 0 ? UI_K3 : UI_G2);
    }
    if (v->erot) {                                      /* the rotation's mark, outside the ring (KNOB 3) */
        tl_ring_xy(-v->erot, n, TL_ER + 5, &x, &y);
        px_rect(x - 1, y - 1, 2, 2, UI_K3);
    }
    px_ctext(TL_ECX, TL_ECY - 11, ui_itoa(s, v->hits), UI_K1, PX_FB, 2);
    px_ctext(TL_ECX, TL_ECY + 6, ui_itoa(s, n), UI_K2, PX_FB, 1);
    if (v->epos >= 0) {                                 /* the robot on its round (lit on a post) */
        const int hit = qd_tl_euclid_hit(v->epos % n, v->hits, n, v->erot);
        tl_ring_xy(v->epos % n, n, TL_ER, &x, &y);
        px_sprite(x - 5, y - 6, hit ? PXS_ROBOT2 : PXS_ROBOT1, 0);
    } else {                                            /* (stopped: parked by the ring) */
        px_sprite(TL_ECX + TL_ER + 2, TL_ECY + TL_ER - 10, PXS_ROBOT1, 1);
    }
    for (m = 0; m < QD_EU_N; m++) {                     /* the landing (KNOB 4): NEW, ADD (a plus), MASK (an inner box) */
        const int bx = 104, by = 20 + m * 13, on = m == v->emode;
        const qd_rgb col = on ? UI_K4 : UI_G2;
        if (m == QD_EU_NEW && on)
            px_rect(bx, by, 9, 9, col);
        else
            px_box(bx, by, 9, 9, col);
        if (m == QD_EU_ADD) {
            px_vline(bx + 4, by + 2, 5, col);
            px_hline(bx + 2, by + 4, 5, col);
        } else if (m == QD_EU_MASK) {
            px_rect(bx + 3, by + 3, 3, 3, col);
        }
    }
    tl_lane(v, 0);
}

/* ---------------------------------------------------------------- GRIDS: the pocket computer's map */
static void tl_grids(const ui_tools_t *v)
{
    const int X = 4, Y = 15, S = 66, mx = X + 2 + v->r[0] * (S - 5) / 255, my = Y + 2 + (255 - v->r[1]) * (S - 5) / 255;
    int a, b, p;
    px_box(X, Y, S, S, UI_G2);
    for (a = 0; a < 3; a++) {                           /* the nine rooms, the corridors between them */
        const int c = X + 9 + a * 24;
        px_vdots(c + 1, Y + 12, S - 22, UI_G3, 2);
        px_dots(X + 12, c - X + Y + 1, S - 22, UI_G3, 2);
        for (b = 0; b < 3; b++)
            px_box(c - 1, Y + 8 + b * 24, 5, 4, UI_G2);
    }
    px_vline(mx, Y + 1, S - 2, UI_K1);
    px_hline(X + 1, my, S - 2, UI_K2);
    for (b = 0; b < 12 && v->r[3]; b++) {               /* (the cloud: fixed points, its size the chaos) */
        const float an = (float)b * 2.39996f;
        const float rr = 4.5f + (float)v->r[3] / 255.f * 17.f * (0.3f + 0.7f * (float)((b * 7) % 12) / 11.f);
        const int cx = mx + tl_rnd(cosf(an) * rr), cy = my + tl_rnd(sinf(an) * rr);
        if (cx > X && cx < X + S - 1 && cy > Y && cy < Y + S - 1)
            px_pset(cx, cy, UI_KSEL);
    }
    px_rect(mx - 2, my - 2, 5, 5, UI_K3);               /* (the marker) */
    px_pset(mx, my, UI_BG);
    tl_shaft(75, Y, 10, S, v->r[2], 255, UI_K3);        /* the density (KNOB 3) */
    for (p = 0; p < 3; p++) {                           /* the drums: LOW, MID, HIGH (the part's lit) */
        static const int R[3] = {6, 4, 3};
        const int y = 25 + p * 22, on = v->scope == QD_TS_PATTERN ? v->gtr[p] >= 0 : p == v->part;
        const qd_rgb col = on ? UI_K4 : UI_G2;
        px_circle(103, y, R[p], col, on ? col : QD_NONE);
        if (p == 2)
            px_hline(98, y - 5, 11, col);
        if (v->scope == QD_TS_PATTERN && v->gtr[p] >= 0) {   /* (the track it goes to) */
            char n[12];
            px_text(112, y - 2, ui_itoa(n, v->gtr[p] + 1), UI_TRACK_PAL[v->gtr[p] & 7], PX_FS, 1);
        }
    }
    tl_lane(v, 0);
}

/* ---------------------------------------------------------------- HUMANIZE and QUANTIZE */
static void tl_hbar(int x, int y, int w, int num, int den, qd_rgb col)   /* a bar filled num / den */
{
    const int f = den > 0 ? (w - 2) * (num < 0 ? 0 : num > den ? den : num) / den : 0;
    px_box(x, y, w, 7, UI_G3);
    if (f > 0)
        px_rect(x + 1, y + 1, f, 5, col);
}
static void tl_timing(const ui_tools_t *v)
{
    const int q = v->tool == QD_TL_QUANTIZE, base = 81, by = 100;
    char s[12];
    int i, k;
    for (i = 0; i < 16; i++) {                          /* the steps' lines; QUANTIZE: the grid's ticks in each */
        const int x = TL_LX(i) + 3;
        px_vline(x, TL_TOP + 3, base - TL_TOP - 3, i % 4 ? UI_G3 : UI_G2);
        for (k = 1; q && k <= v->r[1]; k++)
            px_vline(TL_LX(i) + 3 + 7 * k / (v->r[1] + 1), base - 3, 3, UI_K2);
    }
    for (i = 0; i < 16; i++)
        if (v->on[i]) {
            const int x = TL_LX(i) + 3, dx = v->micro[i] * 7 / 24, h = 3 + v->vel[i] * 58 / 127;
            if (v->micro0[i] != v->micro[i] || v->vel0[i] != v->vel[i]) {   /* (the ghost: as it was, outlined) */
                const int h0 = 3 + v->vel0[i] * 58 / 127;
                px_box(x + v->micro0[i] * 7 / 24 - 1, base - h0, 3, h0, UI_G1);
            }
            px_rect(x + dx - 1, base - h, 3, h, v->tcol);
            if (dx)
                px_hline(dx < 0 ? x + dx : x, base + 2, dx < 0 ? -dx + 1 : dx + 1, UI_K1);
        }
    if (!q) {                                           /* the jitters: KNOB 1 the timing, KNOB 2 the velocity */
        tl_hbar(4, by, 40, v->r[0], 12, UI_K1);
        px_text(46, by + 1, ui_itoa(s, v->r[0]), UI_K1, PX_FS, 1);
        tl_hbar(62, by, 40, v->r[1], 40, UI_K2);
        px_text(104, by + 1, ui_itoa(s, v->r[1]), UI_K2, PX_FS, 1);
        return;
    }
    tl_hbar(4, by, 34, v->r[0], 100, UI_K1);            /* the amount (KNOB 1), the grid (KNOB 2: a step cut in its
                                                         * parts), the velocity's (KNOB 3) */
    px_box(45, by - 1, 29, 9, UI_K2);
    for (k = 1; k <= v->r[1]; k++)
        px_vline(45 + 29 * k / (v->r[1] + 1), by - 1, 9, UI_K2);
    tl_hbar(82, by, 34, v->r[2], 100, UI_K3);
}

/* ---------------------------------------------------------------- LENGTH: the pages */
static void tl_length(const ui_tools_t *v)
{
    char s[12];
    int p;
    for (p = 0; p < 8; p++) {                           /* (looped solid, new outlined white, going a ghost) */
        const int x = 5 + p * 14, y = 16, H = 56, now = (v->loop >> p) & 1, was = (v->loop0 >> p) & 1;
        char d[2] = {(char)('1' + p), 0};
        if (now && was)
            px_rect(x, y, 12, H, v->tcol);
        else if (now)
            px_box(x, y, 12, H, UI_K3);
        else if (was)
            tl_ghost(x, y, 12, H, v->tcol);
        else
            px_box(x, y, 12, H, UI_G3);
        px_ctext(x + 6, y + 2, d, now && was ? UI_BG : now ? UI_K3 : UI_G2, PX_FS, 1);
    }
    px_ctext(60, 82, ui_itoa(s, v->count), UI_K1, PX_FB, 3);
    {   /* KNOB 1's two ways: halving left, doubling right (the way taken lit) */
        const qd_rgb l = v->lpow < 0 ? UI_K1 : UI_G2, r = v->lpow > 0 ? UI_K1 : UI_G2;
        px_text(4, 89, "\xe2\x86\x90", l, PX_FB, 1);
        tl_div(13, 90, 1, l);
        px_text(19, 89, "2", l, PX_FB, 1);
        px_text(94, 89, "\xc3\x97" "2", r, PX_FB, 1);
        px_rtext(116, 89, "\xe2\x86\x92", r, PX_FB, 1);
    }
}

/* ---------------------------------------------------------------- STRETCH: two tapes */
static int tl_sx(int s, int n) { return 4 + s * 112 / n; }
/* a tape: the loop's steps on a strip of tape (its sprocket holes), its trigs marked; *x0, *x1 its ends */
static void tl_tape(const uint8_t *on, const uint8_t *in, int y, int h, int page, qd_rgb col, int *x0, int *x1)
{
    const int s0 = page >= 0 ? page * 16 : 0, n = page >= 0 ? 16 : 128;
    int s, first = -1, last = 0;
    for (s = 0; s < n; s++) {
        const int k = s0 + s;
        if (in[k >> 3] >> (k & 7) & 1)
            first = first < 0 ? s : first, last = s;
    }
    first = first < 0 ? 0 : first;
    *x0 = tl_sx(first, n), *x1 = tl_sx(last + 1, n);
    px_rect(*x0, y, *x1 - *x0, h, PX_BROWN);
    px_dots(*x0 + 1, y, *x1 - *x0 - 1, UI_BG, 3);       /* (its sprocket holes) */
    px_dots(*x0 + 1, y + h - 1, *x1 - *x0 - 1, UI_BG, 3);
    for (s = first; s <= last; s++) {
        const int k = s0 + s, x = tl_sx(s, n), w = tl_sx(s + 1, n) - x;
        if ((in[k >> 3] >> (k & 7) & 1) && (on[k >> 3] >> (k & 7) & 1))
            px_rect(w > 3 ? x + 1 : x, y + 2, w > 3 ? w - 2 : 1, h - 4, col);
    }
}
static void tl_stretch(const ui_tools_t *v)
{
    const int page = v->scope <= QD_TS_PAGE ? v->page : -1, f = v->fit;
    const qd_rgb kc = f ? UI_K2 : UI_K1;
    int b0, b1, a0, a1, k, tw, tx;
    for (k = 1; page < 0 && k < 8; k++) {               /* (the pages' bounds: notches by the tapes) */
        px_vline(4 + k * 14, 14, 2, UI_G2);
        px_vline(4 + k * 14, 78, 2, UI_G2);
    }
    tl_tape(v->ton0, v->tin0, 16, 9, page, UI_G1, &b0, &b1);   /* as it was */
    tl_tape(v->ton, v->tin, 63, 13, page, v->tcol, &a0, &a1);  /* as it is */
    px_line(b0, 26, a0, 62, kc);                        /* the pull */
    px_line(b1 - 1, 26, a1 - 1, 62, kc);
    if (f) {                                            /* (onto f pages: the number, a page beside it) */
        char t[12];
        ui_itoa(t, f);
        tw = px_text_w(t, PX_FB, 2) + 10;
        tx = 60 - tw / 2;
        px_rect(tx - 3, 33, tw + 6, 18, UI_BG);
        px_text(tx, 35, t, UI_K2, PX_FB, 2);
        px_box(tx + tw - 7, 37, 7, 10, UI_K2);
        px_hline(tx + tw - 5, 40, 3, UI_K2);
    } else {                                            /* (the factor: the sign and the number centred together) */
        static const char *const RN[7] = {"1", "1.25", "1.33", "1.5", "2", "3", "4"};
        const char *n = RN[v->lpow < 0 ? -v->lpow : v->lpow];
        const qd_rgb c = v->lpow ? UI_K1 : UI_G2;
        tw = 12 + px_text_w(n, PX_FB, 2);
        tx = 60 - tw / 2;
        px_rect(tx - 3, 33, tw + 6, 18, UI_BG);
        if (v->lpow < 0)
            tl_div(tx, 37, 2, c);
        else
            px_text(tx - 2, 35, "\xc3\x97", c, PX_FB, 2);
        px_text(tx + 12, 35, n, c, PX_FB, 2);
        for (k = -6; k <= 6; k++) {                     /* (KNOB 1's rung: a dot each, x1 a tall tick) */
            const int x = 60 + k * 5;
            if (!k)
                px_vline(x, 52, 5, k == v->lpow ? UI_K1 : UI_G2);
            else if (k == v->lpow)
                px_rect(x - 1, 53, 3, 3, UI_K1);
            else
                px_pset(x, 54, UI_G2);
        }
    }
    tl_lane(v, 0);
}

/* ---------------------------------------------------------------- CLIP: the punch cards */
static const char *tl_clip_on(char *s, int sc, int act, int page)   /* what the action acts on */
{
    if (sc == QD_TS_PATTERN || (act == QD_CL_CLEAR && sc != QD_TS_PAGE))
        return TL_SCOPE[sc];
    s[0] = 'P', s[1] = 'A', s[2] = 'G', s[3] = 'E', s[4] = ' ', s[5] = (char)('1' + (page & 7)), s[6] = 0;
    return s;
}
static void tl_card(int x, int y, int s, qd_rgb col)    /* a punch card, 11 x 7 at s 1: its corner cut, its holes */
{
    int k;
    px_rect(x, y, 11 * s, 7 * s, col);
    px_rect(x, y, s, s, UI_BG);
    for (k = 0; k < 4; k++) {
        px_rect(x + (2 + 2 * k) * s, y + 2 * s, s, s, UI_BG);
        px_rect(x + (3 + 2 * k) * s, y + 4 * s, s, s, UI_BG);
    }
}
static void tl_clip_icon(int k, int cx, int cy, int s, qd_rgb col)
{
    if (k == QD_CL_COPY) {                              /* two cards */
        px_box(cx - 3 * s, cy - 6 * s, 11 * s, 7 * s, col);
        px_rect(cx - 7 * s, cy - 3 * s, 12 * s, 8 * s, UI_BG);
        tl_card(cx - 6 * s, cy - 2 * s, s, col);
    } else if (k == QD_CL_PASTE) {                      /* a card going down into the tray */
        static const char *const AR[5] = {"..#..", "..#..", "#.#.#", ".###.", "..#.."};
        tl_card(cx - 5 * s, cy - 9 * s, s, col);
        tl_bits(AR, 5, cx - 2 * s, cy - s, s, col);
        px_rect(cx - 7 * s, cy + 3 * s, s, 3 * s, col);
        px_rect(cx + 6 * s, cy + 3 * s, s, 3 * s, col);
        px_rect(cx - 7 * s, cy + 6 * s, 14 * s, s, col);
    } else {                                            /* a card struck through */
        int j;
        px_box(cx - 6 * s, cy - 4 * s, 12 * s, 8 * s, col);
        for (j = 0; j < s; j++) {
            px_line(cx - 7 * s + j, cy - 6 * s, cx + 6 * s + j, cy + 5 * s, col);
            px_line(cx + 6 * s + j, cy - 6 * s, cx - 7 * s + j, cy + 5 * s, col);
        }
    }
}
static void tl_clip(const ui_tools_t *v)
{
    static const char *const CN[QD_CL_N] = {"COPY", "PASTE", "CLEAR"};
    const int sc = v->act == QD_CL_CLEAR || v->scope == QD_TS_PATTERN ? v->scope : QD_TS_PAGE;
    qd_rgb ac = UI_K3;
    char s[8];
    const char *nm = tl_clip_on(s, v->scope, v->act, v->page);
    int k, w;
    for (k = 0; k < QD_CL_N; k++) {
        const int ok = (v->act_ok >> k) & 1, on = k == v->act;
        const qd_rgb col = !ok ? UI_G3 : on ? (k == QD_CL_CLEAR ? PX_LRED : UI_K3) : UI_G2;
        const int x = on ? 60 : k < v->act ? 17 : 103;
        if (k < v->act - 1 || k > v->act + 1)
            continue;
        if (on)
            ac = col;
        if (!on || !v->armed || ((int)(v->t * 6.f) & 1))   /* (CLEAR? waiting for the second press: blinking) */
            tl_clip_icon(k, x, on ? 31 : 32, on ? 2 : 1, on && v->armed ? UI_RED : col);
        if (on)
            px_ctext(x, 49, CN[k], col, PX_FB, 1);
        else
            px_ctext(x, 42, CN[k], col, PX_FS, 1);
    }
    w = 18 + px_text_w(nm, PX_FB, 1);                   /* what it acts on: the scope's picture, the name */
    tl_scope_icon(sc, 60 - w / 2 + 6, 66, ac, 1);
    px_text(60 - w / 2 + 18, 63, nm, ac, PX_FB, 1);
    for (k = 0; k < QD_CL_N; k++)                       /* (KNOB 1's place among the three) */
        if (k == v->act)
            px_rect(55 + k * 5, 77, 3, 3, UI_K1);
        else
            px_pset(56 + k * 5, 78, UI_G2);
    tl_lane(v, 0);
}

FZ_COLD void ui_tools_draw(const ui_tools_t *v)
{
    tl_head(v);
    if (v->tool != QD_TL_LENGTH)                        /* (LENGTH draws the pages itself) */
        tl_pages(v);
    switch (v->tool) {
    case QD_TL_ROTATE: tl_rotate(v); break;
    case QD_TL_REVERSE: tl_reverse(v); break;
    case QD_TL_EUCLID: tl_euclid(v); break;
    case QD_TL_LENGTH: tl_length(v); break;
    case QD_TL_STRETCH: tl_stretch(v); break;
    case QD_TL_CLIP: tl_clip(v); break;
    case QD_TL_RANDOM: tl_random(v); break;
    case QD_TL_HUMANIZE:
    case QD_TL_QUANTIZE: tl_timing(v); break;
    default: tl_grids(v); break;
    }
}

/* ---------------------------------------------------------------- the popup
 * a tool's name (kind 0), the scope's takeover (1: SELECTED / PAGE / TRACK / PATTERN as the terminal's menu, the chosen
 * in reverse video), a message (2: the tool's name and what happened), RANDOM's knob named (3). A dark band under the
 * words (the caller's alpha fades it) */
static const char *const TL_MSG[9] = {"KEPT", "REVERTED", "NO UNDO", "COPIED", "PASTED", "CLEAR?", "CLEARED",
                                       "TOO BIG", "FULL"};
static const qd_rgb TL_MSGC[9] = {UI_K2, UI_K4, UI_KSEL, UI_K2, UI_K2, PX_LRED, UI_K4, UI_KSEL, UI_KSEL};
static void tl_big(const char *s, int y, qd_rgb c)      /* a word as big as it fits: the 8 x 8 doubled, else the small
                                                         * one tripled */
{
    if (px_text_w(s, PX_FB, 2) <= 116)
        px_ctext(60, y, s, c, PX_FB, 2);
    else
        px_ctext(60, y - 1, s, c, PX_FS, 3);
}
void ui_tools_pop(int kind, int msg, int tool, int scope, int page)
{
    int k;
    if (kind == 3) {                                    /* RANDOM's knob turned: what it sets, in its colour, over the
                                                         * meters (owner-approved words) */
        static const char *const RN[4] = {"DENSITY", "NOTES", "VELOCITY", "CHANCE"};
        static const qd_rgb RC[4] = {UI_K1, UI_K2, UI_K3, UI_K4};
        px_rect(0, 13, 120, 13, UI_BG);
        px_ctext(60, 16, RN[msg & 3], RC[msg & 3], PX_FB, 1);
        px_dots(0, 25, 120, RC[msg & 3], 2);
        return;
    }
    if (kind != 1) {                                    /* (the messages after NO UNDO: under the scope's name) */
        char s[8];
        const int cl = kind && msg > 2;                 /* (CLIP's: what it acted on, the band over its scope line) */
        msg = msg < 0 || msg > 8 ? 0 : msg;
        px_rect(0, 40, 120, cl ? 46 : 40, UI_BG);
        px_dots(0, 40, 120, UI_G3, 2);
        px_dots(0, cl ? 85 : 79, 120, UI_G3, 2);
        tl_big(cl ? tl_clip_on(s, scope % QD_TS_N, msg == 5 || msg == 6 ? QD_CL_CLEAR : 0, page) : TL_NAME[tool % QD_TL_N],
               46, UI_K3);
        px_ctext(60, 66, kind ? TL_MSG[msg] : TL_SCOPE[scope % QD_TS_N], kind ? TL_MSGC[msg] : UI_G1, PX_FB, 1);
        return;                                         /* (the name alone: the scope it works on under it) */
    }
    px_rect(0, 13, 120, 107, UI_BG);
    for (k = 0; k < QD_TS_N; k++) {
        const int on = k == scope, y = 25 + k * 24;
        if (on)                                         /* (the chosen: reverse video) */
            px_rect(36, y - 6, 80, 13, UI_K3);
        px_text(40, y - 3, TL_SCOPE[k], on ? UI_BG : UI_G2, PX_FB, 1);
        tl_scope_icon(k, 18, y, on ? UI_K3 : UI_G2, 2);
    }
}
