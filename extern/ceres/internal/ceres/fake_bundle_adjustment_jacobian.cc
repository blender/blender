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
// Authors: joydeepb@cs.utexas.edu (Joydeep Biswas)

#include "ceres/fake_bundle_adjustment_jacobian.h"

#include <memory>
#include <random>
#include <string>
#include <utility>

#include "Eigen/Dense"
#include "ceres/block_sparse_matrix.h"
#include "ceres/internal/eigen.h"

namespace ceres::internal {

std::unique_ptr<BlockSparseMatrix> CreateFakeBundleAdjustmentJacobian(
    int num_cameras,
    int num_points,
    int camera_size,
    int point_size,
    double visibility,
    std::mt19937& prng) {
  constexpr int kResidualSize = 2;

  CompressedRowBlockStructure* bs = new CompressedRowBlockStructure;
  int c = 0;
  // Add column blocks for each point
  for (int i = 0; i < num_points; ++i) {
    bs->cols.push_back(Block(point_size, c));
    c += point_size;
  }

  // Add column blocks for each camera.
  for (int i = 0; i < num_cameras; ++i) {
    bs->cols.push_back(Block(camera_size, c));
    c += camera_size;
  }

  std::bernoulli_distribution visibility_distribution(visibility);
  int row_pos = 0;
  int cell_pos = 0;
  for (int i = 0; i < num_points; ++i) {
    for (int j = 0; j < num_cameras; ++j) {
      if (!visibility_distribution(prng)) {
        continue;
      }
      bs->rows.emplace_back();
      auto& row = bs->rows.back();
      row.block.position = row_pos;
      row.block.size = kResidualSize;
      auto& cells = row.cells;
      cells.resize(2);

      cells[0].block_id = i;
      cells[0].position = cell_pos;
      cell_pos += kResidualSize * point_size;

      cells[1].block_id = num_points + j;
      cells[1].position = cell_pos;
      cell_pos += kResidualSize * camera_size;

      row_pos += kResidualSize;
    }
  }

  auto jacobian = std::make_unique<BlockSparseMatrix>(bs);
  VectorRef(jacobian->mutable_values(), jacobian->num_nonzeros()).setRandom();
  return jacobian;
}

std::pair<
    std::unique_ptr<PartitionedMatrixView<2, Eigen::Dynamic, Eigen::Dynamic>>,
    std::unique_ptr<BlockSparseMatrix>>
CreateFakeBundleAdjustmentPartitionedJacobian(int num_cameras,
                                              int num_points,
                                              int camera_size,
                                              int landmark_size,
                                              double visibility,
                                              std::mt19937& rng) {
  using PartitionedView =
      PartitionedMatrixView<2, Eigen::Dynamic, Eigen::Dynamic>;
  auto block_sparse_matrix = CreateFakeBundleAdjustmentJacobian(
      num_cameras, num_points, camera_size, landmark_size, visibility, rng);
  LinearSolver::Options options;
  options.elimination_groups.push_back(num_points);
  auto partitioned_view =
      std::make_unique<PartitionedView>(options, *block_sparse_matrix);
  return std::make_pair(std::move(partitioned_view),
                        std::move(block_sparse_matrix));
}

}  // namespace ceres::internal
