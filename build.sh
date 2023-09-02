#!/bin/bash

# TODO maybe use pkg-config to get linking flags for my libraries. Fix when needed.
shopt -s nullglob
g++ -fdiagnostics-color -Wall -Wno-switch -O2 -g "$@" *.cpp *.c modules/*.{c,cpp} -lglfw -pthread -lm -lGL -lGLEW -o game
