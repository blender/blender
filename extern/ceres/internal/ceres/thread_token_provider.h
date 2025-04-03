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
// Author: yp@photonscore.de (Yury Prokazov)

#ifndef CERES_INTERNAL_THREAD_TOKEN_PROVIDER_H_
#define CERES_INTERNAL_THREAD_TOKEN_PROVIDER_H_

#include "ceres/concurrent_queue.h"
#include "ceres/internal/config.h"
#include "ceres/internal/export.h"

namespace ceres::internal {

// Helper for C++ thread number identification that is similar to
// omp_get_thread_num() behaviour. This is necessary to support C++
// threading with a sequential thread id. This is used to access preallocated
// resources in the parallelized code parts.  The sequence of tokens varies from
// 0 to num_threads - 1 that can be acquired to identify the thread in a thread
// pool.
//
//
// Example usage pseudocode:
//
// ThreadTokenProvider ttp(N); // allocate N tokens
// Spawn N threads {
//    int token = ttp.Acquire(); // get unique token
//    ...
//    ... use token to access resources bound to the thread
//    ...
//    ttp.Release(token); // return token to the pool
//  }
//
class CERES_NO_EXPORT ThreadTokenProvider {
 public:
  explicit ThreadTokenProvider(int num_threads);

  // Returns the first token from the queue. The acquired value must be
  // given back by Release().
  int Acquire();

  // Makes previously acquired token available for other threads.
  void Release(int thread_id);

 private:
  // This queue initially holds a sequence from 0..num_threads-1. Every
  // Acquire() call the first number is removed from here. When the token is not
  // needed anymore it shall be given back with corresponding Release()
  // call. This concurrent queue is more expensive than TBB's version, so you
  // should not acquire the thread ID on every for loop iteration.
  ConcurrentQueue<int> pool_;
  ThreadTokenProvider(ThreadTokenProvider&) = delete;
  ThreadTokenProvider& operator=(ThreadTokenProvider&) = delete;
};

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_THREAD_TOKEN_PROVIDER_H_
