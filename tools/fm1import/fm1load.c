/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* fm1load: what fm1dump read from an FM-1 backup, made Move projects in a Move store image (this repository's core:
 * 8 tracks, 256 steps, 32 projects). Each FM-1 project becomes a new project in the first free slots from --first:
 * its 64 patterns are the 64 scenes (bank A scenes 1-16, B 17-32 ..: decided 17 November 2026), a pattern's six tracks
 * the scene's clips on tracks 1-6 (7 and 8 start empty), its pages, rate, key, step defaults, steps, locks and track
 * sounds as they were; its pool, its MIDI FX groups, its tracks' names, kinds and mix. The image is written back at
 * the end (an image of 1 MB grows to the 4 MB one: src/flash.c).
 *
 *     fm1load DUMP.txt MOVE-STORE.bin [--first N]          (N: the first project slot to fill, 1..32; 1 by default)
 *
 * What the Move's engine has not: a modulation to the FM-1 0.1.4's envelope destinations (QD_D_EA ..) is kept in the
 * sound but does nothing here; they are counted and said. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "flash.h"
#include "persist.h"
#include "sounds.h"

static qd_app_t A;
static int g_dropped_mods;

static int unhex(const char *s, void *out, size_t cap)  /* hex into out; its bytes ('-': none), -1 malformed */
{
    uint8_t *b = (uint8_t *)out;
    size_t n = 0;
    if (!strcmp(s, "-"))
        return 0;
    while (s[0] && s[1] && n < cap) {
        unsigned v;
        if (sscanf(s, "%2x", &v) != 1)
            return -1;
        b[n++] = (uint8_t)v;
        s += 2;
    }
    return (int)n;
}

static void count_mods(const int16_t *patch)            /* (the FM-1's newer destinations, not this engine's) */
{
    int k;
    for (k = 0; k < 3; k++)
        if (patch[QD_QP_LFO(k, QD_QL_DEST)] >= QD_D_N)
            g_dropped_mods++;
    for (k = 0; k < QD_NMOD; k++)
        if (patch[QD_QP_MOD(k, QD_QM_SRC)] && patch[QD_QP_MOD(k, QD_QM_DST)] >= QD_D_N)
            g_dropped_mods++;
}

static void finish(int slot)                            /* a project filled: its tracks' sounds A01's, written */
{
    int t;
    if (slot < 0)
        return;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        qd_track_from_tsound(&A.model.t[t], &qd_proj_cpat(&A.proj, 0)->snd[t]);
    A.snd_pat = -1;
    A.proj.bpm = qd_proj_cpat(&A.proj, 0)->bpm;
    qd_seq_tempo_mbpm(&A.seq, (int32_t)A.proj.bpm * 1000);
    qd_persist_flush(&A);
}

int main(int argc, char **argv)
{
    static char line[16384];
    FILE *f;
    int first = 0, slot = -1, nproj = 0, ntr = 6, i, orig;
    if (argc < 3) {
        fprintf(stderr, "usage: fm1load DUMP.txt MOVE-STORE.bin [--first N]\n");
        return 2;
    }
    for (i = 3; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--first"))
            first = atoi(argv[i + 1]) - 1;
    if (first < 0 || first >= QD_NPROJ)
        first = 0;
    if (!(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "fm1load: cannot read %s\n", argv[1]);
        return 1;
    }
    qd_app_init(&A);
    qd_persist_attach(&A, flash_open(argv[2]));
    orig = A.per.proj;                                  /* (the project open before: open again at the end) */
    while (fgets(line, sizeof line, f)) {
        char tag[16], a[8192], b[8192];
        int k = 0, t = 0, s = 0, v[24];
        line[strcspn(line, "\n")] = 0;
        if (sscanf(line, "%15s", tag) != 1)
            continue;
        if (!strcmp(tag, "FM1DUMP")) {
            int np = 0;
            if (sscanf(line, "%*s %*d %d %*d %*d %d", &ntr, &np) != 2 || np != QD_QUAD_NP) {
                fprintf(stderr, "fm1load: the dump's sounds have %d values, the Move's %d: not this converter's\n", np,
                        QD_QUAD_NP);
                return 1;
            }
        } else if (!strcmp(tag, "PROJECT")) {
            char nm[QD_PROJ_NAME] = "";
            int p;
            finish(slot);
            for (p = first; p < QD_NPROJ && A.per.pexists[p]; p++)
                ;
            if (p >= QD_NPROJ) {
                fprintf(stderr, "fm1load: no free project slot from %d: the rest not converted\n", first + 1);
                slot = -1;
                break;
            }
            if (!qd_proj_new(&A, p)) {
                fprintf(stderr, "fm1load: project %d not made\n", p + 1);
                return 1;
            }
            slot = p;
            nproj++;
            if (sscanf(line, "%*s %*d %8191s", a) == 1 && unhex(a, nm, sizeof nm - 1) > 0)
                qd_proj_name(&A, p, nm);
            else {
                snprintf(nm, sizeof nm, "FM-1 %d", nproj);
                qd_proj_name(&A, p, nm);
            }
            printf("FM-1 project -> Move project %d (%s)\n", p + 1, A.per.pname[p]);
        } else if (slot < 0) {
            continue;
        } else if (!strcmp(tag, "HDR")) {
            if (sscanf(line, "%*s %d %d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) == 7) {
                A.proj.key_scale = (uint8_t)v[0], A.proj.key_root = (uint8_t)v[1], A.proj.launch = (uint8_t)v[2];
                A.proj.rec_mode = (uint8_t)v[3], A.proj.rec_quant = (uint8_t)v[4], A.proj.rec_dub = (uint8_t)v[5];
                A.proj.rec_auto = (uint8_t)v[6];
            }
        } else if (!strcmp(tag, "TRACK")) {
            if (sscanf(line, "%*s %d %8191s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &t, a, &v[0], &v[1], &v[2], &v[3],
                       &v[4], &v[5], &v[6], &v[7], &v[8], &v[9], &v[10], &v[11], &v[12], &v[13], &v[14]) == 17 &&
                t >= 0 && t < ntr && t < QD_NTRACK) {
                qd_track_t *tk = &A.model.t[t];
                char nm[QD_TRACK_NAME] = "";
                if (unhex(a, nm, sizeof nm - 1) > 0)
                    memcpy(tk->name, nm, sizeof nm);
                tk->named = (uint8_t)v[0], tk->kind = (uint8_t)v[1];
                tk->mix.level = (uint8_t)v[2], tk->mix.pan = (int8_t)v[3], tk->mix.chorus = (uint8_t)v[4];
                tk->mix.delay = (uint8_t)v[5], tk->mix.reverb = (uint8_t)v[6], tk->mix.dry = (uint8_t)v[7];
                for (i = 0; i < 5; i++)
                    tk->mix.room[i] = (uint8_t)v[8 + i];
                tk->limit = (uint8_t)v[13], tk->reserve = (uint8_t)v[14];
            }
        } else if (!strcmp(tag, "SOUND")) {
            qd_sound_t sd;
            memset(&sd, 0, sizeof sd);
            if (sscanf(line, "%*s %d %8191s %d %d %d %d %d %d %d %8191s", &k, a, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5],
                       &v[6], b) == 10 && k >= 1 && k <= QD_NSOUND) {
                unhex(a, sd.name, sizeof sd.name - 1);
                sd.preset = (uint16_t)v[0], sd.tags = (uint8_t)v[1], sd.flags = (uint8_t)v[2], sd.pan = (int8_t)v[3];
                sd.chorus = (uint8_t)v[4], sd.delay = (uint8_t)v[5], sd.reverb = (uint8_t)v[6];
                if (unhex(b, sd.patch, sizeof sd.patch) == (int)sizeof sd.patch) {
                    count_mods(sd.patch);
                    qd_sound_write(&A.proj, k, &sd);
                    if (k > A.proj.nsound)
                        A.proj.nsound = (uint8_t)k;
                }
            }
        } else if (!strcmp(tag, "MFX")) {
            if (sscanf(line, "%*s %8191s", a) == 1)
                unhex(a, A.proj.mfx, sizeof A.proj.mfx);
        } else if (!strcmp(tag, "PAT")) {
            if (sscanf(line, "%*s %d %d %d %d %d", &k, &v[0], &v[1], &v[2], &v[3]) == 5 && k >= 0 && k < QD_NPAT) {
                qd_pattern_t *P = qd_proj_pat(&A.proj, k);
                qd_pat_init(P);
                P->used = (uint8_t)v[0], P->master = (uint16_t)v[1], P->swing = (uint8_t)v[2], P->bpm = (uint16_t)v[3];
            }
        } else if (!strcmp(tag, "TSET")) {
            char ps[64], pe[64], df[64];
            unsigned loop = 0;
            if (sscanf(line, "%*s %d %d %d %d %d %d %d %d %d %d %u %63s %63s %d %d %d %d %d %63s", &k, &t, &v[0], &v[1], &v[2],
                       &v[3], &v[4], &v[5], &v[6], &v[7], &loop, ps, pe, &v[8], &v[9], &v[10], &v[11], &v[12], df) == 19 &&
                k >= 0 && k < QD_NPAT && t >= 0 && t < ntr && t < QD_PAT_NTRACK) {
                qd_tset_t *ts = &qd_proj_pat(&A.proj, k)->trk[t];
                uint8_t st[QD_NPAGE], en[QD_NPAGE];
                int n1 = unhex(ps, st, sizeof st), n2 = unhex(pe, en, sizeof en), pg;
                ts->chan = (uint8_t)v[0], ts->limit = (uint8_t)v[1], ts->reserve = (uint8_t)v[2], ts->rbase = (uint8_t)v[3];
                ts->rtype = (uint8_t)v[4], ts->swing = (uint8_t)v[5], ts->transpose = (int8_t)v[6], ts->mute = (uint8_t)v[7];
                ts->loop = (qd_pmask_t)loop;
                for (pg = 0; pg < n1 && pg < n2 && pg < QD_NPAGE; pg++)
                    ts->pstart[pg] = st[pg], ts->pend[pg] = en[pg];
                ts->scale = (uint8_t)v[8], ts->root = (uint8_t)v[9], ts->key_global = (uint8_t)v[10];
                ts->mfx = (uint8_t)v[11], ts->mout = (uint8_t)v[12];
                unhex(df, &ts->def, sizeof ts->def);
            }
        } else if (!strcmp(tag, "TSND")) {
            if (sscanf(line, "%*s %d %d %d %d %d %8191s %8191s", &k, &t, &v[0], &v[1], &v[2], a, b) == 7 && k >= 0 &&
                k < QD_NPAT && t >= 0 && t < ntr && t < QD_PAT_NTRACK) {
                qd_tsound_t *ts = &qd_proj_pat(&A.proj, k)->snd[t];
                ts->sound = (uint8_t)v[0], ts->edited = (uint8_t)v[1], ts->preset = (uint16_t)v[2];
                memset(ts->name, 0, sizeof ts->name);
                unhex(a, ts->name, sizeof ts->name - 1);
                if (unhex(b, ts->patch, sizeof ts->patch) == (int)sizeof ts->patch)
                    count_mods(ts->patch);
            }
        } else if (!strcmp(tag, "STEP")) {
            uint8_t w[QD_STEP_WIRE];
            if (sscanf(line, "%*s %d %d %d %8191s", &k, &t, &s, a) == 4 && k >= 0 && k < QD_NPAT && t >= 0 && t < ntr &&
                t < QD_PAT_NTRACK && s >= 0 && s < QD_NSTEP && unhex(a, w, sizeof w) == (int)sizeof w)
                qd_step_unwire(&qd_proj_pat(&A.proj, k)->step[t][s], w);
        } else if (!strcmp(tag, "LOCK")) {
            if (sscanf(line, "%*s %d %d %d %d %d", &k, &t, &s, &v[0], &v[1]) == 5 && k >= 0 && k < QD_NPAT && t >= 0 &&
                t < ntr && t < QD_PAT_NTRACK && s >= 0 && s < QD_NSTEP)
                if (qd_lock_set(qd_proj_pat(&A.proj, k), t, s, v[0], v[1]) == QD_EFULL)
                    fprintf(stderr, "fm1load: pattern %d's locks full: one dropped\n", k + 1);
        } else if (!strcmp(tag, "END")) {
            break;
        }
    }
    fclose(f);
    finish(slot);
    if (nproj && A.per.proj != orig && qd_proj_load(&A, orig))
        qd_persist_flush(&A);
    flash_flush();
    printf("%d FM-1 project(s) converted into %s", nproj, argv[2]);
    if (g_dropped_mods)
        printf("; %d modulation(s) to the FM-1 0.1.4's envelope destinations do nothing here", g_dropped_mods);
    printf("\n");
    return nproj ? 0 : 1;
}
