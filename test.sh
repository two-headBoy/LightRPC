#!/bin/bash

TEST_DIR="build/bin/tests"
FAILED=0
TOTAL=0
BENCH_FAILED=0

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
echo "  Unit Test Summary"
echo "=========================================="
echo "Total: $TOTAL"
echo "Passed: $((TOTAL - FAILED))"
echo "Failed: $FAILED"

# ==========================================
# 压力测试（始终运行，独立于单元测试结果）
# ==========================================
BENCH_DIR="build/bin/tests/benchmark"
BENCH_SERVER="$BENCH_DIR/benchmark_server"
BENCH_CLIENT="$BENCH_DIR/benchmark_client"
BENCH_PORT=8080
BENCH_PID=""

cleanup_bench() {
    if [ -n "$BENCH_PID" ] && kill -0 "$BENCH_PID" 2>/dev/null; then
        kill "$BENCH_PID" 2>/dev/null
        wait "$BENCH_PID" 2>/dev/null
    fi
    rm -rf benchmark_server_log
}
trap cleanup_bench EXIT

echo ""
echo "=========================================="
echo "  LightRPC Benchmark Test"
echo "=========================================="

if [ ! -x "$BENCH_SERVER" ] || [ ! -x "$BENCH_CLIENT" ]; then
    echo "✗ Benchmark binaries not found in $BENCH_DIR"
    echo "  Run cmake build first."
    BENCH_FAILED=1
else
    # 启动 benchmark server（后台）
    "$BENCH_SERVER" "$BENCH_PORT" &
    BENCH_PID=$!

    # 等待 server 就绪（最多 5 秒）
    READY=0
    for i in $(seq 1 50); do
        if ! kill -0 "$BENCH_PID" 2>/dev/null; then
            echo "✗ Benchmark server failed to start"
            BENCH_FAILED=1
            break
        fi
        # 尝试连接端口检测是否就绪
        if (echo > /dev/tcp/127.0.0.1/"$BENCH_PORT") 2>/dev/null; then
            READY=1
            break
        fi
        sleep 0.1
    done

    if [ $READY -eq 1 ]; then
        echo "Benchmark server started (PID: $BENCH_PID, port: $BENCH_PORT)"

        # 运行压测：4 线程 / 4 连接 / 1000 QPS / 10 秒 / 2 秒预热 / 1 秒超时
        "$BENCH_CLIENT" -connections 4 -threads 4 -qps 1000 -duration 10 -warmup 2 -timeout 1000
        if [ $? -ne 0 ]; then
            BENCH_FAILED=1
        fi
    else
        if [ $BENCH_FAILED -eq 0 ]; then
            echo "✗ Benchmark server did not become ready in time"
            BENCH_FAILED=1
        fi
    fi

    cleanup_bench
fi

# ==========================================
# 清理与汇总
# ==========================================
echo ""
echo "=========================================="
echo "  Cleaning test logs..."
echo "=========================================="
rm -rf test_client_log test_server_log benchmark_server_log

echo ""
echo "=========================================="
echo "  Final Summary"
echo "=========================================="
echo "Unit Tests:     $((TOTAL - FAILED))/$TOTAL passed"
if [ $BENCH_FAILED -eq 0 ]; then
    echo "Benchmark Test: PASSED"
else
    echo "Benchmark Test: FAILED"
fi

if [ $FAILED -eq 0 ] && [ $BENCH_FAILED -eq 0 ]; then
    echo ""
    echo "✓ All tests passed!"
    exit 0
else
    echo ""
    echo "✗ Some tests failed! (unit: $FAILED, benchmark: $BENCH_FAILED)"
    exit 1
fi
