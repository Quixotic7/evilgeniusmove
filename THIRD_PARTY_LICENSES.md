# Third-party code

EvilGenius for Move carries its own copy of the EvilGenius core (`core/`, copied from
https://github.com/Quixotic7/evilgenius, MPL-2.0), and the third-party parts that come with it. The licence texts are
here: `LICENSE` (MPL-2.0), `LICENSES/BSD-2-Clause.txt` (the SID model), `third_party/plutovg/LICENSE` (MIT) and
`third_party/plutovg/source/FTL.TXT`. In short:

| Part | Licence | Where |
|---|---|---|
| EvilGenius core | MPL-2.0 | `core/` |
| SID chip model (Joakim Eriksson) | BSD-2-Clause | `core/voice/sid/` |
| plutovg (and its FreeType-derived rasterizer) | MIT, FTL | `third_party/plutovg/` |
| SAM, the Software Automatic Mouth (Sebastian Macke's port, github.com/s-macke/SAM) | **no licence** (it has none of its own) | `core/voice/sam/` |

This repository's own code:

| Part | Licence | Notes |
|---|---|---|
| `src/` except `heal.c` | MPL-2.0 | |
| `src/heal.c` | MIT | after movy-host's `host/heal/heal.c`, Copyright (c) 2026 megadake (https://github.com/DimaDake/schwung-movy) |
| The SPI page layout and launch contract in `src/plat_move.c` | MIT | from Schwung's `src/lib/schwung_spi_lib.h`, Copyright (c) 2025-2026 Charles Vestal (https://github.com/charlesvestal/schwung) |

`make package NO_SAM=1` builds without SAM (`src/say_stub.c` stands in for the core's speech). The stored VOX words
in the core were rendered with SAM and stay in either build.
