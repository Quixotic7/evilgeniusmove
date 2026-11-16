# SPDX-License-Identifier: MPL-2.0
# EvilGenius for Ableton Move: a standalone Schwung tool.
#
#   make sim                 build/sim/egm-sim, the headless simulator for this machine
#   make test                the simulator run through tests/smoke.txt (pictures into build/shots)
#   make move                build/move/evilgenius-host and evilgenius-heal for the Move (aarch64, in Docker)
#   make package             dist/evilgenius-module.tar.gz, what Schwung Manager installs
#   make package NO_SAM=1    .. without SAM, the speech synthesizer that has no licence (src/say_stub.c says
#                            what goes with it); make clean first when changing it
#
# The EvilGenius core is not copied here: EG points at a checkout of it (https://github.com/Quixotic7/evilgenius).
EG ?= ../evilgenius
CORE := $(EG)/core
PVG := $(EG)/third_party/plutovg
TARGET ?= sim
B := build/$(TARGET)
CC ?= cc
OPT ?= -O2 -g

# the core's sources, as evilgenius/tools/Makefile lists them (the other .c files there are included by these)
SAM_SRC := $(CORE)/voice/sam/sam.c $(CORE)/voice/sam/render.c $(CORE)/voice/sam/reciter.c
CORE_SRC := $(addprefix $(CORE)/app/,app.c ctl.c ctl_step.c ctl_rec.c ctl_leds.c ctl_latch.c ctl_preview.c view.c) \
            $(addprefix $(CORE)/ui/,px.c ui_kit.c ui_idle.c ui_keys.c ui_picker.c ui_tracks.c ui_lane.c ui_held.c ui_turn.c ui_layer.c ui_octave.c) \
            $(CORE)/gfx/qd_gfx_plutovg.c \
            $(addprefix $(CORE)/seq/,model.c pattern.c seq.c player.c demo.c chord.c scale.c sounds.c clip.c) \
            $(CORE)/voice/quad/quad_voice.c $(CORE)/voice/sid/sid.c $(CORE)/voice/pool.c $(CORE)/voice/mix.c \
            $(CORE)/voice/fx/qd_fx.c $(if $(NO_SAM),,$(CORE)/voice/sam/say.c)
PVG_SRC := $(wildcard $(PVG)/source/*.c)
INC := -Isrc $(addprefix -I$(CORE)/,app ui gfx seq voice voice/quad voice/sid voice/fx voice/sam store tests) -I$(PVG)/include
DEFS := -DSID_COMB_CONST -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD -DQD_USB_HOST=1

EGM_SRC := src/egm.c src/flash.c src/mono.c src/screens.c $(if $(NO_SAM),src/say_stub.c,)
PLAT_SRC := $(if $(filter move,$(TARGET)),src/plat_move.c,src/plat_sim.c)
BIN := $(if $(filter move,$(TARGET)),evilgenius-host,egm-sim)
LIBS := -lm -lpthread $(if $(filter move,$(TARGET)),-lrt -static,)

CORE_OBJ := $(addprefix $(B)/core-,$(notdir $(CORE_SRC:.c=.o)))
SAM_OBJ := $(if $(NO_SAM),,$(addprefix $(B)/sam-,$(notdir $(SAM_SRC:.c=.o))))
PVG_OBJ := $(addprefix $(B)/pvg-,$(notdir $(PVG_SRC:.c=.o)))
EGM_OBJ := $(addprefix $(B)/,$(notdir $(EGM_SRC:.c=.o) $(PLAT_SRC:.c=.o)))

vpath %.c $(sort $(dir $(CORE_SRC) $(SAM_SRC) $(PVG_SRC))) src

.PHONY: sim test move package clean
sim:
	$(MAKE) TARGET=sim build/sim/egm-sim

test: sim
	mkdir -p build/shots
	build/sim/egm-sim tests/smoke.txt build/shots

# the Move: the cross compiler from Schwung's own build image (debian bookworm, gcc-aarch64-linux-gnu), linked static
# so the unit's glibc is never the question
DOCKER_IMAGE ?= evilgenius-move-build
move:
	docker build -q -t $(DOCKER_IMAGE) docker
	docker run --rm -v "$(abspath .)":/w -v "$(abspath $(EG))":/eg -w /w $(DOCKER_IMAGE) \
	    make TARGET=move EG=/eg CC=aarch64-linux-gnu-gcc NO_SAM=$(NO_SAM) build/move/evilgenius-host build/move/evilgenius-heal

package: move
	scripts/package.sh

$(B)/$(BIN): $(EGM_OBJ) $(CORE_OBJ) $(SAM_OBJ) $(PVG_OBJ)
	$(CC) -o $@ $^ $(LIBS)

build/move/evilgenius-heal: src/heal.c | $(B)
	$(CC) -O2 -Wall -Wextra -std=gnu11 -static $< -o $@

$(B):
	mkdir -p $@
$(B)/core-%.o: %.c | $(B)
	$(CC) $(OPT) -std=c99 -Wall -Wextra $(INC) $(DEFS) -c $< -o $@
$(B)/sam-%.o: %.c | $(B)
	$(CC) $(OPT) -std=gnu99 -w $(INC) $(DEFS) -c $< -o $@
$(B)/pvg-%.o: %.c | $(B)
	$(CC) $(OPT) -std=gnu11 -w $(INC) $(DEFS) -c $< -o $@
# (the same DEFS as the core: its structs' layouts depend on them)
$(B)/%.o: src/%.c $(wildcard src/*.h) Makefile | $(B)
	$(CC) $(OPT) -std=gnu11 -Wall -Wextra $(INC) $(DEFS) -c $< -o $@

clean:
	rm -rf build dist
