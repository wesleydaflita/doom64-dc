TARGET = doom64
PSPSDK = $(shell psp-config --pspsdk-path)
PPSSPP_GAME_DIR ?= /home/wa59/.config/ppsspp/PSP/GAME/DOOM64

PSP_SOURCES := $(filter-out src/i_main.c src/r_phase2.c src/sndwav.c src/s_sound.c,$(wildcard src/*.c))
PSP_SOURCES += $(wildcard src/psp/*.c)
OBJS := $(PSP_SOURCES:.c=.o)

CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu17 -D__PSP__ -Isrc -Isrc/psp
ASFLAGS = $(CFLAGS)
LIBS = -lpspgu -lpspdisplay -lpspctrl -lm

BUILD_PRX = 1
PSP_FW_VERSION = 660
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Doom 64 PSP
PSP_EBOOT_ICON = $(firstword $(wildcard ICON0.png ICON0.PNG))
PSP_EBOOT_PIC1 = $(firstword $(wildcard PIC1.png PIC1.PNG))
EXTRA_CLEAN = $(TARGET).elf

.PHONY: all prx eboot deploy

all: deploy

prx: deploy

eboot: deploy

deploy: EBOOT.PBP
	mkdir -p "$(PPSSPP_GAME_DIR)"
	cp EBOOT.PBP $(TARGET).prx "$(PPSSPP_GAME_DIR)/"

include $(PSPSDK)/lib/build.mak
