// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2018 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Author: vitus@google.com (Michael Vitus)

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifdef CERES_USE_CXX_THREADS

#include "ceres/parallel_for.h"

#include <cmath>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "ceres/concurrent_queue.h"
#include "ceres/scoped_thread_token.h"
#include "ceres/thread_token_provider.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {
namespace {
// This class creates a thread safe barrier which will block until a
// pre-specified number of threads call Finished.  This allows us to block the
// main thread until all the parallel threads are finished processing all the
// work.
class BlockUntilFinished {
 public:
  explicit BlockUntilFinished(int num_total)
      : num_finished_(0), num_total_(num_total) {}

  // Increment the number of jobs that have finished and signal the blocking
  // thread if all jobs have finished.
  void Finished() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++num_finished_;
    CHECK_LE(num_finished_, num_total_);
    if (num_finished_ == num_total_) {
      condition_.notify_one();
    }
  }

  // Block until all threads have signaled they are finished.
  void Block() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [&]() { return num_finished_ == num_total_; });
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  // The current number of jobs finished.
  int num_finished_;
  // The total number of jobs.
  int num_total_;
};

// Shared state between the parallel tasks. Each thread will use this
// information to get the next block of work to be performed.
struct SharedState {
  SharedState(int start, int end, int num_work_items)
      : start(start),
        end(end),
        num_work_items(num_work_items),
        i(0),
        thread_token_provider(num_work_items),
        block_until_finished(num_work_items) {}

  // The start and end index of the for loop.
  const int start;
  const int end;
  // The number of blocks that need to be processed.
  const int num_work_items;

  // The next block of work to be assigned to a worker.  The parallel for loop
  // range is split into num_work_items blocks of work, i.e. a single block of
  // work is:
  //  for (int j = start + i; j < end; j += num_work_items) { ... }.
  int i;
  std::mutex mutex_i;

  // Provides a unique thread ID among all active threads working on the same
  // group of tasks.  Thread-safe.
  ThreadTokenProvider thread_token_provider;

  // Used to signal when all the work has been completed.  Thread safe.
  BlockUntilFinished block_until_finished;
};

}  // namespace

int MaxNumThreadsAvailable() {
  return ThreadPool::MaxNumThreadsAvailable();
}

// See ParallelFor (below) for more details.
void ParallelFor(ContextImpl* context,
                 int start,
                 int end,
                 int num_threads,
                 const std::function<void(int)>& function) {
  CHECK_GT(num_threads, 0);
  CHECK(context != NULL);
  if (end <= start) {
    return;
  }

  // Fast path for when it is single threaded.
  if (num_threads == 1) {
    for (int i = start; i < end; ++i) {
      function(i);
    }
    return;
  }

  ParallelFor(context, start, end, num_threads,
              [&function](int /*thread_id*/, int i) { function(i); });
}

// This implementation uses a fixed size max worker pool with a shared task
// queue. The problem of executing the function for the interval of [start, end)
// is broken up into at most num_threads blocks and added to the thread pool. To
// avoid deadlocks, the calling thread is allowed to steal work from the worker
// pool. This is implemented via a shared state between the tasks. In order for
// the calling thread or thread pool to get a block of work, it will query the
// shared state for the next block of work to be done. If there is nothing left,
// it will return. We will exit the ParallelFor call when all of the work has
// been done, not when all of the tasks have been popped off the task queue.
//
// A unique thread ID among all active tasks will be acquired once for each
// block of work.  This avoids the significant performance penalty for acquiring
// it on every iteration of the for loop. The thread ID is guaranteed to be in
// [0, num_threads).
//
// A performance analysis has shown this implementation is onpar with OpenMP and
// TBB.
void ParallelFor(ContextImpl* context,
                 int start,
                 int end,
                 int num_threads,
                 const std::function<void(int thread_id, int i)>& function) {
  CHECK_GT(num_threads, 0);
  CHECK(context != NULL);
  if (end <= start) {
    return;
  }

  // Fast path for when it is single threaded.
  if (num_threads == 1) {
    // Even though we only have one thread, use the thread token provider to
    // guarantee the exact same behavior when running with multiple threads.
    ThreadTokenProvider thread_token_provider(num_threads);
    const ScopedThreadToken scoped_thread_token(&thread_token_provider);
    const int thread_id = scoped_thread_token.token();
    for (int i = start; i < end; ++i) {
      function(thread_id, i);
    }
    return;
  }

  // We use a std::shared_ptr because the main thread can finish all
  // the work before the tasks have been popped off the queue.  So the
  // shared state needs to exist for the duration of all the tasks.
  const int num_work_items = std::min((end - start), num_threads);
  std::shared_ptr<SharedState> shared_state(
      new SharedState(start, end, num_work_items));

  // A function which tries to perform a chunk of work. This returns false if
  // there is no work to be done.
  auto task_function = [shared_state, &function]() {
    int i = 0;
    {
      // Get the next available chunk of work to be performed. If there is no
      // work, return false.
      std::lock_guard<std::mutex> lock(shared_state->mutex_i);
      if (shared_state->i >= shared_state->num_work_items) {
        return false;
      }
      i = shared_state->i;
      ++shared_state->i;
    }

    const ScopedThreadToken scoped_thread_token(
        &shared_state->thread_token_provider);
    const int thread_id = scoped_thread_token.token();

    // Perform each task.
    for (int j = shared_state->start + i;
         j < shared_state->end;
         j += shared_state->num_work_items) {
      function(thread_id, j);
    }
    shared_state->block_until_finished.Finished();
    return true;
  };

  // Add all the tasks to the thread pool.
  for (int i = 0; i < num_work_items; ++i) {
    // Note we are taking the task_function as value so the shared_state
    // shared pointer is copied and the ref count is increased. This is to
    // prevent it from being deleted when the main thread finishes all the
    // work and exits before the threads finish.
    context->thread_pool.AddTask([task_function]() { task_function(); });
  }

  // Try to do any available work on the main thread. This may steal work from
  // the thread pool, but when there is no work left the thread pool tasks
  // will be no-ops.
  while (task_function()) {
  }

  // Wait until all tasks have finished.
  shared_state->block_until_finished.Block();
}

}  // namespace internal
}  // namespace ceres

#endif // CERES_USE_CXX_THREADS
