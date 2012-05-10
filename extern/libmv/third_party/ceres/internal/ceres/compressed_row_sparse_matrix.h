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

#ifndef CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_
#define CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_

#include <glog/logging.h>
#include "ceres/sparse_matrix.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/macros.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

class SparseMatrixProto;

class CompressedRowSparseMatrix : public SparseMatrix {
 public:
  // Build a matrix with the same content as the TripletSparseMatrix
  // m. TripletSparseMatrix objects are easier to construct
  // incrementally, so we use them to initialize SparseMatrix
  // objects.
  //
  // We assume that m does not have any repeated entries.
  explicit CompressedRowSparseMatrix(const TripletSparseMatrix& m);
#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
  explicit CompressedRowSparseMatrix(const SparseMatrixProto& proto);
#endif

  // Use this constructor only if you know what you are doing. This
  // creates a "blank" matrix with the appropriate amount of memory
  // allocated. However, the object itself is in an inconsistent state
  // as the rows and cols matrices do not match the values of
  // num_rows, num_cols and max_num_nonzeros.
  //
  // The use case for this constructor is that when the user knows the
  // size of the matrix to begin with and wants to update the layout
  // manually, instead of going via the indirect route of first
  // constructing a TripletSparseMatrix, which leads to more than
  // double the peak memory usage.
  CompressedRowSparseMatrix(int num_rows,
                            int num_cols,
                            int max_num_nonzeros);

  // Build a square sparse diagonal matrix with num_rows rows and
  // columns. The diagonal m(i,i) = diagonal(i);
  CompressedRowSparseMatrix(const double* diagonal, int num_rows);

  virtual ~CompressedRowSparseMatrix();

  // SparseMatrix interface.
  virtual void SetZero();
  virtual void RightMultiply(const double* x, double* y) const;
  virtual void LeftMultiply(const double* x, double* y) const;
  virtual void SquaredColumnNorm(double* x) const;
  virtual void ScaleColumns(const double* scale);

  virtual void ToDenseMatrix(Matrix* dense_matrix) const;
#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
  virtual void ToProto(SparseMatrixProto* proto) const;
#endif
  virtual void ToTextFile(FILE* file) const;
  virtual int num_rows() const { return num_rows_; }
  virtual int num_cols() const { return num_cols_; }
  virtual int num_nonzeros() const { return rows_[num_rows_]; }
  virtual const double* values() const { return values_.get(); }
  virtual double* mutable_values() { return values_.get(); }

  // Delete the bottom delta_rows.
  // num_rows -= delta_rows
  void DeleteRows(int delta_rows);

  // Append the contents of m to the bottom of this matrix. m must
  // have the same number of columns as this matrix.
  void AppendRows(const CompressedRowSparseMatrix& m);

  // Low level access methods that expose the structure of the matrix.
  const int* cols() const { return cols_.get(); }
  int* mutable_cols() { return cols_.get(); }

  const int* rows() const { return rows_.get(); }
  int* mutable_rows() { return rows_.get(); }

 private:
  scoped_array<int> cols_;
  scoped_array<int> rows_;
  scoped_array<double> values_;

  int num_rows_;
  int num_cols_;

  int max_num_nonzeros_;

  DISALLOW_COPY_AND_ASSIGN(CompressedRowSparseMatrix);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_
