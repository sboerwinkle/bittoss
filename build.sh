#!/bin/bash

shopt -s nullglob

# On my local setup, GLFW3 seems to require -ldl, but doesn't list it in the pkg-config libs?
L_GLFW3="`pkg-config --libs glfw3` -ldl"

# I include -ldl here again because I need it myself too haha
# -rdynamic exports a bunch of symbols (more than we need),
#     which is just so I can load .so files that use symbols from the main executable.
g++ -fdiagnostics-color -Wall -Wno-switch -Wno-format-truncation -Wshadow -O2 -g "$@" \
	-rdynamic \
	*.cpp *.c modules/*.{c,cpp} compounds/*.{c,cpp} \
	$L_GLFW3 -pthread -lm -lGL -ldl -o game
