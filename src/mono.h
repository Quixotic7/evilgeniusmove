/* SPDX-License-Identifier: MPL-2.0 */
/* The Move's screen: 128 x 64, one bit a pixel. A frame is drawn here a byte a pixel and packed for the panel
 * (8 pages x 128 columns, bit 0 the top pixel of a page: schwung's js_display_pack format). Text uses the
 * EvilGenius core's own pixel fonts (core/ui/px.h): S, 4 x 6 cells, and B, 8 x 8 cells. */
#ifndef EGM_MONO_H
#define EGM_MONO_H

#include <stdint.h>

#define MONO_W 128
#define MONO_H 64
#define MONO_PACKED 1024

enum { MONO_FS, MONO_FB };              /* the fonts, as px.h's PX_FS / PX_FB */

typedef struct { uint8_t px[MONO_H][MONO_W]; } mono_t;

void mono_clear(mono_t *m);
void mono_pset(mono_t *m, int x, int y, int on);
void mono_rect(mono_t *m, int x, int y, int w, int h, int on);      /* filled */
void mono_box(mono_t *m, int x, int y, int w, int h, int on);       /* outlined */
void mono_line(mono_t *m, int x0, int y0, int x1, int y1, int on);
void mono_dots_h(mono_t *m, int x, int y, int w, int step);
void mono_dots_v(mono_t *m, int x, int y, int h, int step);
void mono_disc(mono_t *m, int cx, int cy, int rx, int ry, int on);  /* a filled ellipse */
void mono_dither(mono_t *m, int x, int y, int w, int h, int step);  /* a sparse half tone */
/* text: x the left, y the top of the cell; returns the x after it */
int mono_text(mono_t *m, int x, int y, const char *s, int font, int scale, int on);
int mono_text_w(const char *s, int font, int scale);
void mono_ctext(mono_t *m, int cx, int y, const char *s, int font, int scale, int on);
void mono_pack(const mono_t *m, uint8_t out[MONO_PACKED]);

#endif
