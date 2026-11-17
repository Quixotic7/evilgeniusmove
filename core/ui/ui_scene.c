/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: a confirmation as a scene (built through ui_turn.c). FIZZ's confirmation is a word over a black screen
 * for a second; here the word keeps its place and its second, and the room's cast plays what it means above it:
 *   ZAP     something was cleared: the robot charges, its bolt crosses the floor, the agent flashes to the bone and is
 *           a little heap of ash
 *   ARM     armed, the second press does it ("CLEAR?"): the sentry takes aim at him, its eye red, its arm sparking
 *   DENIED  no room, full, nothing to undo: the sentry fills the screen, the border flashing red
 *   STEAL   copied, stored: the agent searches the cabinet (the bar fills), the document comes out
 *   DROP    pasted, recalled, doubled: he carries the document to the terminal and it goes in
 *   WORD    anything else: the villain says it, at his desk
 * The scene is chosen from the words (ui_scene_kind), so every confirmation FIZZ's controller raises has one without
 * the controller knowing; THE VOICE (app.c, ctl.c confirm_cue) reads the same choice. t: seconds since it came up. */

static int sc_has(const char *s, const char *w) { return s && strstr(s, w) != 0; }

int ui_scene_kind(const char *big, const char *small)
{
    if (!big || !big[0])
        return UI_SC_WORD;
    if (big[strlen(big) - 1] == '?')
        return UI_SC_ARM;
    if (sc_has(big, "NO ROOM") || sc_has(big, "FULL") || sc_has(big, "NO UNDO") || sc_has(big, "ONE PAGE") ||
        sc_has(small, "too big"))
        return UI_SC_DENIED;
    if (sc_has(big, "CLEARED") || sc_has(big, "REMOVED") || sc_has(big, "RESET"))
        return UI_SC_ZAP;
    if (sc_has(small, "copied") || sc_has(small, "stored"))
        return UI_SC_STEAL;
    if (sc_has(small, "pasted") || sc_has(small, "recalled") || sc_has(big, "DOUBLED"))
        return UI_SC_DROP;
    return UI_SC_WORD;
}

#define SC_FLOOR 66                                     /* the scene's floor */

static void sc_floor(void)
{
    px_rect(8, SC_FLOOR, 104, 1, PX_LBLUE);
    px_rect(8, SC_FLOOR + 1, 104, 2, PX_BLUE);
    px_checker(8, SC_FLOOR + 1, 104, 1, PX_BLACK, 0);
}

static void sc_bolt(int x0, int x1, int y, int ph)      /* a bolt along the floor, x0 > x1 */
{
    int x = x0, k = ph, py = y;
    while (x > x1) {
        const int nx = x - 5 < x1 ? x1 : x - 5, ny = y + ((k & 1) ? 3 : -3) + ((k & 2) ? 1 : 0);
        px_line(x, py, nx, nx == x1 ? y : ny, (k + ph) & 1 ? PX_YELLOW : PX_WHITE);
        py = nx == x1 ? y : ny;
        x = nx;
        k++;
    }
}

static void sc_zap(float t)
{
    const int ph = (int)(t * 30.f);
    sc_floor();
    px_sprite_sc(78, SC_FLOOR - 24, t > 0.18f && t < 0.75f ? PXS_ROBOT2 : PXS_ROBOT1, 1, 2, QD_NONE);
    if (t < 0.25f) {                                    /* he sees it coming */
        px_sprite_sc(22, SC_FLOOR - 32, PXS_AGENT_STAND, 0, 2, QD_NONE);
        if (t > 0.1f)
            px_text(42, SC_FLOOR - 38, "!", PX_YELLOW, PX_FB, 1);
    } else if (t < 0.72f) {                             /* the bolt: bones and flash, turn about */
        sc_bolt(76, 40, SC_FLOOR - 16, ph);
        if (ph & 1)
            px_sprite_sc(22, SC_FLOOR - 32, PXS_AGENT_ZAP, 0, 2, QD_NONE);
        else
            px_sprite_sc(22, SC_FLOOR - 32, PXS_AGENT_STAND, 0, 2, PX_YELLOW);
        if ((ph & 3) == 0) {
            px_rect(0, 0, 120, 1, PX_WHITE), px_rect(0, 119, 120, 1, PX_WHITE);
            px_rect(0, 0, 1, 120, PX_WHITE), px_rect(119, 0, 1, 120, PX_WHITE);
        }
    } else {                                            /* ash, a wisp of smoke */
        const int up = (int)((t - 0.72f) * 30.f);
        px_rect(24, SC_FLOOR - 2, 14, 2, PX_GREY);
        px_rect(27, SC_FLOOR - 4, 8, 2, PX_LGREY);
        px_pset(30 + (up & 1), SC_FLOOR - 8 - up, PX_GREY);
        px_pset(32 - (up & 1), SC_FLOOR - 12 - up, PX_DGREY);
    }
}

static void sc_arm(float t)
{
    const int blink = (int)(t * 8.f) & 1;
    sc_floor();
    px_sprite_sc(70, SC_FLOOR - 42, blink ? PXS_SENTRY2 : PXS_SENTRY1, 0, 3, QD_NONE);
    px_sprite_sc(24, SC_FLOOR - 32, PXS_AGENT_STAND, 0, 2, QD_NONE);
    if (blink) {                                        /* its arm, charged */
        px_rect(66, SC_FLOOR - 18, 2, 2, PX_YELLOW);
        px_rect(62, SC_FLOOR - 21, 2, 2, PX_WHITE);
        px_rect(63, SC_FLOOR - 15, 2, 2, PX_YELLOW);
    }
    px_dots(44, SC_FLOOR - 17, 20, PX_LRED, 3);         /* its aim */
}

static void sc_denied(float t)
{
    const int blink = (int)(t * 10.f) & 1;
    px_sprite_sc(43, 12, blink ? PXS_SENTRY2 : PXS_SENTRY1, 0, 4, QD_NONE);
    if (blink) {
        px_box(0, 0, 120, 120, PX_LRED);
        px_box(1, 1, 118, 118, PX_RED);
    }
    px_line(14, 22, 30, 38, PX_LRED), px_line(15, 22, 31, 38, PX_LRED);   /* no */
    px_line(30, 22, 14, 38, PX_LRED), px_line(31, 22, 15, 38, PX_LRED);
    px_line(90, 22, 106, 38, PX_LRED), px_line(91, 22, 107, 38, PX_LRED);
    px_line(106, 22, 90, 38, PX_LRED), px_line(107, 22, 91, 38, PX_LRED);
}

static void sc_steal(float t)
{
    const float k = t / 0.55f > 1.f ? 1.f : t / 0.55f;
    sc_floor();
    px_sprite_sc(72, SC_FLOOR - 28, PXS_CABINET, 0, 2, QD_NONE);
    px_sprite_sc(48, SC_FLOOR - 32, PXS_AGENT_STAND, 0, 2, QD_NONE);
    px_box(62, SC_FLOOR - 40, 40, 6, PX_LGREY);         /* the search */
    px_rect(64, SC_FLOOR - 38, (int)(36.f * k), 2, PX_LGREEN);
    if (k >= 1.f) {                                     /* found: the document, up and out */
        const int up = (int)((t - 0.55f) * 40.f);
        px_sprite_sc(40, SC_FLOOR - 52 - (up > 8 ? 8 : up), PXS_DOC, 0, 2, QD_NONE);
        if ((int)(t * 12.f) & 1) {
            px_pset(37, SC_FLOOR - 50, PX_YELLOW), px_pset(57, SC_FLOOR - 46, PX_YELLOW);
            px_pset(47, SC_FLOOR - 62, PX_WHITE);
        }
    }
}

static void sc_drop(float t)
{
    const float k = t / 0.6f > 1.f ? 1.f : t / 0.6f;
    const int x = 30 + (int)(38.f * k), y = SC_FLOOR - 44 - (int)(56.f * k * (1.f - k)) + (int)(20.f * k);
    sc_floor();
    px_sprite_sc(74, SC_FLOOR - 24, PXS_TERMINAL, 0, 2, QD_NONE);
    px_sprite_sc(14, SC_FLOOR - 32, PXS_AGENT_STAND, 0, 2, QD_NONE);
    if (k < 1.f) {
        px_sprite(x, y, PXS_DOC, 0);
    } else if ((int)(t * 10.f) & 1) {                   /* in: the terminal takes it */
        px_rect(78, SC_FLOOR - 20, 20, 10, PX_LGREEN);
    }
}

static void sc_word(float t)
{
    const int talk = t < 0.8f && ((int)(t * 9.f) & 1);
    ui_genius(32, 13, talk, t);
    px_rect(20, 56, 80, 2, PX_GREY);                    /* his desk */
    px_rect(24, 58, 2, 6, PX_DGREY), px_rect(94, 58, 2, 6, PX_DGREY);
}

void ui_confirm_draw(const char *big, const char *small, qd_rgb col, float t)
{
    const int kind = ui_scene_kind(big, small);
    switch (kind) {
    case UI_SC_ZAP: sc_zap(t); break;
    case UI_SC_ARM: sc_arm(t); break;
    case UI_SC_DENIED: sc_denied(t); break;
    case UI_SC_STEAL: sc_steal(t); break;
    case UI_SC_DROP: sc_drop(t); break;
    default: sc_word(t); break;
    }
    if (kind == UI_SC_ARM || kind == UI_SC_DENIED)
        col = PX_LRED;
    if (big && big[0]) {
        const int sc = px_text_w(big, PX_FB, 2) <= 116 ? 2 : 1;
        px_text_sh(60 - (px_text_w(big, PX_FB, sc) + 1) / 2, sc == 2 ? 78 : 82, big, col, UI_G3, PX_FB, sc);
    }
    if (small && small[0] && !strchr(small, ' '))
        px_ctext(60, 100, small, col, PX_FS, 1);
}
