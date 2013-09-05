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

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_CRS_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_CRS_MATRIX_H_

#include <set>
#include <vector>
#include <utility>
#include "ceres/mutex.h"
#include "ceres/block_random_access_matrix.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/collections_port.h"
#include "ceres/integral_types.h"
#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

// A square BlockRandomAccessMatrix where the underlying storage is a
// compressed row sparse matrix. The matrix need not be symmetric.
class BlockRandomAccessCRSMatrix : public BlockRandomAccessMatrix {
 public:
  // blocks is an array of block sizes. block_pairs is a set of
  // <row_block_id, col_block_id> pairs to identify the non-zero cells
  // of this matrix.
  BlockRandomAccessCRSMatrix(const vector<int>& blocks,
                             const set<pair<int, int> >& block_pairs);

  // The destructor is not thread safe. It assumes that no one is
  // modifying any cells when the matrix is being destroyed.
  virtual ~BlockRandomAccessCRSMatrix();

  // BlockRandomAccessMatrix Interface.
  virtual CellInfo* GetCell(int row_block_id,
                            int col_block_id,
                            int* row,
                            int* col,
                            int* row_stride,
                            int* col_stride);

  // This is not a thread safe method, it assumes that no cell is
  // locked.
  virtual void SetZero();

  // Since the matrix is square, num_rows() == num_cols().
  virtual int num_rows() const { return crsm_->num_rows(); }
  virtual int num_cols() const { return crsm_->num_cols(); }

    // Access to the underlying matrix object.
  const CompressedRowSparseMatrix* matrix() const { return crsm_.get(); }
  CompressedRowSparseMatrix* mutable_matrix() { return crsm_.get(); }

 private:
  int64 IntPairToLong(int a, int b) {
    return a * kMaxRowBlocks + b;
  }

  const int64 kMaxRowBlocks;
  // row/column block sizes.
  const vector<int> blocks_;
  vector<int> col_layout_;
  vector<int> row_strides_;

  // A mapping from <row_block_id, col_block_id> to the position in
  // the values array of tsm_ where the block is stored.
  typedef HashMap<long int, CellInfo* > LayoutType;
  LayoutType layout_;

  scoped_ptr<CompressedRowSparseMatrix> crsm_;
  friend class BlockRandomAccessCRSMatrixTest;
  CERES_DISALLOW_COPY_AND_ASSIGN(BlockRandomAccessCRSMatrix);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_CRS_MATRIX_H_
