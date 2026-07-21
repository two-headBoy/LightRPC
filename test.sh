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

QPS_FAILED=0
CONC_FAILED=0

run_bench_case() {
    # $1 = 显示名称  $2.. = benchmark_client 参数
    local name="$1"
    shift
    echo ""
    echo "--- Benchmark: $name ---"
    "$BENCH_CLIENT" "$@"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "✗ $name FAILED (exit=$rc)"
        return 1
    fi
    echo "✓ $name PASSED"
    return 0
}

echo ""
echo "=========================================="
echo "  LightRPC Benchmark Tests"
echo "=========================================="

if [ ! -x "$BENCH_SERVER" ] || [ ! -x "$BENCH_CLIENT" ]; then
    echo "✗ Benchmark binaries not found in $BENCH_DIR"
    echo "  Run cmake build first."
    QPS_FAILED=1
    CONC_FAILED=1
else
    # 启动 benchmark server（后台）
    "$BENCH_SERVER" "$BENCH_PORT" &
    BENCH_PID=$!

    # 等待 server 就绪（最多 5 秒）
    READY=0
    for i in $(seq 1 50); do
        if ! kill -0 "$BENCH_PID" 2>/dev/null; then
            echo "✗ Benchmark server failed to start"
            QPS_FAILED=1
            CONC_FAILED=1
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

        # Case 1: QPS 模式 — 开环压测，固定发送速率
        run_bench_case "qps_mode_1000qps" \
            -mode qps -connections 4 -threads 4 -qps 1000 \
            -duration 10 -warmup 2 -timeout 1000 || QPS_FAILED=1

        # Case 2: Concurrency 模式 — 闭环并发，固定在途请求数
        run_bench_case "concurrency_mode_200" \
            -mode concurrency -connections 4 -threads 4 -concurrency 200 \
            -duration 10 -warmup 2 -timeout 1000 || CONC_FAILED=1
    else
        if [ $QPS_FAILED -eq 0 ] && [ $CONC_FAILED -eq 0 ]; then
            echo "✗ Benchmark server did not become ready in time"
            QPS_FAILED=1
            CONC_FAILED=1
        fi
    fi

    cleanup_bench
fi

BENCH_FAILED=$((QPS_FAILED + CONC_FAILED))

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
echo "Unit Tests:       $((TOTAL - FAILED))/$TOTAL passed"

# Benchmark 明细：QPS 模式 / Concurrency 模式
if [ $QPS_FAILED -eq 0 ]; then
    echo "Benchmark (QPS): PASSED"
else
    echo "Benchmark (QPS): FAILED"
fi
if [ $CONC_FAILED -eq 0 ]; then
    echo "Benchmark (Concurrency): PASSED"
else
    echo "Benchmark (Concurrency): FAILED"
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
