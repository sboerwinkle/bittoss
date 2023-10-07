#!/bin/bash

shopt -s nullglob

# On my local setup, GLFW3 seems to require -ldl, but doesn't list it in the pkg-config libs?
L_GLFW3="`pkg-config --libs glfw3` -ldl"

g++ -fdiagnostics-color -Wall -Wno-switch -O2 -g "$@" *.cpp *.c modules/*.{c,cpp} $L_GLFW3 -pthread -lm -lGL -lGLEW -o game
