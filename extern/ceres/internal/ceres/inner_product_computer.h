// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_INNER_PRODUCT_COMPUTER_H_
#define CERES_INTERNAL_INNER_PRODUCT_COMPUTER_H_

#include <memory>
#include <vector>

#include "ceres/block_sparse_matrix.h"
#include "ceres/compressed_row_sparse_matrix.h"

namespace ceres {
namespace internal {

// This class is used to repeatedly compute the inner product
//
//   result = m' * m
//
// where the sparsity structure of m remains constant across calls.
//
// Upon creation, the class computes and caches information needed to
// compute v, and then uses it to efficiently compute the product
// every time InnerProductComputer::Compute is called.
//
// See sparse_normal_cholesky_solver.cc for example usage.
//
// Note that the result matrix is a block upper or lower-triangular
// matrix, i.e., it will contain entries in the upper or lower
// triangular part of the matrix corresponding to the block that occur
// along its diagonal.
//
// This is not a problem as sparse linear algebra libraries can ignore
// these entries with ease and the space used is minimal/linear in the
// size of the matrices.
class InnerProductComputer {
 public:
  // Factory
  //
  // m is the input matrix
  //
  // Since m' * m is a symmetric matrix, we only compute half of the
  // matrix and the value of storage_type which must be
  // UPPER_TRIANGULAR or LOWER_TRIANGULAR determines which half is
  // computed.
  //
  // The user must ensure that the matrix m is valid for the life time
  // of this object.
  static InnerProductComputer* Create(
      const BlockSparseMatrix& m,
      CompressedRowSparseMatrix::StorageType storage_type);

  // This factory method allows the user control over range of row
  // blocks of m that should be used to compute the inner product.
  //
  // a = m(start_row_block : end_row_block, :);
  // result = a' * a;
  static InnerProductComputer* Create(
      const BlockSparseMatrix& m,
      int start_row_block,
      int end_row_block,
      CompressedRowSparseMatrix::StorageType storage_type);

  // Update result_ to be numerically equal to m' * m.
  void Compute();

  // Accessors for the result containing the inner product.
  //
  // Compute must be called before accessing this result for
  // the first time.
  const CompressedRowSparseMatrix& result() const { return *result_; }
  CompressedRowSparseMatrix* mutable_result() const { return result_.get(); }

 private:
  // A ProductTerm is a term in the block inner product of a matrix
  // with itself.
  struct ProductTerm {
    ProductTerm(const int row, const int col, const int index)
        : row(row), col(col), index(index) {}

    bool operator<(const ProductTerm& right) const {
      if (row == right.row) {
        if (col == right.col) {
          return index < right.index;
        }
        return col < right.col;
      }
      return row < right.row;
    }

    int row;
    int col;
    int index;
  };

  InnerProductComputer(const BlockSparseMatrix& m,
                       int start_row_block,
                       int end_row_block);

  void Init(CompressedRowSparseMatrix::StorageType storage_type);

  CompressedRowSparseMatrix* CreateResultMatrix(
      const CompressedRowSparseMatrix::StorageType storage_type,
      int num_nonzeros);

  int ComputeNonzeros(const std::vector<ProductTerm>& product_terms,
                      std::vector<int>* row_block_nnz);

  void ComputeOffsetsAndCreateResultMatrix(
      const CompressedRowSparseMatrix::StorageType storage_type,
      const std::vector<ProductTerm>& product_terms);

  const BlockSparseMatrix& m_;
  const int start_row_block_;
  const int end_row_block_;
  std::unique_ptr<CompressedRowSparseMatrix> result_;

  // For each term in the inner product, result_offsets_ contains the
  // location in the values array of the result_ matrix where it
  // should be stored.
  //
  // This is the principal look up table that allows this class to
  // compute the inner product fast.
  std::vector<int> result_offsets_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_INNER_PRODUCT_COMPUTER_H_
