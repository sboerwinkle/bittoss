#!/bin/bash

shopt -s nullglob
g++ -fdiagnostics-color -Wall -Wno-switch -O2 -g *.cpp *.c modules/*.{c,cpp} -lallegro -pthread -lm -lGL -lGLEW -o game
