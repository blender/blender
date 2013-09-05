// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_INCOMPLETE_LQ_FACTORIZATION_H_
#define CERES_INTERNAL_INCOMPLETE_LQ_FACTORIZATION_H_

#include <vector>
#include <utility>
#include "ceres/compressed_row_sparse_matrix.h"

namespace ceres {
namespace internal {

// Incomplete LQ factorization as described in
//
// Preconditioning techniques for indefinite and nonsymmetric linear
// systems. Yousef Saad, Preprint RIACS-ILQ-TR, RIACS, NASA Ames
// Research Center, Moffett Field, CA, 1987.
//
// An incomplete LQ factorization of a matrix A is a decomposition
//
//   A = LQ + E
//
// Where L is a lower triangular matrix, and Q is a near orthonormal
// matrix. The extent of orthonormality depends on E. E is the "drop"
// matrix. Each row of L has a maximum of l_level_of_fill entries, and
// all non-zero entries are within l_drop_tolerance of the largest
// entry. Each row of Q has a maximum of q_level_of_fill entries and
// all non-zero entries are within q_drop_tolerance of the largest
// entry.
//
// E is the error of the incomplete factorization.
//
// The purpose of incomplete factorizations is preconditioning and
// there one only needs the L matrix, therefore this function just
// returns L.
//
// Caller owns the result.
CompressedRowSparseMatrix* IncompleteLQFactorization(
    const CompressedRowSparseMatrix& matrix,
    const int l_level_of_fill,
    const double l_drop_tolerance,
    const int q_level_of_fill,
    const double q_drop_tolerance);

// In the row vector dense_row(0:num_cols), drop values smaller than
// the max_value * drop_tolerance. Of the remaining non-zero values,
// choose at most level_of_fill values and then add the resulting row
// vector to matrix.
//
// scratch is used to prevent allocations inside this function. It is
// assumed that scratch is of size matrix->num_cols().
void DropEntriesAndAddRow(const Vector& dense_row,
                          const int num_entries,
                          const int level_of_fill,
                          const double drop_tolerance,
                          vector<pair<int, double> >* scratch,
                          CompressedRowSparseMatrix* matrix);

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_INCOMPLETE_LQ_FACTORIZATION_H_
