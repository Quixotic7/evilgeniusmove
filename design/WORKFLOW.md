# EvilGenius on Move: clip workflow (proposal, 16 November 2026)

The board is `evil-genius.movedesign.json` (open it in Ableton Move Designer). The user's original file is kept beside
it as `evil-genius.original.movedesign.json`. Everything the user typed into "Main Layout" is unchanged. Only empty
fields were filled in, plus LEDs and a screen.

Two changes drive the design:

1. Each track has its own clips, launched and queued on their own, as in Move's Session mode.
2. A clip has up to 16 pages of 16 steps, so 256 steps.

## Clip model

- **8 tracks** (the user's choice). The track buttons choose Sequence's or Session's four modes (see below).
- **16 clip slots on each of the 8 tracks** (the user's choice). The pads show 4 at a time; Plus / Minus and
  Left / Right move through them.
- **A scene is slot n on all eight tracks.** Scenes are implicit, as in Live and Move; Shift + a pad launches its row.
  SCENES adds per-scene tempo and follow actions.
- **Launching.** Tap a clip pad. If the transport is stopped, the clip is selected and plays on Play (Move's habit). If
  it is running, the clip is queued to the launch quantize, which defaults to 1 bar (`LAUNCH Q`). Choices: NONE,
  1/16, 1/4, 1 bar, 2, 4 or 8 bars, or CLIP END, which is today's master-length change. A queued clip pulses green, and
  the clip it replaces pulses in the track colour.
- **Stopping a track.** Its stop step (the first of the pair under its column), a double-tap on any of its pads, or a
  tap on an empty slot in its column. Shift + Play stops every clip.
- **Scene launch with an empty slot** stops that track. *Choice: the Live/Move behaviour. A setting (`EMPTY SLOT`) can
  keep it playing instead.*
- **Clip editing.**
  - Copy + pad, then pad: copy a clip. Copying onto a clip replaces it.
  - Copy tapped alone: duplicate the selected clip into the next free slot.
  - Delete + pad, then pad: cut a clip and paste it (the same slot puts it back), as Delete does everywhere.
  - Copy + step, then step: copy a scene. Delete + step, then step: cut a scene and paste it.
  - Shift + step 14 (New Clip): select the next empty slot.
  - Shift + step 15 (Double Loop): double the clip.
  - Undo covers all of these.
- **Copy and Delete in Sequence** (the user's Main Layout) work like the FM-1's F1 (copy) and F2 (cut): take
  something, then paste it.

  | Combination | Does |
  |---|---|
  | Copy + step | Copy the step, then tap steps to paste it |
  | Delete + step | Cut the step, then tap a step to paste it (or the same step to put it back) |
  | Copy + track pad | Copy the track, then tap a track pad to paste it |
  | Delete + track pad | Cut (clear) the track, then tap a track pad to paste it, or the same one to put it back |
  | Delete + knob | Remove every lock for that parameter on the track |
  | Step held, knob touched, Delete | Remove that step's lock for that knob |
  | Shift + Delete | Clear the whole clip, every page; only page 1 stays active, reset to steps 1-16 |

  The core already has a step clipboard, a page clipboard and cut (`seq/clip.h`); a track clipboard is new.
- **Sound stays with the track**, as on Move. A clip changes it with sound locks and parameter locks; loading a new
  track sound keeps them. Per-step sound locks still build a kit on one track.
- **Tempo and swing move to the project.** A scene could carry a tempo later.

### Two modes, four buttons each

**Menu** switches between **Sequence** (one track at a time) and **Session** (the whole song). In both, the 4 track
buttons change the mode and bring up that mode's top-row shortcuts.

| Button | Sequence | Session |
|---|---|---|
| 1 | TRACKS: top row selects tracks; other rows play the track; steps sequence it | CLIPS: all four rows are clips (8 track columns × 4 slots) |
| 2 | TRIG / SOUND: top row picks the parameter pages | MIDI FX: top row is the selected track's 8 MIDI FX slots |
| 3 | FX on the selected track: top row picks the FX page, pad 8 PERFORM | FX on the whole mix: the same top row |
| 4 | TOOLS: top row picks the tool | MIX: top row picks VOLUME, PAN or a send; the rows below are 8 track columns |

- **Tapping the button of the mode you're in** hides its top row; all four rows become keys (or clips).
- **The hidden state is remembered** across mode changes until a track button is tapped again.
- **Holding a button** shows its top row while held.
- The button of the current mode is lit white, or dim white while its top row is hidden. The others are dim.
- **Mute held**, in Sequence, whatever top row is showing: the top row is the 8 tracks. Tap to mute or unmute;
  double-tap, or Shift + Mute + tap (the Ableton gesture), to solo.
- **Copy / Delete held + a track pad** copies or cuts the whole track; tap a track pad to paste it.
- The room (the home screen) draws no track lift, by the user's choice. The floor and the page rail run the full width.

### Sequence 1: TRACKS

- The top row is the 8 tracks; tap to select one.
- **Hold a track pad and turn:**

  | Turn | Sets |
  |---|---|
  | Volume | Its volume |
  | Jog wheel | Its pan |
  | Knobs 1-8 | Its sends: CHORUS, ECHO, ROOM, CRUSH, ROBOT, WARP, GLITCH, TV |

  Each shows its popup.
- **Holding a step:**
  - Step + jog wheel sets micro timing (popup `MICRO`).
  - Step + Volume sets velocity (popup `VELOCITY`); the master volume is untouched while a step is held.
  - The knobs are the step's values.
  - To lock a sound or FX value, enter that page first, then hold the step and turn the knob.
- **Pad layouts:** Shift + step 8 (Layout) cycles in key, chromatic, drums, and SOUND LOCKS. In SOUND LOCKS every key
  plays a sound from the project's pool, and with a step held a tap locks that sound on the step.
- **The sound lives on the track.** A clip changes it with sound locks and parameter locks, and loading a new track
  sound keeps the locks.

### Sequence 2: TRIG / SOUND

| Pad | Page | Repeat taps cycle |
|---|---|---|
| 1 | TRIG | TRIG, VOICING (voices, portamento, mono / poly), AMP (level, crush) |
| 2 | OSC | OSC 1, OSC 2, OSC 3, MOD (ring, sync) |
| 3 | SPEECH | WORD, SAY, DIGI |
| 4 | FILTER | FILTER (with its envelope), CHIP |
| 5 | ENV (a contrasting colour) | ENV 1, ENV 2, ENV 3 |
| 6 | LFO (ENV's colour) | LFO 1, LFO 2, LFO 3 |
| 7 | RANDOMIZE PAGE | Hold 1 s |
| 8 | RANDOMIZE SOUND | Hold 1 s |

- A pad remembers the page last chosen on it.
- **Randomize:** hold for a second; the brain laughs and a bar fills, then `SOUND` / `SHAKEN`. A tap does nothing.
- **Mod mapping:** on any page, hold ENV or LFO and turn a knob. The envelope or LFO last chosen on that pad
  modulates that parameter, and the turn sets the depth (popup `ENV2>CUTOFF +40`). *New in the core: a modulation
  matrix.*

### FX (Sequence 3 and Session 3)

- **Top row:**

  | Pad | Page |
  |---|---|
  | 1 | SENDS (the brain machine) |
  | 2 | CHORUS |
  | 3 | ECHO |
  | 4 | ROOM |
  | 5 | CRUSH |
  | 6 | ROBOT |
  | 7 | WARP, then GLITCH, then TV on repeat taps |
  | 8 | PERFORM |

- Each page is its own picture after the FM-1 (below), and the wheel rides the lift between them.
- **Scope:** in Sequence the FX act on the selected track (its sends and throws); in Session on the whole mix.
- **PERFORM:** the 24 pads below become performance pads, a row per kind:

  | Row | Pads |
  |---|---|
  | 2 | Effect throws (CHORUS … TV). Hold: on while held; double-tap: latch |
  | 3 | Beat repeat: 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32, 1/32T |
  | 4 | Filter sweeps LP 1, LP 2, HP 1, HP 2 (slide along to sweep); tape STOP, HALF, REV, SCRATCH |

### Sequence 4: TOOLS

- **Top row:** ROTATE, REVERSE, RANDOM, EUCLID, GRIDS, HUMANIZE, QUANTIZE, and LENGTH / STRETCH / CLIP on repeat taps.
- **Knobs:** the tool's amounts and its scope (step, page, track, clip).
- **Wheel click** applies the tool; Undo takes it back.

### Session 1: CLIPS

| Control | What it does |
|---|---|
| Pads | 8 track columns × 4 clip slots (of 16). Tap: launch the clip (queued to the launch quantize, pulsing green). Empty: select the slot and stop the track. Shift + tap: launch the whole row as a scene. Double-tap: queue the track to stop |
| Plus / Minus | Move the rows by 1 |
| Left / Right | Move the rows by 4. Double-click Left: back to the top |
| Step buttons | In pairs under each track column: the first stops the track, the second mutes it (Shift: solo). Steps 1-2 are track 1, 3-4 track 2, and so on |
| Shift + Play | Stop every clip at the next bar, the transport running |
| Copy / Delete + pad, then pad | Copy, or cut and paste, a clip |
| Wheel | The Session page belt |

LEDs, as on Move:

- track colour: a clip
- white: the selected slot
- green: playing
- pale green, pulsing: queued
- track colour, pulsing: about to stop

*Choice (the user's): columns = tracks, so all 8 tracks are on the pads at once and a column is a track everywhere.*

### Session 2: MIDI FX

- The top row is the 8 MIDI FX slots of the track selected in Sequence.
  - Hold a slot and turn knob 1 to choose its engine: ARP, CHANCE, REPEAT, HARM, TRANSPOSE, SCALER, RANDOM, CHORD,
    MONO or SELECTOR.
  - Hold a slot and turn the jog wheel to move it.
  - Double-tap a slot to bypass it.
- The knobs are the slot's values; the wheel turns its pages.
- **The MIDI FX group:** press Back and find the group option at the start, or hold track button 2 and turn the jog
  wheel.
- **The step buttons** are the selected track's steps here. Hold a step and turn the jog wheel to lock that step's
  MIDI FX group.

### Session 4: MIX

- **Top row:** VOLUME, PAN, CHORUS, ECHO, ROOM, CRUSH, ROBOT, and WARP / GLITCH / TV on repeat taps. It picks what the
  8 knobs and the pad columns set.
- **The three rows below** are the 8 track columns, lit in their colours up to the value. Tap a pad to set the value
  to that height; double-tap for the default. Mute held + pad mutes. PAN is one lit pad per column, lower for left.
- **The screen:** eight faders with live meters, or eight pan markers.
- **The step buttons** stay the stop / mute pairs.

### SCENES and ARRANGE (Session pages, in the first version)

- **SCENES:** a subpage per scene. Knobs: tempo (off = the project's), follow action, after (bars), chance, repeats,
  launch quantize, name, colour.
- **ARRANGE:** the song as positions. A knob is a position's scene, Shift + knob its repeats; the wheel pages through
  positions; wheel click plays the song.

### Today's patterns become clips

Today a pattern holds six tracks (`qd_pattern_t`), and there are 64 of them in banks A-D (`QD_NPAT`). To convert:

- Each used pattern becomes one scene, and its six track parts become six of the clips in that slot; tracks 7 and 8
  start empty.
- Bank A becomes scenes 1-16.
- Banks B-D, if they are used, each become a project of their own ("NAME B", ...).

Loop + step for choosing a pattern goes away; Session does that job.

## Pages: up to 16 per clip

- **Length** is 1-16 pages. Loop + wheel sets it (Shift + wheel: one step at a time); Shift + step 15 doubles it; Right
  past the last page shows an empty "+" page, and putting a step on it adds the page (Move's habit).
- **Each page** keeps the core's existing model: its own start and end, and whether it is in the loop.
- **Page mode** follows the user's design for the Loop button:
  - Loop tapped latches page mode; Loop held gives page mode until it is released.
  - The step buttons are pages 1-16. Tap a page to show it. Hold a page (0.4 s) to mute or unmute it.
    Double-tap a page to loop only it (again: all). Hold one page and tap another to loop that range. Changing the
    length keeps muted pages muted.
  - Pads rows 3-4 are the 16 steps of the page chosen, for its start and end: tap to set the start, double-tap to set
    the end, hold one and tap another for both.
  - Pad row 2 previews a page's trigs, two steps a pad: the page held, else the page playing, else the one shown.
  - Copy + page, then page: copy a page, using the core's page clipboard. Delete + page: clear it.
- **Feedback.**
  - Step LEDs in page mode: white for the page shown, green for the page playing, the track colour for pages in the
    loop, dim for pages out of the loop, grey for pages past the end.
  - The room's rail under the floor is the page map (solid: shown, blinking: playing); Left / Right show `PAGE 3/6`.
  - Left and Right light on the side the playhead is on.
  - **Page follow is a track setting, off by default** (Track Settings, Shift + step 3, as on the FM-1). Shift + Loop
    toggles it for the selected track. It is saved with the project; today it is forced off at every start (QA
    follow-up).

## The modes

| Mode | How you get there | Knobs | Pads | Screen |
|---|---|---|---|---|
| Sequence 1 TRACKS (Main Layout) | Default; Menu; track button 1 | The page last entered (TRIG > STEP at first: the track's defaults) | Top row: tracks 1-8. Rows 2-4: notes in key | The room, with the page rail under its floor |
| Step held | Hold a step | That step's 8 values; wheel = micro, Volume = velocity | Add or remove notes | STEP 37 and its notes, large |
| Loop: pages | Loop tapped or held | – | Rows 3-4: the page's start and end | Length and the page map, large |
| Sequence 2 TRIG / SOUND | Track button 2 | The page's values | Top row: the 8 pages | A picture (envelope, waveform) |
| FX | Track button 3 (either mode) | The FX page's values | Top row: the FX pages, pad 8 PERFORM | Each page its own picture, after the FM-1 |
| Sequence 4 TOOLS | Track button 4 | The tool's amounts and scope | Top row: the tools | The tool's picture (EUCLID's ring) |
| Session 1 CLIPS | Menu; track button 1 | The page last entered | Clips: 8 columns × 4 slots | Every clip of the set: a column a track |
| Session 2 MIDI FX | Track button 2 | The slot's values | Top row: the 8 slots | The slot's picture |
| Session 4 MIX | Track button 4 | 8 tracks in the mode the top row picks | Top row: VOLUME, PAN, sends; rows below: 8 track columns | Faders, the pan view, live meters |
| Shift layer | Shift held, or tapped (sticky) | Second values | – | SHIFT, fullscreen |
| Global settings | Shift + step 2 | MIDI and SYSTEM (clock in / out, CC out, brightness, screensaver, auto off), then EXIT | – | The all-eight view; a knob's popup |
| Track settings | Shift + step 3 | Scale mode, MIDI channel, voices, page follow, kind, name, engine, MIDI out | – | The track's name, then the all-eight view |
| Keys & scales | Shift + step 9 | Scale, root, global or local | – | Name and value, large |

The wheel turns the page belt and click enters a page (see "Pages and the wheel" below). Back goes up a level.

Shift + step, as the user's "Shift Shortcuts" layout sets it:

| Step | Shortcut |
|---|---|
| 1 | Projects: a mode where the 4 × 8 pads are 32 projects, each with a name and a colour you can change |
| 2 | Global settings, including exit to Move |
| 3 | Track settings (the FM-1's SEL held) |
| 5 | Tempo |
| 6 | Metronome |
| 7 | Groove / swing |
| 8 | Pad layout: in key, chromatic, drums, sound locks |
| 9 | Keys and scales |
| 10 | Keyboard velocity on / off |
| 11 | MIDI FX |
| 13 | Tools (no printed icon) |
| 14 | New clip |
| 15 | Double loop |
| 16 | Quantize |

Step 8 (Layout, as printed) cycles the pad layout: in key (a row up is a fourth), chromatic, drums (one pad a sound;
this needs core work), sound locks. Steps 4 and 12 are free.

**Fix to note:** the README puts "double the track" on Shift + step 14, but Move prints Double Loop under step 15 and
New Clip under step 14.

Screen rules. The mockups are drawn with the app's own drawing code (`src/screens.c`, ported) and the core's two pixel
fonts (`core/ui/px_art.h`: the big 8×8 font and the small 4×6 one), following the original screen mockup
(the "EvilGenius for Move" design canvas):

- **Pictures, not text.** At home the room: notes as platforms, the agent as the playhead, the page rail under the floor (no track lift).
  Sound pages, FX floors and the mix draw their own pictures (envelope, echo, faders).
- **The knob popup.**
  - What: a knob touched or turned shows its name small at the top, its value as large as fits, and a full-width bar.
  - The bar appears only for a value with a range, not for a choice.
  - Held steps, tempo and clip launches (`1>3`, as today's `A01>A03`) use the same layout.
  - The bar lives only inside the popup: page pictures have no value bars, and the screen has no step bars.
- **The page-name popup.** A page change shows the name centred in the big font at 2×, white on black, 8 characters
  a line (a longer name wraps).
- **The lift ride.** Changing FX floor plays the app's lift ride.
- **The menu.** Setup is the app's menu: three items, the chosen one on an inverted bar.
- **New screens use the same pieces:**
  - Pages: in the room, the dotted rail under the floor becomes one dash a page. In page mode, the popup's bar place
    holds 16 page cells.
  - Session: a column per track, a row per slot (16), bars at the edges for the 4 rows on the pads.
- Each layout's screen has frames showing its popups; hover it on the board to play them.
- **Every piece of text on these screens is an editable text layer.** It uses the designer's "EvilGenius Big (8x8)"
  or "EvilGenius Small (4x6)" font at the app's size (1×, 2× or 3×), over one picture layer. Change it with the text
  tool and it still matches what the app would draw.

## Pages and the wheel (as Move's device view)

Each view has a **page belt**: the wheel moves it one page at a time, and click enters the chosen page. On screen the
pages are boxes with icons riding a conveyor into the SID brain in its jar; the chosen page is the large box, its name
below the belt.

| Control | What it does |
|---|---|
| Wheel turn, on the belt | Choose a page |
| Wheel click, on the belt | Enter the page |
| Knobs, inside a page | The page's 8 values |
| Wheel turn, inside a page | The next or previous subpage, 8 values each. A two-line popup says where you are (`ENV` / `ENV 2`) |
| Wheel click, inside a page | The page's action where it has one: PRESET loads the sound, TOOLS applies the tool, MIDIFX bypasses the slot |
| Back | Up a level: subpage → page belt → home picture (the room, or the clips) |
| Knobs, while browsing the belt | They keep the page last entered, so turning the wheel never changes what the knobs do |
| Top-row pads (TRIG / SOUND, FX, TOOLS, MIDI FX, MIX) | The same pages, one tap away; either way works |
| A step held, turning any page's knob | Locks that value on the step |

### Sequence belt

PRESET · TRIG · OSC · SPEECH · FILT · ENV · LFO · FX · TOOLS. The subpages are as in "Sequence 2: TRIG / SOUND" and
"FX" above. PRESET's subpages are the sound kinds (ALL, BASS, KEYS, PLUCK, PAD, LEAD, DRUMS, FX, MINE): knob 1 browses,
click loads, Back keeps your own.

### Session belt

CLIPS · MIDI FX · FX · MIX (the four mode buttons' pages) · SCENES · ARRANGE · SEQ.

- **SEQ:**
  - TIME: tempo, swing, launch quantize, metronome, count-in.
  - REC: record mode, quantize, take, automation.
  - CLOCK: clock in, clock out, CC out.

### The FX floors, after the FM-1

| Floor | Picture | Knobs |
|---|---|---|
| SENDS | The brain in the jar on its machine, the eight send levels below | CHORUS, ECHO, ROOM, DRY, CRUSH, ROBOT, WARP, GLITCH (TV on Shift + knob 8) |
| CHORUS | Two waves drifting apart over three walking agents | RATE, DEPTH, WIDTH, SEND |
| ECHO | A ball bouncing lower each time | TIME, RATIO, FDBK, SEND, SYNC, MODE, TONE, WOW |
| ROOM | A room in perspective, the agent's sound in arcs | SIZE, DAMP, DECAY, SEND, PRE, MOD, LOW, WIDTH |
| CRUSH | Stepped platforms under a press | BITS, RATE, JITTER, SEND |
| ROBOT | A robot beside its bursts | FREQ, WAVE, WOBBLE, SEND |
| WARP | The agent between two dotted hills | TIME, FDBK, SWEEP, SEND |
| GLITCH | The agent cut into slices | SLICE, CHANCE, PITCH, SEND |
| TV | An old TV saying READY., signal bars | TONE, HUM, DRIVE, SEND |

### The brain in the jar

- The app's intro (boot and screensaver) is kept as it is.
- It is the machine every page belt runs into.
- It is the FX > SENDS page, as on the FM-1.

## What the core needs (`core/`, this repository's copy)

Today's limits:

- `seq/pattern.h`: `QD_NSTEP 128` steps a track (`QD_NPAGE` = 8 pages of `QD_PAGE` 16). There are 6 tracks
  (`QD_TRACKS`), 64 patterns (`QD_NBANK 4` × 16) and 1024 locks a pattern (`QD_NLOCK`).
- A track's pages are `qd_tset_t.loop` (a `uint8_t` mask, one bit per page) and `pstart[8]` / `pend[8]`.
- `qd_pattern_t` holds all six tracks' steps, settings, locks and sounds. On the device profile only `QD_PAT_RAM 2`
  patterns are in RAM; the host profile has all 64.

Changes:

1. **Pages: 8 → 16.**
   - `QD_NSTEP` 256, and widen the loop mask (`loop` to `uint16_t`). The page start and end arrays size themselves
     from `QD_NPAGE`.
   - Anything that stores a step index in 8 bits: `qd_lock_t.step` and `qd_sev_t.step` are `uint8_t`. Lock ids are
     below 128 (`QD_LK_N` is about 84), so a lock can stay 4 bytes as a 9-bit step plus a 7-bit id.
   - The step function JUMP n is `QD_FN_JUMP + n` with n up to `QD_NSTEP`. With 256 steps, `QD_NFN` passes 255 and no
     longer fits `qd_step_t.func` (`uint8_t`), so widen it.
   - `QD_SEQ_NHIST`, the tools' `QD_NSTEP`-sized arrays and `master` (16..1024) scale with it.
   - A track's 256 steps are 4 KB in RAM (16 B a step), or 5 KB as records (20 B on the wire).
2. **Clips: the six-track pattern becomes a per-track clip.**
   - **8 tracks:** `QD_TRACKS` back to 8. The core had 8 until 24 October 2026, cut to 6 for the FM-1's RAM (`qd_config.h`),
     so it is a build setting for the Move; the store's format changes with it (`QD_STORE_VER`).
   - A new `qd_clip_t` holds what is a track's today: `step[256]`, `qd_tset_t`, its locks (a pool of its own, for
     example 512) and optionally a `qd_tsound_t`.
   - A project holds 8 × 16 clips, a slot table, the tempo and the swing.
   - The sequencer needs one playing clip and one queued clip on each track, with a launch quantize, in place of
     `sw_pat`.
   - Chains (`QD_SEQ_NCHAIN`) convert into ARRANGE (decided 17 November 2026).
   - Paging on the device stays possible, with one record for each clip.
   - **Name clash:** `seq/clip.h` today is the clipboard. Rename it (for example `clipboard.h`) before adding clips.
3. **Storage.**
   - 96 clip records replace 64 pattern records (`QD_PRJ_KEYS` is 1024 on the host).
   - Bump the project format and convert old projects on load, as described in "Today's patterns become clips".
   - Undo levels (`QD_CLIP_ZBYTES`, the undo pool) are per track today and stay per clip.

## Build status (16 November 2026)

**Stage 1, built in the Move layer (`src/egm.c`, `src/screens.c`), no core change.** Checked in the simulator with
`tests/modes.txt`; the older scripts (`smoke`, `map`, `pages`) are updated to the new mapping and pass. The Move build
compiles.

- **Sequence's four modes on the track buttons** (TRACKS, TRIG / SOUND, FX, TOOLS):
  - tap a button for its mode and top row;
  - tap the active one to hide its top row (all four rows keys, remembered across modes);
  - hold a button for its top row while held.
- **The top rows:**
  - TRACKS: the tracks (8 since stage 2).
  - TRIG / SOUND: six page pads that cycle and remember, RND PAGE and RND SOUND held a second, with the brain-and-bar
    animation.
  - FX: the floors, pad 7 cycling WARP / GLITCH / TV.
  - TOOLS: the core's TOOLS page.
- **Mute:** held, the tracks on the top row (tap mutes, double tap or Shift solos); tapped alone, the selected track
  muted or heard. Shift + Mute opens the old MIX until Session has its own.
- **Hold shortcuts:**
  - a step held: the wheel sets micro timing, Volume sets velocity;
  - a track pad held: Volume sets its volume, the wheel its pan, knobs 1-8 its sends.
- **The room:** no track lift; the page rail under the floor.

**Stage 2, 8 tracks and 16 pages, in the core and the Move layer.** Checked in the simulator with `tests/eight.txt`
(and the four older scripts, which pass at the new sizes). The Move build compiles.

- **The core (then in the evilgenius repository; now `core/` here):** `QD_TRACKS` and `QD_NSTEP` are build settings with the FM-1's
  values as defaults; the Move sets 8 and 256.
  - The loop mask is `qd_pmask_t`: 8 bits at 8 pages, 16 bits at 16.
  - The FM-1's screens and black keys use `QD_NPAGE_UI`, at most 8.
  - Tracks 7 and 8 are CHOIR and FX.
  - The store's format number depends on the sizes, so a store of other sizes is wiped at mount (MY PRESETS kept).
  - JUMP targets are limited to the first 128 steps (the condition's argument is 7 bits).
  - The FM-1 build is unchanged in behaviour; the core's host tests give the same results as before the change.
- **The Move layer:**
  - the top row in TRACKS and Mute holds 8 tracks;
  - MIX shows 8 faders;
  - **page mode on Loop** (tapped: latched; held: while held; Shift + Loop: the patterns until Session):
    - the step buttons are the 16 pages: tap shows one, hold takes it out of the loop, two taps solo it, two held set a
      range;
    - the wheel sets the clip's length in pages;
    - pad rows 3-4 are the shown page's start and end;
    - Copy / Delete held with a page copies, pastes or clears it;
    - the screen shows the pages strip.

**Stage 3, clips and Session's CLIPS, in the core and the Move layer.** Checked in the simulator with
`tests/session.txt` (and the five older scripts), and in the core's `test_app` (`t_clips`, 9 checks). The Move build
compiles.

- **The model, simpler than "What the core needs" proposed:** a clip is a pattern's part of one track. Slot k of
  track t is pattern k's track t, so 16 slots are A01-A16 and a pattern is a scene. Nothing in the store changes, and
  today's patterns are already clips (no conversion).
- **The core (`seq/seq.c`):** `QD_CLIPS` (on in the host profile, which the Move builds; off on the FM-1, which keeps
  its RAM) lets each track's cursor play its own pattern.
  - `qd_seq_launch(t, pat, launch)` queues a clip: NOW, BEAT, BAR from PLAY, or END, the end of the track's own loop.
  - `qd_seq_stop_track(t, launch)` silences the track from that tick; its cursor runs on.
  - With the transport stopped, both take effect at once, and PLAY starts each track's clip.
  - `qd_seq_select_at` launches every track's part of a pattern (a scene).
  - `seq.cur` is then only the pattern edited.
  - The FM-1's budget: 40 bytes of RAM. Its `budget-ratchet` failure (stack_audio 3432 > 3032) is the same before the
    change.
- **The Move layer:**
  - **Menu** switches Sequence and Session (Shift + Menu is the core's HOME).
  - **Track buttons in Session:** CLIPS, MIDI FX (the core's page), FX, MIX.
  - **CLIPS:**
    - the pads are 8 tracks x 4 slots: tap launches at the next bar, two taps stop the track, Shift + a pad launches
      the row's scene (an empty slot stops its track);
    - an empty slot tapped is chosen and its track stopped; it is edited in Sequence, and a step put there launches
      it;
    - the steps are stop / mute pairs (Shift: solo);
    - Plus / Minus, Left / Right (twice: the top) and the wheel move the rows;
    - Copy or Delete held + a clip copies or cuts it, then pastes it where tapped;
    - Shift + Play stops every clip at the bar.
  - **The LEDs:** a clip in its track's colour, playing green, queued or stopping blinking, the clip edited white.
  - **The screen:** the four rows as boxes (solid with a play mark: playing), the clip edited ringed, the track's
    name and clip number on top.
  - **The edited pattern follows the selected track's clip** (the slot chosen, else the one queued, else the one
    playing).
  - **The sound stays with the track:** the edited clip's sounds are carried into each track's playing and queued
    clip, and into a clip as it comes to be edited.
  - **Shift + Loop's pattern page** now launches a scene; its screen shows the selected track's clip and the one
    queued.
- **Since (17 November 2026):**
  - **Launch quantize:** the project's launch mode, turned with Shift + the wheel in CLIPS: now, a beat, 1, 2, 4 or 8
    bars, or the clip's end (the core has 2, 4 and 8 bars now; new projects launch at 1 bar).
  - **Copy tapped alone** duplicates the selected clip into its track's next empty slot and launches it.
  - **Shift + step 14 (New Clip)** chooses the selected track's next empty slot (the clip playing goes on; a step put
    there launches it). **Shift + step 15** is Double Loop (it was on 14, one off the Move's printed icons).
  - **Undo** takes back a clip's paste, cut, delete and duplicate: each is a level of its own (a level holds one
    pattern, so the clip's pattern is made the edited one for it).
  - **MIX (button 4)** is the Move's own: the top row what is set, the rows below the tracks' columns in their colours,
    knobs 1-8 the tracks, the screen eight bars (PAN: marks either side of the middle).
  - **MIDI FX (button 2)** is the core's page with the design's gestures: the top row its slots, two taps bypass, a
    slot held + the wheel moves it, button 2 held + the wheel the track's group, a step held + the wheel the step's.
    (A double tap on the selected slot also turns its page first: the core's second tap.)
  - Tests: `tests/clips.txt`, `mix.txt`, `midifx.txt`.
- **Then the last three of Session:**
  - **`EMPTY SLOT`:** Shift + the wheel pressed in CLIPS turns it between STOP (a scene stops a track whose slot is
    empty, as Live and the Move do; the default) and KEEP (that track plays on). The project's, saved in its header
    record (older records: STOP).
  - **MIX's meters:** each track's level now, a bar beside its column (the core's mix meter: its voices' level times
    its volume), in both the volume and the pan view.
  - **MIDI FX's slots on the pads** in the selected track's colour: an effect lit, bypassed dim, empty off; the
    selected slot white (bypassed: light grey; empty: grey).
  - Tests: `tests/clips.txt` (EMPTY SLOT both ways), `keep.txt` (EMPTY SLOT kept), `mix.txt` (plays, for the
    meters), `midifx.txt` (the state line's `toprow=`: the top row's colours as sent).
- **Page mode's pad row 2** previews a page's trigs: 8 pads for 16 steps, so a pad is two steps (both on: the track's
  colour; one: dim; the play head: white). The page is the page step held, else the page playing, else the one shown.
  Tapping it does nothing. Test: `tests/preview.txt` (the state line's `row2=`).

**Not built yet:**

1. **PERFORM:** throws, beat repeat, filter sweeps, tape.
2. **Mod mapping:** a modulation matrix for ENV and LFO.
3. **SCENES and ARRANGE.**
4. **The page belt on the wheel**, the sound-lock keys, the Trig page on pad 1.

**17 November 2026: the core is this repository's own.** `core/` and `third_party/plutovg/` were copied here from the
evilgenius repository (0.1.2 with the stage 2 and 3 changes); the Makefile and the Docker build use only this
repository. The FM-1's repository is back on its `main`, untouched. The FM-1 switches went with the copy: clips are
always on, 8 tracks and 256 steps are the core's own defaults, and the store's format number is the sizes'.
(`QD_NPAGE_UI` stays: the core's own pages have 8 page keys.) `make core-test` runs the core's `test_app` at the Move's
sizes (110 checks).

**Checked on a Move (17 November 2026).** `tests/session.txt`, `modes.txt` and `eight.txt` played on the unit
through the test hook (silent) give the same states as the simulator, and its screens match. Not checked: the LED
colours and the sound, which need someone at the unit.

**Each track's clip is saved with the project** (the project's header record, after the rooms): a project opens with
its tracks on the clips they played, stopped ones stopped (`tests/keep.txt`, run twice on one store, in the simulator
and on the unit). The header record had not been read back since the rooms were added to it (it was longer than its
reader allowed), so a project's mix, key, launch mode and track names came back at their defaults; that is fixed here
too.

## Open questions for the user

1. **Track 8's indigo** may look like track 4's blue on the pads; check on a unit.

## Decided by the user (17 November 2026)

- **Banks B-D on conversion:** one project keeps all four banks as 64 scenes, four banks of 16, paged in Session.
- **Chains** convert into ARRANGE: a chain's patterns laid out in order as scenes on its timeline.
- **Pad row 2 in page mode** previews a page's 16 steps' trigs (the page under the finger, else the playing page).
- **Sequence FX on one track** is that track's sends and throws only, as drawn; the rooms stay shared on the mix (no
  per-track effect).

## Decided by the user (16 November 2026)

Some of these were later replaced by round 2 (the track buttons, the belts' pages, the MIX page's step buttons).

- **Projects:** 4 × 8, so all 32 are on the pads at once.
- **Track settings** are the FM-1's SEL-held menu on the 8 knobs: scale mode, MIDI channel, voices, page follow,
  kind, name, engine, MIDI out. Page follow is therefore per track.
- **Shift + step 8** changes the pad layout.
- **Delete + a clip** cuts it and pastes it, as Delete does for steps and tracks.
- **Navigation is a page belt in each view**, as on Move: the wheel chooses, click enters, the wheel turns subpages
  of 8 values, Back goes up. Track View: PRESET, TRIGGER, MIDIFX, TOOLS, SOUND (OSC, SPEECH, FILT, ENV, LFO), FX.
  Session View: SCENES, ARRANGE, MIX, SEQ.
- **Each FX floor has its own picture after the FM-1**, and the brain in the jar is part of the design.
- **8 tracks.** In Note mode the 4 track buttons are top-row layers (TRACKS, then three more), tap to toggle, hold
  for while held. Mute held shows the tracks to mute or solo. In Session the grid is 8 track columns × 4 scenes; the
  track buttons launch the scenes in view, Plus and Minus move through the slots.
- **Hold shortcuts:** step + wheel = micro timing, step + Volume = velocity; track pad + Volume = volume,
  + wheel = pan, + knob = a send.
- **The room drops the track lift** at the left; the track buttons and top pads show the tracks.
- **A MIX page** where the pads are 8 track columns and the step buttons choose VOLUME, PAN or a send for the knobs.

## Decided by the user, round 2 (16 November 2026)

These replace the earlier design where they differ, and are applied to the board and to the sections above.

### Two modes, four buttons each

Sequence (one track at a time) and Session (the whole song). In both, the 4 track buttons do two jobs: they change
the mode, and they bring up that mode's top-row shortcuts.

| Button | Sequence | Session |
|---|---|---|
| 1 | TRACKS: top row selects tracks; other rows play notes for the active track; steps sequence | CLIP LAUNCH |
| 2 | TRIG / SOUND DESIGN: top row picks parameter pages; other rows play notes; steps sequence | MIDI FX |
| 3 | FX, for the selected track only | FX, for the whole mix |
| 4 | TOOLS | MIX |

- **Tapping the button of the mode you're in** hides its top row; all four rows become keys (or clips).
- **The hidden state is remembered** across mode changes: the top row stays off until a track button is tapped again.
- **Holding a button** shows its shortcuts while held.

### Sequence > TRIG / SOUND DESIGN: the top-row pages

| Pad | Page |
|---|---|
| 1 | TRIG / VOICING (with AMP, voices and portamento) |
| 2 | OSC (with MOD) |
| 3 | SPEECH |
| 4 | FILTER |
| 5 | ENV (a contrasting colour) |
| 6 | LFOs (ENV's colour) |
| 7 | RANDOMIZE PAGE (its own colour) |
| 8 | RANDOMIZE SOUND (its own colour) |

- **Repeat taps:** a pad holding several pages cycles through them. The last one chosen is remembered when you go to
  another pad and come back.
- **Randomize:** hold pad 7 or 8 for one second to randomize, with a fun animation while you hold.
- **Mod mapping:** while on another page, hold ENV or LFO and turn a knob. That maps the envelope or LFO to that
  parameter, and the turn sets the modulation depth. The ENV or LFO used is the one last chosen on its pad.

### FX mode and PERFORM

- **Top row:** the FX pages, one pad each, to edit the effect. Pad 8 is PERFORM.
- **In PERFORM** the lower 24 pads are performance pads, a row per kind:

  | Row | Pads |
  |---|---|
  | 2 | Eight effect throws |
  | 3 | Beat repeat, 1/4 to 1/32 (with triplets) |
  | 4 | Filter sweeps LP 1, LP 2, HP 1, HP 2 on the left; tape stop, half speed, reverse and scratch on the right |

- **Scope:** FX in Sequence acts on the selected track; FX in Session acts on all tracks.

### Session

- **Grid:** 8 track columns × 4 rows of clip slots, 16 slots in all.
  - Tap a pad to launch the clip.
  - Shift + pad launches the whole row as a scene.
  - Double-tap a pad to queue its track to stop.
- **Navigation:** Plus / Minus move the rows by 1; Left / Right jump by 4; double-click Left returns to the top.
- **Step buttons in pairs:** each pad column has two step buttons below it. The first stops that track, the second
  mutes it (steps 1-2 for track 1, 3-4 for track 2, and so on).
- **Stop every clip, transport running:** Shift + Play.
- **MIDI FX mode:**
  - The top row picks the MIDI FX slots 1-8 of the track selected in Sequence.
  - Hold a pad and turn knob 1 to choose that slot's effect engine.
  - Hold a pad and turn the jog wheel to move the slot.
  - The jog wheel alone turns the chosen slot's pages.
  - To change the MIDI FX group: press Back and find the group option at the start, or hold the track button and turn
    the jog wheel.
  - The step buttons become the selected track's steps. Hold a step and turn the jog wheel to lock that step's MIDI FX
    group. Everywhere else, step + wheel stays micro timing.
- **MIX mode:** the top row picks what the knobs and columns set.

  | Pad | Sets |
  |---|---|
  | 1 | VOLUME |
  | 2 | PAN |
  | 3 | CHORUS |
  | 4 | ECHO |
  | 5 | ROOM |
  | 6 | CRUSH |
  | 7 | ROBOT |
  | 8 | WARP, then GLITCH, then TV on repeat taps |

- **SCENES and ARRANGE** (per-scene tempo, follow actions, the song order) ship with the first version.

### Other answers

- **Copy / Delete + a track pad** copies or cuts a whole track.
- **Solo:** both Mute held + double-tap and Shift + Mute + tap. Shift + Mute is the Ableton gesture players will
  expect.
- **Clip slots:** 16 per track.
- **Pages:**
  - Tap a page to select it; its start and end steps are then set on the pads.
  - Hold a page to mute or unmute it.
  - Double-tap a page to loop only that page; double-tap again for all.
  - Changing the clip length keeps muted pages muted.
- **Pages from the wheel:** the top-row pads or the wheel both pick pages. Inside a page the wheel turns subpages
  (shown on the belt), click is the page's action, Back goes out.
- **Sound:**
  - It lives on the track.
  - A clip changes it with sound locks and parameter locks.
  - Loading a new track sound keeps the locks.
  - A pad layout on Shift + step 8 (Layout) makes every key play the sound locks instead of notes.
- **Settings:** track settings on Shift + step 3; scale settings on Shift + step 9.
- **Knob popup:** keeps its value bar, inside the popup only.
- **Source of truth:** the board file (`design/evil-genius.movedesign.json`). The Design canvas is rebuilt from it
  for review.

## Not checked

- None of this has been tried on a unit or in code; it is a paper design.
- The board was loaded through the designer's own `normalizeDoc` / `importJson` on the dev server. I also checked every
  control id, scope, LED kind and metadata field against `hardware.ts`, and looked at each of the nine layouts in the
  designer.
- LED colours were chosen on screen, not seen on the Move's pads.
