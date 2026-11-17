/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The store's backup and restore away from the unit (qd_bulk.h): firmware/app/app.c app_usb_cmd's FZBI, FZBR, FZSR and
 * FZXD, the restore's blocking pull made a state machine. */
#include <string.h>

#include "qd_bulk.h"

static uint32_t bk_rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t bk_rd24(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16; }
static void bk_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}
#define BK_TAG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)

uint32_t qd_bulk_crc32(uint32_t c, const uint8_t *p, uint32_t n)
{
    uint32_t i, k;
    c = ~c;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* F0 pack7(00 59 cmd len16 00 body ~sum) F7 (7 bits a byte, the low bits first: ota.c ota_pack7, app.c fz_usb_frame) */
static void bk_send(qd_bulk_t *b, uint8_t cmd, const uint8_t *body, uint32_t nbody)
{
    uint8_t m[7 + 8 + QD_BULK_CHUNK], w[(7 + 8 + QD_BULK_CHUNK) * 8 / 7 + 4];
    uint32_t i, s = 0, n = 0, k = 0, acc = 0, nb = 0;
    m[n++] = 0, m[n++] = 0x59, m[n++] = cmd, m[n++] = (uint8_t)nbody, m[n++] = (uint8_t)(nbody >> 8), m[n++] = 0;
    for (i = 0; i < nbody; i++)
        s += m[n++] = body[i];
    m[n++] = (uint8_t)~s;
    w[k++] = 0xF0;
    for (i = 0; i < n; i++) {
        acc |= (uint32_t)m[i] << nb;
        nb += 8u;
        while (nb >= 7u) {
            w[k++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        w[k++] = (uint8_t)(acc & 0x7Fu);
    w[k++] = 0xF7;
    if (b->io.send)
        b->io.send(b->io.ctx, w, k);
}
static void bk_reply(qd_bulk_t *b, const char *tag, uint32_t code)
{
    uint8_t r[8];
    memcpy(r, tag, 4);
    bk_wr32(r + 4, code);
    bk_send(b, QD_BULK_CMD, r, 8u);
}
static void bk_ask(qd_bulk_t *b)                 /* the read request for the restore's next 512 bytes (ota.c ota_read) */
{
    uint8_t q[8] = {0};
    bk_wr32(q + 1, b->a);
    q[5] = (uint8_t)QD_BULK_PULL, q[6] = (uint8_t)(QD_BULK_PULL >> 8);
    bk_send(b, QD_BULK_READ, q, 8u);
    b->asked_ms = b->ms;
    b->tries++;
    b->reqs++;
}

void qd_bulk_init(qd_bulk_t *b, const qd_bulk_io_t *io, const qd_flash_t *fl, uint32_t base, uint32_t size, uint32_t lo)
{
    memset(b, 0, sizeof *b);
    b->io = *io;
    b->fl = *fl;
    b->base = base, b->size = size, b->lo = lo;
}

int qd_bulk_busy(const qd_bulk_t *b) { return b->phase != 0; }

uint32_t qd_bulk_region_crc(const qd_bulk_t *b)
{
    uint8_t buf[QD_BULK_PULL];
    uint32_t a, c = 0;
    for (a = 0; a < b->size; a += sizeof buf) {
        if (b->fl.read(b->fl.ctx, b->base + a, buf, sizeof buf))
            return 0;
        c = qd_bulk_crc32(c, buf, sizeof buf);
    }
    return c;
}

/* a sector: 1 the store's (its magic), 0 blank, -1 anything else (-2: unread) */
static int bk_sector(const qd_bulk_t *b, uint32_t k)
{
    uint8_t buf[QD_BULK_PULL];
    uint32_t a, i;
    for (a = 0; a < QDS_SECTOR; a += sizeof buf) {
        if (b->fl.read(b->fl.ctx, b->base + k + a, buf, sizeof buf))
            return -2;
        if (!a && bk_rd32(buf) == QDS_MAGIC)
            return 1;
        for (i = 0; i < sizeof buf; i++)
            if (buf[i] != 0xFFu)
                return -1;
    }
    return 0;
}

int qd_bulk_foreign(const qd_bulk_t *b)
{
    uint32_t k;
    int other = 0;
    for (k = 0; k < b->size; k += QDS_SECTOR) {
        const int s = bk_sector(b, k);
        if (s == 1 || s == -2)
            return 0;
        other += s < 0;
    }
    return other;
}

int qd_bulk_wipe(const qd_bulk_t *b)
{
    uint32_t k;
    int n = 0;
    for (k = 0; k < b->size; k += QDS_SECTOR)
        if (bk_sector(b, k) && !b->fl.erase(b->fl.ctx, b->base + k))
            n++;
    return n;
}

/* the restore's end: the app again when the region was written (done, 13, 14: as app.c's FZSR), the answer */
static void bk_end(qd_bulk_t *b, uint32_t rc)
{
    const int wrote = b->phase == 2;
    b->phase = 0;
    b->rc = rc;
    if (wrote && b->io.restart)
        b->io.restart(b->io.ctx);
    if (b->io.hold)
        b->io.hold(b->io.ctx, 0);
    bk_reply(b, rc ? "FZER" : "FZOK", rc ? rc : 1u);
}

/* an answer to the read request out (ota.c ota_read's test): its 512 bytes checked (phase 1) or written (phase 2) */
static void bk_answer(qd_bulk_t *b, const uint8_t *data)
{
    uint32_t p;
    b->answers++;
    if (b->phase == 1) {
        b->c = qd_bulk_crc32(b->c, data, QD_BULK_PULL);
        b->a += QD_BULK_PULL;
        if (b->a < b->len) {
            b->tries = 0;
            bk_ask(b);
            return;
        }
        if (b->c != b->crc) {
            bk_end(b, 12);                       /* (nothing erased for a bad image) */
            return;
        }
        b->phase = 2, b->a = 0, b->tries = 0;    /* (app.c: app_panic, the message: the app is held already) */
        bk_ask(b);
        return;
    }
    if (!(b->a & (QDS_SECTOR - 1u)) && b->fl.erase(b->fl.ctx, b->base + b->a)) {
        bk_end(b, 13);
        return;
    }
    for (p = 0; p < QD_BULK_PULL; p += QDS_PAGE)   /* (the flash's prog: within one page) */
        if (b->fl.prog(b->fl.ctx, b->base + b->a + p, data + p, QDS_PAGE)) {
            bk_end(b, 13);
            return;
        }
    b->a += QD_BULK_PULL;
    if (b->a < b->len) {
        b->tries = 0;
        bk_ask(b);
        return;
    }
    bk_end(b, qd_bulk_region_crc(b) == b->crc ? 0u : 14u);
}

int qd_bulk_frame(qd_bulk_t *b, const uint8_t *d, uint32_t n, uint32_t ms)
{
    uint32_t tag, off, len;
    b->ms = ms;
    if (n < 7u || d[0] != 0 || d[1] != 0x59u)
        return 0;
    if (d[2] == QD_BULK_READ) {                  /* an answer: the one asked for, else a late duplicate (ignored) */
        if (!b->phase)
            return 0;
        if (n == QD_BULK_PULL + 15u && bk_rd24(d + 3) == QD_BULK_PULL + 8u && bk_rd32(d + 7) == b->a &&
            bk_rd24(d + 11) == QD_BULK_PULL)
            bk_answer(b, d + 14);
        return 1;
    }
    if (d[2] != QD_BULK_CMD || n < 7u + 4u)
        return 0;
    tag = bk_rd32(d + 6);
    if (b->phase)
        return 1;                                /* (a restore running: as the unit's pull loop, nothing else heard) */
    if (tag == BK_TAG('F', 'Z', 'B', 'I')) {
        uint8_t r[16];
        if (b->io.flush)
            b->io.flush(b->io.ctx);
        memcpy(r, "FZBI", 4);
        bk_wr32(r + 4, b->lo), bk_wr32(r + 8, b->size), bk_wr32(r + 12, qd_bulk_region_crc(b));
        bk_send(b, QD_BULK_CMD, r, 16u);
        return 1;
    }
    if (tag == BK_TAG('F', 'Z', 'B', 'R')) {
        uint8_t r[8 + QD_BULK_CHUNK];
        if (n < 7u + 12u)
            return 1;
        off = bk_rd32(d + 10), len = bk_rd32(d + 14);
        if (len > QD_BULK_CHUNK || off > b->size || len > b->size - off) {
            bk_reply(b, "FZER", 11);
            return 1;
        }
        if (!off && b->io.flush)                 /* (a read from the start: a backup's, MY PRESETS' list) */
            b->io.flush(b->io.ctx);
        if (len && b->fl.read(b->fl.ctx, b->base + off, r + 8, len)) {
            bk_reply(b, "FZER", 11);
            return 1;
        }
        memcpy(r, "FZBD", 4);
        bk_wr32(r + 4, off);
        bk_send(b, QD_BULK_CMD, r, 8u + len);
        return 1;
    }
    if (tag == BK_TAG('F', 'Z', 'S', 'R')) {
        if (n < 7u + 12u)
            return 1;
        len = bk_rd32(d + 10);
        if (b->io.playing && b->io.playing(b->io.ctx)) {
            b->rc = 10;
            bk_reply(b, "FZER", 10);
            return 1;
        }
        if (len != b->size || !len || len % QD_BULK_PULL) {
            b->rc = 11;
            bk_reply(b, "FZER", 11);
            return 1;
        }
        b->len = len, b->crc = bk_rd32(d + 14);
        b->phase = 1, b->a = 0, b->c = 0, b->tries = 0;
        if (b->io.hold)
            b->io.hold(b->io.ctx, 1);
        bk_ask(b);
        return 1;
    }
    if (tag == BK_TAG('F', 'Z', 'X', 'D')) {
        if (n >= 7u + 16u)
            bk_reply(b, "FZOK", 0);              /* (the player's data is in it: the unit's answer for its blob there) */
        return 1;
    }
    return 0;
}

void qd_bulk_tick(qd_bulk_t *b, uint32_t ms)
{
    b->ms = ms;
    if (!b->phase || ms - b->asked_ms < QD_BULK_TRY_MS)
        return;
    if (b->tries >= QD_BULK_TRIES) {
        bk_end(b, b->phase == 1 ? 12u : 13u);   /* (the host went away: app.c fz_pull's -1) */
        return;
    }
    bk_ask(b);
}
