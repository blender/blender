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
// Authors: dmitriy.korchemkin@gmail.com (Dmitriy Korchemkin)

#include "ceres/cuda_partitioned_block_sparse_crs_view.h"

#include <glog/logging.h>
#include <gtest/gtest.h>

#ifndef CERES_NO_CUDA

namespace ceres::internal {

namespace {
struct RandomPartitionedMatrixOptions {
  int num_row_blocks_e;
  int num_row_blocks_f;
  int num_col_blocks_e;
  int num_col_blocks_f;
  int min_row_block_size;
  int max_row_block_size;
  int min_col_block_size;
  int max_col_block_size;
  double empty_f_probability;
  double cell_probability_f;
  int max_cells_f;
};

std::unique_ptr<BlockSparseMatrix> CreateRandomPartitionedMatrix(
    const RandomPartitionedMatrixOptions& options, std::mt19937& rng) {
  const int num_row_blocks =
      std::max(options.num_row_blocks_e, options.num_row_blocks_f);
  const int num_col_blocks =
      options.num_col_blocks_e + options.num_col_blocks_f;

  CompressedRowBlockStructure* block_structure =
      new CompressedRowBlockStructure;
  block_structure->cols.reserve(num_col_blocks);
  block_structure->rows.reserve(num_row_blocks);

  // Create column blocks
  std::uniform_int_distribution<int> col_size(options.min_col_block_size,
                                              options.max_col_block_size);
  int num_cols = 0;
  for (int i = 0; i < num_col_blocks; ++i) {
    const int size = col_size(rng);
    block_structure->cols.emplace_back(size, num_cols);
    num_cols += size;
  }

  // Prepare column-block indices of E cells
  std::vector<int> e_col_block_idx;
  e_col_block_idx.reserve(options.num_row_blocks_e);
  std::uniform_int_distribution<int> col_e(0, options.num_col_blocks_e - 1);
  for (int i = 0; i < options.num_row_blocks_e; ++i) {
    e_col_block_idx.emplace_back(col_e(rng));
  }
  std::sort(e_col_block_idx.begin(), e_col_block_idx.end());

  // Prepare cell structure
  std::uniform_int_distribution<int> row_size(options.min_row_block_size,
                                              options.max_row_block_size);
  std::uniform_real_distribution<double> uniform;
  int num_rows = 0;
  for (int i = 0; i < num_row_blocks; ++i) {
    const int size = row_size(rng);
    block_structure->rows.emplace_back();
    auto& row = block_structure->rows.back();
    row.block.size = size;
    row.block.position = num_rows;
    num_rows += size;
    if (i < options.num_row_blocks_e) {
      row.cells.emplace_back(e_col_block_idx[i], -1);
      if (uniform(rng) < options.empty_f_probability) {
        continue;
      }
    }
    if (i >= options.num_row_blocks_f) continue;
    const int cells_before = row.cells.size();
    for (int j = options.num_col_blocks_e; j < num_col_blocks; ++j) {
      if (uniform(rng) > options.cell_probability_f) {
        continue;
      }
      row.cells.emplace_back(j, -1);
    }
    if (row.cells.size() > cells_before + options.max_cells_f) {
      std::shuffle(row.cells.begin() + cells_before, row.cells.end(), rng);
      row.cells.resize(cells_before + options.max_cells_f);
      std::sort(
          row.cells.begin(), row.cells.end(), [](const auto& a, const auto& b) {
            return a.block_id < b.block_id;
          });
    }
  }

  // Fill positions in E sub-matrix
  int num_nonzeros = 0;
  for (int i = 0; i < options.num_row_blocks_e; ++i) {
    CHECK_GE(block_structure->rows[i].cells.size(), 1);
    block_structure->rows[i].cells[0].position = num_nonzeros;
    const int col_block_size =
        block_structure->cols[block_structure->rows[i].cells[0].block_id].size;
    const int row_block_size = block_structure->rows[i].block.size;
    num_nonzeros += row_block_size * col_block_size;
    CHECK_GE(num_nonzeros, 0);
  }
  // Fill positions in F sub-matrix
  for (int i = 0; i < options.num_row_blocks_f; ++i) {
    const int row_block_size = block_structure->rows[i].block.size;
    for (auto& cell : block_structure->rows[i].cells) {
      if (cell.position >= 0) continue;
      cell.position = num_nonzeros;
      const int col_block_size = block_structure->cols[cell.block_id].size;
      num_nonzeros += row_block_size * col_block_size;
      CHECK_GE(num_nonzeros, 0);
    }
  }
  // Populate values
  auto bsm = std::make_unique<BlockSparseMatrix>(block_structure, true);
  for (int i = 0; i < num_nonzeros; ++i) {
    bsm->mutable_values()[i] = i + 1;
  }
  return bsm;
}
}  // namespace

class CudaPartitionedBlockSparseCRSViewTest : public ::testing::Test {
  static constexpr int kNumColBlocksE = 456;

 protected:
  void SetUp() final {
    std::string message;
    CHECK(context_.InitCuda(&message))
        << "InitCuda() failed because: " << message;

    RandomPartitionedMatrixOptions options;
    options.num_row_blocks_f = 123;
    options.num_row_blocks_e = 456;
    options.num_col_blocks_f = 123;
    options.num_col_blocks_e = kNumColBlocksE;
    options.min_row_block_size = 1;
    options.max_row_block_size = 4;
    options.min_col_block_size = 1;
    options.max_col_block_size = 4;
    options.empty_f_probability = .1;
    options.cell_probability_f = .2;
    options.max_cells_f = options.num_col_blocks_f;

    std::mt19937 rng;
    short_f_ = CreateRandomPartitionedMatrix(options, rng);

    options.num_row_blocks_e = 123;
    options.num_row_blocks_f = 456;
    short_e_ = CreateRandomPartitionedMatrix(options, rng);

    options.max_cells_f = 1;
    options.num_row_blocks_e = options.num_row_blocks_f;
    options.num_row_blocks_e = options.num_row_blocks_f;
    f_crs_compatible_ = CreateRandomPartitionedMatrix(options, rng);
  }

  void TestMatrix(const BlockSparseMatrix& A_) {
    const int num_col_blocks_e = 456;
    CudaPartitionedBlockSparseCRSView view(A_, kNumColBlocksE, &context_);

    const int num_rows = A_.num_rows();
    const int num_cols = A_.num_cols();

    const auto& bs = *A_.block_structure();
    const int num_cols_e = bs.cols[num_col_blocks_e].position;
    const int num_cols_f = num_cols - num_cols_e;

    auto matrix_e = view.matrix_e();
    auto matrix_f = view.matrix_f();
    ASSERT_EQ(matrix_e->num_cols(), num_cols_e);
    ASSERT_EQ(matrix_e->num_rows(), num_rows);
    ASSERT_EQ(matrix_f->num_cols(), num_cols_f);
    ASSERT_EQ(matrix_f->num_rows(), num_rows);

    Vector x(num_cols);
    Vector x_left(num_cols_e);
    Vector x_right(num_cols_f);
    Vector y(num_rows);
    CudaVector x_cuda(&context_, num_cols);
    CudaVector x_left_cuda(&context_, num_cols_e);
    CudaVector x_right_cuda(&context_, num_cols_f);
    CudaVector y_cuda(&context_, num_rows);
    Vector y_cuda_host(num_rows);

    for (int i = 0; i < num_cols_e; ++i) {
      x.setZero();
      x_left.setZero();
      y.setZero();
      y_cuda.SetZero();
      x[i] = 1.;
      x_left[i] = 1.;
      x_left_cuda.CopyFromCpu(x_left);
      A_.RightMultiplyAndAccumulate(
          x.data(), y.data(), &context_, std::thread::hardware_concurrency());
      matrix_e->RightMultiplyAndAccumulate(x_left_cuda, &y_cuda);
      y_cuda.CopyTo(&y_cuda_host);
      // There will be up to 1 non-zero product per row, thus we expect an exact
      // match on 32-bit integer indices
      EXPECT_EQ((y - y_cuda_host).squaredNorm(), 0.);
    }
    for (int i = num_cols_e; i < num_cols_f; ++i) {
      x.setZero();
      x_right.setZero();
      y.setZero();
      y_cuda.SetZero();
      x[i] = 1.;
      x_right[i - num_cols_e] = 1.;
      x_right_cuda.CopyFromCpu(x_right);
      A_.RightMultiplyAndAccumulate(
          x.data(), y.data(), &context_, std::thread::hardware_concurrency());
      matrix_f->RightMultiplyAndAccumulate(x_right_cuda, &y_cuda);
      y_cuda.CopyTo(&y_cuda_host);
      // There will be up to 1 non-zero product per row, thus we expect an exact
      // match on 32-bit integer indices
      EXPECT_EQ((y - y_cuda_host).squaredNorm(), 0.);
    }
  }

  // E sub-matrix might have less row-blocks with cells than F sub-matrix. This
  // test matrix checks if this case is handled properly
  std::unique_ptr<BlockSparseMatrix> short_e_;
  // In case of non-crs compatible F matrix, permuting values from block-order
  // to crs order involves binary search over row-blocks of F. Having lots of
  // row-blocks with no F cells is an edge case for this algorithm.
  std::unique_ptr<BlockSparseMatrix> short_f_;
  // With F matrix being CRS-compatible, update of the values of partitioned
  // matrix view reduces to two host->device memcopies, and uses a separate code
  // path
  std::unique_ptr<BlockSparseMatrix> f_crs_compatible_;

  ContextImpl context_;
};

TEST_F(CudaPartitionedBlockSparseCRSViewTest, CreateUpdateValuesShortE) {
  TestMatrix(*short_e_);
}

TEST_F(CudaPartitionedBlockSparseCRSViewTest, CreateUpdateValuesShortF) {
  TestMatrix(*short_f_);
}

TEST_F(CudaPartitionedBlockSparseCRSViewTest,
       CreateUpdateValuesCrsCompatibleF) {
  TestMatrix(*f_crs_compatible_);
}
}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
