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

#ifndef CERES_INTERNAL_PARALLEL_FOR_H_
#define CERES_INTERNAL_PARALLEL_FOR_H_

#include <mutex>
#include <vector>

#include "ceres/context_impl.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/parallel_invoke.h"
#include "ceres/partition_range_for_parallel_for.h"
#include "glog/logging.h"

namespace ceres::internal {

// Use a dummy mutex if num_threads = 1.
inline decltype(auto) MakeConditionalLock(const int num_threads,
                                          std::mutex& m) {
  return (num_threads == 1) ? std::unique_lock<std::mutex>{}
                            : std::unique_lock<std::mutex>{m};
}

// Execute the function for every element in the range [start, end) with at most
// num_threads. It will execute all the work on the calling thread if
// num_threads or (end - start) is equal to 1.
// Depending on function signature, it will be supplied with either loop index
// or a range of loop indicies; function can also be supplied with thread_id.
// The following function signatures are supported:
//  - Functions accepting a single loop index:
//     - [](int index) { ... }
//     - [](int thread_id, int index) { ... }
//  - Functions accepting a range of loop index:
//     - [](std::tuple<int, int> index) { ... }
//     - [](int thread_id, std::tuple<int, int> index) { ... }
//
// When distributing workload between threads, it is assumed that each loop
// iteration takes approximately equal time to complete.
template <typename F>
void ParallelFor(ContextImpl* context,
                 int start,
                 int end,
                 int num_threads,
                 F&& function,
                 int min_block_size = 1) {
  CHECK_GT(num_threads, 0);
  if (start >= end) {
    return;
  }

  if (num_threads == 1 || end - start < min_block_size * 2) {
    InvokeOnSegment(0, std::make_tuple(start, end), std::forward<F>(function));
    return;
  }

  CHECK(context != nullptr);
  ParallelInvoke(context,
                 start,
                 end,
                 num_threads,
                 std::forward<F>(function),
                 min_block_size);
}

// Execute function for every element in the range [start, end) with at most
// num_threads, using user-provided partitions array.
// When distributing workload between threads, it is assumed that each segment
// bounded by adjacent elements of partitions array takes approximately equal
// time to process.
template <typename F>
void ParallelFor(ContextImpl* context,
                 int start,
                 int end,
                 int num_threads,
                 F&& function,
                 const std::vector<int>& partitions) {
  CHECK_GT(num_threads, 0);
  if (start >= end) {
    return;
  }
  CHECK_EQ(partitions.front(), start);
  CHECK_EQ(partitions.back(), end);
  if (num_threads == 1 || end - start <= num_threads) {
    ParallelFor(context, start, end, num_threads, std::forward<F>(function));
    return;
  }
  CHECK_GT(partitions.size(), 1);
  const int num_partitions = partitions.size() - 1;
  ParallelFor(context,
              0,
              num_partitions,
              num_threads,
              [&function, &partitions](int thread_id,
                                       std::tuple<int, int> partition_ids) {
                // partition_ids is a range of partition indices
                const auto [partition_start, partition_end] = partition_ids;
                // Execution over several adjacent segments is equivalent
                // to execution over union of those segments (which is also a
                // contiguous segment)
                const int range_start = partitions[partition_start];
                const int range_end = partitions[partition_end];
                // Range of original loop indices
                const auto range = std::make_tuple(range_start, range_end);
                InvokeOnSegment(thread_id, range, function);
              });
}

// Execute function for every element in the range [start, end) with at most
// num_threads, taking into account user-provided integer cumulative costs of
// iterations. Cumulative costs of iteration for indices in range [0, end) are
// stored in objects from cumulative_cost_data. User-provided
// cumulative_cost_fun returns non-decreasing integer values corresponding to
// inclusive cumulative cost of loop iterations, provided with a reference to
// user-defined object. Only indices from [start, end) will be referenced. This
// routine assumes that cumulative_cost_fun is non-decreasing (in other words,
// all costs are non-negative);
// When distributing workload between threads, input range of loop indices will
// be partitioned into disjoint contiguous intervals, with the maximal cost
// being minimized.
// For example, with iteration costs of [1, 1, 5, 3, 1, 4] cumulative_cost_fun
// should return [1, 2, 7, 10, 11, 15], and with num_threads = 4 this range
// will be split into segments [0, 2) [2, 3) [3, 5) [5, 6) with costs
// [2, 5, 4, 4].
template <typename F, typename CumulativeCostData, typename CumulativeCostFun>
void ParallelFor(ContextImpl* context,
                 int start,
                 int end,
                 int num_threads,
                 F&& function,
                 const CumulativeCostData* cumulative_cost_data,
                 CumulativeCostFun&& cumulative_cost_fun) {
  CHECK_GT(num_threads, 0);
  if (start >= end) {
    return;
  }
  if (num_threads == 1 || end - start <= num_threads) {
    ParallelFor(context, start, end, num_threads, std::forward<F>(function));
    return;
  }
  // Creating several partitions allows us to tolerate imperfections of
  // partitioning and user-supplied iteration costs up to a certain extent
  constexpr int kNumPartitionsPerThread = 4;
  const int kMaxPartitions = num_threads * kNumPartitionsPerThread;
  const auto& partitions = PartitionRangeForParallelFor(
      start,
      end,
      kMaxPartitions,
      cumulative_cost_data,
      std::forward<CumulativeCostFun>(cumulative_cost_fun));
  CHECK_GT(partitions.size(), 1);
  ParallelFor(
      context, start, end, num_threads, std::forward<F>(function), partitions);
}
}  // namespace ceres::internal

#endif  // CERES_INTERNAL_PARALLEL_FOR_H_
