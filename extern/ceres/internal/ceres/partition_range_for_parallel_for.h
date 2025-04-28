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

#ifndef CERES_INTERNAL_PARTITION_RANGE_FOR_PARALLEL_FOR_H_
#define CERES_INTERNAL_PARTITION_RANGE_FOR_PARALLEL_FOR_H_

#include <algorithm>
#include <vector>

namespace ceres::internal {
// Check if it is possible to split range [start; end) into at most
// max_num_partitions  contiguous partitions of cost not greater than
// max_partition_cost. Inclusive integer cumulative costs are provided by
// cumulative_cost_data objects, with cumulative_cost_offset being a total cost
// of all indices (starting from zero) preceding start element. Cumulative costs
// are returned by cumulative_cost_fun called with a reference to
// cumulative_cost_data element with index from range[start; end), and should be
// non-decreasing. Partition of the range is returned via partition argument
template <typename CumulativeCostData, typename CumulativeCostFun>
bool MaxPartitionCostIsFeasible(int start,
                                int end,
                                int max_num_partitions,
                                int max_partition_cost,
                                int cumulative_cost_offset,
                                const CumulativeCostData* cumulative_cost_data,
                                CumulativeCostFun&& cumulative_cost_fun,
                                std::vector<int>* partition) {
  partition->clear();
  partition->push_back(start);
  int partition_start = start;
  int cost_offset = cumulative_cost_offset;

  while (partition_start < end) {
    // Already have max_num_partitions
    if (partition->size() > max_num_partitions) {
      return false;
    }
    const int target = max_partition_cost + cost_offset;
    const int partition_end =
        std::partition_point(
            cumulative_cost_data + partition_start,
            cumulative_cost_data + end,
            [&cumulative_cost_fun, target](const CumulativeCostData& item) {
              return cumulative_cost_fun(item) <= target;
            }) -
        cumulative_cost_data;
    // Unable to make a partition from a single element
    if (partition_end == partition_start) {
      return false;
    }

    const int cost_last =
        cumulative_cost_fun(cumulative_cost_data[partition_end - 1]);
    partition->push_back(partition_end);
    partition_start = partition_end;
    cost_offset = cost_last;
  }
  return true;
}

// Split integer interval [start, end) into at most max_num_partitions
// contiguous intervals, minimizing maximal total cost of a single interval.
// Inclusive integer cumulative costs for each (zero-based) index are provided
// by cumulative_cost_data objects, and are returned by cumulative_cost_fun call
// with a reference to one of the objects from range [start, end)
template <typename CumulativeCostData, typename CumulativeCostFun>
std::vector<int> PartitionRangeForParallelFor(
    int start,
    int end,
    int max_num_partitions,
    const CumulativeCostData* cumulative_cost_data,
    CumulativeCostFun&& cumulative_cost_fun) {
  // Given maximal partition cost, it is possible to verify if it is admissible
  // and obtain corresponding partition using MaxPartitionCostIsFeasible
  // function. In order to find the lowest admissible value, a binary search
  // over all potentially optimal cost values is being performed
  const int cumulative_cost_last =
      cumulative_cost_fun(cumulative_cost_data[end - 1]);
  const int cumulative_cost_offset =
      start ? cumulative_cost_fun(cumulative_cost_data[start - 1]) : 0;
  const int total_cost = cumulative_cost_last - cumulative_cost_offset;

  // Minimal maximal partition cost is not smaller than the average
  // We will use non-inclusive lower bound
  int partition_cost_lower_bound = total_cost / max_num_partitions - 1;
  // Minimal maximal partition cost is not larger than the total cost
  // Upper bound is inclusive
  int partition_cost_upper_bound = total_cost;

  std::vector<int> partition;
  // Range partition corresponding to the latest evaluated upper bound.
  // A single segment covering the whole input interval [start, end) corresponds
  // to minimal maximal partition cost of total_cost.
  std::vector<int> partition_upper_bound = {start, end};
  // Binary search over partition cost, returning the lowest admissible cost
  while (partition_cost_upper_bound - partition_cost_lower_bound > 1) {
    partition.reserve(max_num_partitions + 1);
    const int partition_cost =
        partition_cost_lower_bound +
        (partition_cost_upper_bound - partition_cost_lower_bound) / 2;
    bool admissible = MaxPartitionCostIsFeasible(
        start,
        end,
        max_num_partitions,
        partition_cost,
        cumulative_cost_offset,
        cumulative_cost_data,
        std::forward<CumulativeCostFun>(cumulative_cost_fun),
        &partition);
    if (admissible) {
      partition_cost_upper_bound = partition_cost;
      std::swap(partition, partition_upper_bound);
    } else {
      partition_cost_lower_bound = partition_cost;
    }
  }

  return partition_upper_bound;
}
}  // namespace ceres::internal

#endif
