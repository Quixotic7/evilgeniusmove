/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* EvilGenius: SAM inside the firmware. Every global of SAM's C port gets a sam_ prefix (it has names such as A, X,
 * Y, input, buffer, speed), its debug output is compiled out, and its output buffer is a fixed array (SAM_BUF_LEN
 * samples at the words' 6 kHz, writes past it dropped) instead of 10 seconds from malloc. Included by SAM's .c files after
 * the C library's headers and before SAM's own, and by core/voice/sam/say.c. See README.md here. */
#pragma once
#include "../../qd_config.h"
/* SAM's clock runs 50 ticks a sample of its 22050 Hz; its output is written a sample every SAM_DIV ticks instead:
 * 184 ticks is 5992 Hz, the words' rate (QD_VOX_FS), so the buffer is the word as the voices play it and a second of
 * speech is 6 KB, not 22 (the C64's own SAM spoke at about this rate). A sample is the last value SAM gave it */
#define SAM_DIV 184
#define SAM_AHEAD 2                     /* (SAM writes five 22 kHz samples ahead: about one and a half of these) */
#ifndef SAM_BUF_LEN
#define SAM_BUF_LEN (6000u * QD_SAY_TENTHS / 10u)
#endif
#define debug 0
#define PrintOutput(...) ((void)0)
#define PrintPhonemes(...) ((void)0)
#define PrintRule(...) ((void)0)
#define printf(...) ((void)0)
#define puts(...) ((void)0)
#define A sam_A
#define AddInflection sam_AddInflection
#define AdjustLengths sam_AdjustLengths
#define Code37055 sam_Code37055
#define Code37066 sam_Code37066
#define Code41240 sam_Code41240
#define Code47503 sam_Code47503
#define CopyStress sam_CopyStress
#define EnableSingmode sam_EnableSingmode
#define GetBuffer sam_GetBuffer
#define GetBufferLength sam_GetBufferLength
#define GetRuleByte sam_GetRuleByte
#define Init sam_Init
#define Insert sam_Insert
#define InsertBreath sam_InsertBreath
#define Output8Bit sam_Output8Bit
#define Output8BitAry sam_Output8BitAry
#define Parser1 sam_Parser1
#define Parser2 sam_Parser2
#define PrepareOutput sam_PrepareOutput
#define Read sam_Read
#define Render sam_Render
#define RenderSample sam_RenderSample
#define SAMMain sam_SAMMain
#define SetInput sam_SetInput
#define SetMouth sam_SetMouth
#define SetMouthThroat sam_SetMouthThroat
#define SetPhonemeLength sam_SetPhonemeLength
#define SetPitch sam_SetPitch
#define SetSpeed sam_SetSpeed
#define SetThroat sam_SetThroat
#define TextToPhonemes sam_TextToPhonemes
#define Write sam_Write
#define X sam_X
#define Y sam_Y
#define ampl1data sam_ampl1data
#define ampl2data sam_ampl2data
#define ampl3data sam_ampl3data
#define amplitude1 sam_amplitude1
#define amplitude2 sam_amplitude2
#define amplitude3 sam_amplitude3
#define amplitudeRescale sam_amplitudeRescale
#define blendRank sam_blendRank
#define buffer sam_buffer
#define bufferpos sam_bufferpos
#define flags sam_flags
#define flags2 sam_flags2
#define freq1data sam_freq1data
#define freq2data sam_freq2data
#define freq3data sam_freq3data
#define frequency1 sam_frequency1
#define frequency2 sam_frequency2
#define frequency3 sam_frequency3
#define inBlendLength sam_inBlendLength
#define input sam_input
#define mem39 sam_mem39
#define mem44 sam_mem44
#define mem47 sam_mem47
#define mem49 sam_mem49
#define mem50 sam_mem50
#define mem51 sam_mem51
#define mem53 sam_mem53
#define mem56 sam_mem56
#define mem59 sam_mem59
#define mouth sam_mouth
#define outBlendLength sam_outBlendLength
#define phonemeIndexOutput sam_phonemeIndexOutput
#define phonemeLength sam_phonemeLength
#define phonemeLengthOutput sam_phonemeLengthOutput
#define phonemeLengthTable sam_phonemeLengthTable
#define phonemeStressedLengthTable sam_phonemeStressedLengthTable
#define phonemeindex sam_phonemeindex
#define pitch sam_pitch
#define pitches sam_pitches
#define rectangle sam_rectangle
#define rules sam_rules
#define rules2 sam_rules2
#define sampleTable sam_sampleTable
#define sampledConsonantFlag sam_sampledConsonantFlag
#define sampledConsonantFlags sam_sampledConsonantFlags
#define signInputTable1 sam_signInputTable1
#define signInputTable2 sam_signInputTable2
#define singmode sam_singmode
#define sinus sam_sinus
#define speed sam_speed
#define stress sam_stress
#define stressInputTable sam_stressInputTable
#define stressOutput sam_stressOutput
#define tab36376 sam_tab36376
#define tab37489 sam_tab37489
#define tab37515 sam_tab37515
#define tab47492 sam_tab47492
#define tab48426 sam_tab48426
#define throat sam_throat
#define timetable sam_timetable
#define trans sam_trans
#define wait1 sam_wait1
#define wait2 sam_wait2
#define EnableDebug sam_EnableDebug
#define ResetOutput sam_ResetOutput
