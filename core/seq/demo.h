/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The built-in demo: one song in four patterns (demo.c), made through the editing API so it is what the UI would
 * make. 124 BPM, A minor, two bars a pattern:
 *
 *   A01  INTRO   the arp under its filter (its CUTOFF locked a little higher a step), the drone, EVIL .. GENIUS
 *   A02  GROOVE  + the kick, the hats (a syncopated line into the echo), the bass
 *   A03  BUILD   the arp's CUTOFF and RESO locked higher on every step (the sweep), a noise riser, a snare roll that
 *                doubles and ratchets, a lift in fifths, the kick and the bass gone in bar 2, EVIL stuttering
 *   A04  DROP    all of it, the arp open (its pulse a PAL octave arpeggio), the open hat, GENIUS on the one, and
 *                the effects: the stab under PAL arpeggios locked a step (a buzzing chord, octave runs, falls)
 *
 * Played as a song: SEQ held + a pattern's key queues it, and it starts when the playing one ends. */
#pragma once
#include "model.h"
#include "pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QD_DEMO_N 4                     /* A01..A04 */
#define QD_DEMO_NSTEP 32                /* the steps of track 1 that lock a pool sound (two bars) */
void qd_demo_load(qd_project_t *pr);   /* patterns 0..3 written (the rest untouched) */
void qd_demo_pattern(qd_pattern_t *p, int k);   /* demo k (0..3) into p */
void qd_demo_model(qd_model_t *m);     /* the tracks' mix the demo was made in (levels, pans, sends) */
/* (2026-10-24) demo k's pool sound locks without making it: track 1's steps 1..32, each its sound (0 none; every one
 * on, its sound overridden; no other track locks one, no track sound has a source: test_persist checks) */
extern const uint8_t QD_DEMO_DRUMS[QD_DEMO_N][QD_DEMO_NSTEP];

#ifdef __cplusplus
}
#endif
