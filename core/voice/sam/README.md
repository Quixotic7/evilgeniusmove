# SAM, the Software Automatic Mouth, inside EvilGenius

EvilGenius speaks live with SAM: the text-to-speech program Don't Ask Software published for the Commodore 64 in 1982
(now SoftVoice, Inc.). The C here is Sebastian Macke's port, [s-macke/SAM](https://github.com/s-macke/SAM), commit
`a7b36efac730957b59471a42a45fd779f94d77dd` (2020-02-09). The port was converted semi-automatically from the 6502
original's disassembly.

**Licence: none.** The port carries no licence, and the original's rights are SoftVoice's. These files are here by the
owner's decision (2026-11-14), apart from EvilGenius's own MPL-2.0 code, and they are not covered by EvilGenius's
licence. Everything else in this directory (`say.c`, `say.h`, `sam_names.h`, this file) is EvilGenius's.

## What is SAM's

| File | |
|---|---|
| `sam.c`, `sam.h`, `SamTabs.h` | the phoneme parser, the stress and length rules |
| `render.c`, `render.h`, `RenderTabs.h` | the renderer: three formants and the sampled consonants, 22050 Hz |
| `reciter.c`, `reciter.h`, `ReciterTabs.h` | the reciter: English text to SAM's phonemes |

Left out of the port: `main.c` (the command line, the WAV writer, SDL) and `debug.c` / `debug.h`.

## EvilGenius's changes to SAM's files

1. **The prefix.** Each `.c` includes `sam_names.h` after the C library's headers and before SAM's own. It renames
   every global the port has to `sam_*`, so names such as `A`, `X`, `Y`, `input`, `buffer` and `speed` cannot meet
   the firmware's.
2. **No debug output.** `debug` is the constant 0. `printf`, `puts` and the `Print*` helpers are compiled out.
3. **The buffer.** `malloc(22050*10)` is replaced by a fixed `sam_store[SAM_BUF_LEN]`, sized by `QD_SAY_TENTHS` in
   `core/qd_config.h`. Every write into it is bounded, and writes past its end are dropped.
4. **The same render every time.** `ResetOutput()` (in `render.c`) clears the one static the renderer kept between
   runs, the output's time-table index. `Init()` calls it.
5. **One set of registers.** `reciter.c` refers to `sam.c`'s `A, X, Y` with `extern`. In the original build the two
   files' tentative definitions merged into one common symbol, and today's linkers reject the duplicate.
6. **Prototypes.** `sam.h`'s empty parameter lists are now `(void)`.

## How EvilGenius uses it

`say.c`:
- runs SAM in the app's free time only, never in the audio: `qd_say_want`, from `app.c`'s 16 ms look and after a
  knob turn;
- trims the silence, box-filters the output to 6 kHz and quantises it to the SID volume register's 4 bits;
- keeps up to `QD_SAY_SLOTS` words.

A voice looks its word up (`qd_say_get`). A word that isn't rendered yet plays as the stored one, or as silence for
a SAY word.

The 54 stored words (`core/voice/sid/eg_vox_data.h`) are rendered by SAM too, offline, by `tools/eg/gen_vox.py`.
