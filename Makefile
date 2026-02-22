CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -pedantic
LDFLAGS ?=

BIN_DIR := bin
TARGET := $(BIN_DIR)/templater
SRC := \
	src/main.c \
	src/templater/util.c \
	src/templater/dom.c \
	src/templater/parser.c \
	src/templater/engine.c \
	src/templater/index.c

.PHONY: all build run demos dev test clean

all: build

build: $(TARGET)

$(TARGET): $(SRC) src/templater/common.h
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run: build
	./$(TARGET) demos/site/src generated/site

demos: build
	./scripts/build-all-demos.sh

dev: build
	./scripts/dev.sh

test: build
	./scripts/test.sh

clean:
	rm -rf $(BIN_DIR) generated .tmp-test-out
