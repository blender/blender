// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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
// Author: strandmark@google.com (Petter Strandmark)

#ifndef CERES_INTERNAL_CXSPARSE_H_
#define CERES_INTERNAL_CXSPARSE_H_

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_CXSPARSE

#include <vector>
#include "cs.h"

namespace ceres {
namespace internal {

class CompressedRowSparseMatrix;
class TripletSparseMatrix;

// This object provides access to solving linear systems using Cholesky
// factorization with a known symbolic factorization. This features does not
// explicity exist in CXSparse. The methods in the class are nonstatic because
// the class manages internal scratch space.
class CXSparse {
 public:
  CXSparse();
  ~CXSparse();

  // Solves a symmetric linear system A * x = b using Cholesky factorization.
  //  A                      - The system matrix.
  //  symbolic_factorization - The symbolic factorization of A. This is obtained
  //                           from AnalyzeCholesky.
  //  b                      - The right hand size of the linear equation. This
  //                           array will also recieve the solution.
  // Returns false if Cholesky factorization of A fails.
  bool SolveCholesky(cs_di* A, cs_dis* symbolic_factorization, double* b);

  // Creates a sparse matrix from a compressed-column form. No memory is
  // allocated or copied; the structure A is filled out with info from the
  // argument.
  cs_di CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A);

  // Creates a new matrix from a triplet form. Deallocate the returned matrix
  // with Free. May return NULL if the compression or allocation fails.
  cs_di* CreateSparseMatrix(TripletSparseMatrix* A);

  // B = A'
  //
  // The returned matrix should be deallocated with Free when not used
  // anymore.
  cs_di* TransposeMatrix(cs_di* A);

  // C = A * B
  //
  // The returned matrix should be deallocated with Free when not used
  // anymore.
  cs_di* MatrixMatrixMultiply(cs_di* A, cs_di* B);

  // Computes a symbolic factorization of A that can be used in SolveCholesky.
  //
  // The returned matrix should be deallocated with Free when not used anymore.
  cs_dis* AnalyzeCholesky(cs_di* A);

  // Computes a symbolic factorization of A that can be used in
  // SolveCholesky, but does not compute a fill-reducing ordering.
  //
  // The returned matrix should be deallocated with Free when not used anymore.
  cs_dis* AnalyzeCholeskyWithNaturalOrdering(cs_di* A);

  // Computes a symbolic factorization of A that can be used in
  // SolveCholesky. The difference from AnalyzeCholesky is that this
  // function first detects the block sparsity of the matrix using
  // information about the row and column blocks and uses this block
  // sparse matrix to find a fill-reducing ordering. This ordering is
  // then used to find a symbolic factorization. This can result in a
  // significant performance improvement AnalyzeCholesky on block
  // sparse matrices.
  //
  // The returned matrix should be deallocated with Free when not used
  // anymore.
  cs_dis* BlockAnalyzeCholesky(cs_di* A,
                               const vector<int>& row_blocks,
                               const vector<int>& col_blocks);

  // Compute an fill-reducing approximate minimum degree ordering of
  // the matrix A. ordering should be non-NULL and should point to
  // enough memory to hold the ordering for the rows of A.
  void ApproximateMinimumDegreeOrdering(cs_di* A, int* ordering);

  void Free(cs_di* sparse_matrix);
  void Free(cs_dis* symbolic_factorization);

 private:
  // Cached scratch space
  CS_ENTRY* scratch_;
  int scratch_size_;
};

}  // namespace internal
}  // namespace ceres

#else  // CERES_NO_CXSPARSE

typedef void cs_dis;

class CXSparse {
 public:
  void Free(void*) {};

};
#endif  // CERES_NO_CXSPARSE

#endif  // CERES_INTERNAL_CXSPARSE_H_
