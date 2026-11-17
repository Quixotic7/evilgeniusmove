/* SPDX-License-Identifier: MPL-2.0
 * EvilGenius: the pixel kit (px.h). A 120-space pixel is qd_fill_rect's 2 x 2 at (2x, 2y): the same frame pipeline as
 * FIZZ's (strips, the display list, alpha for the fades). Runs of lit pixels in a row are one rectangle. */
#include "px.h"
#include "px_art.h"

const qd_rgb PX_C64[16] = {PX_BLACK, PX_WHITE, PX_RED, PX_CYAN, PX_PURPLE, PX_GREEN, PX_BLUE, PX_YELLOW,
                           PX_ORANGE, PX_BROWN, PX_LRED, PX_DGREY, PX_GREY, PX_LGREEN, PX_LBLUE, PX_LGREY};

#ifndef QD_DEVICE
unsigned px_calls;                      /* the rectangles drawn (the host: tools/eg/tour.c's mark counts a frame's;
                                         * the unit's display list holds 2816 of them, 22528 B at 8 B each: a frame
                                         * over that is drawn live, the whole screen again for every band) */
#endif
static void fill(int x, int y, int w, int h, qd_rgb c)
{
    if (c == QD_NONE || w <= 0 || h <= 0)
        return;
#ifndef QD_DEVICE
    px_calls++;
#endif
    qd_fill_rect((float)(2 * x), (float)(2 * y), (float)(2 * w), (float)(2 * h), c);
}

void px_pset(int x, int y, qd_rgb c) { fill(x, y, 1, 1, c); }
void px_rect(int x, int y, int w, int h, qd_rgb c) { fill(x, y, w, h, c); }
void px_hline(int x, int y, int w, qd_rgb c) { fill(x, y, w, 1, c); }
void px_vline(int x, int y, int h, qd_rgb c) { fill(x, y, 1, h, c); }

void px_box(int x, int y, int w, int h, qd_rgb c)
{
    if (w <= 0 || h <= 0)
        return;
    fill(x, y, w, 1, c);
    if (h > 1)
        fill(x, y + h - 1, w, 1, c);
    fill(x, y + 1, 1, h - 2, c);
    if (w > 1)
        fill(x + w - 1, y + 1, 1, h - 2, c);
}

void px_rbox(int x, int y, int w, int h, qd_rgb line, qd_rgb f)
{
    if (w < 3 || h < 3) {
        fill(x, y, w, h, f != QD_NONE ? f : line);
        return;
    }
    fill(x + 1, y + 1, w - 2, h - 2, f);
    fill(x + 1, y, w - 2, 1, line);
    fill(x + 1, y + h - 1, w - 2, 1, line);
    fill(x, y + 1, 1, h - 2, line);
    fill(x + w - 1, y + 1, 1, h - 2, line);
}

void px_dots(int x, int y, int w, qd_rgb c, int step)
{
    int i;
    for (i = 0; i < w; i += step < 1 ? 2 : step)
        fill(x + i, y, 1, 1, c);
}
void px_vdots(int x, int y, int h, qd_rgb c, int step)
{
    int i;
    for (i = 0; i < h; i += step < 1 ? 2 : step)
        fill(x, y + i, 1, 1, c);
}

void px_line(int x0, int y0, int x1, int y1, qd_rgb c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, guard = 512;
    if (c == QD_NONE)
        return;
    if (y0 == y1) {
        fill(x0 < x1 ? x0 : x1, y0, dx + 1, 1, c);
        return;
    }
    if (x0 == x1) {
        fill(x0, y0 < y1 ? y0 : y1, 1, -dy + 1, c);
        return;
    }
    while (guard--) {
        int e2;
        fill(x0, y0, 1, 1, c);
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

void px_circle(int cx, int cy, int r, qd_rgb line, qd_rgb f)
{
    int x = r, y = 0, d = 1 - r;
    if (r <= 0) {
        fill(cx, cy, 1, 1, f != QD_NONE ? f : line);
        return;
    }
    while (x >= y) {                                     /* the midpoint circle, its spans filled */
        if (f != QD_NONE) {
            fill(cx - x + 1, cy + y, 2 * x - 1, 1, f);
            fill(cx - x + 1, cy - y, 2 * x - 1, 1, f);
            fill(cx - y + 1, cy + x, 2 * y - 1, 1, f);
            fill(cx - y + 1, cy - x, 2 * y - 1, 1, f);
        }
        if (line != QD_NONE) {
            fill(cx + x, cy + y, 1, 1, line);
            fill(cx - x, cy + y, 1, 1, line);
            fill(cx + x, cy - y, 1, 1, line);
            fill(cx - x, cy - y, 1, 1, line);
            fill(cx + y, cy + x, 1, 1, line);
            fill(cx - y, cy + x, 1, 1, line);
            fill(cx + y, cy - x, 1, 1, line);
            fill(cx - y, cy - x, 1, 1, line);
        }
        y++;
        if (d < 0) {
            d += 2 * y + 1;
        } else {
            x--;
            d += 2 * (y - x) + 1;
        }
    }
}

void px_poly(const int *p, int n, int close, qd_rgb line, qd_rgb f)
{
    int i;
    if (n < 2)
        return;
    if (f != QD_NONE && n >= 3) {                        /* even-odd scanlines, through the pixel centres */
        int y, y0 = p[1], y1 = p[1];
        for (i = 1; i < n; i++) {
            y0 = p[2 * i + 1] < y0 ? p[2 * i + 1] : y0;
            y1 = p[2 * i + 1] > y1 ? p[2 * i + 1] : y1;
        }
        for (y = y0; y <= y1; y++) {
            int xs[16], k = 0, a, b;
            for (i = 0; i < n && k < 16; i++) {
                const int j = (i + 1) % n, ya = p[2 * i + 1], yb = p[2 * j + 1];
                if ((ya <= y && yb > y) || (yb <= y && ya > y))
                    xs[k++] = p[2 * i] + (y - ya) * (p[2 * j] - p[2 * i]) / (yb - ya);
            }
            for (a = 1; a < k; a++)                      /* (sorted: a handful) */
                for (b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
                    const int t = xs[b];
                    xs[b] = xs[b - 1];
                    xs[b - 1] = t;
                }
            for (a = 0; a + 1 < k; a += 2)
                fill(xs[a], y, xs[a + 1] - xs[a] + 1, 1, f);
        }
    }
    if (line != QD_NONE) {
        for (i = 0; i + 1 < n; i++)
            px_line(p[2 * i], p[2 * i + 1], p[2 * i + 2], p[2 * i + 3], line);
        if (close)
            px_line(p[2 * n - 2], p[2 * n - 1], p[0], p[1], line);
    }
}

void px_checker(int x, int y, int w, int h, qd_rgb c, int phase)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = (j + phase) & 1; i < w; i += 2)
            fill(x + i, y + j, 1, 1, c);
}

/* ---- text */
static uint32_t utf8(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = *p++;
    if (c >= 0xC0u && c < 0xE0u && (*p & 0xC0u) == 0x80u)
        c = (c & 0x1Fu) << 6 | (*p++ & 0x3Fu);
    else if (c >= 0xE0u && c < 0xF0u && (p[0] & 0xC0u) == 0x80u && (p[1] & 0xC0u) == 0x80u) {
        c = (c & 0x0Fu) << 12 | (uint32_t)(p[0] & 0x3Fu) << 6 | (p[1] & 0x3Fu);
        p += 2;
    }
    *s = (const char *)p;
    return c;
}

static int glyph_index(uint32_t c)                        /* -1: a space; unknown: '?' */
{
    int i;
    if (c >= 'a' && c <= 'z')
        c -= 32u;
    if (c == ' ' || c == 0xA0u)
        return -1;
    if (c == 0x2212u || c == 0x2013u || c == 0x2014u)    /* the minus signs and dashes */
        c = '-';
    if (c == 0x2026u)
        ;
    if (c > 32u && c < 127u)
        return (int)c - 32;
    for (i = 0; i < PX_NXCHAR; i++)
        if (PX_XCHAR[i] == c)
            return 95 + i;
    return '?' - 32;
}

#define CELL_W(f) ((f) == PX_FB ? 8 : 4)
#define ROWS(f) ((f) == PX_FB ? 7 : 5)

static void glyph(int x, int y, int gi, qd_rgb c, int font, int sc)
{
    const uint8_t *rows = font == PX_FB ? PX_FONT_B_ROWS[gi] : PX_FONT_S_ROWS[gi];
    int r;
    for (r = 0; r < ROWS(font); r++) {
        unsigned b = rows[r];
        int col = 0;
        while (b) {                                      /* runs of lit pixels: one rectangle each */
            int n = 0;
            while (!(b & 0x80u)) {
                b = (b << 1) & 0xFFu;
                col++;
            }
            while (b & 0x80u) {
                b = (b << 1) & 0xFFu;
                n++;
            }
            fill(x + col * sc, y + r * sc, n * sc, sc, c);
            col += n;
        }
    }
}

int px_glyph_row(int font, int ch, int row)             /* a glyph's row, bit 7 its leftmost pixel (0: none) */
{
    const int gi = glyph_index((uint32_t)ch);
    if (gi < 0 || row < 0 || row >= (font == PX_FB ? 7 : 5))
        return 0;
    return font == PX_FB ? PX_FONT_B_ROWS[gi][row] : PX_FONT_S_ROWS[gi][row];
}

int px_text(int x, int y, const char *s, qd_rgb c, int font, int sc)
{
    if (sc < 1)
        sc = 1;
    while (s && *s) {
        const int gi = glyph_index(utf8(&s));
        if (gi >= 0 && c != QD_NONE)
            glyph(x, y, gi, c, font, sc);
        x += CELL_W(font) * sc;
    }
    return x;
}

int px_text_w(const char *s, int font, int sc)
{
    int n = 0;
    if (sc < 1)
        sc = 1;
    while (s && *s) {
        (void)utf8(&s);
        n++;
    }
    return n ? n * CELL_W(font) * sc - sc : 0;
}

int px_ctext(int cx, int y, const char *s, qd_rgb c, int font, int sc)
{
    return px_text(cx - (px_text_w(s, font, sc) + 1) / 2, y, s, c, font, sc);
}
int px_rtext(int rx, int y, const char *s, qd_rgb c, int font, int sc)
{
    return px_text(rx - px_text_w(s, font, sc) + 1, y, s, c, font, sc);
}
int px_text_sh(int x, int y, const char *s, qd_rgb c, qd_rgb sh, int font, int sc)
{
    px_text(x + sc, y + sc, s, sh, font, sc);
    return px_text(x, y, s, c, font, sc);
}

/* ---- sprites */
int px_sprite_w(int id) { return id >= 0 && id < PXS_N ? PX_SPR[id].w : 0; }
int px_sprite_h(int id) { return id >= 0 && id < PXS_N ? PX_SPR[id].h : 0; }

static void sprite_sc(int x, int y, int id, int flip, qd_rgb tint, int sc)
{
    const px_sprite_t *s;
    int r, i;
    if (id < 0 || id >= PXS_N)
        return;
    s = &PX_SPR[id];
    if (s->nr) {                                        /* a picture kept as rectangles */
        const uint8_t *q = s->d;
        for (i = 0; i < s->nr; i++, q += 5)
            fill(x + (flip ? s->w - q[0] - q[2] : q[0]) * sc, y + q[1] * sc, q[2] * sc, q[3] * sc,
                 tint != QD_NONE ? tint : PX_C64[q[4] & 15u]);
        return;
    }
    for (r = 0; r < s->h; r++) {
        const uint8_t *row = s->d + r * s->w;
        for (i = 0; i < s->w;) {                         /* runs of one colour */
            const int k = flip ? s->w - 1 - i : i;
            const uint8_t v = row[k];
            int n = 1;
            while (i + n < s->w && row[flip ? s->w - 1 - (i + n) : i + n] == v)
                n++;
            if (v != 255u)
                fill(x + i * sc, y + r * sc, n * sc, sc, tint != QD_NONE ? (v ? tint : QD_NONE) : PX_C64[v & 15u]);
            i += n;
        }
    }
}
void px_sprite(int x, int y, int id, int flip) { sprite_sc(x, y, id, flip, QD_NONE, 1); }
void px_sprite_part(int x, int y, int id, int sx, int sy, int w, int h)
{
    const px_sprite_t *s;
    int r, i;
    if (id < 0 || id >= PXS_N)
        return;
    s = &PX_SPR[id];
    if (sx < 0 || sy < 0 || sx + w > s->w || sy + h > s->h)
        return;
    if (s->nr) {                                        /* a picture kept as rectangles: each cut to the part */
        const uint8_t *q = s->d;
        for (i = 0; i < s->nr; i++, q += 5) {
            const int x0 = q[0] > sx ? q[0] : sx, y0 = q[1] > sy ? q[1] : sy;
            const int x1 = q[0] + q[2] < sx + w ? q[0] + q[2] : sx + w, y1 = q[1] + q[3] < sy + h ? q[1] + q[3] : sy + h;
            if (x1 > x0 && y1 > y0)
                fill(x + x0 - sx, y + y0 - sy, x1 - x0, y1 - y0, PX_C64[q[4] & 15u]);
        }
        return;
    }
    for (r = 0; r < h; r++) {
        const uint8_t *row = s->d + (sy + r) * s->w + sx;
        for (i = 0; i < w;) {                           /* runs of one colour */
            const uint8_t v = row[i];
            int n = 1;
            while (i + n < w && row[i + n] == v)
                n++;
            if (v != 255u)
                fill(x + i, y + r, n, 1, PX_C64[v & 15u]);
            i += n;
        }
    }
}
void px_sprite_tint(int x, int y, int id, int flip, qd_rgb tint) { sprite_sc(x, y, id, flip, tint, 1); }
void px_sprite_sc(int x, int y, int id, int flip, int sc, qd_rgb tint) { sprite_sc(x, y, id, flip, tint, sc < 1 ? 1 : sc); }
