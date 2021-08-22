#!/bin/bash

shopt -s nullglob
g++ -Wall -Wno-switch -g *.cpp *.c tinyscheme/*.c -lallegro -lallegro_primitives -lm -lGL -o game
