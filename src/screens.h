/* SPDX-License-Identifier: MPL-2.0 */
/* The Move's own screens (the canvas's designs): drawn into a mono_t, nothing else touched. */
#ifndef EGM_SCREENS_H
#define EGM_SCREENS_H

#include <stdint.h>
#include "mono.h"

/* the intro and screensaver: the laughing brain in its jar; ms animates the jaw and the lightning */
void scr_intro(mono_t *m, uint32_t ms);
/* a page's name, the whole screen, one fixed size (the 16-pixel font: 8 characters a line, two lines at most) */
void scr_page_name(mono_t *m, const char *name);
/* a knob touched or turned: its name, its value large (value 0: none known), a bar (frac < 0: no bar; else 0..1000) */
void scr_knob(mono_t *m, const char *name, const char *value, int frac);
/* a part of the core's own screen (240 x 240 RGB565 big-endian, a 120 x 120 picture at 2x): from logical column
 * left and row top, one logical pixel a Move pixel, lit when it is brighter than a dark blue; centred when narrower */
void scr_core_crop(mono_t *m, const uint16_t *lcd_be, int left, int top);
/* an oscillator's wave, the whole screen; pw 0..1000 (the pulse's width) */
enum { SCR_W_OFF, SCR_W_TRI, SCR_W_SAW, SCR_W_PULSE, SCR_W_NOISE, SCR_W_MIX };
void scr_osc(mono_t *m, int wave, int pw, uint32_t ms);
/* an envelope's shape with a handle a stage; each 0..1000 */
void scr_env(mono_t *m, int a, int d, int s, int r);
/* the filter's response: kind 0 off, 1 low pass, 2 band pass, 3 high pass; cutoff and resonance 0..1000 */
void scr_filter(mono_t *m, int kind, int cutoff, int reso);
/* the mix: n faders (level 0..127, mute 1: not heard, meter 0..1000), the selected one's cap solid */
void scr_mix(mono_t *m, const int *level, const int *mute, const int *meter, int n, int sel);
/* every knob of the page at once, in knob order: names, values, bars (frac 0..1000, < 0 none) */
void scr_eight(mono_t *m, const char *const *lab, const char *const *val, const int *frac, int n);
/* the FX tower's lift between two floors; t 0..1000 through the ride */
void scr_lift(mono_t *m, const char *from, const char *to, int up, int t);
/* a short menu: three rows shown, the chosen one a solid bar */
void scr_menu(mono_t *m, const char *const *items, int n, int sel);

#endif
