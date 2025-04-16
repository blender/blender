// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
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
#include <random>

#include "ceres/block_structure.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/context_impl.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/export.h"
#include "ceres/sparse_matrix.h"

namespace ceres::internal {

class TripletSparseMatrix;

// This class implements the SparseMatrix interface for storing and
// manipulating block sparse matrices. The block structure is stored
// in the CompressedRowBlockStructure object and one is needed to
// initialize the matrix. For details on how the blocks structure of
// the matrix is stored please see the documentation
//
//   internal/ceres/block_structure.h
//
class CERES_NO_EXPORT BlockSparseMatrix final : public SparseMatrix {
 public:
  // Construct a block sparse matrix with a fully initialized
  // CompressedRowBlockStructure objected. The matrix takes over
  // ownership of this object and destroys it upon destruction.
  //
  // TODO(sameeragarwal): Add a function which will validate legal
  // CompressedRowBlockStructure objects.
  explicit BlockSparseMatrix(CompressedRowBlockStructure* block_structure,
                             bool use_page_locked_memory = false);
  ~BlockSparseMatrix();

  BlockSparseMatrix(const BlockSparseMatrix&) = delete;
  void operator=(const BlockSparseMatrix&) = delete;

  // Implementation of SparseMatrix interface.
  void SetZero() override final;
  void SetZero(ContextImpl* context, int num_threads) override final;
  void RightMultiplyAndAccumulate(const double* x, double* y) const final;
  void RightMultiplyAndAccumulate(const double* x,
                                  double* y,
                                  ContextImpl* context,
                                  int num_threads) const final;
  void LeftMultiplyAndAccumulate(const double* x, double* y) const final;
  void LeftMultiplyAndAccumulate(const double* x,
                                 double* y,
                                 ContextImpl* context,
                                 int num_threads) const final;
  void SquaredColumnNorm(double* x) const final;
  void SquaredColumnNorm(double* x,
                         ContextImpl* context,
                         int num_threads) const final;
  void ScaleColumns(const double* scale) final;
  void ScaleColumns(const double* scale,
                    ContextImpl* context,
                    int num_threads) final;

  // Convert to CompressedRowSparseMatrix
  std::unique_ptr<CompressedRowSparseMatrix> ToCompressedRowSparseMatrix()
      const;
  // Create CompressedRowSparseMatrix corresponding to transposed matrix
  std::unique_ptr<CompressedRowSparseMatrix>
  ToCompressedRowSparseMatrixTranspose() const;
  // Copy values to CompressedRowSparseMatrix that has compatible structure
  void UpdateCompressedRowSparseMatrix(
      CompressedRowSparseMatrix* crs_matrix) const;
  // Copy values to CompressedRowSparseMatrix that has structure of transposed
  // matrix
  void UpdateCompressedRowSparseMatrixTranspose(
      CompressedRowSparseMatrix* crs_matrix) const;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;

  void AddTransposeBlockStructure();

  // clang-format off
  int num_rows()         const final { return num_rows_;     }
  int num_cols()         const final { return num_cols_;     }
  int num_nonzeros()     const final { return num_nonzeros_; }
  const double* values() const final { return values_; }
  double* mutable_values()     final { return values_; }
  // clang-format on

  void ToTripletSparseMatrix(TripletSparseMatrix* matrix) const;
  const CompressedRowBlockStructure* block_structure() const;
  const CompressedRowBlockStructure* transpose_block_structure() const;

  // Append the contents of m to the bottom of this matrix. m must
  // have the same column blocks structure as this matrix.
  void AppendRows(const BlockSparseMatrix& m);

  // Delete the bottom delta_rows_blocks.
  void DeleteRowBlocks(int delta_row_blocks);

  static std::unique_ptr<BlockSparseMatrix> CreateDiagonalMatrix(
      const double* diagonal, const std::vector<Block>& column_blocks);

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
  static std::unique_ptr<BlockSparseMatrix> CreateRandomMatrix(
      const RandomMatrixOptions& options,
      std::mt19937& prng,
      bool use_page_locked_memory = false);

 private:
  double* AllocateValues(int size);
  void FreeValues(double*& values);

  const bool use_page_locked_memory_;
  int num_rows_;
  int num_cols_;
  int num_nonzeros_;
  int max_num_nonzeros_;
  double* values_;
  std::unique_ptr<CompressedRowBlockStructure> block_structure_;
  std::unique_ptr<CompressedRowBlockStructure> transpose_block_structure_;
};

// A number of algorithms like the SchurEliminator do not need
// access to the full BlockSparseMatrix interface. They only
// need read only access to the values array and the block structure.
//
// BlockSparseDataMatrix a struct that carries these two bits of
// information
class CERES_NO_EXPORT BlockSparseMatrixData {
 public:
  explicit BlockSparseMatrixData(const BlockSparseMatrix& m)
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

std::unique_ptr<CompressedRowBlockStructure> CreateTranspose(
    const CompressedRowBlockStructure& bs);

}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_
