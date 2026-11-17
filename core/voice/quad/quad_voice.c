/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius's SID TONE (quad_voice.h): a voice is an emulated SID chip (../sid/sid.c), driven a control block at a
 * time the way C64 music drivers drive the real one: the registers written once a block (pitch, pulse width, the
 * filter, the gates), the chip clocked for the block's 32 samples. VOX writes the chip's volume register as a 4-bit
 * speech sample between those writes (the digi of the 1984 games), so the chip's tones under a word are modulated by
 * it, as they were. */
#include <stdint.h>
#include <string.h>

#include "quad_voice.h"
#define FZ_TABLES_DEFINE                /* (this unit holds the shared DSP tables: the mix and the effects read them) */
#include "fz_tables.h"
#include "fz_voice.h"                   /* (clamp, softclip, VOICE_FS, fz_drive) */
#include "../sid/eg_vox_data.h"         /* (the words: tools/eg/gen_vox.py) */
#include "../sam/say.h"                 /* (SAY: SAM's live words) */

#define BLK QD_QUAD_CTL
#define SR 44100
#define PAL_CLK 985248u

/* ---------------------------------------------------------------- the values: names, ranges, texts --- */
enum { VK_NUM, VK_SIGN, VK_C64, VK_LIST, VK_NOTE, VK_WORD, VK_ROUTE, VK_DEST, VK_SAY };
typedef struct {
    const char *name;
    int16_t min, max, def;
    uint8_t kind;
    const char *const *list;
} val_t;

static const char *const L_WAVE[QD_SW_N] = {"OFF", "TRI", "SAW", "PULSE", "NOISE", "T+S", "P+T", "P+S", "P+T+S"};
static const char *const L_MOD[4] = {"OFF", "SYNC", "RING", "S+R"};
static const char *const L_OFFON[2] = {"OFF", "ON"};
static const char *const L_FTYPE[QD_QF_N] = {"OFF", "LP", "BP", "HP", "NOTCH", "LP+BP", "BP+HP", "ALL"};
static const char *const L_CHIP[2] = {"6581", "8580"};
static const char *const L_PREPOST[2] = {"POST", "PRE"};
static const char *const L_BEFAFT[2] = {"BEFORE", "AFTER"};
static const char *const L_PORT[3] = {"OFF", "ON", "LEGATO"};
static const char *const L_PSLOPE[2] = {"TIME", "RATE"};
static const char *const L_VLOOP[3] = {"OFF", "FWD", "BOUNCE"};
static const char *const L_DHOLD[8] = {"2K", "3K", "4K", "6K", "8K", "11K", "16K", "22K"};
/* (1/64 and 1/128 after the bars: a sound's MULT is its place in this list, and the sounds saved before them keep
 * theirs; the knob turns through MULT_ORDER, the speed's order. At 120 BPM a 1/64 is 32 Hz and a 1/128 64 Hz: an ARP wave a note a division reaches the PAL frame's 50 Hz) */
static const char *const L_MULT[12] = {"FREE", "1/32", "1/16", "1/8", "1/4", "1/2", "1 BAR", "2 BAR", "4 BAR", "8 BAR",
                                       "1/64", "1/128"};
static const char *const L_LWAVE[QD_LW_N] = {"TRI", "SINE", "SAW", "SQR", "S&H", "EXP", "RAMP", "ARP OCT", "ARP 5TH",
                                             "ARP MAJ", "ARP MIN", "ARP SUS", "ARP 7", "ARP MI7", "ARP MA7",
                                             "ARP DIM"};   /* (seven letters: the editor's value) */
static const char *const L_LTRIG[QD_LT_N] = {"FREE", "TRIG", "HOLD", "ONE", "HALF"};
static const char *const L_MSRC[QD_MS_N] = {"OFF", "VEL", "KEY", "MW", "AT", "PB", "BR", "RAND", "F ENV"};
static const char *const L_VMODE[2] = {"POLY", "MONO"};
static const char *const L_PRIO[3] = {"LOW", "MID", "HIGH"};
static const char *const DEST_NAME[QD_D_N] = {"OFF", "PITCH", "CUTOFF", "RESO", "PW 1", "PW 2", "PW 3", "PW ALL",
                                              "PITCH 1", "PITCH 2", "PITCH 3", "LEVEL", "PAN", "DRIVE", "BITS",
                                              "SRR", "F ENV", "WORD POS", "VOX PITCH", "DETUNE"};
static const uint16_t HOLD_HZ[8] = {2000, 3000, 4000, 6000, 8000, 11025, 16000, 22050};

static val_t val(int i)
{
    static const val_t OSC[5] = {{"WAVE", 0, QD_SW_N - 1, QD_SW_SAW, VK_LIST, L_WAVE},
                                 {"COARSE", -24, 24, 0, VK_SIGN, 0},
                                 {"FINE", 0, 127, 64, VK_C64, 0},
                                 {"PW", 0, 127, 64, VK_NUM, 0},
                                 {"MOD", 0, 3, 0, VK_LIST, L_MOD}};
    static const val_t ENV[4] = {{"ATK", 0, 15, 0, VK_NUM, 0}, {"DEC", 0, 15, 8, VK_NUM, 0},
                                 {"SUS", 0, 15, 12, VK_NUM, 0}, {"REL", 0, 15, 5, VK_NUM, 0}};
    static const val_t LFO[8] = {{"SPEED", 0, 127, 60, VK_NUM, 0},
                                 {"MULT", 0, 11, 0, VK_LIST, L_MULT},
                                 {"FADE", 0, 127, 64, VK_C64, 0},
                                 {"DEST", 0, QD_D_N - 1, 0, VK_DEST, 0},
                                 {"WAVE", 0, QD_LW_N - 1, 0, VK_LIST, L_LWAVE},
                                 {"PHASE", 0, 127, 0, VK_NUM, 0},
                                 {"TRIG", 0, QD_LT_N - 1, 0, VK_LIST, L_LTRIG},
                                 {"DEPTH", 0, 127, 64, VK_C64, 0}};
    static const val_t MOD[3] = {{"SOURCE", 0, QD_MS_N - 1, 0, VK_LIST, L_MSRC},
                                 {"DEST", 0, QD_D_N - 1, 0, VK_DEST, 0},
                                 {"DEPTH", -127, 127, 0, VK_SIGN, 0}};
    static const val_t NONE = {"", 0, 0, 0, VK_NUM, 0};
    if (i < 0 || i >= QD_QUAD_NP)
        return NONE;
    if (i <= QD_QP_M3) {
        val_t v = OSC[i % 5];
        if (i >= QD_QP_W2 && i % 5 == 0)
            v.def = QD_SW_OFF;
        return v;
    }
    if (i >= QD_QP_LFO0 && i < QD_QP_MOD0)
        return LFO[(i - QD_QP_LFO0) % 8];
    if (i >= QD_QP_MOD0 && i < QD_QP_VMODE)
        return MOD[(i - QD_QP_MOD0) % 3];
    switch (i) {
    case QD_QP_DETUNE: return (val_t){"DETUNE", 0, 127, 0, VK_NUM, 0};
    case QD_QP_EATK: case QD_QP_E2A: case QD_QP_E3A: return ENV[0];
    case QD_QP_EDEC: case QD_QP_E2D: case QD_QP_E3D: return ENV[1];
    case QD_QP_ESUS: case QD_QP_E2S: case QD_QP_E3S: return ENV[2];
    case QD_QP_EREL: case QD_QP_E2R: case QD_QP_E3R: return ENV[3];
    case QD_QP_E2LINK: case QD_QP_E3LINK: return (val_t){"LINK", 0, 1, 1, VK_LIST, L_OFFON};
    case QD_QP_HR: return (val_t){"HR", 0, 127, 2, VK_NUM, 0};
    case QD_QP_FREQ: return (val_t){"CUTOFF", 0, 2047, 1024, VK_NUM, 0};
    case QD_QP_RESO: return (val_t){"RESO", 0, 15, 0, VK_NUM, 0};
    case QD_QP_FTYPE: return (val_t){"MODE", 0, QD_QF_N - 1, QD_QF_OFF, VK_LIST, L_FTYPE};
    case QD_QP_FROUTE: return (val_t){"ROUTE", 0, 15, 15, VK_ROUTE, 0};   /* (bits 0..2 the oscillators, 3 the word) */
    case QD_QP_FDEPTH: return (val_t){"DEPTH", 0, 127, 64, VK_C64, 0};
    case QD_QP_FKTRK: return (val_t){"KEYTRK", 0, 127, 0, VK_NUM, 0};
    case QD_QP_FATK: return (val_t){"F ATK", 0, 127, 0, VK_NUM, 0};
    case QD_QP_FDEC: return (val_t){"F DEC", 0, 127, 40, VK_NUM, 0};
    case QD_QP_FSUS: return (val_t){"F SUS", 0, 127, 0, VK_NUM, 0};
    case QD_QP_FREL: return (val_t){"F REL", 0, 127, 40, VK_NUM, 0};
    case QD_QP_CHIP: return (val_t){"CHIP", 0, 1, 0, VK_LIST, L_CHIP};
    case QD_QP_LEVEL: return (val_t){"LEVEL", 0, 127, 100, VK_NUM, 0};
    case QD_QP_VEL: return (val_t){"VEL", 0, 127, 64, VK_NUM, 0};
    case QD_QP_DRIVE: return (val_t){"DRIVE", 0, 127, 0, VK_NUM, 0};
    case QD_QP_DRPRE: return (val_t){"ROUTE", 0, 1, 0, VK_LIST, L_PREPOST};
    case QD_QP_BITS: return (val_t){"BITS", 0, 127, 0, VK_NUM, 0};
    case QD_QP_SRR: return (val_t){"SRR", 0, 127, 0, VK_NUM, 0};
    case QD_QP_CRPOST: return (val_t){"ROUTE", 0, 1, 1, VK_LIST, L_BEFAFT};
    case QD_QP_PORT: return (val_t){"PORT", 0, 2, 0, VK_LIST, L_PORT};
    case QD_QP_PTIM: return (val_t){"TIME", 0, 127, 40, VK_NUM, 0};
    case QD_QP_PSLOPE: return (val_t){"SLOPE", 0, 1, 0, VK_LIST, L_PSLOPE};
    case QD_QP_WORD: return (val_t){"WORD", 0, VOX_NWORDS, 0, VK_WORD, 0};
    case QD_QP_ROOT: return (val_t){"ROOT", 24, 96, 60, VK_NOTE, 0};
    case QD_QP_VSTART: return (val_t){"START", 0, 127, 0, VK_NUM, 0};
    case QD_QP_VEND: return (val_t){"END", 0, 127, 127, VK_NUM, 0};
    case QD_QP_VLOOP: return (val_t){"LOOP", 0, 2, 0, VK_LIST, L_VLOOP};
    case QD_QP_DBITS: return (val_t){"BITS", 1, 4, 4, VK_NUM, 0};
    case QD_QP_DHOLD: return (val_t){"RATE", 0, 7, 4, VK_LIST, L_DHOLD};
    case QD_QP_DBOOST: return (val_t){"BOOST", 0, 1, 0, VK_LIST, L_OFFON};
    case QD_QP_DLVL: return (val_t){"LEVEL", 0, 127, 127, VK_NUM, 0};
    case QD_QP_VMODE: return (val_t){"VOICE", 0, 1, 0, VK_LIST, L_VMODE};
    case QD_QP_UNI: return (val_t){"UNISON", 1, 8, 1, VK_NUM, 0};
    case QD_QP_UDET: return (val_t){"DETUNE", 0, 127, 20, VK_NUM, 0};
    case QD_QP_USPR: return (val_t){"SPREAD", 0, 127, 64, VK_NUM, 0};
    case QD_QP_VPRIO: return (val_t){"PRIO", 0, 2, 1, VK_LIST, L_PRIO};
    case QD_QP_SAY: return (val_t){"SAY", 0, QD_SAY_MAX, 0, VK_SAY, 0};
    case QD_QP_SSPD: return (val_t){"SPEED", 20, 200, QD_SAY_SPD0, VK_NUM, 0};
    case QD_QP_SPIT: return (val_t){"PITCH", 0, 255, QD_SAY_PIT0, VK_NUM, 0};
    case QD_QP_STHR: return (val_t){"THROAT", 0, 255, QD_SAY_THR0, VK_NUM, 0};
    case QD_QP_SMTH: return (val_t){"MOUTH", 0, 255, QD_SAY_MTH0, VK_NUM, 0};
    default: return NONE;
    }
}

int qd_quad_value_range(int i, int16_t *min, int16_t *max, int16_t *def)
{
    const val_t v = val(i);
    if (i < 0 || i >= QD_QUAD_NP)
        return 0;
    if (min)
        *min = v.min;
    if (max)
        *max = v.max;
    if (def)
        *def = v.def;
    return 1;
}

const char *qd_quad_value_name(int i) { return val(i).name; }

int qd_vox_nwords(void) { return (int)VOX_NWORDS; }
const char *qd_vox_word_name(int w) { return w >= 1 && w <= (int)VOX_NWORDS ? VOX_WORD_NAME[w - 1] : "OFF"; }
const char *qd_vox_word_phon(int w) { return w >= 1 && w <= (int)VOX_NWORDS ? VOX_WORD_PHON[w - 1] : ""; }
typedef char vox_fs_is_say_fs[VOX_FS == QD_VOX_FS ? 1 : -1];

static char *itoa_(char *b, int n)
{
    char t[12];
    int i = 0, k = 0;
    unsigned u = n < 0 ? 0u - (unsigned)n : (unsigned)n;
    do
        t[i++] = (char)('0' + u % 10u);
    while ((u /= 10u) && i < 11);
    if (n < 0)
        b[k++] = '-';
    while (i)
        b[k++] = t[--i];
    b[k] = 0;
    return b;
}

const char *qd_quad_value_text(int i, int v, char *buf)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const val_t d = val(i);
    buf[0] = 0;
    switch (d.kind) {                                   /* (always into buf: the view copies from it) */
    case VK_LIST:
        strcpy(buf, d.list && v >= d.min && v <= d.max ? d.list[v] : "?");
        return buf;
    case VK_DEST:
        strcpy(buf, v >= 0 && v < QD_D_N ? DEST_NAME[v] : "?");
        return buf;
    case VK_WORD:
        strncpy(buf, qd_vox_word_name(v), 11);
        buf[11] = 0;
        return buf;
    case VK_SAY:                                        /* (the word itself; a view shows eleven characters) */
        if (!qd_say_text(v, buf))
            strcpy(buf, "OFF");
        buf[11] = 0;
        return buf;
    case VK_SIGN:
        if (v > 0)
            buf[0] = '+', buf[1] = 0;
        itoa_(buf + (v > 0), v);
        return buf;
    case VK_C64:
        v -= 64;
        if (v > 0)
            buf[0] = '+', buf[1] = 0;
        itoa_(buf + (v > 0), v);
        return buf;
    case VK_NOTE: {
        char o[12];
        strcpy(buf, NN[(v < 0 ? 0 : v) % 12]);
        strcat(buf, itoa_(o, v / 12 - 1));
        return buf;
    }
    case VK_ROUTE: {
        int k = 0, n;
        for (n = 0; n < 3; n++)
            if (v >> n & 1)
                buf[k++] = (char)('1' + n);
        if (v & 8)                                      /* (the word: VOX's speech into the filter) */
            buf[k++] = 'V';
        if (!k)
            buf[k++] = '-';
        buf[k] = 0;
        return buf;
    }
    default:
        return itoa_(buf, v);
    }
}

int qd_quad_lockable(int id) { return id >= 0 && id < QD_QUAD_NP && (id < QD_QP_VMODE || id > QD_QP_VPRIO); }

/* ---------------------------------------------------------------- the presets --- */
typedef struct __attribute__((packed)) { uint8_t id; int16_t v; } kv_t;   /* (three bytes a value: the unit's flash) */
#define LF(k, f) QD_QP_LFO(k, QD_QL_##f)
static const kv_t P_SAWLEAD[] = {{QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 1100}, {QD_QP_RESO, 6}, {QD_QP_W2, QD_SW_SAW},
                                 {QD_QP_F2, 70}, {QD_QP_EDEC, 9}, {QD_QP_ESUS, 10}, {QD_QP_EREL, 6},
                                 {LF(0, DEST), QD_D_PITCH}, {LF(0, SPEED), 72}, {LF(0, DEPTH), 66}, {LF(0, FADE), 40},
                                 {LF(0, TRIG), QD_LT_TRIG}};
static const kv_t P_PWMBASS[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 40}, {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 500},
                                 {QD_QP_RESO, 10}, {QD_QP_FDEPTH, 100}, {QD_QP_FDEC, 30}, {QD_QP_EDEC, 8},
                                 {QD_QP_ESUS, 6}, {QD_QP_EREL, 4}, {LF(0, DEST), QD_D_PW1}, {LF(0, SPEED), 45},
                                 {LF(0, DEPTH), 84}};
static const kv_t P_SYNC[] = {{QD_QP_W1, QD_SW_SAW}, {QD_QP_C1, 12}, {QD_QP_M1, 1}, {QD_QP_W3, QD_SW_OFF},
                              {QD_QP_EDEC, 10}, {QD_QP_ESUS, 11}, {QD_QP_EREL, 8}, {LF(0, DEST), QD_D_P1},
                              {LF(0, WAVE), QD_LW_RAMP}, {LF(0, TRIG), QD_LT_ONE}, {LF(0, SPEED), 40}, {LF(0, DEPTH), 100}};
static const kv_t P_BELL[] = {{QD_QP_W1, QD_SW_TRI}, {QD_QP_M1, 2}, {QD_QP_C3, 19}, {QD_QP_EDEC, 10}, {QD_QP_ESUS, 0},
                              {QD_QP_EREL, 10}};
static const kv_t P_PAD[] = {{QD_QP_W2, QD_SW_SAW}, {QD_QP_F2, 72}, {QD_QP_DETUNE, 30}, {QD_QP_FTYPE, QD_QF_LP},
                             {QD_QP_FREQ, 300}, {QD_QP_RESO, 13}, {QD_QP_EATK, 10}, {QD_QP_EDEC, 8}, {QD_QP_ESUS, 12},
                             {QD_QP_EREL, 10}, {LF(0, DEST), QD_D_CUT}, {LF(0, SPEED), 30}, {LF(0, DEPTH), 80}};
static const kv_t P_ORGAN[] = {{QD_QP_W1, QD_SW_PS}, {QD_QP_W2, QD_SW_TRI}, {QD_QP_C2, 12}, {QD_QP_EDEC, 0},
                               {QD_QP_ESUS, 15}, {QD_QP_EREL, 4}};
static const kv_t P_BPSWEEP[] = {{QD_QP_FTYPE, QD_QF_BP}, {QD_QP_RESO, 15}, {QD_QP_FREQ, 300}, {QD_QP_FDEPTH, 114},
                                 {QD_QP_FATK, 60}, {QD_QP_FDEC, 80}, {QD_QP_FSUS, 0}, {QD_QP_EDEC, 9}, {QD_QP_ESUS, 9}};
static const kv_t P_ZAP[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 30}, {QD_QP_EDEC, 9}, {QD_QP_ESUS, 0}, {QD_QP_EREL, 8},
                             {LF(0, DEST), QD_D_PITCH}, {LF(0, WAVE), QD_LW_EXP}, {LF(0, TRIG), QD_LT_ONE},
                             {LF(0, SPEED), 66}, {LF(0, DEPTH), 127}};
static const kv_t P_BUZZ[] = {{QD_QP_W2, QD_SW_PUL}, {QD_QP_C2, -12}, {QD_QP_PW2, 30}, {QD_QP_FTYPE, QD_QF_LP},
                              {QD_QP_FREQ, 400}, {QD_QP_RESO, 8}, {QD_QP_FDEPTH, 110}, {QD_QP_FDEC, 24},
                              {QD_QP_EDEC, 7}, {QD_QP_ESUS, 8}, {QD_QP_EREL, 3}};
static const kv_t P_CHOIR[] = {{QD_QP_W1, QD_SW_TRI}, {QD_QP_W2, QD_SW_TRI}, {QD_QP_W3, QD_SW_TRI}, {QD_QP_C3, 12},
                               {QD_QP_DETUNE, 60}, {QD_QP_EATK, 8}, {QD_QP_ESUS, 13}, {QD_QP_EREL, 9},
                               {LF(0, DEST), QD_D_PITCH}, {LF(0, SPEED), 66}, {LF(0, DEPTH), 66}};
static const kv_t P_WOBBLE[] = {{QD_QP_W2, QD_SW_SAW}, {QD_QP_C2, -12}, {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 350},
                                {QD_QP_RESO, 12}, {QD_QP_ESUS, 14}, {LF(0, DEST), QD_D_CUT}, {LF(0, MULT), 3},
                                {LF(0, DEPTH), 110}, {LF(0, TRIG), QD_LT_TRIG}, {QD_QP_LEVEL, 85}};
static const kv_t P_WIND[] = {{QD_QP_W1, QD_SW_NOI}, {QD_QP_FTYPE, QD_QF_BP}, {QD_QP_FREQ, 600}, {QD_QP_RESO, 14},
                              {QD_QP_EATK, 9}, {QD_QP_ESUS, 14}, {QD_QP_EREL, 11}, {LF(0, DEST), QD_D_CUT},
                              {LF(0, SPEED), 25}, {LF(0, DEPTH), 110}};
static const kv_t P_INTRUDER[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_WORD, 1}, {QD_QP_EREL, 0}};
static const kv_t P_ALERT[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_C1, -24}, {QD_QP_PW1, 20}, {QD_QP_ESUS, 15},
                               {QD_QP_WORD, 2}, {QD_QP_FTYPE, QD_QF_LP}, {QD_QP_FREQ, 600}, {QD_QP_FROUTE, 1}};
static const kv_t P_COUNT[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_WORD, 27}, {QD_QP_DHOLD, 3}, {QD_QP_EREL, 0}};
static const kv_t P_LAUGH[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_WORD, 44}, {QD_QP_VLOOP, 1}, {QD_QP_DBITS, 3},
                               {QD_QP_EREL, 0}, {LF(0, DEST), QD_D_VOXP}, {LF(0, SPEED), 60}, {LF(0, DEPTH), 68}};
/* ---- 0.2: the second set. Written with these (an oscillator's wave and coarse; the three envelopes, 2 and 3 with
 * their LINK off: an oscillator's own level and length; the filter and its envelope; an LFO by SPEED or by MULT; a
 * modulation slot). An ARP wave's SPEED 127 is a note a PAL frame, 119 two, 111 three; its DEPTH 65 the chord up, 63
 * down, +-30 two octaves, +-50 three */
#define OSC(n, w, c) {QD_QP_W1 + 5 * ((n) - 1), QD_SW_##w}, {QD_QP_C1 + 5 * ((n) - 1), c}
#define AMP(a, d, s_, r) {QD_QP_EATK, a}, {QD_QP_EDEC, d}, {QD_QP_ESUS, s_}, {QD_QP_EREL, r}
#define EN2(a, d, s_, r) {QD_QP_E2LINK, 0}, {QD_QP_E2A, a}, {QD_QP_E2D, d}, {QD_QP_E2S, s_}, {QD_QP_E2R, r}
#define EN3(a, d, s_, r) {QD_QP_E3LINK, 0}, {QD_QP_E3A, a}, {QD_QP_E3D, d}, {QD_QP_E3S, s_}, {QD_QP_E3R, r}
#define FLT(t, f, r) {QD_QP_FTYPE, QD_QF_##t}, {QD_QP_FREQ, f}, {QD_QP_RESO, r}
#define FEN(a, d, s_, r) {QD_QP_FATK, a}, {QD_QP_FDEC, d}, {QD_QP_FSUS, s_}, {QD_QP_FREL, r}
#define LFO(k, dst, wav, trg, spd, dep)                                                                               \
    {LF(k, DEST), QD_D_##dst}, {LF(k, WAVE), QD_LW_##wav}, {LF(k, TRIG), QD_LT_##trg}, {LF(k, SPEED), spd},           \
        {LF(k, DEPTH), dep}
#define LFM(k, dst, wav, trg, mul, dep)                                                                               \
    {LF(k, DEST), QD_D_##dst}, {LF(k, WAVE), QD_LW_##wav}, {LF(k, TRIG), QD_LT_##trg}, {LF(k, MULT), mul},            \
        {LF(k, DEPTH), dep}
#define MOD(k, src, dst, dep)                                                                                         \
    {QD_QP_MOD(k, QD_QM_SRC), QD_MS_##src}, {QD_QP_MOD(k, QD_QM_DST), QD_D_##dst}, {QD_QP_MOD(k, QD_QM_DEPTH), dep}
#define VIB(k) LFO(k, PITCH, SINE, TRIG, 86, 65), {LF(k, FADE), 6}   /* a vibrato that comes in */
#define WORD(w) {QD_QP_W1, QD_SW_OFF}, {QD_QP_WORD, w}, {QD_QP_EREL, 0}
/* BASS */
static const kv_t N_BUNKER[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 50}, LFO(0, PW1, TRI, FREE, 52, 76), FLT(LP, 640, 8),
                                MOD(0, FENV, CUT, 28), FEN(0, 26, 0, 20), AMP(0, 8, 7, 3), {QD_QP_LEVEL, 110}};
static const kv_t N_ACID[] = {FLT(LP, 560, 15), MOD(0, FENV, CUT, 36), MOD(1, VEL, CUT, 24), FEN(0, 22, 0, 16),
                              AMP(0, 9, 9, 2), {QD_QP_VMODE, 1}, {QD_QP_PORT, QD_PORT_LEGATO}, {QD_QP_PTIM, 30}};
static const kv_t N_TRISUB[] = {OSC(1, TRI, 0), OSC(2, SAW, 0), EN2(0, 5, 0, 0), FLT(LP, 700, 2), {QD_QP_FROUTE, 2},
                                AMP(0, 8, 13, 4), {QD_QP_LEVEL, 115}};
static const kv_t N_OCTRUN[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 34}, LFM(0, PITCH, AOCT, TRIG, 2, 65),
                                FLT(LP, 760, 6), MOD(0, FENV, CUT, 20), FEN(0, 30, 20, 20), AMP(0, 8, 10, 3)};
static const kv_t N_RUBBER[] = {{QD_QP_CHIP, 1}, {QD_QP_W1, QD_SW_PS}, {QD_QP_PW1, 70}, FLT(LP, 90, 11),
                                MOD(0, FENV, CUT, 22), FEN(0, 18, 0, 14), AMP(0, 7, 8, 3), {QD_QP_LEVEL, 120}};
static const kv_t N_SYNCBASS[] = {OSC(1, SAW, 7), {QD_QP_M1, 1}, LFO(0, P1, EXP, ONE, 70, 96), FLT(LP, 900, 4),
                                  AMP(0, 8, 9, 3), {QD_QP_LEVEL, 127}};
static const kv_t N_TRIPLE[] = {OSC(2, SAW, 0), OSC(3, SAW, -12), {QD_QP_DETUNE, 46}, FLT(LP, 720, 5),
                                MOD(0, FENV, CUT, 18), FEN(0, 34, 30, 24), AMP(0, 9, 11, 4), {QD_QP_LEVEL, 75}};
static const kv_t N_DIRTY[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 50}, OSC(2, PUL, -12), {QD_QP_PW2, 20},
                               {QD_QP_DRIVE, 70}, {QD_QP_BITS, 30}, FLT(LP, 820, 10), MOD(0, FENV, CUT, 24),
                               FEN(0, 24, 10, 16), AMP(0, 7, 9, 2), {QD_QP_LEVEL, 75}};
static const kv_t N_FIFTH[] = {LFO(0, PITCH, A5TH, TRIG, 127, 65), {QD_QP_DRIVE, 40}, FLT(LP, 800, 6),
                               AMP(0, 8, 10, 3), {QD_QP_LEVEL, 120}};
/* LEAD */
static const kv_t N_HERO[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 40}, LFO(0, PW1, TRI, FREE, 58, 80), VIB(1),
                              AMP(0, 7, 11, 5)};
static const kv_t N_GLIDE[] = {OSC(2, SAW, 0), {QD_QP_DETUNE, 24}, FLT(LP, 1250, 5), {QD_QP_VMODE, 1},
                               {QD_QP_PORT, QD_PORT_ON}, {QD_QP_PTIM, 45}, VIB(0), AMP(0, 8, 12, 4), {QD_QP_LEVEL, 90}};
static const kv_t N_SCREAM[] = {OSC(1, SAW, 12), {QD_QP_M1, 1}, LFO(0, P1, TRI, FREE, 38, 88), MOD(0, MW, P1, 60),
                                AMP(0, 8, 12, 6), {QD_QP_LEVEL, 115}};
static const kv_t N_RINGSOLO[] = {{QD_QP_W1, QD_SW_TRI}, {QD_QP_M1, 2}, {QD_QP_C3, 7}, OSC(2, PUL, 0), {QD_QP_PW2, 30},
                                  EN2(0, 8, 7, 5), VIB(0), AMP(0, 8, 13, 5)};
static const kv_t N_FLUTE[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, NOI, 24), EN2(0, 3, 0, 0), VIB(0), AMP(3, 8, 12, 5),
                               {QD_QP_LEVEL, 115}};
static const kv_t N_BRASS[] = {{QD_QP_CHIP, 1}, OSC(2, SAW, 0), {QD_QP_DETUNE, 14}, FLT(LP, 60, 3),
                               MOD(0, FENV, CUT, 40), FEN(34, 50, 70, 30), AMP(2, 8, 12, 5), {QD_QP_LEVEL, 90}};
static const kv_t N_POWER5[] = {OSC(2, SAW, 7), OSC(3, PUL, -12), {QD_QP_PW3, 40}, {QD_QP_DETUNE, 10},
                                FLT(LP, 1150, 4), {QD_QP_DRIVE, 30}, AMP(0, 8, 12, 5), {QD_QP_LEVEL, 70}};
static const kv_t N_SQUELCH[] = {{QD_QP_W1, QD_SW_PUL}, MOD(0, FENV, PW1, -55), FEN(0, 36, 0, 30), VIB(0),
                                 AMP(0, 8, 12, 5)};
static const kv_t N_MAJ50[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 48}, LFO(0, PITCH, AMAJ, TRIG, 127, 65),
                               LFO(1, PW1, TRI, FREE, 50, 76), AMP(0, 8, 11, 5)};
static const kv_t N_MIN50[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 30}, LFO(0, PITCH, AMIN, TRIG, 127, 65),
                               LFO(1, PW1, TRI, FREE, 44, 74), AMP(0, 8, 11, 5)};
static const kv_t N_DUET[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, PUL, 12), {QD_QP_PW2, 28}, LFM(0, P2, AMAJ, TRIG, 2, 65),
                              EN2(0, 8, 9, 5), AMP(0, 8, 12, 5)};
/* KEYS, PLUCK */
static const kv_t N_CLAVI[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 10}, FLT(LP, 900, 6), MOD(0, FENV, CUT, 40),
                               MOD(1, VEL, CUT, 20), FEN(0, 24, 0, 18), AMP(0, 7, 0, 4), {QD_QP_LEVEL, 120}};
static const kv_t N_MARIMBA[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, TRI, 24), EN2(0, 2, 0, 1), AMP(0, 6, 0, 6),
                                 {QD_QP_VEL, 90}, {QD_QP_LEVEL, 120}};
static const kv_t N_HARPSI[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 8}, OSC(2, SAW, 12), EN2(0, 7, 0, 5),
                                AMP(0, 9, 0, 6), {QD_QP_LEVEL, 95}};
static const kv_t N_EPIANO[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, PUL, 0), {QD_QP_PW2, 24}, EN2(0, 7, 2, 6),
                                {QD_QP_DETUNE, 12}, LFO(0, PAN, SINE, FREE, 62, 84), AMP(0, 10, 3, 7), {QD_QP_VEL, 100},
                                {QD_QP_LEVEL, 110}};
static const kv_t N_FPLUCK[] = {OSC(2, PUL, 0), {QD_QP_PW2, 50}, {QD_QP_DETUNE, 18}, FLT(LP, 480, 10),
                                MOD(0, FENV, CUT, 55), FEN(0, 22, 0, 22), AMP(0, 8, 0, 7), {QD_QP_LEVEL, 85}};
static const kv_t N_CHIME[] = {OSC(1, TRI, 12), {QD_QP_M1, 2}, {QD_QP_C3, 26}, OSC(2, TRI, 24), EN2(0, 5, 0, 5),
                               AMP(0, 10, 0, 10), {QD_QP_LEVEL, 115}};
static const kv_t N_MBOX[] = {OSC(1, TRI, 12), OSC(2, TRI, 31), EN2(0, 4, 0, 3), AMP(0, 8, 0, 8), {QD_QP_VEL, 100},
                              {QD_QP_LEVEL, 120}};
static const kv_t N_SYNCPL[] = {OSC(1, PUL, 5), {QD_QP_PW1, 40}, {QD_QP_M1, 1}, LFO(0, P1, EXP, ONE, 74, 104),
                                AMP(0, 7, 0, 6)};
static const kv_t N_LADDER[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 30}, LFM(0, PITCH, AOCT, TRIG, 1, 114),
                                FLT(LP, 1300, 6), AMP(0, 9, 0, 8)};
static const kv_t N_CASCADE[] = {OSC(2, PUL, 12), {QD_QP_PW2, 30}, EN2(0, 6, 0, 4), LFM(0, PITCH, AMIN7, TRIG, 2, 34),
                                 FLT(LP, 1100, 7), MOD(0, FENV, CUT, 25), FEN(0, 45, 20, 30), AMP(0, 10, 6, 8), {QD_QP_LEVEL, 85}};
static const kv_t N_DOM7[] = {OSC(2, SAW, 0), {QD_QP_DETUNE, 20}, LFO(0, PITCH, ADOM7, TRIG, 127, 65),
                              FLT(LP, 1000, 8), MOD(0, FENV, CUT, 35), FEN(0, 30, 0, 20), AMP(0, 7, 0, 6), {QD_QP_LEVEL, 90}};
/* PAD */
static const kv_t N_STRINGS[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 40}, OSC(2, PUL, 0), {QD_QP_PW2, 70},
                                 OSC(3, PUL, 12), {QD_QP_PW3, 30}, {QD_QP_DETUNE, 40}, LFO(0, PWALL, TRI, FREE, 48, 78),
                                 LFO(1, PW2, SINE, FREE, 37, 74), FLT(LP, 1350, 2), AMP(9, 8, 13, 10),
                                 {QD_QP_LEVEL, 55}};
static const kv_t N_CATHEDRAL[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, SAW, -12), OSC(3, PUL, 12), {QD_QP_DETUNE, 20},
                                   EN2(10, 8, 10, 10), FLT(LP, 1000, 4), LFO(0, CUT, SINE, FREE, 28, 72),
                                   AMP(10, 8, 14, 11), {QD_QP_LEVEL, 75}};
static const kv_t N_GLASSPAD[] = {{QD_QP_W1, QD_SW_TRI}, {QD_QP_M1, 2}, {QD_QP_C3, 12}, OSC(2, TRI, 0), {QD_QP_F2, 70},
                                  LFO(0, P3, SINE, FREE, 25, 65), AMP(9, 8, 13, 11)};
static const kv_t N_NOTCH[] = {OSC(2, SAW, 0), OSC(3, SAW, 0), {QD_QP_DETUNE, 60}, FLT(NOTCH, 1000, 8),
                               LFO(0, CUT, TRI, FREE, 24, 90), AMP(8, 8, 13, 10), {QD_QP_LEVEL, 75}};
static const kv_t N_HOLLOW[] = {{QD_QP_W1, QD_SW_PUL}, OSC(2, TRI, -12), OSC(3, PUL, 0), {QD_QP_PW3, 56},
                                {QD_QP_DETUNE, 25}, FLT(BP, 1100, 3), {QD_QP_FROUTE, 5}, LFO(0, PAN, SINE, FREE, 40, 90),
                                AMP(9, 8, 13, 10), {QD_QP_LEVEL, 90}};
static const kv_t N_DARK[] = {OSC(1, SAW, -12), OSC(2, SAW, -12), OSC(3, PUL, -24), {QD_QP_PW3, 50},
                              {QD_QP_DETUNE, 35}, FLT(LP, 420, 11), MOD(0, FENV, CUT, 40), FEN(100, 90, 60, 70),
                              AMP(11, 8, 14, 11), {QD_QP_LEVEL, 75}};
static const kv_t N_SHIMMER[] = {OSC(1, TRI, 12), OSC(2, PUL, 0), {QD_QP_PW2, 40}, LFO(0, P1, ASUS, FREE, 111, 94),
                                 LFO(1, PW2, TRI, FREE, 45, 78), AMP(9, 8, 13, 10), {QD_QP_LEVEL, 85}};
static const kv_t N_DREAM[] = {{QD_QP_W1, QD_SW_TRI}, OSC(2, TRI, 12), EN2(8, 8, 8, 10),
                               LFM(0, PITCH, AMAJ7, FREE, 2, 65), AMP(6, 9, 12, 10), {QD_QP_LEVEL, 110}};
/* DRUMS (made to be played at C4) */
static const kv_t N_SQKICK[] = {OSC(1, PUL, -24), LFO(0, PITCH, EXP, ONE, 76, 112), OSC(2, NOI, 12), EN2(0, 1, 0, 0),
                                AMP(0, 7, 0, 5), {QD_QP_LEVEL, 120}, {QD_QP_HR, 3}};
static const kv_t N_TRITOM[] = {OSC(1, TRI, -12), LFO(0, PITCH, EXP, ONE, 70, 84), OSC(2, NOI, 0), EN2(0, 2, 0, 1),
                                AMP(0, 8, 0, 7), {QD_QP_LEVEL, 127}, {QD_QP_HR, 3}};
static const kv_t N_SYNTOM[] = {OSC(1, PUL, -12), LFO(0, PITCH, EXP, ONE, 62, 100), LFO(1, PW1, RAMP, ONE, 62, 90),
                                AMP(0, 8, 0, 8), {QD_QP_HR, 3}};
static const kv_t N_PSNARE[] = {OSC(1, NOI, 18), OSC(2, PUL, -5), EN2(0, 4, 0, 3), LFO(0, P2, EXP, ONE, 80, 96),
                                FLT(HP, 600, 0), {QD_QP_FROUTE, 1}, AMP(0, 6, 0, 6), {QD_QP_LEVEL, 115}, {QD_QP_HR, 3}};
static const kv_t N_RIM[] = {OSC(1, TRI, 19), {QD_QP_M1, 2}, {QD_QP_C3, 24}, {QD_QP_F3, 100}, OSC(2, PUL, 12),
                             EN2(0, 1, 0, 1), AMP(0, 3, 0, 3), {QD_QP_LEVEL, 127}, {QD_QP_HR, 3}};
static const kv_t N_COWBELL[] = {OSC(1, PUL, 14), OSC(2, PUL, 20), {QD_QP_F2, 80}, FLT(BP, 1050, 6), AMP(0, 6, 0, 6),
                                 {QD_QP_LEVEL, 115}, {QD_QP_HR, 3}};
static const kv_t N_MHAT[] = {OSC(1, TRI, 29), {QD_QP_M1, 2}, {QD_QP_C3, 22}, {QD_QP_F3, 100}, OSC(2, NOI, 24),
                              EN2(0, 2, 0, 2), FLT(HP, 1700, 4), AMP(0, 3, 0, 2), {QD_QP_LEVEL, 120}, {QD_QP_HR, 3}};
static const kv_t N_SHAKER[] = {OSC(1, NOI, 24), FLT(HP, 1500, 3), AMP(3, 3, 0, 3), {QD_QP_LEVEL, 110}};
static const kv_t N_CRASH[] = {OSC(1, NOI, 24), OSC(2, TRI, 23), {QD_QP_M2, 2}, EN2(0, 9, 0, 9), FLT(HP, 1400, 5),
                               AMP(0, 10, 0, 10), {QD_QP_HR, 3}};
static const kv_t N_BLIP[] = {OSC(1, PUL, 24), AMP(0, 2, 0, 1), {QD_QP_HR, 3}, {QD_QP_LEVEL, 120}};
/* FX */
static const kv_t N_LASER[] = {OSC(1, PUL, 12), {QD_QP_PW1, 30}, LFO(0, PITCH, SAW, TRIG, 104, 112), AMP(0, 8, 10, 6)};
static const kv_t N_SIREN[] = {{QD_QP_W1, QD_SW_PUL}, {QD_QP_PW1, 50}, OSC(2, PUL, 0), {QD_QP_F2, 72},
                               LFO(0, PITCH, TRI, FREE, 52, 76), FLT(LP, 1200, 8), AMP(2, 8, 14, 6), {QD_QP_LEVEL, 50}};
static const kv_t N_DATABANK[] = {OSC(1, PUL, 12), LFM(0, PITCH, SH, TRIG, 1, 104), LFM(1, PW1, SH, TRIG, 2, 90),
                                  AMP(0, 8, 13, 2)};
static const kv_t N_EXPLODE[] = {OSC(1, NOI, -6), OSC(2, NOI, 6), LFO(0, PITCH, EXP, ONE, 48, 100), FLT(LP, 500, 6),
                                 LFO(1, CUT, EXP, ONE, 44, 110), {QD_QP_DRIVE, 60}, AMP(0, 11, 0, 11),
                                 {QD_QP_LEVEL, 105}};
static const kv_t N_RISER[] = {OSC(1, NOI, 12), OSC(2, SAW, 0), OSC(3, SAW, 0), {QD_QP_DETUNE, 80}, FLT(BP, 1000, 13),
                               LFO(0, CUT, RAMP, ONE, 30, 96), LFO(1, PITCH, RAMP, ONE, 30, 96), AMP(8, 8, 15, 5), {QD_QP_LEVEL, 85}};
static const kv_t N_PWRDOWN[] = {OSC(2, PUL, -12), {QD_QP_PW2, 50}, LFO(0, PITCH, SAW, ONE, 36, 124),
                                 LFO(1, SRR, RAMP, ONE, 36, 100), AMP(0, 8, 13, 6), {QD_QP_LEVEL, 80}};
static const kv_t N_STATIC[] = {OSC(1, NOI, 24), FLT(BP, 1300, 12), LFO(0, CUT, SH, FREE, 100, 100),
                                LFO(1, LEVEL, SH, FREE, 108, 90), AMP(0, 8, 14, 3)};
static const kv_t N_MODEM[] = {{QD_QP_W1, QD_SW_PUL}, OSC(2, PUL, 7), LFM(0, P1, SH, TRIG, 1, 90),
                               LFM(1, P2, SH, TRIG, 1, 84), {LF(1, PHASE), 64}, {QD_QP_BITS, 40}, AMP(0, 8, 14, 2), {QD_QP_LEVEL, 60}};
static const kv_t N_PANIC[] = {OSC(1, PUL, 12), {QD_QP_PW1, 20}, LFO(0, PITCH, ADIM, TRIG, 119, 14), AMP(0, 8, 13, 3)};
static const kv_t N_LIFTUP[] = {OSC(1, PUL, -12), {QD_QP_PW1, 40}, LFO(0, PITCH, A5TH, ONE, 111, 114),
                                AMP(0, 8, 12, 4)};
static const kv_t N_LIFTDN[] = {OSC(1, PUL, -12), {QD_QP_PW1, 40}, LFO(0, PITCH, A5TH, ONE, 111, 14),
                                AMP(0, 8, 12, 4)};
/* VOX */
static const kv_t N_EVIL[] = {WORD(42)};
static const kv_t N_GENIUS[] = {WORD(43)};
static const kv_t N_DESTROY[] = {WORD(21), FLT(LP, 900, 10), {QD_QP_DRIVE, 50}, {QD_QP_DBITS, 3}};
static const kv_t N_CHANT[] = {WORD(29), {QD_QP_VLOOP, 2}, {QD_QP_DBITS, 2}, {QD_QP_VSTART, 10}, {QD_QP_VEND, 70},
                               LFM(0, VOXP, SQR, TRIG, 3, 76)};
static const kv_t N_OBEY[] = {OSC(1, PUL, -24), {QD_QP_PW1, 20}, {QD_QP_ESUS, 15}, {QD_QP_WORD, 51}, FLT(LP, 700, 8),
                              {QD_QP_FROUTE, 9}, {QD_QP_LEVEL, 85}};
static const kv_t N_FOOLS[] = {WORD(50), {QD_QP_ROOT, 72}, {QD_QP_DHOLD, 3}};
static const kv_t N_VOWEL[] = {WORD(47), {QD_QP_VSTART, 38}, {QD_QP_VEND, 46}, {QD_QP_VLOOP, 1},
                               LFO(0, WPOS, TRI, FREE, 40, 80), FLT(BP, 1100, 8)};
static const kv_t N_SAMSAYS[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_SAY, 77}, {QD_QP_EREL, 0}};
static const kv_t N_DEEPSAM[] = {{QD_QP_W1, QD_SW_OFF}, {QD_QP_SAY, 303}, {QD_QP_EREL, 0}, {QD_QP_SPIT, 96},
                                 {QD_QP_STHR, 190}, {QD_QP_SSPD, 90}};
static const kv_t N_GLITCH[] = {WORD(20), {QD_QP_VLOOP, 1}, {QD_QP_DBITS, 2}, LFM(0, WPOS, SH, TRIG, 2, 110)};
static const kv_t N_WELCOME[] = {WORD(45)};
static const kv_t N_DENIED[] = {WORD(15), {QD_QP_ROOT, 67}, FLT(BP, 1000, 9)};
#undef LF
/* a preset: its values over the defaults, its kind (sounds.h's QD_PK_*: 0 BASS, KEYS, PLUCK, PAD, LEAD, DRUMS, 6 FX)
 * and the sends it loads with (chorus, echo, room) */
enum { K_BASS, K_KEYS, K_PLUCK, K_PAD, K_LEAD, K_DRUMS, K_FX };
static const struct {
    const char *name;
    const kv_t *kv;
    uint8_t n, kind, send[3];
} PRESET[QD_QUAD_NPRESET] = {
#define P(nm, a, kind, c, d, r) {nm, a, (uint8_t)(sizeof(a) / sizeof(a[0])), kind, {c, d, r}}
    {"INIT", 0, 0, K_LEAD, {0, 0, 20}},
    /* (1..16: the first set; their sends as 0.1 had them) */
    P("SAW LEAD", P_SAWLEAD, K_LEAD, 30, 20, 40), P("PWM BASS", P_PWMBASS, K_BASS, 20, 35, 60),
    P("SYNC LEAD", P_SYNC, K_LEAD, 0, 10, 10), P("RING BELL", P_BELL, K_KEYS, 20, 30, 40),
    P("RESO PAD", P_PAD, K_PAD, 25, 20, 40), P("P+S ORGAN", P_ORGAN, K_KEYS, 50, 25, 70),
    P("BP SWEEP", P_BPSWEEP, K_LEAD, 35, 25, 50), P("ZAP", P_ZAP, K_DRUMS, 25, 40, 40),
    P("BUZZ BASS", P_BUZZ, K_BASS, 20, 30, 50), P("CHIP CHOIR", P_CHOIR, K_PAD, 0, 15, 10),
    P("WOBBLE", P_WOBBLE, K_BASS, 15, 20, 25), P("WIND", P_WIND, K_FX, 55, 15, 65),
    P("INTRUDER", P_INTRUDER, K_FX, 15, 25, 40), P("ALERT", P_ALERT, K_FX, 50, 30, 80),
    P("COUNTDOWN", P_COUNT, K_FX, 25, 30, 40), P("HA HA HA", P_LAUGH, K_FX, 20, 30, 50),
    /* (17..: the second set) */
    P("BUNKER BASS", N_BUNKER, K_BASS, 0, 0, 10), P("ACID 6581", N_ACID, K_BASS, 0, 20, 10),
    P("TRI SUB", N_TRISUB, K_BASS, 0, 0, 5), P("OCTAVE RUN", N_OCTRUN, K_BASS, 0, 15, 10),
    P("RUBBER 8580", N_RUBBER, K_BASS, 0, 0, 10), P("SYNC BASS", N_SYNCBASS, K_BASS, 0, 10, 10),
    P("TRIPLE SAW", N_TRIPLE, K_BASS, 20, 0, 15), P("DIRTY PULSE", N_DIRTY, K_BASS, 0, 0, 10),
    P("FIFTH DRIVE", N_FIFTH, K_BASS, 0, 10, 15),
    P("HERO LEAD", N_HERO, K_LEAD, 10, 35, 25), P("GLIDE SAW", N_GLIDE, K_LEAD, 15, 30, 25),
    P("SCREAMER", N_SCREAM, K_LEAD, 0, 30, 20), P("RING SOLO", N_RINGSOLO, K_LEAD, 10, 30, 30),
    P("TRI FLUTE", N_FLUTE, K_LEAD, 10, 20, 45), P("BRASS 8580", N_BRASS, K_LEAD, 20, 15, 35),
    P("POWER FIFTH", N_POWER5, K_LEAD, 10, 25, 25), P("PW SQUELCH", N_SQUELCH, K_LEAD, 10, 30, 20),
    P("MAJOR 50HZ", N_MAJ50, K_LEAD, 0, 30, 25), P("MINOR 50HZ", N_MIN50, K_LEAD, 0, 30, 25),
    P("DUET ARP", N_DUET, K_LEAD, 15, 35, 30),
    P("CLAVI PULSE", N_CLAVI, K_KEYS, 10, 15, 15), P("TRI MARIMBA", N_MARIMBA, K_PLUCK, 0, 15, 35),
    P("HARPSI", N_HARPSI, K_KEYS, 10, 10, 35), P("E PIANO", N_EPIANO, K_KEYS, 35, 10, 30),
    P("FILTER PLUCK", N_FPLUCK, K_PLUCK, 20, 35, 30), P("GLASS CHIME", N_CHIME, K_KEYS, 20, 30, 55),
    P("MUSIC BOX", N_MBOX, K_PLUCK, 0, 25, 50), P("SYNC PLUCK", N_SYNCPL, K_PLUCK, 10, 40, 25),
    P("OCT LADDER", N_LADDER, K_PLUCK, 20, 40, 30), P("MIN7 CASCADE", N_CASCADE, K_KEYS, 20, 35, 40),
    P("DOM7 STAB", N_DOM7, K_PLUCK, 15, 30, 30),
    P("PWM STRINGS", N_STRINGS, K_PAD, 60, 20, 55), P("CATHEDRAL", N_CATHEDRAL, K_PAD, 40, 20, 80),
    P("GLASS PAD", N_GLASSPAD, K_PAD, 50, 30, 70), P("NOTCH DRIFT", N_NOTCH, K_PAD, 30, 25, 50),
    P("HOLLOW PAD", N_HOLLOW, K_PAD, 40, 20, 60), P("DARK TOWER", N_DARK, K_PAD, 20, 20, 70),
    P("SUS SHIMMER", N_SHIMMER, K_PAD, 40, 30, 60), P("MAJ7 DREAM", N_DREAM, K_PAD, 30, 45, 60),
    P("SQUARE KICK", N_SQKICK, K_DRUMS, 0, 0, 5), P("TRI TOM", N_TRITOM, K_DRUMS, 0, 0, 20),
    P("SYN TOM", N_SYNTOM, K_DRUMS, 0, 10, 25), P("PULSE SNARE", N_PSNARE, K_DRUMS, 0, 0, 20),
    P("RIMSHOT", N_RIM, K_DRUMS, 0, 0, 15), P("COWBELL", N_COWBELL, K_DRUMS, 0, 0, 15),
    P("METAL HAT", N_MHAT, K_DRUMS, 0, 0, 5), P("SHAKER", N_SHAKER, K_DRUMS, 0, 0, 10),
    P("CRASH", N_CRASH, K_DRUMS, 0, 0, 35), P("BLIP", N_BLIP, K_DRUMS, 0, 20, 15),
    P("LASER GUN", N_LASER, K_FX, 0, 35, 25), P("TOWER SIREN", N_SIREN, K_FX, 20, 20, 40),
    P("DATA BANK", N_DATABANK, K_FX, 0, 30, 25), P("EXPLOSION", N_EXPLODE, K_FX, 0, 10, 45),
    P("RISER", N_RISER, K_FX, 20, 30, 50), P("POWER DOWN", N_PWRDOWN, K_FX, 0, 20, 30),
    P("RADIO STATIC", N_STATIC, K_FX, 0, 10, 20), P("MODEM", N_MODEM, K_FX, 0, 15, 15),
    P("DIM PANIC", N_PANIC, K_FX, 0, 20, 25), P("LIFT UP", N_LIFTUP, K_FX, 0, 25, 25),
    P("LIFT DOWN", N_LIFTDN, K_FX, 0, 25, 25),
    P("EVIL", N_EVIL, K_FX, 0, 20, 35), P("GENIUS", N_GENIUS, K_FX, 0, 20, 35),
    P("DESTROY", N_DESTROY, K_FX, 0, 25, 40), P("ROBOT CHANT", N_CHANT, K_FX, 20, 20, 30),
    P("OBEY", N_OBEY, K_FX, 20, 20, 50), P("FOOLS", N_FOOLS, K_FX, 0, 30, 45),
    P("VOWEL MORPH", N_VOWEL, K_LEAD, 30, 25, 40), P("SAM SAYS", N_SAMSAYS, K_FX, 0, 20, 30),
    P("DEEP SAM", N_DEEPSAM, K_FX, 0, 20, 40), P("GLITCH DATA", N_GLITCH, K_FX, 0, 30, 20),
    P("WELCOME", N_WELCOME, K_FX, 0, 20, 35), P("DENIED", N_DENIED, K_FX, 0, 25, 35),
#undef P
};
typedef char preset_count_ok[sizeof PRESET / sizeof PRESET[0] == QD_QUAD_NPRESET ? 1 : -1];

static void defaults(int16_t *p)
{
    int i;
    for (i = 0; i < QD_QUAD_NP; i++)
        p[i] = val(i).def;
}

static int preset_k(int k) { return k < 0 ? 0 : k >= QD_QUAD_NPRESET ? QD_QUAD_NPRESET - 1 : k; }
void qd_quad_preset_patch(int k, int16_t *p)
{
    int i;
    k = preset_k(k);
    defaults(p);
    for (i = 0; i < PRESET[k].n; i++) {
        const val_t v = val(PRESET[k].kv[i].id);
        const int x = PRESET[k].kv[i].v;
        p[PRESET[k].kv[i].id] = (int16_t)(x < v.min ? v.min : x > v.max ? v.max : x);
    }
}
const char *qd_quad_preset_name(int k) { return PRESET[preset_k(k)].name; }
int qd_quad_preset_kind(int k) { return PRESET[preset_k(k)].kind; }
const uint8_t *qd_quad_preset_sends(int k) { return PRESET[preset_k(k)].send; }

/* ---------------------------------------------------------------- packing --- */
#define MAGIC 'S'
#define VERSION 2                       /* (2: SAY and its voice; 1 had none: they take their defaults) */
#define TAG_O (QD_QUAD_BLOB - 4)
static const uint8_t MS_TAG[4] = {'M', 'I', 'D', 'I'};

const uint8_t QD_MSND_ID[QD_MSND_N] = {
    QD_QP_PW1, QD_QP_PW2, QD_QP_PW3, QD_QP_F1, QD_QP_F2, QD_QP_F3, QD_QP_DETUNE, QD_QP_HR, QD_QP_FDEPTH, QD_QP_FKTRK,
    QD_QP_FATK, QD_QP_FDEC, QD_QP_FSUS, QD_QP_FREL, QD_QP_LEVEL, QD_QP_VEL, QD_QP_BITS, QD_QP_SRR, QD_QP_DLVL,
    QD_QP_VSTART, QD_QP_VEND, QD_QP_LFO(0, QD_QL_FADE), QD_QP_LFO(1, QD_QL_FADE), QD_QP_LFO(2, QD_QL_FADE),
    QD_QP_LFO(0, QD_QL_PHASE), QD_QP_LFO(1, QD_QL_PHASE), QD_QP_LFO(0, QD_QL_DEPTH), QD_QP_LFO(1, QD_QL_DEPTH)};

int qd_msnd_slot(int id)
{
    int k;
    for (k = 0; k < QD_MSND_N; k++)
        if (QD_MSND_ID[k] == id)
            return k;
    return -1;
}
void qd_msnd_over(int16_t *p)
{
    int k;
    for (k = 0; k < QD_MSND_N; k++)
        p[QD_MSND_ID[k]] = 0, p[QD_MSND_CC(k)] = (int16_t)(16 + k);
    p[QD_QP_VPRIO] = QD_MSND_MARK;
}
void qd_msnd_init(int16_t *p)
{
    qd_quad_preset_patch(0, p);
    qd_msnd_over(p);
}
void qd_quad_side_pack(const int16_t *p, uint8_t *b)
{
    const int m = qd_msnd_is(p);
    int k;
    for (k = 0; k < QD_MSND_N; k++) {
        const int i = QD_MSND_CC(k);
        const val_t r = val(i);
        const int top = m ? p[i] & QD_MSND_TOUCHED : k < 2 ? (p[QD_QP_VPRIO] >> k & 1) << 7 : 0;
        b[k] = (uint8_t)(clamp(p[QD_MSND_ID[k]], 0, 127) | top);
        b[QD_MSND_N + k] = (uint8_t)(m ? p[i] & 127 : clamp(p[i], r.min, r.max) - r.min);
    }
}
void qd_quad_side_unpack(const uint8_t *b, int midi, int16_t *p)
{
    int k;
    for (k = 0; k < QD_MSND_N; k++) {
        const int i = QD_MSND_CC(k);
        p[QD_MSND_ID[k]] = (int16_t)(b[k] & 127);
        p[i] = (int16_t)(midi ? b[QD_MSND_N + k] | (b[k] & QD_MSND_TOUCHED) : b[QD_MSND_N + k] + val(i).min);
    }
    p[QD_QP_VPRIO] = (int16_t)(midi ? QD_MSND_MARK : (b[0] >> 7) | (b[1] >> 7) << 1);
}

void qd_quad_pack(const int16_t *p, uint8_t *b)
{
    int i, o = 2;
    memset(b, 0, QD_QUAD_BLOB);
    b[0] = MAGIC, b[1] = VERSION;
    if (qd_msnd_is(p)) {
        int k;
        for (k = 0; k < QD_MSND_N; k++) {
            b[2 + k] = (uint8_t)(clamp(p[QD_MSND_ID[k]], 0, 127) | (p[QD_MSND_CC(k)] & QD_MSND_TOUCHED));
            b[2 + QD_MSND_N + k] = (uint8_t)(p[QD_MSND_CC(k)] & 127);
        }
        memcpy(b + TAG_O, MS_TAG, 4);
        return;
    }
    for (i = 0; i < QD_QUAD_NP; i++) {
        const val_t r = val(i);
        const int x = clamp(p[i], r.min, r.max) - r.min;
        b[o++] = (uint8_t)(x & 255);
        if (r.max - r.min > 255)
            b[o++] = (uint8_t)(x >> 8);
    }
}
void qd_quad_pack5(const int16_t *p, uint8_t *blob)
{
    uint8_t b[QD_QUAD_BLOB];
    qd_quad_pack(p, b);
    memcpy(blob, b, QD_QUAD_BLOB5);
}
int qd_quad_unpack(const uint8_t *b, int n, int16_t *p)
{
    int i, o = 2, np;
    defaults(p);
    if (!b || n < QD_QUAD_BLOB || b[0] != MAGIC || (b[1] != VERSION && b[1] != 1))
        return 0;
    np = b[1] == 1 ? QD_QP_SAY : QD_QUAD_NP;            /* (version 1: no SAY, no voice: their defaults) */
    if (!memcmp(b + TAG_O, MS_TAG, 4)) {
        int k;
        for (k = 0; k < QD_MSND_N; k++)
            if (b[2 + QD_MSND_N + k] > 127)
                return 0;
        qd_msnd_init(p);
        for (k = 0; k < QD_MSND_N; k++) {
            p[QD_MSND_ID[k]] = (int16_t)(b[2 + k] & 127);
            p[QD_MSND_CC(k)] = (int16_t)(b[2 + QD_MSND_N + k] | (b[2 + k] & QD_MSND_TOUCHED));
        }
        return 1;
    }
    for (i = 0; i < np; i++) {
        const val_t r = val(i);
        int x = b[o++];
        if (r.max - r.min > 255)
            x |= b[o++] << 8;
        if (x > r.max - r.min) {
            defaults(p);
            return 0;
        }
        p[i] = (int16_t)(x + r.min);
    }
    return 1;
}
int qd_quad_ext_used(const int16_t *p) { return p != 0; }
uint32_t qd_quad_srr_inc(int srr)
{
    srr = srr < 0 ? 0 : srr > 127 ? 127 : srr;
    return srr ? 0xFFFFFFFFu / (uint32_t)(1 + srr / 3) : 0xFFFFFFFFu;
}
int qd_quad_crush_bits(int bits) { return bits <= 0 ? 16 : 16 - (bits > 127 ? 127 : bits) * 14 / 127; }
void qd_quad_algo(int a, uint8_t *mod4, int *fb, int *outx, int *outy)
{
    (void)a;
    if (mod4)
        memset(mod4, 0, 4);
    if (fb)
        *fb = -1;
    if (outx)
        *outx = 1;
    if (outy)
        *outy = 0;
}

/* ---------------------------------------------------------------- the destinations --- */
int qd_quad_dest_id(int d)
{
    switch (d) {
    case QD_D_CUT: return QD_QP_FREQ;
    case QD_D_RES: return QD_QP_RESO;
    case QD_D_PW1: return QD_QP_PW1;
    case QD_D_PW2: return QD_QP_PW2;
    case QD_D_PW3: return QD_QP_PW3;
    case QD_D_LEVEL: return QD_QP_LEVEL;
    case QD_D_DRIVE: return QD_QP_DRIVE;
    case QD_D_BITS: return QD_QP_BITS;
    case QD_D_SRR: return QD_QP_SRR;
    case QD_D_FENV: return QD_QP_FDEPTH;
    case QD_D_DETUNE: return QD_QP_DETUNE;
    default: return -1;                                 /* (PITCH, PAN, the word's place ..: no one value) */
    }
}
int qd_quad_dest_of(int id, int lfo)
{
    int d;
    (void)lfo;
    for (d = 1; d < QD_D_N; d++)
        if (qd_quad_dest_id(d) == id)
            return d;
    return -1;
}
int qd_quad_ndest(void) { return QD_D_N; }
const char *qd_quad_dest_name(int d) { return d >= 0 && d < QD_D_N ? DEST_NAME[d] : "?"; }
int qd_quad_dest_step(int d, int s)
{
    d += s;
    return d < 0 ? 0 : d >= QD_D_N ? QD_D_N - 1 : d;
}
int qd_quad_dest_pos(int d) { return d < 0 ? 0 : d >= QD_D_N ? QD_D_N - 1 : d; }
/* 2026-11-14 (owner): MULT turns in the order of its speed, FREE, then 1/128 fastest .. 8 BAR slowest (the stored value
 * stays the place in L_MULT, so the sounds saved before 1/64 and 1/128 keep theirs) */
static const uint8_t MULT_ORDER[12] = {0, 11, 10, 1, 2, 3, 4, 5, 6, 7, 8, 9};
int qd_quad_mult_pos(int m)
{
    int i;
    for (i = 0; i < 12; i++)
        if (MULT_ORDER[i] == m)
            return i;
    return 0;
}
int qd_quad_mult_at(int pos) { return MULT_ORDER[pos < 0 ? 0 : pos > 11 ? 11 : pos]; }
int qd_quad_mult_step(int m, int d) { return qd_quad_mult_at(qd_quad_mult_pos(m) + d); }

/* ---------------------------------------------------------------- the math: 2^x, the LFO shapes --- */
static float ex2(float x)                               /* 2^x, about 0.001 % (no libm: the device's flash diet) */
{
    int i = (int)(x >= 0.f ? x : x - 1.f);
    float f = x - (float)i, r;
    union { float f; uint32_t u; } u;
    r = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
    if (i < -126)
        return 0.f;
    if (i > 127)
        i = 127;
    u.f = r;
    u.u += (uint32_t)i << 23;
    return u.f;
}

static uint32_t rnd(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

/* the ARP waves (quad_voice.h): a chord's notes, played up */
static const uint8_t ARP_CHORD[QD_LW_N - QD_LW_ARP0][5] = {   /* (their count, then the semitones) */
    {2, 0, 12, 0, 0}, {2, 0, 7, 0, 0},  {3, 0, 4, 7, 0},  {3, 0, 3, 7, 0}, {3, 0, 5, 7, 0},
    {4, 0, 4, 7, 10}, {4, 0, 3, 7, 10}, {4, 0, 4, 7, 11}, {4, 0, 3, 6, 9}};
int qd_quad_arp_chord(int w, const uint8_t **notes)
{
    if (w < QD_LW_ARP0 || w >= QD_LW_N)
        return 0;
    if (notes)
        *notes = ARP_CHORD[w - QD_LW_ARP0] + 1;
    return ARP_CHORD[w - QD_LW_ARP0][0];
}
static int arp_pitch(int dest) { return dest == QD_D_PITCH || (dest >= QD_D_P1 && dest <= QD_D_P3); }
/* LFO k's arpeggio: its steps a cycle (0: not an ARP wave): the chord, on a pitch over DEPTH's octaves */
static int arp_steps(const int16_t *p, int k)
{
    const int n = qd_quad_arp_chord(p[QD_QP_LFO(k, QD_QL_WAVE)], 0);
    int dep = p[QD_QP_LFO(k, QD_QL_DEPTH)] - 64;
    if (!n || !arp_pitch(p[QD_QP_LFO(k, QD_QL_DEST)]))
        return n;
    dep = dep < 0 ? -dep : dep;
    return n * (dep >= 43 ? 3 : dep >= 22 ? 2 : 1);
}
/* .. its semitones at phase ph (the notes up; DEPTH under 64: down) */
static int arp_semis(const int16_t *p, int k, uint32_t ph)
{
    const uint8_t *c = 0;
    const int n = qd_quad_arp_chord(p[QD_QP_LFO(k, QD_QL_WAVE)], &c), steps = arp_steps(p, k);
    int i = (int)(((uint64_t)ph * (uint32_t)steps) >> 32);
    if (!n)
        return 0;
    if (p[QD_QP_LFO(k, QD_QL_DEPTH)] < 64)
        i = steps - 1 - i;
    return c[i % n] + 12 * (i / n);
}

/* an LFO's step a block: FREE its SPEED (0.05 .. 51 Hz), a division the tempo's. An ARP wave: the rate is a step's
 * (FREE: SPEED in PAL frames, 50.12 Hz the frame), the cycle its steps */
static uint32_t lfo_inc(const int16_t *p, int k, int bpm)
{
    static const float QUARTERS[12] = {0.f, 0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f, 16.f, 32.f, 0.0625f, 0.03125f};
    const int mu = p[QD_QP_LFO(k, QD_QL_MULT)], steps = arp_steps(p, k);
    float hz;
    if (mu > 0 && mu < 12)
        hz = (float)(bpm > 0 ? bpm : 120) / 60.f / QUARTERS[mu];
    else if (steps)
        hz = ((float)PAL_CLK / 19656.f) / (float)(1 + (127 - p[QD_QP_LFO(k, QD_QL_SPEED)]) / 8);
    else
        hz = 0.05f * ex2((float)p[QD_QP_LFO(k, QD_QL_SPEED)] * (10.f / 127.f));
    if (steps)
        hz /= (float)steps;
    return (uint32_t)(hz * ((float)BLK / (float)SR) * 4294967296.f);
}

/* the shape at phase ph (Q15, bipolar but EXP: 1 .. 0) */
static int32_t lfo_shape(int w, uint32_t ph, int32_t sh)
{
    const int32_t u = (int32_t)(ph >> 17);              /* 0 .. 32767 */
    switch (w) {
    case QD_LW_TRI: return u < 16384 ? u * 4 - 32767 : 32767 - (u - 16384) * 4;
    case QD_LW_SINE: {                                  /* a parabola each half */
        const int32_t h = u & 16383, y = (int32_t)(((int64_t)h * (16384 - h)) >> 11);   /* 0 .. 32768 */
        return u < 16384 ? (y > 32767 ? 32767 : y) : -(y > 32767 ? 32767 : y);
    }
    case QD_LW_SAW: return 32767 - u * 2;
    case QD_LW_SQR: return u < 16384 ? 32767 : -32767;
    case QD_LW_SH: return sh;
    case QD_LW_EXP: {
        const int32_t r = 32767 - u, r2 = (int32_t)(((int64_t)r * r) >> 15);
        return (int32_t)(((int64_t)r2 * r) >> 15);
    }
    case QD_LW_RAMP: return u * 2 - 32767;
    default: return 0;
    }
}

/* ---------------------------------------------------------------- the owner --- */
void qd_quad_owner_init(qd_quad_owner_t *o)
{
    memset(o, 0, sizeof *o);
    o->bpm = 120;
    o->last_pitch = -1;
    o->quad_lfo.rnd[0] = 0x1234u, o->quad_lfo.rnd[1] = 0x5678u, o->quad_lfo.rnd[2] = 0x9ABCu;
}

void qd_quad_owner_block(qd_quad_owner_t *o)
{
    int k;
    if (!o->quad_patch)
        return;
    for (k = 0; k < 3; k++) {
        const uint32_t was = o->quad_lfo.ph[k];
        o->quad_lfo.ph[k] += lfo_inc(o->quad_patch, k, o->bpm);
        if (o->quad_lfo.ph[k] < was)                    /* a new cycle: S&H's new value */
            (void)rnd(&o->quad_lfo.rnd[k]);
    }
}

void qd_quad_owner_publish(qd_quad_owner_t *o)
{
    const qd_quad_voice_t *v = o->m_v;
    int k;
    if (!o->defer_off || !v || !v->nx_ok)
        return;
    o->quad_pan_off = v->nx_pan;
    for (k = 0; k < 4; k++)
        o->quad_fx_off[k] = v->nx_fx[k];
}
void qd_quad_voice_prepare(qd_quad_voice_t *v) { (void)v; }
uint32_t qd_quad_shared_sig(void) { return 0; }

int qd_quad_owner_lfos(const qd_quad_owner_t *o, int16_t *w, uint8_t *dest)
{
    const qd_quad_voice_t *v = o ? o->m_v : 0;
    int k;
    if (!v || !v->active || v->owner != o || !v->nx_ok)
        return 0;
    for (k = 0; k < 3; k++) {
        w[k] = v->nx_lw[k];
        dest[k] = (uint8_t)v->patch[QD_QP_LFO(k, QD_QL_DEST)];
    }
    return 1;
}
int qd_quad_lfo_value(int id, int v, const int16_t *w, const uint8_t *dest)
{
    int16_t lo, hi;
    int32_t sum = 0;
    int k;
    if (!qd_quad_value_range(id, &lo, &hi, 0))
        return v;
    for (k = 0; k < 3; k++)
        if (w[k] && dest[k] && qd_quad_dest_of(id, k) == dest[k])
            sum += w[k];
    if (!sum)
        return v;
    v += (int)((sum * (int32_t)(hi - lo)) >> 15);
    return v < lo ? lo : v > hi ? hi : v;
}

/* ---------------------------------------------------------------- a voice --- */
static uint8_t wave_bits(int w)
{
    static const uint8_t B[QD_SW_N] = {0, SID_TRI, SID_SAW, SID_PULSE, SID_NOISE, SID_TRI | SID_SAW, SID_PULSE | SID_TRI,
                                       SID_PULSE | SID_SAW, SID_PULSE | SID_TRI | SID_SAW};
    return w >= 0 && w < QD_SW_N ? B[w] : 0;
}

void qd_quad_voice_init(qd_quad_voice_t *v, uint32_t idx)
{
    memset(v, 0, sizeof *v);
    defaults(v->patch);
    v->idx = idx;
    v->model = 0;
    sid_init(&v->chip, SID_6581, PAL_CLK, SR);
    sid_write(&v->chip, SID_MODE_VOL, 15);
}

static void vox_start(qd_quad_voice_t *v)
{
    const int16_t *p = v->patch;
    const int w = p[QD_QP_WORD], say = p[QD_QP_SAY];
    const uint8_t *src = 0;
    uint32_t len = 0;
    v->vox.on = 0;
    if (say > 0 || (w >= 1 && w <= (int)VOX_NWORDS))
        src = qd_say_get(qd_say_key(p), &len);          /* SAM's render (the app made it: qd_say_want) */
    if (!src || !len)                                   /* not rendered (yet), or no room for it: silence */
        return;
    {
        uint32_t lo = len * (uint32_t)p[QD_QP_VSTART] / 127u, hi = len * (uint32_t)p[QD_QP_VEND] / 127u;
        if (hi > len)
            hi = len;
        if (hi <= lo + 8u)
            hi = lo + 8u < len ? lo + 8u : len;
        v->vox.word = (uint16_t)(w > 0 ? w - 1 : 0);
        v->vox.src = src;
        v->vox.len = len;
        v->vox.lo = lo << 16;
        v->vox.hi = hi << 16;
        v->vox.pos = v->vox.lo;
        v->vox.dir = 0;
        v->vox.hold_acc = 0;
        v->vox.on = 1;
    }
}

void qd_quad_voice_start(qd_quad_voice_t *v, const int16_t *patch, qd_quad_owner_t *o, int note, int vel)
{
    const int sounding = v->active && !v->kill;
    int k;
    if (patch != v->patch)
        memcpy(v->patch, patch, sizeof v->patch);
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    v->owner = o;
    v->note = (uint8_t)note;
    v->vel = v->mvel = (uint8_t)vel;
    v->gate = v->active = 1;
    v->kill = 0;
    v->nx_ok = 0;
    v->pitch16 = v->pitch_cur = (int32_t)note * 16;
    v->gstep = 0;
    {   /* RANDOM: a value a note, from the voice's place and the track's count of notes (deterministic) */
        uint32_t r = v->idx * 0x9E3779B9u + ++o->nserial * 0x85EBCA6Bu;
        r ^= r >> 15, r *= 0x2C1B3C6Du, r ^= r >> 12;
        v->mrnd = (int16_t)(r >> 16);
    }
    o->m_v = v;
    if (v->patch[QD_QP_CHIP] != v->model || !sounding) {   /* a fresh chip (or another model) */
        if (v->patch[QD_QP_CHIP] != v->model) {
            v->model = (uint8_t)(v->patch[QD_QP_CHIP] ? 1 : 0);
            sid_init(&v->chip, v->model ? SID_8580 : SID_6581, PAL_CLK, SR);
        }
        if (!sounding) {
            v->env = v->env_out = 0;
            v->level = 0;
            memset(&v->crush, 0, sizeof v->crush);
            v->gated = 0;
        }
    }
    v->hr_left = 0;
    if (sounding && v->patch[QD_QP_HR])                 /* hard restart: the gate off, the ADSR 0, a few blocks */
        v->hr_left = (uint8_t)clamp(1 + v->patch[QD_QP_HR] * 138 / 100, 1, 180);
    else if (sounding)
        v->gated = 0;                                   /* (a gate edge this block: the attack again) */
    /* The chip's ADSR delay: its 15-bit rate counter runs on at the last note's release rate, and an attack whose
     * period is under the count waits for the counter to go all the way round, 32768 cycles, 33 ms: a kick a 32nd
     * late, or its noise on time and its body late. A C64 player's hard restart is there to prevent it, so a note
     * on a silent voice, and one after a hard restart, starts the counters at 0; a note struck over a sounding
     * voice with HR 0 is left as the chip plays it. */
    v->esync = (uint8_t)(!sounding || v->patch[QD_QP_HR] != 0);
    for (k = 0; k < 3; k++) {                           /* the LFOs that restart with a note */
        v->lph[k] = (uint32_t)v->patch[QD_QP_LFO(k, QD_QL_PHASE)] << 25;
        v->lstage[k] = 0;
        v->lsh[k] = (int32_t)(rnd(&o->quad_lfo.rnd[k]) >> 17) - 16384;
    }
    for (k = 0; k < 3; k++) {                           /* HOLD: the FREE LFO's value now, held */
        const int w = v->patch[QD_QP_LFO(k, QD_QL_WAVE)];
        v->lhold[k] = w >= QD_LW_ARP0 ? arp_semis(v->patch, k, o->quad_lfo.ph[k] + v->lph[k])
                                      : lfo_shape(w, o->quad_lfo.ph[k] + v->lph[k], (int32_t)(o->quad_lfo.rnd[k] >> 17) - 16384);
    }
    v->lage = 0;
    v->fenv = 0;
    v->fstage = 0;
    vox_start(v);
}

void qd_quad_voice_release(qd_quad_voice_t *v) { v->gate = 0; }

void qd_quad_voice_legato(qd_quad_voice_t *v, int note, int vel)
{
    if (!v->active || v->kill || !v->owner)
        return;
    note = note < 0 ? 0 : note > 127 ? 127 : note;
    vel = vel < 1 ? 1 : vel > 127 ? 127 : vel;
    v->note = (uint8_t)note;
    {
        const int from = v->pitch_cur;
        v->pitch16 = v->pitch_cur = (int32_t)note * 16;
        v->gstep = 0;
        if (v->patch[QD_QP_PORT])
            qd_quad_voice_glide(v, from);
    }
    v->mvel = (uint8_t)vel;
    if (vel > v->vel)
        v->vel = (uint8_t)vel;
    v->gate = 1;
}

/* the glide: TIME 1 + PTIM^2 / 8 blocks whatever the interval, RATE a fixed speed */
void qd_quad_voice_glide(qd_quad_voice_t *v, int from)
{
    v->gstep = 0;
    if (from < 0 || !v->patch[QD_QP_PORT] || from == v->pitch16) {
        v->pitch_cur = v->pitch16;
        return;
    }
    v->pitch_cur = clamp(from, 0, 127 * 16);
    if (!v->patch[QD_QP_PSLOPE]) {
        const int32_t n = 1 + v->patch[QD_QP_PTIM] * v->patch[QD_QP_PTIM] / 8;
        const int32_t gap = v->pitch16 - v->pitch_cur;
        v->gstep = (gap < 0 ? -gap : gap) / n;
        if (v->gstep < 1)
            v->gstep = 1;
    }
}

void qd_quad_voice_kill(qd_quad_voice_t *v)
{
    if (v->active) {
        v->gate = 0;
        v->kill = 1;
    }
}

/* the filter envelope, a block: 0..127 times (1 ms .. 4 s, squared) */
static void fenv_block(qd_quad_voice_t *v)
{
    const int16_t *p = v->patch;
#define STEP(t) ((int32_t)(32767 * 32 / 44) / (1 + (t) * (t) / 4))
    if (!v->gate && v->fstage < 3)
        v->fstage = 3;
    switch (v->fstage) {
    case 0:
        v->fenv += STEP(p[QD_QP_FATK]) + 1;
        if (v->fenv >= 32767)
            v->fenv = 32767, v->fstage = 1;
        break;
    case 1: {
        const int32_t s = p[QD_QP_FSUS] * 258;
        v->fenv -= STEP(p[QD_QP_FDEC]) + 1;
        if (v->fenv <= s)
            v->fenv = s, v->fstage = 2;
        break;
    }
    case 2:
        v->fenv = p[QD_QP_FSUS] * 258;
        break;
    case 3:
        v->fenv -= STEP(p[QD_QP_FREL]) + 1;
        if (v->fenv <= 0)
            v->fenv = 0, v->fstage = 4;
        break;
    default:
        v->fenv = 0;
        break;
    }
#undef STEP
}

/* the modulation of a block: d[dest] in Q15 (the sum of the LFOs and the slots) */
static void modulate(qd_quad_voice_t *v, int32_t *d)
{
    const int16_t *p = v->patch;
    const qd_quad_owner_t *o = v->owner;
    int k;
    memset(d, 0, sizeof(int32_t) * QD_D_N);
    for (k = 0; k < 3; k++) {
        const int dest = p[QD_QP_LFO(k, QD_QL_DEST)], depth = p[QD_QP_LFO(k, QD_QL_DEPTH)] - 64;
        const int w = p[QD_QP_LFO(k, QD_QL_WAVE)], tr = p[QD_QP_LFO(k, QD_QL_TRIG)];
        const int fade = p[QD_QP_LFO(k, QD_QL_FADE)] - 64;
        int32_t x, f = 32767;
        uint32_t aph = v->lph[k];                       /* (an ARP wave's phase) */
        v->nx_lw[k] = 0;
        if (tr == QD_LT_FREE) {
            aph += o->quad_lfo.ph[k];
            x = lfo_shape(w, aph, (int32_t)(o->quad_lfo.rnd[k] >> 17) - 16384);
        } else if (tr == QD_LT_HOLD) {
            x = v->lhold[k];
        } else {
            const uint32_t was = v->lph[k], end = tr == QD_LT_HALF ? 0x80000000u : 0u;
            if (!v->lstage[k]) {
                v->lph[k] += lfo_inc(p, k, o->bpm);
                if (v->lph[k] < was)
                    v->lsh[k] = (int32_t)(rnd(&v->idx) >> 17) - 16384;
                if ((tr == QD_LT_ONE && v->lph[k] < was) || (tr == QD_LT_HALF && was < 0x80000000u &&
                                                             v->lph[k] >= 0x80000000u))
                    v->lstage[k] = 1, v->lph[k] = end ? end - 1u : 0xFFFFFFFFu;
            }
            x = lfo_shape(w, v->lph[k], v->lsh[k]);
            aph = v->lph[k];
        }
        if (w >= QD_LW_ARP0) {                          /* ARP: on a pitch the chord's semitones exactly (a pitch's
                                                         * Q15 is 24 semitones), no fade; else a staircase */
            const int semis = tr == QD_LT_HOLD ? v->lhold[k] : arp_semis(p, k, aph);
            if (arp_pitch(dest)) {
                x = depth ? (semis * 32768 + 12) / 24 : 0;
                v->nx_lw[k] = (int16_t)clamp(x, -32767, 32767);
                d[dest] += x;
                continue;
            }
            x = semis * 2730;
        }
        if (fade) {                                     /* in (under 64) or out (over), over |fade| x 8 blocks */
            const int32_t n = (fade < 0 ? -fade : fade) * 8, a = (int32_t)(v->lage < (uint32_t)n ? v->lage : (uint32_t)n);
            f = fade < 0 ? a * 32767 / n : 32767 - a * 32767 / n;
        }
        x = (int32_t)(((int64_t)x * depth * f) >> 21);   /* (Q15 x depth/64 x fade) */
        v->nx_lw[k] = (int16_t)clamp(x, -32767, 32767);
        if (dest > 0 && dest < QD_D_N)
            d[dest] += x;
    }
    for (k = 0; k < QD_NMOD; k++) {
        const int src = p[QD_QP_MOD(k, QD_QM_SRC)], dst = p[QD_QP_MOD(k, QD_QM_DST)], dep = p[QD_QP_MOD(k, QD_QM_DEPTH)];
        int32_t s;
        if (!src || dst <= 0 || dst >= QD_D_N || !dep)
            continue;
        switch (src) {
        case QD_MS_VEL: s = v->vel * 258; break;
        case QD_MS_KEY: s = (v->note - 60) * 512; break;
        case QD_MS_MW: s = o->mw * 258; break;
        case QD_MS_AT: s = o->at * 258; break;
        case QD_MS_PB: s = o->pb * 4; break;
        case QD_MS_BR: s = o->br * 258; break;
        case QD_MS_RAND: s = v->mrnd; break;
        case QD_MS_FENV: s = v->fenv; break;
        default: s = 0; break;
        }
        d[dst] += s * dep / 127;
    }
}

static uint32_t freq_reg(float semis, uint32_t udet)
{
    float hz = 440.f * ex2((semis - 69.f) * (1.f / 12.f)), r;
    if (udet)
        hz *= (float)udet * (1.f / 65536.f);
    r = hz * (16777216.f / (float)PAL_CLK);
    return r < 0.f ? 0u : r > 65535.f ? 65535u : (uint32_t)r;
}

/* the chip's registers for this block */
static void registers(qd_quad_voice_t *v, const int32_t *d, int *vox_only)
{
    const int16_t *p = v->patch;
    sid_t *c = &v->chip;
    const float base = (float)v->pitch_cur * (1.f / 16.f) + (float)v->owner->pb * (2.f / 8192.f) +
                       (float)d[QD_D_PITCH] * (24.f / 32768.f);
    const int det = clamp(p[QD_QP_DETUNE] + (d[QD_D_DETUNE] * 127 >> 15), 0, 127);
    const int hr = v->hr_left > 0;
    const int boost = v->vox.on && p[QD_QP_DBOOST];
    int n, on = 0, cut, res, route = 0, mode;
    for (n = 0; n < 3; n++) {
        const int w = p[QD_QP_W1 + 5 * n], o = 5 * n;
        const int ea = n == 0 || p[n == 1 ? QD_QP_E2LINK : QD_QP_E3LINK] ? QD_QP_EATK : n == 1 ? QD_QP_E2A : QD_QP_E3A;
        const int a = p[ea], dd = p[ea + 1], s = p[ea + 2], r = p[ea + 3];
        const int pd = n == 0 ? QD_D_P1 : n == 1 ? QD_D_P2 : QD_D_P3;
        float semis = base + (float)p[QD_QP_C1 + o] + (float)(p[QD_QP_F1 + o] - 64) * (1.f / 64.f) +
                      (float)d[pd] * (24.f / 32768.f);
        uint32_t fr, pw;
        uint8_t ctrl;
        if (n == 1)
            semis += (float)det * (0.4f / 127.f);
        else if (n == 2)
            semis -= (float)det * (0.4f / 127.f);
        fr = freq_reg(semis, v->udet);
        pw = (uint32_t)clamp(p[QD_QP_PW1 + o] * 32 + (d[QD_D_PW1 + n] >> 3) + (d[QD_D_PWALL] >> 3), 8, 4087);
        sid_write(c, (uint32_t)(7 * n + SID_FREQ_LO), fr & 255u);
        sid_write(c, (uint32_t)(7 * n + SID_FREQ_HI), fr >> 8);
        sid_write(c, (uint32_t)(7 * n + SID_PW_LO), pw & 255u);
        sid_write(c, (uint32_t)(7 * n + SID_PW_HI), pw >> 8);
        if (n == 2 && boost) {                          /* the 8580's digi boost: voice 3 a steady DC */
            sid_write(c, 7 * 2 + SID_AD, 0x00);
            sid_write(c, 7 * 2 + SID_SR, 0xF0);
            sid_write(c, 7 * 2 + SID_CTRL, SID_PULSE | SID_TEST | SID_GATE);
            on |= 4;
            continue;
        }
        if (hr) {
            sid_write(c, (uint32_t)(7 * n + SID_AD), 0);
            sid_write(c, (uint32_t)(7 * n + SID_SR), 0);
        } else {
            sid_write(c, (uint32_t)(7 * n + SID_AD), (uint32_t)(a << 4 | dd));
            sid_write(c, (uint32_t)(7 * n + SID_SR), (uint32_t)(s << 4 | r));
        }
        ctrl = wave_bits(w);
        if (ctrl) {
            on |= 1 << n;
            ctrl |= (uint8_t)((p[QD_QP_M1 + o] & 1 ? SID_SYNC : 0) | (p[QD_QP_M1 + o] & 2 ? SID_RING : 0));
            if (v->gate && !hr && v->gated)
                ctrl |= SID_GATE;
            else if (v->gate && !hr) {                  /* the edge: off, then on */
                if (v->esync)
                    c->v[n].rate_cnt = 0;
                sid_write(c, (uint32_t)(7 * n + SID_CTRL), ctrl);
                ctrl |= SID_GATE;
            }
        }
        sid_write(c, (uint32_t)(7 * n + SID_CTRL), ctrl);
    }
    v->gated = (uint8_t)(v->gate && !hr);
    if (v->gated)
        v->esync = 0;
    *vox_only = !(on & 3) && !(on & 4 && !boost) && v->vox.on;
    /* the filter: its cutoff from the knob, the envelope, the key and the modulation */
    cut = p[QD_QP_FREQ] + (int)(((int64_t)(p[QD_QP_FDEPTH] - 64 + (d[QD_D_FENV] * 64 >> 15)) * v->fenv) >> 14) +
          (v->note - 60) * p[QD_QP_FKTRK] / 8 + (d[QD_D_CUT] >> 4);
    cut = clamp(cut, 0, 2047);
    res = clamp(p[QD_QP_RESO] + (d[QD_D_RES] * 15 >> 15), 0, 15);
    {
        static const uint8_t MODE[QD_QF_N] = {0, SID_LP, SID_BP, SID_HP, SID_LP | SID_HP, SID_LP | SID_BP,
                                              SID_BP | SID_HP, SID_LP | SID_BP | SID_HP};
        mode = MODE[clamp(p[QD_QP_FTYPE], 0, QD_QF_N - 1)];
    }
    /* the word through the filter (ROUTE's V, a mode on): it is the chip's EXT IN then, FILT EX set, the volume
     * register left full; else the volume register's digi, after the filter as on the C64 */
    v->vox.filt = (uint8_t)(v->vox.on && mode && (p[QD_QP_FROUTE] & 8));
    if (mode)
        route = (p[QD_QP_FROUTE] & on & 7) | (v->vox.filt ? 8 : 0);
    if (!v->vox.filt)
        c->ext = 0;
    sid_write(c, SID_FC_LO, (uint32_t)cut & 7u);
    sid_write(c, SID_FC_HI, (uint32_t)cut >> 3);
    sid_write(c, SID_RES_FILT, (uint32_t)(res << 4 | route));
    sid_write(c, SID_MODE_VOL, (uint32_t)(mode | (v->vox.on && !v->vox.filt ? v->vox.nib : 15)));
}

/* VOX: the nibble at the word's place, its bits and level */
static uint32_t vox_nib(const qd_quad_voice_t *v, int32_t wpos)
{
    const int16_t *p = v->patch;
    const uint32_t len = v->vox.len;
    int32_t i = (int32_t)(v->vox.pos >> 16) + (int32_t)(((int64_t)wpos * (int32_t)len) >> 16);
    uint32_t b, nib;
    int bits = clamp(p[QD_QP_DBITS], 1, 4), x;
    i = i < 0 ? 0 : i >= (int32_t)len ? (int32_t)len - 1 : i;
    b = v->vox.src[(uint32_t)i / 2u];
    nib = i & 1 ? b & 15u : b >> 4;
    if (bits < 4)
        nib = (nib & (0xFu << (4 - bits)) & 0xFu) | (1u << (3 - bits));
    x = 8 + ((int)nib - 8) * p[QD_QP_DLVL] / 127;
    return (uint32_t)clamp(x, 0, 15);
}

/* the word moves on n samples; at its end it loops (held, LOOP on) or stops */
static void vox_move(qd_quad_voice_t *v, uint32_t step, uint32_t n)
{
    const int loop = v->gate ? v->patch[QD_QP_VLOOP] : 0;
    const uint32_t adv = step * n;
    if (!v->vox.dir) {
        v->vox.pos += adv;
        if (v->vox.pos >= v->vox.hi) {
            if (loop == 1)
                v->vox.pos = v->vox.lo + (v->vox.pos - v->vox.hi) % (v->vox.hi - v->vox.lo);
            else if (loop == 2)
                v->vox.dir = 1, v->vox.pos = v->vox.hi - 1u;
            else
                v->vox.on = 0;
        }
    } else {
        if (v->vox.pos - v->vox.lo <= adv || !loop)
            v->vox.dir = 0, v->vox.pos = v->vox.lo;
        else
            v->vox.pos -= adv;
    }
}

int qd_quad_voice_render(qd_quad_voice_t *v, int32_t *out)
{
    qd_quad_owner_t *o = v->owner;
    const int16_t *p = v->patch;
    int32_t d[QD_D_N], buf[BLK], g, amp0, amp1, peak = 0;
    int i, vox_only = 0;
    if (!v->active || !o)
        return 0;
    if (v->pitch_cur != v->pitch16) {                   /* the glide */
        int32_t st = v->gstep ? v->gstep : 1 + (127 - p[QD_QP_PTIM]) / 8;
        const int32_t gap = v->pitch16 - v->pitch_cur;
        v->pitch_cur += gap > st ? st : gap < -st ? -st : gap;
    }
    fenv_block(v);
    modulate(v, d);
    registers(v, d, &vox_only);
    if (v->hr_left)
        v->hr_left--;
    v->lage++;
    v->nx_pan = (int8_t)clamp(d[QD_D_PAN] >> 9, -64, 63);
    v->nx_fx[0] = (int8_t)clamp(d[QD_D_DRIVE] >> 8, -127, 127);
    v->nx_ok = 1;
    if (!o->defer_off && o->m_v == v) {
        o->quad_pan_off = v->nx_pan;
        o->quad_fx_off[0] = v->nx_fx[0];
    }
    /* the chip: a block, or in pieces with the volume register's writes between them (VOX) */
    if (v->vox.on) {
        const float semis = ((float)v->pitch_cur * (1.f / 16.f) - (float)p[QD_QP_ROOT]) * (1.f / 12.f) +
                            (float)d[QD_D_VOXP] * (2.f / 32768.f) + (float)o->pb * (2.f / 8192.f / 12.f);
        const uint32_t step = (uint32_t)(ex2(semis) * ((float)VOX_FS / (float)SR) * 65536.f);
        const uint32_t ival = ((uint32_t)SR << 16) / HOLD_HZ[clamp(p[QD_QP_DHOLD], 0, 7)];
        const int32_t wpos = d[QD_D_WPOS];
        static const uint8_t MODE[QD_QF_N] = {0, SID_LP, SID_BP, SID_HP, SID_LP | SID_HP, SID_LP | SID_BP,
                                              SID_BP | SID_HP, SID_LP | SID_BP | SID_HP};
        const uint32_t mode = MODE[clamp(p[QD_QP_FTYPE], 0, QD_QF_N - 1)];
        i = 0;
        while (i < BLK) {
            uint32_t n;
            if (v->vox.hold_acc < 65536u) {
                if (v->vox.on)
                    v->vox.nib = (uint8_t)vox_nib(v, wpos);
                if (v->vox.filt)                        /* (EXT IN: a volume step of the 6581's digi is 375 after the
                                                         * volume: 800 before it, the same loudness) */
                    v->chip.ext = v->vox.on ? ((int32_t)v->vox.nib - 8) * 800 : 0;
                else
                    sid_write(&v->chip, SID_MODE_VOL, mode | (v->vox.on ? v->vox.nib : 15u));
                v->vox.hold_acc += ival;
            }
            n = (v->vox.hold_acc + 65535u) >> 16;
            if (n > (uint32_t)(BLK - i))
                n = (uint32_t)(BLK - i);
            if (!n)
                n = 1;
            sid_render(&v->chip, buf + i, n);
            if (v->vox.on)
                vox_move(v, step, n);
            v->vox.hold_acc -= v->vox.hold_acc >= (n << 16) ? n << 16 : v->vox.hold_acc;
            i += (int)n;
        }
    } else {
        sid_render(&v->chip, buf, BLK);
    }
    /* the gain: LEVEL, the velocity's reach, the modulation; a word alone louder (a digi is quieter than a tone) */
    {
        const int32_t lvl = clamp(p[QD_QP_LEVEL] + (d[QD_D_LEVEL] * 127 >> 15), 0, 127);
        const int32_t vs = p[QD_QP_VEL];
        g = lvl * 64;                                   /* (Q12: LEVEL 127 about x 2) */
        g = g * (127 * 127 - vs * (127 - v->vel)) / (127 * 127);
        if (vox_only)
            g = g * (v->model && p[QD_QP_DBOOST] ? 4 : 5) / 2;
    }
    {   /* CRUSH: bits, then the sample rate */
        const int bits = qd_quad_crush_bits(clamp(p[QD_QP_BITS] + (d[QD_D_BITS] * 127 >> 15), 0, 127));
        const int srr = clamp(p[QD_QP_SRR] + (d[QD_D_SRR] * 127 >> 15), 0, 127);
        const uint32_t inc = qd_quad_srr_inc(srr);
        for (i = 0; i < BLK; i++) {
            int32_t x = (int32_t)(((int64_t)buf[i] * g) >> 12);
            if (bits < 16)
                x = (x >> (16 - bits)) << (16 - bits);
            if (srr) {
                const uint32_t was = v->crush.ph;
                v->crush.ph += inc;
                if (v->crush.ph < was || !i)
                    v->crush.held = x;
                x = v->crush.held;
            }
            buf[i] = x;
        }
    }
    amp0 = v->env_out;                                  /* the ramp: in at a start, out for a kill or a dip */
    amp1 = v->kill || v->dip ? 0 : 32767;
    v->env_out = amp1;
    for (i = 0; i < BLK; i++) {
        const int32_t a = amp0 + (amp1 - amp0) * (i + 1) / BLK;
        const int32_t x = (int32_t)(((int64_t)buf[i] * a) >> 15);
        const int32_t ax = x < 0 ? -x : x;
        out[i] += x;
        if (ax > peak)
            peak = ax;
    }
    v->level = peak > 32767 ? 32767 : peak;
    v->env = v->level << 9;
    if (v->kill) {
        v->env = v->env_out = 0;
        v->active = v->gate = v->kill = 0;
        return 1;
    }
    if (!v->gate && !v->hr_left && !v->vox.on) {        /* released, every voice of the chip silent: the end */
        int n, any = 0;
        for (n = 0; n < 3; n++)
            if (wave_bits(p[QD_QP_W1 + 5 * n]) && v->chip.v[n].env)
                any = 1;
        if (!any) {
            v->env = v->env_out = 0;
            v->active = 0;
            v->level = 0;
        }
    }
    return 1;
}

int32_t qd_quad_voice_level(const qd_quad_voice_t *v) { return v->active ? v->level : 0; }
int qd_quad_voice_endless(const qd_quad_voice_t *v) { return v->active && v->vox.on && v->patch[QD_QP_VLOOP] && v->gate; }

/* ---------------------------------------------------------------- DRIVE --- */
void qd_quad_dist(qd_dist_t *st, int drive, int32_t *b, int n)
{
    fz_drive_t dd;
    dd.hp = st->hp, dd.lp1 = st->lp1, dd.lp2 = st->lp2;
    fz_drive(&dd, drive, b, n);
    st->hp = dd.hp, st->lp1 = dd.lp1, st->lp2 = dd.lp2;
}

/* ---------------------------------------------------------------- the bench --- */
static qd_quad_owner_t b_own;
static qd_quad_voice_t b_v;
static int16_t b_patch[QD_QUAD_NP];
void qd_quad_bench_start(int k, int note)
{
    qd_quad_owner_init(&b_own);
    qd_quad_preset_patch(k, b_patch);
    b_own.quad_patch = b_patch;
    qd_quad_voice_init(&b_v, 0);
    qd_quad_voice_start(&b_v, b_patch, &b_own, note, 100);
}
int32_t qd_quad_bench_block(int what)
{
    int32_t buf[BLK], s = 0;
    int i;
    (void)what;
    memset(buf, 0, sizeof buf);
    qd_quad_owner_block(&b_own);
    qd_quad_voice_render(&b_v, buf);
    for (i = 0; i < BLK; i++)
        s += buf[i];
    return s;
}
int32_t qd_quad_bench(int k, int note, int nblocks)
{
    int32_t s = 0;
    qd_quad_bench_start(k, note);
    while (nblocks-- > 0)
        s += qd_quad_bench_block(0);
    return s;
}
