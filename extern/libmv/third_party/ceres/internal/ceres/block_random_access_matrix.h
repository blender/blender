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
//
// Interface for matrices that allow block based random access.

#ifndef CERES_INTERNAL_BLOCK_RANDOM_ACCESS_MATRIX_H_
#define CERES_INTERNAL_BLOCK_RANDOM_ACCESS_MATRIX_H_

#include "ceres/mutex.h"

namespace ceres {
namespace internal {

// A matrix implementing the BlockRandomAccessMatrix interface is a
// matrix whose rows and columns are divided into blocks. For example
// the matrix A:
//
//            3     4      5
//  A =  5 [c_11  c_12  c_13]
//       4 [c_21  c_22  c_23]
//
// has row blocks of size 5 and 4, and column blocks of size 3, 4 and
// 5. It has six cells corresponding to the six row-column block
// combinations.
//
// BlockRandomAccessMatrix objects provide access to cells c_ij using
// the GetCell method. when a cell is present, GetCell will return a
// CellInfo object containing a pointer to an array which contains the
// cell as a submatrix and a mutex that guards this submatrix. If the
// user is accessing the matrix concurrently, it is his responsibility
// to use the mutex to exclude other writers from writing to the cell
// concurrently.
//
// There is no requirement that all cells be present, i.e. the matrix
// itself can be block sparse. When a cell is not present, the GetCell
// method will return a NULL pointer.
//
// There is no requirement about how the cells are stored beyond that
// form a dense submatrix of a larger dense matrix. Like everywhere
// else in Ceres, RowMajor storage assumed.
//
// Example usage:
//
//  BlockRandomAccessMatrix* A = new BlockRandomAccessMatrixSubClass(...)
//
//  int row, col, row_stride, col_stride;
//  CellInfo* cell = A->GetCell(row_block_id, col_block_id,
//                              &row, &col,
//                              &row_stride, &col_stride);
//
//  if (cell != NULL) {
//     MatrixRef m(cell->values, row_stride, col_stride);
//     CeresMutexLock l(&cell->m);
//     m.block(row, col, row_block_size, col_block_size) = ...
//  }

// Structure to carry a pointer to the array containing a cell and the
// Mutex guarding it.
struct CellInfo {
  CellInfo()
      : values(NULL) {
  }

  explicit CellInfo(double* ptr)
      : values(ptr) {
  }

  double* values;
  Mutex m;
};

class BlockRandomAccessMatrix {
 public:
  virtual ~BlockRandomAccessMatrix();

  // If the cell (row_block_id, col_block_id) is present, then return
  // a CellInfo with a pointer to the dense matrix containing it,
  // otherwise return NULL. The dense matrix containing this cell has
  // size row_stride, col_stride and the cell is located at position
  // (row, col) within this matrix.
  //
  // The size of the cell is row_block_size x col_block_size is
  // assumed known to the caller. row_block_size less than or equal to
  // row_stride and col_block_size is upper bounded by col_stride.
  virtual CellInfo* GetCell(int row_block_id,
                            int col_block_id,
                            int* row,
                            int* col,
                            int* row_stride,
                            int* col_stride) = 0;

  // Zero out the values of the array. The structure of the matrix
  // (size and sparsity) is preserved.
  virtual void SetZero() = 0;

  // Number of scalar rows and columns in the matrix, i.e the sum of
  // all row blocks and column block sizes respectively.
  virtual int num_rows() const = 0;
  virtual int num_cols() const = 0;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_RANDOM_ACCESS_MATRIX_H_
