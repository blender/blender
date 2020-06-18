// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2015 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_TRIPLET_SPARSE_MATRIX_H_
#define CERES_INTERNAL_TRIPLET_SPARSE_MATRIX_H_

#include <memory>
#include <vector>
#include "ceres/sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

// An implementation of the SparseMatrix interface to store and
// manipulate sparse matrices in triplet (i,j,s) form.  This object is
// inspired by the design of the cholmod_triplet struct used in the
// SuiteSparse package and is memory layout compatible with it.
class TripletSparseMatrix : public SparseMatrix {
 public:
  TripletSparseMatrix();
  TripletSparseMatrix(int num_rows, int num_cols, int max_num_nonzeros);
  TripletSparseMatrix(int num_rows,
                      int num_cols,
                      const std::vector<int>& rows,
                      const std::vector<int>& cols,
                      const std::vector<double>& values);

  explicit TripletSparseMatrix(const TripletSparseMatrix& orig);

  TripletSparseMatrix& operator=(const TripletSparseMatrix& rhs);

  virtual ~TripletSparseMatrix();

  // Implementation of the SparseMatrix interface.
  void SetZero() final;
  void RightMultiply(const double* x, double* y) const final;
  void LeftMultiply(const double* x, double* y) const final;
  void SquaredColumnNorm(double* x) const final;
  void ScaleColumns(const double* scale) final;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;
  int num_rows()        const final   { return num_rows_;     }
  int num_cols()        const final   { return num_cols_;     }
  int num_nonzeros()    const final   { return num_nonzeros_; }
  const double* values()  const final { return values_.get(); }
  double* mutable_values() final      { return values_.get(); }
  void set_num_nonzeros(int num_nonzeros);

  // Increase max_num_nonzeros and correspondingly increase the size
  // of rows_, cols_ and values_. If new_max_num_nonzeros is smaller
  // than max_num_nonzeros_, then num_non_zeros should be less than or
  // equal to new_max_num_nonzeros, otherwise data loss is possible
  // and the method crashes.
  void Reserve(int new_max_num_nonzeros);

  // Append the matrix B at the bottom of this matrix. B should have
  // the same number of columns as num_cols_.
  void AppendRows(const TripletSparseMatrix& B);

  // Append the matrix B at the right of this matrix. B should have
  // the same number of rows as num_rows_;
  void AppendCols(const TripletSparseMatrix& B);

  // Resize the matrix. Entries which fall outside the new matrix
  // bounds are dropped and the num_non_zeros changed accordingly.
  void Resize(int new_num_rows, int new_num_cols);

  int max_num_nonzeros() const { return max_num_nonzeros_; }
  const int* rows()      const { return rows_.get();       }
  const int* cols()      const { return cols_.get();       }
  int* mutable_rows()          { return rows_.get();       }
  int* mutable_cols()          { return cols_.get();       }

  // Returns true if the entries of the matrix obey the row, column,
  // and column size bounds and false otherwise.
  bool AllTripletsWithinBounds() const;

  bool IsValid() const { return AllTripletsWithinBounds(); }

  // Build a sparse diagonal matrix of size num_rows x num_rows from
  // the array values. Entries of the values array are copied into the
  // sparse matrix.
  static TripletSparseMatrix* CreateSparseDiagonalMatrix(const double* values,
                                                         int num_rows);

  // Options struct to control the generation of random
  // TripletSparseMatrix objects.
  struct RandomMatrixOptions {
    int num_rows;
    int num_cols;
    // 0 < density <= 1 is the probability of an entry being
    // structurally non-zero. A given random matrix will not have
    // precisely this density.
    double density;
  };

  // Create a random CompressedRowSparseMatrix whose entries are
  // normally distributed and whose structure is determined by
  // RandomMatrixOptions.
  //
  // Caller owns the result.
  static TripletSparseMatrix* CreateRandomMatrix(
      const TripletSparseMatrix::RandomMatrixOptions& options);

 private:
  void AllocateMemory();
  void CopyData(const TripletSparseMatrix& orig);

  int num_rows_;
  int num_cols_;
  int max_num_nonzeros_;
  int num_nonzeros_;

  // The data is stored as three arrays. For each i, values_[i] is
  // stored at the location (rows_[i], cols_[i]). If the there are
  // multiple entries with the same (rows_[i], cols_[i]), the values_
  // entries corresponding to them are summed up.
  std::unique_ptr<int[]> rows_;
  std::unique_ptr<int[]> cols_;
  std::unique_ptr<double[]> values_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_TRIPLET_SPARSE_MATRIX_H__
