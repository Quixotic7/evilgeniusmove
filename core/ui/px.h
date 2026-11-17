/* SPDX-License-Identifier: MPL-2.0
 * EvilGenius: the pixel kit. Every screen is drawn at 120 x 120 in the C64's sixteen colours and shown at 2x on the
 * FM-1's 240 x 240 panel: a pixel here is a 2 x 2 square there, drawn through qd_gfx (qd_fill_rect on the even grid),
 * so the frame pipeline (strips, the display list, the overlays) stays FIZZ's. Coordinates are 120-space integers;
 * colours are qd_rgb (PX_C64[n], or the UI_* roles of ui.h, which are C64 colours). */
#pragma once
#include <stdint.h>
#include "qd_gfx.h"
#include "px_ids.h"                   /* the sprites: PXS_* (tools/eg/gen_art.py) */

#define PX_W 120
#define PX_H 120

/* the C64's colours (Colodore), by name */
#define PX_BLACK 0x000000u
#define PX_WHITE 0xffffffu
#define PX_RED 0x813338u
#define PX_CYAN 0x75cec8u
#define PX_PURPLE 0x8e3c97u
#define PX_GREEN 0x56ac4du
#define PX_BLUE 0x2e2c9bu
#define PX_YELLOW 0xedf171u
#define PX_ORANGE 0x8e5029u
#define PX_BROWN 0x553800u
#define PX_LRED 0xc46c71u
#define PX_DGREY 0x4a4a4au
#define PX_GREY 0x7b7b7bu
#define PX_LGREEN 0xa9ff9fu
#define PX_LBLUE 0x706debu
#define PX_LGREY 0xb2b2b2u
extern const qd_rgb PX_C64[16];

enum { PX_FS, PX_FB };                  /* the fonts: S 4 x 6 cells (3 x 5 glyphs), B 8 x 8 (7 x 7, 2-px stems) */

void px_pset(int x, int y, qd_rgb c);
void px_rect(int x, int y, int w, int h, qd_rgb c);              /* filled */
void px_box(int x, int y, int w, int h, qd_rgb c);               /* outlined, 1 px */
void px_rbox(int x, int y, int w, int h, qd_rgb line, qd_rgb fill);   /* a box with its corners cut (QD_NONE: none) */
void px_hline(int x, int y, int w, qd_rgb c);
void px_vline(int x, int y, int h, qd_rgb c);
void px_dots(int x, int y, int w, qd_rgb c, int step);           /* a dotted rule */
void px_vdots(int x, int y, int h, qd_rgb c, int step);
void px_line(int x0, int y0, int x1, int y1, qd_rgb c);          /* Bresenham */
void px_circle(int cx, int cy, int r, qd_rgb line, qd_rgb fill);
void px_poly(const int *xy, int n, int close, qd_rgb line, qd_rgb fill);
void px_checker(int x, int y, int w, int h, qd_rgb c, int phase);   /* every other pixel: the C64's half tone */

/* text: x the left, y the TOP of the cell; scale 1..; returns the x after the text. Lower case is drawn upper case;
 * UTF-8's · × → ← … ° ± are glyphs too */
int px_text(int x, int y, const char *s, qd_rgb c, int font, int scale);
int px_glyph_row(int font, int ch, int row);                    /* a glyph's pixels: bit 7 the leftmost */
int px_text_w(const char *s, int font, int scale);              /* the drawn width (no trailing gap) */
int px_ctext(int cx, int y, const char *s, qd_rgb c, int font, int scale);
int px_rtext(int rx, int y, const char *s, qd_rgb c, int font, int scale);
int px_text_sh(int x, int y, const char *s, qd_rgb c, qd_rgb sh, int font, int scale);   /* with a drop shadow */

/* the sprites (art-eg/sprites.txt, px_art.h PXS_*) */
void px_sprite(int x, int y, int id, int flip);
void px_sprite_part(int x, int y, int id, int sx, int sy, int w, int h);   /* a rectangle of it, its corner at x, y */
void px_sprite_tint(int x, int y, int id, int flip, qd_rgb tint);   /* every lit pixel in one colour */
void px_sprite_sc(int x, int y, int id, int flip, int scale, qd_rgb tint);   /* scaled (tint QD_NONE: its colours) */
int px_sprite_w(int id);
int px_sprite_h(int id);
