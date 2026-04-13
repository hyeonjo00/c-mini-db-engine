CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Iinclude
BUILD_DIR ?= build
RAYLIB_DIR ?= deps/raylib

STUDIO_TARGET := $(BUILD_DIR)/c-mini-db-studio.exe
CLI_TARGET := $(BUILD_DIR)/c-mini-db-engine.exe

ENGINE_SOURCES := $(filter-out src/main.c src/studio_main.c src/studio_app.c src/ui.c,$(wildcard src/*.c))
STUDIO_SOURCES := $(ENGINE_SOURCES) src/studio_app.c src/studio_main.c
CLI_SOURCES := $(ENGINE_SOURCES) src/ui.c src/main.c

STUDIO_CFLAGS := $(CFLAGS) -I$(RAYLIB_DIR)/include
STUDIO_LDFLAGS := -L$(RAYLIB_DIR)/lib -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32 -mwindows

.PHONY: all studio cli run run-cli clean

all: studio

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

studio: $(STUDIO_TARGET)

cli: $(CLI_TARGET)

$(STUDIO_TARGET): $(STUDIO_SOURCES) | $(BUILD_DIR)
	$(CC) $(STUDIO_CFLAGS) $(STUDIO_SOURCES) -o $@ $(STUDIO_LDFLAGS)

$(CLI_TARGET): $(CLI_SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CLI_SOURCES) -o $@

run: $(STUDIO_TARGET)
	./$(STUDIO_TARGET)

run-cli: $(CLI_TARGET)
	./$(CLI_TARGET)

clean:
	rm -rf $(BUILD_DIR)
