/* SPDX-License-Identifier: MPL-2.0 */
/* The core's NOR flash as a file: the image lives in RAM (the core's read / erase / program hooks work on it, on the UI
 * thread), and is written to its file, whole and atomically, once it has been quiet for a moment. */
#ifndef EGM_FLASH_H
#define EGM_FLASH_H

#include <stdint.h>
#include "store.h"

#define EGM_FLASH_SIZE 0x100000u        /* the host profile's image: persist.h QD_FLASH_PRJ + its sectors */

const qd_flash_t *flash_open(const char *path);         /* path 0: RAM only */
void flash_poll(uint32_t now_ms);                       /* writes the file when changed and quiet for 2 s */
void flash_flush(void);                                 /* now */

#endif
