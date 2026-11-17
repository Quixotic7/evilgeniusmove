/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: THE ROOM, the Home view's picture (EDIT with nothing held, turned or picked; built through ui_held.c).
 * The page edited is a room of the villain's tower, drawn from the lane's model (ui_lane_t), over the page's row:
 *   the steps      sixteen columns, the row's own (the agent is over the cell that sounds)
 *   a note         a platform as high as its pitch and as long as its gate: yellow with the step's own notes, cyan
 *                  on the track's, a sound lock's in its sound's colour, white while it sounds; a chord's other notes thin lines under it; a chance or a
 *                  condition: a crumbling (dotted) platform; a ratchet: its hits as studs under it
 *   the agent      the playhead: he lands on each note as it sounds, runs its gate, somersaults the gaps (the longer
 *                  the gap the higher), and leaves by the floor at the page's end. Stopped, he waits by the lift
 *   a robot        a step with parameter locks (the patrol robot) or a trigless LOCK step (the sentry, on its skirt): it
 *                  stands under its step and fires as the step plays, the bolt up to the note; the agent caught on
 *                  it flashes to the bone. Robots side by side: every other one, the floor between them live
 *   the floor      a girder under the steps in the page's start..end; outside it a pit
 *   the lift       the shaft at the left: a floor a track, the car at the selected one (a change of track rides it,
 *                  the room sliding by); a track's number flashes green as it plays, red when it is not heard
 * A track that is not heard: the lights are out (greys), the robots asleep. */

#define RM_CEIL 12
#define RM_FLOOR 84                                     /* the floor's top */
#define RM_PTOP 36                                      /* the highest platform's top .. */
#define RM_PBOT 76                                      /* .. the lowest's */
#define RM_X0 9                                         /* the room's left (the shaft: 0..8) */

static int rm_x(int i) { return 10 + i * 6 + (i / 4) * 2; }   /* (ui_lane.c's ov_x: the row's cells) */

static int rm_py(const ui_lane_t *v, int note)
{
    const int span = v->span > 0 ? v->span : 16;
    const int y = RM_PBOT - (note - v->lo) * (RM_PBOT - RM_PTOP) / span;
    return y < RM_PTOP ? RM_PTOP : y > RM_PBOT ? RM_PBOT : y;
}

static int rm_top(const ui_lcol_t *c)
{
    int j, n = c->note[0];
    for (j = 1; j < c->nnote && j < 6; j++)
        n = c->note[j] > n ? c->note[j] : n;
    return n;
}

static int rm_noted(const ui_lcol_t *c) { return c->in_range && c->on && c->type != 2 && c->nnote > 0; }
static int rm_guard(const ui_lcol_t *c) { return c->in_range && c->on && (c->nplock || c->type == 2); }

/* a bolt from x, y0 up to y1 (y1 < y0), jagged, its shape by the frame */
static void rm_bolt(int x, int y0, int y1, int ph)
{
    int y = y0, k = ph;
    int px = x;
    while (y > y1) {
        const int ny = y - 4 < y1 ? y1 : y - 4;
        const int nx = x + ((k & 1) ? 2 : -2) + ((k & 2) ? 1 : 0);
        px_line(px, y, ny == y1 ? x : nx, ny, (k + ph) & 1 ? PX_YELLOW : PX_WHITE);
        px = ny == y1 ? x : nx;
        y = ny;
        k++;
    }
}

typedef struct {
    float at, len;
    int x, y, i;
} rm_stand_t;

/* the lift's shaft: the tracks' floors, the car */
static void rm_shaft(const ui_lane_t *v)
{
    const int n = UI_NTRACK, pitch = 66 / (n > 1 ? n : 1), sel = v->track - 1;
    int k, cy;
    px_vdots(0, RM_CEIL + 1, RM_FLOOR + 4 - RM_CEIL, UI_G2, 2);
    px_vdots(8, RM_CEIL + 1, RM_FLOOR + 4 - RM_CEIL, UI_G2, 2);
    cy = 18 + sel * pitch;
    if (v->ride_from >= 0 && v->ride < 1.f)
        cy = 18 + v->ride_from * pitch + (int)((float)((sel - v->ride_from) * pitch) * v->ride);
    for (k = 0; k < n; k++) {
        const int y = 18 + k * pitch, mute = (v->trk_mute >> k) & 1;
        const int hot = v->trk_flash[k] >= 0.f && v->trk_flash[k] < 0.12f;
        char d[2] = {(char)('1' + k), 0};
        if (k == sel)
            continue;
        px_text(3, y, d, hot && !mute ? PX_LGREEN : mute ? PX_RED : PX_DGREY, PX_FS, 1);
        px_hline(1, y + 7, 2, PX_DGREY);
        px_hline(6, y + 7, 2, PX_DGREY);
    }
    {
        char d[2] = {(char)('1' + sel), 0};
        const int mute = (v->trk_mute >> sel) & 1;
        px_rect(1, cy - 2, 7, 9, PX_BLACK);
        px_text(3, cy, d, mute ? PX_LRED : PX_WHITE, PX_FS, 1);
        px_rect(1, cy + 6, 7, 1, PX_YELLOW);
        px_rect(1, cy + 7, 7, 1, PX_ORANGE);
        px_pset(2, cy + 7, PX_BLACK), px_pset(4, cy + 7, PX_BLACK), px_pset(6, cy + 7, PX_BLACK);
    }
}

static void rm_floor(const ui_lane_t *v, int dark)
{
    int i;
    for (i = 0; i < 16; i++) {
        const int x = i == 0 ? RM_X0 : rm_x(i), x1 = i == 15 ? 116 : rm_x(i + 1);
        if (v->col[i].in_range) {
            px_rect(x, RM_FLOOR, x1 - x, 1, dark ? PX_GREY : PX_LBLUE);
            px_rect(x, RM_FLOOR + 1, x1 - x, 2, dark ? PX_DGREY : PX_BLUE);
            px_checker(x, RM_FLOOR + 1, x1 - x, 1, PX_BLACK, 0);
        } else {                                        /* outside the page's range: a pit */
            px_pset(x + 2, RM_FLOOR + 2, UI_G2);
        }
    }
}

static const qd_rgb RM_SND[5][2] = {{PX_LRED, PX_RED}, {PX_LGREEN, PX_GREEN}, {PX_PURPLE, PX_BLUE},
                                    {PX_ORANGE, PX_BROWN}, {PX_LBLUE, PX_BLUE}};
static void rm_platforms(const ui_lane_t *v, int dark)
{
    const int pg = v->page >= 0 && v->page < 8 ? v->page : 0;
    int i, j;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        int x0, w, y, top, nr;
        qd_rgb hi, lo;
        if (!rm_noted(c))
            continue;
        x0 = rm_x(i) + c->micro * 6 / 24;
        w = (int)(c->len * 6.f) - 1;
        if (w > 116 - x0)
            w = 116 - x0;
        if (w < 5)
            w = 5;
        top = rm_top(c);
        y = rm_py(v, top);
        if (c->lit)
            hi = PX_WHITE, lo = PX_LGREY;
        else if (dark)
            hi = PX_GREY, lo = PX_DGREY;
        else if (c->sound)                              /* a sound lock: the sound's own colour */
            hi = RM_SND[c->sound % 5][0], lo = RM_SND[c->sound % 5][1];
        else if (v->cell[pg][i] & UI_C_OWN)
            hi = PX_YELLOW, lo = PX_ORANGE;
        else
            hi = PX_CYAN, lo = PX_BLUE;
        for (j = 0; j < c->nnote && j < 6; j++)         /* a chord's other notes */
            if (c->note[j] != top)
                px_hline(x0, rm_py(v, c->note[j]), w > 5 ? 5 : w, lo);
        if (c->chance < 100 || c->cond[0]) {            /* it may not play: crumbling */
            px_dots(x0, y, w, hi, 2);
            px_dots(x0 + 1, y + 1, w - 1, lo, 2);
        } else {
            px_rect(x0, y, w, 1, hi);
            px_rect(x0, y + 1, w, 1, lo);
        }
        nr = c->ratch > 1 ? c->ratch : 0;
        for (j = 0; j < nr && j < 8; j++)               /* the ratchet's hits */
            px_pset(x0 + j * 2 % (w > 1 ? w : 1), y + 3 + (j * 2 / (w > 1 ? w : 1)), hi);
        if (c->held)
            px_box(x0 - 1, y - 1, w + 2, 4, PX_WHITE);
        if (c->func)                                    /* a step function: a sign on the floor */
            px_rect(rm_x(i) + 1, RM_FLOOR - 3, 3, 3, UI_MODE_COL[UI_F_FUNC]);
    }
}

static void rm_robots(const ui_lane_t *v, int dark, int agent_x)
{
    int i, run = 0;
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        const int cx = rm_x(i) + 2;
        if (!rm_guard(c)) {
            run = 0;
            continue;
        }
        if (run++ & 1) {                                /* between two robots: the floor live */
            px_dots(rm_x(i), RM_FLOOR, 6, c->lit ? PX_WHITE : PX_YELLOW, 2);
            if (c->lit)
                rm_bolt(cx, RM_FLOOR - 1, rm_noted(c) ? rm_py(v, rm_top(c)) + 2 : RM_FLOOR - 14, (int)(v->t * 24.f));
            continue;
        }
        {
            const int sentry = c->type == 2;        /* a trigless lock: the sentry */
            const int id = sentry ? (c->lit ? PXS_SENTRY2 : PXS_SENTRY1) : (c->lit ? PXS_ROBOT2 : PXS_ROBOT1);
            const int h = px_sprite_h(id), w = px_sprite_w(id);
            const int sway = v->playing || dark ? 0 : (int)(fz_sinf(v->t * 1.3f + (float)i) * 1.4f);
            const int x = cx - w / 2 + sway;
            if (dark) {
                px_sprite_tint(x, RM_FLOOR - h, id, 0, PX_DGREY);
                px_text(x + w - 2, RM_FLOOR - h - 6, "Z", PX_GREY, PX_FS, 1);
            } else {
                px_sprite(x, RM_FLOOR - h, id, sentry ? agent_x > cx : agent_x < cx);
            }
            if (c->lit && !dark)
                rm_bolt(cx, RM_FLOOR - h - 1, rm_noted(c) ? rm_py(v, rm_top(c)) + 2 : RM_CEIL + 4, (int)(v->t * 24.f));
        }
    }
}

/* where the agent is at pos (steps into the page): his left, his feet; the sprite; 1: on a guarded step, caught */
static int rm_agent(const ui_lane_t *v, float pos, int *ax, int *ay, int *spr)
{
    static const int RUN[4] = {PXS_AGENT_RUN1, PXS_AGENT_RUN2, PXS_AGENT_RUN3, PXS_AGENT_RUN2};
    rm_stand_t st[18], a, b;
    int n = 0, i, k = -1;
    float hold, t0;
    st[n].at = -0.01f, st[n].len = 0.f, st[n].x = RM_X0 + 1, st[n].y = RM_FLOOR, st[n].i = -1, n++;   /* the door in */
    for (i = 0; i < 16; i++) {
        const ui_lcol_t *c = &v->col[i];
        if (!rm_noted(c))
            continue;
        st[n].at = (float)i + (float)c->micro / 24.f;
        st[n].len = c->len;
        st[n].x = rm_x(i) - 2 + c->micro * 6 / 24;
        st[n].y = rm_py(v, rm_top(c));
        st[n].i = i;
        n++;
    }
    st[n].at = 16.f, st[n].len = 0.f, st[n].x = 110, st[n].y = RM_FLOOR, st[n].i = -1, n++;            /* .. and out */
    if (n == 2) {                                       /* an empty room: he runs the floor */
        *ax = RM_X0 + 1 + (int)(pos * 6.2f);
        *ay = RM_FLOOR;
        *spr = RUN[(int)(pos * 4.f) & 3];
        return 0;
    }
    for (i = 0; i < n; i++)
        if (st[i].at <= pos)
            k = i;
    if (k < 0)
        k = 0;
    if (k > n - 2)
        k = n - 2;
    a = st[k], b = st[k + 1];
    hold = a.len < b.at - a.at - 0.5f ? a.len : b.at - a.at - 0.5f;
    if (hold < 0.f || a.i < 0)
        hold = 0.f;
    t0 = a.at + hold;
    if (pos < t0) {                                     /* on the platform: running its gate */
        *ax = a.x + (int)((pos - a.at) * 6.f);
        *ay = a.y;
        *spr = RUN[(int)(pos * 4.f) & 3];
    } else {                                            /* the leap to the next */
        const float gap = b.at - t0 > 0.05f ? b.at - t0 : 0.05f;
        const float u = (pos - t0) / gap > 1.f ? 1.f : (pos - t0) / gap;
        const float x0 = (float)a.x + hold * 6.f;
        float J = 4.f + 3.f * gap;
        if (J > 15.f)
            J = 15.f;
        *ax = (int)(x0 + ((float)b.x - x0) * u + 0.5f);
        *ay = (int)((float)a.y + (float)(b.y - a.y) * u - J * 4.f * u * (1.f - u) + 0.5f);
        *spr = gap >= 0.9f && u > 0.2f && u < 0.8f ? PXS_AGENT_FLIP : u < 0.5f ? PXS_AGENT_RUN1 : PXS_AGENT_RUN3;
    }
    return a.i >= 0 && rm_guard(&v->col[a.i]) && pos - a.at < 0.6f;
}

void ui_room_draw(const ui_lane_t *v)
{
    const int mode = v->mode >= 0 && v->mode < UI_NFIELD ? v->mode : UI_F_NOTE;
    const int sel = v->track - 1, dark = sel >= 0 && ((v->trk_mute >> sel) & 1);
    int ax = RM_X0 + 2, ay = RM_FLOOR, spr = PXS_AGENT_STAND, zap = 0, here = 1, flip = 0;
    int mx, dy = 0;

    ui_head(v->name, v->pattern, v->track, UI_K3, UI_G1);
    mx = (int)(ui_head_x(v->track) * 0.5f) + px_text_w(v->name, PX_FS, 1) + 5;
    mx = px_text(mx, 4, UI_FIELD_NAME[mode], UI_MODE_COL[mode], PX_FS, 1);
    if (dark)
        px_text(mx + 4, 4, "MUTE", PX_LRED, PX_FS, 1);
    px_dots(RM_X0, RM_CEIL, 120 - RM_X0, UI_G3, 2);
    if (mode != UI_F_NOTE && v->def_label[0])           /* the mode's default, as the lanes name it */
        px_rtext(117, RM_CEIL + 3, v->def_label, v->def_hot ? UI_MODE_COL[mode] : UI_G1, PX_FS, 1);

    if (v->playing) {
        if (v->pos >= 0.f)
            zap = rm_agent(v, v->pos, &ax, &ay, &spr);
        else
            here = 0;                                   /* (he is in another page's room) */
    } else {
        flip = fz_fmodf(v->t, 5.f) > 4.f;               /* waiting: a look over his shoulder now and then */
    }

    if (v->ride_from >= 0 && v->ride < 1.f)             /* the lift's ride: the room slides by */
        dy = (int)((1.f - v->ride) * 78.f) * (sel > v->ride_from ? 1 : -1);
    qd_save();
    qd_clip_rect((float)(2 * RM_X0), (float)(2 * (RM_CEIL + 1)), (float)(2 * (120 - RM_X0)),
                 (float)(2 * (RM_FLOOR + 5 - RM_CEIL - 1)));
    if (dy || v->slide != 0.f)                          /* (a page change: the next room slides in from its side) */
        qd_translate((float)(2 * (int)(v->slide * 40.f)), (float)(2 * dy));
    rm_floor(v, dark);
    rm_platforms(v, dark);
    rm_robots(v, dark, ax + 4);
    if (!here && v->play_page >= 0) {
        char s[8] = "PAGE ";
        s[5] = (char)('1' + (v->play_page & 7));
        s[6] = 0;
        px_rtext(116, RM_CEIL + (mode != UI_F_NOTE && v->def_label[0] ? 10 : 3), s, PX_LRED, PX_FS, 1);
    }
    if (here) {
        const int w = px_sprite_w(spr), h = px_sprite_h(spr);
        const int x = ax + (9 - w) / 2, y = ay - h - (spr == PXS_AGENT_FLIP ? 4 : 0);
        if (zap && ((int)(v->t * 30.f) & 1))
            px_sprite(ax, ay - 16, PXS_AGENT_ZAP, 0);
        else if (zap)
            px_sprite_tint(x, y, spr, 0, PX_YELLOW);
        else if (dark)
            px_sprite_tint(x, y, spr, flip, PX_GREY);
        else
            px_sprite(x, y, spr, flip);
    }
    qd_restore();
    rm_shaft(v);
}
