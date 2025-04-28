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
// Author: wjr@google.com (William Rucklidge)

#ifndef CERES_INTERNAL_PARALLEL_UTILS_H_
#define CERES_INTERNAL_PARALLEL_UTILS_H_

#include "ceres/internal/export.h"

namespace ceres::internal {

// Converts a linear iteration order into a triangular iteration order.
// Suppose you have nested loops that look like
// for (int i = 0; i < n; i++) {
//   for (int j = i; j < n; j++) {
//     ... use i and j
//   }
// }
// Naively using ParallelFor to parallelise those loops might look like
// ParallelFor(..., 0, n * n, num_threads,
//   [](int thread_id, int k) {
//     int i = k / n, j = k % n;
//     if (j < i) return;
//     ...
//    });
// but these empty work items can lead to very unbalanced threading. Instead,
// do this:
// int actual_work_items = (n * (n + 1)) / 2;
// ParallelFor(..., 0, actual_work_items, num_threads,
//   [](int thread_id, int k) {
//     int i, j;
//     UnfoldIteration(k, n, &i, &j);
//     ...
//    });
// which in each iteration will produce i and j satisfying
// 0 <= i <= j < n
CERES_NO_EXPORT void LinearIndexToUpperTriangularIndex(int k,
                                                       int n,
                                                       int* i,
                                                       int* j);

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_PARALLEL_UTILS_H_
