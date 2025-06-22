#!/bin/bash

shopt -s nullglob

g++ -fdiagnostics-color -Wall -Wno-switch -Wno-format-truncation -O2 -g "$@" \
	-fPIC \
	*.cpp *.c \
	-shared -o srm.so
