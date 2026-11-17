/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius's voice engine: SID TONE. A voice is a whole emulated SID chip (core/voice/sid/sid.c, fm1-sid's
 * clean-room 6581 / 8580, BSD-2): its three oscillators (a main one and two partners: detuned, an interval, sync and
 * ring sources), their three hardware ADSRs, the chip's filter and its master volume. VOX: the same chip's volume
 * register played as a 4-bit speech sample ("digi"), the way the 1984 games spoke: a word a note, pitched by the key.
 *
 * The interface is FIZZ's (FM TONE's, so the sequencer, the voice pool, the mix and the locks are FIZZ's unchanged):
 * qd_quad_* names, a patch of QD_QUAD_NP int16 values per voice, an owner per track. A voice renders one control
 * block (QD_QUAD_CTL = 32 samples at 44.1 kHz) a call; owners' blocks first, then the voices'. Integer DSP but for the
 * pitch's exponent; single-threaded; two runs give the same samples. A full-scale voice is near 24000 (VOICE_FS). */
#pragma once
#include <stdint.h>
#include "../sid/sid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_QUAD_CTL 32                  /* samples a control block */
#define QD_QUAD_BLOB 128                /* a packed sound: 'S', version, the values (value - min, a byte; FREQ two), the
                                         * MIDI sound's tag at 124..127 */
#define QD_QUAD_BLOB5 96                /* (FIZZ's older SysEx size: the first 96 bytes) */
#define QD_QUAD_NPRESET0 17             /* 0 INIT, 1..16 the first set (0.1's) .. */
#define QD_QUAD_NPRESET 89              /* .. and 17.. the second (0.2: sounds.h puts the six kit drums between them) */
/* preset k's factory id (sounds.h: 1..17 INIT and the first set, 18..23 the kit, 24.. the second set) */
#define QD_QUAD_PRESET_ID(k) ((k) < QD_QUAD_NPRESET0 ? (k) + 1 : (k) + 7)

/* the sound's values (the p-lock ids). Ranges in qd_quad_value_range; values shown "+-" are stored centred on 64 */
enum {
    /* the oscillators: WAVE 0 OFF, TRI SAW PULSE NOISE and the combined; COARSE -24..24; FINE 0..127 (64: none, +-1
     * semitone); PW 0..127; MOD 0 OFF, SYNC, RING, S+R (by the oscillator before it: 1 by 3, 2 by 1, 3 by 2) */
    QD_QP_W1, QD_QP_C1, QD_QP_F1, QD_QP_PW1, QD_QP_M1,
    QD_QP_W2, QD_QP_C2, QD_QP_F2, QD_QP_PW2, QD_QP_M2,
    QD_QP_W3, QD_QP_C3, QD_QP_F3, QD_QP_PW3, QD_QP_M3,
    QD_QP_DETUNE,                       /* 0..127: oscillators 2 and 3 spread apart (a chorus), 0 none */
    /* the three envelopes, the SID's own (0..15 each); 2 and 3 can follow 1 (LINK) */
    QD_QP_EATK, QD_QP_EDEC, QD_QP_ESUS, QD_QP_EREL,
    QD_QP_E2A, QD_QP_E2D, QD_QP_E2S, QD_QP_E2R, QD_QP_E2LINK,
    QD_QP_E3A, QD_QP_E3D, QD_QP_E3S, QD_QP_E3R, QD_QP_E3LINK,
    QD_QP_HR,                           /* hard restart: a struck voice's gate held off this many ms first (0..127) */
    /* the filter, the chip's: cutoff 0..2047, resonance 0..15, its mode, the oscillators routed into it (bits), the
     * filter envelope (a software one: 0..127 times, its depth +-), key tracking, the chip's model */
    QD_QP_FREQ, QD_QP_RESO, QD_QP_FTYPE, QD_QP_FROUTE, QD_QP_FDEPTH, QD_QP_FKTRK,
    QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_CHIP,
    /* the amp: LEVEL, velocity's reach, DRIVE (the mix runs it: qd_quad_dist), CRUSH */
    QD_QP_LEVEL, QD_QP_VEL, QD_QP_DRIVE, QD_QP_DRPRE, QD_QP_BITS, QD_QP_SRR, QD_QP_CRPOST,
    /* the glide (FIZZ's SLIDE locks these three) */
    QD_QP_PORT, QD_QP_PTIM, QD_QP_PSLOPE,
    /* VOX: the word (0 OFF, 1.. the vocabulary), the key it plays at its own speed, where in it to start and end, the
     * loop, the digi's bits (1..4), its update rate, the 8580's boost (oscillator 3 as the DC the volume scales), level */
    QD_QP_WORD, QD_QP_ROOT, QD_QP_VSTART, QD_QP_VEND, QD_QP_VLOOP, QD_QP_DBITS, QD_QP_DHOLD, QD_QP_DBOOST, QD_QP_DLVL,
    QD_QP_LFO0,                         /* three LFOs, eight values each (QD_QP_LFO(k, f)) */
    QD_QP_MOD0 = QD_QP_LFO0 + 3 * 8,    /* eight modulation slots (QD_QP_MOD(k, f)) */
    /* the VOICE page (not lockable): POLY / MONO, UNISON 1..8, its DETUNE and SPREAD, PRIORITY */
    QD_QP_VMODE = QD_QP_MOD0 + 3 * 8,
    QD_QP_UNI, QD_QP_UDET, QD_QP_USPR, QD_QP_VPRIO,
    /* SAY, SAM's live speech (core/voice/sam/say.c): the word (0 OFF: WORD's; 1..999 a random word from FIZZ's name
     * generator, each value its own), and the voice SAM speaks with, its own four: SPEED, PITCH, THROAT, MOUTH (at
     * SAM's defaults WORD's words are the stored ones; else SAM renders them too). Lockable, as every sound value */
    QD_QP_SAY, QD_QP_SSPD, QD_QP_SPIT, QD_QP_STHR, QD_QP_SMTH,
    QD_QUAD_NP
};
#define QD_SAY_SPD0 72                  /* SAM's own voice */
#define QD_SAY_PIT0 64
#define QD_SAY_THR0 128
#define QD_SAY_MTH0 128
/* (FM TONE's that FIZZ's code names but SID TONE does not have: never a value's id) */
#define QD_QP_RB1 1000
#define QD_QP_RB2 1001
#define QD_QP_ALGO 1002
#define QD_QP_PHRT 1003
#define QD_QP_AMOD 1004
#define QD_QP_BWPOST 1005
#define QD_QP_FSHAPE 1006
#define QD_QUAD_NP_E4 QD_QP_VMODE       /* (FIZZ's old mix lock ids started here: kept for its persist's arithmetic) */
#define QD_QUAD_FMNP QD_QP_VMODE
#define QD_QUAD_NP5 QD_QP_VMODE

int qd_quad_lockable(int id);           /* a step may lock value id (the VOICE page's may not) */

/* FIZZ MIDI: a MIDI track's sound in the same patch (FIZZ's scheme): its 28 slots' values in SID TONE values of range
 * 0..127 (QD_MSND_ID), their CC numbers and touched flags in 28 consecutive values (QD_MSND_CC: the third LFO's last
 * four and the modulation slots), the mark in VPRIO */
#define QD_MSND_N 28
#define QD_MSND_PAGE 4
#define QD_MSND_NPAGE 7
#define QD_MSND_MARK 0x4D
#define QD_MSND_TOUCHED 128
#define QD_MSND_CC0 (QD_QP_LFO0 + 2 * 8 + 4)
#define QD_MSND_CC(k) (QD_MSND_CC0 + (k))
extern const uint8_t QD_MSND_ID[QD_MSND_N];
static inline int qd_msnd_is(const int16_t *p) { return p && p[QD_QP_VPRIO] == QD_MSND_MARK; }
int qd_msnd_slot(int id);
void qd_msnd_init(int16_t *p);
void qd_msnd_over(int16_t *p);
#define QD_SIDE_N 56
void qd_quad_side_pack(const int16_t *p, uint8_t *b);
void qd_quad_side_unpack(const uint8_t *b, int midi, int16_t *p);

enum { QD_QM_SRC, QD_QM_DST, QD_QM_DEPTH };
enum { QD_MS_OFF, QD_MS_VEL, QD_MS_KEY, QD_MS_MW, QD_MS_AT, QD_MS_PB, QD_MS_BR, QD_MS_RAND, QD_MS_FENV, QD_MS_N };
#define QD_QP_MOD(k, f) (QD_QP_MOD0 + (k) * 3 + (f))
#define QD_NMOD 8
enum { QD_PORT_OFF, QD_PORT_ON, QD_PORT_LEGATO };
/* the filter's modes (FTYPE) */
enum { QD_QF_OFF, QD_QF_LP, QD_QF_BP, QD_QF_HP, QD_QF_NOTCH, QD_QF_LPBP, QD_QF_BPHP, QD_QF_ALL, QD_QF_N };
#define QD_QF_LP12 QD_QF_LP
#define QD_QF_HP12 QD_QF_HP
#define QD_QF_LP24 QD_QF_LP
/* the oscillators' waves (W1..W3) */
enum { QD_SW_OFF, QD_SW_TRI, QD_SW_SAW, QD_SW_PUL, QD_SW_NOI, QD_SW_TS, QD_SW_PT, QD_SW_PS, QD_SW_PTS, QD_SW_N };
/* an LFO's values: SPEED 0..127, MULT (0 FREE, 1.. a tempo division), FADE (64 none, under: in, over: out), DEST,
 * WAVE, PHASE 0..127, TRIG (FREE, TRIG, HOLD, ONE, HALF), DEPTH (64 none) */
enum { QD_QL_SPEED, QD_QL_MULT, QD_QL_FADE, QD_QL_DEST, QD_QL_WAVE, QD_QL_PHASE, QD_QL_TRIG, QD_QL_DEPTH };
#define QD_QP_LFO(k, f) (QD_QP_LFO0 + (k) * 8 + (f))
/* (0.2) the ARP waves: the LFO is a C64 player's arpeggio, a chord's notes one after another on the one voice. On a
 * pitch (PITCH, PITCH 1..3) the steps are the chord's semitones exactly, whatever DEPTH: its sign is the direction
 * (over 64 up, under down) and its size the range (1..21 the chord, 22..42 two octaves of it, 43.. three). SPEED is a
 * step's length in PAL frames (127: one frame, 50 Hz; a frame more every 8 below, to 16), MULT a step's length in the
 * tempo's divisions. On another destination an ARP wave is a staircase, DEPTH as ever */
enum { QD_LW_TRI, QD_LW_SINE, QD_LW_SAW, QD_LW_SQR, QD_LW_SH, QD_LW_EXP, QD_LW_RAMP,
       QD_LW_ARP0, QD_LW_AOCT = QD_LW_ARP0, QD_LW_A5TH, QD_LW_AMAJ, QD_LW_AMIN, QD_LW_ASUS, QD_LW_ADOM7, QD_LW_AMIN7,
       QD_LW_AMAJ7, QD_LW_ADIM, QD_LW_N };
int qd_quad_arp_chord(int wave, const uint8_t **notes);   /* an ARP wave's notes (semitones) and their count; 0: not one */
enum { QD_LT_FREE, QD_LT_TRIG, QD_LT_HOLD, QD_LT_ONE, QD_LT_HALF, QD_LT_N };
/* the modulation destinations (LFO DEST, a slot's DST): a value of the patch, or the voice's own (pitch, pan ..) */
enum { QD_D_OFF, QD_D_PITCH, QD_D_CUT, QD_D_RES, QD_D_PW1, QD_D_PW2, QD_D_PW3, QD_D_PWALL, QD_D_P1, QD_D_P2, QD_D_P3,
       QD_D_LEVEL, QD_D_PAN, QD_D_DRIVE, QD_D_BITS, QD_D_SRR, QD_D_FENV, QD_D_WPOS, QD_D_VOXP, QD_D_DETUNE, QD_D_N };

typedef struct qd_quad_voice qd_quad_voice_t;
typedef struct {
    int16_t sp, bpm;
    uint8_t mu, ok;
    int32_t inc;
} qd_lfo_cache_t;

/* A track as the voices' owner: its FREE LFOs, its tempo, its chord and glide memory, its MIDI state */
typedef struct qd_quad_owner {
    int16_t p[1];
    struct { uint32_t ph[3], rnd[3]; } quad_lfo;   /* the FREE LFOs' phases and random values */
    int8_t quad_pan_off;                /* the LFOs' PAN offset, from the latest note's voice (-64..63) */
    int8_t quad_fx_off[4];              /* (DRIVE, REVERB, DELAY, CHORUS offsets: none here) */
    const qd_quad_voice_t *m_v;         /* the latest note's voice */
    const int16_t *quad_patch;          /* the track's sound: the FREE LFOs run at its rates */
    int16_t bpm;
    int16_t last_pitch;                 /* the track's last note (1/16 semitone; -1 none): a glide's start */
    uint8_t lc_n;
    int16_t lc[8];
    uint8_t cc_n;
    uint8_t cc[8], cc_slot[8];
    const void *cc_key;
    int64_t cc_tick;
    uint8_t cc_held;
    uint8_t mw, at, br;                 /* the track's MIDI state: CC 1, pressure, CC 2 .. */
    int16_t pb;                         /* .. the pitch bend */
    uint32_t nserial;                   /* the notes struck (the RANDOM source's seed) */
    qd_lfo_cache_t lfc[3];
    uint8_t defer_off;                  /* the split block: a voice writes its offsets in itself (nx_*), published after */
} qd_quad_owner_t;

/* A voice: the fields FIZZ's pool and mix read, then the chip and its driver's state */
struct qd_quad_voice {
    uint8_t note, vel, gate, active;
    uint8_t mvel;
    uint8_t kill;                       /* the fade steal: the next block ramps to 0, then it ends */
    int32_t env, env_out;               /* env_out: the last block's ramp (Q15; 0: silent) */
    int32_t pitch16, pitch_cur;         /* 1/16 semitone */
    uint32_t udet;                      /* a unison voice's detune (x udet / 65536; 0 none) */
    int16_t patch[QD_QUAD_NP];
    qd_quad_owner_t *owner;
    uint8_t dip;
    int32_t gstep;                      /* the glide's step a block (1/16 semitone; 0: by its RATE) */
    int16_t mrnd;                       /* the RANDOM source's value for this note (Q15) */
    int8_t nx_pan, nx_fx[4];
    uint8_t nx_ok;
    int16_t nx_lw[3];                   /* the LFOs' last outputs (Q15, depth in): the editor shows the values as heard */
    /* SID TONE's own */
    sid_t chip;
    uint32_t idx;
    int32_t level;                      /* the last block's peak (Q15) */
    uint8_t hr_left, gated, model;      /* hard restart blocks left; the gate as the chip has it; its model */
    uint8_t esync;                      /* the next gate edge starts the chip's envelope rate counters at 0 (below) */
    uint8_t lstage[3];                  /* the LFOs: 0 running, 1 stopped (ONE / HALF done) */
    uint32_t lph[3];                    /* their phases (32-bit: a cycle) */
    int32_t lhold[3], lsh[3];           /* HOLD's value (an ARP wave: its semitones); S&H's current value */
    uint32_t lage;                      /* blocks since the note (the fades) */
    int32_t fenv;                       /* the filter envelope (Q15) and its stage (0 attack .. 3 release, 4 done) */
    uint8_t fstage;
    struct {                            /* VOX: the word playing */
        uint8_t on, dir, nib;
        uint8_t filt;                   /* the word goes through the filter (ROUTE's V, a mode on): the chip's EXT IN */
        uint16_t word;
        const uint8_t *src;             /* the word's 4-bit samples (the stored ones, or SAM's render) and their count */
        uint32_t len;
        uint32_t pos, lo, hi;           /* Q16 samples of the word */
        uint32_t hold_acc;              /* Q16: samples to the next volume write */
    } vox;
    struct { uint32_t ph; int32_t held; } crush;
};

/* ---- the owner (one per track) */
void qd_quad_owner_init(qd_quad_owner_t *o);
void qd_quad_owner_block(qd_quad_owner_t *o);
void qd_quad_owner_publish(qd_quad_owner_t *o);
void qd_quad_voice_prepare(qd_quad_voice_t *v);
int32_t qd_quad_bench(int k, int note, int nblocks);
void qd_quad_bench_start(int k, int note);
int32_t qd_quad_bench_block(int what);
uint32_t qd_quad_shared_sig(void);

/* ---- a voice */
void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx);
void qd_quad_voice_start(qd_quad_voice_t *v, const int16_t *patch, qd_quad_owner_t *o, int note, int vel);
void qd_quad_voice_release(qd_quad_voice_t *v);
void qd_quad_voice_kill(qd_quad_voice_t *v);
void qd_quad_voice_glide(qd_quad_voice_t *v, int from);
void qd_quad_voice_legato(qd_quad_voice_t *v, int note, int vel);
int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out);   /* one block added to out[0..31]; 1: rendered */
int32_t qd_quad_voice_level(const qd_quad_voice_t *v);
int qd_quad_voice_endless(const qd_quad_voice_t *v);

/* ---- DRIVE: FIZZ's (fz_voice.h fz_drive) on one voice's block */
typedef struct { int32_t hp, lp1, lp2; } qd_dist_t;
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n);

/* ---- patches */
void qd_quad_preset_patch(int k, int16_t *p);
const char *qd_quad_preset_name(int k);
int qd_quad_preset_kind(int k);         /* its kind (sounds.h QD_PK_*) */
const uint8_t *qd_quad_preset_sends(int k);   /* the sends it loads with: chorus, echo, room */
void qd_quad_pack(const int16_t *p, uint8_t *blob);
void qd_quad_pack5(const int16_t *p, uint8_t *blob);
int qd_quad_unpack(const uint8_t *blob, int n, int16_t *p);
int qd_quad_ext_used(const int16_t *p);
uint32_t qd_quad_srr_inc(int srr);
int qd_quad_crush_bits(int bits);
void qd_quad_algo(int a, uint8_t *mod4, int *fb, int *outx, int *outy);   /* (FM TONE's: nothing here) */
const char *qd_quad_value_text(int i, int v, char *buf);
int qd_quad_value_range(int i, int16_t *min, int16_t *max, int16_t *def);
const char *qd_quad_value_name(int i);
int qd_quad_dest_id(int dest);
int qd_quad_dest_of(int id, int lfo);
int qd_quad_ndest(void);
int qd_quad_owner_lfos(const qd_quad_owner_t *o, int16_t *w, uint8_t *dest);
int qd_quad_lfo_value(int id, int v, const int16_t *w, const uint8_t *dest);
const char *qd_quad_dest_name(int dest);
int qd_quad_dest_step(int dest, int d);
int qd_quad_dest_pos(int dest);
int qd_quad_mult_pos(int mult);           /* an LFO's MULT in the speed's order: FREE, 1/128 .. 8 BAR */
int qd_quad_mult_at(int pos);
int qd_quad_mult_step(int mult, int d);

/* VOX's vocabulary (eg_vox_data.h): the words' count and names */
int qd_vox_nwords(void);
const char *qd_vox_word_name(int w);    /* w 1..; 0: "OFF" */
const char *qd_vox_word_phon(int w);    /* w 1..: the word in SAM's phonetic alphabet (its render with another voice) */

#ifdef __cplusplus
}
#endif
