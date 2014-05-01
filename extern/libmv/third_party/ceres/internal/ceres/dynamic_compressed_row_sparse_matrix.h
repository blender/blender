// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2014 Google Inc. All rights reserved.
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
// Author: richie.stebbing@gmail.com (Richard Stebbing)
//
// A compressed row sparse matrix that provides an extended interface to
// allow dynamic insertion of entries. This is provided for the use case
// where the sparsity structure and number of non-zero entries is dynamic.
// This flexibility is achieved by using an (internal) scratch space that
// allows independent insertion of entries into each row (thread-safe).
// Once insertion is complete, the `Finalize` method must be called to ensure
// that the underlying `CompressedRowSparseMatrix` is consistent.
//
// This should only be used if you really do need a dynamic sparsity pattern.

#ifndef CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_SPARSE_MATRIX_H_
#define CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_SPARSE_MATRIX_H_

#include "ceres/compressed_row_sparse_matrix.h"

namespace ceres {
namespace internal {

class DynamicCompressedRowSparseMatrix : public CompressedRowSparseMatrix {
 public:
  // Set the number of rows and columns for the underlyig
  // `CompressedRowSparseMatrix` and set the initial number of maximum non-zero
  // entries. Note that following the insertion of entries, when `Finalize`
  // is called the number of non-zeros is determined and all internal
  // structures are adjusted as required. If you know the upper limit on the
  // number of non-zeros, then passing this value here can prevent future
  // memory reallocations which may improve performance. Otherwise, if no
  // upper limit is available a value of 0 is sufficient.
  //
  // Typical usage of this class is to define a new instance with a given
  // number of rows, columns and maximum number of non-zero elements
  // (if available). Next, entries are inserted at row and column positions
  // using `InsertEntry`. Finally, once all elements have been inserted,
  // `Finalize` must be called to make the underlying
  // `CompressedRowSparseMatrix` consistent.
  DynamicCompressedRowSparseMatrix(int num_rows,
                                   int num_cols,
                                   int initial_max_num_nonzeros);

  // Insert an entry at a given row and column position. This method is
  // thread-safe across rows i.e. different threads can insert values
  // simultaneously into different rows. It should be emphasised that this
  // method always inserts a new entry and does not check for existing
  // entries at the specified row and column position. Duplicate entries
  // for a given row and column position will result in undefined
  // behavior.
  void InsertEntry(int row, int col, const double& value);

  // Clear all entries for rows, starting from row index `row_start`
  // and proceeding for `num_rows`.
  void ClearRows(int row_start, int num_rows);

  // Make the underlying internal `CompressedRowSparseMatrix` data structures
  // consistent. Additional space for non-zero entries in the
  // `CompressedRowSparseMatrix` can be reserved by specifying
  // `num_additional_elements`. This is useful when it is known that rows will
  // be appended to the `CompressedRowSparseMatrix` (e.g. appending a diagonal
  // matrix to the jacobian) as it prevents need for future reallocation.
  void Finalize(int num_additional_elements);

 private:
  vector<vector<int> > dynamic_cols_;
  vector<vector<double> > dynamic_values_;
};

}  // namespace internal
}  // namespace ceres

#endif // CERES_INTERNAL_DYNAMIC_COMPRESSED_ROW_SPARSE_MATRIX_H_
