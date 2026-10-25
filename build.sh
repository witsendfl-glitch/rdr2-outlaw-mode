#!/usr/bin/env bash
# Builds build/OutlawMode.asi. On Ubuntu: sudo apt install cmake g++-mingw-w64-x86-64-posix
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake-mingw.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j
echo "Built: build/OutlawMode.asi"
