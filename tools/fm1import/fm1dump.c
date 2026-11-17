/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* fm1dump: an FM-1 EVILGENIUS backup (.egbak: the store's region, 200704 bytes) read with the FM-1's OWN core (built
 * from an FM-1 checkout at its sizes: 6 tracks, 8 pages, the Bluetooth package's store; nothing of this repository's
 * core), every project written out as text the Move's importer (fm1load) reads. After tools/flash_dump.c in the FM-1's
 * repository: the image is read into RAM and never written back.
 *
 *     fm1dump BACKUP.egbak > projects.txt
 *
 * The lines (numbers decimal, bytes as hex):
 *   FM1DUMP 1 tracks steps pages np                          the FM-1's sizes, the patch's values
 *   PROJECT p name                                           (name: hex)
 *   HDR key_scale key_root launch rec_mode rec_quant rec_dub rec_auto
 *   TRACK t name named kind level pan chorus delay reverb dry r0 r1 r2 r3 r4 limit reserve
 *   SOUND k name preset tags flags pan chorus delay reverb patch
 *   MFX bytes                                                (the five groups, as they are: mfx.h is the Move's too)
 *   PAT k used master swing bpm
 *   TSET k t chan limit reserve rbase rtype swing transpose mute loop pstart pend scale root key_global mfx mout def
 *   TSND k t sound edited preset name patch
 *   STEP k t s wire                                          (QD_STEP_WIRE bytes)
 *   LOCK k t s id value
 *   END */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "persist.h"
#include "sounds.h"

#if !QD_STORE_BLE
#error "fm1dump: build it with -DQD_STORE_BLE=1 (the FM-1's store)"
#endif

#define IMG 0x100000u
#define EGBAK 200704u                                   /* (the region: QD_FLASH_LIB on, 49 sectors) */
static uint8_t g_img[IMG];

static int f_read(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    (void)ctx;
    if (off > IMG || n > IMG - off)
        return -1;
    memcpy(dst, g_img + off, n);
    return 0;
}
static int f_erase(void *ctx, uint32_t off)
{
    (void)ctx;
    if (off >= IMG)
        return -1;
    memset(g_img + (off & ~4095u), 0xFF, 4096);
    return 0;
}
static int f_prog(void *ctx, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)ctx;
    if (off > IMG || n > IMG - off)
        return -1;
    for (i = 0; i < n; i++)
        g_img[off + i] &= ((const uint8_t *)src)[i];
    return 0;
}
static const qd_flash_t g_fl = { f_read, f_erase, f_prog, 0 };

static void hex(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    size_t i;
    putchar(' ');
    if (!n)
        putchar('-');
    for (i = 0; i < n; i++)
        printf("%02x", b[i]);
}
static void name(const char *s, size_t cap)
{
    size_t n = strnlen(s, cap);
    hex(s, n);
}

static qd_app_t A;

int main(int argc, char **argv)
{
    FILE *f;
    size_t n;
    int p, k, t, s;
    if (argc < 2) {
        fprintf(stderr, "usage: fm1dump BACKUP.egbak > projects.txt\n");
        return 2;
    }
    memset(g_img, 0xFF, sizeof g_img);
    if (!(f = fopen(argv[1], "rb"))) {
        fprintf(stderr, "fm1dump: cannot read %s\n", argv[1]);
        return 1;
    }
    n = fread(g_img + QD_FLASH_LIB, 1, EGBAK, f);
    fclose(f);
    if (n != EGBAK) {
        fprintf(stderr, "fm1dump: %s is %zu bytes, not a backup's %u\n", argv[1], n, EGBAK);
        return 1;
    }
    qd_app_init(&A);
    qd_persist_attach(&A, &g_fl);
    printf("FM1DUMP 1 %d %d %d %d\n", QD_PAT_NTRACK, QD_NSTEP, QD_NPAGE, QD_QUAD_NP);
    for (p = 0; p < QD_NPROJ; p++) {
        if (!A.per.pexists[p])
            continue;
        if (A.per.proj != p && !qd_proj_load(&A, p)) {
            fprintf(stderr, "fm1dump: project %d not loaded\n", p + 1);
            continue;
        }
        printf("PROJECT %d", p);
        name(A.per.pname[p], QD_PROJ_NAME);
        putchar('\n');
        printf("HDR %d %d %d %d %d %d %d\n", A.proj.key_scale, A.proj.key_root, A.proj.launch, A.proj.rec_mode,
               A.proj.rec_quant, A.proj.rec_dub, A.proj.rec_auto);
        for (t = 0; t < QD_NTRACK; t++) {
            const qd_track_t *tk = &A.model.t[t];
            printf("TRACK %d", t);
            name(tk->name, sizeof tk->name);
            printf(" %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", tk->named, tk->kind, tk->mix.level, tk->mix.pan,
                   tk->mix.chorus, tk->mix.delay, tk->mix.reverb, tk->mix.dry, tk->mix.room[0], tk->mix.room[1],
                   tk->mix.room[2], tk->mix.room[3], tk->mix.room[4], tk->limit, tk->reserve);
        }
        for (k = 1; k <= A.proj.nsound && k <= QD_NSOUND; k++) {
            qd_sound_t sd;
            qd_sound_read(&A.proj, k, &sd);
            if (!sd.name[0])
                continue;
            printf("SOUND %d", k);
            name(sd.name, sizeof sd.name);
            printf(" %d %d %d %d %d %d %d", sd.preset, sd.tags, sd.flags, sd.pan, sd.chorus, sd.delay, sd.reverb);
            hex(sd.patch, sizeof sd.patch);
            putchar('\n');
        }
        printf("MFX");
        hex(A.proj.mfx, sizeof A.proj.mfx);
        putchar('\n');
        for (k = 0; k < QD_NPAT; k++) {
            const qd_pattern_t *P = qd_proj_cpat(&A.proj, k);
            int any = 0;
            for (t = 0; t < QD_PAT_NTRACK && !any; t++)
                for (s = 0; s < QD_NSTEP && !any; s++)
                    any = qd_step_is_on(qd_step(P, t, s));
            if (!P->used && !any)
                continue;
            printf("PAT %d %d %d %d %d\n", k, P->used, P->master, P->swing, P->bpm);
            for (t = 0; t < QD_PAT_NTRACK; t++) {
                const qd_tset_t *ts = &P->trk[t];
                int j;
                printf("TSET %d %d %d %d %d %d %d %d %d %d %u", k, t, ts->chan, ts->limit, ts->reserve, ts->rbase, ts->rtype,
                       ts->swing, ts->transpose, ts->mute, (unsigned)ts->loop);
                hex(ts->pstart, sizeof ts->pstart);
                hex(ts->pend, sizeof ts->pend);
                printf(" %d %d %d %d %d", ts->scale, ts->root, ts->key_global, ts->mfx, ts->mout);
                hex(&ts->def, sizeof ts->def);
                putchar('\n');
                printf("TSND %d %d %d %d %d", k, t, P->snd[t].sound, P->snd[t].edited, P->snd[t].preset);
                name(P->snd[t].name, sizeof P->snd[t].name);
                hex(P->snd[t].patch, sizeof P->snd[t].patch);
                putchar('\n');
                for (s = 0; s < QD_NSTEP; s++) {
                    const qd_step_t *st = qd_step(P, t, s);
                    uint8_t w[QD_STEP_WIRE], z[QD_STEP_WIRE];
                    qd_step_t empty;
                    memset(&empty, 0, sizeof empty);
                    qd_step_wire(st, w);
                    qd_step_wire(&empty, z);
                    if (!memcmp(w, z, sizeof w))
                        continue;                       /* (a step with nothing in it) */
                    printf("STEP %d %d %d", k, t, s);
                    hex(w, sizeof w);
                    putchar('\n');
                }
                for (j = 0; j < P->lock_n[t]; j++) {
                    const qd_lock_t *lk = &P->lock[P->lock_start[t] + j];
                    printf("LOCK %d %d %d %d %d\n", k, t, lk->step, lk->id, lk->value);
                }
            }
        }
    }
    printf("END\n");
    return 0;
}
