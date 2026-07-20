#!/bin/bash

TEST_DIR="build/bin/tests"
FAILED=0
TOTAL=0

echo "=========================================="
echo "  LightRPC Unit Tests"
echo "=========================================="

for test in "$TEST_DIR"/*_test; do
    if [ -x "$test" ]; then
        TOTAL=$((TOTAL + 1))
        echo ""
        echo "--- Running $(basename "$test") ---"
        "$test"
        if [ $? -ne 0 ]; then
            echo "FAILED"
            FAILED=$((FAILED + 1))
        else
            echo "PASSED"
        fi
    fi
done

echo ""
echo "=========================================="
echo "  Test Summary"
echo "=========================================="
echo "Total: $TOTAL"
echo "Passed: $((TOTAL - FAILED))"
echo "Failed: $FAILED"

echo ""
echo "Cleaning test logs..."
rm -rf test_client_log test_server_log

if [ $FAILED -eq 0 ]; then
    echo ""
    echo "✓ All tests passed!"
    exit 0
else
    echo ""
    echo "✗ $FAILED tests failed!"
    exit 1
fi