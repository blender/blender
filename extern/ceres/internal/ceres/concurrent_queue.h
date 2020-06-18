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

#ifndef CERES_INTERNAL_CONCURRENT_QUEUE_H_
#define CERES_INTERNAL_CONCURRENT_QUEUE_H_

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "glog/logging.h"

namespace ceres {
namespace internal {

// A thread-safe multi-producer, multi-consumer queue for queueing items that
// are typically handled asynchronously by multiple threads. The ConcurrentQueue
// has two states which only affect the Wait call:
//
//  (1) Waiters have been enabled (enabled by default or calling
//      EnableWaiters). The call to Wait will block until an item is available.
//      Push and pop will operate as expected.
//
//  (2) StopWaiters has been called. All threads blocked in a Wait() call will
//      be woken up and pop any available items from the queue. All future Wait
//      requests will either return an element from the queue or return
//      immediately if no element is present.  Push and pop will operate as
//      expected.
//
// A common use case is using the concurrent queue as an interface for
// scheduling tasks for a set of thread workers:
//
// ConcurrentQueue<Task> task_queue;
//
// [Worker threads]:
//   Task task;
//   while(task_queue.Wait(&task)) {
//     ...
//   }
//
// [Producers]:
//   task_queue.Push(...);
//   ..
//   task_queue.Push(...);
//   ...
//   // Signal worker threads to stop blocking on Wait and terminate.
//   task_queue.StopWaiters();
//
template <typename T>
class ConcurrentQueue {
 public:
  // Defaults the queue to blocking on Wait calls.
  ConcurrentQueue() : wait_(true) {}

  // Atomically push an element onto the queue.  If a thread was waiting for an
  // element, wake it up.
  void Push(const T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(value);
    work_pending_condition_.notify_one();
  }

  // Atomically pop an element from the queue.  If an element is present, return
  // true. If the queue was empty, return false.
  bool Pop(T* value) {
    CHECK(value != nullptr);

    std::lock_guard<std::mutex> lock(mutex_);
    return PopUnlocked(value);
  }

  // Atomically pop an element from the queue. Blocks until one is available or
  // StopWaiters is called.  Returns true if an element was successfully popped
  // from the queue, otherwise returns false.
  bool Wait(T* value) {
    CHECK(value != nullptr);

    std::unique_lock<std::mutex> lock(mutex_);
    work_pending_condition_.wait(lock,
                                 [&]() { return !(wait_ && queue_.empty()); });

    return PopUnlocked(value);
  }

  // Unblock all threads waiting to pop a value from the queue, and they will
  // exit Wait() without getting a value. All future Wait requests will return
  // immediately if no element is present until EnableWaiters is called.
  void StopWaiters() {
    std::lock_guard<std::mutex> lock(mutex_);
    wait_ = false;
    work_pending_condition_.notify_all();
  }

  // Enable threads to block on Wait calls.
  void EnableWaiters() {
    std::lock_guard<std::mutex> lock(mutex_);
    wait_ = true;
  }

 private:
  // Pops an element from the queue.  If an element is present, return
  // true. If the queue was empty, return false.  Not thread-safe. Must acquire
  // the lock before calling.
  bool PopUnlocked(T* value) {
    if (queue_.empty()) {
      return false;
    }

    *value = queue_.front();
    queue_.pop();

    return true;
  }

  // The mutex controls read and write access to the queue_ and stop_
  // variables. It is also used to block the calling thread until an element is
  // available to pop from the queue.
  std::mutex mutex_;
  std::condition_variable work_pending_condition_;

  std::queue<T> queue_;
  // If true, signals that callers of Wait will block waiting to pop an
  // element off the queue.
  bool wait_;
};


}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_CONCURRENT_QUEUE_H_
