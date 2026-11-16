# EvilGenius for Move

The [EvilGenius](https://github.com/Quixotic7/evilgenius) SID chip groovebox (an M-VAVE FM-1 firmware) running on
Ableton Move as a standalone [Schwung](https://github.com/charlesvestal/schwung) tool. While it runs, Move's own
software is stopped and EvilGenius owns the pads, the screen and the audio. Leaving it starts Move again.

This repository holds only the Move side. The instrument itself is the EvilGenius core, compiled in from a checkout
of that repository.

## Status

It runs on a Move: sound, surface, screen and real-time audio, with the Move's own pages for the room, the sound
editor, the FX tower, the mix, the knob pop-ups, the setup menu, tempo and patterns. Checked on the unit over ssh with
the test hook (below); the LED colours and the sound itself have only been judged by ear and eye in passing.

Not done yet:

- pictures for four sound pages (AMP, VOICE, MOD, CHIP), which show the core's own drawing, cropped
- booting straight into EvilGenius (it starts from Schwung's Tools menu)
- the power button's menu is written from movy-host's notes and has not been pressed on a unit
- the core's own pages (settings, projects, the browser, tools, MIDI FX) show through a window, with its knobs

## Build

```bash
make test                    # the simulator on this machine, tests/smoke.txt into build/shots
make move                    # the Move binaries, in Docker
make package                 # dist/evilgenius-module.tar.gz
make clean && make package NO_SAM=1   # .. without SAM, the speech synthesizer that has no licence
scripts/deploy.sh --launch   # copy to ableton@move.local and start it
```

`EG` names the EvilGenius checkout (default `../evilgenius`).

### Checking a unit with nobody at it

```bash
scripts/deploy.sh --test                 # start it with the test hook on
scripts/unit.sh run tests/pages.txt      # play a simulator script on the unit; its real screens into build/unit
scripts/unit.sh state                    # the app's state as one line
```

The hook exists only while `/dev/shm/egm-test` does (a reboot clears it). The same scripts run in the simulator:
`build/sim/egm-sim tests/pages.txt build/shots`.

## Controls

| Move | Does |
|---|---|
| 16 step buttons | the steps: tap, hold to edit, as the FM-1's W1-W16 |
| Shift + step 1 / 2 / 5 / 6 / 11 / 14 | projects / setup (settings, exit) / tempo / metronome / MIDI FX / double the track |
| Top pad row | at home the six tracks; in SOUND the 8 sound groups; in FX the floors (pad 8 steps through the last two) |
| Lower three pad rows | notes in the track's scale; a row up is a fourth up (Shift + - / +: octave) |
| Knobs 1-8 | the open page's values; touch or turn for the name and value full screen. At home: velocity, length, micro, ratchet, chance, condition, trig, slide of the held step (none held: the track's defaults); Shift + a knob: its second value |
| Wheel | next / previous page (SOUND, FX), track (MIX), the core's SELECT elsewhere; Shift: the track's sound |
| Wheel touched | all eight values at once |
| Wheel click | choose in the setup menu; yes / enter in the core's own menus |
| Volume | master |
| Track 1-4 | select track 1-4 (Shift + Track 1/2: tracks 5/6) |
| Sampling / Capture | SOUND / FX (again: next floor) |
| Mute | tapped: MIX. Held with a track pad or button: mute it (Shift too: solo) |
| Shift + Sampling / Shift + Capture | save the sound / MIDI FX |
| Shift + a top pad, in SOUND | shake the page's values (the dice) |
| Power button | a menu: CANCEL / POWEROFF |
| Menu / Back | home (again: the next home view) / back |
| Record | arm recording from the pads (stopped: starts with the first note); again: off |
| Loop | held: the step buttons choose the pattern, top pads 1-4 the bank. Tapped: the core's TOOLS |
| Copy (Shift: paste) | the pattern |
| Delete | the core's B11: clear |
| - / + | the core's F1 / F2 layers |
| Left / Right | the previous / next page of 16 steps |
| Undo (Shift: redo) | undo |
| Shift + volume touch + wheel click | leave, back to Move (also in the setup menu) |

## Licence

MPL-2.0, as the core. See `THIRD_PARTY_LICENSES.md`, in particular the note on SAM.
