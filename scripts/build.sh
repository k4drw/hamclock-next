#!/bin/bash
rm -rf build
cmake -B build -DENABLE_DEBUG_API=ON
JOBS=$(( $(nproc) / 2 ))
[ $JOBS -lt 1 ] && JOBS=1
cmake --build build -j$JOBS --clean-first
