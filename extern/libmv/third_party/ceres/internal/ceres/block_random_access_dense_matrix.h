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

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DENSE_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DENSE_MATRIX_H_

#include "ceres/block_random_access_matrix.h"

#include <vector>

#include "ceres/internal/macros.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"

namespace ceres {
namespace internal {

// A square block random accessible matrix with the same row and
// column block structure. All cells are stored in the same single
// array, so that its also accessible as a dense matrix of size
// num_rows x num_cols.
//
// This class is NOT thread safe. Since all n^2 cells are stored,
// GetCell never returns NULL for any (row_block_id, col_block_id)
// pair.
//
// ReturnCell is a nop.
class BlockRandomAccessDenseMatrix : public BlockRandomAccessMatrix {
 public:
  // blocks is a vector of block sizes. The resulting matrix has
  // blocks.size() * blocks.size() cells.
  explicit BlockRandomAccessDenseMatrix(const vector<int>& blocks);

  // The destructor is not thread safe. It assumes that no one is
  // modifying any cells when the matrix is being destroyed.
  virtual ~BlockRandomAccessDenseMatrix();

  // BlockRandomAccessMatrix interface.
  virtual CellInfo* GetCell(int row_block_id,
                            int col_block_id,
                            int* row,
                            int* col,
                            int* row_stride,
                            int* col_stride);

  // This is not a thread safe method, it assumes that no cell is
  // locked.
  virtual void SetZero();

  // Since the matrix is square with the same row and column block
  // structure, num_rows() = num_cols().
  virtual int num_rows() const { return num_rows_; }
  virtual int num_cols() const { return num_rows_; }

  // The underlying matrix storing the cells.
  const double* values() const { return values_.get(); }
  double* mutable_values() { return values_.get(); }

 private:
  CellInfo cell_info_;
  int num_rows_;
  vector<int> block_layout_;
  scoped_array<double> values_;

  DISALLOW_COPY_AND_ASSIGN(BlockRandomAccessDenseMatrix);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_DENSE_MATRIX_H_
