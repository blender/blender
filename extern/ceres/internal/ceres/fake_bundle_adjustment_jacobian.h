
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

#ifndef CERES_INTERNAL_FAKE_BUNDLE_ADJUSTMENT_JACOBIAN
#define CERES_INTERNAL_FAKE_BUNDLE_ADJUSTMENT_JACOBIAN

#include <memory>
#include <random>

#include "ceres/block_sparse_matrix.h"
#include "ceres/partitioned_matrix_view.h"

namespace ceres::internal {
std::unique_ptr<BlockSparseMatrix> CreateFakeBundleAdjustmentJacobian(
    int num_cameras,
    int num_points,
    int camera_size,
    int point_size,
    double visibility,
    std::mt19937& prng);

template <int kEBlockSize = 3, int kFBlockSize = 6>
std::pair<std::unique_ptr<PartitionedMatrixView<2, kEBlockSize, kFBlockSize>>,
          std::unique_ptr<BlockSparseMatrix>>
CreateFakeBundleAdjustmentPartitionedJacobian(int num_cameras,
                                              int num_points,
                                              double visibility,
                                              std::mt19937& rng) {
  using PartitionedView = PartitionedMatrixView<2, kEBlockSize, kFBlockSize>;
  auto block_sparse_matrix = CreateFakeBundleAdjustmentJacobian(
      num_cameras, num_points, kFBlockSize, kEBlockSize, visibility, rng);
  auto partitioned_view =
      std::make_unique<PartitionedView>(*block_sparse_matrix, num_points);
  return std::make_pair(std::move(partitioned_view),
                        std::move(block_sparse_matrix));
}

std::pair<
    std::unique_ptr<PartitionedMatrixView<2, Eigen::Dynamic, Eigen::Dynamic>>,
    std::unique_ptr<BlockSparseMatrix>>
CreateFakeBundleAdjustmentPartitionedJacobian(int num_cameras,
                                              int num_points,
                                              int camera_size,
                                              int landmark_size,
                                              double visibility,
                                              std::mt19937& rng);

}  // namespace ceres::internal

#endif  // CERES_INTERNAL_FAKE_BUNDLE_ADJUSTMENT_JACOBIAN
