CFLAGS = -Os -g3 -Wall -Werror

SRC_DIR=src
BUILD_DIR=build
INSTALL_DIR=/usr/local/bin

SOURCES_STMDFU = dfucommands.c dfurequests.c dfuse.c crc32.c stmdfu.c
LDFLAGS_STMDFU = -lusb-1.0 -lm

SOURCES_BIN2DFU = dfuse.c crc32.c bin2dfu.c
LDFLAGS_BIN2DFU =

SOURCES_HEX2DFU = dfuse.c crc32.c hex2dfu.c
LDFLAGS_HEX2DFU =

EXE_FILES = stmdfu bin2dfu hex2dfu
# CFLAGS_STMDFU += -D STMDFU_DEBUG_PRINTFS=0

CC = gcc

.PHONY: clean install uninstall

all: $(EXE_FILES)

stmdfu: $(addprefix $(SRC_DIR)/, $(SOURCES_STMDFU))
	@mkdir -p ${BUILD_DIR}
	$(CC) $(CFLAGS) $(LDFLAGS_STMDFU) $^ -o ${BUILD_DIR}/$@

bin2dfu: $(addprefix $(SRC_DIR)/, $(SOURCES_BIN2DFU))
	@mkdir -p ${BUILD_DIR}
	$(CC) $(CFLAGS) $(LDFLAGS_BIN2DFU) $^ -o ${BUILD_DIR}/$@

hex2dfu: $(addprefix $(SRC_DIR)/, $(SOURCES_HEX2DFU))
	@mkdir -p ${BUILD_DIR}
	$(CC) $(CFLAGS) $(LDFLAGS_BIN2DFU) $^ -o ${BUILD_DIR}/$@

install:
	@strip $(addprefix $(BUILD_DIR)/, $(EXE_FILES))
	cp $(addprefix $(BUILD_DIR)/, $(EXE_FILES)) ${INSTALL_DIR}

uninstall:
	rm -rf $(addprefix $(INSTALL_DIR)/, $(EXE_FILES))

clean:
	rm -rf $(BUILD_DIR)
