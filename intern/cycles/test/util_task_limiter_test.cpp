/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

#include "util/task_limiter.h"
#include "util/tbb.h"

CCL_NAMESPACE_BEGIN

class TaskLimiterTest : public testing::Test {
 protected:
  void SetUp() override
  {
    prev_memory_limit_ = TaskLimiter::set_memory_limit(10);
  }

  void TearDown() override
  {
    TaskLimiter::set_memory_limit(prev_memory_limit_);
  }

  size_t prev_memory_limit_ = 0;
};

TEST_F(TaskLimiterTest, exceed_limit_single)
{
  /* One task can always execute regardless of the limit. */
  bool executed = false;
  EXPECT_TRUE(TaskLimiter::execute(100, [&]() { executed = true; }));
  EXPECT_TRUE(executed);
}

TEST_F(TaskLimiterTest, exception)
{
  /* Task throwing an exception should not hold memory. */
  EXPECT_THROW(TaskLimiter::execute(6, []() { throw std::runtime_error("test"); }),
               std::runtime_error);

  bool executed = false;
  EXPECT_TRUE(TaskLimiter::execute(6, [&]() { executed = true; }));
  EXPECT_TRUE(executed);
}

TEST_F(TaskLimiterTest, within_limit)
{
  std::atomic<size_t> used = 0;
  std::atomic<size_t> max_used = 0;
  std::atomic<int> executed = 0;

  parallel_for(0, 16, [&](const int i) {
    const size_t size = (i % 3 == 0) ? 6 : 2;
    TaskLimiter::execute(size, [&]() {
      const size_t new_used = (used += size);
      size_t old_max = max_used;
      while (old_max < new_used && !max_used.compare_exchange_weak(old_max, new_used)) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      used -= size;
      executed++;
    });
  });

  EXPECT_EQ(executed, 16);
  EXPECT_LE(max_used, 10);
}

TEST_F(TaskLimiterTest, cancel)
{
  bool executed = false;
  EXPECT_FALSE(TaskLimiter::execute(6, [&]() { executed = true; }, [] { return true; }));
  EXPECT_FALSE(executed);

  /* Cancelled task should not hold memory. */
  EXPECT_TRUE(TaskLimiter::execute(10, [&]() { executed = true; }));
  EXPECT_TRUE(executed);
}

TEST_F(TaskLimiterTest, cancel_while_waiting)
{
  std::atomic<bool> started = false;
  std::atomic<bool> cancelled = false;

  std::thread thread([&]() {
    TaskLimiter::execute(6, [&]() {
      started = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      cancelled = true;
    });
  });

  while (!started) {
    std::this_thread::yield();
  }

  bool executed = false;
  EXPECT_FALSE(
      TaskLimiter::execute(6, [&]() { executed = true; }, [&]() { return bool(cancelled); }));
  EXPECT_FALSE(executed);
  thread.join();
}

TEST_F(TaskLimiterTest, wait_for_release)
{
  std::atomic<bool> started = false;
  std::atomic<bool> released = false;

  std::thread thread([&]() {
    TaskLimiter::execute(6, [&]() {
      started = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      released = true;
    });
  });

  while (!started) {
    std::this_thread::yield();
  }

  bool executed_before_release = true;
  TaskLimiter::execute(6, [&]() { executed_before_release = !released; });
  thread.join();

  EXPECT_FALSE(executed_before_release);
}

TEST_F(TaskLimiterTest, help_while_waiting)
{
  /* With a single thread only one task can execute at a time. */
  if (std::thread::hardware_concurrency() < 2) {
    GTEST_SKIP();
  }

  std::atomic<bool> started = false;
  std::atomic<std::thread::id> waiting_thread_id;
  std::atomic<bool> helped = false;

  /* Verify a waiting thread helps with another task. */
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  std::thread thread([&]() {
    TaskLimiter::execute(6, [&]() {
      started = true;
      parallel_for(
          blocked_range<int>(0, 1024, 1),
          [&](const blocked_range<int> & /*range*/) {
            if (std::this_thread::get_id() == waiting_thread_id) {
              helped = true;
              return;
            }
            while (!helped && std::chrono::steady_clock::now() < deadline) {
              std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
          },
          tbb::simple_partitioner());
    });
  });

  while (!started) {
    std::this_thread::yield();
  }

  waiting_thread_id = std::this_thread::get_id();
  TaskLimiter::execute(6, []() {});
  thread.join();

  EXPECT_TRUE(helped);
}

CCL_NAMESPACE_END
