/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <limits>
#include <thread>
#include <utility>

#include "util/task_limiter.h"

#include "util/deque.h"
#include "util/scoped_defer.h"
#include "util/semaphore.h"
#include "util/system.h"
#include "util/tbb.h"
#include "util/thread.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Allow using up to 25% of memory. */
static size_t default_memory_limit()
{
  const size_t ram = system_physical_ram();
  const size_t unknown_limit = size_t(1024) * 1024 * 1024 * 2; /* 2 GB */
  return (ram) ? ram / 4 : unknown_limit;
}

size_t TaskLimiter::memory_limit_ = default_memory_limit();

namespace {

uint default_max_threads()
{
  return std::max(uint(1), std::thread::hardware_concurrency());
}

struct TaskLimiterState {
  /* Thread waiting for memory. Enqueuing the wake-up task resumes it. */
  struct MemoryRequest {
    tbb::task_handle wake_up;
    size_t memory_size;
  };

  size_t memory_used = 0;
  deque<MemoryRequest> requests;
  thread_mutex mutex;

  /* Arena in which work is executed. Protected by a counting semaphore so the
   * number of threads participating never exceeds the available slots, which
   * could deadlock if a thread is holding memory and waiting on others. */
  const uint max_threads = default_max_threads();
  tbb::task_arena arena{int(2 * max_threads), max_threads};
  thread_counting_semaphore arena_counting_semaphore{int(max_threads)};

  bool available(const size_t memory_size, const size_t memory_limit) const
  {
    /* Written to avoid overflow with huge memory sizes. */
    return memory_used == 0 ||
           (memory_size <= memory_limit && memory_used <= memory_limit - memory_size);
  }

  bool acquire(const size_t memory_size, const size_t memory_limit, tbb::task_group &group)
  {
    const thread_scoped_lock lock(mutex);

    /* Acquire memory immediately if available and nothing else is waiting (to
     * keep the scheduling fair). */
    if (requests.empty() && available(memory_size, memory_limit)) {
      memory_used += memory_size;
      return true;
    }

    /* Add request and wait. */
    requests.push_back({group.defer([]() {}), memory_size});
    return false;
  }

  void release(const size_t memory_size, const size_t memory_limit)
  {
    const thread_scoped_lock lock(mutex);
    memory_used -= memory_size;

    /* Enqueue requests that were waiting, so the task group completes
     * and the other thread wakes up. */
    while (!requests.empty() && available(requests.front().memory_size, memory_limit)) {
      MemoryRequest &request = requests.front();
      arena.enqueue(std::move(request.wake_up));
      memory_used += request.memory_size;
      requests.pop_front();
    }
  }
};

}  // namespace

bool TaskLimiter::execute(const size_t memory_size,
                          const std::function<void()> &task,
                          const std::function<bool()> &cancel)
{
  static TaskLimiterState state;

  if (cancel && cancel()) {
    return false;
  }

  state.arena_counting_semaphore.acquire();
  SCOPED_DEFER(state.arena_counting_semaphore.release());

  bool executed = false;
  state.arena.execute([&]() {
    /* Create task group that we will use as a synchronization mechanism
     * to wait while other threads take up memory. This way we can call
     * wait() so this thread participates in TBB work stealing. */
    tbb::task_group group;
    if (!state.acquire(memory_size, memory_limit_, group)) {
      group.wait();
    }
    SCOPED_DEFER(state.release(memory_size, memory_limit_));

    if (!(cancel && cancel())) {
      task();
      executed = true;
    }
  });

  return executed;
}

size_t TaskLimiter::set_memory_limit(const size_t memory_limit)
{
  return std::exchange(memory_limit_, memory_limit);
}

CCL_NAMESPACE_END
