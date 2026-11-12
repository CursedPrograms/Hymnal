#!/usr/bin/env bash
# Builds Hymnal on Linux (cmake, a C++17 compiler, libsdl2-dev). The engine: build acestep.cpp's ace-server and put it
# next to Hymnal as engine/cpu/ace-server (or engine/cuda/ for an NVIDIA card).
set -e
cd "$(dirname "$0")"
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Built: build/Hymnal"
