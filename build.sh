#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
flags=(-std=c++20 -O2 -Wall -Wextra -Wpedantic -Isrc)
sources=(src/custom_hash.cpp src/models.cpp)
"${CXX:-c++}" "${flags[@]}" "${sources[@]}" src/main.cpp -o build/blockchain
"${CXX:-c++}" "${flags[@]}" "${sources[@]}" tests/test_blockchain.cpp -o build/tests
