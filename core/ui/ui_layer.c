/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the layer maps, SEQ's patterns, SHIFT, the settings menus, the latch mode (FIZZ's ui_layer.c, redrawn on
 * the 120 x 120 grid; the models are FIZZ's). While F1, F2, F3 or B11 is held the screen is its map, so it explains
 * itself: the black keys drawn where they are on the panel (ui_keymap), each with an icon or a number.
 *   F1 / B11  the eight pages as rooms of the tower: each one's number, its steps as a grid of lights, the edited one
 *             white, the loop's grey, the rest dark blue, the playing one's number underlined red
 *   F2        the hand: the step picked up, as a card; the next W's act (cut / paste) big beside it
 *   F3        the page's sixteen shafts under its range's bracket, the range and the rate big, the tempo
 *   SEL       the six tracks as speakers: a muted one crossed, a soloed one's S
 * And SEQ's patterns (the pattern big, the bank as a keypad of sixteen), SHIFT (the track and its shortcuts), the
 * menus (REC, SETTINGS, the track's: a row a knob, its badge, its value big in its colour, a little picture). */
#include <math.h>
#include "fz_math.h"
#include <string.h>

#include "ui.h"
#include "px.h"

static const char *const DOT = " \xc2\xb7 ";

#define cat ui_cat                               /* (the kit's) */

static const char *const NUM[8] = {"1", "2", "3", "4", "5", "6", "7", "8"};
#define KM_Y 202.f
#define KM_H 30.f

static void keys_clear(ui_kkey_t k[11])
{
    int i;
    for (i = 0; i < 11; i++) {
        k[i].icon = UI_IC_NONE, k[i].num = 0, k[i].col = 0;
        k[i].lit = k[i].dim = k[i].mark = 0;
    }
}

/* a head for the maps: the title left, [n] the track right (ui_head_track), a rule under it */
static void map_head(const char *t, int track, const char *name)
{
    ui_head_track(t, track, name, UI_K3);
    px_dots(0, 11, 120, UI_G3, 2);
}

/* ---------------------------------------------------------------- F1 / B11: the pages as the tower's rooms */
static void page_row(const ui_layer_t *v)
{
    uint16_t trig = 0;
    int i;
    for (i = 0; i < 16; i++)
        trig |= (uint16_t)((v->trig[v->page & 7][i] != 0) << i);
    ui_page_row(KM_Y + 6.f, trig, v->b11_sel, -1, v->b11_play);
}

static void f1(const ui_layer_t *v)
{
    ui_kkey_t k[11];
    int p, i;
    for (p = 0; p < 8; p++) {
        const int x = 3 + (p % 4) * 29, y = 14 + (p / 4) * 33, w = 27, h = 31;
        const int ed = p == v->page, in = (v->loop >> p) & 1, cp = p == v->page_copied;
        const int st = v->pstart[p] > 15 ? 15 : v->pstart[p];
        const int en = v->pend[p] < st ? st : v->pend[p] > 15 ? 15 : v->pend[p];
        const qd_rgb fc = cp ? (v->blink ? UI_K2 : UI_G2) : ed ? UI_K3 : in ? UI_G1 : UI_G3;
        px_box(x, y, w, h, fc);
        if (ed)
            px_box(x + 1, y + 1, w - 2, h - 2, fc);
        px_text(x + 3, y + 3, NUM[p], ed ? UI_K3 : in ? UI_G1 : UI_G2, PX_FB, 1);
        for (i = 0; i < 16; i++) {                      /* its steps, a beat a row */
            const int dx = x + 12 + (i % 4) * 3, dy = y + 3 + (i / 4) * 3;
            if (i < st || i > en)
                continue;
            if (v->trig[p][i])
                px_rect(dx, dy, 2, 2, ed ? UI_K3 : in ? UI_G1 : UI_G2);
            else
                px_pset(dx, dy, UI_G3);
        }
        if (v->f1_copied && v->f1_src >= 0 && v->f1_src / 16 == p) {   /* F1's copy came from here */
            const int j = v->f1_src % 16;
            px_box(x + 11 + (j % 4) * 3, y + 2 + (j / 4) * 3, 4, 4, UI_K2);
        }
        if (p == v->play_page)
            px_rect(x + 3, y + 12, 7, 1, v->blink ? PX_LRED : UI_RED);
        px_sprite(x + 9, y + h - 4, PXS_LIFT, 0);      /* the room's floor */
    }
    keys_clear(k);
    for (p = 0; p < 8; p++) {
        k[p].num = p + 1;
        k[p].col = v->page_copied >= 0 ? UI_K2 : (v->loop >> p) & 1 ? UI_K3 : UI_G1;
        k[p].lit = p == v->page && v->page_copied < 0;
    }
    page_row(v);
    if (v->b11)
        return;
    k[8].icon = UI_IC_CLEAR, k[8].col = UI_K4;
    k[9].icon = UI_IC_COPY, k[9].lit = v->page_copied >= 0;
    k[10].icon = UI_IC_UNDO, k[10].dim = !v->can_undo;
    ui_keymap(k, 164.f, KM_H);
}

/* ---------------------------------------------------------------- F2: the hand */
void ui_step_card(float x, float y, float w, float h, int full, int type, int nnote, const uint8_t *note)
{
    const int gx = (int)(x * 0.5f), gy = (int)(y * 0.5f), gw = (int)(w * 0.5f), gh = (int)(h * 0.5f);
    if (!full) {
        int i;
        for (i = 0; i < gw; i += 3)
            px_pset(gx + i, gy, UI_G2), px_pset(gx + i, gy + gh - 1, UI_G2);
        for (i = 0; i < gh; i += 3)
            px_pset(gx, gy + i, UI_G2), px_pset(gx + gw - 1, gy + i, UI_G2);
        return;
    }
    px_rbox(gx, gy, gw, gh, UI_K3, PX_DGREY);
    if (type == 0) {
        ui_icon(UI_IC_KEY, x + w / 2.f, y + h / 2.f, 2.f, UI_G1);
    } else if (type == 2) {
        px_rect(gx + gw / 2 - 4, gy + gh / 2 - 3, 8, 6, UI_LOCK_COL);
    } else {
        qd_rgb fill[128];
        int i, lo = 127;
        for (i = 0; i < 128; i++)
            fill[i] = QD_NONE;
        for (i = 0; i < nnote && i < 6; i++) {
            fill[note[i] & 127] = UI_K3;
            lo = note[i] < lo ? note[i] : lo;
        }
        ui_keyboard(x + 8.f, y + 10.f, w - 16.f, h - 20.f, lo >= 5 ? lo - (lo + 7) % 12 : 0, fill);
    }
}

static void f2(const ui_layer_t *v)
{
    const int next_paste = v->hand_loaded && v->hand;
    page_row(v);
    if (v->last_w >= 0 && v->last_w < 16)
        ui_icon(v->last_act == 1 ? UI_IC_CUT : UI_IC_PASTE, (float)(2 * (9 + v->last_w * 6 + (v->last_w / 4) * 2)),
                KM_Y - 10.f, 1.f, v->last_act == 1 ? UI_K4 : UI_K2);
    ui_step_card(32.f, 48.f, 120.f, 100.f, v->hand, v->clip_type, v->clip_nnote, v->clip_note);
    px_sprite_sc(84, 44, PXS_AGENT_STAND, 0, 2, QD_NONE);   /* the agent carrying it */
    ui_icon(next_paste ? UI_IC_PASTE : UI_IC_CUT, 206.f, 70.f, 2.f, next_paste ? UI_K2 : UI_K4);
}

/* ---------------------------------------------------------------- F3: the range and the rate */
static int sx16(int i) { return 4 + i * 7 + (i / 4); }

static void f3(const ui_layer_t *v)
{
    const int pg = v->page < 0 ? 0 : v->page > 7 ? 7 : v->page;
    const int st = v->pstart[pg] > 15 ? 15 : v->pstart[pg];
    const int en = v->pend[pg] < st ? st : v->pend[pg] > 15 ? 15 : v->pend[pg];
    ui_kkey_t k[11];
    char s[64], n[12];
    int i;
    s[0] = 0;
    cat(cat(s, "PAGE "), NUM[pg]);
    px_text(4, 14, s, UI_G1, PX_FS, 1);
    for (i = 0; i < 16; i++) {                          /* the shafts: in the range bright, out dark */
        const int in = i >= st && i <= en, an = i == v->anchor;
        const qd_rgb oc = an ? (v->blink ? UI_K3 : UI_G1) : in ? UI_G1 : UI_G3;
        px_box(sx16(i), 26, 6, 20, oc);
        if (in && v->trig[pg][i])
            px_rect(sx16(i) + 2, 41, 2, 2, UI_G1);
    }
    px_hline(sx16(st), 23, sx16(en) + 6 - sx16(st), UI_K3);   /* the bracket: KNOB 3's start, KNOB 4's end */
    px_vline(sx16(st), 23, 3, UI_K2);
    px_vline(sx16(en) + 5, 23, 3, UI_K4);
    px_text(sx16(st) + 1, 48, ui_itoa(n, st + 1), UI_K2, PX_FS, 1);
    if (en != st)
        px_rtext(sx16(en) + 5, 48, ui_itoa(n, en + 1), UI_K4, PX_FS, 1);
    s[0] = 0;
    cat(cat(cat(s, ui_itoa(n, st + 1)), "-"), ui_itoa(n, en + 1));
    px_text(4, 62, s, UI_K3, PX_FB, 1);
    {   /* the rate: its note value in KNOB 1's colour, the dot or T in KNOB 2's */
        const char *val = v->rate_val_name ? v->rate_val_name : "1/16";
        const int xr = 116 - (v->rate_type ? 6 : 0);
        px_rtext(xr, 58, val, UI_K1, PX_FB, 2);
        if (v->rate_type == 1)
            px_rect(xr + 2, 70, 3, 3, UI_K2);
        else if (v->rate_type == 2)
            px_text(xr + 2, 58, "T", UI_K2, PX_FS, 1);
    }
    s[0] = 0;
    cat(cat(cat(s, v->ext ? "EXT " : ""), ui_itoa(n, v->bpm)), " BPM");
    cat(cat(cat(s, DOT), ui_itoa(n, v->swing)), "%");
    if (v->tempo_hot > 0.f)
        px_box(60 - px_text_w(s, PX_FS, 1) / 2 - 3, 84, px_text_w(s, PX_FS, 1) + 6, 9, UI_KSEL);
    px_ctext(60, 86, s, v->tempo_hot > 0.f ? UI_KSEL : UI_K3, PX_FS, 1);
    keys_clear(k);
    for (i = 0; i < 7; i++) {
        k[i].icon = UI_IC_NOTE + i;
        k[i].col = UI_K1;
        k[i].lit = i == v->rate_val;
    }
    k[7].icon = UI_IC_NOTE + 2, k[8].icon = UI_IC_DOT, k[9].icon = UI_IC_TRI;
    for (i = 7; i < 10; i++) {
        k[i].col = UI_K2;
        k[i].lit = i - 7 == v->rate_type;
    }
    ui_keymap(k, KM_Y, KM_H);
}

void ui_layer_draw(const ui_layer_t *v)
{
    static const char *const T[3] = {"F1 \xc2\xb7 PAGES", "F2 \xc2\xb7 CUT+PASTE", "F3 \xc2\xb7 RANGE+RATE"};
    const int l = v->layer < 1 ? 1 : v->layer > 3 ? 3 : v->layer;   /* (SEL's track map: gone, as FIZZ's) */
    if (l == 3 && v->latched) {                         /* a latched menu: the back arrow (OCT-) */
        map_head(T[l - 1], v->track, "");
        px_box(88, 1, 12, 9, UI_K3);
        ui_icon(UI_IC_BACK, 188.f, 11.f, 1.f, UI_K3);
    } else {
        map_head(v->b11 ? "B11 \xc2\xb7 PAGES" : T[l - 1], v->track, v->name ? v->name : "");
    }
    if (l == 1)
        f1(v);
    else if (l == 2)
        f2(v);
    else
        f3(v);
}

/* ---------------------------------------------------------------- SEQ: the patterns, the pocket computer's keypad */
void ui_patterns_draw(const ui_patterns_t *v)
{
    static const char *const LAUNCH[4] = {"END", "BAR", "BEAT", "NOW"};
    static const char *const BANK[4] = {"A", "B", "C", "D"};
    const int bank = v->bank < 0 ? 0 : v->bank > 3 ? 3 : v->bank;
    const int big = v->queued >= 0 ? v->queued : v->cur;
    char s[8], n[12], hb[8];
    int i;
    hb[0] = 0;
    cat(cat(hb, "BANK "), BANK[bank]);
    ui_head("PATTERNS", hb, 0, UI_K3, UI_G1);
    px_dots(0, 11, 120, UI_G3, 2);
    s[0] = 0;
    cat(s, BANK[(big >> 4) & 3]);
    if ((big & 15) + 1 < 10)
        cat(s, "0");
    cat(s, ui_itoa(n, (big & 15) + 1));
    px_text_sh(4, 22, s, v->queued >= 0 ? UI_K4 : UI_K3, UI_G3, PX_FB, 2);
    ui_icon(v->playing ? UI_IC_PLAY : UI_IC_STOP, 14.f, 100.f, 1.f, v->playing ? UI_K3 : UI_G1);
    if (v->queued >= 0) {                               /* queued: a fuse burning down to the change */
        px_hline(14, 50, 44, UI_G3);
        if (v->playing && v->left > 0.f)
            px_hline(14, 50, (int)(44.f * v->left), UI_K4);
    }
    px_rbox(66, 15, 50, 66, UI_G1, PX_DGREY);           /* the keypad */
    for (i = 0; i < 16; i++) {
        const int k = bank * 16 + i, x = 70 + (i % 4) * 11, y = 19 + (i / 4) * 11;
        if (k == v->cur)
            px_rect(x, y, 9, 9, UI_K3);
        else if (k == v->queued)
            px_rect(x, y, 9, 9, v->blink ? UI_K4 : UI_G2);
        else if ((v->used >> i) & 1u)
            px_rect(x, y, 9, 9, PX_GREY);
        else
            px_box(x, y, 9, 9, UI_G2);
        if (v->layer && k == (v->layer == 1 ? v->src : v->last))   /* (F1's copy, F2's last W: framed) */
            px_box(x - 1, y - 1, 11, 11, v->layer == 2 && v->last_act == 1 ? UI_K4 : UI_K2);
    }
    if (v->layer == 2) {                                /* F2: the hand, a card with its pattern (dim: empty) */
        px_box(4, 56, 24, 13, v->loaded ? UI_K3 : UI_G2);
        if (v->loaded && v->hand >= 0) {
            n[0] = BANK[(v->hand >> 4) & 3][0], n[1] = (char)('0' + ((v->hand & 15) + 1) / 10);
            n[2] = (char)('0' + ((v->hand & 15) + 1) % 10), n[3] = 0;
            px_ctext(16, 60, n, UI_K3, PX_FS, 1);
        }
    }
    if (v->layer)                                       /* what the next W does */
        ui_icon(v->layer == 1 ? (v->src < 0 ? UI_IC_COPY : UI_IC_PASTE) : v->next_paste ? UI_IC_PASTE : UI_IC_CUT,
                84.f, 126.f, 1.f, v->layer == 2 && !v->next_paste ? UI_K4 : UI_K2);
    for (i = 0; i < 4; i++) {                           /* the banks: the one shown boxed */
        const int x = 70 + i * 11, y = 66, on = i == bank, has = (v->bank_used >> i) & 1u;
        if (on)
            px_box(x, y, 9, 10, UI_K3);
        px_ctext(x + 5, y + 2, BANK[i], on || has ? UI_K3 : UI_G1, PX_FS, 1);
        if ((v->cur >> 4) == i)
            px_pset(x + 4, y + 11, UI_K3);
    }
    ui_knob_mark(14.f, 176.f, (float)v->launch / 3.f, UI_G1);
    for (i = 0; i < 4; i++) {                           /* the launch mode (ALGORITHM) */
        const int x = 34 + i * 22;
        px_ctext(x, 86, LAUNCH[i], i == v->launch ? UI_K3 : UI_G2, PX_FS, 1);
        if (i == v->launch)
            px_hline(x - 6, 93, 12, UI_K3);
    }
}

/* ---------------------------------------------------------------- SHIFT */
static void tile(int x, int y, int w, qd_rgb c) { px_rbox(x, y, w, 15, c, QD_NONE); }
static void btn(const char *nm, int x, int y, qd_rgb c)   /* a button's box with its printed name */
{
    const int w = px_text_w(nm, PX_FS, 1) + 4;
    px_box(x, y, w, 9, c);
    px_text(x + 2, y + 2, nm, c, PX_FS, 1);
}

static void shift_keys(const ui_shift_t *v)
{
    static const uint8_t SK[11] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1};
    int i;
    for (i = 0; i < 11; i++) {
        const int x = 4 + i * 10 + (i == 10 ? 2 : 0), cx = x + 4, cy = 105;
        const qd_rgb c = !SK[i] ? UI_G3 : (i == 2 && !v->snap_ok) || (i == 4 && !v->pclip_ok) ? UI_G2
                       : i == 10 ? UI_K4 : i == 6 && v->metro ? UI_K2 : i == 7 && v->follow ? UI_K2 : UI_K3;
        px_box(x, 96, 9, 18, c);
        if (i == 10 && v->armed)
            px_box(x + 1, 97, 7, 16, c);
        switch (i) {
        case 0: px_box(cx - 3, cy - 2, 7, 5, c), px_pset(cx, cy, c); break;   /* the snapshot stored */
        case 2: px_box(cx - 3, cy, 7, 5, c), px_vline(cx, cy - 5, 5, c); break;   /* .. recalled */
        case 3: ui_icon(UI_IC_COPY, (float)(2 * cx + 1), (float)(2 * cy), 1.f, c); break;
        case 4: ui_icon(UI_IC_PASTE, (float)(2 * cx + 1), (float)(2 * cy), 1.f, c); break;
        case 5: px_rect(cx - 1, cy - 1, 3, 3, c), px_box(cx - 3, cy - 3, 7, 7, c); break;   /* tap */
        case 6: {                                       /* the metronome */
            int p[6] = {cx - 3, cy + 4, cx + 3, cy + 4, cx, cy - 5};
            px_poly(p, 3, 1, c, QD_NONE);
            px_line(cx, cy + 2, cx + 3, cy - 3, c);
            break;
        }
        case 7: ui_icon(UI_IC_FOLLOW, (float)(2 * cx + 1), (float)(2 * cy), 1.f, c); break;
        case 8: px_box(cx - 4, cy - 2, 4, 5, c), px_box(cx + 1, cy - 2, 4, 5, c); break;   /* the track doubled */
        case 10: ui_icon(UI_IC_CLEAR, (float)(2 * cx + 1), (float)(2 * cy), 1.f, c); break;
        default: break;
        }
    }
}

FZ_COLD void ui_shift_draw(const ui_shift_t *v)
{
    const int t = v->track < 1 ? 0 : (v->track - 1) % UI_NTRACK;
    const qd_rgb tc = UI_TRACK_PAL[t], kc = v->kind >= 0 && v->kind < 7 ? UI_PK_COL[v->kind] : UI_K3;
    const char *nm = v->name ? v->name : "";
    char n[12], sw[8];
    int x, i;
    px_rect(0, 0, 120, 120, UI_BG);
    /* the track band: its number, its kind, its name big; SELECT's six, the selected filled */
    px_rect(3, 3, 16, 16, tc);
    px_ctext(11, 7, ui_itoa(n, t + 1), UI_BG, PX_FB, 1);
    x = 23;
    if (v->kind >= 0) {
        ui_kind_icon(v->kind, 54.f, 22.f, 1.f, kc);
        x = 33;
    }
    if (px_text_w(nm, PX_FB, 1) <= 116 - x)
        px_text(x, 4, nm, kc, PX_FB, 1);
    else
        px_text(x, 6, nm, kc, PX_FS, 1);
    for (i = 0; i < UI_NTRACK; i++) {
        if (i == t)
            px_rect(x + i * 6, 14, 4, 5, UI_TRACK_PAL[i]);
        else
            px_box(x + i * 6, 14, 4, 5, UI_G2);
    }
    px_rect(98, 13, 7, 7, UI_KSEL);
    px_ctext(102, 14, "S", UI_BG, PX_FS, 1);
    px_dots(0, 23, 120, UI_G3, 2);
    /* the tiles: P the root, A the scale; PLAY the tempo and swing; F1 / F2; REC's mode; SEL the track settings */
    tile(3, 27, 55, UI_K3);
    px_rect(6, 31, 7, 7, PX_WHITE), px_ctext(10, 32, "P", UI_BG, PX_FS, 1);
    px_rtext(55, 30, v->root ? v->root : "", UI_K3, PX_FB, 1);
    tile(61, 27, 56, UI_K3);
    px_rect(64, 31, 7, 7, UI_KALG), px_ctext(68, 32, "A", UI_BG, PX_FS, 1);
    px_rtext(114, 32, v->scale ? v->scale : "", UI_K3, PX_FS, 1);
    tile(3, 45, 55, UI_K3);
    ui_icon(UI_IC_PLAY, 18.f, 105.f, 1.f, UI_K3);
    ui_itoa(n, v->bpm);
    px_text(15, 49, n, v->ext ? UI_G1 : UI_K3, PX_FB, 1);
    sw[0] = 0;
    cat(cat(sw, ui_itoa(n, v->swing)), "%");
    px_rtext(55, 50, sw, UI_K2, PX_FS, 1);
    tile(61, 45, 27, UI_K3);
    btn("F1", 63, 48, UI_K3);
    ui_icon(UI_IC_LEFT, 162.f, 105.f, 1.f, v->can_undo ? UI_K3 : UI_G2);
    tile(90, 45, 27, UI_K3);
    btn("F2", 92, 48, UI_K3);
    ui_icon(UI_IC_RIGHT, 220.f, 105.f, 1.f, v->can_redo ? UI_K3 : UI_G2);
    tile(3, 63, 55, PX_LRED);
    px_rect(7, 67, 7, 7, PX_LRED);
    px_text(19, 66, v->rec_step ? "STEP" : "LIVE", PX_LRED, PX_FB, 1);
    tile(61, 63, 56, tc);
    btn("SEL", 63, 66, tc);
    px_rect(82, 66, 9, 9, tc);
    px_ctext(87, 68, ui_itoa(n, t + 1), UI_BG, PX_FS, 1);
    ui_icon(UI_IC_GEAR, 200.f, 141.f, 1.f, tc);
    px_dots(0, 82, 120, UI_G3, 2);
    shift_keys(v);
}

/* ---------------------------------------------------------------- the settings menus */
static const qd_rgb KCOL[4] = {UI_K1, UI_K2, UI_K3, UI_K4};
#define ROW_Y(k) (15 + (k) * 26)                       /* four rows under the head, 26 apart */

/* a row: the knob's badge, its name; its value big at the right in the knob's colour */
static void row(int k, const char *name, const char *val)
{
    char n[4] = {(char)('1' + k), 0};
    const int y = ROW_Y(k);
    px_rect(3, y, 7, 7, KCOL[k]);
    px_ctext(7, y + 1, n, UI_BG, PX_FS, 1);
    px_text(13, y + 1, name, UI_G1, PX_FS, 1);
    if (val) {
        if (px_text_w(val, PX_FB, 2) <= 64)
            px_rtext(116, y + 9, val, KCOL[k], PX_FB, 2);
        else if (px_text_w(val, PX_FB, 1) <= 100)
            px_rtext(116, y + 12, val, KCOL[k], PX_FB, 1);
        else
            px_rtext(116, y + 14, val, KCOL[k], PX_FS, 1);
    }
}

static void menu_head(const char *title, int npage, int page)
{
    int k;
    px_text(4, 3, title, UI_K3, PX_FS, 1);
    for (k = 0; k < npage; k++)
        px_rect(4 + px_text_w(title, PX_FS, 1) + 4 + k * 4, 5, 2, 2, k == page ? UI_K3 : UI_G2);
    px_box(104, 1, 13, 9, UI_K3);                       /* latched: OCT- back */
    ui_icon(UI_IC_BACK, 221.f, 11.f, 1.f, UI_K3);
    px_dots(0, 11, 120, UI_G3, 2);
}

static void menu_rec2(const ui_menu_t *v)
{
    static const uint8_t LV[8] = {7, 11, 18, 24, 19, 14, 8, 12};
    static const uint8_t NOTE[8] = {1, 0, 0, 1, 0, 1, 0, 0};
    const int on = v->rec_auto, px = 14 + (int)fmodf(v->t * 22.f, 88.f);
    int i;
    row(0, "AUTOMATION", on ? "ON" : "OFF");
    for (i = 0; i < 8; i++) {
        const int x = 14 + i * 11;
        px_rect(x + 3, 84 - LV[i], 4, LV[i], on ? UI_K1 : UI_G3);
        if (NOTE[i])
            px_rect(x, 88, 9, 9, UI_G1);
        else
            px_box(x, 88, 9, 9, on ? UI_K1 : UI_G3);
    }
    if (!on)
        px_line(12, 52, 104, 86, PX_LRED);
    px_vline(px, 50, 50, on ? PX_LRED : UI_G2);
}

static void menu_rec(const ui_menu_t *v)
{
    char q[12];
    int i;
    menu_head("REC", 2, v->rec_page);
    if (v->rec_page) {
        menu_rec2(v);
        return;
    }
    row(0, "MODE", v->rec_step ? "STEP" : "LIVE");
    for (i = 0; i < 8; i++) {                           /* STEP: cells, the cursor's lit; LIVE: notes off the grid */
        if (v->rec_step) {
            if (i == 3)
                px_rect(14 + i * 6, 27, 5, 6, UI_K1);
            else
                px_box(14 + i * 6, 27, 5, 6, UI_G2);
        } else if (i < 4) {
            px_vline(16 + i * 12, 27, 7, UI_G3);
            px_rect(16 + i * 12 + (i & 1 ? -2 : 2), 28, 3, 3, UI_K1);
        }
    }
    cat(ui_itoa(q, v->quant), "%");
    row(1, "QUANTIZE", q);
    for (i = 0; i < 4; i++) {                           /* the notes pulled onto their grid lines by q % */
        static const int8_t OFF[4] = {4, -3, 2, -5};
        const int gx = 18 + i * 14, nx = gx + OFF[i] * (100 - v->quant) / 100;
        px_vline(gx, 53, 8, UI_G1);
        px_rect(nx - 1, 56, 3, 3, UI_K2);
    }
    row(2, "TAKE", v->dub == 0 ? "CAPTURE" : v->dub == 1 ? "OVERDUB" : "REPLACE");
    for (i = 0; i < 6; i++)
        px_box(14 + i * 7, 80, 5, 10, i < 4 ? UI_G1 : UI_G3);
    if (v->dub == 0)
        px_vline(42, 77, 16, PX_LRED);
    row(3, "EDIT KEYS", v->ekeys ? "FULL" : "SCALE");
    for (i = 0; i < 8; i++)
        px_box(14 + i * 6, 106, 5, 10, !v->ekeys && i % 7 == 0 ? UI_K4 : UI_G1);
}

void ui_auto_cue(int k, float since, float t)
{
    const int lit = fz_sinf(t * 6.f) > 0.f || (since >= 0.f && since < 0.3f);
    px_box(0, 0, 120, 120, lit ? PX_LRED : UI_RED);
    if (since >= 0.f && since < 0.9f && k >= 0 && k < 4)
        px_rect(13 + k * 30, 116, 4, 3, PX_LRED);
}

void ui_lmode_draw(int mode, float t)
{
    int side, i;
    (void)t;
    px_rect(0, 0, 120, 120, UI_BG);
    for (side = 0; side < 2; side++) {
        const int x0 = 5 + side * 57, modes = side == 0, on = (modes ? 1 : 0) == mode;
        const qd_rgb c = on ? UI_K3 : UI_G2;
        px_box(x0, 14, 53, 80, c);
        if (on)
            px_box(x0 + 1, 15, 51, 78, c);
        for (i = 0; i < 4; i++) {
            if (i == 1)
                px_rect(x0 + 7 + i * 10, 22, 8, 8, c);
            else
                px_box(x0 + 7 + i * 10, 22, 8, 8, UI_G3);
        }
        for (i = 0; i < 10; i++) {                      /* the black keys */
            const qd_rgb kc = modes ? (on ? UI_MODE_COL[i] : UI_G2)
                                    : i == 6 ? (on ? UI_K1 : UI_G1) : on ? UI_G2 : UI_G3;
            px_rect(x0 + 4 + i * 5, 38, 3, 15, kc);
        }
        if (!modes) {
            px_box(x0 + 7, 60, 38, 6, c);
            px_rect(x0 + 8, 61, 26, 4, on ? UI_K1 : UI_G2);
        } else {
            px_circle(x0 + 26, 64, 7, c, QD_NONE);
            px_line(x0 + 26, 64, x0 + 31, 59, on ? UI_K1 : UI_G2);
        }
        px_ctext(x0 + 27, 80, modes ? "MODES" : "VALUES", c, PX_FS, 1);
    }
    px_ctext(60, 102, "LATCH", UI_K3, PX_FB, 1);
}

#if QD_PKG_BLE
static __attribute__((noinline)) void menu_set2(const ui_menu_t *v)
{
    static const char *const ST[3] = {"OFF", "WAITING", "CONNECTED"};
    const int st = v->ble_st < 0 || v->ble_st > 2 ? 0 : v->ble_st;
    row(0, "BLUETOOTH", v->ble_on ? "ON" : "OFF");
    row(1, "STATUS", ST[st]);
}
#endif

#if QD_USB_HOST
/* SETTINGS' page 3 (FIZZ 2026-11-14), in pixels: KNOB 1 USB HOST ON / OFF (the USB mark, struck through when OFF),
 * KNOB 2's row its state, shown only: OFF, WAITING (three lamps in turn), CONNECTED (all lit), NOT MIDI (red) */
static __attribute__((noinline)) void menu_usbh(const ui_menu_t *v)
{
    static const char *const ST[4] = {"OFF", "WAITING", "CONNECTED", "NOT MIDI"};
    const int st = !v->usbh_on ? 0 : v->usbh_st < 0 || v->usbh_st > 2 ? 1 : v->usbh_st + 1;
    const qd_rgb c = v->usbh_on ? KCOL[0] : UI_G2;
    const int x = 20, y = ROW_Y(0) + 10;
    int i;
    row(0, "USB HOST", v->usbh_on ? "ON" : "OFF");
    px_vline(x, y + 2, 10, c);                          /* the mark: the stem and its arrow, the two branches */
    px_rect(x - 1, y + 1, 3, 1, c);
    px_rect(x - 1, y + 12, 3, 2, c);
    px_line(x, y + 9, x - 4, y + 6, c);
    px_rect(x - 5, y + 4, 2, 2, c);
    px_line(x, y + 11, x + 4, y + 8, c);
    px_box(x + 3, y + 5, 3, 3, c);
    if (!v->usbh_on)
        px_line(x - 6, y + 1, x + 6, y + 13, PX_LRED);
    row(1, "STATUS", ST[st]);
    for (i = 0; i < 3; i++) {
        const int on = st == 2 || (st == 1 && i == (int)fmodf(v->t * 3.f, 3.f));
        if (on || st == 3)
            px_rect(14 + i * 8, ROW_Y(1) + 13, 5, 5, st == 3 ? PX_LRED : KCOL[1]);
        else
            px_box(14 + i * 8, ROW_Y(1) + 13, 5, 5, UI_G3);
    }
}
#endif

static __attribute__((noinline)) void menu_home(const ui_menu_t *v)
{
    static const char *const VN[3] = {"STEPS", "EDIT", "GEEK OUT"};   /* (FIZZ 2026-11-13: the PIANO ROLL is SEQ's) */
    const int hv = v->home_view < 0 || v->home_view > 2 ? 1 : v->home_view;
    int k;
    row(0, "HOME VIEW", VN[hv]);
    row(1, "LOCK", v->home_lock ? "ON" : "OFF");
    {   /* EvilGenius: THE VOICE: when the villain speaks (and the effects under him), how loud */
        static const char *const VM[3] = {"STOPPED", "ALWAYS", "OFF"};
        char lv[8];
        const int m = v->voice < 0 || v->voice > 2 ? 0 : v->voice;
        ui_itoa(lv, v->voice_lvl);
        strcat(lv, "%");
        row(2, "VOICE", VM[m]);
        row(3, "VOICE LEVEL", lv);
        px_sprite(14, ROW_Y(2) + 9, m != 2 && ((int)(v->t * 4.f) & 1) ? PXS_GENIUS_TALK : PXS_GENIUS, 0);
        for (k = 0; k < 10; k++)                        /* the level: ten bars */
            px_rect(14 + k * 4, ROW_Y(3) + 20 - k, 3, 2 + k, m != 2 && k * 10 < v->voice_lvl ? KCOL[3] : UI_G3);
    }
    (void)hv;
}

static void menu_set(const ui_menu_t *v)
{
    int k;
    menu_head("SETTINGS", 2 + QD_USB_HOST + QD_PKG_BLE, v->set_page);
    if (v->set_page == 1) {
        menu_home(v);
        return;
    }
#if QD_USB_HOST
    if (v->set_page == 2) {
        menu_usbh(v);
        return;
    }
#endif
#if QD_PKG_BLE
    if (v->set_page) {
        menu_set2(v);
        return;
    }
#endif
    row(0, "PAGE FOLLOW", v->follow ? "ON" : "OFF");
    {
        const int px = 14 + (int)fmodf(v->t * 11.f, 44.f);
        for (k = 0; k < 4; k++)
            px_box(14 + k * 11, 26, 10, 9, v->follow && (px - 14) / 11 == k ? UI_K1 : UI_G2);
        px_vline(px, 24, 13, PX_LRED);
    }
    row(1, "BOOST", v->boost ? "+6 DB" : "OFF");
    for (k = 0; k < 8; k++) {
        const int lit = k < (v->boost ? 8 : 5);
        px_rect(14 + k * 6, 60 - k * 2, 4, 4 + k * 2, lit ? (k >= 6 ? PX_LRED : KCOL[1]) : UI_G3);
    }
    row(2, "CLOCK OUT", v->clk_out ? "ON" : "OFF");
    row(3, "CC OUT", v->cc_out ? "ON" : "OFF");
}

static const char *const KIND_NAME[7] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
/* a MIDI socket (DIN 5), 240-space as FIZZ's callers give it */
static void din5(float x, float y, float r, qd_rgb col)
{
    static const float PX[5] = {-1.f, -0.7071f, 0.f, 0.7071f, 1.f}, PY[5] = {0.f, -0.7071f, -1.f, -0.7071f, 0.f};
    int i;
    px_circle((int)(x * 0.5f), (int)(y * 0.5f), (int)(r * 0.5f + 0.5f), col, QD_NONE);
    for (i = 0; i < 5; i++)
        px_pset((int)((x + PX[i] * r * 0.55f) * 0.5f), (int)((y + PY[i] * r * 0.55f) * 0.5f), col);
}

static void menu_trk(const ui_menu_t *v)
{
    const qd_rgb tc = UI_TRACK_PAL[(v->track - 1) & 7];
    char n[12];
    int k;
    px_rect(3, 2, 8, 8, tc);
    px_ctext(7, 3, ui_itoa(n, v->track), UI_BG, PX_FS, 1);
    px_text(14, 3, v->tname ? v->tname : "", tc, PX_FS, 1);
    for (k = 0; k < 3; k++)
        px_rect(84 + k * 4, 5, 2, 2, k == v->page ? UI_K3 : UI_G2);
    px_box(104, 1, 13, 9, UI_K3);
    ui_icon(UI_IC_BACK, 221.f, 11.f, 1.f, UI_K3);
    px_dots(0, 11, 120, UI_G3, 2);
    if (!v->page) {
        row(0, "MODE", v->follow_key ? "GLOBAL" : "LOCAL");
        row(1, "SCALE", v->scale);
        for (k = 0; k < 12; k++) {                      /* C .. B: the scale's notes */
            const int x = 14 + k * 4, in = (v->scale_mask >> k) & 1;
            if (in)
                px_rect(x, 33 + 26, 3, 3, UI_K2);
            else
                px_pset(x + 1, 34 + 26, UI_G3);
            if (k == v->root_pc)
                px_box(x - 1, 32 + 26, 5, 5, UI_K3);
        }
        row(2, "ROOT", v->root);
        row(3, "MIDI CH", ui_itoa(n, v->chan));
    } else if (v->page == 2) {
        static const char *const TL[4] = {"KIND", "NAME", "ENGINE", "MIDI OUT"};
        for (k = 0; k < 4; k++) {
            const char *val = k == 0 ? (v->tkind >= 0 ? KIND_NAME[v->tkind] : "NONE") : k == 1 ? (v->tname ? v->tname : "")
                            : k == 2 ? (v->engine == 2 ? "MIDI" : v->engine ? "SID+MIDI" : "SID TONE")
                            : v->mout ? ui_itoa(n, v->mout) : "OFF";
            row(k, TL[k], val);
            if (k == 0 && v->tkind >= 0)
                ui_kind_icon(v->tkind, 30.f, (float)(2 * (ROW_Y(0) + 14)), 1.f, UI_PK_COL[v->tkind]);
            else if (k == 2 && v->engine != 2)          /* SID TONE: the chip */
                px_sprite(10, ROW_Y(2) + 10, PXS_ICON_SID, 0);
            else if (k == 2)
                din5(32.f, (float)(2 * (ROW_Y(2) + 14)), 9.f, KCOL[2]);
        }
    } else {
        row(0, "VOICES", ui_itoa(n, v->limit));
        for (k = 0; k < 16; k++)
            px_rect(14 + k * 4, ROW_Y(0) + 12, 2, 3, k < v->voices_now ? UI_K3 : k < v->limit ? UI_K1 : UI_G3);
        row(1, "SWING", v->swing ? "ON" : "OFF");
        row(2, "PAGE FOLLOW", v->follow ? "ON" : "OFF");
        row(3, "MIDI FX", v->mfx ? ui_itoa(n, v->mfx) : "OFF");
    }
    if (v->last >= 0 && v->last < 4)                    /* the row turned last: its badge ringed */
        px_box(2, ROW_Y(v->last) - 1, 9, 9, KCOL[v->last]);
}

FZ_COLD void ui_menu_draw(const ui_menu_t *v)
{
    if (v->kind == UI_MENU_REC)
        menu_rec(v);
    else if (v->kind == UI_MENU_TRK)
        menu_trk(v);
    else
        menu_set(v);
}

/* ---------------------------------------------------------------- P1.4a: the sound editor */
#include "ui_snd.c"

/* ---------------------------------------------------------------- P1.5a: the sounds page, names, the projects */
#include <string.h>
#include "ui_lib.c"
#include "ui_presets.c"                                 /* P1.5t: the library browser */
#include "ui_type.c"                                    /* P1.5u: the typewriter (SAVE PRESET, a project's name) */
#include "ui_mfx.c"                                     /* P1.6c: the MIDI FX screen */
#include "ui_geek.c"                                    /* P2.11: the Geek Out pages */
#include "ui_tools.c"                                   /* T1: the track tools */
#include "ui_ov.c"                                      /* 2026-11-10: OVERVIEW */
