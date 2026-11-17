/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: a knob turned takes the screen (FIZZ's ui_turn.c, redrawn on the 120 x 120 grid; the models are FIZZ's):
 * its knob and name at the top in its colour, a picture of the value in the middle, the value big at the foot (the
 * 8 x 8 font tripled). The pictures:
 *   CUTOFF the SID filter's response, its corner moving      DECAY the envelope falling as long as the decay
 *   PW     the pulse wave, its duty                          RESO  the filter's peak rising
 *   TEMPO / SWING  the villain drumming his fingers in time  RATE / SCALE / ROOT  a picker (the value, its neighbours)
 *   LEN    the track's loop over the eight pages             LIMIT the voice pool as robots, the limit's lit
 *   PAGE   the eight pages, the one edited boxed             TRACK / TRACKBIG  the six tracks / the one chosen big
 *   FOLLOW the pages and the playhead's arrow                TSWING a pendulum, even or swung
 *   TAP    the villain at his desk, a finger a tap, the BPM big
 * And the knob band (a turn in a hold mode: a strip over the top), the word band, the confirmation word, UNDO / REDO. */
#include <string.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"
#include "../app/undo.h"



/* the value at the foot: as big as it fits (x3, x2, x1, the small font) */
static void foot_value(const char *s, qd_rgb c)
{
    int sc;
    for (sc = 3; sc >= 1; sc--)
        if (px_text_w(s, PX_FB, sc) <= 116) {
            px_ctext(60, sc == 3 ? 92 : sc == 2 ? 97 : 101, s, c, PX_FB, sc);
            return;
        }
    px_ctext(60, 103, s, c, PX_FS, 1);
}

static void frame_box(void) { px_box(4, 16, 112, 72, UI_G3); }

/* ---------------------------------------------------------------- the quick knobs: SID TONE's */
static void d_cutoff(const ui_turn_t *v, qd_rgb c)
{
    const int xc = 8 + (int)(v->v * 104.f);
    int x, prev = -1;
    frame_box();
    for (x = 6; x < 114; x++) {                         /* a 12 dB low-pass, its corner at xc */
        const float f = (float)(x - 6) / (float)(xc - 6 > 1 ? xc - 6 : 1);
        float g = f < 1.f ? 1.f : 1.f / (f * f);
        int y = 80 - (int)(g * 52.f);
        if (y < 18)
            y = 18;
        if (prev >= 0)
            px_line(x - 1, prev, x, y, c);
        prev = y;
    }
    px_vdots(xc, 18, 68, UI_G1, 2);
}

static void d_decay(const ui_turn_t *v, qd_rgb c)
{
    const int end = 14 + (int)(v->v * 96.f);
    int x, prev = -1;
    frame_box();
    px_line(8, 82, 14, 22, c);
    for (x = 14; x < 114; x++) {
        const float t = (float)(x - 14) / (float)(end - 14 > 1 ? end - 14 : 1);
        const int y = 82 - (int)(60.f * fz_expf(-t * 3.f));
        if (prev >= 0)
            px_line(x - 1, prev, x, y, c);
        prev = y;
    }
    if (v->playing)                                     /* the beat's dot running down it */
        px_rect(14 + (int)(v->beat * 96.f) - 1, 80, 3, 3, PX_LRED);
}

static void d_pw(const ui_turn_t *v, qd_rgb c)
{
    const float duty = 0.05f + v->v * 0.9f;
    int k;
    frame_box();
    for (k = 0; k < 3; k++) {                           /* three cycles of the pulse */
        const int x0 = 8 + k * 35, xh = x0 + (int)(duty * 35.f);
        px_line(x0, 74, x0, 28, c);
        px_line(x0, 28, xh, 28, c);
        px_line(xh, 28, xh, 74, c);
        px_line(xh, 74, x0 + 35, 74, c);
    }
}

static void d_reso(const ui_turn_t *v, qd_rgb c)
{
    const int peak = (int)(v->v * 44.f);
    int x, prev = -1;
    frame_box();
    for (x = 6; x < 114; x++) {
        const float d = (float)(x - 70) / 9.f;
        const float bump = (float)peak / (1.f + d * d);
        int y = 64 - (int)bump - (x > 70 ? (x - 70) * 2 / 3 : 0);
        y = y < 18 ? 18 : y > 86 ? 86 : y;
        if (prev >= 0)
            px_line(x - 1, prev, x, y, c);
        prev = y;
    }
}

/* ---------------------------------------------------------------- the villain drumming his fingers */
static void villain_desk(int bpm, float beat, int beats, qd_rgb c)
{
    const int up = beat < 0.25f;                        /* a finger comes down on the beat */
    int k;
    px_sprite_sc(44, 18, beat < 0.5f ? PXS_GENIUS_TALK : PXS_GENIUS, 0, 2, QD_NONE);
    px_rect(20, 52, 80, 3, PX_BROWN);                   /* the desk */
    px_rect(20, 55, 80, 1, PX_ORANGE);
    for (k = 0; k < 4; k++) {                           /* four fingers: the one of this beat down */
        const int down = (beats & 3) == k && up;
        px_rect(48 + k * 6, down ? 50 : 46, 4, down ? 2 : 5, PX_LRED);
    }
    px_rect(78, 44, 10, 8, PX_DGREY);                   /* the button he taps */
    px_rect(80, 46 + (up ? 1 : 0), 6, 4, up ? c : PX_RED);
    (void)bpm;
}

static void metronome(int cx, int cy, int beats, float beat, int swing, qd_rgb c)
{
    const float ph = (beats & 1) ? 1.f - beat : beat;   /* to and fro */
    const float sw = (float)(swing - 50) / 50.f;
    const float a = (ph - 0.5f) * 1.3f + (beats & 1 ? sw * 0.3f : -sw * 0.3f);
    const int tx = cx + (int)(fz_sinf(a) * 34.f), ty = cy - (int)(fz_cosf(a) * 34.f);
    int p[6] = {cx - 16, cy + 6, cx + 16, cy + 6, cx, cy - 40};
    px_poly(p, 3, 1, UI_G2, QD_NONE);
    px_line(cx, cy, tx, ty, c);
    px_rect(tx - 2, ty - 2, 5, 5, c);
}

/* ---------------------------------------------------------------- the pickers and the maps */
static void picker(const ui_turn_t *v, qd_rgb c)
{
    if (v->prev)
        px_ctext(60, 26, v->prev, UI_G2, PX_FB, 1);
    if (v->next)
        px_ctext(60, 80, v->next, UI_G2, PX_FB, 1);
    if (v->val[0]) {
        const int sc = px_text_w(v->val, PX_FB, 2) <= 116 ? 2 : 1;
        px_ctext(60, sc == 2 ? 46 : 50, v->val, c, PX_FB, sc);
    }
}

static void d_scale(const ui_turn_t *v, qd_rgb c)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    int k, wi = 0;
    picker(v, c);
    for (k = 0; k < 12; k++) {                          /* one octave from C: the scale's notes lit (the root white) */
        const int pc = k, in = (v->scale_mask >> ((pc - v->root + 12) % 12)) & 1u;
        if (BLACK[k])
            continue;
        px_rect(32 + wi * 8, 92, 7, 16, pc == v->root % 12 ? UI_K3 : in ? c : UI_G2);
        wi++;
    }
    for (k = 0, wi = 0; k < 12; k++) {
        const int in = (v->scale_mask >> ((k - v->root + 12) % 12)) & 1u;
        if (!BLACK[k]) {
            wi++;
            continue;
        }
        px_rect(32 + wi * 8 - 3, 92, 5, 9, k == v->root % 12 ? UI_K3 : in ? c : UI_BG);
        px_box(32 + wi * 8 - 3, 92, 5, 9, UI_G2);
    }
}

static void pages(const ui_turn_t *v, qd_rgb c, int y0)
{
    int p, s;
    for (p = 0; p < 8; p++) {
        const int y = y0 + p * 8, in = (v->loop >> p) & 1;
        char n[2] = {(char)('1' + p), 0};
        px_text(6, y + 1, n, p == v->play_page ? PX_LRED : p == v->page ? c : UI_G1, PX_FS, 1);
        for (s = 0; s < 16; s++) {
            const int x = 14 + s * 6 + (s / 4) * 2;
            const int on = v->trig[p][s], rng = s >= v->pstart[p] && s <= v->pend[p];
            if (!rng)
                px_pset(x + 2, y + 3, UI_G3);
            else if (on)
                px_rect(x, y + 1, 5, 5, p == v->page ? c : in ? UI_G1 : UI_G2);
            else
                px_box(x, y + 1, 5, 5, in ? UI_G2 : UI_G3);
        }
        if (p == v->page)
            px_box(12, y, 106, 8, c);
    }
}

static void d_trackbig(const ui_turn_t *v)
{
    const int t = v->track < 1 ? 1 : v->track > UI_NTRACK ? UI_NTRACK : v->track;
    const qd_rgb c = UI_TRACK_PAL[(t - 1) & 7];
    char n[12];
    int k;
    ui_itoa(n, t);
    px_text(4, 4, "TRACK", c, PX_FS, 1);
    px_ctext(60, 22, n, c, PX_FB, 4);
    if (ui_track_kind(t) >= 0)
        ui_kind_icon(ui_track_kind(t), 120.f, 132.f, 2.f, c);
    px_ctext(60, 76, v->tname[t - 1] ? v->tname[t - 1] : "", UI_K3, PX_FB, 1);
    for (k = 0; k < UI_NTRACK; k++) {                   /* the six as blocks, the chosen one filled */
        char d[2] = {(char)('1' + k), 0};
        const int x = 18 + k * 15;
        if (k + 1 == t)
            px_rect(x, 100, 11, 11, UI_TRACK_PAL[k & 7]);
        else
            px_box(x, 100, 11, 11, UI_TRACK_PAL[k & 7]);
        px_ctext(x + 6, 103, d, k + 1 == t ? UI_BG : UI_TRACK_PAL[k & 7], PX_FS, 1);
    }
}

static void d_follow(const ui_turn_t *v, qd_rgb c)
{
    int k;
    for (k = 0; k < 4; k++)
        px_box(10 + k * 26, 30, 22, 30, k == 1 ? c : UI_G2);
    if (v->value) {                                     /* on: the arrow along the pages */
        px_hline(10, 70, 96, c);
        px_line(100, 66, 106, 70, c);
        px_line(100, 74, 106, 70, c);
    } else {
        px_line(30, 66, 90, 76, PX_LRED);
        px_line(30, 76, 90, 66, PX_LRED);
    }
}

/* ---------------------------------------------------------------- the frame */
void ui_turn_draw(const ui_turn_t *v)
{
    const qd_rgb col = v->has_col ? v->col : UI_K[v->knob & 3];
    const char *val = v->val;
    char pg[8] = "PAGE 1";
    if (v->kind == UI_TK_TAP) {
        ui_ape_draw(v);
        return;
    }
    if (v->kind == UI_TK_TRACKBIG) {                    /* (its own head) */
        d_trackbig(v);
        return;
    }
    px_rect(3, 3, 5, 5, col);                           /* the knob's colour, its name */
    px_text(11, 3, v->name, col, PX_FS, 1);
    if (v->lock)
        px_rect(13 + px_text_w(v->name, PX_FS, 1), 4, 4, 3, col);
    if (v->ctx)
        px_rtext(116, 3, v->ctx, UI_G1, PX_FS, 1);
    px_dots(0, 11, 120, UI_G3, 2);
    switch (v->kind) {
    case UI_TK_FREQ: d_cutoff(v, col); break;
    case UI_TK_DECAY: d_decay(v, col); break;
    case UI_TK_HARM: d_pw(v, col); break;
    case UI_TK_FDBK: d_reso(v, col); break;
    case UI_TK_TEMPO:
    case UI_TK_SWING: ui_ape_tempo(v, col); val = 0; break;
    case UI_TK_RATE: picker(v, col); val = 0; break;
    case UI_TK_PAGE:
        pages(v, col, 16);
        if (!val[0]) {
            pg[5] = (char)('1' + (v->page < 0 ? 0 : v->page > 7 ? 7 : v->page));
            val = pg;
        }
        break;
    case UI_TK_SCALE:
    case UI_TK_ROOT: d_scale(v, col); val = 0; break;
    case UI_TK_TRACKBIG: d_trackbig(v); val = 0; break;
    case UI_TK_FOLLOW: d_follow(v, col); break;
    case UI_TK_TSWING:
        metronome(60, 64, v->beats, v->beat, v->value ? (v->swing > 50 ? v->swing : 62) : 50, col);
        if (!val[0])
            val = v->value ? "ON" : "OFF";
        break;
    default: break;
    }
    if (val && val[0])
        foot_value(val, col);
    if (v->lock && v->ghost[0])
        px_rtext(116, 113, v->ghost, UI_G2, PX_FS, 1);
}

/* ---------------------------------------------------------------- TAP (SHIFT + B6) and TEMPO / SWING (F3) */
int ui_ape_pose(int n, int bpm) { return (n + (bpm >= 180 ? 1 : 0)) & 3; }
void ui_ape_prewarm(int ext) { (void)ext; }
void ui_ape_forget(void) {}

void ui_ape_draw(const ui_turn_t *v)
{
    char s[12];
    int k;
    const qd_rgb c = v->ext ? UI_G1 : UI_K1;
    px_text(4, 3, "TAP", c, PX_FS, 1);
    if (v->ext)
        px_rtext(116, 3, "EXT CLOCK", UI_G1, PX_FS, 1);
    px_dots(0, 11, 120, UI_G3, 2);
    villain_desk(v->bpm, v->since < 0.2f ? 0.f : 0.6f, v->ntotal, c);
    for (k = 0; k < 8 && k < v->ntap; k++) {            /* the taps: dots, the newest brightest */
        const qd_rgb dc = v->tap_age[k] < 0.3f ? UI_K3 : v->tap_age[k] < 1.5f ? c : UI_G2;
        px_rect(36 + k * 6, 62, 4, 4, dc);
    }
    ui_itoa(s, v->tap_bpm[0] ? v->tap_bpm[0] : v->bpm);
    px_ctext(60, 74, s, c, PX_FB, 3);
    px_ctext(60, 100, "BPM", UI_G1, PX_FS, 1);
}

void ui_ape_tempo(const ui_turn_t *v, qd_rgb col)
{
    char s[16];
    villain_desk(v->bpm, v->beat, v->beats, col);
    ui_itoa(s, v->bpm);
    if (v->kind == UI_TK_SWING) {
        metronome(96, 80, v->beats, v->beat, v->swing, UI_K2);
        ui_itoa(s, v->swing);
        strcat(s, "%");
        px_ctext(48, 74, s, UI_K2, PX_FB, 3);
        px_ctext(48, 100, "SWING", UI_G1, PX_FS, 1);
    } else {
        px_ctext(60, 66, s, col, PX_FB, 3);
        ui_itoa(s, v->swing);
        strcat(s, "% SWING");
        px_ctext(60, 92, "BPM", UI_G1, PX_FS, 1);
        px_ctext(60, 104, s, UI_K2, PX_FS, 1);
    }
}

/* ---------------------------------------------------------------- the knob band: a strip over the top */
void ui_band_draw(const ui_band_t *v)
{
    static const char *const BADGE[7] = {"S", "A", "P", "1", "2", "3", "4"};
    const int k = v->knob < 0 ? 0 : v->knob > 6 ? 6 : v->knob;
    const qd_rgb bc = k == 0 ? UI_KSEL : k >= 3 ? UI_K[k - 3] : k == 1 ? UI_KALG : UI_K3;
    char top[48];
    int x, room;
    px_rect(0, 0, 120, 20, UI_BG);
    px_rect(3, 3, 9, 9, bc);
    px_ctext(8, 5, BADGE[k], UI_BG, PX_FS, 1);
    top[0] = 0;
    if (v->ctx)
        strncat(top, v->ctx, sizeof top - 1);
    if (v->mode) {
        strncat(top, " \xc2\xb7 ", sizeof top - 1 - strlen(top));
        strncat(top, v->mode, sizeof top - 1 - strlen(top));
    }
    px_text(15, 3, top, v->col, PX_FS, 1);
    x = px_text(15, 11, v->name ? v->name : "", UI_K3, PX_FS, 1);
    room = 116 - x - 4;
    if (px_text_w(v->val, PX_FB, 1) <= room && px_text_w(v->val, PX_FB, 1) <= 116 - 15 - px_text_w(top, PX_FS, 1) - 4)
        px_rtext(116, 3, v->val, v->col, PX_FB, 1);
    else
        px_rtext(116, 11, v->val, v->col, PX_FS, 1);
    px_hline(0, 19, 120, v->col);
}

void ui_word_band(const char *s)
{
    px_rect(0, 10, 120, 13, UI_K1);
    px_ctext(60, 14, s, UI_BG, PX_FS, 1);
}

/* ---------------------------------------------------------------- the confirmation's scene, UNDO / REDO */
#include "ui_scene.c"                   /* EvilGenius: the scenes (built here: the source lists stay) */

/* FIZZ 2026-11-13: PROJECT SAVED: a green tick in its box (the box snapping shut over its first 150 ms), the word under */
FZ_COLD void ui_saved_draw(const char *big, float t)
{
    const int r = t < .15f ? 4 + (int)(t * 80.f) : 16;
    int k;
    px_box(60 - r, 46 - r, 2 * r + 1, 2 * r + 1, UI_K2);
    for (k = 0; k < 3; k++) {                           /* the tick, three pixels thick */
        px_line(51, 46 + k, 57, 52 + k, UI_K2);
        px_line(57, 52 + k, 69, 38 + k, UI_K2);
    }
    px_ctext(60, 76, big, UI_K2, PX_FB, 1);
}

/* P1.5c: UNDO / REDO, what it was as a glyph */
FZ_COLD void ui_undo_draw(int redo, int kind, float t)
{
    const qd_rgb c = kind < 0 ? UI_G2 : UI_K3;
    const int cx = 60, cy = 76;
    (void)t;
    px_ctext(60, 22, redo ? "REDO" : "UNDO", c, PX_FB, 3);
    {   /* the arc's head at the left (undo) or the right (redo) */
        const int hx = redo ? 92 : 28;
        int x;
        for (x = 28; x <= 92; x++) {
            const int d = x - 60;
            px_pset(x, 56 - (int)(10.f * (1.f - (float)(d * d) / 1024.f)), c);
        }
        px_line(hx, 56, hx + (redo ? -4 : 4), 51, c);
        px_line(hx, 56, hx + (redo ? -5 : 5), 58, c);
    }
    switch (kind) {
    case QD_UK_PASTE: ui_icon(UI_IC_PASTE, (float)(2 * cx), (float)(2 * cy), 2.f, c); break;
    case QD_UK_CLEAR: ui_icon(UI_IC_CLEAR, (float)(2 * cx), (float)(2 * cy), 2.f, c); break;
    case QD_UK_REC: px_circle(cx, cy, 7, PX_LRED, QD_NONE), px_circle(cx, cy, 3, QD_NONE, PX_LRED); break;
    case QD_UK_RANDOM: ui_icon(UI_IC_DICE, (float)(2 * cx), (float)(2 * cy), 2.f, c); break;
    case QD_UK_KNOB:
        px_circle(cx, cy, 8, c, QD_NONE);
        px_line(cx, cy, cx + 5, cy - 5, c);
        break;
    case QD_UK_SOUNDS: {                                /* a floppy */
        px_box(cx - 10, cy - 10, 20, 20, c);
        px_rect(cx - 7, cy - 8, 14, 4, c);
        px_circle(cx, cy + 3, 3, c, QD_NONE);
        break;
    }
    case QD_UK_SNAP:
        px_box(cx - 12, cy - 8, 24, 16, c);
        px_circle(cx, cy, 4, c, QD_NONE);
        break;
    case QD_UK_DOUBLE:
        px_box(cx - 15, cy - 6, 13, 12, c);
        px_box(cx + 2, cy - 6, 13, 12, c);
        break;
    case QD_UK_EDIT: {
        int k;
        for (k = 0; k < 4; k++) {
            if (k == 1)
                px_rect(cx - 17 + k * 9, cy - 4, 7, 8, c);
            else
                px_box(cx - 17 + k * 9, cy - 4, 7, 8, UI_G2);
        }
        break;
    }
    default:
        px_hline(cx - 7, cy, 15, c);
        break;
    }
}
