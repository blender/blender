// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
// Authors: vitus@google.com (Michael Vitus),
//          dmitriy.korchemkin@gmail.com (Dmitriy Korchemkin)

#ifndef CERES_INTERNAL_PARALLEL_INVOKE_H_
#define CERES_INTERNAL_PARALLEL_INVOKE_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>

namespace ceres::internal {

// InvokeWithThreadId handles passing thread_id to the function
template <typename F, typename... Args>
void InvokeWithThreadId(int thread_id, F&& function, Args&&... args) {
  constexpr bool kPassThreadId = std::is_invocable_v<F, int, Args...>;

  if constexpr (kPassThreadId) {
    function(thread_id, std::forward<Args>(args)...);
  } else {
    function(std::forward<Args>(args)...);
  }
}

// InvokeOnSegment either runs a loop over segment indices or passes it to the
// function
template <typename F>
void InvokeOnSegment(int thread_id, std::tuple<int, int> range, F&& function) {
  constexpr bool kExplicitLoop =
      std::is_invocable_v<F, int> || std::is_invocable_v<F, int, int>;

  if constexpr (kExplicitLoop) {
    const auto [start, end] = range;
    for (int i = start; i != end; ++i) {
      InvokeWithThreadId(thread_id, std::forward<F>(function), i);
    }
  } else {
    InvokeWithThreadId(thread_id, std::forward<F>(function), range);
  }
}

// This class creates a thread safe barrier which will block until a
// pre-specified number of threads call Finished.  This allows us to block the
// main thread until all the parallel threads are finished processing all the
// work.
class BlockUntilFinished {
 public:
  explicit BlockUntilFinished(int num_total_jobs);

  // Increment the number of jobs that have been processed by the number of
  // jobs processed by caller and signal the blocking thread if all jobs
  // have finished.
  void Finished(int num_jobs_finished);

  // Block until receiving confirmation of all jobs being finished.
  void Block();

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  int num_total_jobs_finished_;
  const int num_total_jobs_;
};

// Shared state between the parallel tasks. Each thread will use this
// information to get the next block of work to be performed.
struct ParallelInvokeState {
  // The entire range [start, end) is split into num_work_blocks contiguous
  // disjoint intervals (blocks), which are as equal as possible given
  // total index count and requested number of  blocks.
  //
  // Those num_work_blocks blocks are then processed in parallel.
  //
  // Total number of integer indices in interval [start, end) is
  // end - start, and when splitting them into num_work_blocks blocks
  // we can either
  //  - Split into equal blocks when (end - start) is divisible by
  //    num_work_blocks
  //  - Split into blocks with size difference at most 1:
  //     - Size of the smallest block(s) is (end - start) / num_work_blocks
  //     - (end - start) % num_work_blocks will need to be 1 index larger
  //
  // Note that this splitting is optimal in the sense of maximal difference
  // between block sizes, since splitting into equal blocks is possible
  // if and only if number of indices is divisible by number of blocks.
  ParallelInvokeState(int start, int end, int num_work_blocks);

  // The start and end index of the for loop.
  const int start;
  const int end;
  // The number of blocks that need to be processed.
  const int num_work_blocks;
  // Size of the smallest block
  const int base_block_size;
  // Number of blocks of size base_block_size + 1
  const int num_base_p1_sized_blocks;

  // The next block of work to be assigned to a worker.  The parallel for loop
  // range is split into num_work_blocks blocks of work, with a single block of
  // work being of size
  //  - base_block_size + 1 for the first num_base_p1_sized_blocks blocks
  //  - base_block_size for the rest of the blocks
  //  blocks of indices are contiguous and disjoint
  std::atomic<int> block_id;

  // Provides a unique thread ID among all active threads
  // We do not schedule more than num_threads threads via thread pool
  // and caller thread might steal one ID
  std::atomic<int> thread_id;

  // Used to signal when all the work has been completed.  Thread safe.
  BlockUntilFinished block_until_finished;
};

// This implementation uses a fixed size max worker pool with a shared task
// queue. The problem of executing the function for the interval of [start, end)
// is broken up into at most num_threads * kWorkBlocksPerThread blocks (each of
// size at least min_block_size) and added to the thread pool. To avoid
// deadlocks, the calling thread is allowed to steal work from the worker pool.
// This is implemented via a shared state between the tasks. In order for
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
// A performance analysis has shown this implementation is on par with OpenMP
// and TBB.
template <typename F>
void ParallelInvoke(ContextImpl* context,
                    int start,
                    int end,
                    int num_threads,
                    F&& function,
                    int min_block_size) {
  CHECK(context != nullptr);

  // Maximal number of work items scheduled for a single thread
  //  - Lower number of work items results in larger runtimes on unequal tasks
  //  - Higher number of work items results in larger losses for synchronization
  constexpr int kWorkBlocksPerThread = 4;

  // Interval [start, end) is being split into
  // num_threads * kWorkBlocksPerThread contiguous disjoint blocks.
  //
  // In order to avoid creating empty blocks of work, we need to limit
  // number of work blocks by a total number of indices.
  const int num_work_blocks = std::min((end - start) / min_block_size,
                                       num_threads * kWorkBlocksPerThread);

  // We use a std::shared_ptr because the main thread can finish all
  // the work before the tasks have been popped off the queue.  So the
  // shared state needs to exist for the duration of all the tasks.
  auto shared_state =
      std::make_shared<ParallelInvokeState>(start, end, num_work_blocks);

  // A function which tries to schedule another task in the thread pool and
  // perform several chunks of work. Function expects itself as the argument in
  // order to schedule next task in the thread pool.
  auto task = [context, shared_state, num_threads, &function](auto& task_copy) {
    int num_jobs_finished = 0;
    const int thread_id = shared_state->thread_id.fetch_add(1);
    // In order to avoid dead-locks in nested parallel for loops, task() will be
    // invoked num_threads + 1 times:
    //  - num_threads times via enqueueing task into thread pool
    //  - one more time in the main thread
    //  Tasks enqueued to thread pool might take some time before execution, and
    //  the last task being executed will be terminated here in order to avoid
    //  having more than num_threads active threads
    if (thread_id >= num_threads) return;
    const int num_work_blocks = shared_state->num_work_blocks;
    if (thread_id + 1 < num_threads &&
        shared_state->block_id < num_work_blocks) {
      // Add another thread to the thread pool.
      // Note we are taking the task as value so the copy of shared_state shared
      // pointer (captured by value at declaration of task lambda-function) is
      // copied and the ref count is increased. This is to prevent it from being
      // deleted when the main thread finishes all the work and exits before the
      // threads finish.
      context->thread_pool.AddTask([task_copy]() { task_copy(task_copy); });
    }

    const int start = shared_state->start;
    const int base_block_size = shared_state->base_block_size;
    const int num_base_p1_sized_blocks = shared_state->num_base_p1_sized_blocks;

    while (true) {
      // Get the next available chunk of work to be performed. If there is no
      // work, return.
      int block_id = shared_state->block_id.fetch_add(1);
      if (block_id >= num_work_blocks) {
        break;
      }
      ++num_jobs_finished;

      // For-loop interval [start, end) was split into num_work_blocks,
      // with num_base_p1_sized_blocks of size base_block_size + 1 and remaining
      // num_work_blocks - num_base_p1_sized_blocks of size base_block_size
      //
      // Then, start index of the block #block_id is given by a total
      // length of preceeding blocks:
      //  * Total length of preceeding blocks of size base_block_size + 1:
      //     min(block_id, num_base_p1_sized_blocks) * (base_block_size + 1)
      //
      //  * Total length of preceeding blocks of size base_block_size:
      //     (block_id - min(block_id, num_base_p1_sized_blocks)) *
      //     base_block_size
      //
      // Simplifying sum of those quantities yields a following
      // expression for start index of the block #block_id
      const int curr_start = start + block_id * base_block_size +
                             std::min(block_id, num_base_p1_sized_blocks);
      // First num_base_p1_sized_blocks have size base_block_size + 1
      //
      // Note that it is guaranteed that all blocks are within
      // [start, end) interval
      const int curr_end = curr_start + base_block_size +
                           (block_id < num_base_p1_sized_blocks ? 1 : 0);
      // Perform each task in current block
      const auto range = std::make_tuple(curr_start, curr_end);
      InvokeOnSegment(thread_id, range, function);
    }
    shared_state->block_until_finished.Finished(num_jobs_finished);
  };

  // Start scheduling threads and doing work. We might end up with less threads
  // scheduled than expected, if scheduling overhead is larger than the amount
  // of work to be done.
  task(task);

  // Wait until all tasks have finished.
  shared_state->block_until_finished.Block();
}

}  // namespace ceres::internal

#endif
