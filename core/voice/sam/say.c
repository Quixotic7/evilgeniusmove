/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: SAY, SAM speaking (say.h). Every word the voices speak is SAM's render: a patch's SAY word (a random
 * word) or its WORD (one of the 54, from its phonetic spelling), in the patch's voice. SAM (this directory, see
 * README.md) writes the word at the words' rate into its buffer; here the silence at either end is trimmed, the rest
 * normalised and quantised to the 4 bits of the SID's volume register, faded in and out (no click), two samples a
 * byte. The cache is one arena (QD_SAY_ARENA bytes) holding up to QD_SAY_WORDS words, each as long as it is; a word
 * not wanted in the app's current look (qd_say_pass) gives way, the oldest first, and the arena closes up over it.
 * A word being written has no key (no voice finds it); a voice still speaking a word that moved reads other samples
 * of the arena for the rest of its note (in bounds: words only move down). */
#include <string.h>
#include "say.h"
#include "quad_voice.h"
#include "names.h"
#include "sam_names.h"                  /* (last of all but SAM's own: its macros rename SAM's globals) */
#include "sam.h"
#include "reciter.h"

typedef struct {
    volatile uint32_t key;              /* (0: empty, or being written) */
    uint32_t off, len, used;            /* its place in the arena (bytes), its samples, the clock it was last wanted at */
} say_word_t;
static say_word_t words[QD_SAY_WORDS];
static uint8_t arena[QD_SAY_ARENA];     /* (a voice still reading a word that moved reads from where the word was:
                                         * inside the arena, since a word is placed whole within it) */
static uint32_t arena_end;              /* bytes in use */
static uint32_t say_clock, say_pass;    /* a tick a want; the tick the app's current look began at */

int qd_say_text(int seed, char *out)
{
    uint32_t s;
    out[0] = 0;
    if (seed <= 0)
        return 0;
    s = (uint32_t)seed * 2654435761u + 12345u;         /* (the name generators' seed: one word a value) */
    if (seed % 3 == 0)
        qd_name_rnd(&s, out);                           /* a third: namesizer's pairs ("WARM CAT") */
    else
        qd_name_phonic(&s, out);                        /* the rest: phonic nonsense ("SMAIRSH") */
    return out[0] != 0;
}

int qd_say_default_voice(const int16_t *p)
{
    return p[QD_QP_SSPD] == QD_SAY_SPD0 && p[QD_QP_SPIT] == QD_SAY_PIT0 && p[QD_QP_STHR] == QD_SAY_THR0 &&
           p[QD_QP_SMTH] == QD_SAY_MTH0;
}

uint32_t qd_say_key(const int16_t *p)
{
    const int say = p[QD_QP_SAY], w = p[QD_QP_WORD];
    int32_t v[5];
    uint32_t h = 2166136261u;
    int k;
    if (say > 0)
        v[0] = 1000 + say;
    else if (w >= 1 && w <= qd_vox_nwords())
        v[0] = w;
    else
        return 0;
    v[1] = p[QD_QP_SSPD], v[2] = p[QD_QP_SPIT], v[3] = p[QD_QP_STHR], v[4] = p[QD_QP_SMTH];
    for (k = 0; k < 5; k++)
        h = (h ^ (uint32_t)v[k]) * 16777619u;
    return h ? h : 1u;
}

const uint8_t *qd_say_get(uint32_t key, uint32_t *len)
{
    int i;
    if (!key)
        return 0;
    for (i = 0; i < QD_SAY_WORDS; i++)
        if (words[i].key == key) {
            *len = words[i].len;
            return arena + words[i].off;
        }
    return 0;
}

static int clampi(int x, int a, int b) { return x < a ? a : x > b ? b : x; }
static int dev(int x) { return x < 128 ? 128 - x : x - 128; }

/* SAM's buffer after a render: the speech in it, a .. e (its silence at either end left out) and its peak; 0: none */
static int sam_span(uint32_t *a, uint32_t *e, uint32_t *peak)
{
    const unsigned char *b = (const unsigned char *)GetBuffer();
    uint32_t n = (uint32_t)GetBufferLength() / SAM_DIV, i;
    if (n > SAM_BUF_LEN)
        n = SAM_BUF_LEN;
    for (*a = 0; *a < n && dev(b[*a]) < 8; (*a)++)
        ;
    for (*e = n; *e > *a && dev(b[*e - 1]) < 8; (*e)--)
        ;
    *peak = 0;
    for (i = *a; i < *e; i++)
        if ((uint32_t)dev(b[i]) > *peak)
            *peak = (uint32_t)dev(b[i]);
    return *e > *a && *peak >= 8u;
}

/* SAM speaks p's word into its own buffer; 1: it did */
static int say_speak(const int16_t *p)
{
    static char in[256];
    const int say = p[QD_QP_SAY];
    if (say > 0) {
        char t[16];
        if (!qd_say_text(say, t))
            return 0;
        strcpy(in, t);
        strcat(in, "[");                                /* (the reciter's end mark: it writes the phonemes back) */
        if (!TextToPhonemes((unsigned char *)in))
            return 0;
    } else {
        strncpy(in, qd_vox_word_phon(p[QD_QP_WORD]), 240);
        in[240] = 0;
        strcat(in, "\x9b");                             /* (SAM's end of input) */
    }
    SetInput(in);
    SetSpeed((unsigned char)clampi(p[QD_QP_SSPD], 1, 255));
    SetPitch((unsigned char)clampi(p[QD_QP_SPIT], 0, 255));
    SetThroat((unsigned char)clampi(p[QD_QP_STHR], 0, 255));
    SetMouth((unsigned char)clampi(p[QD_QP_SMTH], 0, 255));
    return SAMMain() != 0;
}

/* the word in SAM's buffer (a .. e, its peak) into d: normalised, 4 bits, faded at its ends; its samples (even) */
static uint32_t say_pack(uint8_t *d, uint32_t a, uint32_t e, uint32_t peak)
{
    const unsigned char *b = (const unsigned char *)GetBuffer();
    const uint32_t m = (e - a) & ~1u, f = 60u;          /* (10 ms fades) */
    uint32_t j;
    for (j = 0; j < m; j++) {
        int32_t x = (int32_t)b[a + j] - 128, q;
        if (j < f)
            x = x * (int32_t)j / (int32_t)f;
        else if (m - j < f)
            x = x * (int32_t)(m - j) / (int32_t)f;
        q = 8 + (x * 15 + (x < 0 ? -(int32_t)peak : (int32_t)peak)) / (2 * (int32_t)peak);
        q = clampi(q, 0, 15);
        if (j & 1u)
            d[j / 2u] = (uint8_t)((d[j / 2u] & 0xF0u) | (uint32_t)q);
        else
            d[j / 2u] = (uint8_t)((uint32_t)q << 4 | 8u);
    }
    return m;
}

/* the word in entry i leaves: the arena closes up over it */
static void say_drop(int i)
{
    const uint32_t off = words[i].off, n = (words[i].len + 1u) / 2u;
    int k;
    words[i].key = 0;
    memmove(arena + off, arena + off + n, arena_end - off - n);
    arena_end -= n;
    for (k = 0; k < QD_SAY_WORDS; k++)
        if (words[k].key && words[k].off > off)
            words[k].off -= n;
    words[i].len = 0;
}

/* a word that found no room is not spoken again at every look (SAM's time): its key waits ~1 s of looks */
#define SAY_NMISS 4
static uint32_t miss_key[SAY_NMISS], miss_at[SAY_NMISS], say_npass;
static int say_missed(uint32_t key)
{
    int i;
    for (i = 0; i < SAY_NMISS; i++)
        if (miss_key[i] == key && say_npass - miss_at[i] < 64u)
            return 1;
    return 0;
}
static void say_miss(uint32_t key)
{
    int i, o = 0;
    for (i = 1; i < SAY_NMISS; i++)
        if (miss_at[i] < miss_at[o])
            o = i;
    miss_key[o] = key, miss_at[o] = say_npass;
}

void qd_say_pass(void)
{
    say_pass = say_clock;
    say_npass++;
}

int qd_say_touch(const int16_t *p)
{
    const uint32_t key = qd_say_key(p);
    int i;
    for (i = 0; key && i < QD_SAY_WORDS; i++)
        if (words[i].key == key) {
            words[i].used = ++say_clock;
            return 1;
        }
    return 0;
}

/* ---------------------------------------------------------------- THE VOICE (say.h) */
static volatile uint32_t ann_len, ann_pos;              /* SAM's samples to speak; the audio's place, Q16 samples */
static volatile uint8_t ann_ring;                       /* the sentries' voice: a 30 Hz ring over it */
static volatile int32_t ann_gain;                       /* the line's peak brought to the same level */
#define ANN_STEP ((uint32_t)(65536u * 6000u / 44100u))  /* (SAM's rate against the output's) */

int qd_ann_busy(void) { return (ann_pos >> 16) < ann_len; }
void qd_ann_stop(void) { ann_len = 0; }

int qd_ann_say(const char *text, int voice, int level)
{
    static char in[256];
    uint32_t a, e, peak;
    ann_len = 0;                                        /* (the line before stops: its buffer is SAM's again) */
    ann_pos = 0;
    strncpy(in, text, 240);
    in[240] = 0;
    strcat(in, "[");
    if (!TextToPhonemes((unsigned char *)in))
        return 0;
    SetInput(in);
    SetSpeed((unsigned char)(voice == QD_ANN_ROBOT ? 68 : 64));
    SetPitch((unsigned char)(voice == QD_ANN_ROBOT ? 58 : 92));
    SetThroat((unsigned char)(voice == QD_ANN_ROBOT ? 190 : 138));
    SetMouth((unsigned char)(voice == QD_ANN_ROBOT ? 190 : 118));
    if (!SAMMain() || !sam_span(&a, &e, &peak))
        return 0;
    ann_gain = 150 * (level < 0 ? 0 : level > 100 ? 100 : level) / (int32_t)peak;
    ann_ring = voice == QD_ANN_ROBOT;
    ann_pos = a << 16;
    ann_len = e;                                        /* (last: the audio finds it whole) */
    return 1;
}

void qd_ann_mix(int32_t *l, int32_t *r, int n)
{
    const unsigned char *b = (const unsigned char *)GetBuffer();
    const uint32_t len = ann_len;
    uint32_t p = ann_pos;
    int i;
    if ((p >> 16) >= len)
        return;
    for (i = 0; i < n && (p >> 16) < len; i++, p += ANN_STEP) {
        int32_t v = ((int32_t)b[p >> 16] - 128) * ann_gain;
        if (ann_ring && (((p >> 16) / 100u) & 1u))      /* (30 Hz, a square: the pepper-pots' croak) */
            v = -v;
        l[i] += v, r[i] += v;
    }
    ann_pos = p;
}

int qd_say_want(const int16_t *p)
{
    const uint32_t key = qd_say_key(p);
    uint32_t a, e, peak, need;
    int i, free_i = -1;
    if (!key)
        return 0;
    for (i = 0; i < QD_SAY_WORDS; i++) {
        if (words[i].key == key) {
            words[i].used = ++say_clock;
            return 0;
        }
        if (!words[i].key && free_i < 0)
            free_i = i;
    }
    if (say_missed(key))
        return 0;
    qd_ann_stop();                                      /* (THE VOICE speaks from SAM's buffer: a track's word comes
                                                         * first, his line is cut) */
    if (!say_speak(p) || !sam_span(&a, &e, &peak)) {
        say_miss(key);
        return 1;                                       /* (SAM's time was spent; nothing to keep) */
    }
    need = (((e - a) & ~1u) + 1u) / 2u;
    while (free_i < 0 || arena_end + need > QD_SAY_ARENA) {   /* room: the oldest word not wanted in this look goes */
        int old = -1;
        for (i = 0; i < QD_SAY_WORDS; i++)
            if (words[i].key && words[i].used <= say_pass && (old < 0 || words[i].used < words[old].used))
                old = i;
        if (old < 0) {
            say_miss(key);
            return 1;                                   /* (every word here is wanted now: this one stays silent) */
        }
        say_drop(old);
        if (free_i < 0)
            free_i = old;
    }
    words[free_i].off = arena_end;
    words[free_i].len = say_pack(arena + arena_end, a, e, peak);
    arena_end += need;
    words[free_i].used = ++say_clock;
    words[free_i].key = key;                            /* (last: a voice finds it whole) */
    return 1;
}

void qd_say_reset(void)
{
    memset(words, 0, sizeof words);
    arena_end = 0;
    say_clock = say_pass = say_npass = 0;
    memset(miss_key, 0, sizeof miss_key);
    memset(miss_at, 0, sizeof miss_at);
    ann_len = ann_pos = 0;
}
