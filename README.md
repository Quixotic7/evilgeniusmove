# EvilGenius for Move

The [EvilGenius](https://github.com/Quixotic7/evilgenius) SID chip groovebox (an M-VAVE FM-1 firmware) running on
Ableton Move as a standalone [Schwung](https://github.com/charlesvestal/schwung) tool. While it runs, Move's own
software is stopped and EvilGenius owns the pads, the screen and the audio. Leaving it starts Move again.

This repository holds only the Move side. The instrument itself is the EvilGenius core, compiled in from a checkout
of that repository.

## Status

Early. It runs on a Move (sound, surface, screen, real-time audio). The Move's own pages are in for the sound editor,
the FX tower, the mix, the knob pop-ups, the setup menu and tempo.

Not done yet:

- hand-drawn pictures for every page: the oscillators, the envelopes and the mix are drawn for this screen; the other
  sound pages, the FX floors and the room still show the core's own picture, cropped to its drawing
- the power button, and booting straight into EvilGenius
- the core's B11 in the sound editor's dice and bottle (randomise) have no control here

## Build

```bash
make test        # the simulator on this machine, tests/smoke.txt into build/shots
make move        # the Move binaries, in Docker
make package     # dist/evilgenius-module.tar.gz
```

`EG` names the EvilGenius checkout (default `../evilgenius`).

## Controls

| Move | Does |
|---|---|
| 16 step buttons | the steps: tap, hold to edit, as the FM-1's W1-W16 |
| Shift + step 1 / 2 / 5 / 6 / 11 / 14 | projects / setup (settings, exit) / tempo / metronome / MIDI FX / double the track |
| Top pad row | page keys: the hold modes at home, the 8 sound groups in SOUND, the floors in FX (pad 8 steps through the keys past the eighth) |
| Lower three pad rows | notes in the track's scale; a row up is a fourth up (Shift + - / +: octave) |
| Knobs 1-8 | the open page's values; touch or turn for the name and value full screen |
| Wheel | next / previous page (SOUND, FX), track (MIX), the core's SELECT elsewhere; Shift: the track's sound |
| Wheel touched | all eight values at once |
| Wheel click | choose in the setup menu; yes / enter in the core's own menus |
| Volume | master |
| Track 1-4 | select track 1-4 (Shift + Track 1/2: tracks 5/6) |
| Sampling / Capture / Mute | SOUND / FX (again: next floor) / MIX |
| Shift + Sampling / Shift + Capture | save the sound / MIDI FX |
| Menu / Back | home (again: the next home view) / back |
| Record | arm recording from the pads (stopped: starts with the first note); again: off |
| Loop | the core's SEQ: tools, patterns |
| Copy (Shift: paste) | the pattern |
| Delete | the core's B11: clear |
| - / + | the core's F1 / F2 layers |
| Left / Right | the previous / next page of 16 steps |
| Undo (Shift: redo) | undo |
| Shift + volume touch + wheel click | leave, back to Move (also in the setup menu) |

## Licence

MPL-2.0, as the core. See `THIRD_PARTY_LICENSES.md`, in particular the note on SAM.
