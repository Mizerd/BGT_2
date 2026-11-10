#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/../.."
python3 assignment3/scripts/setup.py
cmake -S . -B assignment3/build -DBUILD_ASSIGNMENT3=ON -DCMAKE_BUILD_TYPE=Release
cmake --build assignment3/build -j 2
lab_python=assignment3/.venv/bin/python
"$lab_python" assignment3/scripts/node.py start
trap '"$lab_python" assignment3/scripts/node.py stop' EXIT
"$lab_python" assignment3/scripts/lab.py run
"$lab_python" assignment3/python/rpc_example.py --output assignment3/.runtime/rpc-example.json
"$lab_python" assignment3/python/rpc_transaction.py --output assignment3/.runtime/transaction-analysis.json
"$lab_python" assignment3/python/rpc_block.py --output assignment3/.runtime/block-analysis.json
"$lab_python" assignment3/scripts/run_tests.py --live
"$lab_python" assignment3/scripts/audit.py --live
