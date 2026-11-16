/* SPDX-License-Identifier: MPL-2.0 */
/* The desk's platform: the app run headless on a simulated clock from a script of surface gestures, the Move's screen
 * written as PGM pictures (4x) and the sound as a WAV. No hardware, no threads: the audio and the UI take turns.
 *
 *     egm-sim SCRIPT OUT_DIR
 *
 * The script, a command a line ('#' a comment):
 *     wait MS                run MS milliseconds
 *     note N VEL             a surface note (pads 68..99, steps 16..31, touches 0..9); VEL 0: released
 *     cc N V                 a surface CC (buttons: 127 down, 0 up; encoders: 1..63 clockwise, 127..65 back)
 *     tap N                  a button: cc N 127, 30 ms, cc N 0, 30 ms
 *     pkt A B C D            a raw USB-MIDI packet (hex), as the SPI mailbox gives it
 *     shot NAME              OUT_DIR/NAME.pgm, the screen now
 *     leds                   the LED packets sent since the last one, printed
 *     state NAME             the selected track, the core's screen and the transport, printed
 *     wav PATH               the sound from here on into PATH */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "egm.h"

static uint32_t now;
static uint64_t frames_done;
static uint8_t screen[1024];
static FILE *wavf;
static uint32_t wavn, led_count;
static long peak;

static void run(uint32_t n)
{
    while (n--) {
        uint8_t leds[20][4];
        now++;
        egm_ui_step(now);
        if (now % 33u == 0u)
            egm_frame(screen, now);
        while (frames_done + 128u <= (uint64_t)now * 44100u / 1000u) {
            int16_t blk[256];
            int i;
            egm_audio(blk, 128);
            for (i = 0; i < 256; i++)
                if (labs((long)blk[i]) > peak)
                    peak = labs((long)blk[i]);
            if (wavf)
                wavn += (uint32_t)fwrite(blk, 2, 256, wavf) * 2u;
            frames_done += 128u;
            led_count += (uint32_t)egm_leds_take(leds, 20);
        }
    }
}

static void midi(int status, int d1, int d2)
{
    const uint8_t p[4] = { (uint8_t)(status >> 4), (uint8_t)status, (uint8_t)d1, (uint8_t)d2 };
    egm_midi_in(p);
}

static int shot(const char *dir, const char *name)
{
    char path[1024];
    FILE *f;
    int x, y, lit = 0;
    egm_frame(screen, now);
    snprintf(path, sizeof path, "%s/%s.pgm", dir, name);
    if (!(f = fopen(path, "wb")))
        return -1;
    fprintf(f, "P5\n%d %d\n255\n", 128 * 4, 64 * 4);
    for (y = 0; y < 64 * 4; y++)
        for (x = 0; x < 128 * 4; x++) {
            const int on = (screen[(y / 4 / 8) * 128 + x / 4] >> ((y / 4) & 7)) & 1;
            lit += on;
            fputc(on ? 255 : 0, f);
        }
    fclose(f);
    return lit / 16;
}

int main(int argc, char **argv)
{
    char line[256];
    FILE *f;
    if (argc < 3 || !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: egm-sim SCRIPT OUT_DIR\n");
        return 2;
    }
    egm_init(0);
    run(100);
    while (fgets(line, sizeof line, f)) {
        char cmd[32] = "", a[128] = "", b[32] = "";
        char *hash = strchr(line, '#');
        if (hash)
            *hash = 0;
        if (sscanf(line, "%31s %127s %31s", cmd, a, b) < 1)
            continue;
        if (!strcmp(cmd, "wait")) run((uint32_t)atoi(a));
        else if (!strcmp(cmd, "note")) midi(atoi(b) ? 0x90 : 0x80, atoi(a), atoi(b));
        else if (!strcmp(cmd, "cc")) midi(0xB0, atoi(a), atoi(b));
        else if (!strcmp(cmd, "pkt")) {                 /* a raw USB-MIDI packet, four hex bytes */
            unsigned q[4] = { 0, 0, 0, 0 };
            uint8_t p[4];
            sscanf(line, "%*s %x %x %x %x", &q[0], &q[1], &q[2], &q[3]);
            p[0] = (uint8_t)q[0], p[1] = (uint8_t)q[1], p[2] = (uint8_t)q[2], p[3] = (uint8_t)q[3];
            egm_midi_in(p);
        }
        else if (!strcmp(cmd, "tap")) { midi(0xB0, atoi(a), 127); run(30); midi(0xB0, atoi(a), 0); run(30); }
        else if (!strcmp(cmd, "shot")) printf("shot %-16s %5d pixels lit, %u ms\n", a, shot(argv[2], a), (unsigned)now);
        else if (!strcmp(cmd, "leds")) { printf("leds %u packets, peak %ld\n", (unsigned)led_count, peak); led_count = 0; peak = 0; }
        else if (!strcmp(cmd, "core")) {               /* the core's own screen, as a colour PPM */
            const uint16_t *l = egm_core_lcd();
            char path[1024];
            FILE *o;
            int i;
            snprintf(path, sizeof path, "%s/%s.ppm", argv[2], a);
            if ((o = fopen(path, "wb"))) {
                fprintf(o, "P6\n240 240\n255\n");
                for (i = 0; i < 240 * 240; i++) {
                    const uint16_t c = (uint16_t)((l[i] >> 8) | (l[i] << 8));
                    fputc((int)(((c >> 11) & 31u) * 255u / 31u), o);
                    fputc((int)(((c >> 5) & 63u) * 255u / 63u), o);
                    fputc((int)((c & 31u) * 255u / 31u), o);
                }
                fclose(o);
            }
        }
        else if (!strcmp(cmd, "state")) {
            int t, sc, pl;
            egm_state(&t, &sc, &pl);
            char d[256];
            egm_describe(d, sizeof d);
            (void)t, (void)sc, (void)pl;
            printf("state %-14s %s\n", a, d);
        }
        else if (!strcmp(cmd, "wav")) {
            static const unsigned char H[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
                                                16, 0, 0, 0, 1, 0, 2, 0, 0x44, 0xAC, 0, 0, 0x10, 0xB1, 2, 0, 4, 0, 16, 0,
                                                'd', 'a', 't', 'a', 0, 0, 0, 0};
            if ((wavf = fopen(a, "wb")))
                fwrite(H, 1, 44, wavf);
        } else
            fprintf(stderr, "unknown command: %s\n", cmd);
    }
    if (wavf) {
        const uint32_t riff = wavn + 36u;
        fseek(wavf, 4, SEEK_SET); fwrite(&riff, 4, 1, wavf);
        fseek(wavf, 40, SEEK_SET); fwrite(&wavn, 4, 1, wavf);
        fclose(wavf);
    }
    egm_shutdown();
    return 0;
}
