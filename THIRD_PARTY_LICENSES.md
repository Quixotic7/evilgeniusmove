# Third-party code

EvilGenius for Move links the EvilGenius core (https://github.com/Quixotic7/evilgenius), which is MPL-2.0 and brings
its own third-party parts. That repository's `LICENSING.md`, `LICENSES/` and `third_party/README.md` are the full
account; in short:

| Part | Licence | Where |
|---|---|---|
| EvilGenius core | MPL-2.0 | `core/` |
| SID chip model (Joakim Eriksson) | BSD-2-Clause | `core/voice/sid/` |
| plutovg (and its FreeType-derived rasterizer) | MIT, FTL | `third_party/plutovg/` |
| SAM, the Software Automatic Mouth (Sebastian Macke's port) | **no licence**: see the core's `LICENSING.md` | `core/voice/sam/` |

This repository's own code:

| Part | Licence | Notes |
|---|---|---|
| `src/` except `heal.c` | MPL-2.0 | |
| `src/heal.c` | MIT | after movy-host's `host/heal/heal.c`, Copyright (c) 2026 megadake (https://github.com/DimaDake/schwung-movy) |
| The SPI page layout and launch contract in `src/plat_move.c` | MIT | from Schwung's `src/lib/schwung_spi_lib.h`, Copyright (c) 2025-2026 Charles Vestal (https://github.com/charlesvestal/schwung) |
