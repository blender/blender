// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "ceres/block_random_access_sparse_matrix.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/mutex.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

BlockRandomAccessSparseMatrix::BlockRandomAccessSparseMatrix(
    const vector<int>& blocks,
    const set<pair<int, int> >& block_pairs)
    : kMaxRowBlocks(10 * 1000 * 1000),
      blocks_(blocks) {
  CHECK_LT(blocks.size(), kMaxRowBlocks);

  // Build the row/column layout vector and count the number of scalar
  // rows/columns.
  int num_cols = 0;
  block_positions_.reserve(blocks_.size());
  for (int i = 0; i < blocks_.size(); ++i) {
    block_positions_.push_back(num_cols);
    num_cols += blocks_[i];
  }

  // Count the number of scalar non-zero entries and build the layout
  // object for looking into the values array of the
  // TripletSparseMatrix.
  int num_nonzeros = 0;
  for (set<pair<int, int> >::const_iterator it = block_pairs.begin();
       it != block_pairs.end();
       ++it) {
    const int row_block_size = blocks_[it->first];
    const int col_block_size = blocks_[it->second];
    num_nonzeros += row_block_size * col_block_size;
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
  for (set<pair<int, int> >::const_iterator it = block_pairs.begin();
       it != block_pairs.end();
       ++it) {
    const int row_block_size = blocks_[it->first];
    const int col_block_size = blocks_[it->second];
    cell_values_.push_back(make_pair(make_pair(it->first, it->second),
                                     values + pos));
    layout_[IntPairToLong(it->first, it->second)] =
        new CellInfo(values + pos);
    pos += row_block_size * col_block_size;
  }

  // Fill the sparsity pattern of the underlying matrix.
  for (set<pair<int, int> >::const_iterator it = block_pairs.begin();
       it != block_pairs.end();
       ++it) {
    const int row_block_id = it->first;
    const int col_block_id = it->second;
    const int row_block_size = blocks_[row_block_id];
    const int col_block_size = blocks_[col_block_id];
    int pos =
        layout_[IntPairToLong(row_block_id, col_block_id)]->values - values;
    for (int r = 0; r < row_block_size; ++r) {
      for (int c = 0; c < col_block_size; ++c, ++pos) {
          rows[pos] = block_positions_[row_block_id] + r;
          cols[pos] = block_positions_[col_block_id] + c;
          values[pos] = 1.0;
          DCHECK_LT(rows[pos], tsm_->num_rows());
          DCHECK_LT(cols[pos], tsm_->num_rows());
      }
    }
  }
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
BlockRandomAccessSparseMatrix::~BlockRandomAccessSparseMatrix() {
  for (LayoutType::iterator it = layout_.begin();
       it != layout_.end();
       ++it) {
    delete it->second;
  }
}

CellInfo* BlockRandomAccessSparseMatrix::GetCell(int row_block_id,
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

  // Each cell is stored contiguously as its own little dense matrix.
  *row = 0;
  *col = 0;
  *row_stride = blocks_[row_block_id];
  *col_stride = blocks_[col_block_id];
  return it->second;
}

// Assume that the user does not hold any locks on any cell blocks
// when they are calling SetZero.
void BlockRandomAccessSparseMatrix::SetZero() {
  if (tsm_->num_nonzeros()) {
    VectorRef(tsm_->mutable_values(),
              tsm_->num_nonzeros()).setZero();
  }
}

void BlockRandomAccessSparseMatrix::SymmetricRightMultiply(const double* x,
                                                           double* y) const {
  vector< pair<pair<int, int>, double*> >::const_iterator it =
      cell_values_.begin();
  for (; it != cell_values_.end(); ++it) {
    const int row = it->first.first;
    const int row_block_size = blocks_[row];
    const int row_block_pos = block_positions_[row];

    const int col = it->first.second;
    const int col_block_size = blocks_[col];
    const int col_block_pos = block_positions_[col];

    MatrixVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
        it->second, row_block_size, col_block_size,
        x + col_block_pos,
        y + row_block_pos);

    // Since the matrix is symmetric, but only the upper triangular
    // part is stored, if the block being accessed is not a diagonal
    // block, then use the same block to do the corresponding lower
    // triangular multiply also.
    if (row != col) {
      MatrixTransposeVectorMultiply<Eigen::Dynamic, Eigen::Dynamic, 1>(
          it->second, row_block_size, col_block_size,
          x + row_block_pos,
          y + col_block_pos);
    }
  }
}

}  // namespace internal
}  // namespace ceres
