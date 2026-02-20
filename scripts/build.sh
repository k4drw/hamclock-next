#!/bin/bash
cmake -B build
JOBS=$(( $(nproc) / 2 ))
[ $JOBS -lt 1 ] && JOBS=1
cmake --build build -j$JOBS --clean-first
