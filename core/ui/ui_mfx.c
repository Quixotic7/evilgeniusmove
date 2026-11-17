/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the MIDI FX screen (ui.h ui_mfx_t; FIZZ's P1.6c screen redrawn on the 120 x 120 grid, the model
 * FIZZ's). Built through ui_layer.c (the core's source lists stay as they are).
 *   the head    the page dots, MIDI FX, the group's number in a hexagon in ALGORITHM's cyan (OFF: a dotted one), the
 *               routing bypassed a red OFF block, the tracks routed to the group as chips (the selected underlined)
 *   the rail    the eight slots as tiles: each its effect's 7 x 7 glyph in its colour; empty dotted; bypassed grey and
 *               struck; the selected raised in a white frame; a note passing flashes it in reverse video; the notes
 *               run along the rail as 2 x 2 dots (an ARP or REPEAT starts a stream, CHANCE drops some, HARM / CHORD
 *               stack three); a SELECTOR's block bracketed under it
 *   the middle  the selected effect's name, then its picture:
 *               ARP        the walk as lift platforms, the agent running along them; a keybed on the walk's scale at
 *                          the left, the held keys in their tracks' colours (a padlock: HOLD / 1-ST). Pages 3 and 4:
 *                          the MOD lane (16 cells, the notes it makes above) and the TRANSPOSE lane (bars)
 *               CHANCE     the percentage; the notes rolling along a floor, the robot at the trapdoor letting some by
 *               REPEAT     the robot pacing along the hits' posts (HITS), their heights the velocities (FADE)
 *               HARM       the voices as platforms from the keybed, their intervals at the right
 *               TRANSPOSE  the agent riding two lifts: SEMI, then OCT, to the lit floor
 *               SCALER     a keyboard: the scale's keys light, a note off the scale slides down onto it
 *               RANDOM     a die rolling over PITCH (a jumping note), VEL (a bar), LENGTH (a note growing)
 *               CHORD      the chord's name over a keyboard, its keys struck in turn
 *               MONO       four lanes into one: a new note cuts the one before
 *               SELECTOR   the lever throwing each note to one of the next slots
 *               NONE       an empty box, the ten effects orbiting it
 *   the foot    KNOB 1..4's labels and values in their colours (the turned one underlined)
 *   PRESETS turned: the types as a rolodex; ALGORITHM turned: the routing map (the tracks wired to the groups); a track
 *   on no group: OFF, its notes straight to the voices. A bypassed slot's picture is half-toned (the C64's dimming). */
#include <math.h>
#include <string.h>

#include "ui.h"
#include "px.h"
#include "../seq/chord.h"
#include "../seq/mfx.h"
#include "../seq/scale.h"

/* each effect's colour, the C64's: NONE grey, ARP yellow (the lifts), CHANCE red, REPEAT orange, HARM purple,
 * TRANSPOSE green, SCALER light grey, RANDOM light red, CHORD cyan, MONO light green, SELECTOR light blue */
const qd_rgb UI_MFX_COL[11] = {PX_GREY, PX_YELLOW, PX_RED, PX_ORANGE, PX_PURPLE, PX_GREEN,
                               PX_LGREY, PX_LRED, PX_CYAN, PX_LGREEN, PX_LBLUE};
#define MF_MINT UI_KALG                 /* a group: ALGORITHM's colour (ALGORITHM picks the track's group) */
static const qd_rgb MF_KC[4] = {UI_K1, UI_K2, UI_K3, UI_K4};
#define MF_RY 22                        /* the rail's line (120-space) */
#define MF_TW 11                        /* a slot's tile */
#define MF_TOP 38                       /* the picture's area */
#define MF_BOT 88

/* ---------------------------------------------------------------- the helpers */
static int mf_r(float x) { return (int)floorf(x + 0.5f); }
static int mf_g(float v) { return (int)floorf(v * 0.5f + 0.25f); }   /* FIZZ's 240-space to the grid */
static float mf_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static int mf_clampi(int x, int a, int b) { return x < a ? a : x > b ? b : x; }
static float mf_ease(float x)
{
    x = mf_clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}
static int mf_lerpi(int a, int b, float u) { return a + mf_r((float)(b - a) * u); }
static uint32_t mf_hash(uint32_t n)
{
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
static int mf_pct(int n) { return (int)(mf_hash((uint32_t)n) % 100u); }
static int mf_ms(float t) { return t > 0.f ? (int)(t * 1000.f) : 0; }
static int mf_rx(int i) { return 4 + i * 14; }          /* slot i's tile */
static int mf_black(int p) { return (int)((0x54au >> (((p % 12) + 12) % 12)) & 1u); }   /* C# D# F# G# A# */

/* a dotted line: Bresenham, `on` pixels lit of every `per` */
static void mf_dline(int x0, int y0, int x1, int y1, qd_rgb c, int on, int per)
{
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int e = dx + dy, n = 0, guard = 512;
    while (guard--) {
        int e2;
        if (n++ % per < on)
            px_pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        e2 = 2 * e;
        if (e2 >= dy) {
            e += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y0 += sy;
        }
    }
}
static void mf_dbox(int x, int y, int w, int h, qd_rgb c)   /* a dotted box */
{
    px_dots(x, y, w, c, 2);
    px_dots(x, y + h - 1, w, c, 2);
    px_vdots(x, y, h, c, 2);
    px_vdots(x + w - 1, y, h, c, 2);
}

/* a small bitmap, rows of '#', top-left at x, y, scale sc */
static void mf_bits(const char *const *rows, int nr, int x, int y, int sc, qd_rgb c)
{
    int r;
    for (r = 0; r < nr; r++) {
        int i = 0;
        while (rows[r][i]) {
            int n = 0;
            if (rows[r][i] != '#') {
                i++;
                continue;
            }
            while (rows[r][i + n] == '#')
                n++;
            px_rect(x + i * sc, y + r * sc, n * sc, sc, c);
            i += n;
        }
    }
}

/* the effects' glyphs, 7 x 7 (the C64's icons) */
static const char *const MF_GLYPH[QD_MFX_NTYPE][7] = {
    {"#.#.#.#", ".......", "#.....#", ".......", "#.....#", ".......", "#.#.#.#"},   /* NONE: an empty box */
    {".......", "....###", "....#..", "..###..", "..#....", "###....", "......."},   /* ARP: the stairs */
    {"##....#", "##...#.", "....#..", "...#...", "..#....", ".#...##", "#....##"},   /* CHANCE: % */
    {"#......", "#.#....", "#.#.#..", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#.#.#.#"},   /* REPEAT: the hits */
    {"#######", "#......", "#####..", "#......", "###....", "#......", "#......"},   /* HARM: the voices */
    {"...####", ".....##", "....#.#", "...#..#", "..#....", ".#.....", "#......"},   /* TRANSPOSE: up */
    {"#######", "#.#.#.#", "#.#.#.#", "#.#.#.#", "#..#..#", "#..#..#", "#######"},   /* SCALER: the keys */
    {"#######", "#.....#", "#.#.#.#", "#..#..#", "#.#.#.#", "#.....#", "#######"},   /* RANDOM: a die */
    {"...#...", "..###..", "...#...", ".......", ".#...#.", "###.###", ".#...#."},   /* CHORD: a triad */
    {"#......", ".#.....", "..#....", "#######", "..#....", ".#.....", "#......"},   /* MONO: into one */
    {"....###", "...#...", "..#....", "##..###", ".......", ".......", "....###"},   /* SELECTOR: the switch */
};
static void mf_glyph_px(int ty, int x, int y, int sc, qd_rgb c)
{
    if (ty >= 0 && ty < QD_MFX_NTYPE)
        mf_bits(MF_GLYPH[ty], 7, x, y, sc, c);
}

/* each type's small glyph, in a box of 2 s px (240-space; 7 x 7 pixels, doubled from s 14, tripled from 21) */
void ui_mfx_glyph(int ty, float cx, float cy, float s, qd_rgb col, float lw)
{
    const int sc = s >= 21.f ? 3 : s >= 14.f ? 2 : 1;
    (void)lw;
    if (ty <= QD_MFX_NONE || ty >= QD_MFX_NTYPE)
        return;
    mf_glyph_px(ty, mf_g(cx) - 7 * sc / 2, mf_g(cy) - 7 * sc / 2, sc, col);
}

/* a hexagon, its point up, radius r (120-space): each slanted edge drawn from its vertex outward (mirror-exact) */
static void mf_hexagon(int cx, int cy, int r, qd_rgb c, int dashed)
{
    const int w = (r * 7 + 4) / 8, h = r / 2, on = 1, per = dashed ? 2 : 1;
    mf_dline(cx, cy - r, cx + w, cy - h, c, on, per);
    mf_dline(cx, cy - r, cx - w, cy - h, c, on, per);
    mf_dline(cx, cy + r, cx + w, cy + h, c, on, per);
    mf_dline(cx, cy + r, cx - w, cy + h, c, on, per);
    mf_dline(cx + w, cy - h, cx + w, cy + h, c, on, per);
    mf_dline(cx - w, cy - h, cx - w, cy + h, c, on, per);
}
static void mf_hexn(int cx, int cy, int r, int n, qd_rgb c, int dashed)   /* .. with a group's number in it */
{
    mf_hexagon(cx, cy, r, c, dashed);
    if (n > 0) {
        const char s[2] = {(char)('0' + n % 10), 0};
        if (r >= 13)
            px_ctext(cx + 1, cy - 6, s, c, PX_FB, 2);
        else if (r >= 8)
            px_ctext(cx + 1, cy - 3, s, c, PX_FB, 1);
        else
            px_ctext(cx + 1, cy - 2, s, c, PX_FS, 1);
    }
}
void ui_mfx_hex(float x, float y, float r, int n, qd_rgb col, float lw, int dashed)
{
    const int rr = mf_r(r * 0.5f) < 3 ? 3 : mf_r(r * 0.5f);
    mf_hexn(mf_g(x), mf_g(y), rr, n, col, dashed);
    if (lw >= 2.5f)
        mf_hexagon(mf_g(x), mf_g(y), rr - 1, col, dashed);
}

/* a slot's tile, w square at x, y: its glyph in its colour; glow 0.5: a note going by (the glyph lit white), 1: a note
 * passing through it (reverse video) */
static void mf_tile(int x, int y, int w, int ty, int sel, int byp, float glow)
{
    const int sc = w >= 20 ? 2 : 1, gx = x + (w - 7 * sc) / 2, gy = y + (w - 7 * sc) / 2;
    if (!ty) {
        px_rect(x, y, w, w, UI_BG);
        px_rbox(x, y, w, w, sel ? UI_K3 : UI_G3, QD_NONE);
        return;
    }
    {
        const qd_rgb col = byp ? UI_G2 : UI_MFX_COL[ty];
        if (glow >= 0.9f && !byp) {
            px_rbox(x, y, w, w, col, col);
            mf_glyph_px(ty, gx, gy, sc, UI_BG);
        } else {
            px_rbox(x, y, w, w, col, UI_BG);
            mf_glyph_px(ty, gx, gy, sc, glow >= 0.4f && !byp ? UI_K3 : col);
        }
        if (sel)
            px_rbox(x - 1, y - 1, w + 2, w + 2, UI_K3, QD_NONE);
        if (byp)
            px_line(x + 1, y + w - 2, x + w - 2, y + 1, UI_G1);
    }
}

/* the vertical keybed beside the ARP's and HARM's drawings: a row a semitone (rows ya(p)..yb(p)); lit: fill[] */
static void mf_vkeys(int x, int lo, int hi, int (*yrow)(int, int, int), int a, int b, const qd_rgb *lit)
{
    int p;
    for (p = lo; p <= hi; p++) {
        int ya = yrow(p + 1, a, b) + 1;
        const int yb = yrow(p, a, b), bk = mf_black(p);
        const qd_rgb c = lit && lit[p - lo] != QD_NONE ? lit[p - lo] : bk ? UI_G2 : PX_LGREY;
        if (ya > yb)
            ya = yb;
        px_rect(x, ya, bk && (!lit || lit[p - lo] == QD_NONE) ? 5 : 8, yb - ya + 1, c);
        if (!bk && (p % 12 == 0 || p % 12 == 5) && yb - ya >= 2)   /* (E-F, B-C: a seam) */
            px_hline(x, yb, 8, UI_BG);
    }
}

/* ---------------------------------------------------------------- the head, the rail, the foot */
static void mf_head(int npg, int pg, int g, const uint8_t *tr, int ntr, int track, int off)
{
    int x = 4, i;
    if (npg > 1) {
        for (i = 0; i < npg; i++)
            px_rect(x + i * 4, 5, 2, 2, i == pg ? UI_K3 : UI_G2);
        x += npg * 4 + 2;
    }
    x = px_text(x, 4, "MIDI FX", UI_K3, PX_FS, 1) + 5;  /* (the hexagon's centre) */
    if (g > 0)
        mf_hexn(x, 6, 5, g, MF_MINT, 0);
    else if (g == 0)
        mf_hexagon(x, 6, 5, UI_G1, 1);
    if (off) {                                          /* the routing bypassed: OFF, reverse video */
        px_rect(x + 7, 3, 13, 7, UI_RED);
        px_text(x + 8, 4, "OFF", UI_K3, PX_FS, 1);
    }
    for (i = 0; i < ntr; i++) {                         /* the tracks on the group: chips, the selected underlined */
        const int n = tr[ntr - 1 - i], cx = 112 - i * 6;
        const char d[2] = {(char)('0' + n % 10), 0};
        px_rect(cx, 3, 5, 7, UI_TRACK_PAL[(n - 1) & 7]);
        px_text(cx + 1, 4, d, UI_BG, PX_FS, 1);
        if (n == track)
            px_hline(cx, 11, 5, UI_K3);
    }
    px_dots(0, 12, 120, UI_G3, 2);
}

/* notes running along the rail: a dot stream; an ARP or REPEAT starts a faster stream, CHANCE drops some, HARM / CHORD
 * stack three (FIZZ's railFlow, in whole pixels) */
static void mf_flow(const uint8_t *type, const uint8_t *byp, float t, float *hits)
{
    struct {
        float x, rate;
        qd_rgb col;
        int id;
    } st[9];
    float cx[8];
    int ns = 1, si, i;
    st[0].x = 1.f, st[0].rate = 1.7f, st[0].col = UI_K3, st[0].id = 0;
    for (i = 0; i < 8; i++) {
        cx[i] = (float)(mf_rx(i) + MF_TW / 2);
        if (type[i] && !byp[i] && (type[i] == QD_MFX_ARP || type[i] == QD_MFX_REPEAT))
            st[ns].x = cx[i], st[ns].rate = 6.f, st[ns].col = UI_MFX_COL[type[i]], st[ns].id = i + 1, ns++;
    }
    for (si = 0; si < ns; si++) {
        const float end = si + 1 < ns ? st[si + 1].x : 118.f, speed = 32.f, life = (end - st[si].x) / speed;
        int k;
        for (k = (int)floorf((t - life) * st[si].rate) - 1; k <= (int)floorf(t * st[si].rate); k++) {
            const float age = t - (float)k / st[si].rate, x = st[si].x + age * speed;
            qd_rgb col = st[si].col;
            int mult = 1, m, fell = -1, xi;
            if (age < 0.f || age > life)
                continue;
            for (i = 0; i < 8; i++) {
                const float sx = cx[i];
                if (!type[i] || byp[i] || sx <= st[si].x + 0.5f || sx > end)
                    continue;
                if (fabsf(x - sx) < 6.5f) {
                    const float h = 1.f - fabsf(x - sx) / 6.5f;
                    hits[i] = h > hits[i] ? h : hits[i];
                }
                if (sx > x)
                    continue;
                if (type[i] == QD_MFX_CHANCE && mf_pct(k * 7 + i * 3 + st[si].id * 17 + 1000) > 62) {
                    fell = mf_r(x - sx);
                    col = UI_MFX_COL[QD_MFX_CHANCE];
                    break;
                }
                if (type[i] == QD_MFX_HARM || type[i] == QD_MFX_CHORD)
                    mult = 3;
                col = UI_MFX_COL[type[i]];
            }
            xi = mf_r(x);
            if (fell >= 0) {                            /* (dropped: falling off the rail) */
                if (fell < 10)
                    px_rect(xi - 1, MF_RY - 1 + fell * 2 / 3, 2, 2, fell < 5 ? col : UI_G2);
                continue;
            }
            for (m = 0; m < mult; m++)
                px_rect(xi - 1, MF_RY - 1 + (m - (mult - 1) / 2) * 3, 2, 2, col);
        }
    }
}

/* P1.6e: where slot i shows while slot `from` is previewed at `to` (the ones between making room) */
static int mf_dpos(int i, int from, int to)
{
    if (from < to && i > from && i <= to)
        return i - 1;
    if (to < from && i >= to && i < from)
        return i + 1;
    return i;
}
static void mf_tri(int x, int y, int dir, qd_rgb c)    /* a small arrow head pointing dir (-1 / +1), its tip at x */
{
    px_pset(x, y, c);
    px_vline(x - dir, y - 1, 3, c);
    px_vline(x - 2 * dir, y - 2, 5, c);
}

static void mf_rail(const ui_mfx_t *v)
{
    float hits[8] = {0};
    int i;
    const int ty0 = MF_RY - MF_TW / 2;
    px_hline(0, MF_RY, 118, UI_G2);
    px_line(115, MF_RY - 3, 118, MF_RY, UI_G2);
    px_line(115, MF_RY + 3, 118, MF_RY, UI_G2);
    if (v->flow)
        mf_flow(v->type, v->byp, v->t, hits);
    for (i = 0; i < 8; i++) {                           /* a SELECTOR's block: its next slots bracketed */
        if (v->type[i] == QD_MFX_SELECTOR && !v->byp[i]) {
            const int n = v->v[QD_ML_LEN] < 0 ? 0 : v->v[QD_ML_LEN], e = i + n > 7 ? 7 : i + n;
            if (n > 0 && i < 7) {
                const int x1 = mf_rx(i + 1) - 1, x2 = mf_rx(e) + MF_TW;
                px_hline(x1, 30, x2 - x1 + 1, UI_MFX_COL[QD_MFX_SELECTOR]);
                px_pset(x1, 29, UI_MFX_COL[QD_MFX_SELECTOR]);
                px_pset(x2, 29, UI_MFX_COL[QD_MFX_SELECTOR]);
            }
        }
    }
    for (i = 0; i < 8; i++) {
        int x = mf_rx(i), y = ty0;
        const float g = v->glow[i] >= 0.5f ? 1.f : hits[i] >= 0.6f ? 0.5f : 0.f;   /* (the player's pass: reversed; a
                                                         * dot of the rail's going by: the glyph lit) */
        const int sel = i == v->sel && v->lift_from < 0;
        if (v->lift_from >= 0 && i == v->lift_to)       /* (the data already moved: the carried one is there) */
            continue;
        if (v->drag_from >= 0) {                        /* P1.6e: a slot held: drawn apart; the others making room */
            if (i == v->drag_from)
                continue;
            x = mf_lerpi(mf_rx(mf_dpos(i, v->drag_from, v->drag_prev)), mf_rx(mf_dpos(i, v->drag_from, v->drag_to)),
                         mf_ease(v->drag_u));
        }
        if (v->drop >= 0 && i == v->drop)               /* .. dropped: landing */
            y -= mf_r(4.f * (1.f - mf_ease(v->drop_u)));
        if (v->lift_from >= 0 && v->lift_to > v->lift_from && i >= v->lift_from && i < v->lift_to)
            x = mf_lerpi(mf_rx(i + 1), mf_rx(i), mf_ease((v->lift_u - 0.2f) / 0.5f));
        if (v->lift_from >= 0 && v->lift_to < v->lift_from && i > v->lift_to && i <= v->lift_from)
            x = mf_lerpi(mf_rx(i - 1), mf_rx(i), mf_ease((v->lift_u - 0.2f) / 0.5f));
        if (v->arm > 0.f && i == v->sel)                /* (armed: shaking) */
            x += sinf(v->t * 46.f) > 0.f ? 1 : -1;
        if (sel) {                                      /* the selected: raised, its shadow under it */
            y -= 2;
            px_hline(x + 1, MF_RY + MF_TW / 2 + 1, MF_TW - 2, UI_G2);
        }
        mf_tile(x, y, MF_TW, v->type[i], sel, v->byp[i], g);
        if (i == v->f1_src) {                           /* P1.6e: F1's copy came from here: marked */
            mf_dbox(x - 2, y - 2, MF_TW + 4, MF_TW + 4, UI_K2);
            ui_icon(UI_IC_COPY, (float)(2 * (x + MF_TW / 2) + 1), (float)(2 * (y + MF_TW + 6)), 1.f, UI_K2);
        }
        if (v->arm > 0.f && i == v->sel) {
            px_line(x - 1, y - 1, x + MF_TW, y + MF_TW, UI_RED);
            px_line(x + MF_TW, y - 1, x - 1, y + MF_TW, UI_RED);
        }
    }
    if (v->drag_from >= 0) {                            /* P1.6e: the slot held: lifted, at its preview place; arrows
                                                         * either side (it can move) */
        const int x = mf_lerpi(mf_rx(v->drag_prev), mf_rx(v->drag_to), mf_ease(v->drag_u));
        const int y = ty0 - mf_r(3.f * v->drag_up);
        if (v->drag_up > 0.05f)
            px_hline(x + 2, MF_RY + MF_TW / 2 + 1, MF_TW - 4, UI_G2);
        mf_tile(x, y, MF_TW, v->type[v->drag_from], 1, v->byp[v->drag_from], 0.f);
        if (v->drag_up > 0.3f) {
            const int off = v->drag_moved ? 0 : (sinf(v->t * 9.f) > 0.f ? 1 : 0);
            const qd_rgb c = v->drag_moved ? UI_G1 : UI_K3;
            if (v->drag_to > 0)
                mf_tri(x - 3 - off, y + MF_TW / 2, -1, c);
            if (v->drag_to < 7)
                mf_tri(x + MF_TW + 2 + off, y + MF_TW / 2, 1, c);
        }
    }
    if (v->lift_from >= 0) {                            /* picked up: raised above the rail, carried, dropped */
        const float u = v->lift_u;
        const float up = u < 0.15f ? mf_ease(u / 0.15f) : u > 0.75f ? 1.f - mf_ease((u - 0.75f) / 0.15f) : 1.f;
        const int x = mf_lerpi(mf_rx(v->lift_from), mf_rx(v->lift_to), mf_ease((u - 0.2f) / 0.5f));
        const int y = ty0 - mf_r(5.f * up);
        if (up > 0.05f)
            px_hline(x + 2, MF_RY + MF_TW / 2 + 1, MF_TW - 4, UI_G2);
        mf_tile(x, y, MF_TW, v->type[v->lift_to], 1, 0, 0.f);
    }
}

static void mf_foot(const ui_mfx_t *v, int dim)
{
    int k;
    px_dots(3, 89, 114, UI_G3, 2);
    for (k = 0; k < 4; k++) {
        const int x0 = 3 + k * 29, cx = x0 + 14;
        int f = PX_FB, w, x;
        if (!v->has[k])
            continue;
        px_ctext(cx + 1, 92, v->lab[k] ? v->lab[k] : "", dim ? UI_G2 : UI_G1, PX_FS, 1);
        if (v->last == k)
            px_hline(cx - 9, 99, 19, MF_KC[k]);
        w = px_text_w(v->val[k], PX_FB, 1);
        if (w > 27) {                                   /* (AS PLAYED, 100%: the small font, kept on the screen) */
            f = PX_FS;
            w = px_text_w(v->val[k], PX_FS, 1);
        }
        x = cx + 1 - (w + 1) / 2;
        x = x < 1 ? 1 : x + w > 119 ? 119 - w : x;
        px_text(x, f == PX_FB ? 105 : 107, v->val[k], MF_KC[k], f, 1);
    }
}

/* ---------------------------------------------------------------- the pictures (MF_TOP .. MF_BOT) */
/* ARP: a pitch's height, lo at y 85 .. lo + span at y 55 */
static int mf_ay(int p, int lo, int span) { return 85 - (p - lo) * 30 / span; }

/* P1.6g: the ARP's picture: the PATTERN's real walk (its notes in play, or C E G grey) as lift platforms over RANGE
 * octaves (the MOD lane's rests gaps, the TRANSPOSE lane's offsets in it), morphing from the last one when it
 * changes; the agent running along the playing step; at the left a keybed on the same scale, the notes the arp plays
 * from lit in their tracks' colours (a padlock over it in HOLD / 1-ST) */
static void mf_arp(const ui_mfx_t *v)
{
    const int ns = v->nseq > 0 ? (v->nseq < 48 ? v->nseq : 48) : 1, held = v->nheld > 0, ms = mf_ms(v->t);
    const int latched = v->v[QD_MA_MODE] == QD_AM_HOLD || v->v[QD_MA_MODE] == QD_AM_1ST;
    const int x0 = 17, xw = 99, gate = mf_clampi(v->v[QD_MA_GATE], 5, 100);
    const float mo = mf_ease(v->morph);
    int ys[48], i, lo = 127, hi = 0, span, cur, run;
    for (i = 0; i < ns; i++)
        lo = v->seq[i] < lo ? v->seq[i] : lo, hi = v->seq[i] > hi ? v->seq[i] : hi;
    for (i = 0; i < v->nheld && i < 8; i++)
        lo = v->held_p[i] < lo ? v->held_p[i] : lo, hi = v->held_p[i] > hi ? v->held_p[i] : hi;
    if (hi - lo > 30)                                   /* (a wide walk: its top 30 semitones) */
        lo = hi - 30;
    span = hi - lo;
    if (span < 12) {                                    /* (a narrow one: centred on an octave) */
        lo -= (12 - span) / 2;
        span = 12;
    }
    for (i = 0; i < ns; i++) {                          /* the platforms' heights, morphing from the last walk */
        const int yn = mf_clampi(mf_ay(v->seq[i], lo, span), 52, 85);
        if (mo < 1.f && v->nprev > 0)
            ys[i] = mf_lerpi(mf_clampi(mf_ay(v->prev[i < v->nprev ? i : v->nprev - 1], lo, span), 52, 85), yn, mo);
        else
            ys[i] = yn;
    }
    {   /* the keybed, lit: the notes held */
        qd_rgb lit[32];
        int p, k;
        for (p = 0; p <= span && p < 32; p++) {
            lit[p] = QD_NONE;
            for (k = 0; k < v->nheld && k < 8; k++)
                if (v->held_p[k] == lo + p)
                    lit[p] = UI_TRACK_PAL[(v->held_t[k] - 1) & 7];
        }
        mf_vkeys(4, lo, lo + (span < 31 ? span : 31), mf_ay, lo, span, lit);
    }
    if (latched) {                                      /* HOLD / 1-ST: the padlock (MODE: KNOB 1's, page 2) */
        static const char *const LOCK[8] = {"..###..", ".#...#.", ".#...#.", "#######", "###.###", "###.###", "#######",
                                            "......."};
        mf_bits(LOCK, 7, 5, 41, 1, UI_K1);
    }
    for (i = 1; i <= 2; i++)                            /* the octaves above the lowest note: dotted */
        if (lo + 12 * i <= hi)
            px_dots(x0, mf_ay(lo + 12 * i, lo, span), xw, UI_G3, 3);
    run = v->arp_run && held;
    cur = run ? v->arp_step % ns : (ms / 200) % ns;
    for (i = 0; i < ns; i++) {                          /* each step a platform, GATE wide; the lifts between */
        const int x = x0 + i * xw / ns, xn = x0 + (i + 1) * xw / ns;
        const int pw = mf_clampi((xn - x - 1) * gate / 100, 1, xn - x);
        const qd_rgb top = i == cur ? UI_K3 : held ? PX_YELLOW : PX_GREY, under = held ? PX_ORANGE : UI_G2;
        if (v->gap[i])
            continue;
        if (i > 0 && !v->gap[i - 1] && ys[i - 1] != ys[i])
            px_vline(x, ys[i - 1] < ys[i] ? ys[i - 1] : ys[i], (ys[i - 1] > ys[i] ? ys[i - 1] - ys[i] : ys[i] - ys[i - 1]) + 1,
                     UI_G3);
        px_hline(x, ys[i], pw, top);
        px_hline(x, ys[i] + 1, pw, under);
    }
    {   /* the agent: on the playing step's platform, running (over a rest: a somersault) */
        const int x = x0 + cur * xw / ns, xn = x0 + (cur + 1) * xw / ns;
        const int pw = mf_clampi((xn - x - 1) * gate / 100, 1, xn - x);
        const int f = run ? pw / 2 : (ms % 200) * pw / 200;
        const int ax = mf_clampi(x + f - 4, 13, 108);
        if (v->gap[cur])
            px_sprite(ax - 1, ys[cur] - 14, PXS_AGENT_FLIP, 0);
        else
            px_sprite(ax, ys[cur] - 15, PXS_AGENT_RUN1 + (ms / 90) % 3, 0);
    }
}

/* the arp's lanes: 16 cells, a cell 7 px */
static int mf_lx(int i) { return 4 + i * 7; }
static void mf_lane_len(int len)                        /* the lane's LENGTH (KNOB 4): a marker after its last cell */
{
    const int x = mf_lx(len) - 1;
    px_vline(x, 40, 48, UI_K4);
    px_hline(x - 2, 40, 2, UI_K4);
}
static void mf_modlane(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_ARP];
    const int len = mf_clampi(v->v[QD_MA_MLEN], 1, 16), sel = v->lane_sel & 15;
    const int cur = v->lane_cur >= 0 ? v->lane_cur : (mf_ms(v->t) / 200) % len;
    static const int SEQ[6] = {0, 4, 7, 12, 16, 19};
    int i, ai = 0, last[3] = {0, 0, 0}, nl = 0;
#define MF_LY(p) (69 - ((p) + 12) * 26 / 43)
    for (i = 0; i < 16; i++) {                          /* the cells */
        const int x = mf_lx(i), in = i < len;
        if (i == cur && in)
            px_rect(x, 76, 6, 11, UI_G3);
        if (in)
            px_box(x, 76, 6, 11, UI_G2);
        else
            mf_dbox(x, 76, 6, 11, UI_G3);
    }
    for (i = 0; i < len; i++) {                         /* what the lane makes of the arp: each step's notes above it */
        const int m = v->lane[0][i], on = i == cur, x = mf_lx(i);
        const qd_rgb c = on ? UI_K3 : col;
        int ps[6], np = 0, k;
        switch (m) {
        case QD_AL_PLAY: ps[np++] = SEQ[ai % 6], ai++; break;
        case QD_AL_REPEAT:
            for (k = 0; k < nl; k++)
                ps[np++] = last[k];
            break;
        case QD_AL_LO: ps[np++] = -12; break;
        case QD_AL_HI: ps[np++] = 31; break;
        case QD_AL_CHORD: ps[np++] = 0, ps[np++] = 4, ps[np++] = 7; break;
        case QD_AL_PWR: ps[np++] = 0, ps[np++] = 19; break;
        case QD_AL_TIE:
            if (nl)
                px_rect(x - 2, MF_LY(last[0]), 8, 2, c);
            break;
        case QD_AL_REST: break;
        default: ps[np++] = SEQ[(m - QD_AL_N1) % 6]; break;
        }
        for (k = 0; k < np; k++) {
            px_hline(x, MF_LY(ps[k]), 6, c);
            px_hline(x, MF_LY(ps[k]) + 1, 6, on ? UI_G1 : PX_ORANGE);
        }
        if (np) {
            nl = np > 3 ? 3 : np;
            for (k = 0; k < nl; k++)
                last[k] = ps[k];
        }
    }
#undef MF_LY
    for (i = 0; i < 16; i++) {                          /* the cells' values (the selected: KNOB 1's) */
        char s[12];
        const int in = i < len;
        qd_mfx_lane_text(0, v->lane[0][i], s);
        if (!strcmp(s, "x"))
            strcpy(s, "\xc3\x97");                      /* (x: a times sign) */
        px_ctext(mf_lx(i) + 3, 79, s, i == sel ? UI_K1 : in ? (i == cur ? UI_K3 : col) : UI_G2, PX_FS, 1);
    }
    px_rbox(mf_lx(sel) - 1, 74, 8, 14, UI_K3, QD_NONE);
    mf_lane_len(len);
}
static int mf_trh(int v)                                /* a transpose's bar: 2 px a semitone, then squeezed */
{
    const int a = v < 0 ? -v : v, h = a <= 8 ? 2 * a : 16 + (a - 8) / 5;
    return v < 0 ? -(h > 21 ? 21 : h) : (h > 24 ? 24 : h);
}
static void mf_trlane(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_ARP];
    const int len = mf_clampi(v->v[QD_MA_TLEN], 1, 16), sel = v->lane_sel & 15;
    const int cur = v->lane_cur >= 0 ? v->lane_cur : (mf_ms(v->t) / 200) % len, z = 64;
    int i;
    for (i = 0; i < 16; i++) {
        const int x = mf_lx(i), in = i < len, val = v->lane[1][i], on = i == cur;
        const qd_rgb c = i == sel ? UI_K1 : on ? UI_K3 : col;
        if (!in) {
            px_dots(x, z, 6, UI_G3, 2);
            continue;
        }
        px_hline(x, z, 6, UI_G2);
        if (val) {
            const int h = mf_trh(val);
            if (h > 0)
                px_rect(x + 1, z - h, 4, h, c);
            else
                px_rect(x + 1, z + 1, 4, -h, c);
        } else {
            px_hline(x + 2, z - 1, 2, c);
        }
        if (on)
            px_hline(x, 87, 6, UI_K3);
    }
    {   /* the selected step: framed, its offset beside it (KNOB 1's) */
        const int sx = mf_lx(sel);
        char s[12];
        int w;
        px_rbox(sx - 1, 38, 8, 50, UI_K3, QD_NONE);
        qd_mfx_lane_text(1, v->lane[1][sel], s);
        w = px_text_w(s, PX_FB, 1);
        px_text(sx > 70 ? sx - 3 - w : sx + 9, 41, s, UI_K1, PX_FB, 1);
    }
    mf_lane_len(len);
}

/* CHANCE: the percentage (KNOB 1's); the notes rolling along the floor, the robot on its ledge over the trapdoor
 * opening it under the ones it drops */
static void mf_chance(int pct, int ms)
{
    const int door = 84, fy = 82, period = 333, speed = 24;
    char s[8];
    int k, j, open = 0;
    strcat(ui_itoa(s, pct), "%");
    px_text(4, 41, s, UI_K1, PX_FB, 2);
    for (k = (ms - 5000) / period - 1; k <= ms / period; k++) {
        const int age = ms - k * period, x = 4 + age * speed / 1000;
        if (age < 0 || x > 113)
            continue;
        if (x >= door - 2 && mf_pct(k * 31 + 7) >= pct) {   /* dropped: through the trapdoor */
            const int d = x - (door - 2), y = fy - 3 + d * d / 5;
            if (y < 88)
                px_rect(door + 2, y, 3, 3, d < 4 ? UI_K3 : UI_G1);
            open |= y < 88;
        } else {
            px_rect(x, fy - 3, 3, 3, UI_K3);
        }
    }
    for (j = 0; j < 14; j++)                            /* the floor, the trapdoor open under a note it drops */
        if (4 + j * 8 != door || !open)
            px_sprite(4 + j * 8, fy, PXS_FLOOR, 0);
    px_sprite(door, 67, PXS_LIFT, 0);                   /* the robot's ledge */
    px_sprite(door - 2, 55, open ? PXS_ROBOT2 : PXS_ROBOT1, 0);
    if (open)
        px_vdots(door + 3, 71, 9, UI_MFX_COL[QD_MFX_CHANCE], 2);
}

/* REPEAT: the robot pacing along the hits' posts (HITS, KNOB 2's), each post as tall as its velocity (FADE, KNOB 3's
 * caps), striking each in turn */
static void mf_repeat(const ui_mfx_t *v)
{
    const int n = mf_clampi(v->v[QD_MR_HITS], 1, 8), fade = v->v[QD_MR_FADE], fy = 83, ms = mf_ms(v->t);
    const float u = (float)(ms % 2200) / 2200.f, pos = u * ((float)n + 0.6f);
    const int cur = (int)pos, span = 88 / n;
    const float f = pos - (float)cur;
    int i, j, rx;
    for (j = 0; j < 14; j++)
        px_sprite(4 + j * 8, fy, PXS_FLOOR, 0);
    px_rect(5, 42, 4, 4, UI_K3);                        /* the note coming in */
    px_vdots(6, 47, fy - 48, UI_G2, 2);
    for (i = 0; i < n; i++) {
        const int a = fade < 0 ? 100 : 100 - fade, b = fade < 0 ? 100 + fade : 100;
        const int vel = n > 1 ? a + (b - a) * i / (n - 1) : b, h = 4 + 26 * vel / 100;
        const int x = 26 + i * span, lit = cur == i && f >= 0.3f && f < 0.6f;
        px_rect(x, fy - h, 2, h, lit ? UI_K3 : UI_K2);
        px_hline(x - 1, fy - h - 1, 4, UI_K3);
    }
    {   /* the robot: to the next post, then the strike */
        const int at = cur < n ? cur : n - 1, xb = 26 + at * span - 12;
        const int xa = cur >= n ? xb : cur == 0 ? 2 : 26 + (cur - 1) * span - 12;
        const int zap = cur < n && f >= 0.3f && f < 0.6f;
        rx = mf_lerpi(xa, xb, cur < n ? mf_ease(f / 0.3f) : 1.f);
        rx = rx < 2 ? 2 : rx;
        px_sprite(rx, fy - 12, zap ? PXS_ROBOT2 : PXS_ROBOT1, 0);
        if (zap)                                        /* (the zap) */
            px_hline(rx + 11, fy - 7, 26 + at * span - rx - 11, UI_K3);
    }
}

/* HARM: the keybed, the voices as platforms out of it (NOTE 2..7 in KNOB 1..3's colours, page by page; the note
 * itself, ORIG, KNOB 4's), their intervals at the right */
static int mf_hy(int p, int lo, int ph) { return 86 - (p - lo) * ph; }
static void mf_harm(const ui_mfx_t *v)
{
    static const uint8_t ID[6] = {QD_MH_N2, QD_MH_N3, QD_MH_N4, QD_MH_N5, QD_MH_N6, QD_MH_N7};
    static const qd_rgb KC[6] = {UI_K1, UI_K2, UI_K3, UI_K1, UI_K2, UI_K3};
    const int ms = mf_ms(v->t);
    const float u = (float)(ms % 2200) / 2200.f;
    int j, nn = 0, ints[7], ik[7], lo = 0, hi = 12, ph, ly[7], lx[7];
    if (v->v[QD_MH_ORIG])
        ints[nn] = 0, ik[nn++] = -1;
    for (j = 0; j < 6; j++)
        if (v->v[ID[j]])
            ints[nn] = v->v[ID[j]], ik[nn++] = j;
    for (j = 0; j < nn; j++)
        lo = ints[j] < lo ? ints[j] : lo, hi = ints[j] > hi ? ints[j] : hi;
    lo -= 1, hi += 1;
    ph = 46 / (hi - lo + 1);
    ph = ph < 1 ? 1 : ph > 3 ? 3 : ph;
    {   /* the keybed: the incoming note's key white, the voices' in their colours */
        qd_rgb lit[100];
        int p;
        for (p = 0; p <= hi - lo && p < 100; p++)
            lit[p] = lo + p == 0 ? UI_G1 : QD_NONE;
        for (j = 0; j < nn; j++)
            if (ints[j] - lo >= 0 && ints[j] - lo < 100)
                lit[ints[j] - lo] = ik[j] >= 0 ? KC[ik[j]] : UI_K4;
        mf_vkeys(4, lo, hi, mf_hy, lo, ph, lit);
    }
    for (j = 0; j < nn; j++) {                          /* the voices, a note running out along each in turn */
        const float w = u * 1.6f - (float)j * 0.12f;
        const int y = mf_hy(ints[j], lo, ph) - (ph > 1 ? 1 : 0);
        const qd_rgb c = ik[j] >= 0 ? KC[ik[j]] : UI_K4;
        px_rect(15, y, 69, ph > 2 ? 2 : 1, c);
        if (w >= 0.f && w < 1.f)
            px_rect(15 + mf_r(66.f * w), y - 1, 3, ph > 2 ? 4 : 3, UI_K3);
        ly[j] = -100, lx[j] = 0;
        if (ik[j] >= 0) {                               /* the interval, kept clear of the others' */
            char s[12];
            int k, x = 88, yy = y - 2;
            s[0] = ints[j] > 0 ? '+' : 0, s[1] = 0;
            ui_itoa(s + (ints[j] > 0), ints[j]);
            for (k = 0; k < j; k++)
                if (lx[k] == x && ly[k] > yy - 6 && ly[k] < yy + 6)
                    x += 15, k = -1;
            if (x > 116)
                continue;
            px_text(x, yy, s, c, PX_FS, 1);
            ly[j] = yy, lx[j] = x;
        }
    }
}

/* TRANSPOSE: three floors and two lifts: the agent rides SEMI (KNOB 1's lift), then OCT (KNOB 2's) to the lit floor */
static void mf_transpose(const ui_mfx_t *v)
{
    const int semi = v->v[QD_MT_SEMI], oct = v->v[QD_MT_OCT], ms = mf_ms(v->t), p = ms % 3000;
    const int b = semi, c = semi + 12 * oct;
    const int lo = b < c ? (b < 0 ? b : 0) : (c < 0 ? c : 0), hi = b > c ? (b > 0 ? b : 0) : (c > 0 ? c : 0);
    const float s = hi > lo ? mf_clamp(20.f / (float)(hi - lo), 0.f, 2.f) : 0.f;
    const int base = 82 - (20 - mf_r((float)(hi - lo) * s)) / 2;
    const int yA = base - mf_r((float)(0 - lo) * s), yB = base - mf_r((float)(b - lo) * s), yC = base - mf_r((float)(c - lo) * s);
    int ax, ay, l1, l2, j, run = 1;
    char t[12];
    for (j = 0; j < 3; j++)
        px_sprite(4 + j * 8, yA, PXS_FLOOR, 0);
    for (j = 0; j < 4; j++)
        px_sprite(40 + j * 8, yB, PXS_FLOOR, 0);
    for (j = 0; j < 4; j++)
        px_sprite(84 + j * 8, yC, p >= 2200 ? PXS_FLOOR_LIT : PXS_FLOOR, 0);
    /* the agent's run: A to the first lift, up (down) it, B to the second, up it, on to C */
    if (p < 600)
        ax = mf_lerpi(8, 33, (float)p / 600.f), ay = yA;
    else if (p < 1100)
        ax = 33, ay = mf_lerpi(yA, yB, mf_ease((float)(p - 600) / 500.f)), run = 0;
    else if (p < 1700)
        ax = mf_lerpi(33, 77, (float)(p - 1100) / 600.f), ay = yB;
    else if (p < 2200)
        ax = 77, ay = mf_lerpi(yB, yC, mf_ease((float)(p - 1700) / 500.f)), run = 0;
    else if (p < 2700)
        ax = mf_lerpi(77, 100, (float)(p - 2200) / 500.f), ay = yC;
    else
        ax = 100, ay = yC, run = 0;
    l1 = p < 600 ? yA : p < 1100 ? ay : p < 2200 ? yB : mf_lerpi(yB, yA, mf_ease((float)(p - 2200) / 500.f));
    l2 = p < 1700 ? yB : p < 2200 ? ay : p < 2700 ? yC : mf_lerpi(yC, yB, mf_ease((float)(p - 2700) / 300.f));
    {   /* the shafts: their rails, the lifts */
        const int t1 = (yA < yB ? yA : yB) - 16, b1 = yA > yB ? yA : yB, t2 = (yB < yC ? yB : yC) - 16, b2 = yB > yC ? yB : yC;
        px_vline(29, t1, b1 - t1 + 4, UI_G2), px_vline(38, t1, b1 - t1 + 4, UI_G2);
        px_vline(73, t2, b2 - t2 + 4, UI_G2), px_vline(82, t2, b2 - t2 + 4, UI_G2);
        px_sprite(30, l1, PXS_LIFT, 0);
        px_sprite(74, l2, PXS_LIFT, 0);
        t[0] = semi > 0 ? '+' : 0, t[1] = 0;            /* SEMI over its lift, OCT over its */
        ui_itoa(t + (semi > 0), semi);
        px_ctext(34, t1 - 7 < 39 ? 39 : t1 - 7, t, UI_K1, PX_FS, 1);
        t[0] = oct > 0 ? '+' : 0, t[1] = 0;
        ui_itoa(t + (oct > 0), oct);
        px_ctext(78, t2 - 7 < 39 ? 39 : t2 - 7, t, UI_K2, PX_FS, 1);
    }
    px_sprite(ax - 4, ay - 15, run ? PXS_AGENT_RUN1 + (ms / 90) % 3 : PXS_AGENT_STAND, 0);
}

/* SCALER: a keyboard of a C to the E an octave up: the scale's keys light (SCALE: KNOB 1's dots, ROOT: KNOB 2's), a
 * note off the scale drops onto its key and slides down onto the scale */
static void mf_scaler(const ui_mfx_t *v, uint32_t mask)
{
    static const int WP[7] = {0, 2, 4, 5, 7, 9, 11};
    const int ms = mf_ms(v->t), root = ((v->v[QD_MZ_ROOT] % 12) + 12) % 12;
    int px[17] = {0}, py[17] = {0}, i, p, k, from = -1, to = -1, n = 0;
    for (i = 0; i < 10; i++) {                          /* the white keys */
        const int x = 5 + i * 11, q = (i / 7) * 12 + WP[i % 7], in = (int)((mask >> (q % 12)) & 1u);
        px_rect(x, 52, 10, 33, in ? PX_LGREY : UI_G2);
        if (in)
            px_rect(x + 3, 79, 4, 3, q % 12 == root ? UI_K2 : UI_K1);
        if (q < 17)
            px[q] = x + 5, py[q] = 77;
    }
    for (i = 0; i < 9; i++) {                           /* the black keys */
        const int q = (i / 7) * 12 + WP[i % 7] + 1, x = 5 + (i + 1) * 11 - 4;
        int in;
        if (WP[i % 7] == 4 || WP[i % 7] == 11)
            continue;
        in = (int)((mask >> (q % 12)) & 1u);
        px_rect(x - 1, 52, 9, 21, UI_BG);
        px_rect(x, 52, 7, 20, in ? PX_GREY : UI_BG);
        if (!in)
            px_box(x, 52, 7, 20, UI_G2);
        else
            px_rect(x + 2, 66, 3, 3, q % 12 == root ? UI_K2 : UI_K1);
        if (q < 17)
            px[q] = x + 3, py[q] = 63;
    }
    for (p = 1; p < 17; p++)                            /* the notes off the scale, in turn: each slides down */
        n += !((mask >> (p % 12)) & 1u);
    if (n) {
        const int pick = (ms / 1500) % n;
        for (k = 0, p = 1; p < 17; p++)
            if (!((mask >> (p % 12)) & 1u) && k++ == pick)
                from = p;
        for (to = from; to > 0 && !((mask >> (to % 12)) & 1u); to--)
            ;
    }
    if (from >= 0) {
        const float u = (float)(ms % 1500) / 1500.f;
        const int Ax = px[from], Ay = py[from], Bx = px[to], By = py[to];
        int x, y;
        qd_rgb c = UI_K3;
        if (u < 0.3f) {
            x = Ax, y = mf_lerpi(40, Ay, mf_ease(u / 0.3f));
        } else if (u < 0.45f) {
            x = Ax, y = Ay;
        } else if (u < 0.8f) {
            const float w = mf_ease((u - 0.45f) / 0.35f);
            x = mf_lerpi(Ax, Bx, w), y = mf_lerpi(Ay, By, w) - mf_r(10.f * 4.f * w * (1.f - w));
        } else {
            x = Bx, y = By, c = UI_K1;
        }
        px_rect(x - 2, y - 2, 5, 5, UI_BG);
        px_rect(x - 1, y - 1, 3, 3, c);
    }
}

/* RANDOM: a die rolling; PITCH (KNOB 1's) a note jumping over its rows, VEL (KNOB 2's) a bar's top landing anywhere in
 * its range, LENGTH (KNOB 3's) a note that only grows */
static void mf_die(int x, int y, int face)
{
    static const uint8_t PIP[7][6] = {{0}, {4}, {0, 8}, {0, 4, 8}, {0, 2, 6, 8}, {0, 2, 4, 6, 8}, {0, 2, 3, 5, 6, 8}};
    int k;
    px_rbox(x, y, 11, 11, UI_K3, UI_K3);
    for (k = 0; k < face; k++)
        px_rect(x + 2 + (PIP[face][k] % 3) * 3, y + 2 + (PIP[face][k] / 3) * 3, 2, 2, UI_BG);
}
static void mf_random(const ui_mfx_t *v)
{
    const int ms = mf_ms(v->t), tick = ms / 5000 * 12 + ms % 5000 * 12 / 5000, fresh = ms % 5000 * 12 / 5 % 1000 < 300;
    const int pr = mf_pct(tick * 13) % 7, ppr = mf_pct((tick - 1) * 13) % 7;
    const int sp = mf_clampi(v->v[QD_MD_PITCH], 0, 12), vw = mf_clampi(v->v[QD_MD_VEL], 0, 127);
    const int lw = mf_clampi(v->v[QD_MD_LEN], 0, 100);
    int r;
    mf_die(55, fresh ? 38 : 39, 1 + mf_pct(tick * 5 + 3) % 6);
    mf_dbox(4, 51, 32, 37, UI_G3);                     /* PITCH: seven rows, the note's own in the middle */
    for (r = 0; r < 7; r++)
        px_dots(8, 54 + r * 5, 22, r == 3 ? UI_G1 : UI_G3, 2);
    {
        const int y = 54 + (3 + (pr - 3) * sp / 12) * 5, yp = 54 + (3 + (ppr - 3) * sp / 12) * 5, rr = 3 * sp / 12;
        px_vline(32, 54 + (3 - rr) * 5, 10 * rr + 1, UI_K1);   /* (its range) */
        if (!fresh)
            px_rect(16, yp - 1, 6, 3, UI_G2);
        px_rect(16, y - 1, 6, 3, UI_K1);
        if (fresh)
            px_box(14, y - 3, 10, 7, UI_K1);
    }
    mf_dbox(42, 51, 34, 37, UI_G3);                    /* VELOCITY: a bar, its top anywhere in its range */
    {
        const int vy = 69 + (mf_pct(tick * 27 + 5) - 50) * 15 * vw / (50 * 127);
        px_dots(46, 69, 26, UI_G1, 2);
        px_rect(56, vy, 6, 87 - vy, UI_K2);
        px_hline(54, vy, 10, UI_K3);
    }
    mf_dbox(82, 51, 34, 37, UI_G3);                    /* LENGTH: a note bar, longer, never shorter */
    {
        const int lx = 100 + mf_pct(tick * 39 + 2) * 14 * lw / 10000;
        px_vline(86, 65, 9, UI_G1);
        px_vline(100, 63, 13, UI_G1);
        px_rect(86, 68, lx - 86 + 1, 3, UI_K3);
        px_vline(lx, 65, 9, UI_K3);
    }
}

/* CHORD: the chord's name (KNOB 1's) over two octaves of keys, its notes struck in turn (on A) */
static void mf_chord(const ui_mfx_t *v, const uint8_t *off, int n)
{
    static const int WP[7] = {0, 2, 4, 5, 7, 9, 11};
    const float u = (float)(mf_ms(v->t) % 1800) / 1800.f;
    qd_rgb fill[24];
    int i, j;
    if (px_text_w(v->chord, PX_FB, 2) <= 112)
        px_ctext(60, 39, v->chord, UI_K1, PX_FB, 2);
    else
        px_ctext(60, 42, v->chord, UI_K1, PX_FB, 1);
    for (i = 0; i < 24; i++)
        fill[i] = QD_NONE;
    for (j = 0; j < n; j++) {
        const int p = 9 + off[j];
        if (p < 24 && u >= (float)j * 0.08f)
            fill[p] = j ? UI_MFX_COL[QD_MFX_CHORD] : UI_K3;
    }
    for (i = 0; i < 14; i++) {
        const int p = (i / 7) * 12 + WP[i % 7];
        px_rect(4 + i * 8, 58, 7, 29, fill[p] != QD_NONE ? fill[p] : PX_GREY);
    }
    for (i = 0; i < 13; i++) {
        const int p = (i / 7) * 12 + WP[i % 7] + 1, x = 4 + (i + 1) * 8 - 3;
        if (WP[i % 7] == 4 || WP[i % 7] == 11)
            continue;
        px_rect(x - 1, 58, 7, 18, UI_BG);
        if (fill[p] != QD_NONE)
            px_rect(x, 58, 5, 17, fill[p]);
    }
}

/* MONO: four lanes into one: each new note cuts the one before it */
static void mf_along(const int *p, int n, float u, int *x, int *y)   /* a point a fraction u along a polyline */
{
    float L = 0.f, seg[8], d;
    int i;
    for (i = 1; i < n && i <= 8; i++) {
        const float dx = (float)(p[2 * i] - p[2 * i - 2]), dy = (float)(p[2 * i + 1] - p[2 * i - 1]);
        seg[i - 1] = sqrtf(dx * dx + dy * dy);
        L += seg[i - 1];
    }
    d = mf_clamp(u, 0.f, 1.f) * L;
    for (i = 0; i < n - 1; i++) {
        if (d <= seg[i] || i == n - 2) {
            const float w = seg[i] > 0.f ? d / seg[i] : 0.f;
            *x = mf_lerpi(p[2 * i], p[2 * i + 2], w);
            *y = mf_lerpi(p[2 * i + 1], p[2 * i + 3], w);
            return;
        }
        d -= seg[i];
    }
    *x = p[0], *y = p[1];
}
static void mf_mono(int ms)
{
    static const int YS[4] = {44, 54, 66, 76};
    const qd_rgb col = UI_MFX_COL[QD_MFX_MONO];
    const int mx = 62, my = 60, per = 750;
    const int n0 = ms / per;
    int i, n;
    for (i = 0; i < 4; i++) {
        px_hline(4, YS[i], 26, UI_G2);
        px_line(30, YS[i], mx, my, UI_G2);
    }
    px_hline(mx, my, 54, UI_G2);
    for (n = n0 - 3; n <= n0; n++) {
        static const int LN[4] = {0, 2, 1, 3};
        const int age = ms - n * per, travel = 550, ln = LN[((n % 4) + 4) % 4];
        if (age < 0)
            continue;
        if (age < travel) {
            const int p[6] = {4, YS[ln], 30, YS[ln], mx, my};
            int x, y;
            mf_along(p, 3, (float)age / (float)travel, &x, &y);
            px_rect(x - 1, y - 1, 3, 3, UI_K3);
            continue;
        }
        {
            const int out = age - travel, x = mx + (out < per ? out : per) * 60 / 1000;
            if (out < per) {
                px_rect(mx + 3, my - 1, x - mx - 3, 3, col);
                px_rect(x - 1, my - 1, 3, 3, UI_K3);
            } else if (out - per < 250) {               /* (cut: gone from the left) */
                const int x0 = mx + 3 + (x - mx - 3) * (out - per) / 250;
                px_rect(x0, my - 1, x - x0, 3, UI_G2);
            }
        }
    }
    px_rect(mx - 2, my - 2, 5, 5, col);
    px_pset(mx, my, UI_BG);
}

/* SELECTOR: the lever (MODE: KNOB 1's) throwing each note to one of the next LENGTH (KNOB 2's) slots */
static void mf_selector(const ui_mfx_t *v)
{
    const qd_rgb col = UI_MFX_COL[QD_MFX_SELECTOR];
    const int ms = mf_ms(v->t), per = 1300, sx = 34, sy = 62, tx = 62, ox = 116;
    const int n = ms / per, ln = v->v[QD_ML_LEN] < 1 ? 1 : v->v[QD_ML_LEN], len = ln > 3 ? 3 : ln;
    const float u = (float)(ms % per) / (float)per;
    int ty[3], ch, prev, i;
    for (i = 0; i < len; i++)
        ty[i] = len == 1 ? sy : 44 + 36 * i / (len - 1);
    ch = v->v[QD_ML_MODE] == QD_SM_UP ? n % len : v->v[QD_ML_MODE] == QD_SM_DOWN ? len - 1 - n % len
                                                : mf_pct(n * 53) % len;
    prev = v->v[QD_ML_MODE] == QD_SM_UP ? (n + len - 1) % len : v->v[QD_ML_MODE] == QD_SM_DOWN ? len - 1 - (n + len - 1) % len
                                                                : mf_pct((n - 1) * 53) % len;
    px_hline(4, sy, sx - 4, UI_G2);
    for (i = 0; i < len; i++) {
        px_line(sx, sy, tx - 2, ty[i], i == ch ? col : UI_G2);
        px_line(tx + MF_TW + 1, ty[i], 98, sy, i == ch ? col : UI_G2);
    }
    px_hline(98, sy, ox - 98, UI_G2);
    for (i = 0; i < len; i++) {                         /* the next slots, the one chosen flashing as the note lands */
        const int k = v->sel + 1 + i;
        mf_tile(tx, ty[i] - MF_TW / 2, MF_TW, k < 8 ? v->type[k] : 0, 0, 0, i == ch && u > 0.45f && u < 0.7f ? 1.f : 0.f);
    }
    if (ln > 3) {                                       /* (more than three: the count) */
        char s[12] = "+";
        ui_itoa(s + 1, ln - 3);
        px_text(tx + MF_TW + 3, 80, s, UI_K2, PX_FS, 1);
    }
    {   /* the lever, swinging from the last choice to this one; the note along the way */
        const float w = mf_ease(u / 0.18f);
        const int ex = sx + 10, ey = sy + mf_r(((float)(ty[prev] - sy) + (float)(ty[ch] - ty[prev]) * w) * 10.f / (float)(tx - sx));
        int x, y;
        px_line(sx, sy, ex, ey, UI_K1);
        px_line(sx, sy + 1, ex, ey + 1, UI_K1);
        px_rect(sx - 2, sy - 2, 5, 5, UI_K1);
        px_pset(sx, sy, UI_BG);
        if (u < 0.25f) {
            const int p[4] = {4, sy, sx, sy};
            mf_along(p, 2, u / 0.25f, &x, &y);
            px_rect(x - 1, y - 1, 3, 3, UI_K3);
        } else if (u < 0.45f) {
            const int p[4] = {sx, sy, tx - 2, ty[ch]};
            mf_along(p, 2, (u - 0.25f) / 0.2f, &x, &y);
            px_rect(x - 1, y - 1, 3, 3, col);
        } else if (u >= 0.65f) {
            const int p[6] = {tx + MF_TW + 1, ty[ch], 98, sy, ox, sy};
            mf_along(p, 3, (u - 0.65f) / 0.35f, &x, &y);
            px_rect(x - 1, y - 1, 3, 3, col);
        }
    }
}

/* NONE: an empty box (PRESETS fills it), the ten effects orbiting it */
static void mf_empty(float t)
{
    const int ms = mf_ms(t), hi = (int)(t * 1.5f) % 10, cx = 60, cy = 62;
    const qd_rgb c = (ms / 400) & 1 ? UI_K3 : UI_G1;
    int i;
    mf_dbox(cx - 13, cy - 13, 27, 27, c);
    px_hline(cx - 4, cy, 9, c);
    px_vline(cx, cy - 4, 9, c);
    for (i = 0; i < 10; i++) {
        const float an = -UI_PI / 2.f + (float)i * UI_TAU / 10.f + t * 0.25f;
        const int gx = cx + mf_r(cosf(an) * 46.f), gy = cy + mf_r(sinf(an) * 18.f);
        mf_glyph_px(i + 1, gx - 3, gy - 3, 1, i == hi ? UI_MFX_COL[i + 1] : UI_G2);
    }
}

/* PRESETS: the types as a rolodex (NONE first) */
static void mf_roll(float pos)
{
    int k;
    for (k = 0; k < QD_MFX_NTYPE; k++) {
        const float d = (float)k - pos, ad = fabsf(d) < 1.f ? fabsf(d) : 1.f;
        const int y = 61 + mf_r(d * 13.f + (d < 0.f ? -1.f : d > 0.f ? 1.f : 0.f) * ad * 5.f);
        const qd_rgb col = k ? UI_MFX_COL[k] : UI_G1;
        if (fabsf(d) > 2.4f)
            continue;
        if (ad < 0.5f) {                                /* the one under the cursor: framed, big */
            px_rbox(10, y - 10, 102, 21, col, QD_NONE);
            mf_glyph_px(k, 15, y - 7, 2, col);
            px_text(36, y - 3, qd_mfx_type_name(k), col, PX_FB, 1);
        } else {
            mf_glyph_px(k, 19, y - 3, 1, fabsf(d) < 1.5f ? col : UI_G2);
            px_text(36, y - 2, qd_mfx_type_name(k), fabsf(d) < 1.5f ? UI_G1 : UI_G2, PX_FS, 1);
        }
    }
    {
        const int at = mf_r(pos * 46.f / (float)(QD_MFX_NTYPE - 1));
        px_vline(116, 35, 54, UI_G3);
        px_rect(115, 35 + at, 3, 8, UI_K3);
    }
}

/* ALGORITHM: the routing map (FIZZ's mfxMap): the six tracks on the left, OFF and the five groups with their eight
 * slots on the right; a wire from each track to its group in its kind's colour (the selected one in ALGORITHM's cyan,
 * doubled, gliding to where ALGORITHM puts it, the place it left dotted while it glides); a track on OFF a stub, its
 * icon grey; a group no track is on grey */
static int mf_gy(float p) { return 21 + mf_r(p * 16.f); }
static void mf_wire(int y0, float p, qd_rgb c, int thick, int dash)
{
    const int ey = mf_gy(p), ex = p < 0.5f ? 60 : 63;
    int i, px0 = 17, py0 = y0;
    for (i = 1; i <= 12; i++) {                         /* (a cubic: level out of the icon, level into the hexagon) */
        const float u = (float)i / 12.f, w = 1.f - u;
        const int x = mf_r(w * w * w * 17.f + 3.f * w * u * 45.f + u * u * u * (float)ex);
        const int y = mf_lerpi(y0, ey, mf_ease(u));
        if (dash)
            mf_dline(px0, py0, x, y, c, 2, 4);
        else {
            px_line(px0, py0, x, y, c);
            if (thick)
                px_line(px0, py0 + 1, x, y + 1, c);
        }
        px0 = x, py0 = y;
    }
}
static void mf_gsel(const ui_mfx_t *v)
{
    const int sel = (v->track - 1) % UI_NTRACK, to = v->tgrp[sel], from = mf_r(v->gsel_from);
    int g, k, i;
    px_ctext(68, 19, "OFF", UI_G1, PX_FS, 1);
    for (g = 1; g <= 5; g++) {
        const int y = mf_gy((float)g);
        int on = 0;
        for (i = 0; i < UI_NTRACK; i++)
            on |= v->tgrp[i] == g;
        mf_hexn(68, y, 5, g, on ? MF_MINT : UI_G2, 0);
        for (k = 0; k < 8; k++) {                       /* the group's slots: filled in their colours (bypassed: half) */
            const int ty = v->gtype[g - 1][k] & 0x7f, x = 77 + k * 5;
            if (!ty)
                px_rect(x + 1, y, 2, 1, UI_G2);
            else if (v->gtype[g - 1][k] & 0x80)
                px_checker(x, y - 2, 4, 5, UI_MFX_COL[ty], 0);
            else
                px_rect(x, y - 2, 4, 5, UI_MFX_COL[ty]);
        }
    }
    for (i = 1; i <= UI_NTRACK; i++) {                  /* (the selected last: on top) */
        const int t = (sel + i) % UI_NTRACK, kd = ui_track_kind(t + 1), s = t == sel;
        const int y = 25 + t * 16;
        const float p = s ? v->gsel_pos : (float)v->tgrp[t];
        const qd_rgb c = s ? MF_MINT : kd >= 0 ? UI_PK_COL[kd] : UI_TRACK_PAL[t & 7];
        if (s) {
            px_rbox(2, y - 7, 15, 15, UI_K3, QD_NONE);
            if (from != to && fabsf((float)to - p) > 0.05f)   /* the place it left, dotted while it glides */
                mf_wire(y, (float)from, UI_G1, 0, 1);
        }
        if (kd >= 0)
            ui_kind_icon(kd, (float)(2 * 9 + 1), (float)(2 * y + 1), 1.f, p > 0.02f ? UI_PK_COL[kd] : UI_G2);
        else {
            const char d[2] = {(char)('1' + t), 0};
            px_rect(6, y - 3, 7, 7, p > 0.02f ? UI_TRACK_PAL[t & 7] : UI_G2);
            px_text(8, y - 2, d, UI_BG, PX_FS, 1);
        }
        if (p > 0.02f)
            mf_wire(y, p, c, s, 0);
        else
            px_hline(17, y, 5, UI_G2);
    }
}

/* a track routed OFF: its notes straight to the voices; ALGORITHM picks a group (its badge, the five dotted) */
static void mf_off(const ui_mfx_t *v)
{
    static const char *const SPK[7] = {"....##", "...#.#", "####.#", "#....#", "####.#", "...#.#", "....##"};
    const qd_rgb tc = UI_TRACK_PAL[(v->track - 1) & 7];
    const int ms = mf_ms(v->t), y = 64;
    const char d[2] = {(char)('0' + v->track % 10), 0};
    int k, gi;
    px_dots(0, MF_RY, 120, UI_G3, 3);
    px_ctext(60, 34, "OFF", UI_G1, PX_FB, 2);
    px_rect(4, y - 4, 7, 9, tc);
    px_text(6, y - 2, d, UI_BG, PX_FS, 1);
    px_hline(13, y, 85, UI_G2);
    for (k = 0; k < 6; k++)
        px_rect(14 + (ms * 30 / 1000 + k * 15) % 80, y - 1, 3, 3, tc);
    mf_bits(SPK, 7, 101, y - 3, 1, UI_K3);
    px_rect(38, 92, 7, 7, UI_KALG);                     /* ALGORITHM's badge, its word */
    px_text(40, 93, "A", UI_BG, PX_FS, 1);
    px_text(48, 93, "ALGORITHM", UI_KALG, PX_FS, 1);
    for (gi = 1; gi <= 5; gi++)
        mf_hexn(24 + (gi - 1) * 20, 107, 5, gi, UI_G2, 1);
}

static void mf_frame(int on)                            /* P1.6e: STEPS' steps held: the step's frame */
{
    if (on) {
        px_box(0, 0, 120, 120, UI_STEP_FRAME);
        px_box(1, 1, 118, 118, UI_STEP_FRAME);
    }
}
static void mf_arrow(int x, int y, qd_rgb c)            /* an arrow 16 long from x */
{
    px_hline(x, y, 13, c);
    mf_tri(x + 15, y, 1, c);
}

void ui_mfx_draw(const ui_mfx_t *v)
{
    const int ty = v->sel >= 0 && v->sel < 8 ? v->type[v->sel] : 0;
    const int npg = ty ? qd_mfx_npages(ty) : 0, bob = sinf(v->t * 4.f) > 0.f ? 1 : 0;
    if (v->gsel) {                                      /* ALGORITHM turning: the routing map */
        mf_head(0, 0, mf_r(v->gsel_pos), v->tr, v->ntr, v->track, 0);
        mf_gsel(v);
        return;
    }
    mf_head(npg, v->page, v->g, v->tr, v->ntr, v->track, v->route_off);
    if (!v->g && !((v->f1 || v->f2) && v->fstep)) {
        mf_off(v);
        mf_frame(v->steps_frame);
        return;
    }
    if (v->g)
        mf_rail(v);
    if ((v->f1 || v->f2) && v->fstep) {                 /* P1.6h, STEPS: F1 / F2 on a step: the step as a card, then
                                                         * where it goes (the step view's F1 / F2) */
        const int full = v->f2 ? v->f2_hand >= 0 : v->st_copied;
        const qd_rgb ac = v->f1 || full ? UI_K2 : UI_K4;
        ui_step_card(40.f, 100.f + (full ? 2.f * (float)bob : 0.f), 64.f, 60.f, full, v->st_type, v->st_nnote, v->st_note);
        if (full)
            mf_arrow(58, 65, ac);
        ui_icon(full ? UI_IC_PASTE : v->f1 ? UI_IC_COPY : UI_IC_CUT, 176.f, 130.f, 2.6f, ac);
        mf_frame(v->steps_frame);
        return;
    }
    if (v->f1 || v->f2) {                               /* P1.6e: F1 / F2 held: what they do (the step view's) */
        const int copied = v->f1 && v->f1_src >= 0, hand = v->f2 ? v->f2_hand : copied ? v->type[v->f1_src] : -1;
        const qd_rgb ac = v->f1 ? UI_K2 : hand >= 0 ? UI_K2 : UI_K4;
        if (hand >= 0) {                                /* the copy / the hand's effect, lifted, then where it goes */
            px_hline(28, 77, 18, UI_G2);
            mf_tile(25, 52 + bob, 22, hand, 1, 0, 0.f);
            mf_arrow(54, 65, ac);
            ui_icon(UI_IC_PASTE, 176.f, 130.f, 2.6f, ac);
        } else if (v->f1) {                             /* nothing picked yet: COPY */
            mf_dbox(25, 54, 22, 22, UI_G2);
            ui_icon(UI_IC_COPY, 72.f, 130.f, 2.2f, UI_G1);
            ui_icon(UI_IC_COPY, 176.f, 130.f, 2.6f, ac);
        } else {                                        /* F2, the hand empty: PICK UP */
            mf_dbox(25, 54, 22, 22, UI_G2);
            ui_icon(UI_IC_CUT, 176.f, 130.f, 2.6f, ac);
        }
        mf_frame(v->steps_frame);
        return;
    }
    if (v->roll) {                                      /* a slot held (2026-11-13; PRESETS before): the types' rolodex,
                                                         * KNOB 1 turns it, KNOB 2 moves the slot (the foot says) */
        qd_save();
        qd_clip_rect(0.f, 64.f, 240.f, 116.f);
        mf_roll(v->roll_pos);
        qd_restore();
        if (v->hold_list)
            mf_foot(v, 1.f);
        return;
    }
    if (ty)
        px_text(4, 33, qd_mfx_type_name(ty), v->byp_sel ? UI_G2 : UI_MFX_COL[ty], PX_FS, 1);
    qd_save();
    qd_clip_rect(0.f, (float)(2 * MF_TOP), 240.f, (float)(2 * (MF_BOT - MF_TOP)));
    switch (ty) {
    case QD_MFX_NONE: mf_empty(v->t); break;
    case QD_MFX_ARP:
        if (v->page == 2)
            mf_modlane(v);
        else if (v->page == 3)
            mf_trlane(v);
        else
            mf_arp(v);
        break;
    case QD_MFX_CHANCE: mf_chance(v->v[QD_MC_CHANCE], mf_ms(v->t)); break;
    case QD_MFX_REPEAT: mf_repeat(v); break;
    case QD_MFX_HARM: mf_harm(v); break;
    case QD_MFX_TRANSPOSE: mf_transpose(v); break;
    case QD_MFX_SCALER: {
        uint32_t mask = 0;
        int p;
        for (p = 0; p < 12; p++)
            mask |= qd_scale_has(v->v[QD_MZ_SCALE], v->v[QD_MZ_ROOT], 60 + p) ? 1u << p : 0u;
        mf_scaler(v, mask);
        break;
    }
    case QD_MFX_RANDOM: mf_random(v); break;
    case QD_MFX_CHORD: {
        const qd_shape_t *sh = &QD_SHAPE[v->v[QD_MK_SHAPE] > 0 && v->v[QD_MK_SHAPE] < QD_NSHAPE ? v->v[QD_MK_SHAPE] : 1];
        uint8_t off[6];
        int n = mf_clampi(v->v[QD_MK_NOTES], 1, 6), j;
        for (j = 0; j < n; j++)
            off[j] = (uint8_t)((j % (sh->n + 1) ? sh->off[j % (sh->n + 1) - 1] : 0) + 12 * (j / (sh->n + 1)));
        mf_chord(v, off, n);
        break;
    }
    case QD_MFX_MONO: mf_mono(mf_ms(v->t)); break;
    case QD_MFX_SELECTOR: mf_selector(v); break;
    default: break;
    }
    if (v->byp_sel)                                     /* bypassed: its picture half-toned, the C64's dimming */
        px_checker(0, MF_TOP, 120, MF_BOT - MF_TOP, UI_BG, 0);
    qd_restore();
    if (v->arm > 0.f) {                                 /* B9 armed: a red X over the picture, shaking */
        const int sh = sinf(v->t * 46.f) > 0.f ? 1 : -1;
        int d;
        for (d = 0; d < 2; d++) {
            px_line(36 + sh + d, 46, 84 + sh + d, 86, UI_RED);
            px_line(84 + sh + d, 46, 36 + sh + d, 86, UI_RED);
        }
    }
    if (ty && v->lift_from < 0)
        mf_foot(v, v->byp_sel);
    mf_frame(v->steps_frame);
}

/* a held step's MIDI FX: m 0 TRACK (the track's group g), 1 OFF, 2.. a group */
void ui_mfx_step_draw(int step, int m, int g, const uint8_t type[8], const char *trig, float t)
{
    char head[32], n[12];
    const int pulse = mf_ms(t) / 500 % 2;
    int i;
    mf_frame(1);
    head[0] = 0;
    strcat(head, "STEP "), strcat(head, ui_itoa(n, step)), strcat(head, " \xc2\xb7 MIDI FX");
    px_text(4, 4, head, UI_K3, PX_FS, 1);
    px_dots(3, 12, 114, UI_G3, 2);
    if (g > 0) {                                        /* the group: its hexagon big (pulsing), its rail */
        if (pulse)
            mf_hexagon(60, 39, 19, UI_G3, 0);
        mf_hexn(60, 39, 16, g, MF_MINT, 0);
        mf_hexagon(60, 39, 15, MF_MINT, 0);
        px_hline(2, 76, 116, UI_G2);
        for (i = 0; i < 8; i++)
            mf_tile(5 + i * 14, 71, MF_TW, type[i], 0, 0, 0.f);
    } else {
        mf_hexagon(60, 39, 16, UI_G1, 1);
        px_ctext(60, 36, "OFF", UI_G1, PX_FB, 1);
    }
    if (m == QD_SMFX_TRACK)                             /* (TRACK: the track's group, said small under it) */
        px_ctext(60, 62, "TRACK", UI_G1, PX_FS, 1);
    px_dots(3, 89, 114, UI_G3, 2);
    px_ctext(18, 92, "TRIG", UI_G1, PX_FS, 1);
    {
        const char *s = trig ? trig : "NOTE";
        const int f = px_text_w(s, PX_FB, 1) > 27 ? PX_FS : PX_FB;
        px_ctext(18, f == PX_FB ? 105 : 107, s, UI_K1, f, 1);
    }
    px_ctext(47, 92, "MIDI FX", UI_G1, PX_FS, 1);
    px_hline(37, 99, 19, UI_K2);
    if (m == QD_SMFX_TRACK)
        px_ctext(47, 107, "TRACK", UI_K2, PX_FS, 1);
    else if (g > 0)
        mf_hexn(46, 109, 6, g, UI_K2, 0);
    else
        px_ctext(47, 105, "OFF", UI_K2, PX_FB, 1);
}
