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

#ifndef CERES_INTERNAL_COMPRESSED_COL_SPARSE_MATRIX_UTILS_H_
#define CERES_INTERNAL_COMPRESSED_COL_SPARSE_MATRIX_UTILS_H_

#include <vector>
#include "ceres/internal/port.h"

namespace ceres {
namespace internal {

// Extract the block sparsity pattern of the scalar compressed columns
// matrix and return it in compressed column form. The compressed
// column form is stored in two vectors block_rows, and block_cols,
// which correspond to the row and column arrays in a compressed
// column sparse matrix.
//
// If c_ij is the block in the matrix A corresponding to row block i
// and column block j, then it is expected that A contains at least
// one non-zero entry corresponding to the top left entry of c_ij,
// as that entry is used to detect the presence of a non-zero c_ij.
void CompressedColumnScalarMatrixToBlockMatrix(
    const int* scalar_rows,
    const int* scalar_cols,
    const std::vector<int>& row_blocks,
    const std::vector<int>& col_blocks,
    std::vector<int>* block_rows,
    std::vector<int>* block_cols);

// Given a set of blocks and a permutation of these blocks, compute
// the corresponding "scalar" ordering, where the scalar ordering of
// size sum(blocks).
void BlockOrderingToScalarOrdering(
    const std::vector<int>& blocks,
    const std::vector<int>& block_ordering,
    std::vector<int>* scalar_ordering);

// Solve the linear system
//
//   R * solution = rhs
//
// Where R is an upper triangular compressed column sparse matrix.
template <typename IntegerType>
void SolveUpperTriangularInPlace(IntegerType num_cols,
                                 const IntegerType* rows,
                                 const IntegerType* cols,
                                 const double* values,
                                 double* rhs_and_solution) {
  for (IntegerType c = num_cols - 1; c >= 0; --c) {
    rhs_and_solution[c] /= values[cols[c + 1] - 1];
    for (IntegerType idx = cols[c]; idx < cols[c + 1] - 1; ++idx) {
      const IntegerType r = rows[idx];
      const double v = values[idx];
      rhs_and_solution[r] -= v * rhs_and_solution[c];
    }
  }
}

// Solve the linear system
//
//   R' * solution = rhs
//
// Where R is an upper triangular compressed column sparse matrix.
template <typename IntegerType>
void SolveUpperTriangularTransposeInPlace(IntegerType num_cols,
                                          const IntegerType* rows,
                                          const IntegerType* cols,
                                          const double* values,
                                          double* rhs_and_solution) {
  for (IntegerType c = 0; c < num_cols; ++c) {
    for (IntegerType idx = cols[c]; idx < cols[c + 1] - 1; ++idx) {
      const IntegerType r = rows[idx];
      const double v = values[idx];
      rhs_and_solution[c] -= v * rhs_and_solution[r];
    }
    rhs_and_solution[c] =  rhs_and_solution[c] / values[cols[c + 1] - 1];
  }
}

// Given a upper triangular matrix R in compressed column form, solve
// the linear system,
//
//  R'R x = b
//
// Where b is all zeros except for rhs_nonzero_index, where it is
// equal to one.
//
// The function exploits this knowledge to reduce the number of
// floating point operations.
template <typename IntegerType>
void SolveRTRWithSparseRHS(IntegerType num_cols,
                           const IntegerType* rows,
                           const IntegerType* cols,
                           const double* values,
                           const int rhs_nonzero_index,
                           double* solution) {
  std::fill(solution, solution + num_cols, 0.0);
  solution[rhs_nonzero_index] = 1.0 / values[cols[rhs_nonzero_index + 1] - 1];

  for (IntegerType c = rhs_nonzero_index + 1; c < num_cols; ++c) {
    for (IntegerType idx = cols[c]; idx < cols[c + 1] - 1; ++idx) {
      const IntegerType r = rows[idx];
      if (r < rhs_nonzero_index) continue;
      const double v = values[idx];
      solution[c] -= v * solution[r];
    }
    solution[c] =  solution[c] / values[cols[c + 1] - 1];
  }

  SolveUpperTriangularInPlace(num_cols, rows, cols, values, solution);
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_COMPRESSED_COL_SPARSE_MATRIX_UTILS_H_
