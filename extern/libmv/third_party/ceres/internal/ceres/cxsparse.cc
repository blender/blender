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
// Author: strandmark@google.com (Petter Strandmark)

// This include must come before any #ifndef check on Ceres compile options.
#include "ceres/internal/port.h"

#ifndef CERES_NO_CXSPARSE

#include "ceres/cxsparse.h"

#include <vector>
#include "ceres/compressed_col_sparse_matrix_utils.h"
#include "ceres/compressed_row_sparse_matrix.h"
#include "ceres/triplet_sparse_matrix.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::vector;

CXSparse::CXSparse() : scratch_(NULL), scratch_size_(0) {
}

CXSparse::~CXSparse() {
  if (scratch_size_ > 0) {
    cs_di_free(scratch_);
  }
}


bool CXSparse::SolveCholesky(cs_di* A,
                             cs_dis* symbolic_factorization,
                             double* b) {
  // Make sure we have enough scratch space available.
  if (scratch_size_ < A->n) {
    if (scratch_size_ > 0) {
      cs_di_free(scratch_);
    }
    scratch_ =
        reinterpret_cast<CS_ENTRY*>(cs_di_malloc(A->n, sizeof(CS_ENTRY)));
    scratch_size_ = A->n;
  }

  // Solve using Cholesky factorization
  csn* numeric_factorization = cs_di_chol(A, symbolic_factorization);
  if (numeric_factorization == NULL) {
    LOG(WARNING) << "Cholesky factorization failed.";
    return false;
  }

  // When the Cholesky factorization succeeded, these methods are
  // guaranteed to succeeded as well. In the comments below, "x"
  // refers to the scratch space.
  //
  // Set x = P * b.
  cs_di_ipvec(symbolic_factorization->pinv, b, scratch_, A->n);
  // Set x = L \ x.
  cs_di_lsolve(numeric_factorization->L, scratch_);
  // Set x = L' \ x.
  cs_di_ltsolve(numeric_factorization->L, scratch_);
  // Set b = P' * x.
  cs_di_pvec(symbolic_factorization->pinv, scratch_, b, A->n);

  // Free Cholesky factorization.
  cs_di_nfree(numeric_factorization);
  return true;
}

cs_dis* CXSparse::AnalyzeCholesky(cs_di* A) {
  // order = 1 for Cholesky factorization.
  return cs_schol(1, A);
}

cs_dis* CXSparse::AnalyzeCholeskyWithNaturalOrdering(cs_di* A) {
  // order = 0 for Natural ordering.
  return cs_schol(0, A);
}

cs_dis* CXSparse::BlockAnalyzeCholesky(cs_di* A,
                                       const vector<int>& row_blocks,
                                       const vector<int>& col_blocks) {
  const int num_row_blocks = row_blocks.size();
  const int num_col_blocks = col_blocks.size();

  vector<int> block_rows;
  vector<int> block_cols;
  CompressedColumnScalarMatrixToBlockMatrix(A->i,
                                            A->p,
                                            row_blocks,
                                            col_blocks,
                                            &block_rows,
                                            &block_cols);
  cs_di block_matrix;
  block_matrix.m = num_row_blocks;
  block_matrix.n = num_col_blocks;
  block_matrix.nz  = -1;
  block_matrix.nzmax = block_rows.size();
  block_matrix.p = &block_cols[0];
  block_matrix.i = &block_rows[0];
  block_matrix.x = NULL;

  int* ordering = cs_amd(1, &block_matrix);
  vector<int> block_ordering(num_row_blocks, -1);
  std::copy(ordering, ordering + num_row_blocks, &block_ordering[0]);
  cs_free(ordering);

  vector<int> scalar_ordering;
  BlockOrderingToScalarOrdering(row_blocks, block_ordering, &scalar_ordering);

  cs_dis* symbolic_factorization =
      reinterpret_cast<cs_dis*>(cs_calloc(1, sizeof(cs_dis)));
  symbolic_factorization->pinv = cs_pinv(&scalar_ordering[0], A->n);
  cs* permuted_A = cs_symperm(A, symbolic_factorization->pinv, 0);

  symbolic_factorization->parent = cs_etree(permuted_A, 0);
  int* postordering = cs_post(symbolic_factorization->parent, A->n);
  int* column_counts = cs_counts(permuted_A,
                                 symbolic_factorization->parent,
                                 postordering,
                                 0);
  cs_free(postordering);
  cs_spfree(permuted_A);

  symbolic_factorization->cp = (int*) cs_malloc(A->n+1, sizeof(int));
  symbolic_factorization->lnz = cs_cumsum(symbolic_factorization->cp,
                                          column_counts,
                                          A->n);
  symbolic_factorization->unz = symbolic_factorization->lnz;

  cs_free(column_counts);

  if (symbolic_factorization->lnz < 0) {
    cs_sfree(symbolic_factorization);
    symbolic_factorization = NULL;
  }

  return symbolic_factorization;
}

cs_di CXSparse::CreateSparseMatrixTransposeView(CompressedRowSparseMatrix* A) {
  cs_di At;
  At.m = A->num_cols();
  At.n = A->num_rows();
  At.nz = -1;
  At.nzmax = A->num_nonzeros();
  At.p = A->mutable_rows();
  At.i = A->mutable_cols();
  At.x = A->mutable_values();
  return At;
}

cs_di* CXSparse::CreateSparseMatrix(TripletSparseMatrix* tsm) {
  cs_di_sparse tsm_wrapper;
  tsm_wrapper.nzmax = tsm->num_nonzeros();
  tsm_wrapper.nz = tsm->num_nonzeros();
  tsm_wrapper.m = tsm->num_rows();
  tsm_wrapper.n = tsm->num_cols();
  tsm_wrapper.p = tsm->mutable_cols();
  tsm_wrapper.i = tsm->mutable_rows();
  tsm_wrapper.x = tsm->mutable_values();

  return cs_compress(&tsm_wrapper);
}

void CXSparse::ApproximateMinimumDegreeOrdering(cs_di* A, int* ordering) {
  int* cs_ordering = cs_amd(1, A);
  std::copy(cs_ordering, cs_ordering + A->m, ordering);
  cs_free(cs_ordering);
}

cs_di* CXSparse::TransposeMatrix(cs_di* A) {
  return cs_di_transpose(A, 1);
}

cs_di* CXSparse::MatrixMatrixMultiply(cs_di* A, cs_di* B) {
  return cs_di_multiply(A, B);
}

void CXSparse::Free(cs_di* sparse_matrix) {
  cs_di_spfree(sparse_matrix);
}

void CXSparse::Free(cs_dis* symbolic_factorization) {
  cs_di_sfree(symbolic_factorization);
}

}  // namespace internal
}  // namespace ceres

#endif  // CERES_NO_CXSPARSE
