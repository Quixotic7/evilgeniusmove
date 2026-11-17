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

The clip workflow (`design/WORKFLOW.md`) is being built in stages. Stage 1 (Sequence's four modes on the track
buttons, their top rows, Mute held, the hold shortcuts) is in; `tests/modes.txt` exercises it. Stage 2 (8 tracks, 16
pages of 16 steps, page mode on Loop) is in; `tests/eight.txt` exercises it. Stage 3 (clips and Session's CLIPS: each
track's clips launched and stopped on their own, scenes) is in; `tests/session.txt` exercises it.

A clip is a pattern's part of one track: slot k of a track is pattern A(k+1)'s part, so a pattern is a scene. The core
plays each track's clip on its own (`QD_CLIPS`, on in the host profile the Move builds; the FM-1 leaves it out). The
sound stays with the track, as on Move: launching a clip, or editing one, carries the track's sound into it (the demo
patterns' own sounds give way to the track's once they are played or edited).

The Move builds the core with `-DQD_TRACKS=8 -DQD_NSTEP=256` (the FM-1 keeps 6 tracks and 8 pages). The projects store
of other sizes is not read: **the first launch of this build on a unit erases the saved projects** (MY PRESETS are
kept). The FM-1's JUMP condition reaches only the first 128 steps.

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
scripts/deploy.sh --test                 # start it with the test hook on, SILENT
scripts/deploy.sh --test 5               # .. or at 5% of the output
scripts/unit.sh run tests/pages.txt      # play a simulator script on the unit; its real screens into build/unit
scripts/unit.sh state                    # the app's state as one line
```

The hook exists only while `/dev/shm/egm-test` does (a reboot clears it). The same scripts run in the simulator:
`build/sim/egm-sim tests/pages.txt build/shots`.

### Rules for testing on a unit

1. **A test is quiet.** The Move's speaker is loud. Every unattended run starts with `scripts/deploy.sh --test`,
   which is silent. Ask for sound only when someone will listen, and then at 10% or less (`--test 5`).
   Never use `--launch` for a scripted run: that plays at full level.
2. **Check sound by number, not by ear.** The state line carries `vol=` (the percent in force) and `peak=` (the
   loudest sample since the last state, measured before the volume is applied; 32767 is full scale). A silent run
   with `peak=0` while a pattern plays is a failure.
3. **Read `late=`** after any run with playback: it must stay 0.
4. **Leave Move running.** When the run ends, stop EvilGenius so Move's own software comes back, and say what the
   run changed in the project on the unit.

## Controls

| Move | Does |
|---|---|
| 16 step buttons | the steps: tap, hold to edit, as the FM-1's W1-W16. In page mode: the clip's 16 pages (below) |
| Shift + step 1 / 2 / 5 / 6 / 11 / 14 | projects / setup (settings, exit) / tempo / metronome / MIDI FX / double the track |
| Top pad row | the mode's shortcuts (below). Hidden: a fourth row of notes |
| Lower three pad rows | notes in the track's scale; a row up is a fourth up (Shift + - / +: octave) |
| Knobs 1-8 | the open page's values; touch or turn for the name and value full screen. At home: velocity, length, micro, ratchet, chance, condition, trig, slide of the held step (none held: the track's defaults); Shift + a knob: its second value. A track pad held: its sends (CHORUS, ECHO, ROOM, CRUSH, ROBOT, WARP, GLITCH, TV) |
| Wheel | next / previous page (SOUND, FX), track (MIX), the page of steps at home. In page mode: the clip's length in pages. A step held: its micro timing. A track pad held: its pan |
| Shift + wheel | the sounds: turn to hear each on the track, click to take it, Back to keep your own. With a step held: that step's own sound |
| Wheel touched | all eight values at once |
| Wheel click | choose in the setup menu; yes / enter in the core's own menus |
| Volume | master. A step held: its velocity. A track pad held: the track's volume |
| Track 1-4 | Sequence's modes: 1 TRACKS, 2 TRIG / SOUND, 3 FX, 4 TOOLS. Tap: the mode and its top row; the mode's own button tapped: its top row hidden (all four rows notes) until a button is tapped again. Held: that mode's top row while held |
| Sampling / Capture | SOUND / FX (again: next floor) |
| Mute | tapped: the selected track muted / heard. Held: the top row is the tracks: tap mutes, double tap or Shift + tap solos. Shift + Mute tapped: MIX |
| Shift + Sampling / Shift + Capture | save the sound / MIDI FX |
| Top row in TRACKS | the 8 tracks: tap selects; held, Volume / the wheel / the knobs are its volume, pan, sends |
| Top row in TRIG / SOUND | pages: TRIG (AMP, VOICE), OSC (1-3, MOD), SPEECH (WORD, SAY, DIGI), FILTER (and CHIP), ENV (1-3), LFO (1-3): tap again for the next, each remembers its last; pads 7 / 8 held a second: the page / the whole sound randomized (Shift + a page pad: the page at once) |
| Top row in FX | the floors: SENDS, CHORUS, ECHO, ROOM, CRUSH, ROBOT, then WARP / GLITCH / TV on pad 7 (pad 8, PERFORM: not built yet) |
| Top row in TOOLS | the core's TOOLS keys |
| Shift + a top pad, in SOUND | shake the page's values (the dice) |
| Power button | a menu: CANCEL / POWEROFF |
| Menu | Sequence <-> Session. Shift + Menu: the core's HOME (again: the next home view) |
| Back | out of whatever page is open |
| Track 1-4, in Session | Session's modes: 1 CLIPS, 2 MIDI FX (the core's page), 3 FX, 4 MIX |
| Pads, in CLIPS | 8 tracks (columns) x 4 clip slots (rows, of 16). Tap: the clip launched at the next bar (stopped: at once); an empty slot: chosen (the clip edited in Sequence; a step put there launches it) and the track stopped. Two taps: the track stopped. Shift + a pad: that row's scene (an empty slot stops its track). Green: playing; blinking: queued or stopping; the track's colour: a clip; white: the clip edited |
| Steps, in CLIPS | two under each track: the first stops it (at the bar), the second mutes it (Shift: solo) |
| Copy / Delete held + a clip, in CLIPS | copied, then pasted where tapped / cut, then pasted where tapped (released first: deleted) |
| - / +, Left / Right, the wheel, in CLIPS | the slots shown a row / 4 rows / a row a detent; Left twice: the first four |
| Shift + Play, in Session | every clip stopped at the next bar, the transport running |
| Record | arm recording from the pads (stopped: starts with the first note); again: off |
| Loop | page mode: tapped, on (again: off); held, while held. Shift + Loop held: the step buttons launch a pattern's clips on every track (a scene), top pads 1-4 the bank |
| A page step, in page mode | tap: the page shown; held: out of the loop / back in; two taps: that page alone (again: the loop back); one held and another tapped: the loop from one to the other. White shown, green playing, blue in the loop |
| Pad rows 3-4, in page mode | the shown page's 16 steps: tap its start, two taps its end, one held and another tapped both |
| Copy / Delete held + a page step | the page copied, then pasted where tapped / the page cleared |
| Copy (Shift: paste) | the pattern (page mode: see above) |
| Delete | the core's B11: clear |
| - / + | the core's F1 / F2 layers |
| Left / Right | the previous / next page of 16 steps (16 pages). The page shown stays put while it plays; the side the playhead is on lights |
| Undo (Shift: redo) | undo |
| Shift + volume touch + wheel click | leave, back to Move (also in the setup menu) |

## Licence

MPL-2.0, as the core. See `THIRD_PARTY_LICENSES.md`, in particular the note on SAM.
