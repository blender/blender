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

#include "ceres/internal/config.h"

#ifndef CERES_NO_CUDA

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <numeric>

#include "ceres/block_sparse_matrix.h"
#include "ceres/cuda_block_structure.h"

namespace ceres::internal {

class CudaBlockStructureTest : public ::testing::Test {
 protected:
  void SetUp() final {
    std::string message;
    CHECK(context_.InitCuda(&message))
        << "InitCuda() failed because: " << message;

    BlockSparseMatrix::RandomMatrixOptions options;
    options.num_row_blocks = 1234;
    options.min_row_block_size = 1;
    options.max_row_block_size = 10;
    options.num_col_blocks = 567;
    options.min_col_block_size = 1;
    options.max_col_block_size = 10;
    options.block_density = 0.2;
    std::mt19937 rng;
    A_ = BlockSparseMatrix::CreateRandomMatrix(options, rng);
    std::iota(
        A_->mutable_values(), A_->mutable_values() + A_->num_nonzeros(), 1);
  }

  std::vector<Cell> GetCells(const CudaBlockSparseStructure& structure) {
    const auto& cuda_buffer = structure.cells_;
    std::vector<Cell> cells(cuda_buffer.size());
    cuda_buffer.CopyToCpu(cells.data(), cells.size());
    return cells;
  }
  std::vector<Block> GetRowBlocks(const CudaBlockSparseStructure& structure) {
    const auto& cuda_buffer = structure.row_blocks_;
    std::vector<Block> blocks(cuda_buffer.size());
    cuda_buffer.CopyToCpu(blocks.data(), blocks.size());
    return blocks;
  }
  std::vector<Block> GetColBlocks(const CudaBlockSparseStructure& structure) {
    const auto& cuda_buffer = structure.col_blocks_;
    std::vector<Block> blocks(cuda_buffer.size());
    cuda_buffer.CopyToCpu(blocks.data(), blocks.size());
    return blocks;
  }
  std::vector<int> GetRowBlockOffsets(
      const CudaBlockSparseStructure& structure) {
    const auto& cuda_buffer = structure.first_cell_in_row_block_;
    std::vector<int> first_cell_in_row_block(cuda_buffer.size());
    cuda_buffer.CopyToCpu(first_cell_in_row_block.data(),
                          first_cell_in_row_block.size());
    return first_cell_in_row_block;
  }

  std::unique_ptr<BlockSparseMatrix> A_;
  ContextImpl context_;
};

TEST_F(CudaBlockStructureTest, StructureIdentity) {
  auto block_structure = A_->block_structure();
  const int num_row_blocks = block_structure->rows.size();
  const int num_col_blocks = block_structure->cols.size();

  CudaBlockSparseStructure cuda_block_structure(*block_structure, &context_);

  ASSERT_EQ(cuda_block_structure.num_rows(), A_->num_rows());
  ASSERT_EQ(cuda_block_structure.num_cols(), A_->num_cols());
  ASSERT_EQ(cuda_block_structure.num_nonzeros(), A_->num_nonzeros());
  ASSERT_EQ(cuda_block_structure.num_row_blocks(), num_row_blocks);
  ASSERT_EQ(cuda_block_structure.num_col_blocks(), num_col_blocks);

  std::vector<Block> blocks = GetColBlocks(cuda_block_structure);
  ASSERT_EQ(blocks.size(), num_col_blocks);
  for (int i = 0; i < num_col_blocks; ++i) {
    EXPECT_EQ(block_structure->cols[i].position, blocks[i].position);
    EXPECT_EQ(block_structure->cols[i].size, blocks[i].size);
  }

  std::vector<Cell> cells = GetCells(cuda_block_structure);
  std::vector<int> first_cell_in_row_block =
      GetRowBlockOffsets(cuda_block_structure);
  blocks = GetRowBlocks(cuda_block_structure);

  ASSERT_EQ(blocks.size(), num_row_blocks);
  ASSERT_EQ(first_cell_in_row_block.size(), num_row_blocks + 1);
  ASSERT_EQ(first_cell_in_row_block.back(), cells.size());

  for (int i = 0; i < num_row_blocks; ++i) {
    const int num_cells = block_structure->rows[i].cells.size();
    EXPECT_EQ(blocks[i].position, block_structure->rows[i].block.position);
    EXPECT_EQ(blocks[i].size, block_structure->rows[i].block.size);
    const int first_cell = first_cell_in_row_block[i];
    const int last_cell = first_cell_in_row_block[i + 1];
    ASSERT_EQ(last_cell - first_cell, num_cells);
    for (int j = 0; j < num_cells; ++j) {
      EXPECT_EQ(cells[first_cell + j].block_id,
                block_structure->rows[i].cells[j].block_id);
      EXPECT_EQ(cells[first_cell + j].position,
                block_structure->rows[i].cells[j].position);
    }
  }
}

}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
