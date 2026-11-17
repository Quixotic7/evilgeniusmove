# SPDX-License-Identifier: MPL-2.0
# The FM-1 converter (README "FM-1 projects"): two tools.
#   fm1dump  reads an FM-1 backup (.egbak) with the FM-1's OWN core, built here from an FM-1 checkout (FM1=, read only:
#            nothing in it is written, nothing of it is kept in this repository) at its sizes and store
#   fm1load  writes what fm1dump read into a Move store image with this repository's core (8 tracks, 256 steps)
#
#   make -C tools/fm1import FM1=/path/to/EvilGenius        (both, into build/fm1/)
#   tools/fm1import/convert.sh BACKUP.egbak MOVE-STORE.bin  (the two run; see it)
ROOT := ../..
FM1 ?= $(abspath $(ROOT)/../../FM1/EvilGenius)
B := $(ROOT)/build/fm1
CC ?= cc
OPT ?= -O1 -g

# ---- fm1dump: the FM-1's core, the Bluetooth package's store (its backup's layout), the host's sizes otherwise
F := $(FM1)/core
FPVG := $(FM1)/third_party/plutovg
F_SAM := $(F)/voice/sam/sam.c $(F)/voice/sam/render.c $(F)/voice/sam/reciter.c
F_SRC := $(F)/app/app.c $(F)/app/ctl.c $(F)/app/ctl_step.c $(F)/app/ctl_rec.c $(F)/app/ctl_leds.c $(F)/app/ctl_latch.c \
         $(F)/app/ctl_preview.c $(F)/app/view.c \
         $(F)/ui/px.c $(F)/ui/ui_kit.c $(F)/ui/ui_idle.c $(F)/ui/ui_keys.c $(F)/ui/ui_picker.c $(F)/ui/ui_tracks.c \
         $(F)/ui/ui_lane.c $(F)/ui/ui_held.c $(F)/ui/ui_turn.c $(F)/ui/ui_layer.c $(F)/ui/ui_octave.c \
         $(F)/gfx/qd_gfx_plutovg.c $(F)/seq/model.c $(F)/seq/pattern.c $(F)/seq/seq.c $(F)/seq/player.c $(F)/seq/demo.c \
         $(F)/seq/chord.c $(F)/seq/scale.c $(F)/seq/sounds.c $(F)/seq/clip.c \
         $(F)/voice/quad/quad_voice.c $(F)/voice/sid/sid.c $(F)/voice/pool.c $(F)/voice/mix.c $(F)/voice/fx/qd_fx.c \
         $(F)/voice/sam/say.c
F_INC := -I$(F)/app -I$(F)/ui -I$(F)/gfx -I$(F)/seq -I$(F)/voice -I$(F)/voice/quad -I$(F)/voice/sid -I$(F)/voice/fx \
         -I$(F)/voice/sam -I$(F)/tests -I$(FPVG)/include
F_DEFS := -DSID_COMB_CONST -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD -DQD_USB_HOST=1 \
          -DQD_STORE_BLE=1

all: $(B)/fm1dump $(B)/fm1load

$(B)/fm1dump: fm1dump.c $(F_SRC) $(F_SAM)
	@mkdir -p $(B)/fd
	@test -d $(F) || { echo "fm1import: no FM-1 core at $(F) (FM1=the EvilGenius checkout)"; exit 1; }
	@for f in $(F_SAM); do $(CC) $(OPT) -std=gnu99 -w $(F_INC) $(F_DEFS) -c $$f -o $(B)/fd/sam-$$(basename $$f .c).o || exit 1; done
	$(CC) $(OPT) -std=gnu11 -w $(F_INC) $(F_DEFS) -o $@ fm1dump.c $(F_SRC) $(wildcard $(FPVG)/source/*.c) \
	    $(patsubst %.c,$(B)/fd/sam-%.o,$(notdir $(F_SAM))) -lm

# ---- fm1load: this repository's core, the Move's sizes and store (the same flags as the app's: ../../Makefile)
M := $(ROOT)/core
MPVG := $(ROOT)/third_party/plutovg
M_SAM := $(M)/voice/sam/sam.c $(M)/voice/sam/render.c $(M)/voice/sam/reciter.c
M_SRC := $(filter-out $(M_SAM),$(shell $(MAKE) -s --no-print-directory -C $(ROOT) print-core-src 2>/dev/null))
M_INC := -I$(ROOT)/src $(addprefix -I$(M)/,app ui gfx seq voice voice/quad voice/sid voice/fx voice/sam store tests) \
         -I$(MPVG)/include
M_DEFS := -DSID_COMB_CONST -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD -DQD_USB_HOST=1 \
          -DQD_TRACKS=8 -DQD_NSTEP=256

$(B)/fm1load: fm1load.c $(ROOT)/src/flash.c
	@mkdir -p $(B)/fl
	@for f in $(M_SAM); do $(CC) $(OPT) -std=gnu99 -w $(M_INC) $(M_DEFS) -c $$f -o $(B)/fl/sam-$$(basename $$f .c).o || exit 1; done
	$(CC) $(OPT) -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(M_INC) $(M_DEFS) -o $@ fm1load.c $(ROOT)/src/flash.c \
	    $(addprefix $(ROOT)/,$(M_SRC)) $(wildcard $(MPVG)/source/*.c) $(patsubst %.c,$(B)/fl/sam-%.o,$(notdir $(M_SAM))) -lm

.PHONY: all
