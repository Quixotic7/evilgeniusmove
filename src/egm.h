/* SPDX-License-Identifier: MPL-2.0 */
/* EvilGenius on the Move: the core (evilgenius/core, the FM-1 firmware's portable half) driven from the Move's surface.
 * The platform (plat_move.c on the unit, plat_sim.c on a desk) owns the threads and the hardware; this owns the app.
 *
 * Two contexts, as the firmware's two (its main loop and its audio interrupt):
 *   the audio thread   egm_midi_in (each packet the surface sent), egm_audio (a block), egm_leds_take (LEDs to send)
 *   the UI thread      egm_ui_step (every 2 ms or so: the input, the clock, the stores), egm_frame (the screen)
 * egm_audio and the locked half of egm_ui_step exclude each other with one mutex (priority inheritance); the frame and
 * the stores run outside it, as on the FM-1. */
#ifndef EGM_H
#define EGM_H

#include <stdint.h>

/* flash_path: the store's image (the library, the projects): read now, written back when it changes; 0: RAM only */
void egm_init(const char *flash_path);
void egm_shutdown(void);                                /* everything changed written now */

void egm_midi_in(const uint8_t pkt[4]);                 /* a USB-MIDI packet from the SPI mailbox (cable in pkt[0] >> 4) */
void egm_audio(int16_t *stereo, int frames);            /* 44.1 kHz, interleaved */
int egm_leds_take(uint8_t out[][4], int max);           /* packets for the surface (cable 0); returns how many */

int egm_ui_step(uint32_t now_ms);                       /* 1: the player asked to leave (back to Move); 2: to power off */
int egm_frame(uint8_t packed[1024], uint32_t now_ms);   /* the screen now, packed for the panel; 1: it changed */
/* for the checks (the simulator, the unit's test hook): the state as a line of name=value words */
void egm_describe(char *out, int cap);
/* for the simulator's checks: the selected track, the core's screen (QD_SCR_*), playing */
void egm_state(int *track, int *screen, int *playing);
/* for the simulator: the core's own screen drawn now (240 x 240 RGB565 big-endian) */
const uint16_t *egm_core_lcd(void);

#endif
