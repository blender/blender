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
//
// Block structure objects are used to carry information about the
// dense block structure of sparse matrices. The BlockSparseMatrix
// object uses the BlockStructure objects to keep track of the matrix
// structure and operate upon it. This allows us to use more cache
// friendly block oriented linear algebra operations on the matrix
// instead of accessing it one scalar entry at a time.

#ifndef CERES_INTERNAL_BLOCK_STRUCTURE_H_
#define CERES_INTERNAL_BLOCK_STRUCTURE_H_

#include <cstdint>
#include <vector>

#include "ceres/internal/export.h"

// This file is being included into source files that are compiled with nvcc.
// nvcc shipped with ubuntu 20.04 does not support some features of c++17,
// including nested namespace definitions
namespace ceres {
namespace internal {

using BlockSize = int32_t;

struct CERES_NO_EXPORT Block {
  Block() = default;
  Block(int size_, int position_) noexcept : size(size_), position(position_) {}

  BlockSize size{-1};
  int position{-1};  // Position along the row/column.
};

inline bool operator==(const Block& left, const Block& right) noexcept {
  return (left.size == right.size) && (left.position == right.position);
}

struct CERES_NO_EXPORT Cell {
  Cell() = default;
  Cell(int block_id_, int position_) noexcept
      : block_id(block_id_), position(position_) {}

  // Column or row block id as the case maybe.
  int block_id{-1};
  // Where in the values array of the jacobian is this cell located.
  int position{-1};
};

// Order cell by their block_id;
CERES_NO_EXPORT bool CellLessThan(const Cell& lhs, const Cell& rhs);

struct CERES_NO_EXPORT CompressedList {
  CompressedList() = default;

  // Construct a CompressedList with the cells containing num_cells
  // entries.
  explicit CompressedList(int num_cells) noexcept : cells(num_cells) {}
  Block block;
  std::vector<Cell> cells;
  // Number of non-zeros in cells of this row block
  int nnz{-1};
  // Number of non-zeros in cells of this and every preceeding row block in
  // block-sparse matrix
  int cumulative_nnz{-1};
};

using CompressedRow = CompressedList;
using CompressedColumn = CompressedList;

// CompressedRowBlockStructure specifies the storage structure of a row block
// sparse matrix.
//
// Consider the following matrix A:
// A = [A_11 A_12 ...
//      A_21 A_22 ...
//      ...
//      A_m1 A_m2 ... ]
//
// A row block sparse matrix is a matrix where the following properties hold:
// 1. The number of rows in every block A_ij and A_ik are the same.
// 2. The number of columns in every block A_ij and A_kj are the same.
// 3. The number of rows in A_ij and A_kj may be different (i != k).
// 4. The number of columns in A_ij and A_ik may be different (j != k).
// 5. Any block A_ij may be all 0s, in which case the block is not stored.
//
// The structure of the matrix is stored as follows:
//
// The `rows' array contains the following information for each row block:
// - rows[i].block.size: The number of rows in each block A_ij in the row block.
// - rows[i].block.position: The starting row in the full matrix A of the
//       row block i.
// - rows[i].cells[j].block_id: The index into the `cols' array corresponding to
//       the non-zero blocks A_ij.
// - rows[i].cells[j].position: The index in the `values' array for the contents
//       of block A_ij.
//
// The `cols' array contains the following information for block:
// - cols[.].size: The number of columns spanned by the block.
// - cols[.].position: The starting column in the full matrix A of the block.
//
//
// Example of a row block sparse matrix:
// block_id: | 0  |1|2  |3 |
// rows[0]:  [ 1 2 0 3 4 0 ]
//           [ 5 6 0 7 8 0 ]
// rows[1]:  [ 0 0 9 0 0 0 ]
//
// This matrix is stored as follows:
//
// There are four column blocks:
// cols[0].size = 2
// cols[0].position = 0
// cols[1].size = 1
// cols[1].position = 2
// cols[2].size = 2
// cols[2].position = 3
// cols[3].size = 1
// cols[3].position = 5

// The first row block spans two rows, starting at row 0:
// rows[0].block.size = 2          // This row block spans two rows.
// rows[0].block.position = 0      // It starts at row 0.
// rows[0] has two cells, at column blocks 0 and 2:
// rows[0].cells[0].block_id = 0   // This cell is in column block 0.
// rows[0].cells[0].position = 0   // See below for an explanation of this.
// rows[0].cells[1].block_id = 2   // This cell is in column block 2.
// rows[0].cells[1].position = 4   // See below for an explanation of this.
//
// The second row block spans two rows, starting at row 2:
// rows[1].block.size = 1          // This row block spans one row.
// rows[1].block.position = 2      // It starts at row 2.
// rows[1] has one cell at column block 1:
// rows[1].cells[0].block_id = 1   // This cell is in column block 1.
// rows[1].cells[0].position = 8   // See below for an explanation of this.
//
// The values in each blocks are stored contiguously in row major order.
// However, there is no unique way to order the blocks -- it is usually
// optimized to promote cache coherent access, e.g. ordering it so that
// Jacobian blocks of parameters of the same type are stored nearby.
// This is one possible way to store the values of the blocks in a values array:
// values = { 1, 2, 5, 6, 3, 4, 7, 8, 9 }
//           |           |          |   |    // The three blocks.
//            ^ rows[0].cells[0].position = 0
//                        ^ rows[0].cells[1].position = 4
//                                    ^ rows[1].cells[0].position = 8
struct CERES_NO_EXPORT CompressedRowBlockStructure {
  std::vector<Block> cols;
  std::vector<CompressedRow> rows;
};

struct CERES_NO_EXPORT CompressedColumnBlockStructure {
  std::vector<Block> rows;
  std::vector<CompressedColumn> cols;
};

inline int NumScalarEntries(const std::vector<Block>& blocks) {
  if (blocks.empty()) {
    return 0;
  }

  auto& block = blocks.back();
  return block.position + block.size;
}

std::vector<Block> Tail(const std::vector<Block>& blocks, int n);
int SumSquaredSizes(const std::vector<Block>& blocks);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_STRUCTURE_H_
