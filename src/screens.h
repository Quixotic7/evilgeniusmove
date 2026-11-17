/* SPDX-License-Identifier: MPL-2.0 */
/* The Move's own screens (the canvas's designs): drawn into a mono_t, nothing else touched. */
#ifndef EGM_SCREENS_H
#define EGM_SCREENS_H

#include <stdint.h>
#include "mono.h"

/* the intro and screensaver: the laughing brain in its jar; ms animates the jaw and the lightning */
void scr_intro(mono_t *m, uint32_t ms);
/* RND PAGE / RND SOUND held: the brain laughs while the bar fills (pct 0..100) */
void scr_randomize(mono_t *m, uint32_t ms, int pct);
/* a page's name, the whole screen, one fixed size (the 16-pixel font: 8 characters a line, two lines at most) */
void scr_page_name(mono_t *m, const char *name);
/* a knob touched or turned: its name, its value large (value 0: none known), a bar (frac < 0: no bar; else 0..1000) */
void scr_knob(mono_t *m, const char *name, const char *value, int frac);
/* page mode: the knob pop-up's layout, its bar's place the clip's 16 pages: solid in the loop, outline out of it,
 * dots past the end; a notch over the page shown; the page playing blinks */
void scr_pages(mono_t *m, const char *name, const char *value, int npage, unsigned loop, int shown, int playing, uint32_t ms);
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
/* the room: the page's sixteen steps as platforms (y their top row, w their width), a robot on a step that holds
 * locks, the agent (x < 0: none; pose 0, 1 running, 2 in the air), the tracks' lift at the left */
#define SCR_RM_X(i) (13 + (i) * 7 + (i) / 4)
#define SCR_RM_TOP 12
#define SCR_RM_BOT 48
#define SCR_RM_FLOOR 57
typedef struct {
    struct { uint8_t on, guard, held, y, w; } col[16];
    int agent_x, agent_y, agent_pose;
    int ntrack, sel;                    /* ntrack 0: no lift (the floor the full width) */
    uint8_t mute, flash;                /* bit t: track t not heard, playing a trig now */
    int npage, page_shown, page_playing;   /* the rail under the floor: one dash a page in the loop (npage 0: dots),
                                         * the page shown solid, the page playing blinking (-1 none) */
    uint16_t page_loop;                 /* bit p: page p in the loop */
} scr_room_t;
void scr_room(mono_t *m, const scr_room_t *r, uint32_t ms);
/* an FX floor's picture from its values, in knob order (0..1000, < 0 none) */
enum { SCR_FX_SENDS, SCR_FX_CHORUS, SCR_FX_ECHO, SCR_FX_ROOM, SCR_FX_CRUSH, SCR_FX_ROBOT, SCR_FX_WARP, SCR_FX_GLITCH,
       SCR_FX_TV };
void scr_fx(mono_t *m, int kind, const int *v, uint32_t ms);
/* an LFO: shape 0 sine, 1 triangle, 2 saw, 3 square, 4 random; speed and depth 0..1000 */
void scr_lfo(mono_t *m, int shape, int speed, int depth, uint32_t ms);
/* a voice's word: n 4-bit samples, two a byte, the high nibble first; its start and end 0..1000 (< 0 none) */
void scr_word(mono_t *m, const uint8_t *nib, uint32_t n, int start, int end);
/* Session's CLIPS: four rows of slots (first: the first one's number) under eight tracks; a clip outlined, playing
 * solid with a play mark, queued and stopping blinking; the selected one (col, row; row out of 0..3: none) ringed */
enum { SCR_CL_EMPTY, SCR_CL_CLIP, SCR_CL_PLAYING, SCR_CL_QUEUED, SCR_CL_STOPPING };
void scr_session(mono_t *m, const uint8_t cell[4][8], int n, int first, int sel_col, int sel_row, const char *title,
                 uint32_t ms);
/* a short menu: three rows shown, the chosen one a solid bar */
void scr_menu(mono_t *m, const char *const *items, int n, int sel);

#endif
