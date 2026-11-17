/* SPDX-License-Identifier: MPL-2.0 */
/* Session's page belt, after the design board (its frames, design/evil-genius.movedesign.json item 39): each
 * page's icon, the brain machine. Rows MSB first (generated: the board's pixels, cropped) */
#pragma once
#include <stdint.h>

typedef struct { uint8_t w, h; uint32_t row[24]; } belt_icon_t;
static const belt_icon_t BELT_ICON[7] = {
    { 15, 8, { 0xFBEE0000u, 0xFBEE0000u, 0x00000000u, 0xFBE00000u, 0xFBE00000u, 0x00000000u, 0xF83E0000u, 0xF83E0000u } },   /* CLIPS */
    { 15, 13, { 0x001E0000u, 0x00120000u, 0x00120000u, 0x01F20000u, 0x01020000u, 0x01020000u, 0x1F020000u, 0x10020000u, 0x10020000u, 0xF0020000u, 0x80020000u, 0x80020000u, 0xFFFE0000u } },   /* MIDI FX */
    { 15, 11, { 0x01000000u, 0x01000000u, 0x11100000u, 0x09200000u, 0x04400000u, 0xE00E0000u, 0x04400000u, 0x09200000u, 0x11100000u, 0x01000000u, 0x01000000u } },   /* FX */
    { 13, 7, { 0x42100000u, 0x42380000u, 0x47100000u, 0xE2100000u, 0x42100000u, 0x42100000u, 0x42100000u } },   /* MIX */
    { 15, 7, { 0xFFFE0000u, 0x82020000u, 0xFFFE0000u, 0x82020000u, 0xFFFE0000u, 0x82020000u, 0xFFFE0000u } },   /* SCENES */
    { 15, 9, { 0xF77E0000u, 0xF77E0000u, 0x07000000u, 0xFEF00000u, 0xFEF00000u, 0x00000000u, 0x20000000u, 0x20000000u, 0x20000000u } },   /* ARRANGE */
    { 15, 6, { 0xAAAA0000u, 0x00000000u, 0xD3580000u, 0xD3580000u, 0x00000000u, 0xAAAA0000u } },   /* SEQ */
};
#define BELT_MX 100                           /* the machine: its left, its width, its rows from the top */
#define BELT_MW 28
static const uint32_t BELT_MACHINE[42] = {
    0x00000000u, 0x101FF800u, 0x0E1FF830u, 0x037FFE60u, 0x03FFFFC0u, 0x02400140u,
    0x0222A240u, 0x020B6840u, 0x02BF7EC0u, 0x023F7E40u, 0x027F7F40u, 0x023FFE40u,
    0x02BBEEC0u, 0x021FFC40u, 0x021AAC40u, 0x02000040u, 0x028080C0u, 0x02000040u,
    0x02222240u, 0x02000040u, 0x028888C0u, 0x02000040u, 0x02222240u, 0x03FFFFC0u,
    0x00000000u, 0x00000000u, 0x3FFFFFF0u, 0x3FFFFFF0u, 0x20000010u, 0x20000010u,
    0x20000010u, 0x2039CE10u, 0x2039CE10u, 0x20000010u, 0x20000010u, 0x3FFFFFF0u,
    0xF0000030u, 0x30000030u, 0x30000030u, 0xB0000030u, 0x3FFFFFF0u, 0xFFFFFFF0u,
};
