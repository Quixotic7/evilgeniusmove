/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the octave takeover (FIZZ's P1.3c, redrawn at 120 x 120). A change of NOTE's entry octave (OCT- / OCT+
 * with a step held in NOTE), of the keyboard's octave, or of SOUND's bank of ten takes the screen for about a second.
 *   - the octave as a lift: the shaft at the left, a floor for each octave (bank), the car at the one chosen
 *   - the number big at the right, at once (the owner: no roll), the line under it (the note and key, the range)
 *   - under them three octaves of keys, the keys in use lit (the entry octave's twelve, B1's note dotted in KNOB 1's
 *     colour; or the keybed's 27 F .. G), the C's named under; for a bank, the pool as ticks in tens, the bank's ten
 *     bracketed and numbered, sliding to it (the step's sound lock tall in SOUND's colour)
 *   - at the foot the distance from the centre as dots on the side whose LED blinks (OCT- left, OCT+ right) */
#include <math.h>
#include "fz_math.h"

#include "ui.h"
#include "px.h"

#define OC_BLACK(n) ((0x54au >> ((n) % 12)) & 1u)       /* C# D# F# G# A# */

/* three octaves of keys from the C c0, x0 .. x0 + 104; keys lo .. hi lit; mark a note with a dot in mcol */
static void oc_keys(int x0, int y, int c0, int lo, int hi, int mark, qd_rgb mcol)
{
    const int h = 22, bh = 13;
    int n, w = 0;
    for (n = c0; n < c0 + 36; n++) {                    /* the white keys: 4 wide, a gap */
        if (OC_BLACK(n))
            continue;
        px_rect(x0 + w * 5, y, 4, h, n >= lo && n <= hi ? PX_WHITE : PX_DGREY);
        if (n == mark)
            px_rect(x0 + w * 5 + 1, y + h - 4, 2, 2, mcol);
        if (n % 12 == 0) {                              /* a C: its name under */
            char s[8];
            s[0] = 'C';
            ui_itoa(s + 1, n / 12 - 1);
            px_text(x0 + w * 5, y + h + 2, s, n >= lo && n <= hi ? UI_G1 : UI_G2, PX_FS, 1);
        }
        w++;
    }
    for (n = c0, w = 0; n < c0 + 36; n++) {             /* the black keys over the gaps */
        if (!OC_BLACK(n)) {
            w++;
            continue;
        }
        px_rect(x0 + w * 5 - 2, y, 4, bh + 1, PX_BLACK);
        px_rect(x0 + w * 5 - 1, y, 2, bh, n >= lo && n <= hi ? PX_LGREY : PX_BLACK);
        if (n == mark)
            px_rect(x0 + w * 5 - 1, y + bh - 3, 2, 2, mcol);
    }
}

/* the pool as ticks, a bank of ten 20 wide; centred on bank position pos (eased) */
static void oc_pool(float pos, int bank, int nsound, int nbank, int sound, int y)
{
    const int bw = 26, x0 = 60 - (int)floorf((pos + 0.5f) * (float)bw + 0.5f);
    int b, k;
    for (b = 0; b < nbank && b < 13; b++) {
        const int bx = x0 + b * bw + 3;
        char s[8];
        for (k = 0; k < 10; k++) {
            const int snd = b * 10 + k + 1, x = bx + k * 2;
            const int have = snd <= nsound, lock = snd == sound;
            const qd_rgb col = lock ? UI_MODE_COL[UI_F_SOUND] : !have ? UI_G3 : b == bank ? UI_K3 : UI_G2;
            if (x < 2 || x > 117)
                continue;
            px_rect(x, lock ? y - 5 : have ? y : y + 8, 1, lock ? 21 : have ? 16 : 8, col);
        }
        if (bx + 9 >= 4 && bx + 9 <= 115)
            px_ctext(bx + 9, y + 20, ui_itoa(s, b + 1), b == bank ? UI_K3 : UI_G2, PX_FS, 1);
        if (b == bank) {                                /* the bank's bracket */
            px_hline(bx - 2, y - 8, 23, UI_K3);
            px_vline(bx - 2, y - 8, 4, UI_K3);
            px_vline(bx + 20, y - 8, 4, UI_K3);
        }
    }
}

/* the lift: a floor for each value lo .. hi, the car at v */
static void oc_lift(int lo, int hi, int v)
{
    const int x = 6, y0 = 16, y1 = 60, n = hi - lo;
    int i;
    px_vdots(x, y0 - 2, y1 - y0 + 6, UI_G2, 2);
    px_vdots(x + 15, y0 - 2, y1 - y0 + 6, UI_G2, 2);
    for (i = lo; i <= hi; i++) {
        const int fy = n > 0 ? y1 - (i - lo) * (y1 - y0) / n : (y0 + y1) / 2;
        px_hline(x + 2, fy, 2, i == v ? UI_K3 : UI_G3);
        px_hline(x + 12, fy, 2, i == v ? UI_K3 : UI_G3);
        if (i == v) {
            px_rect(x + 3, fy - 2, 10, 3, PX_YELLOW);   /* the car */
            px_hline(x + 3, fy + 1, 10, PX_ORANGE);
        }
    }
}

void ui_octave_draw(const ui_octave_t *v)
{
    static const char *const TITLE[3] = {"ENTRY OCTAVE", "KEYBOARD OCTAVE", "SOUND BANK"};
    const int kind = v->kind < 0 ? 0 : v->kind > 2 ? 2 : v->kind;
    const float k = v->k < 0.f ? 0.f : v->k > 1.f ? 1.f : v->k;
    char s[16];
    int off, i;

    px_text(4, 3, TITLE[kind], UI_K3, PX_FS, 1);
    px_dots(0, 11, 120, UI_G3, 2);

    /* the number, at once; the line under it */
    if (kind == UI_OCT_KEYS && v->value) {
        s[0] = v->value > 0 ? '+' : '-';
        ui_itoa(s + 1, v->value < 0 ? -v->value : v->value);
    } else {
        ui_itoa(s, kind == UI_OCT_BANK ? v->value + 1 : v->value);
    }
    px_text_sh(72 - (px_text_w(s, PX_FB, 4) + 1) / 2, 17, s, UI_K3, UI_G3, PX_FB, 4);
    px_ctext(68, 54, v->sub, UI_G1, PX_FS, 1);

    /* the lift; the keys or the pool */
    if (kind == UI_OCT_NOTE) {                          /* octave o's C is MIDI 12 (o + 1) */
        const int c = 12 * (v->value + 1);
        oc_lift(-1, 8, v->value);
        oc_keys(8, 68, c - 12 < 0 ? 0 : c - 12, c, c + 11, v->lo_note, UI_K1);
    } else if (kind == UI_OCT_KEYS) {
        oc_lift(-2, 2, v->value);
        oc_keys(8, 68, v->lo_note - v->lo_note % 12, v->lo_note, v->lo_note + 26, -1, UI_K3);
    } else {
        const float pos = (float)v->from + (float)(v->value - v->from) * k;
        oc_lift(0, v->nbank > 1 ? v->nbank - 1 : 0, v->value);
        oc_pool(pos, v->value, v->nsound, v->nbank, v->sound, 74);
    }

    /* the distance from the centre: the dots on the side whose LED blinks (OCT- left, OCT+ right) */
    off = kind == UI_OCT_NOTE ? v->value - 4 : v->value;
    for (i = 0; i < 3; i++) {
        const int lit_l = off < 0 && i < -off, lit_r = off > 0 && i < off;
        px_rect(42 - i * 6, 110, 3, 3, lit_l ? UI_K3 : UI_G3);
        px_rect(76 + i * 6, 110, 3, 3, lit_r ? UI_K3 : UI_G3);
    }
    px_ctext(60, 109, "OCT", UI_G2, PX_FS, 1);
    (void)v->t;
}
