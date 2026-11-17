/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: OVERVIEW (FIZZ's, redrawn at 120 x 120; ui.h ui_ov_t): the six tracks as six floors of the tower, each
 * the page's 16 steps as cells on black (an empty one: its floor), a step's bar as tall as its velocity (a ghost: an outline), the one playing white, the
 * playing column marked red. The selected track's floor framed in its colour; muted floors grey and struck through
 * (a page out of the loop: dashed). The black keys picking the tracks: the icons on lit keys, swept in top to bottom;
 * picking a hold mode: its name on top, and when it changes a band over the head with the ten keys, the mode's lit.
 * A function of its model only (no clock). Built through ui_layer.c (it includes this file). */

#define OV_X0 16                                        /* the first cell's left; a cell 5 wide, 6 a step, 2 more a beat */
#define OV_Y0 16                                        /* the first floor's top .. */
#define OV_RP 17                                        /* .. a floor's pitch .. */
#define OV_H 14                                         /* .. its cells' height */
static int ov_cx(int i) { return OV_X0 + i * 6 + (i / 4) * 2; }

void ui_ov_draw(const ui_ov_t *v)
{
    int t, i, mark = -1;
    const int keys = v->mode < 0;                       /* the black keys pick the tracks */
    int x = px_text(3, 3, v->pattern, UI_OV_HEAD, PX_FS, 1);
    if (!keys)                                          /* .. else the hold mode: its name in its colour */
        px_text(x + 4, 3, UI_FIELD_NAME[v->mode], UI_MODE_COL[v->mode], PX_FS, 1);
    for (i = 0; i < 8; i++) {                           /* the pages, the one shown lit; the selected track's loop: a page
                                                         * in it grey, out of it dark; the page shown out of the loop: an
                                                         * outline */
        const int px0 = 86 + i * 4, in = (v->loop_sel >> i) & 1;
        if (i == v->page) {
            if (in)
                px_rect(px0, 4, 3, 3, UI_OV_HEAD);
            else
                px_box(px0, 4, 3, 3, UI_OV_HEAD);
        } else {
            px_rect(px0 + 1, 5, 1, 1, in ? UI_G1 : UI_G3);
        }
    }
    px_dots(0, 10, 120, UI_G3, 2);
    for (i = 0; i < 16; i += 4)                         /* the beats */
        px_pset(ov_cx(i) + 2, 13, i ? UI_G2 : UI_G1);
    if (v->sel >= 0 && v->sel < UI_NTRACK && v->play[v->sel] >= 0)
        mark = v->play[v->sel];                         /* the marker: the selected track's column .. */
    for (t = 0; t < UI_NTRACK && mark < 0; t++)         /* .. else the first track's on this page */
        mark = v->play[t];
    if (mark >= 0 && mark < 16) {
        const int mx = ov_cx(mark);
        px_hline(mx, 12, 5, UI_RED);
        px_hline(mx + 1, 13, 3, UI_RED);
        px_pset(mx + 2, 14, UI_RED);
    }
    for (t = 0; t < UI_NTRACK; t++) {
        const int y = OV_Y0 + t * OV_RP;
        const int k = ui_track_kind(t + 1), solo = v->mute[t] == 2, pgoff = (v->pg_off >> t) & 1;
        const int mute = v->mute[t] == 1 || pgoff;
        const qd_rgb col = mute ? UI_G2 : k >= 0 ? UI_PK_COL[k] : UI_G1;
        int lit = 0;                                    /* the tracks' keys: each icon on a lit key, swept in top to
                                                         * bottom (60 ms apart): white as it lands, then its colour */
        if (keys) {
            const int d = (int)v->sw_ms - 60 * t;
            lit = d >= 220 ? 2 : d > 0 ? 1 : 0;
            if (lit)
                px_rect(1, y, 13, OV_H, lit == 1 ? PX_WHITE : col);
        }
        if (t == v->sel)                                /* the selected track's floor framed in its colour */
            px_box(OV_X0 - 2, y - 2, 120 - OV_X0 + 1, OV_H + 4, col);
        if (t == v->sel || solo)                        /* selected: its icon boxed white; soloed: yellow */
            px_box(0, y - 1, 15, OV_H + 2, solo ? UI_KSEL : UI_K3);
        if (solo || ((v->mself >> t) & 1))              /* M muted, S soloed, in the icon's place */
            px_ctext(8, y + 3, solo ? "S" : "M", lit ? UI_BG : solo ? UI_KSEL : col, PX_FB, 1);
        else
            ui_kind_icon(k, 15.f, (float)(2 * y + OV_H), 1.f, lit ? UI_BG : col);
        for (i = 0; i < 16; i++) {
            const int cx = ov_cx(i);
            const int vel = v->vel[t][i], lock = (v->lock[t] >> i) & 1, out = (v->out[t] >> i) & 1;
            const qd_rgb sc = out ? UI_G2 : col;        /* outside the page's start..end: grey, half-toned */
            /* (the owner, 2026-11-14: "some tracks are very hard to read the steps on": a cell is black now, not the
             * C64's blue the reds and the light blue sank into: an empty step its floor only, a step a solid bar) */
            if (out)                                    /* outside the page's start..end: a dotted floor */
                px_pset(cx + 2, y + OV_H - 1, UI_G2);
            else
                px_hline(cx, y + OV_H - 1, 5, vel ? sc : UI_G3);
            if (vel) {                                  /* a bar as tall as the velocity (a ghost: an outline) */
                const int h = lock ? OV_H : vel * OV_H / 127 < 3 ? 3 : vel * OV_H / 127;
                const int hot = i == v->play[t] && !mute && !lock && !out;
                if (lock)
                    px_box(cx, y, 5, OV_H, sc);
                else
                    px_rect(cx, y + OV_H - h, 5, h, hot ? PX_WHITE : sc);
            } else if (i == v->play[t] && !mute && !out) {   /* the playhead on an empty step: its floor white */
                px_hline(cx, y + OV_H - 1, 5, PX_WHITE);
            }
            if (t == v->sel && ((v->held >> i) & 1))    /* held / selected: outlined white */
                px_box(cx - 1, y - 1, 7, OV_H + 2, UI_K3);
        }
        if (mute && !pgoff)                             /* not heard: a line through the floor */
            px_hline(OV_X0, y + OV_H / 2, 120 - OV_X0 - 2, UI_G1);
        else if (pgoff)                                 /* the page out of its loop: dashed through */
            for (x = OV_X0; x < 116; x += 6)
                px_hline(x, y + OV_H / 2, 3, UI_G1);
    }
    if (!keys && v->sw_ms < 1200u && ((int)v->sw_ms < 900 || (v->sw_ms / 75u) % 2u == 0u)) {
        /* the hold mode changed (or the keys became the modes): a band over the head ~0.9 s, then it blinks out: the
         * ten black keys, the mode's lit, its name in its colour */
        const qd_rgb mc = UI_MODE_COL[v->mode];
        px_rect(0, 0, 120, 16, UI_BG);
        for (i = 0; i < 10; i++) {
            const int on = i == v->bkey;
            if (on)
                px_rect(3 + i * 6, 2, 4, 11, mc);
            else
                px_box(3 + i * 6, 4, 4, 8, UI_G2);
        }
        {
            const char *n = v->mname ? v->mname : "";
            const int big = px_text_w(n, PX_FB, 1) <= 54;
            px_rtext(117, big ? 4 : 5, n, mc, big ? PX_FB : PX_FS, 1);
        }
        px_hline(0, 15, 120, mc);
    }
}
