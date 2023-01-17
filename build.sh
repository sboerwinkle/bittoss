#!/bin/bash

shopt -s nullglob
g++ -Wall -Wno-switch -g *.cpp *.c -lallegro -pthread -lm -lGL -lGLEW -o game
