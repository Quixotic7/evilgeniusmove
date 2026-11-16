/* SPDX-License-Identifier: MPL-2.0 */
#include <stdio.h>
#include <string.h>

#include "flash.h"

#define QUIET_MS 2000u

static uint8_t g_img[EGM_FLASH_SIZE];
static char g_path[512];
static int g_dirty;
static uint32_t g_changed_ms, g_now_ms;

static int f_read(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    (void)ctx;
    if (off > EGM_FLASH_SIZE || n > EGM_FLASH_SIZE - off)
        return -1;
    memcpy(dst, g_img + off, n);
    return 0;
}

static int f_erase(void *ctx, uint32_t off)
{
    (void)ctx;
    if (off >= EGM_FLASH_SIZE)
        return -1;
    memset(g_img + (off & ~4095u), 0xFF, 4096);
    g_dirty = 1, g_changed_ms = g_now_ms;
    return 0;
}

static int f_prog(void *ctx, uint32_t off, const void *src, uint32_t n)
{
    uint32_t i;
    (void)ctx;
    if (off > EGM_FLASH_SIZE || n > EGM_FLASH_SIZE - off)
        return -1;
    for (i = 0; i < n; i++)                             /* NOR: a program only clears bits */
        g_img[off + i] &= ((const uint8_t *)src)[i];
    g_dirty = 1, g_changed_ms = g_now_ms;
    return 0;
}

static const qd_flash_t g_fl = { f_read, f_erase, f_prog, 0 };

const qd_flash_t *flash_open(const char *path)
{
    FILE *f;
    memset(g_img, 0xFF, sizeof g_img);                  /* an erased flash: the first project, the demos */
    g_path[0] = 0;
    if (path) {
        snprintf(g_path, sizeof g_path, "%s", path);
        if ((f = fopen(path, "rb"))) {
            const size_t n = fread(g_img, 1, sizeof g_img, f);
            fclose(f);
            if (n != sizeof g_img) {                    /* a short file is not an image: start erased, keep it aside */
                char bad[540];
                memset(g_img, 0xFF, sizeof g_img);
                snprintf(bad, sizeof bad, "%s.bad", path);
                rename(path, bad);
                fprintf(stderr, "flash: %s was %zu bytes, not %u; kept as %s\n", path, n, EGM_FLASH_SIZE, bad);
            }
        }
    }
    return &g_fl;
}

void flash_flush(void)
{
    char tmp[540];
    FILE *f;
    if (!g_dirty || !g_path[0])
        return;
    snprintf(tmp, sizeof tmp, "%s.tmp", g_path);
    if (!(f = fopen(tmp, "wb"))) {
        fprintf(stderr, "flash: cannot write %s\n", tmp);
        return;
    }
    if (fwrite(g_img, 1, sizeof g_img, f) == sizeof g_img && fclose(f) == 0 && rename(tmp, g_path) == 0)
        g_dirty = 0;
    else
        fprintf(stderr, "flash: writing %s failed; the last good image is kept\n", g_path);
}

void flash_poll(uint32_t now_ms)
{
    g_now_ms = now_ms;
    if (g_dirty && now_ms - g_changed_ms >= QUIET_MS)
        flash_flush();
}
