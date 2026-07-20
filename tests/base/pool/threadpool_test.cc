#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include "base/pool/threadpool.h"

namespace rpc {

TEST(ThreadPoolTest, Initialization) { ThreadPool pool(4, 1024); }

TEST(ThreadPoolTest, ExecuteSingleTask) {
    ThreadPool pool(2, 1024);
    std::atomic<int> counter{0};

    pool.AddTask([&counter]() { counter++; });

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), 1);
}

TEST(ThreadPoolTest, ExecuteMultipleTasks) {
    ThreadPool pool(4, 1024);
    std::atomic<int> counter{0};
    const int taskCount = 100;

    for (int i = 0; i < taskCount; ++i) {
        pool.AddTask([&counter]() { counter++; });
    }

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), taskCount);
}

TEST(ThreadPoolTest, ThreadCount) {
    ThreadPool pool(4, 1024);
    std::atomic<int> threadIds{0};
    std::vector<std::thread::id> ids(4);

    for (int i = 0; i < 4; ++i) {
        pool.AddTask([&, i]() {
            ids[i] = std::this_thread::get_id();
            threadIds++;
        });
    }

    pool.WaitForTasks();
    EXPECT_EQ(threadIds.load(), 4);
}

TEST(ThreadPoolTest, TaskOrder) {
    ThreadPool pool(1, 1024);
    std::vector<int> results;
    std::mutex mtx;

    for (int i = 0; i < 10; ++i) {
        pool.AddTask([&, i]() {
            std::lock_guard<std::mutex> lock(mtx);
            results.push_back(i);
        });
    }

    pool.WaitForTasks();

    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(results[i], i);
    }
}

TEST(ThreadPoolTest, TaskReturnValue) {
    ThreadPool pool(2, 1024);
    std::atomic<int> sum{0};

    pool.AddTask([&sum]() { sum += 10; });
    pool.AddTask([&sum]() { sum += 20; });
    pool.AddTask([&sum]() { sum += 30; });

    pool.WaitForTasks();
    EXPECT_EQ(sum.load(), 60);
}

TEST(ThreadPoolTest, TaskWithLongerDuration) {
    ThreadPool pool(2, 1024);
    std::atomic<int> counter{0};

    pool.AddTask([&counter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        counter++;
    });
    pool.AddTask([&counter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        counter++;
    });

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), 2);
}

TEST(ThreadPoolTest, QueueFullThrows) {
    ThreadPool pool(1, 2);

    pool.AddTask([]() {});
    pool.AddTask([]() {});

    EXPECT_THROW(pool.AddTask([]() {}), std::runtime_error);
}

TEST(ThreadPoolTest, ClosedPoolThrows) {
    auto pool = std::make_unique<ThreadPool>(1, 1024);

    pool->AddTask([]() {});
    pool->WaitForTasks();

    pool.reset();

    ThreadPool newPool(1, 1024);
    EXPECT_NO_THROW(newPool.AddTask([]() {}));
}

TEST(ThreadPoolTest, ParallelExecution) {
    ThreadPool pool(4, 10000);
    std::atomic<int> counter{0};
    const int iterations = 10000;

    for (int i = 0; i < iterations; ++i) {
        pool.AddTask([&counter]() {
            for (int j = 0; j < 100; ++j) {
                counter++;
            }
        });
    }

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), iterations * 100);
}

TEST(ThreadPoolTest, DestructorWaitsForTasks) {
    std::atomic<bool> completed{false};

    {
        ThreadPool pool(1, 1024);
        pool.AddTask([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            completed = true;
        });
    }

    EXPECT_TRUE(completed.load());
}

TEST(ThreadPoolTest, EmptyPool) {
    ThreadPool pool(1, 1024);
    pool.WaitForTasks();
}

TEST(ThreadPoolTest, TryAddTaskSuccess) {
    ThreadPool pool(2, 1024);
    std::atomic<int> counter{0};

    bool ret = pool.TryAddTask([&counter]() { counter++; });

    EXPECT_TRUE(ret);
    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), 1);
}

TEST(ThreadPoolTest, TryAddTaskQueueFull) {
    ThreadPool pool(1, 2);

    bool ret1 = pool.TryAddTask([]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); });
    bool ret2 = pool.TryAddTask([]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); });
    bool ret3 = pool.TryAddTask([]() {});

    EXPECT_TRUE(ret1);
    EXPECT_TRUE(ret2);
    EXPECT_FALSE(ret3);

    pool.WaitForTasks();
}

TEST(ThreadPoolTest, TryAddTaskClosedPool) {
    auto pool = std::make_unique<ThreadPool>(1, 1024);

    pool->AddTask([]() {});
    pool->WaitForTasks();
    pool.reset();

    ThreadPool newPool(1, 1024);
    EXPECT_TRUE(newPool.TryAddTask([]() {}));
    newPool.WaitForTasks();
}

TEST(ThreadPoolTest, TryAddTaskMultipleSuccess) {
    ThreadPool pool(4, 100);
    std::atomic<int> counter{0};
    const int taskCount = 50;

    for (int i = 0; i < taskCount; ++i) {
        bool ret = pool.TryAddTask([&counter]() { counter++; });
        EXPECT_TRUE(ret);
    }

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), taskCount);
}

TEST(ThreadPoolTest, TryAddTaskConcurrent) {
    ThreadPool pool(4, 1000);
    std::atomic<int> counter{0};
    std::atomic<int> rejected{0};
    const int tasksPerThread = 500;
    const int threadCount = 8;

    std::vector<std::thread> threads;
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&pool, &counter, &rejected]() {
            for (int i = 0; i < tasksPerThread; ++i) {
                if (pool.TryAddTask([&counter]() { counter++; })) {
                    // success
                } else {
                    rejected++;
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    pool.WaitForTasks();

    EXPECT_EQ(counter.load() + rejected.load(), threadCount * tasksPerThread);
}

TEST(ThreadPoolTest, TryAddTaskMixedWithAddTask) {
    ThreadPool pool(2, 5);
    std::atomic<int> counter{0};

    bool ret1 = pool.TryAddTask([&counter]() { counter += 1; });
    bool ret2 = pool.TryAddTask([&counter]() { counter += 2; });
    EXPECT_TRUE(ret1);
    EXPECT_TRUE(ret2);

    pool.AddTask([&counter]() { counter += 3; });

    bool ret3 = pool.TryAddTask([&counter]() { counter += 4; });
    bool ret4 = pool.TryAddTask([&counter]() { counter += 5; });
    EXPECT_TRUE(ret3);
    EXPECT_TRUE(ret4);

    bool ret5 = pool.TryAddTask([&counter]() { counter += 6; });
    EXPECT_FALSE(ret5);

    EXPECT_THROW(pool.AddTask([]() {}), std::runtime_error);

    pool.WaitForTasks();
    EXPECT_EQ(counter.load(), 1 + 2 + 3 + 4 + 5);
}

}  // namespace rpc

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
