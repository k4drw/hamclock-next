#!/bin/bash
cmake -B build
cmake --build build -j10 --clean-first
