#!/bin/bash

inotifywait -m -e CLOSE_WRITE ./ | grep --line-buffered -E '\.c' | while read; do echo "================"; ./build.sh; done;
