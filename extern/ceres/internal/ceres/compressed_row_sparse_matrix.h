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

#ifndef CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_
#define CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_

#include <memory>
#include <random>
#include <vector>

#include "ceres/block_structure.h"
#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/sparse_matrix.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

struct CRSMatrix;

namespace internal {

class ContextImpl;
class TripletSparseMatrix;

class CERES_NO_EXPORT CompressedRowSparseMatrix : public SparseMatrix {
 public:
  enum class StorageType {
    UNSYMMETRIC,
    // Matrix is assumed to be symmetric but only the lower triangular
    // part of the matrix is stored.
    LOWER_TRIANGULAR,
    // Matrix is assumed to be symmetric but only the upper triangular
    // part of the matrix is stored.
    UPPER_TRIANGULAR
  };

  // Create a matrix with the same content as the TripletSparseMatrix
  // input. We assume that input does not have any repeated
  // entries.
  //
  // The storage type of the matrix is set to UNSYMMETRIC.
  static std::unique_ptr<CompressedRowSparseMatrix> FromTripletSparseMatrix(
      const TripletSparseMatrix& input);

  // Create a matrix with the same content as the TripletSparseMatrix
  // input transposed. We assume that input does not have any repeated
  // entries.
  //
  // The storage type of the matrix is set to UNSYMMETRIC.
  static std::unique_ptr<CompressedRowSparseMatrix>
  FromTripletSparseMatrixTransposed(const TripletSparseMatrix& input);

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
  //
  // The storage type is set to UNSYMMETRIC.
  CompressedRowSparseMatrix(int num_rows, int num_cols, int max_num_nonzeros);

  // Build a square sparse diagonal matrix with num_rows rows and
  // columns. The diagonal m(i,i) = diagonal(i);
  //
  // The storage type is set to UNSYMMETRIC
  CompressedRowSparseMatrix(const double* diagonal, int num_rows);

  // SparseMatrix interface.
  ~CompressedRowSparseMatrix() override;
  void SetZero() final;
  void RightMultiplyAndAccumulate(const double* x, double* y) const final;
  void RightMultiplyAndAccumulate(const double* x,
                                  double* y,
                                  ContextImpl* context,
                                  int num_threads) const final;
  void LeftMultiplyAndAccumulate(const double* x, double* y) const final;
  void SquaredColumnNorm(double* x) const final;
  void ScaleColumns(const double* scale) final;
  void ToDenseMatrix(Matrix* dense_matrix) const final;
  void ToTextFile(FILE* file) const final;
  int num_rows() const final { return num_rows_; }
  int num_cols() const final { return num_cols_; }
  int num_nonzeros() const final { return rows_[num_rows_]; }
  const double* values() const final { return values_.data(); }
  double* mutable_values() final { return values_.data(); }

  // Delete the bottom delta_rows.
  // num_rows -= delta_rows
  void DeleteRows(int delta_rows);

  // Append the contents of m to the bottom of this matrix. m must
  // have the same number of columns as this matrix.
  void AppendRows(const CompressedRowSparseMatrix& m);

  void ToCRSMatrix(CRSMatrix* matrix) const;

  std::unique_ptr<CompressedRowSparseMatrix> Transpose() const;

  // Destructive array resizing method.
  void SetMaxNumNonZeros(int num_nonzeros);

  // Non-destructive array resizing method.
  void set_num_rows(const int num_rows) { num_rows_ = num_rows; }
  void set_num_cols(const int num_cols) { num_cols_ = num_cols; }

  // Low level access methods that expose the structure of the matrix.
  const int* cols() const { return cols_.data(); }
  int* mutable_cols() { return cols_.data(); }

  const int* rows() const { return rows_.data(); }
  int* mutable_rows() { return rows_.data(); }

  StorageType storage_type() const { return storage_type_; }
  void set_storage_type(const StorageType storage_type) {
    storage_type_ = storage_type;
  }

  const std::vector<Block>& row_blocks() const { return row_blocks_; }
  std::vector<Block>* mutable_row_blocks() { return &row_blocks_; }

  const std::vector<Block>& col_blocks() const { return col_blocks_; }
  std::vector<Block>* mutable_col_blocks() { return &col_blocks_; }

  // Create a block diagonal CompressedRowSparseMatrix with the given
  // block structure. The individual blocks are assumed to be laid out
  // contiguously in the diagonal array, one block at a time.
  static std::unique_ptr<CompressedRowSparseMatrix> CreateBlockDiagonalMatrix(
      const double* diagonal, const std::vector<Block>& blocks);

  // Options struct to control the generation of random block sparse
  // matrices in compressed row sparse format.
  //
  // The random matrix generation proceeds as follows.
  //
  // First the row and column block structure is determined by
  // generating random row and column block sizes that lie within the
  // given bounds.
  //
  // Then we walk the block structure of the resulting matrix, and with
  // probability block_density determine whether they are structurally
  // zero or not. If the answer is no, then we generate entries for the
  // block which are distributed normally.
  struct RandomMatrixOptions {
    // Type of matrix to create.
    //
    // If storage_type is UPPER_TRIANGULAR (LOWER_TRIANGULAR), then
    // create a square symmetric matrix with just the upper triangular
    // (lower triangular) part. In this case, num_col_blocks,
    // min_col_block_size and max_col_block_size will be ignored and
    // assumed to be equal to the corresponding row settings.
    StorageType storage_type = StorageType::UNSYMMETRIC;

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
  };

  // Create a random CompressedRowSparseMatrix whose entries are
  // normally distributed and whose structure is determined by
  // RandomMatrixOptions.
  static std::unique_ptr<CompressedRowSparseMatrix> CreateRandomMatrix(
      RandomMatrixOptions options, std::mt19937& prng);

 private:
  static std::unique_ptr<CompressedRowSparseMatrix> FromTripletSparseMatrix(
      const TripletSparseMatrix& input, bool transpose);

  int num_rows_;
  int num_cols_;
  std::vector<int> rows_;
  std::vector<int> cols_;
  std::vector<double> values_;
  StorageType storage_type_;

  // If the matrix has an underlying block structure, then it can also
  // carry with it row and column block sizes. This is auxiliary and
  // optional information for use by algorithms operating on the
  // matrix. The class itself does not make use of this information in
  // any way.
  std::vector<Block> row_blocks_;
  std::vector<Block> col_blocks_;
};

inline std::ostream& operator<<(std::ostream& s,
                                CompressedRowSparseMatrix::StorageType type) {
  switch (type) {
    case CompressedRowSparseMatrix::StorageType::UNSYMMETRIC:
      s << "UNSYMMETRIC";
      break;
    case CompressedRowSparseMatrix::StorageType::UPPER_TRIANGULAR:
      s << "UPPER_TRIANGULAR";
      break;
    case CompressedRowSparseMatrix::StorageType::LOWER_TRIANGULAR:
      s << "LOWER_TRIANGULAR";
      break;
    default:
      s << "UNKNOWN CompressedRowSparseMatrix::StorageType";
  }
  return s;
}
}  // namespace internal
}  // namespace ceres

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_COMPRESSED_ROW_SPARSE_MATRIX_H_
