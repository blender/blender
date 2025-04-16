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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_INTERNAL_EIGEN_VECTOR_OPS_H_
#define CERES_INTERNAL_EIGEN_VECTOR_OPS_H_

#include <numeric>

#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/parallel_for.h"
#include "ceres/parallel_vector_ops.h"

namespace ceres::internal {

// Blas1 operations on Eigen vectors. These functions are needed as an
// abstraction layer so that we can use different versions of a vector style
// object in the conjugate gradients linear solver.
template <typename Derived>
inline double Norm(const Eigen::DenseBase<Derived>& x,
                   ContextImpl* context,
                   int num_threads) {
  FixedArray<double> norms(num_threads, 0.);
  ParallelFor(
      context,
      0,
      x.rows(),
      num_threads,
      [&x, &norms](int thread_id, std::tuple<int, int> range) {
        auto [start, end] = range;
        norms[thread_id] += x.segment(start, end - start).squaredNorm();
      },
      kMinBlockSizeParallelVectorOps);
  return std::sqrt(std::accumulate(norms.begin(), norms.end(), 0.));
}
inline void SetZero(Vector& x, ContextImpl* context, int num_threads) {
  ParallelSetZero(context, num_threads, x);
}
inline void Axpby(double a,
                  const Vector& x,
                  double b,
                  const Vector& y,
                  Vector& z,
                  ContextImpl* context,
                  int num_threads) {
  ParallelAssign(context, num_threads, z, a * x + b * y);
}
template <typename VectorLikeX, typename VectorLikeY>
inline double Dot(const VectorLikeX& x,
                  const VectorLikeY& y,
                  ContextImpl* context,
                  int num_threads) {
  FixedArray<double> dots(num_threads, 0.);
  ParallelFor(
      context,
      0,
      x.rows(),
      num_threads,
      [&x, &y, &dots](int thread_id, std::tuple<int, int> range) {
        auto [start, end] = range;
        const int block_size = end - start;
        const auto& x_block = x.segment(start, block_size);
        const auto& y_block = y.segment(start, block_size);
        dots[thread_id] += x_block.dot(y_block);
      },
      kMinBlockSizeParallelVectorOps);
  return std::accumulate(dots.begin(), dots.end(), 0.);
}
inline void Copy(const Vector& from,
                 Vector& to,
                 ContextImpl* context,
                 int num_threads) {
  ParallelAssign(context, num_threads, to, from);
}

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_EIGEN_VECTOR_OPS_H_
