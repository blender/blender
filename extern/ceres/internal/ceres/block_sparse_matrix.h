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
//
// Implementation of the SparseMatrix interface for block sparse
// matrices.

#ifndef CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_
#define CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_

#include <memory>
#include "ceres/block_structure.h"
#include "ceres/sparse_matrix.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

class TripletSparseMatrix;

// This class implements the SparseMatrix interface for storing and
// manipulating block sparse matrices. The block structure is stored
// in the CompressedRowBlockStructure object and one is needed to
// initialize the matrix. For details on how the blocks structure of
// the matrix is stored please see the documentation
//
//   internal/ceres/block_structure.h
//
class BlockSparseMatrix : public SparseMatrix {
 public:
  // Construct a block sparse matrix with a fully initialized
  // CompressedRowBlockStructure objected. The matrix takes over
  // ownership of this object and destroys it upon destruction.
  //
  // TODO(sameeragarwal): Add a function which will validate legal
  // CompressedRowBlockStructure objects.
  explicit BlockSparseMatrix(CompressedRowBlockStructure* block_structure);

  BlockSparseMatrix();
  BlockSparseMatrix(const BlockSparseMatrix&) = delete;
  void operator=(const BlockSparseMatrix&) = delete;

  virtual ~BlockSparseMatrix();

  // Implementation of SparseMatrix interface.
  void SetZero() final;
  void RightMultiply(const double* x, double* y) const final;
  void LeftMultiply(const double* x, double* y) const final;
  void SquaredColumnNorm(double* x) const final;
  void ScaleColumns(const double* scale) final;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;

  int num_rows()         const final { return num_rows_;     }
  int num_cols()         const final { return num_cols_;     }
  int num_nonzeros()     const final { return num_nonzeros_; }
  const double* values() const final { return values_.get(); }
  double* mutable_values()     final { return values_.get(); }

  void ToTripletSparseMatrix(TripletSparseMatrix* matrix) const;
  const CompressedRowBlockStructure* block_structure() const;

  // Append the contents of m to the bottom of this matrix. m must
  // have the same column blocks structure as this matrix.
  void AppendRows(const BlockSparseMatrix& m);

  // Delete the bottom delta_rows_blocks.
  void DeleteRowBlocks(int delta_row_blocks);

  static BlockSparseMatrix* CreateDiagonalMatrix(
      const double* diagonal,
      const std::vector<Block>& column_blocks);

  struct RandomMatrixOptions {
    int num_row_blocks = 0;
    int min_row_block_size = 0;
    int max_row_block_size = 0;
    int num_col_blocks = 0;
    int min_col_block_size = 0;
    int max_col_block_size = 0;

    // 0 < block_density <= 1 is the probability of a block being
    // present in the matrix. A given random matrix will not have
    // precisely this density.
    double block_density = 0.0;

    // If col_blocks is non-empty, then the generated random matrix
    // has this block structure and the column related options in this
    // struct are ignored.
    std::vector<Block> col_blocks;
  };

  // Create a random BlockSparseMatrix whose entries are normally
  // distributed and whose structure is determined by
  // RandomMatrixOptions.
  //
  // Caller owns the result.
  static BlockSparseMatrix* CreateRandomMatrix(
      const RandomMatrixOptions& options);

 private:
  int num_rows_;
  int num_cols_;
  int num_nonzeros_;
  int max_num_nonzeros_;
  std::unique_ptr<double[]> values_;
  std::unique_ptr<CompressedRowBlockStructure> block_structure_;
};

// A number of algorithms like the SchurEliminator do not need
// access to the full BlockSparseMatrix interface. They only
// need read only access to the values array and the block structure.
//
// BlockSparseDataMatrix a struct that carries these two bits of
// information
class BlockSparseMatrixData {
 public:
  BlockSparseMatrixData(const BlockSparseMatrix& m)
      : block_structure_(m.block_structure()), values_(m.values()){};

  BlockSparseMatrixData(const CompressedRowBlockStructure* block_structure,
                        const double* values)
      : block_structure_(block_structure), values_(values) {}

  const CompressedRowBlockStructure* block_structure() const {
    return block_structure_;
  }
  const double* values() const { return values_; }

 private:
  const CompressedRowBlockStructure* block_structure_;
  const double* values_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_
