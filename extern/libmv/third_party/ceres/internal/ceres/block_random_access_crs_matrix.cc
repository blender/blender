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

#include "ceres/block_random_access_crs_matrix.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/mutex.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

BlockRandomAccessCRSMatrix::BlockRandomAccessCRSMatrix(
    const vector<int>& blocks,
    const set<pair<int, int> >& block_pairs)
    : kMaxRowBlocks(10 * 1000 * 1000),
      blocks_(blocks) {
  CHECK_LT(blocks.size(), kMaxRowBlocks);

  col_layout_.resize(blocks_.size(), 0);
  row_strides_.resize(blocks_.size(), 0);

  // Build the row/column layout vector and count the number of scalar
  // rows/columns.
  int num_cols = 0;
  for (int i = 0; i < blocks_.size(); ++i) {
    col_layout_[i] = num_cols;
    num_cols += blocks_[i];
  }

  // Walk the sparsity pattern and count the number of non-zeros.
  int num_nonzeros = 0;
  for (set<pair<int, int> >::const_iterator it = block_pairs.begin();
       it != block_pairs.end();
       ++it) {
    const int row_block_size = blocks_[it->first];
    const int col_block_size = blocks_[it->second];
    num_nonzeros += row_block_size * col_block_size;
  }

  VLOG(2) << "Matrix Size [" << num_cols
          << "," << num_cols
          << "] " << num_nonzeros;

  crsm_.reset(new CompressedRowSparseMatrix(num_cols, num_cols, num_nonzeros));
  int* rows = crsm_->mutable_rows();
  int* cols = crsm_->mutable_cols();
  double* values = crsm_->mutable_values();

  // Iterate over the sparsity pattern and fill the scalar sparsity
  // pattern of the underlying compressed sparse row matrix. Along the
  // way also fill out the Layout object which will allow random
  // access into the CRS Matrix.
  set<pair<int, int> >::const_iterator it = block_pairs.begin();
  vector<int> col_blocks;
  int row_pos = 0;
  rows[0] = 0;
  while (it != block_pairs.end()) {
    // Add entries to layout_ for all the blocks for this row.
    col_blocks.clear();
    const int row_block_id = it->first;
    const int row_block_size = blocks_[row_block_id];
    int num_cols = 0;
    while ((it != block_pairs.end()) && (it->first == row_block_id)) {
      layout_[IntPairToLong(it->first, it->second)] =
          new CellInfo(values + num_cols);
      col_blocks.push_back(it->second);
      num_cols += blocks_[it->second];
      ++it;
    };

    // Count the number of non-zeros in the row block.
    for (int j = 0; j < row_block_size; ++j) {
      rows[row_pos + j + 1] = rows[row_pos + j] + num_cols;
    }

    // Fill out the sparsity pattern for each row.
    int col_pos = 0;
    for (int j = 0; j < col_blocks.size(); ++j) {
      const int col_block_id = col_blocks[j];
      const int col_block_size = blocks_[col_block_id];
      for (int r = 0; r < row_block_size; ++r) {
        const int column_block_begin = rows[row_pos + r] + col_pos;
        for (int c = 0; c < col_block_size; ++c) {
          cols[column_block_begin + c] = col_layout_[col_block_id] + c;
        }
      }
      col_pos += col_block_size;
    }

    row_pos += row_block_size;
    values += row_block_size * num_cols;
    row_strides_[row_block_id] = num_cols;
  }
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
BlockRandomAccessCRSMatrix::~BlockRandomAccessCRSMatrix() {
  // TODO(sameeragarwal) this should be rationalized going forward and
  // perhaps moved into BlockRandomAccessMatrix.
  for (LayoutType::iterator it = layout_.begin();
       it != layout_.end();
       ++it) {
    delete it->second;
  }
}

CellInfo* BlockRandomAccessCRSMatrix::GetCell(int row_block_id,
                                              int col_block_id,
                                              int* row,
                                              int* col,
                                              int* row_stride,
                                              int* col_stride) {
  const LayoutType::iterator it  =
      layout_.find(IntPairToLong(row_block_id, col_block_id));
  if (it == layout_.end()) {
    return NULL;
  }

  *row = 0;
  *col = 0;
  *row_stride = blocks_[row_block_id];
  *col_stride = row_strides_[row_block_id];
  return it->second;
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessCRSMatrix::SetZero() {
  crsm_->SetZero();
}

}  // namespace internal
}  // namespace ceres
