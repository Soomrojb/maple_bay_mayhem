# Maple Bay Mayhem - native Linux build (X11 + OpenGL, ALSA loaded at runtime)
CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
LDLIBS  = -lGL -lX11 -lm -ldl -lpthread
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:src/%.c=build/%.o)

maplebay: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

build/%.o: src/%.c src/game.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build maplebay

.PHONY: clean

test: maplebay
	./tests/run_tests.sh
.PHONY: test
