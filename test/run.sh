#!/bin/bash
# test/run.sh

MODE=${1:-all}

function run_test_file() {
    echo "Running: $1"
    timeout 2s python3 "$1"
    RET=$?
    if [ $RET -ne 0 ]; then
        if [ $RET -eq 124 ]; then
            echo "TIMED OUT: $1"
        else
            echo "FAILED: $1 (Code: $RET)"
        fi
        exit 1
    fi
}

if [[ "$MODE" == "ffi" || "$MODE" == "all" ]]; then
    echo "--- FFI Tests (fftvm + TVM FFI) ---"
    for f in test/ffi/*.py; do
        run_test_file "$f"
    done
fi

if [[ "$MODE" == "tvm" || "$MODE" == "all" ]]; then
    echo "--- TVM Tests (fftvm + TVM Runtime) ---"
    for f in test/tvm/*.py; do
        run_test_file "$f"
    done
fi

echo "--- ALL TESTS PASSED ---"
