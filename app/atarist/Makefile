# MDTEST.TOS, the Multidevice Test's program for the Atari.
#
# Built inside atarist-toolkit-docker (m68k-atari-mint-gcc and libcmini), with
# the repository mounted, since the program shares common/ with the firmware:
#
#   STCMD_IMAGE_TAG=1.5.0 ST_WORKING_FOLDER=<repo> stcmd make -C app/atarist
#
# build.sh does this after the firmware. The program is dist/MDTEST.TOS.

VERSION := $(shell cat version.txt)

SRC := src
COMMON := ../../common
BUILD := build
DIST := dist
EXE := MDTEST.TOS

LIBCMINI := /freemint/libcmini
CC := m68k-atari-mint-gcc
# -Wno-unused-value: osbind.h's bindings are expressions with a value.
CFLAGS := -std=gnu99 -Os -Wall -Werror -Wno-unused-value -I$(LIBCMINI)/include -I$(COMMON) \
          -DVERSION=\"$(VERSION)\" $(EXTRA_CFLAGS)
# DEBUG=1: the bench's build, with a key that overrides the line test's gate.
ifeq ($(DEBUG),1)
CFLAGS += -DMDTEST_DEBUG=1
endif
LDFLAGS := -nostdlib -L$(LIBCMINI)/lib -lcmini -lgcc -Wl,--traditional-format -s

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(SOURCES))
HEADERS := $(wildcard $(SRC)/*.h) $(COMMON)/mdtest.h

.PHONY: all clean
.DELETE_ON_ERROR:

all: $(DIST)/$(EXE)

$(BUILD)/%.o: $(SRC)/%.c $(HEADERS) version.txt | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(DIST)/$(EXE): $(OBJECTS) | $(DIST)
	$(CC) $(LIBCMINI)/lib/crt0.o $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD) $(DIST):
	mkdir -p $@

clean:
	rm -rf $(BUILD) $(DIST)
