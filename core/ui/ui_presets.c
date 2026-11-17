/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the library browser, the step's pool picker and the LOADED / BACK / SAVED takeovers (FIZZ's P1.5t,
 * redrawn at 120 x 120; ui.h ui_browse_t, ui_pfx_draw). Built through ui_layer.c after ui_lib.c (its lb_* helpers).
 * The list is a disk's directory: the neighbours above and below as quoted names (the C64's listing), the chosen one
 * as a card: its floppy (the label in its kind's colour, the hub turning while it sounds), its name, its amp envelope
 * (lit along while it sounds), its tags as dots, its sends as three bars, a star (one of MY presets) or an arrow
 * (imported), the pool's tray (in the pool; or empty with a +: B10 adds it). Where in the list: a bar at the right.
 * DELETE? / OUT?: the head red, a red X across the card, it shakes; who uses it along the foot. */

const qd_rgb UI_PK_COL[7] = {UI_K1, UI_K3, 0x75cec8u, UI_PAD_COL, UI_K2, UI_K4, 0xc46c71u};
#define PR_ORANGE UI_K4

static qd_rgb pr_kind_col(int k) { return k >= 0 && k < 7 ? UI_PK_COL[k] : UI_G1; }
static const char *pr_kind_name(int k)                  /* (sounds.h QD_PK_*'s) */
{
    static const char *const N[7] = {"BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"};
    return k >= 0 && k < 7 ? N[k] : "";
}

/* the pool: a little tray, 9 x 7: full (four slots) or empty with a + */
static void pr_tray(int x, int y, qd_rgb col, int full)
{
    px_box(x, y, 9, 7, col);
    if (full) {
        px_rect(x + 2, y + 2, 2, 1, col);
        px_rect(x + 5, y + 2, 2, 1, col);
        px_rect(x + 2, y + 4, 2, 1, col);
        px_rect(x + 5, y + 4, 2, 1, col);
    } else {
        px_hline(x + 3, y + 3, 3, col);
        px_vline(x + 4, y + 2, 3, col);
    }
}

/* a preset's tags as 2 x 2 dots in their kinds' colours from x; their width */
static int pr_tag_dots(int x, int y, int tags)
{
    int k, i = 0;
    for (k = 0; k < 7; k++)
        if ((tags >> k) & 1) {
            px_rect(x + i * 4, y, 2, 2, pr_kind_col(k));
            i++;
        }
    return i * 4;
}

/* a star (one of MY presets), an arrow down (imported): 5 x 5 */
static void pr_mark(int x, int y, int imported, qd_rgb col)
{
    static const char *const STAR[5] = {"..#..", "#####", ".###.", ".#.#.", "#...#"};
    static const char *const ARROW[5] = {"..#..", "..#..", "#.#.#", ".###.", "..#.."};
    const char *const *b = imported ? ARROW : STAR;
    int r, c;
    for (r = 0; r < 5; r++)
        for (c = 0; c < 5; c++)
            if (b[r][c] == '#')
                px_pset(x + c, y + r, col);
}

/* the head: TITLE, a chip (the kind filter), "7/31", the track's box */
static void pr_head(const char *title, qd_rgb col, const char *right, int track, int chip)
{
    int x = px_text(4, 3, title, col, PX_FS, 1) + 3;
    if (chip > 0) {
        const char *nm = pr_kind_name(chip - 1);
        const qd_rgb cc = pr_kind_col(chip - 1);
        px_box(x, 1, px_text_w(nm, PX_FS, 1) + 4, 9, cc);
        px_text(x + 2, 3, nm, cc, PX_FS, 1);
    }
    if (track)
        ui_box(216.f, 4.f, track, UI_TRACK_PAL[(track - 1) & 7]);
    if (right && right[0])
        px_rtext(track ? 104 : 116, 3, right, UI_G1, PX_FS, 1);
    px_dots(0, 11, 120, UI_G3, 2);
}

#define PR_CW 110                                       /* the card */
#define PR_CH 40

/* a preset as a card at (x, y): its floppy, name, envelope, marks; none: a dashed card with a label */
static void pr_card(int x, int y, const ui_preset_t *p, qd_rgb col, float t, int dashed, const char *label,
                    float sound)
{
    if (dashed)
        lb_dashbox(x, y, PR_CW, PR_CH, col);
    else {
        px_rect(x, y, PR_CW, PR_CH, UI_BG);
        px_box(x, y, PR_CW, PR_CH, col);
    }
    if (!p || !p->ok || p->track) {                     /* (none: TRACK, the step plays the track's sound) */
        px_ctext(x + PR_CW / 2, y + PR_CH / 2 - 4, label ? label : "TRACK", col, PX_FB, 1);
        return;
    }
    lb_floppy(x + 4, y + 5, 30, p->missing ? UI_G2 : pr_kind_col(p->kind), sound > 0.f ? t * 9.f : -1.f);
    lb_name(x + 38, y + 4, PR_CW - 50, p->name ? p->name : "", p->missing ? UI_G1 : UI_K3, 0);
    lb_env(x + 38, y + 15, 36, 10, p->env, p->missing ? UI_G2 : pr_kind_col(p->kind), sound, UI_K3);
    if (p->tags)
        pr_tag_dots(x + 38, y + 32, p->tags);
    {   /* its sends, chorus / echo / room: three bars */
        static const qd_rgb SC[3] = {UI_K1, UI_K2, UI_K3};
        int j;
        for (j = 0; j < 3; j++) {
            const int s = p->send[j] < 0 ? 0 : p->send[j] > 127 ? 127 : p->send[j], hh = 1 + s * 11 / 127;
            px_rect(x + 80 + j * 3, y + 25 - hh, 2, hh, p->missing ? UI_G2 : SC[j]);
        }
    }
    if (p->imported || p->mine)
        pr_mark(x + PR_CW - 9, y + 4, p->imported, UI_LIB_COL);
    if (p->tray == 1)                                   /* B10 adds it */
        pr_tray(x + PR_CW - 13, y + PR_CH - 11, UI_POOL_COL, 0);
    else if (p->tray == 2 || p->in_pool)                /* .. B10 takes it out */
        pr_tray(x + PR_CW - 13, y + PR_CH - 11, UI_POOL_COL, 1);
}

/* armed (DELETE?, OUT?): a big red X across the card */
static void pr_red_x(int x, int y)
{
    px_line(x + 2, y + 2, x + PR_CW - 3, y + PR_CH - 3, UI_RED);
    px_line(x + 3, y + 2, x + PR_CW - 2, y + PR_CH - 3, UI_RED);
    px_line(x + PR_CW - 3, y + 2, x + 2, y + PR_CH - 3, UI_RED);
    px_line(x + PR_CW - 2, y + 2, x + 3, y + PR_CH - 3, UI_RED);
}

/* the list as a directory: two neighbours above and below, the chosen as a card, where in the list at the right */
static void pr_rollo(const ui_browse_t *v, qd_rgb col)
{
    static const int D[4] = {-2, -1, 1, 2};
    const int dy = v->roll > 0.f ? lb_r(lb_ease(v->roll) * 5.f) * (v->dir < 0 ? -1 : 1) : 0;
    int i;
    for (i = 0; i < 4; i++) {
        const int d = D[i];
        const ui_preset_t *p = &v->e[2 + d];
        const int y = (d < 0 ? 28 + (d + 1) * 10 : 85 + (d - 1) * 10) + dy;
        char nm[24];
        if (!p->ok || (v->users_row && d == 2))
            continue;                                   /* (the POOL, OUT?: its users' row there) */
        if (d > 0 && v->fx && v->fx_u < 1.f && !v->picker)
            continue;                                   /* (+ / - POOL's animation and its count there) */
        if (y < 13 || y > 102)
            continue;
        nm[0] = 0;
        if (p->track)
            cat(nm, "TRACK");
        else
            cat(cat(cat(nm, "\""), p->name ? p->name : ""), "\"");
        if (!p->track && p->tags)
            pr_tag_dots(6, y + 2, p->tags & -p->tags);   /* (its first tag's dot) */
        else if (!p->track)
            px_rect(6, y + 2, 2, 2, pr_kind_col(p->kind));
        px_text(12, y, nm, p->missing ? UI_G2 : d * d == 1 ? UI_G1 : UI_G2, PX_FS, 1);
        if (p->in_pool && !v->pool && !v->picker)
            pr_tray(100, y - 1, d * d == 1 ? UI_POOL_COL : UI_G2, 1);
    }
    {
        const int armed = v->out_armed || v->del_armed;
        const int sx = armed ? lb_r(sinf(v->arm_t * 48.f) * (v->arm_t < 0.45f ? 3.f : 1.f)) : 0;
        pr_card(3 + sx, 40 + dy / 2, v->e[2].ok ? &v->e[2] : 0, col, v->t, !v->e[2].ok || v->e[2].track, "TRACK",
                v->sound);
        if (armed)
            pr_red_x(3 + sx, 40 + dy / 2);
    }
    {
        const int n = v->n + (v->picker ? 1 : 0), at = v->pos;
        const int yy = 16 + 82 * at / (n > 1 ? n - 1 : 1);
        px_vline(117, 14, 90, UI_G3);
        if (n > 0)
            px_rect(116, yy - 3 < 14 ? 14 : yy - 3, 3, 7, col);
    }
}

/* the users along the foot: the tracks' boxes, the steps (x n), -> TRACK when OUT? / DELETE? */
static int pr_times(int x, int n, qd_rgb col)
{
    char s[12] = "\xc3\x97";
    int w;
    ui_itoa(s + 2, n);
    w = px_text_w(s, PX_FS, 1) + 5;
    px_box(x, 107, w, 9, col);
    px_text(x + 3, 109, s, col, PX_FS, 1);
    return x + w + 3;
}
static void pr_to_track(int x)
{
    px_hline(x, 111, 7, UI_RED);
    px_pset(x + 5, 110, UI_RED);
    px_pset(x + 5, 112, UI_RED);
    px_text(x + 10, 109, "TRACK", UI_RED, PX_FS, 1);
}

void ui_browse_draw(const ui_browse_t *v)
{
    char right[16];
    right[0] = 0;
    if (v->picker) {                                    /* the step's pool picker */
        px_text(4, 3, v->title, UI_K3, PX_FS, 1);
        px_dots(0, 11, 120, UI_G3, 2);
        px_box(0, 0, 120, 120, UI_STEP_FRAME);
        px_box(1, 1, 118, 118, UI_STEP_FRAME);
        if (!v->n) {                                    /* the pool empty: TRACK only */
            pr_card(5, 40, 0, UI_POOL_COL, v->t, 1, "TRACK", 0.f);
            pr_tray(46, 98, UI_POOL_COL, 0);
            px_text(58, 99, "0/128", UI_POOL_COL, PX_FS, 1);
            return;
        }
        pr_rollo(v, UI_POOL_COL);
        return;
    }
    if (v->n) {
        char b[8];
        ui_itoa(right, v->pos + 1);
        strcat(right, "/");
        strcat(right, ui_itoa(b, v->pool ? 128 : v->n));
    }
    if (v->del_armed || v->out_armed)                   /* the head red: DELETE? / OUT? */
        pr_head(v->del_armed ? "DELETE?" : "OUT?", UI_RED, right, v->track, v->chip);
    else
        pr_head(v->title, v->tcol, right, v->track, v->chip);
    if (!v->n) {                                        /* nothing in it */
        pr_card(3, 40, 0, v->col, v->t, 1, "\xe2\x80\x94", 0.f);
        return;
    }
    pr_rollo(v, v->out_armed || v->del_armed ? UI_RED : v->ocol ? v->ocol : v->col);   /* (lime: in the pool) */
    if (v->users_row && v->del_armed) {                 /* DELETE?: the projects referencing it, its steps */
        char n[12];
        int x = 6;
        px_box(x + 2, 107, 8, 7, UI_RED);               /* (a project: a little stack) */
        px_rect(x, 109, 8, 7, UI_BG);
        px_box(x, 109, 8, 7, UI_RED);
        px_text(x + 12, 109, ui_itoa(n, v->use_proj), UI_RED, PX_FS, 1);
        x += 12 + px_text_w(n, PX_FS, 1) + 5;
        if (v->use_steps) {
            x = pr_times(x, v->use_steps, UI_RED);
            pr_to_track(x);
        }
    } else if (v->users_row) {                          /* who uses it (OUT?: they go back to TRACK) */
        const qd_rgb uc = v->out_armed ? UI_RED : UI_G1;
        int x = v->out_armed ? 4 : 24, t;
        for (t = 0; t < UI_NTRACK; t++)
            if ((v->users >> t) & 1u) {
                ui_box((float)(2 * x), 214.f, t + 1, v->out_armed ? UI_RED : UI_TRACK_PAL[t]);
                x += 10;
            }
        if (v->usteps)
            x = pr_times(x + 1, v->usteps, uc);
        if (v->out_armed)
            pr_to_track(x);
    }
    if (v->tag_u < 1.f) {                               /* a tag toggled: its chip pops (on: filled; off: struck) */
        const char *nm = pr_kind_name(v->tag_k);
        const qd_rgb cc = pr_kind_col(v->tag_k);
        const int w = px_text_w(nm, PX_FS, 1) + 6, y = 82 - lb_r(lb_ease(v->tag_u) * 3.f);
        if (v->tag_u < 0.85f || (int)(v->tag_u * 20.f) % 2 == 0) {
            px_rect(60 - w / 2, y, w, 9, v->tag_on ? cc : UI_BG);
            px_box(60 - w / 2, y, w, 9, cc);
            px_ctext(60, y + 2, nm, v->tag_on ? UI_BG : cc, PX_FS, 1);
            if (!v->tag_on)
                px_hline(60 - w / 2 + 2, y + 4, w - 4, cc);
        }
    }
    if (v->fx == 5 && v->fx_u < 1.f) {                  /* deleted: the card breaks up, its pieces fly off red */
        const float u = v->fx_u, f = lb_ease(u);
        int k;
        px_rect(3, 40, PR_CW, PR_CH, UI_BG);
        for (k = 0; k < 14; k++) {
            const float px0 = 6.f + lb_hash(k) * 100.f, py0 = 42.f + lb_hash(k + 20) * 34.f;
            const float vx = (px0 - 60.f) * 0.9f, vy = (py0 - 60.f) * 0.9f - 15.f;
            const int x = lb_r(px0 + vx * f), y = lb_r(py0 + vy * f + 30.f * f * f), s = 3 + (int)(5.f * lb_hash(k + 40));
            if (f < 0.85f || k % 2)
                px_box(x, y, s, s * 2 / 3 + 1, UI_RED);
        }
        for (k = 0; k < 12; k++)                        /* .. and sparks */
            if (f < 0.9f)
                lb_spark(lb_r(8.f + lb_hash(k + 60) * 104.f), lb_r(76.f - f * (20.f + 30.f * lb_hash(k + 80))),
                         k % 3 == 0, UI_RED);
        if (u < 0.3f && v->gone.name)                   /* (its name, a moment) */
            px_ctext(60, 56, v->gone.name, UI_RED, PX_FB, 1);
    }
    if (v->fx && v->fx_u < 1.f) {                       /* + POOL */
        const float u = v->fx_u;
        char n[12];
        ui_itoa(n, v->pool_n);
        strcat(n, "/128");
        if (v->fx == 1) {                               /* in: the card shrinks into the tray, sparks */
            pr_tray(48, 102, UI_POOL_COL, 1);
            px_text(60, 103, n, UI_POOL_COL, PX_FS, 1);
            if (u < 0.6f) {
                const float w = lb_ease(u / 0.6f), s = 1.f - 0.92f * w;
                const int cx = lb_r(58.f - 6.f * w), cy = lb_r(60.f + 45.f * w);
                const int ww = lb_r((float)PR_CW * s), hh = lb_r((float)PR_CH * s);
                px_box(cx - ww / 2, cy - hh / 2, ww > 2 ? ww : 2, hh > 2 ? hh : 2, UI_POOL_COL);
            } else {
                const float f = (u - 0.6f) / 0.4f;
                int k;
                for (k = 0; k < 6; k++)
                    lb_spark(48 + (int)(lb_hash(k) * 10.f), lb_r(100.f - f * 14.f * (0.5f + lb_hash(k + 3))), 0,
                             UI_POOL_COL);
            }
        } else if (v->fx == 2) {                        /* there already: the tray blinks */
            if ((int)(u * 8.f) % 2 == 0)
                pr_tray(48, 102, UI_POOL_COL, 1);
            px_text(60, 103, n, UI_POOL_COL, PX_FS, 1);
        } else if (v->fx == 3) {                        /* full: the tray flashes orange */
            const int on = (int)(u * 6.f) % 2 == 0;
            pr_tray(48, 102, on ? PR_ORANGE : UI_POOL_COL, 1);
            px_text(60, 103, n, on ? PR_ORANGE : UI_POOL_COL, PX_FS, 1);
        } else if (v->fx == 4) {                        /* out: the card pops back out of the tray, the + comes back */
            pr_tray(48, 102, UI_POOL_COL, u < 0.55f);
            px_text(60, 103, n, UI_POOL_COL, PX_FS, 1);
            if (u < 0.45f) {
                const float w = lb_ease(u / 0.45f), s = 0.1f + 0.9f * w;
                const int cx = lb_r(52.f + 6.f * w), cy = lb_r(104.f - 44.f * w);
                const int ww = lb_r((float)PR_CW * s), hh = lb_r((float)PR_CH * s);
                px_box(cx - ww / 2, cy - hh / 2, ww > 2 ? ww : 2, hh > 2 ? hh : 2, UI_POOL_COL);
            }
            if (u > 0.12f && u < 0.85f) {               /* (sparks out of the tray, up and away) */
                const float f = (u - 0.12f) / 0.73f;
                int k;
                for (k = 0; k < 10; k++) {
                    const float an = -UI_PI * (0.1f + 0.8f * lb_hash(k + 11)), sp = 15.f + 25.f * lb_hash(k + 23);
                    lb_spark(lb_r(52.f + cosf(an) * sp * f), lb_r(104.f + sinf(an) * sp * f - 5.f * f * f), f < 0.4f,
                             UI_POOL_COL);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------- LOADED, BACK, SAVED */
FZ_COLD void ui_pfx_draw(int kind, const ui_preset_t *s, const ui_preset_t *own, int track, float u, float t)
{
    const qd_rgb tc = UI_TRACK_PAL[(track - 1) & 7];
    px_rect(0, 0, 120, 120, UI_BG);
    if (kind == UI_PFX_LOADED) {                        /* the card flies into the track's box, its name big */
        const float v = lb_ease(u / 0.55f);
        pr_head("PRESETS", UI_LIB_COL, "", track, 0);
        if (u < 0.55f) {
            const float sc = 1.f - 0.9f * v;
            const int x = lb_r(5.f + (108.f - 5.f) * v), y = lb_r(40.f + (2.f - 40.f) * v);
            if (sc > 0.6f)
                pr_card(x, y, s, tc, t, 0, 0, 0.f);
            else
                px_box(x, y, lb_r((float)PR_CW * sc) > 2 ? lb_r((float)PR_CW * sc) : 2,
                       lb_r((float)PR_CH * sc) > 2 ? lb_r((float)PR_CH * sc) : 2, tc);
        } else {
            const char *nm = s && s->name ? s->name : "";
            if (u < 0.8f)
                px_box(106, 1, 12, 10, tc);
            lb_floppy(48, 30, 24, s ? pr_kind_col(s->kind) : tc, t * 9.f);
            lb_name(4, 62, 112, nm, tc, 1);
            px_ctext(60, 76, "LOADED", UI_G1, PX_FS, 1);
        }
        return;
    }
    if (kind == UI_PFX_BACK) {                          /* the one browsed slides out, the track's own back in */
        const float v = lb_ease(u / 0.5f);
        pr_head("PRESETS", UI_LIB_COL, "", track, 0);
        if (s && s->ok && v < 1.f)
            pr_card(lb_r(5.f - 130.f * v), 40, s, UI_LIB_COL, t, 0, 0, 0.f);
        pr_card(lb_r(130.f + (5.f - 130.f) * v), 40, own, tc, t, 0, 0, 0.f);
        if (u > 0.5f)
            px_ctext(60, 90, "BACK", UI_G1, PX_FS, 1);
        return;
    }
    {                                                   /* SAVED: into MY PRESETS, with sparks */
        const float v = lb_ease(u / 0.5f);
        pr_head("MY PRESETS", UI_LIB_COL, "", track, 0);
        pr_card(5, lb_r(24.f + 16.f * v), s, UI_LIB_COL, t, 0, 0, 0.f);
        if (u > 0.5f) {
            const float f = (u - 0.5f) / 0.5f;
            int k;
            for (k = 0; k < 10; k++)
                lb_spark(lb_r(10.f + lb_hash(k) * 100.f), lb_r(40.f - f * 20.f * (0.5f + lb_hash(k + 2))), k % 3 == 0,
                         UI_LIB_COL);
            px_ctext(60, 92, "SAVED", UI_LIB_COL, PX_FB, 1);
        }
    }
}
