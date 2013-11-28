// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
// http://code.google.com/p/ceres-solver/
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

#include "ceres/block_random_access_diagonal_matrix.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/stl_util.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

BlockRandomAccessDiagonalMatrix::BlockRandomAccessDiagonalMatrix(
    const vector<int>& blocks)
    : blocks_(blocks) {
  // Build the row/column layout vector and count the number of scalar
  // rows/columns.
  int num_cols = 0;
  int num_nonzeros = 0;
  vector<int> col_layout;
  for (int i = 0; i < blocks_.size(); ++i) {
    col_layout.push_back(num_cols);
    num_cols += blocks_[i];
    num_nonzeros += blocks_[i] * blocks_[i];
  }

  VLOG(1) << "Matrix Size [" << num_cols
          << "," << num_cols
          << "] " << num_nonzeros;

  tsm_.reset(new TripletSparseMatrix(num_cols, num_cols, num_nonzeros));
  tsm_->set_num_nonzeros(num_nonzeros);
  int* rows = tsm_->mutable_rows();
  int* cols = tsm_->mutable_cols();
  double* values = tsm_->mutable_values();

  int pos = 0;
  for (int i = 0; i < blocks_.size(); ++i) {
    const int block_size = blocks_[i];
    layout_.push_back(new CellInfo(values + pos));
    const int block_begin = col_layout[i];
    for (int r = 0; r < block_size; ++r) {
      for (int c = 0; c < block_size; ++c, ++pos) {
        rows[pos] = block_begin + r;
        cols[pos] = block_begin + c;
      }
    }
  }
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
BlockRandomAccessDiagonalMatrix::~BlockRandomAccessDiagonalMatrix() {
  STLDeleteContainerPointers(layout_.begin(), layout_.end());
}

CellInfo* BlockRandomAccessDiagonalMatrix::GetCell(int row_block_id,
                                                   int col_block_id,
                                                   int* row,
                                                   int* col,
                                                   int* row_stride,
                                                   int* col_stride) {
  if (row_block_id != col_block_id) {
    return NULL;
  }
  const int stride = blocks_[row_block_id];

  // Each cell is stored contiguously as its own little dense matrix.
  *row = 0;
  *col = 0;
  *row_stride = stride;
  *col_stride = stride;
  return layout_[row_block_id];
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessDiagonalMatrix::SetZero() {
  if (tsm_->num_nonzeros()) {
    VectorRef(tsm_->mutable_values(),
              tsm_->num_nonzeros()).setZero();
  }
}

}  // namespace internal
}  // namespace ceres
