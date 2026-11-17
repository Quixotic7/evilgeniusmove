/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* THE STORE'S BACKUP AND RESTORE away from the unit (2026-11-15, 0.1.2): the port's bulk commands (firmware/app/app.c
 * app_usb_cmd: FZBI, FZBR, FZSR, FZXD) for a build with no fm1-base under it, FM1VST's glue first (the .rfm1 RemoteFM1
 * plays: core_quadrille.c), with the same frames, the same error codes and the same meaning, so EvilGenius's web manager
 * (site/manager/eglink.js) backs up and restores the player as it does a unit, and the files are the same (the store
 * in the Bluetooth package's layout: qd_config.h QD_STORE_BLE). Pure C, no OS: the flash is the store's three hooks
 * (store.h qd_flash_t), the wire and the app are the caller's (qd_bulk_io_t). FIZZ's glue can take it as it is.
 *
 * THE FRAMES are the update's (fm1-base, Felucca's ota.c): F0 pack7(00 59 cmd len16 00 body ~sum) F7. The caller
 * decodes them (ota.c ota_service: its ota_dec, the handshake already answered there) and gives qd_bulk_frame each one
 * of cmd 46 (the bulk commands) and 30 (the answers to this side's read requests). Bodies, little-endian words:
 *   46 "FZBI"              -> 46 "FZBI" lo size crc      the store's offset in the unit's flash (0xAF000), its size,
 *                                                       its CRC-32 now (the writes still waiting written first)
 *   46 "FZBR" off n        -> 46 "FZBD" off <n bytes>    n <= 256 bytes of it; outside it: "FZER" 11
 *   46 "FZSR" len crc      the restore: "FZER" 10 while playing, 11 for another size; the image pulled whole with the
 *                          update's read requests (30: flag 0, addr u32, len u24: 512 bytes from address 0 up), its
 *                          CRC-32 checked before anything is erased (12); then each sector erased and written from a
 *                          second pull (13: a write failed, or the host silent), the region read back (14: other than
 *                          the image), the app started again on it: "FZOK" 1. A request not answered is asked again
 *                          every second, 90 times (the unit's fz_pull) before the restore gives up
 *   46 "FZXD" ..           the unit's data blob: the player has its data in it already: "FZOK" 0 (the unit's answer
 *                          when its blob is there)
 * Unlike the unit's, the restore does not block: it runs across frames and ticks (qd_bulk_tick), and while it runs
 * (qd_bulk_busy) the caller holds the app (no ticks, no sound, no input: io.hold), as the unit's loop is held. */
#pragma once
#include <stdint.h>

#include "../store/store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_BULK_CHUNK 256u             /* FZBR's bytes at most (app.c FZ_BK_CHUNK) */
#define QD_BULK_PULL 512u              /* a restore's read request (app.c fz_restore's buffer) */
#define QD_BULK_TRY_MS 1000u           /* a read request asked again after this long (app.c FZ_PULL_MS) .. */
#define QD_BULK_TRIES 90u              /* .. this many times at most (FZ_PULL_TRIES x ota.c's 3) */
#define QD_BULK_CMD 0x46u              /* the bulk commands' cmd byte */
#define QD_BULK_READ 0x30u             /* the update's read request and its answer */

typedef struct {
    void *ctx;
    int (*send)(void *ctx, const uint8_t *wire, uint32_t n);   /* one frame F0 .. F7 to the host; 0 ok */
    int (*playing)(void *ctx);         /* the sequencer playing (a restore refused: FZER 10) */
    void (*flush)(void *ctx);          /* the store's writes still waiting, written now (FZBI, FZBR at 0) */
    void (*hold)(void *ctx, int on);   /* 1: a restore begins (the app held: no writes, no sound); 0: it ended */
    void (*restart)(void *ctx);        /* the region written (restored, or cut short): the app again on what it holds
                                        * (qd_bulk_foreign / qd_bulk_wipe first, as the unit's start) */
} qd_bulk_io_t;

typedef struct {
    qd_bulk_io_t io;
    qd_flash_t fl;                     /* the flash .. */
    uint32_t base, size;               /* .. the store's region in it (offsets as fl's), whole sectors */
    uint32_t lo;                       /* its offset as FZBI says it (the unit's flash: 0xAF000) */
    /* the restore */
    uint8_t phase;                     /* 0 none, 1 the image pulled and checked, 2 written */
    uint32_t len, crc;                 /* the image's, as FZSR said */
    uint32_t a, c;                     /* the next address asked for; the CRC-32 so far (phase 1) */
    uint32_t asked_ms, tries;          /* the request's last send, its sends */
    uint32_t ms;                       /* the caller's clock, last seen */
    uint32_t rc;                       /* the last restore's code (0 done, else FZER's), for the tests */
    uint32_t reqs, answers;            /* read requests sent, answers taken (the tests) */
} qd_bulk_t;

void qd_bulk_init(qd_bulk_t *b, const qd_bulk_io_t *io, const qd_flash_t *fl, uint32_t base, uint32_t size,
                  uint32_t lo);
/* a decoded frame in the update's framing (00 59 cmd len16 00 body ~sum, its checksum checked by the caller: ota.c
 * ota_take): 1 it was a bulk command or an answer this side waits for (taken), 0 not ours */
int qd_bulk_frame(qd_bulk_t *b, const uint8_t *d, uint32_t n, uint32_t ms);
void qd_bulk_tick(qd_bulk_t *b, uint32_t ms);   /* each ms (or as often): a restore's request asked again */
int qd_bulk_busy(const qd_bulk_t *b);           /* a restore running: the app held */
uint32_t qd_bulk_crc32(uint32_t c, const uint8_t *p, uint32_t n);   /* zlib's, as the unit's fz_crc32 */
uint32_t qd_bulk_region_crc(const qd_bulk_t *b);
/* the region's sectors neither blank nor the store's (magic QDS_MAGIC) when none is the store's (else 0): another
 * firmware's, another layout's (the .rfm1's host layout before 0.1.2), a restore cut short; and every non-blank
 * sector erased (the sectors erased). The unit's fz_store_foreign / fz_store_wipe */
int qd_bulk_foreign(const qd_bulk_t *b);
int qd_bulk_wipe(const qd_bulk_t *b);

#ifdef __cplusplus
}
#endif
