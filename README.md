# EvilGenius for Move

The [EvilGenius](https://github.com/Quixotic7/evilgenius) SID chip groovebox (an M-VAVE FM-1 firmware) running on
Ableton Move as a standalone [Schwung](https://github.com/charlesvestal/schwung) tool. While it runs, Move's own
software is stopped and EvilGenius owns the pads, the screen and the audio. Leaving it starts Move again.

This repository is the whole of it: the Move side (`src/`) and its own copy of the instrument's core (`core/`,
`third_party/plutovg/`). The core was copied from the FM-1's EvilGenius (0.1.2) and is reshaped for the Move here (8
tracks, 16 pages, clips); the FM-1's repository is neither needed nor touched.

## Status

It runs on a Move: sound, surface, screen and real-time audio, with the Move's own pages for the room, the sound
editor, the FX tower, the mix, the knob pop-ups, the setup menu, tempo and patterns. Checked on the unit over ssh with
the test hook (below); the LED colours and the sound itself have only been judged by ear and eye in passing.

The clip workflow (`design/WORKFLOW.md`) is being built in stages. Stage 1 (Sequence's four modes on the track
buttons, their top rows, Mute held, the hold shortcuts) is in; `tests/modes.txt` exercises it. Stage 2 (8 tracks, 16
pages of 16 steps, page mode on Loop) is in; `tests/eight.txt` exercises it. Stage 3 (clips and Session's CLIPS: each
track's clips launched and stopped on their own, scenes) is in; `tests/session.txt` exercises it, and
`tests/keep.txt` (run twice with `EGM_FLASH` set to one store file) checks that a project keeps each track's clip,
its mix and its EMPTY SLOT setting.

A clip is a pattern's part of one track: slot k of a track is pattern A(k+1)'s part, so a pattern is a scene. The core
plays each track's clip on its own (`core/seq/seq.c`). The
sound stays with the track, as on Move: launching a clip, or editing one, carries the track's sound into it (the demo
patterns' own sounds give way to the track's once they are played or edited).

The Move builds the core with `-DQD_TRACKS=8 -DQD_NSTEP=256`. A projects store of other sizes is not read: **the first
launch of this build on a unit erases the saved projects** (MY PRESETS are kept). The JUMP condition reaches only the
first 128 steps.

Not done yet:

- pictures for four sound pages (AMP, VOICE, MOD, CHIP), which show the core's own drawing, cropped
- booting straight into EvilGenius (it starts from Schwung's Tools menu)
- the power button's menu is written from movy-host's notes and has not been pressed on a unit
- the core's own pages (settings, projects, the browser, tools, MIDI FX) show through a window, with its knobs

## Build

```bash
make test                    # the simulator on this machine, tests/smoke.txt into build/shots
make core-test               # the core's own checks (core/tests/test_app.c) at the Move's sizes, sanitized
make move                    # the Move binaries, in Docker
make package                 # dist/evilgenius-module.tar.gz
make clean && make package NO_SAM=1   # .. without SAM, the speech synthesizer that has no licence
scripts/deploy.sh --launch   # copy to ableton@move.local and start it
```

Nothing outside this repository is needed (the Move build mounts only it in Docker).

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
| Shift + a step | the icons printed under the steps: 1 projects / 2 setup (settings, exit) / 3 TRACK SETTINGS (key mode, MIDI channel, voices, page follow, kind, name, engine, MIDI out channel) / 5 tempo / 6 metronome / 7 GROOVE (knob 1 the swing; the lowest pad row: which tracks swing) / 8 the pad layout (in key, chromatic, drums) / 9 KEYS & SCALES (mode, scale, root) / 10 velocity fixed on / off / 11 MIDI FX / 13 TOOLS / 14 new clip / 15 double loop / 16 QUANTIZE the clip (held: the wheel the amount; let go: done; undo takes it back). Shift held lights those icons, an open page's bright. A track's settings are the track's in every clip |
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
| Mute | tapped: the selected track muted / heard. Held: the top row is the tracks: tap mutes, double tap or Shift + tap solos; anywhere, knob n touched and let go does the same for track n, and knob n turned sets track n's volume (with its pop-up). Shift + Mute tapped: MIX |
| Shift + Sampling / Shift + Capture | save the sound / MIDI FX |
| Top row in TRACKS | the 8 tracks: tap selects; held, Volume / the wheel / the knobs are its volume, pan, sends |
| Top row in TRIG / SOUND | pages: TRIG (AMP, VOICE), OSC (1-3, MOD), SPEECH (WORD, SAY, DIGI), FILTER (and CHIP), ENV (1-3), LFO (1-3): tap again for the next, each remembers its last; pads 7 / 8 held a second: the page / the whole sound randomized (Shift + a page pad: the page at once) |
| Top row in FX | the floors: SENDS, CHORUS, ECHO, ROOM, CRUSH, ROBOT, then WARP / GLITCH / TV on pad 7 (pad 8, PERFORM: not built yet) |
| Top row in TOOLS | the core's TOOLS keys |
| Shift + a top pad, in SOUND | shake the page's values (the dice) |
| Power button | a menu: CANCEL / POWEROFF |
| Menu | Sequence <-> Session, each back in the mode it was left in. Shift + Menu: the core's HOME (again: the next home view) |
| Back | out of whatever page is open |
| Track 1-4, in Session | Session's modes: 1 CLIPS, 2 MIDI FX, 3 FX, 4 MIX. Button 2 held + the wheel: the selected track's MIDI FX group |
| MIX (Session) | top row: what is set (VOLUME, PAN, CHORUS, ECHO, ROOM, CRUSH, ROBOT; pad 8 WARP, GLITCH, TV in turn); button 4 again hides it (four rows of columns). The rows below: a column a track, lit in its colour up to the value, the bottom row 0 (PAN: left), the top 127 (right). A pad held glides the value to its height, as fast as it is pressed (a light touch slowly, about 20 s across; a hard press about 0.2 s; aftertouch changes it while held); Shift + a pad: straight there; two taps: the default. No pop-up for the pads (the bars show it); the knobs keep theirs. The wheel does nothing here; Back: CLIPS. Mute held + a column: muted. Knobs 1-8: tracks 1-8. Steps: stop / mute pairs. The screen: each track's bar (PAN: its mark), its level now beside it |
| MIDI FX (Session) | the selected track's MIDI FX group (a track with none gets a free one when MIDI FX opens; knob 6: another; the group is the track's in every clip), drawn for this screen: the eight slots (each its effect's short name; empty dotted, bypassed crossed, the selected inverted), the selected effect and its page, knobs 1-4's values. Knob 1 on an empty slot: its effect (turning on: the next ones). Top row: the slots (tap selects; held + knob 1 the effect, held + the wheel moves it; two taps: bypassed; lit in the track's colour, bypassed dim, the selected white). The wheel: the slot's pages; a step held + the wheel: that step's group |
| Pads, in CLIPS | 8 tracks (columns) x 4 clip slots (rows, of 16). Tap: the clip launched at the next bar (stopped: at once); an empty slot: chosen (the clip edited in Sequence; a step put there launches it) and the track stopped. Two taps: the track stopped. Shift + a pad: that row's scene (see SCENES; an empty slot stops its track). Green: playing; blinking: queued or stopping; the track's colour: a clip; white: the clip edited |
| Steps, in CLIPS | two under each track: the first stops it (at the bar), the second mutes it (Shift: solo) |
| Copy / Delete held + a clip, in CLIPS | copied, then pasted where tapped / cut, then pasted where tapped (released first: deleted). Copy tapped alone: the selected clip duplicated into its track's next empty slot and launched. Undo takes each back |
| Shift + the wheel, in CLIPS | the launch quantize: now (a 16th), a beat, 1 / 2 / 4 / 8 bars, the clip's end (the project's; new projects: 1 bar) |
| Shift + the wheel pressed, in CLIPS | EMPTY SLOT: a scene over a track's empty slot stops the track (STOP, the default) or leaves it playing (KEEP); the project's |
| - / +, Left / Right, in CLIPS | the slots shown a row / 4 rows; Left twice: the first four; Shift + Left / Right: a bank of 16. There are 64 slots, a scene each (four banks of 16: an FM-1 project's A-D); the steps in SCENES, ARRANGE, SEQ and with Copy / Delete are the bank shown |
| The wheel, in CLIPS | Session's page belt: CLIPS, MIDI FX, FX, MIX, SCENES, ARRANGE, SEQ, riding into the brain machine. Turn to choose, press to enter; Back closes it |
| FX, pad 8 (either view) | PERFORM: the rows below are performance pads. Row 3: throws CHORUS, ECHO, ROOM, CRUSH, ROBOT, WARP, GLITCH, TV, fully on (Sequence: the selected track's sends; Session: every track's). Row 2: beat repeat 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T. Row 1: filter LP 1, LP 2, HP 1, HP 2 (slide along to sweep), tape STOP, HALF, REV, SCRATCH. Repeat, filter and tape act on the whole mix. Held: on while held; two taps: latched (a tap: off); of a kind the newest wins. Pad 8 again: everything let go |
| ENV / LFO held + a knob, in TRIG / SOUND | the mod matrix: the envelope or LFO last chosen on that pad modulates the knob's value, the turn its depth (pop-up ENV1>CUTOFF +31; NO MOD: a value nothing can modulate). Each is one of the sound's eight modulation slots (sources now also ENV 1-3, LFO 1-3); a tap on ENV / LFO still turns their pages |
| Shift + step 1 | PROJECTS: the 32 projects on the pads (1-8 the top row; the open one white, the others in their colours). Tap: open it; an empty pad: a new project there. Copy held + a project, then an empty pad: copied. Delete held + a project, twice: deleted. Knob 1: the open project's name (ideas), knob 2: its colour. Back: closed |
| A finger on the wheel | the knobs turned keep the all-eight view (no single pop-up) |
| The wheel at home, in Sequence | Sequence's page belt: PRESET, TRIG, OSC, SPEECH, FILT, ENV, LFO, FX, TOOLS; press to enter. In a sound page the wheel turns its own subpages (TRIG / VOICING / AMP, ENV 1-3 ..); Back goes up to the belt, again home. Left / Right turn the step pages |
| Pad 1, in TRIG / SOUND | TRIG first (the step's eight values: velocity, length, micro, ratchet, chance, condition, trig, slide; a step held: its own, else the track's defaults), then VOICING, AMP |
| SOUND LOCKS (Shift + step 8's fourth layout) | pad n of the note rows plays the project's pool sound n at the track's root (its name shown); a step held + a pad: that sound locked on the step (an empty step turned on) |
| Copy / Delete + a step, in CLIPS | the steps are the scenes: Copy + a step, then a step: the scene copied there; Delete + a step: cut, the next step pastes it (let go first: deleted). Undo takes each back |
| Shift + Left / Right | the first page, the last |
| Shift + Rec | record afresh: the selected track's clip cleared (undo brings it back), then armed |
| Shift + Copy, in CLIPS | the scene playing (else the selected clip's row) into the next empty scene, launched |
| Volume (the main knob) | the output level; its pop-up while it turns (VOLUME and a percent) |
| The idle screen (the brain) | any pad, step or button press wakes it and does nothing else |
| SEQ (Session, from the belt) | the wheel: TIME (TEMPO, SWING: every scene's, LAUNCH quantize, METRO, CLICK level, COUNT-IN: 1-4 bars of clicks before PLAY when recording is armed; PLAY again calls it off), REC (MODE live / step, QUANT, TAKE overdub / replace, AUTOmation), CLOCK (CLK OUT, CC OUT, CLK IN: an incoming MIDI clock followed or ignored). Steps 1-16: launch scenes 1-16. Back: the belt |
| SCENES (Session, from the belt) | a scene's settings, the wheel the scene (1-16). Knobs: TEMPO (OFF: the project's), FOLLOW (NONE, NEXT, PREVIOUS, FIRST, ANY, OTHER), AFTER (its length in bars; AUTO: its longest clip), CHANCE, REPEATS, LAUNCH (PROJECT: the project's quantize), NAME (INTRO, VERSE, CHORUS ..), COLOUR (a track's). The wheel pressed: the scene launched. Pads, the lower two rows: the scenes (tap: edit it; Shift + tap: launch it). Steps 1-16: launch scenes 1-16. Back: the belt |
| ARRANGE (Session, from the belt) | the song: knob i the scene at position i of the 8 shown (down to END: the song ends there), Shift + knob its repeats; the wheel the next 8 positions (32 in all). The wheel pressed: the song from the start; again: stopped (the clips play on). Pads: the 32 positions (1-8 the top row; tap: the song from there). Steps 1-16: launch scenes 1-16. Back: the belt |
| Shift + Play, in Session | every clip stopped at the next bar, the transport running |
| Record | arm recording from the pads (stopped: starts with the first note); again: off |
| Loop | page mode: tapped, on (again: off); held, while held. Shift + Loop held: the step buttons launch a pattern's clips on every track (a scene), top pads 1-4 the bank |
| A page step, in page mode | tap: the page shown; held: out of the loop / back in; two taps: that page alone (again: the loop back); one held and another tapped: the loop from one to the other. White shown, green playing, blue in the loop |
| Pad rows 3-4, in page mode | the shown page's 16 steps: tap its start, two taps its end, one held and another tapped both |
| Pad row 2, in page mode | a page's trigs, two steps a pad (both: the track's colour, one: dim; the play head white): the page step held, else the page playing, else the one shown |
| Copy / Delete held + a page step | the page copied, then pasted where tapped / the page cleared |
| Copy (Shift: paste) | the pattern (page mode: see above) |
| Delete | the core's B11: clear |
| - / + | the core's F1 / F2 layers |
| Left / Right | the previous / next page of 16 steps (16 pages). The page shown stays put while it plays; the side the playhead is on lights |
| Undo (Shift: redo) | undo |
| Shift + volume touch + wheel click | leave, back to Move (also in the setup menu) |

## Licence

MPL-2.0, as the core. See `THIRD_PARTY_LICENSES.md`, in particular the note on SAM.

## FM-1 projects

`tools/fm1import/convert.sh` brings the projects of an FM-1 EVILGENIUS backup (the web manager's `.egbak`, or the store
`.bin` that `fizz_xdata.py backup` writes: the same 200704 bytes) to the Move as new projects. Each FM-1 project
takes the first free project slot; its 64 patterns are the Move's 64 scenes (bank A scenes 1-16, B 17-32 ..), a
pattern's six tracks that scene's clips on tracks 1-6 (7 and 8 start empty), with their pages, steps, locks, sounds,
the pool, the MIDI FX groups and the tracks' names, kinds and mix.

```bash
tools/fm1import/convert.sh "EVILGENIUS backup.egbak" --unit
```

`--unit` stops EvilGenius on the Move, keeps its store (on the unit as `evilgenius-store.bin.before-fm1-DATE`, and in
`build/unit-backup/`), adds the projects and starts it again. A file name instead of `--unit` converts into a store
image here. `--first N` starts at project slot N.

The backup is read with the FM-1's own code, built from an FM-1 EvilGenius checkout (`FM1=`, by default
`../../FM1/EvilGenius` beside this repository; it is only read). A modulation to the envelope destinations FM-1 0.1.4
added does nothing on the Move's engine; the converter counts them.
