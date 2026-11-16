/* SPDX-License-Identifier: MPL-2.0 */
#include <stdlib.h>
#include <string.h>

#include "mono.h"

/* the core's fonts (core/ui/px.c): a glyph's row, bit 7 its leftmost pixel */
int px_glyph_row(int font, int ch, int row);

void mono_clear(mono_t *m) { memset(m->px, 0, sizeof m->px); }

void mono_pset(mono_t *m, int x, int y, int on)
{
    if (x >= 0 && x < MONO_W && y >= 0 && y < MONO_H)
        m->px[y][x] = (uint8_t)(on != 0);
}

void mono_rect(mono_t *m, int x, int y, int w, int h, int on)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            mono_pset(m, i, j, on);
}

void mono_box(mono_t *m, int x, int y, int w, int h, int on)
{
    mono_rect(m, x, y, w, 1, on);
    mono_rect(m, x, y + h - 1, w, 1, on);
    mono_rect(m, x, y, 1, h, on);
    mono_rect(m, x + w - 1, y, 1, h, on);
}

void mono_line(mono_t *m, int x0, int y0, int x1, int y1, int on)
{
    const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        mono_pset(m, x0, y0, on);
        if (x0 == x1 && y0 == y1)
            break;
        if (2 * err >= dy) { err += dy; x0 += sx; }
        else { err += dx; y0 += sy; }
    }
}

void mono_dots_h(mono_t *m, int x, int y, int w, int step)
{
    int i;
    for (i = 0; i < w; i += step)
        mono_pset(m, x + i, y, 1);
}

void mono_dots_v(mono_t *m, int x, int y, int h, int step)
{
    int i;
    for (i = 0; i < h; i += step)
        mono_pset(m, x, y + i, 1);
}

void mono_disc(mono_t *m, int cx, int cy, int rx, int ry, int on)
{
    int x, y;
    if (rx < 1 || ry < 1)
        return;
    for (y = -ry; y <= ry; y++)
        for (x = -rx; x <= rx; x++)
            if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
                mono_pset(m, cx + x, cy + y, on);
}

void mono_dither(mono_t *m, int x, int y, int w, int h, int step)
{
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            if (i % step == 0 && j % step == 0 && ((i / step + j / step) & 1) == 0)
                mono_pset(m, i, j, 1);
}

static int cell_w(int font) { return font == MONO_FB ? 8 : 4; }
static int rows(int font) { return font == MONO_FB ? 7 : 5; }

static int upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

int mono_text(mono_t *m, int x, int y, const char *s, int font, int scale, int on)
{
    if (scale < 1)
        scale = 1;
    for (; s && *s; s++) {
        int r, b;
        for (r = 0; r < rows(font); r++) {
            const int bits = px_glyph_row(font, upper((unsigned char)*s), r);
            for (b = 0; b < 8; b++)
                if (bits & (0x80 >> b))
                    mono_rect(m, x + b * scale, y + r * scale, scale, scale, on);
        }
        x += cell_w(font) * scale;
    }
    return x;
}

int mono_text_w(const char *s, int font, int scale)
{
    return (int)strlen(s) * cell_w(font) * (scale < 1 ? 1 : scale);
}

void mono_ctext(mono_t *m, int cx, int y, const char *s, int font, int scale, int on)
{
    mono_text(m, cx - mono_text_w(s, font, scale) / 2, y, s, font, scale, on);
}

void mono_pack(const mono_t *m, uint8_t out[MONO_PACKED])
{
    int page, x, bit;
    for (page = 0; page < 8; page++)
        for (x = 0; x < MONO_W; x++) {
            uint8_t v = 0;
            for (bit = 0; bit < 8; bit++)
                if (m->px[page * 8 + bit][x])
                    v |= (uint8_t)(1u << bit);
            out[page * MONO_W + x] = v;
        }
}
