#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra -Wpedantic src/main.cpp -o build/blockchain
