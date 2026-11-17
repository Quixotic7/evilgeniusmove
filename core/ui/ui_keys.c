/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: KEYS, the keyboard page (FIZZ's, redrawn at 120 x 120): the keybed plays the selected track.
 * The head: the track's box, kind and name; at the right the pool's sixteen voices as dots (white this track's, grey
 * another's). The sound's pool number and name on a nameplate in the middle; the FM-1's own keybed under it, its 27
 * keys from F (F3 .. G5 at OCT 0, QD_NOTE0) so each drawn key is the one pressed (an octave lower when the notes
 * sounding are all below it), the keys sounding white, a selected step's notes in the held step's purple; the octave
 * (and the track's scale) at the foot.
 * rec 1, STEP recording: the cursor's page as sixteen squares over the nameplate (the cursor's blinking red), the red
 * dot and the cursor's step at the foot. B11 held or steps selected: the page edited's row there, the selected outlined.
 * rec 2, LIVE recording: the REC badge blinking (Q by it with quantize on), the notes scrolling left from the red line
 * (a step 5 px, the beats dotted), the keybed small at the foot with the keys held white. */
#include <math.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"

static const char *const OCT[9] = {"OCT -4", "OCT -3", "OCT -2", "OCT -1", "OCT 0", "OCT +1", "OCT +2", "OCT +3",
                                    "OCT +4"};
static const char *oct_name(int o) { return OCT[o < -4 ? 0 : o > 4 ? 8 : o + 4]; }

#define cat ui_cat                               /* (the kit's) */

#define KB_BLACK(n) ((0x54au >> ((n) % 12)) & 1u)       /* C# D# F# G# A# */
static qd_rgb fill[128];                               /* (the keybed's, both drawings': QD_NONE not lit) */

/* the keybed: 16 white keys from the F lo, 7 px a key, x0 .. x0 + 111 */
static void kb(int x0, int y, int h, int lo)
{
    const int bh = h * 3 / 5;
    int n, w;
    for (n = lo, w = 0; w < 16; n++) {
        if (KB_BLACK(n))
            continue;
        px_rect(x0 + w * 7, y, 6, h, n < 128 && fill[n] != QD_NONE ? fill[n] : PX_GREY);
        px_hline(x0 + w * 7, y + h - 1, 6, PX_DGREY);   /* the key's lip */
        w++;
    }
    for (n = lo, w = 0; w < 16; n++) {
        if (!KB_BLACK(n)) {
            w++;
            continue;
        }
        px_rect(x0 + w * 7 - 3, y, 5, bh + 1, PX_BLACK);
        if (n < 128 && fill[n] != QD_NONE)
            px_rect(x0 + w * 7 - 2, y, 3, bh, fill[n] == UI_K3 ? PX_LGREY : fill[n]);
    }
}

/* the pool's first sixteen voices, two rows of eight at the head's right */
static void voices(const ui_keys_t *v, int x)
{
    int i;
    for (i = 0; i < v->nvoice && i < UI_NVOICE_DOTS; i++)
        px_rect(x + (i % 8) * 3, 2 + (i / 8) * 4, 2, 2, v->voice[i] == 2 ? UI_K3 : v->voice[i] ? UI_G1 : UI_G3);
}

/* the foot: the octave, the track's scale */
static void foot(const ui_keys_t *v, int y)
{
    char s[64];
    s[0] = 0;
    cat(s, oct_name(v->octave));
    if (v->scale && v->scale_name && v->scale_name[0])
        cat(cat(s, " \xc2\xb7 "), v->scale_name);
    px_ctext(60, y, s, UI_G1, PX_FS, 1);
}

/* rec 2: LIVE recording */
static void live(const ui_keys_t *v)
{
    const int now = 94, top = 16, bot = 74;
    int i, lo = 53 + 12 * v->octave, mn = 128, rl;      /* the FM-1's keybed: F3 .. G5 at OCT 0 (QD_NOTE0) */
    float st;

    ui_head(v->name, 0, v->track, UI_K3, UI_G1);
    px_rect(92, 1, 25, 9, v->blink ? UI_RED : PX_BROWN);   /* the badge */
    px_ctext(105, 3, "REC", PX_WHITE, PX_FS, 1);
    if (v->quantize)
        px_text(85, 3, "Q", UI_G1, PX_FS, 1);
    px_dots(0, 11, 120, UI_G3, 2);

    for (i = 0; i < 128; i++)
        fill[i] = v->sounding[i] ? UI_K3 : QD_NONE;
    for (i = 0; i < v->nroll && i < 32; i++) {
        if (v->roll[i].note < mn)
            mn = v->roll[i].note;
        if (v->roll[i].live)
            fill[v->roll[i].note & 127] = UI_K3;
    }
    if (mn < lo && lo >= 12)
        lo -= 12;
    rl = lo - 2;

    for (st = ceilf(v->pos - 18.f); st <= v->pos; st += 1.f) {   /* the beats, scrolling with the playhead */
        const int x = now - (int)floorf((v->pos - st) * 5.f + 0.5f);
        if ((int)st % 4 == 0 && x >= 2)
            px_vdots(x, top, bot - top, UI_G3, 2);
    }
    for (i = 0; i < v->nroll && i < 32; i++) {          /* the notes played, scrolling left from the red line */
        const int x0 = now - (int)floorf(v->roll[i].start * 5.f + 0.5f);
        const int x1 = now - (int)floorf(v->roll[i].end * 5.f + 0.5f);
        const int xa = x0 > 2 ? x0 : 2, w = x1 - xa, y = bot - 2 - (v->roll[i].note - rl) * 2;
        if (x1 < 2 || y < top || y > bot)
            continue;
        px_rect(xa, y, w > 2 ? w : 2, 2, v->roll[i].live ? UI_K3 : UI_G1);
    }
    px_vline(now, top - 2, bot - top + 4, UI_RED);
    kb(4, 80, 22, lo);
    foot(v, 108);
}

void ui_keys_draw(const ui_keys_t *v)
{
    char num[24];
    int i, lo = 53 + 12 * v->octave, below = 0, above = 0;   /* (QD_NOTE0, as live()) */

    if (v->rec == 2) {
        live(v);
        return;
    }

    ui_head(v->name, 0, v->track, UI_K3, UI_G1);
    voices(v, 94);
    px_dots(0, 11, 120, UI_G3, 2);

    if (v->rec == 1 || v->row) {                        /* STEP recording: the cursor's page (B11 held or a selection:
                                                         * the page edited's, the selected outlined) */
        const int c = v->rec == 1 && v->blink ? (v->cursor - 1) % 16 : -1;
        uint16_t trig = 0;
        for (i = 0; i < 16; i++)
            trig |= (uint16_t)((v->page_trig[i] != 0) << i);
        ui_page_row(30.f, trig, v->sel, c, -1);
    }

    /* the sound: a nameplate, its pool number small over its name */
    num[0] = (char)('0' + (v->preset / 100) % 10);
    num[1] = (char)('0' + (v->preset / 10) % 10);
    num[2] = (char)('0' + v->preset % 10);
    num[3] = 0;
    {
        const int w = px_text_w(v->sound, PX_FB, 1), x = 60 - (w + 1) / 2;
        px_rbox(x - 5, 26, w + 10, 22, UI_G2, QD_NONE);
        px_rect(x - 5 + 3, 25, px_text_w(num, PX_FS, 1) + 4, 3, PX_BLACK);   /* the number breaks the frame */
        px_text(x - 5 + 5, 24, num, UI_G1, PX_FS, 1);
        px_text_sh(x, 33, v->sound, UI_K3, UI_G3, PX_FB, 1);
        if (v->edited)
            px_rect(x + w + 2, 32, 2, 2, UI_K4);
    }

    /* the keybed */
    for (i = 0; i < 128; i++) {                         /* (a selected step's notes in the held step's purple) */
        fill[i] = v->sounding[i] == 1 ? UI_K3 : v->sounding[i] ? UI_STEP_FRAME : QD_NONE;
        below |= v->sounding[i] && i < lo;
        above |= v->sounding[i] && i >= lo;
    }
    if (below && !above && lo >= 12)
        lo -= 12;
    if (v->ksteps && v->row) {                          /* FIZZ beta.18: the W keys the steps (the row up top): the ten
                                                         * black keys that play, an orange grid */
        for (i = 0; i < 10; i++)
            ui_rec_key(16.f + (float)i * 21.f, 118.f, 18.f, 72.f, v->bst[i]);
    } else {
        kb(4, 56, 40, lo);
    }
    foot(v, 102);

    if (v->rec == 1 || v->row) {                        /* STEP recording: the cursor's page (B11 held or a selection:
                                                         * the page edited's, the selected outlined, the playhead) */
        const int c = v->rec == 1 && v->blink ? (v->cursor - 1) % 16 : -1;
        uint16_t trig = 0;
        for (i = 0; i < 16; i++)
            trig |= (uint16_t)((v->page_trig[i] != 0) << i);
        ui_page_row(28.f, trig, v->sel, c, v->rec != 1 ? v->play : -1);
    }
    if (v->rec == 1) {
        char step[12];
        px_circle(6, 112, 2, UI_RED, UI_RED);           /* step recording: the red dot, the step number */
        px_rtext(116, 110, ui_itoa(step, v->cursor), UI_K3, PX_FS, 1);
    }
    if (v->framed)                                      /* 2026-11-13 (owner): a step selected: the held step's frame,
                                                         * as on the Home views and in the sound editor */
        ui_step_frame();
}
