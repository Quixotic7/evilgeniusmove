/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the typewriter (FIZZ's P1.5u, redrawn at 120 x 120; ui.h ui_type_t): SAVE PRESET's name and a
 * project's, typed on the keybed. Built through ui_layer.c after ui_presets.c.
 *   the name: twelve cells in the BIG font, the cursor a blinking block (the C64's), a selection (the suggested name,
 *   typed over by the first key) on brown; full: the row flashes orange; F1: the character deleted drops away
 *   the keybed: a character on every key (SHIFT: the signs), the key pressed lit; the letter typed flies up from it
 *   the ideas (PRESETS / KNOB 4): the names seen as a listing, the newest at the foot; a new one rolled: a die tumbles
 *   in and lands; NEW / REPLACE (F2 chooses): the one SAVE does filled */

#define TY_X0 12                                        /* the name's twelve cells, 8 a cell */
#define TY_Y 28
#define TY_KY 72                                        /* the keybed's top */
#define TY_WH 40
#define TY_BH 24

static const char TYD_BLACK[] = "ABCDEFGHIJK", TYD_WHITE[] = "LMNOPQRSTUVWXYZ ";
static const char TYD_SBLACK[] = "0123456789-", TYD_SWHITE[] = ".,'&!?#+/()*:_~ ";
static const int TY_BX[11] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15};   /* a black key sits after this many white keys */

static int ty_white_x(int i) { return 4 + i * 7; }
static int ty_black_x(int j) { return 4 + TY_BX[j] * 7 - 3; }

/* a key's character (FS, centred at cx): SPACE as a little open bracket */
static void ty_char(char c, int cx, int y, qd_rgb col)
{
    if (c == ' ') {
        px_vline(cx - 2, y + 2, 3, col);
        px_hline(cx - 2, y + 4, 5, col);
        px_vline(cx + 2, y + 2, 3, col);
        return;
    }
    {
        const char s[2] = {c, 0};
        px_ctext(cx, y, s, col, PX_FS, 1);
    }
}

/* the keybed with a character on every key; hit: the key lit (0..10 black, 11..26 white; -1 none) */
static void ty_keybed(int hit, int shift)
{
    const char *B = shift ? TYD_SBLACK : TYD_BLACK, *W = shift ? TYD_SWHITE : TYD_WHITE;
    int i, j;
    for (i = 0; i < 16; i++) {
        const int x = ty_white_x(i), on = hit == 11 + i;
        px_rect(x, TY_KY, 6, TY_WH, on ? UI_LIB_COL : PX_LGREY);
        px_hline(x, TY_KY + TY_WH - 1, 6, on ? PX_ORANGE : PX_GREY);
        ty_char(W[i], x + 3, TY_KY + TY_WH - 9, PX_BLACK);
    }
    for (j = 0; j < 11; j++) {
        const int x = ty_black_x(j), on = hit == j;
        px_rect(x - 1, TY_KY, 7, TY_BH + 1, PX_LGREY);   /* (a gap around it) */
        px_rect(x, TY_KY, 5, TY_BH, on ? UI_LIB_COL : PX_BLACK);
        ty_char(B[j], x + 2, TY_KY + TY_BH - 8, on ? PX_BLACK : PX_WHITE);
    }
}

/* a key's character's place on the keybed (the flight's start) */
static void ty_key_at(int hit, int *x, int *y)
{
    if (hit >= 11) {
        *x = ty_white_x(hit - 11) + 3;
        *y = TY_KY + TY_WH - 9;
    } else {
        *x = ty_black_x(hit < 0 ? 0 : hit) + 2;
        *y = TY_KY + TY_BH - 8;
    }
}

/* the name: twelve cells, the cursor, a selection */
static void ty_row(const ui_type_t *v)
{
    const int len = (int)strlen(v->name), blink = (int)floorf(v->t * 2.5f) % 2 == 0;
    int k;
    if (v->sel && len)
        px_rect(TY_X0 - 1, TY_Y - 2, len * 8 + 1, 12, PX_BROWN);
    for (k = 0; k < 12; k++) {
        const int x = TY_X0 + k * 8, c = k == v->cur && !v->sel, f = k == v->fresh;
        const char ch[2] = {k < len ? v->name[k] : ' ', 0};
        if (c && blink) {                               /* the cursor: a block, the character in it reversed */
            px_rect(x - 1, TY_Y - 1, 8, 10, UI_KSEL);
            if (ch[0] != ' ')
                px_text(x, TY_Y, ch, UI_BG, PX_FB, 1);
            continue;
        }
        px_dots(x, TY_Y + 10, 7, c ? UI_KSEL : UI_G3, 2);
        if (ch[0] != ' ')
            px_text(x, TY_Y, ch, f ? UI_LIB_COL : UI_K3, PX_FB, 1);
    }
    if (v->full < 1.f && (int)(v->full * 4.f) % 2 == 0)   /* full: the row flashes orange */
        px_box(TY_X0 - 3, TY_Y - 4, 12 * 8 + 5, 17, UI_K4);
    if (v->del < 1.f && v->del_at >= 0 && v->del_ch != ' ' && v->del < 0.85f) {   /* F1: it drops away, orange */
        const char ch[2] = {v->del_ch, 0};
        px_text(TY_X0 + v->del_at * 8, TY_Y + lb_r(v->del * 16.f), ch, UI_K4, PX_FB, 1);
    }
    if (v->fly < 1.f && v->fresh >= 0 && v->hit >= 0) { /* the typed letter flies up from its key */
        const float e = lb_ease(v->fly);
        const char ch[2] = {v->name[v->fresh], 0};
        int kx, ky, x, y;
        ty_key_at(v->hit, &kx, &ky);
        x = kx + lb_r((float)(TY_X0 + v->fresh * 8 + 3 - kx) * e);
        y = ky + lb_r((float)(TY_Y - ky) * e);
        if (ch[0] != ' ')
            px_ctext(x, y, ch, UI_LIB_COL, e > 0.5f ? PX_FB : PX_FS, 1);
    }
}

/* NEW / REPLACE: the one chosen filled, the other outlined dim */
static void ty_pill(int x, int y, int w, const char *s, qd_rgb col, int on)
{
    if (on)
        px_rect(x, y, w, 10, col);
    px_box(x, y, w, 10, on ? col : UI_G2);
    px_ctext(x + w / 2, y + 2, s, on ? UI_BG : UI_G1, PX_FS, 1);
}

/* a die, its face (1..6), at (x, y), 9 x 9 */
static void ty_die(int x, int y, int face, qd_rgb col)
{
    static const signed char PIP[7][6][2] = {
        {{0, 0}}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
        {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
    int k;
    face = face < 1 ? 1 : face > 6 ? 6 : face;
    px_rbox(x, y, 9, 9, col, QD_NONE);
    for (k = 0; k < face; k++)
        px_pset(x + 4 + PIP[face][k][0] * 2, y + 4 + PIP[face][k][1] * 2, col);
}

/* PRESETS / KNOB 4 turned: the names seen, the newest at the foot, older ones above; a new one rolled: a die tumbles
 * in beside it and lands; walking back: the ten places as dots, the one shown lit */
static void ty_ideas(const ui_type_t *v)
{
    int d;
    for (d = -2; d <= 1; d++) {
        static const int Y[4] = {14, 21, 30, 42};       /* (d -2 .. 1) */
        const int i = v->hpos - d;                      /* (above: older; below: newer) */
        if (i < 0 || i >= v->nhist || !v->hist[i])
            continue;
        if (d == 0)
            lb_name(4, Y[2], 100, v->hist[i], UI_K3, 1);
        else
            px_ctext(54, Y[d + 2], v->hist[i], d == -2 ? UI_G2 : UI_G1, PX_FS, 1);
    }
    if (v->dice >= 0.f && v->hpos == 0) {               /* the die: tumbling in from above, then landed */
        const float u = v->dice, e = u < 1.f ? u : 1.f;
        const int bounce = e < 1.f ? lb_r(fabsf(sinf(e * UI_PI * 2.5f)) * (1.f - e) * 14.f) : 0;
        const int face = e < 1.f ? 1 + ((int)(u * 14.f) + v->dice_face) % 6 : v->dice_face;
        ty_die(106, 26 - bounce, face, UI_K2);
    } else {
        lb_spark(110, 31, 2, UI_LIB_COL);
    }
    if (v->hpos > 0) {                                  /* back in the history: where among the ten */
        int k;
        for (k = 0; k < QD_UI_NHIST; k++) {
            const int x = 60 + (QD_UI_NHIST - 1 - k) * 5 - (QD_UI_NHIST - 1) * 5 / 2;
            px_rect(x, 60, 2, 2, k < v->nhist ? (k == v->hpos ? UI_LIB_COL : UI_G1) : UI_G3);
        }
    }
}

FZ_COLD void ui_type_draw(const ui_type_t *v)
{
    px_text(4, 3, v->title, UI_LIB_COL, PX_FS, 1);
    if (v->track)
        ui_box(216.f, 4.f, v->track, UI_TRACK_PAL[(v->track - 1) & 7]);
    if (v->step) {                                      /* FIZZ beta.18: a step selected: the sound saved, the STEP's
                                                         * (filled) or the TRACK's (KNOB 4) */
        char lab[20] = "STEP ";
        ui_itoa(lab + 5, v->step);
        ui_chip(v->src ? lab : "TRACK", 172.f, 18.f, 12.f, 60.f, v->src ? UI_STEP_FRAME : UI_G1, v->src);
    }
    px_dots(0, 11, 120, UI_G3, 2);
    if (v->ideas_on) {
        ty_ideas(v);
    } else {
        ty_row(v);
        if (v->spark)
            lb_spark(114, 18, 2, UI_LIB_COL);
    }
    if (v->shift) {                                     /* SHIFT held: the signs' layer */
        px_box(4, 56, 25, 10, UI_K3);
        px_text(7, 58, "SHIFT", UI_K3, PX_FS, 1);
    } else if (v->replace && !v->ideas_on) {            /* NEW (as the new name) or REPLACE: F2 chooses */
        if (v->newname && v->newname[0] && !v->choice)
            px_ctext(60, 46, v->newname, UI_G1, PX_FS, 1);
        ty_pill(6, 56, 50, "NEW", UI_LIB_COL, !v->choice);
        ty_pill(64, 56, 50, "REPLACE", PR_ORANGE, v->choice);
    }
    if (v->sub && v->sub[0] && !v->ideas_on && !v->shift)
        px_ctext(60, 50, v->sub, UI_LIB_COL, PX_FB, 1);
    ty_keybed(v->glow, v->shift);
}
