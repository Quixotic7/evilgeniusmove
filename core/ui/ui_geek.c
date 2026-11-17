/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the Geek Out pages (FIZZ's P2.11, ui.h ui_geek_t, redrawn on the 120 x 120 grid; the model is FIZZ's) as
 * the C64's own screen: light blue on blue in a light blue border, the head a reverse-video bar (the page's name, the
 * transport's light, the four pages as glyphs, the one shown reversed back). The pages:
 *   METERS   core 0's load, core 1 (off until it runs), the frame rate, RAM, the store as five lift shafts: the
 *            platform at the value, raster lines under it, a peak tick held 2 s, the value on top
 *   NUMBERS  a BASIC listing: the voices of the pool (and its slots as cells, in their tracks' colours), steals, the
 *            audio interrupt's longest half, late halves, the longest lock, the frame time, the BPM; a crash's cause
 *            as ?... ERROR; READY. and the cursor, blinking on the beat
 *   VOICES   a shaft a track: its number, its voice slots, its steals, its share of the audio
 *   SCOPE    ~3 cycles of the loudest track as a hard pixel line on the screen's window
 *   TRACKS   the whole screen: six raster bands, each track's own line in its colour (no sound: flat)
 * Yellow (and a spark of rays) marks trouble. A figure the platform does not give is drawn dim. Built through
 * ui_layer.c (it includes this file). */

#define GK_WARN UI_KSEL                                 /* yellow: trouble, on these pages */
#define GK_BG PX_BLUE                                   /* the C64's screen .. */
#define GK_FG PX_LBLUE                                  /* .. its text and border */
#define GK_DIM PX_DGREY                                 /* a figure not given */
#define GK_INK PX_WHITE                                 /* a value */

static qd_rgb gk_dark(qd_rgb c)                         /* a colour's darker partner (the raster lines) */
{
    switch (c) {
    case PX_LBLUE: return PX_BLUE;
    case PX_LGREEN: return PX_GREEN;
    case PX_WHITE: return PX_LGREY;
    case PX_LRED: return PX_RED;
    case PX_YELLOW: return PX_ORANGE;
    case PX_CYAN: return PX_GREEN;
    case PX_ORANGE: return PX_BROWN;
    case PX_PURPLE: return PX_BLUE;
    default: return PX_DGREY;
    }
}
static qd_rgb gk_heat(float u)                          /* a load: green, yellow over 60 %, light red over 85 % */
{
    return u < 0.6f ? UI_K2 : u < 0.85f ? GK_WARN : UI_K4;
}
static int gk_h(float u, int h)                         /* u (0..1) of h, in whole pixels */
{
    return (int)((u < 0.f ? 0.f : u > 1.f ? 1.f : u) * (float)h + 0.5f);
}

/* a steal: a spark of eight rays round x, y, r long (yellow) */
static void gk_burst(int x, int y, int r)
{
    static const int8_t D[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    int k, j;
    for (k = 0; k < 8; k++)
        for (j = r - 1; j <= r; j++)
            px_pset(x + D[k][0] * j, y + D[k][1] * j, GK_WARN);
}

/* the raster lines: a fill in c, every third row its darker partner */
static void gk_raster(int x, int y, int w, int h, qd_rgb c)
{
    int j;
    if (h <= 0)
        return;
    px_rect(x, y, w, h, c);
    for (j = h - 2; j > 0; j -= 3)
        px_hline(x, y + j, w, gk_dark(c));
}

/* ---------------------------------------------------------------- the screen, the head */
static const char *const GK_PAGE[4] = {"METERS", "NUMBERS", "VOICES", "SCOPE"};
static const char *const GK_ICON[4][7] = {
    {"...#...", "...#...", "#..#...", "#..#..#", "#..#..#", "#..#..#", "#..#..#"},   /* METERS: bars */
    {"##.####", ".......", "##.###.", ".......", "##.####", ".......", "##.##.."},   /* NUMBERS: a listing */
    {"#......", "#......", "....#..", "#...#..", "#.#.#..", "......#", "#.#.#.#"},   /* VOICES: slots lit */
    {".##....", "#..#...", "#..#...", "....#..", "....#.#", ".....#.", "......."},   /* SCOPE: a wave */
};
static void gk_glyph(const char *const *rows, int x, int y, qd_rgb c)
{
    int r, i;
    for (r = 0; r < 7; r++)
        for (i = 0; i < 7; i++)
            if (rows[r][i] == '#') {
                int n = 1;
                while (i + n < 7 && rows[r][i + n] == '#')
                    n++;
                px_rect(x + i, y + r, n, 1, c);
                i += n - 1;
            }
}
static void gk_screen(void)                             /* the border (and the head's bar), the screen */
{
    px_rect(0, 0, 120, 120, GK_FG);
    px_rect(2, 11, 116, 107, GK_BG);
}
static void gk_header(const ui_geek_t *v)
{
    int i, x;
    x = px_text(4, 3, GK_PAGE[v->page & 3], GK_BG, PX_FB, 1);
    if (v->page == 3 && v->scope_track >= 0) {          /* the scope's track, reverse video */
        char n[12];
        px_rect(x + 2, 2, 10, 9, GK_BG);
        px_rect(x + 3, 3, 8, 7, UI_TRACK_PAL[v->scope_track & 7]);
        px_ctext(x + 7, 4, ui_itoa(n, v->scope_track + 1), UI_BG, PX_FS, 1);
    }
    if (v->playing) {
#if QD_GEEK_STEADY_DOT                                  /* (P2.6i: steady: the head not redrawn every frame) */
        px_rect(75, 4, 5, 5, PX_RED);
#else
        if (v->beat < 0.2f)                             /* (the beat: the light big a moment) */
            px_rect(75, 3, 6, 6, PX_RED);
        else
            px_rect(76, 4, 4, 4, PX_RED);
#endif
    } else {
        px_box(76, 4, 4, 4, GK_BG);
    }
    for (i = 0; i < 4; i++) {
        const int gx = 86 + i * 8;
        if (i == v->page) {
            px_rect(gx - 1, 2, 9, 9, GK_BG);
            gk_glyph(GK_ICON[i], gx, 3, GK_FG);
        } else {
            gk_glyph(GK_ICON[i], gx, 3, GK_BG);
        }
    }
}

/* ---------------------------------------------------------------- 1. METERS: the lift shafts */
#define GK_ST 22                                        /* the shafts' top, their foot */
#define GK_SB 100
static void gk_shaft(int cx, float u, float pk, qd_rgb col, qd_rgb pkcol, int on)
{
    static const int SC[3] = {1, 2, 3};
    const int x0 = cx - 6, H = GK_SB - GK_ST;
    int k;
    px_vline(x0, GK_ST - 1, H + 2, PX_GREY);            /* the rails, the dark well */
    px_vline(x0 + 11, GK_ST - 1, H + 2, PX_GREY);
    px_rect(x0 + 1, GK_ST - 1, 10, H + 2, UI_BG);
    for (k = 0; k < 3; k++)                             /* (a quarter, a half, three quarters) */
        px_hline(x0 - 3, GK_SB - SC[k] * H / 4, 2, GK_FG);
    if (!on)
        return;
    {
        const int f = gk_h(u, H - 3), fy = GK_SB - f;
        px_vdots(cx, GK_ST, fy - 2 - GK_ST, PX_DGREY, 2);   /* the cable */
        gk_raster(x0 + 1, fy, 10, f, col);
        px_hline(x0 + 1, fy - 2, 10, PX_LGREY);          /* the platform */
        px_dots(x0 + 1, fy - 1, 10, PX_GREY, 2);
    }
    if (pk >= 0.f)
        px_hline(x0 - 1, GK_SB - gk_h(pk, H - 3) - 2, 14, pkcol);
}
static void gk_pct(char *s, int v)
{
    ui_itoa(s, v);
    while (*s)
        s++;
    s[0] = '%', s[1] = 0;
}
static void gk_val(const char *s, int cx, qd_rgb col) { px_ctext(cx, 14, s, col, PX_FS, 1); }
static void gk_meters(const ui_geek_t *v)
{
    char s[16];
    const qd_rgb lc = gk_heat(v->load);
    const int fv = v->fps_n;
    const qd_rgb fcol = fv >= 50 ? UI_K2 : fv >= 36 ? GK_WARN : UI_K4;
    /* the two cores: core 0 live, core 1 dotted with a cross and OFF until it runs */
    gk_shaft(13, v->load, v->load_pk, lc, gk_heat(v->load_pk), v->load_on);
    if (v->load_on)
        gk_pct(s, v->load_pct), gk_val(s, 13, lc);
    else
        gk_val("--", 13, GK_DIM);
    if (v->core1_on) {
        gk_shaft(34, v->load1, -1.f, gk_heat(v->load1), 0, 1);
        gk_pct(s, v->load1_pct), gk_val(s, 34, gk_heat(v->load1));
    } else {
        px_vdots(28, GK_ST - 1, GK_SB - GK_ST + 2, GK_DIM, 2);
        px_vdots(39, GK_ST - 1, GK_SB - GK_ST + 2, GK_DIM, 2);
        px_line(31, GK_SB - 10, 36, GK_SB - 5, GK_DIM);
        px_line(36, GK_SB - 10, 31, GK_SB - 5, GK_DIM);
        gk_val("OFF", 34, GK_DIM);
    }
    px_vline(7, GK_SB + 2, 2, GK_FG);                   /* (both: CORE) */
    px_hline(7, GK_SB + 3, 33, GK_FG);
    px_vline(39, GK_SB + 2, 2, GK_FG);
    px_ctext(24, 107, "CORE", GK_FG, PX_FS, 1);
    /* the frame rate: full at 60; its tick the slowest frame of the last two seconds */
    gk_shaft(59, v->fps, v->fps_min, fcol, v->fps_min * 60.f < 50.f ? GK_WARN : UI_K2, v->fps_on);
    gk_val(v->fps_on ? ui_itoa(s, fv) : "--", 59, v->fps_on ? fcol : GK_DIM);
    px_ctext(59, 107, "FPS", GK_FG, PX_FS, 1);
    /* RAM and the store: capacities, white (yellow over 90 %) */
    gk_shaft(83, v->ram, v->ram_pk, v->ram > 0.9f ? GK_WARN : GK_INK, v->ram > 0.9f ? GK_WARN : GK_INK, v->ram_on);
    if (v->ram_on)
        gk_pct(s, v->ram_pct), gk_val(s, 83, v->ram > 0.9f ? GK_WARN : GK_INK);
    else
        gk_val("--", 83, GK_DIM);
    px_ctext(83, 107, "RAM", GK_FG, PX_FS, 1);
    gk_shaft(106, v->store, v->store, GK_INK, GK_INK, v->store_on);
    if (v->store_on)
        gk_pct(s, v->store_pct), gk_val(s, 106, GK_INK);
    else
        gk_val("--", 106, GK_DIM);
    px_ctext(106, 107, "STORE", GK_FG, PX_FS, 1);
}

/* ---------------------------------------------------------------- 2. NUMBERS: the listing */
#define GK_RY(r) (13 + (r) * 10)                        /* a line's top */
#define GK_VX 44                                        /* the values' left */
static void gk_line(int r, const char *label, qd_rgb lc)   /* a line: its number, its name */
{
    char n[12];
    ui_itoa(n, 10 * (r + 1));
    px_text(4, GK_RY(r) + 2, n, GK_FG, PX_FS, 1);
    px_text(14, GK_RY(r) + 2, label, lc, PX_FS, 1);
}
static int gk_value(int r, const char *val, const char *unit, qd_rgb col, int on)   /* the x after it */
{
    int x = px_text(GK_VX, GK_RY(r), on ? val : "--", on ? col : GK_DIM, PX_FB, 1);
    if (unit && on)
        x = px_text(x, GK_RY(r) + 2, unit, GK_FG, PX_FS, 1);
    return x;
}
static void gk_numbers(const ui_geek_t *v)
{
    char s[16], s2[16];
    int q, x, r;
    /* the voices: the count of the pool, its slots as cells in their tracks' colours */
    gk_line(0, "VOICES", GK_FG);
    s2[0] = '/';
    ui_itoa(s2 + 1, v->nvoice);
    gk_value(0, ui_itoa(s, v->voices), s2, GK_INK, 1);
    for (q = 0; q < v->nvoice && q < 16; q++) {         /* (8 voices: a row of 8 x 7 cells; 16: two rows of 3 x 3) */
        const int big = v->nvoice <= 8, cx = 84 + (q % 8) * 4, cy = big ? GK_RY(0) : GK_RY(0) + (q / 8) * 4;
        const int h = big ? 7 : 3;
        px_rect(cx, cy, 3, h, UI_BG);
        if (v->slot_lvl[q] > 0.02f) {
            const int f = 1 + gk_h(v->slot_lvl[q], h - 1);
            px_rect(cx, cy + h - f, 3, f > h ? h : f, v->slot_col[q]);
        }
    }
    /* steals: a yellow flash and a spark on each new one */
    gk_line(1, "STEALS", v->steals ? GK_WARN : GK_FG);
    x = gk_value(1, ui_itoa(s, v->steals), 0, v->steals ? (v->steal_flash > 0.4f ? GK_INK : GK_WARN) : GK_FG, 1);
    if (v->steal_flash > 0.02f)                         /* (the spark beside it, growing as it fades) */
        gk_burst(x + 6, GK_RY(1) + 3, 3 + gk_h(1.f - v->steal_flash, 2));
    /* the audio interrupt (its share of a half as a bar), late halves, the locks */
    gk_line(2, "ISR", GK_FG);
    gk_value(2, ui_itoa(s, v->isr_us), "US", GK_INK, v->isr_on);
    if (v->isr_on) {
        px_rect(86, GK_RY(2) + 1, 30, 5, UI_BG);
        px_rect(87, GK_RY(2) + 2, gk_h(v->isr_frac, 28) < 1 ? 1 : gk_h(v->isr_frac, 28), 3, gk_heat(v->isr_frac));
    }
    gk_line(3, "LATE", v->late ? GK_WARN : GK_FG);
    x = gk_value(3, ui_itoa(s, v->late), 0, v->late ? (v->late_flash > 0.4f ? GK_INK : GK_WARN) : GK_FG, v->late_on);
    if (v->late_on && v->late && v->late_flash > 0.02f) {   /* (a new one: a box round it, opening out as it fades) */
        const int g = 2 + gk_h(1.f - v->late_flash, 6);
        px_box(GK_VX - g, GK_RY(3) - 2, x - GK_VX + 2 * g - 1, 11, GK_WARN);
    }
    gk_line(4, "LOCK", GK_FG);
    gk_value(4, ui_itoa(s, v->lock_us), "US", GK_INK, v->lock_on);
    /* the frame time and the tempo */
    {
        const int slow = v->frame_on && v->frame10 > 200;
        gk_line(5, "FRAME", slow ? GK_WARN : GK_FG);
        ui_itoa(s, v->frame10 / 10);
        q = (int)strlen(s);
        s[q] = '.', s[q + 1] = (char)('0' + v->frame10 % 10), s[q + 2] = 0;
        gk_value(5, s, "MS", slow ? GK_WARN : GK_INK, v->frame_on);
    }
    gk_line(6, "BPM", GK_FG);
    gk_value(6, ui_itoa(s, v->bpm), 0, GK_INK, 1);
    r = 7;
    if (v->boot) {                                      /* the last boot's cause: only after a crash */
        px_text(4, GK_RY(r) + 2, "?", GK_WARN, PX_FS, 1);
        x = px_text(8, GK_RY(r) + 2, v->boot, GK_WARN, PX_FS, 1);
        px_text(x + 4, GK_RY(r) + 2, "ERROR", GK_WARN, PX_FS, 1);
        r++;
    }
    px_text(4, GK_RY(r) + 1, "READY.", GK_FG, PX_FB, 1);
#if QD_GEEK_STEADY_DOT                                  /* (the cursor: steady with the steady light, else blinking on
                                                         * the beat) */
    px_rect(4, GK_RY(r + 1) + 1, 7, 7, GK_FG);
#else
    if (!v->playing || v->beat < 0.5f)
        px_rect(4, GK_RY(r + 1) + 1, 7, 7, GK_FG);
#endif
}

/* ---------------------------------------------------------------- 3. VOICES: a shaft a track */
static void gk_voices(const ui_geek_t *v)
{
    char s[12];
    int i, k;
    for (i = 0; i < UI_NTRACK && i < 6; i++) {
        const int cx = 12 + i * 19, top = 63, bot = 114;
        const qd_rgb col = UI_TRACK_PAL[i];
        const float fl = v->tflash[i];
        px_rect(cx - 7, 12, 15, 105, UI_BG);
        if (v->on[i])                                   /* its number: reverse video while it sounds */
            px_rect(cx - 4, 14, 9, 8, col);
        else
            px_box(cx - 4, 14, 9, 8, col);
        px_ctext(cx + 1, 16, ui_itoa(s, i + 1), v->on[i] ? UI_BG : col, PX_FS, 1);
        for (k = 0; k < 3; k++) {                       /* its voice limit as cells, a sounding voice filled to its
                                                         * level */
            const int y = 26 + k * 9;
            if (k >= v->lim[i]) {
                px_pset(cx, y + 3, PX_DGREY);
                continue;
            }
            px_box(cx - 3, y, 7, 7, PX_GREY);
            if (k < v->on[i] && v->lvl[i][k] > 0.02f) {
                const int f = 2 + gk_h(v->lvl[i][k], 3);
                px_rect(cx - 2, y + 6 - f, 5, f, col);
            }
        }
        if (fl > 0.02f)
            gk_burst(cx, 29, 6 + gk_h(1.f - fl, 3));
        if (v->tsteals[i])                              /* its steals so far (a dash: none) */
            px_ctext(cx + 1, 54, ui_itoa(s, v->tsteals[i]), fl > 0.4f ? GK_INK : GK_WARN, PX_FS, 1);
        else
            px_hline(cx - 1, 56, 3, PX_DGREY);
        px_box(cx - 4, top, 9, bot - top + 1, PX_DGREY);   /* its share of the audio (full: half of it) */
        {
            const int f = gk_h(v->share[i] * 2.f, bot - top - 1);
            gk_raster(cx - 3, bot - f, 7, f, col);
        }
    }
}

/* ---------------------------------------------------------------- 4. SCOPE: a hard pixel line */
static void gk_wave(int x0, int n, const int *ys, qd_rgb col)   /* a line through n points, 1 px apart */
{
    int i;
    for (i = 0; i < n; i++) {
        const int y = ys[i], p = i ? ys[i - 1] : y;
        if (y > p + 1)                                  /* (a step: joined by a column) */
            px_vline(x0 + i, p + 1, y - p, col);
        else if (y < p - 1)
            px_vline(x0 + i, y, p - y, col);
        else
            px_pset(x0 + i, y, col);
    }
}
static void gk_scope(const ui_geek_t *v)
{
    const int cy = 64, A = 48;
    int ys[UI_GEEK_SCOPE], i;
    px_rect(3, 13, 114, 103, UI_BG);                    /* the window, its graticule */
    for (i = 1; i < 4; i++)
        px_vdots(3 + i * 114 / 4, 15, 100, PX_DGREY, 4);
    px_dots(4, cy, 112, PX_DGREY, 2);
    if (v->scope_track < 0) {
        px_hline(4, cy, 112, PX_GREY);
        return;
    }
    for (i = 0; i < UI_GEEK_SCOPE; i++) {
        const int y = cy - (int)floorf((float)A * v->w.scope[i] + 0.5f);
        ys[i] = y < 14 ? 14 : y > 114 ? 114 : y;
    }
    gk_wave(4, UI_GEEK_SCOPE, ys, UI_TRACK_PAL[v->scope_track & 7]);
}

/* ---------------------------------------------------------------- 5. TRACKS: the whole screen, six raster bands */
static void gk_tracks(const ui_geek_t *v)
{
    int t, k;
    px_rect(0, 0, 120, 120, GK_BG);
    for (t = 0; t < UI_GEEK_TRK; t++) {
        const int mid = 10 + t * 20;
        const qd_rgb col = UI_TRACK_PAL[t];
        int px0 = 0, py0 = mid;
        px_rect(0, t * 20 + 1, 120, 18, UI_BG);
        if (!v->tsc_live[t]) {                          /* (no sound: one flat line) */
            px_hline(0, mid, 120, col);
            continue;
        }
        for (k = 0; k < UI_GEEK_TPTS; k++) {
            const int x = k * 2 > 119 ? 119 : k * 2, y = mid - v->w.tsc[t][k] * 8 / 127;
            if (k)
                px_line(px0, py0, x, y, col);
            px0 = x, py0 = y;
        }
    }
}

void ui_geek_draw(const ui_geek_t *v)
{
    if (v->page == 4) {
        gk_tracks(v);
        return;
    }
    gk_screen();
    gk_header(v);
    switch (v->page) {
    case 0: gk_meters(v); break;
    case 1: gk_numbers(v); break;
    case 2: gk_voices(v); break;
    default: gk_scope(v); break;
    }
}
