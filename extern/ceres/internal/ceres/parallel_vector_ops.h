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

#ifndef CERES_INTERNAL_PARALLEL_VECTOR_OPS_H_
#define CERES_INTERNAL_PARALLEL_VECTOR_OPS_H_

#include <mutex>
#include <vector>

#include "ceres/context_impl.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/parallel_for.h"

namespace ceres::internal {

// Lower bound on block size for parallel vector operations.
// Operations with vectors of less than kMinBlockSizeParallelVectorOps elements
// will be executed in a single thread.
constexpr int kMinBlockSizeParallelVectorOps = 1 << 16;
// Evaluate vector expression in parallel
// Assuming LhsExpression and RhsExpression are some sort of column-vector
// expression, assignment lhs = rhs is eavluated over a set of contiguous blocks
// in parallel. This is expected to work well in the case of vector-based
// expressions (since they typically do not result into temporaries). This
// method expects lhs to be size-compatible with rhs
template <typename LhsExpression, typename RhsExpression>
void ParallelAssign(ContextImpl* context,
                    int num_threads,
                    LhsExpression& lhs,
                    const RhsExpression& rhs) {
  static_assert(LhsExpression::ColsAtCompileTime == 1);
  static_assert(RhsExpression::ColsAtCompileTime == 1);
  CHECK_EQ(lhs.rows(), rhs.rows());
  const int num_rows = lhs.rows();
  ParallelFor(
      context,
      0,
      num_rows,
      num_threads,
      [&lhs, &rhs](const std::tuple<int, int>& range) {
        auto [start, end] = range;
        lhs.segment(start, end - start) = rhs.segment(start, end - start);
      },
      kMinBlockSizeParallelVectorOps);
}

// Set vector to zero using num_threads
template <typename VectorType>
void ParallelSetZero(ContextImpl* context,
                     int num_threads,
                     VectorType& vector) {
  ParallelSetZero(context, num_threads, vector.data(), vector.rows());
}
void ParallelSetZero(ContextImpl* context,
                     int num_threads,
                     double* values,
                     int num_values);

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_PARALLEL_FOR_H_
