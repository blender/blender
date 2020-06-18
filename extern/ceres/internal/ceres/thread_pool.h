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

#ifndef CERES_INTERNAL_THREAD_POOL_H_
#define CERES_INTERNAL_THREAD_POOL_H_

#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "ceres/concurrent_queue.h"

namespace ceres {
namespace internal {

// A thread-safe thread pool with an unbounded task queue and a resizable number
// of workers.  The size of the thread pool can be increased but never decreased
// in order to support the largest number of threads requested.  The ThreadPool
// has three states:
//
//  (1) The thread pool size is zero.  Tasks may be added to the thread pool via
//  AddTask but they will not be executed until the thread pool is resized.
//
//  (2) The thread pool size is greater than zero.  Tasks may be added to the
//  thread pool and will be executed as soon as a worker is available.  The
//  thread pool may be resized while the thread pool is running.
//
//  (3) The thread pool is destructing.  The thread pool will signal all the
//  workers to stop.  The workers will finish all of the tasks that have already
//  been added to the thread pool.
//
class ThreadPool {
 public:
  // Returns the maximum number of hardware threads.
  static int MaxNumThreadsAvailable();

  // Default constructor with no active threads.  We allow instantiating a
  // thread pool with no threads to support the use case of single threaded
  // Ceres where everything will be executed on the main thread. For single
  // threaded execution this has two benefits: avoid any overhead as threads
  // are expensive to create, and no unused threads shown in the debugger.
  ThreadPool();

  // Instantiates a thread pool with min(MaxNumThreadsAvailable, num_threads)
  // number of threads.
  explicit ThreadPool(int num_threads);

  // Signals the workers to stop and waits for them to finish any tasks that
  // have been scheduled.
  ~ThreadPool();

  // Resizes the thread pool if it is currently less than the requested number
  // of threads.  The thread pool will be resized to min(MaxNumThreadsAvailable,
  // num_threads) number of threads.  Resize does not support reducing the
  // thread pool size.  If a smaller number of threads is requested, the thread
  // pool remains the same size.  The thread pool is reused within Ceres with
  // different number of threads, and we need to ensure we can support the
  // largest number of threads requested.  It is safe to resize the thread pool
  // while the workers are executing tasks, and the resizing is guaranteed to
  // complete upon return.
  void Resize(int num_threads);

  // Adds a task to the queue and wakes up a blocked thread.  If the thread pool
  // size is greater than zero, then the task will be executed by a currently
  // idle thread or when a thread becomes available.  If the thread pool has no
  // threads, then the task will never be executed and the user should use
  // Resize() to create a non-empty thread pool.
  void AddTask(const std::function<void()>& func);

  // Returns the current size of the thread pool.
  int Size();

 private:
  // Main loop for the threads which blocks on the task queue until work becomes
  // available.  It will return if and only if Stop has been called.
  void ThreadMainLoop();

  // Signal all the threads to stop.  It does not block until the threads are
  // finished.
  void Stop();

  // The queue that stores the units of work available for the thread pool.  The
  // task queue maintains its own thread safety.
  ConcurrentQueue<std::function<void()>> task_queue_;
  std::vector<std::thread> thread_pool_;
  std::mutex thread_pool_mutex_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_THREAD_POOL_H_
