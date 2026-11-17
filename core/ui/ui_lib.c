/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: the project pages and the library's shared pictures (FIZZ's P1.5a, redrawn at 120 x 120). Built through
 * ui_layer.c (it includes this file); ui_presets.c and ui_type.c use the lb_* helpers.
 *   the library's things are floppies: a 5.25" disk, its label in the owner's colour, its hub turning while it plays
 *   THE PROJECT: its number big and its name, the 64 patterns as a grid (A .. D a row; the one worked on filled, used
 *   ones boxed, sparks off one just written), the flash and the pool in use as meters
 *   THE PROJECTS: the eight (four) as floppies, their patterns in use under each, their names
 *   IMPORTED: a sound arriving over SysEx, its floppy dropping in */

static float lb_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
#define lb_ease ui_ease
static float lb_hash(int n)
{
    uint32_t h = (uint32_t)n * 0x9E3779B1u + 0x7F4A7C15u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    return (float)(h >> 8) / 16777216.f;
}
static int lb_r(float v) { return (int)floorf(v + 0.5f); }

/* a 5.25" floppy, s x s: its label (lcol), the hub (a notch turning at angle an; an < 0: none), the read slot */
static void lb_floppy(int x, int y, int s, qd_rgb lcol, float an)
{
    const int cx = x + s / 2, cy = y + s / 2 + 1, hr = s / 7 > 2 ? s / 7 : 2;
    px_rect(x, y, s, s, PX_DGREY);
    px_rect(x + 2, y + 2, s - 4, s / 4 > 3 ? s / 4 : 3, lcol);
    px_circle(cx, cy, hr + 1, PX_BLACK, PX_BLACK);
    px_circle(cx, cy, hr - 1 > 0 ? hr - 1 : 1, PX_GREY, QD_NONE);
    px_rect(cx - 1, cy + hr + 3, 3, s - (cy - y) - hr - 5 > 2 ? s - (cy - y) - hr - 5 : 2, PX_BLACK);   /* the slot */
    px_pset(cx + hr + 3, cy - 1, PX_BLACK);             /* the index hole */
    px_rect(x + s - 1, y + s / 4 + 3, 1, 3, PX_BLACK);  /* the notch */
    if (an >= 0.f)
        px_pset(cx + lb_r(cosf(an) * (float)hr), cy + lb_r(sinf(an) * (float)hr), PX_WHITE);
}

/* a sound's amp envelope as its silhouette, a column at a time (w x h); the first frac of it in lit */
static void lb_env(int x, int y, int w, int h, const float *env, qd_rgb col, float frac, qd_rgb lit)
{
    const float at = env[0], de = env[1], su = env[2], re = env[3], tot = 0.15f + at + de + 0.35f + re;
    int i, py = y + h - 1;
    for (i = 0; i < w; i++) {
        const float u = (float)i / (float)(w - 1) * tot;
        float lv;
        int yy;
        if (u < 0.08f)
            lv = 0.f;
        else if (u < 0.08f + at)
            lv = (u - 0.08f) / (at > 0.001f ? at : 0.001f);
        else if (u < 0.08f + at + de)
            lv = 1.f - (1.f - su) * (u - 0.08f - at) / (de > 0.001f ? de : 0.001f);
        else if (u < 0.08f + at + de + 0.35f)
            lv = su;
        else
            lv = su * (1.f - lb_clamp((u - 0.08f - at - de - 0.35f) / (re > 0.001f ? re : 0.001f), 0.f, 1.f));
        yy = y + h - 1 - lb_r(lb_clamp(lv, 0.f, 1.f) * (float)(h - 1));
        {
            const int a = yy < py ? yy : py, b = yy < py ? py : yy;
            px_vline(x + i, a, b - a + 1, (float)i < frac * (float)w ? lit : col);
        }
        py = yy;
    }
}

/* a dashed outline */
static void lb_dashbox(int x, int y, int w, int h, qd_rgb col)
{
    px_dots(x, y, w, col, 2);
    px_dots(x, y + h - 1, w, col, 2);
    px_vdots(x, y, h, col, 2);
    px_vdots(x + w - 1, y, h, col, 2);
}

/* a spark: a plus of pixels, its arms by size 0..2 */
static void lb_spark(int x, int y, int sz, qd_rgb col)
{
    px_pset(x, y, col);
    if (sz > 0) {
        px_pset(x - 1, y, col);
        px_pset(x + 1, y, col);
        px_pset(x, y - 1, col);
        px_pset(x, y + 1, col);
    }
    if (sz > 1) {
        px_pset(x - 2, y, col);
        px_pset(x + 2, y, col);
        px_pset(x, y - 2, col);
        px_pset(x, y + 2, col);
    }
}

/* the head: a title, a rule under it; OCT- back at the right */
static void lb_head(const char *title, qd_rgb col, int back)
{
    px_text(4, 3, title, col, PX_FS, 1);
    if (back) {
        px_box(104, 1, 13, 9, UI_K3);
        ui_icon(UI_IC_BACK, 221.f, 11.f, 1.f, UI_K3);
    }
    px_dots(0, 11, 120, UI_G3, 2);
}

static void lb_num3(char *b, int n)
{
    b[0] = (char)('0' + n / 100 % 10), b[1] = (char)('0' + n / 10 % 10), b[2] = (char)('0' + n % 10), b[3] = 0;
}

/* a name in the BIG font if it fits w, else the small one; centred at cx (cx < 0: left at x) */
static void lb_name(int x, int y, int w, const char *s, qd_rgb col, int centre)
{
    const int big = px_text_w(s, PX_FB, 1) <= w;
    const int font = big ? PX_FB : PX_FS, yy = big ? y : y + 1;
    if (centre)
        px_ctext(x + w / 2, yy, s, col, font, 1);
    else
        px_text(x, yy, s, col, font, 1);
}

/* P1.5r: a sound imported over SysEx: its floppy dropping in, its hub turning, the slot it went to */
void ui_import_draw(const char *name, int pool, int slot, float t)
{
    const qd_rgb col = pool ? UI_POOL_COL : UI_LIB_COL;
    const int drop = t < 0.25f ? lb_r((1.f - lb_ease(t / 0.25f)) * -50.f) : 0;
    char n[8];
    px_rect(0, 0, 120, 120, UI_BG);
    lb_head(pool ? "POOL" : "MY PRESETS", col, 0);
    lb_floppy(40, 18 + drop, 40, col, t * 9.f);
    lb_num3(n, slot);
    px_text(44, 21 + drop, n, PX_BLACK, PX_FS, 1);
    lb_name(4, 66, 112, name ? name : "", UI_K3, 1);
    px_ctext(60, 84, "IMPORTED", col, PX_FB, 1);
}

/* ---------------------------------------------------------------- the project pages */
static void prj_meter(int y, float frac, float own, qd_rgb col, int chip, const char *txt)
{
    const int x0 = 18, w = 72;
    if (chip) {                                         /* the flash: a chip */
        int k;
        px_rect(4, y - 2, 9, 6, UI_G1);
        for (k = 0; k < 4; k++) {
            px_pset(5 + k * 2, y - 3, UI_G1);
            px_pset(5 + k * 2, y + 4, UI_G1);
        }
    } else {                                            /* the pool: a floppy */
        lb_floppy(4, y - 4, 10, UI_POOL_COL, -1.f);
    }
    px_rect(x0, y, w, 3, UI_G3);
    if (frac > 0.f)
        px_rect(x0, y, lb_r((float)w * lb_clamp(frac, 0.f, 1.f)), 3, UI_G1);
    if (own > 0.f) {
        const int a = lb_r((float)w * lb_clamp(frac - own, 0.f, 1.f)), b = lb_r((float)w * lb_clamp(frac, 0.f, 1.f));
        px_rect(x0 + a, y, b - a, 3, col);
    }
    px_rtext(116, y - 1, txt, UI_K3, PX_FS, 1);
}

static void prj_dots(int x, int page)
{
    int i;
    for (i = 0; i < 2; i++)
        px_rect(x + i * 4, 5, 2, 2, i == page ? UI_K3 : UI_G2);
}

FZ_COLD void ui_prj_draw(const ui_prj_t *v)
{
    static const char *const BANK[4] = {"A", "B", "C", "D"};
    char n[16];
    int i;
    if (!v->page) {
        lb_head("PROJECT", UI_LIB_COL, 1);
        prj_dots(4 + px_text_w("PROJECT", PX_FS, 1) + 4, 0);
        n[0] = (char)('0' + v->num / 10), n[1] = (char)('0' + v->num % 10), n[2] = 0;
        px_text_sh(4, 15, n, UI_LIB_COL, PX_BROWN, PX_FB, 2);
        lb_name(40, 19, 76, v->name ? v->name : "", UI_K3, 0);
        for (i = 0; i < 4; i++)                         /* the 64 patterns: a speck empty, a box used, filled the one
                                                         * worked on; sparks off one just written */
            px_text(4, 38 + i * 10, BANK[i], UI_G1, PX_FS, 1);
        for (i = 0; i < 64; i++) {
            const int x = 12 + (i % 16) * 6 + ((i % 16) / 4) * 2, y = 37 + (i / 16) * 10;
            if (i == v->cur)
                px_rect(x, y, 5, 7, UI_K3);
            else if (v->used[i])
                px_box(x, y, 5, 7, UI_G1);
            else
                px_pset(x + 2, y + 3, UI_G2);
            if (v->since[i] >= 0.f && v->since[i] < 1.35f) {
                const float w = v->since[i] / 1.35f;
                int k;
                for (k = 0; k < 3; k++)
                    if (w > (float)k * 0.15f && w < 0.6f + (float)k * 0.15f)
                        lb_spark(x + 2 + (k - 1) * 3, y - 2 - lb_r((w - (float)k * 0.15f) * 14.f), k == 1, UI_LIB_COL);
            }
        }
        n[0] = 0;
        cat(cat(n, ui_itoa(n + 8, (int)(v->flash * 100.f + 0.5f))), "%");
        prj_meter(86, v->flash, v->flash_own, UI_LIB_COL, 1, n);
        prj_meter(102, (float)v->pool_n / 128.f, (float)v->pool_n / 128.f, UI_POOL_COL, 0, ui_itoa(n, v->pool_n));
        return;
    }
    lb_head("PROJECTS", UI_LIB_COL, 1);
    prj_dots(4 + px_text_w("PROJECTS", PX_FS, 1) + 4, 1);
    for (i = 0; i < UI_NPROJ; i++) {                    /* four a row (the Bluetooth package's four: one row) */
        const int x = 4 + (i % 4) * 29, y = (UI_NPROJ > 4 ? 15 : 40) + (i / 4) * 52, s = 26;
        const int cur = i == v->num - 1, pick = i == v->chosen && !cur, blink = (int)floorf(v->t * 3.f) % 2;
        qd_rgb col;
        int q;
        if (!v->p[i].exists) {
            lb_dashbox(x, y, s, s, pick ? UI_K3 : i == v->armed ? UI_K4 : UI_G2);
            px_hline(x + s / 2 - 3, y + s / 2, 7, UI_G2);
            px_vline(x + s / 2, y + s / 2 - 3, 7, UI_G2);
            continue;
        }
        col = i == v->armed ? UI_K4 : i == v->src ? UI_POOL_COL : cur ? UI_LIB_COL : pick ? UI_K3 : UI_G1;
        if (pick && !blink)
            col = UI_G2;
        lb_floppy(x, y, s, col, -1.f);
        n[0] = (char)('0' + (i + 1) / 10), n[1] = (char)('0' + (i + 1) % 10), n[2] = 0;
        px_text(x + 3, y + 3, n, PX_BLACK, PX_FS, 1);
        if (cur || pick || i == v->src || i == v->armed)
            px_box(x - 1, y - 1, s + 2, s + 2, col);
        for (q = 0; q < 64; q++)                        /* its patterns in use */
            px_pset(x + 5 + q % 16, y + s + 2 + (q / 16), v->p[i].used[q] ? (cur ? UI_LIB_COL : UI_G1) : UI_G3);
        {   /* its name, a word a line */
            const char *s2 = v->p[i].name ? v->p[i].name : "";
            char wd[9];
            int line = 0;
            while (*s2 && line < 2) {
                int m = 0;
                while (*s2 == ' ')
                    s2++;
                while (*s2 && *s2 != ' ' && m < 7)
                    wd[m++] = *s2++;
                while (*s2 && *s2 != ' ')               /* (the rest of a long word: cut) */
                    s2++;
                wd[m] = 0;
                if (!m)
                    break;
                px_text(x, y + s + 8 + line * 7, wd, cur ? UI_K3 : UI_G1, PX_FS, 1);
                line++;
            }
        }
    }
}
