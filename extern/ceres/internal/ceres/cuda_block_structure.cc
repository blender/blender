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

#include "ceres/cuda_block_structure.h"

#ifndef CERES_NO_CUDA

namespace ceres::internal {
namespace {
// Dimension of a sorted array of blocks
inline int Dimension(const std::vector<Block>& blocks) {
  if (blocks.empty()) {
    return 0;
  }
  const auto& last = blocks.back();
  return last.size + last.position;
}
}  // namespace
CudaBlockSparseStructure::CudaBlockSparseStructure(
    const CompressedRowBlockStructure& block_structure, ContextImpl* context)
    : CudaBlockSparseStructure(block_structure, 0, context) {}

CudaBlockSparseStructure::CudaBlockSparseStructure(
    const CompressedRowBlockStructure& block_structure,
    const int num_col_blocks_e,
    ContextImpl* context)
    : first_cell_in_row_block_(context),
      value_offset_row_block_f_(context),
      cells_(context),
      row_blocks_(context),
      col_blocks_(context) {
  // Row blocks extracted from CompressedRowBlockStructure::rows
  std::vector<Block> row_blocks;
  // Column blocks can be reused as-is
  const auto& col_blocks = block_structure.cols;

  // Row block offset is an index of the first cell corresponding to row block
  std::vector<int> first_cell_in_row_block;
  // Offset of the first value in the first non-empty row-block of F sub-matrix
  std::vector<int> value_offset_row_block_f;
  // Flat array of all cells from all row-blocks
  std::vector<Cell> cells;

  int f_values_offset = -1;
  num_nonzeros_e_ = 0;
  is_crs_compatible_ = true;
  num_row_blocks_ = block_structure.rows.size();
  num_col_blocks_ = col_blocks.size();

  row_blocks.reserve(num_row_blocks_);
  first_cell_in_row_block.reserve(num_row_blocks_ + 1);
  value_offset_row_block_f.reserve(num_row_blocks_ + 1);
  num_nonzeros_ = 0;
  // Block-sparse matrices arising from block-jacobian writer are expected to
  // have sequential layout (for partitioned matrices - it is expected that both
  // E and F sub-matrices have sequential layout).
  bool sequential_layout = true;
  int row_block_id = 0;
  num_row_blocks_e_ = 0;
  for (; row_block_id < num_row_blocks_; ++row_block_id) {
    const auto& r = block_structure.rows[row_block_id];
    const int row_block_size = r.block.size;
    const int num_cells = r.cells.size();

    if (num_col_blocks_e == 0 || r.cells.size() == 0 ||
        r.cells[0].block_id >= num_col_blocks_e) {
      break;
    }
    num_row_blocks_e_ = row_block_id + 1;
    // In E sub-matrix there is exactly a single E cell in the row
    // since E cells are stored separately from F cells, crs-compatiblity of
    // F sub-matrix only breaks if there are more than 2 cells in row (that
    // is, more than 1 cell in F sub-matrix)
    if (num_cells > 2 && row_block_size > 1) {
      is_crs_compatible_ = false;
    }
    row_blocks.emplace_back(r.block);
    first_cell_in_row_block.push_back(cells.size());

    for (int cell_id = 0; cell_id < num_cells; ++cell_id) {
      const auto& c = r.cells[cell_id];
      const int col_block_size = col_blocks[c.block_id].size;
      const int cell_size = col_block_size * row_block_size;
      cells.push_back(c);
      if (cell_id == 0) {
        DCHECK(c.position == num_nonzeros_e_);
        num_nonzeros_e_ += cell_size;
      } else {
        if (f_values_offset == -1) {
          num_nonzeros_ = c.position;
          f_values_offset = c.position;
        }
        sequential_layout &= c.position == num_nonzeros_;
        num_nonzeros_ += cell_size;
        if (cell_id == 1) {
          // Correct value_offset_row_block_f for empty row-blocks of F
          // preceding this one
          for (auto it = value_offset_row_block_f.rbegin();
               it != value_offset_row_block_f.rend();
               ++it) {
            if (*it != -1) break;
            *it = c.position;
          }
          value_offset_row_block_f.push_back(c.position);
        }
      }
    }
    if (num_cells == 1) {
      value_offset_row_block_f.push_back(-1);
    }
  }
  for (; row_block_id < num_row_blocks_; ++row_block_id) {
    const auto& r = block_structure.rows[row_block_id];
    const int row_block_size = r.block.size;
    const int num_cells = r.cells.size();
    // After num_row_blocks_e_ row-blocks, there should be no cells in E
    // sub-matrix. Thus crs-compatibility of F sub-matrix breaks if there are
    // more than one cells in the row-block
    if (num_cells > 1 && row_block_size > 1) {
      is_crs_compatible_ = false;
    }
    row_blocks.emplace_back(r.block);
    first_cell_in_row_block.push_back(cells.size());

    if (r.cells.empty()) {
      value_offset_row_block_f.push_back(-1);
    } else {
      for (auto it = value_offset_row_block_f.rbegin();
           it != value_offset_row_block_f.rend();
           --it) {
        if (*it != -1) break;
        *it = cells[0].position;
      }
      value_offset_row_block_f.push_back(r.cells[0].position);
    }
    for (const auto& c : r.cells) {
      const int col_block_size = col_blocks[c.block_id].size;
      const int cell_size = col_block_size * row_block_size;
      cells.push_back(c);
      DCHECK(c.block_id >= num_col_blocks_e);
      if (f_values_offset == -1) {
        num_nonzeros_ = c.position;
        f_values_offset = c.position;
      }
      sequential_layout &= c.position == num_nonzeros_;
      num_nonzeros_ += cell_size;
    }
  }

  if (f_values_offset == -1) {
    f_values_offset = num_nonzeros_e_;
    num_nonzeros_ = num_nonzeros_e_;
  }
  // Fill non-zero offsets for the last rows of F submatrix
  for (auto it = value_offset_row_block_f.rbegin();
       it != value_offset_row_block_f.rend();
       ++it) {
    if (*it != -1) break;
    *it = num_nonzeros_;
  }
  value_offset_row_block_f.push_back(num_nonzeros_);
  CHECK_EQ(num_nonzeros_e_, f_values_offset);
  first_cell_in_row_block.push_back(cells.size());
  num_cells_ = cells.size();

  num_rows_ = Dimension(row_blocks);
  num_cols_ = Dimension(col_blocks);

  CHECK(sequential_layout);

  if (VLOG_IS_ON(3)) {
    const size_t first_cell_in_row_block_size =
        first_cell_in_row_block.size() * sizeof(int);
    const size_t cells_size = cells.size() * sizeof(Cell);
    const size_t row_blocks_size = row_blocks.size() * sizeof(Block);
    const size_t col_blocks_size = col_blocks.size() * sizeof(Block);
    const size_t total_size = first_cell_in_row_block_size + cells_size +
                              col_blocks_size + row_blocks_size;
    const double ratio =
        (100. * total_size) / (num_nonzeros_ * (sizeof(int) + sizeof(double)) +
                               num_rows_ * sizeof(int));
    VLOG(3) << "\nCudaBlockSparseStructure:\n"
               "\tRow block offsets: "
            << first_cell_in_row_block_size
            << " bytes\n"
               "\tColumn blocks: "
            << col_blocks_size
            << " bytes\n"
               "\tRow blocks: "
            << row_blocks_size
            << " bytes\n"
               "\tCells: "
            << cells_size << " bytes\n\tTotal: " << total_size
            << " bytes of GPU memory (" << ratio << "% of CRS matrix size)";
  }

  first_cell_in_row_block_.CopyFromCpuVector(first_cell_in_row_block);
  cells_.CopyFromCpuVector(cells);
  row_blocks_.CopyFromCpuVector(row_blocks);
  col_blocks_.CopyFromCpuVector(col_blocks);
  if (num_col_blocks_e || num_row_blocks_e_) {
    value_offset_row_block_f_.CopyFromCpuVector(value_offset_row_block_f);
  }
}
}  // namespace ceres::internal

#endif  // CERES_NO_CUDA
