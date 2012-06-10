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
// Implementation of the SparseMatrix interface for block sparse
// matrices.

#ifndef CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_
#define CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_

#include "ceres/block_structure.h"
#include "ceres/sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/macros.h"
#include "ceres/internal/scoped_ptr.h"

namespace ceres {
namespace internal {

class SparseMatrixProto;
class TripletSparseMatrix;

// A further extension of the SparseMatrix interface to support block-oriented
// matrices. The key addition is the RowBlockValues() accessor, which enables
// the lazy block sparse matrix implementation.
class BlockSparseMatrixBase : public SparseMatrix {
 public:
  BlockSparseMatrixBase() {}
  virtual ~BlockSparseMatrixBase() {}

  // Convert this matrix into a triplet sparse matrix.
  virtual void ToTripletSparseMatrix(TripletSparseMatrix* matrix) const = 0;

  // Returns a pointer to the block structure. Does not transfer
  // ownership.
  virtual const CompressedRowBlockStructure* block_structure() const = 0;

  // Returns a pointer to a row of the matrix. The returned array is only valid
  // until the next call to RowBlockValues. The caller does not own the result.
  //
  // The returned array is laid out such that cells on the specified row are
  // contiguous in the returned array, though neighbouring cells in row order
  // may not be contiguous in the row values. The cell values for cell
  // (row_block, cell_block) are found at offset
  //
  //   block_structure()->rows[row_block].cells[cell_block].position
  //
  virtual const double* RowBlockValues(int row_block_index) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlockSparseMatrixBase);
};

// This class implements the SparseMatrix interface for storing and
// manipulating block sparse matrices. The block structure is stored
// in the CompressedRowBlockStructure object and one is needed to
// initialize the matrix. For details on how the blocks structure of
// the matrix is stored please see the documentation
//
//   internal/ceres/block_structure.h
//
class BlockSparseMatrix : public BlockSparseMatrixBase {
 public:
  // Construct a block sparse matrix with a fully initialized
  // CompressedRowBlockStructure objected. The matrix takes over
  // ownership of this object and destroys it upon destruction.
  //
  // TODO(sameeragarwal): Add a function which will validate legal
  // CompressedRowBlockStructure objects.
  explicit BlockSparseMatrix(CompressedRowBlockStructure* block_structure);

  // Construct a block sparse matrix from a protocol buffer.
#ifndef CERES_DONT_HAVE_PROTOCOL_BUFFERS
  explicit BlockSparseMatrix(const SparseMatrixProto& proto);
#endif

  BlockSparseMatrix();
  virtual ~BlockSparseMatrix();

  // Implementation of SparseMatrix interface.
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

  virtual int num_rows()         const { return num_rows_;     }
  virtual int num_cols()         const { return num_cols_;     }
  virtual int num_nonzeros()     const { return num_nonzeros_; }
  virtual const double* values() const { return values_.get(); }
  virtual double* mutable_values()     { return values_.get(); }

  // Implementation of BlockSparseMatrixBase interface.
  virtual void ToTripletSparseMatrix(TripletSparseMatrix* matrix) const;
  virtual const CompressedRowBlockStructure* block_structure() const;
  virtual const double* RowBlockValues(int row_block_index) const {
    return values_.get();
  }

 private:
  int num_rows_;
  int num_cols_;
  int max_num_nonzeros_;
  int num_nonzeros_;
  scoped_array<double> values_;
  scoped_ptr<CompressedRowBlockStructure> block_structure_;
  DISALLOW_COPY_AND_ASSIGN(BlockSparseMatrix);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_BLOCK_SPARSE_MATRIX_H_
