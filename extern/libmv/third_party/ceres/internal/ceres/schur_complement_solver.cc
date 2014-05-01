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

#include <algorithm>
#include <ctime>
#include <set>
#include <vector>

#include "Eigen/Dense"
#include "ceres/block_random_access_dense_matrix.h"
#include "ceres/block_random_access_matrix.h"
#include "ceres/block_random_access_sparse_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/cxsparse.h"
#include "ceres/detect_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/lapack.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_complement_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres {
namespace internal {

LinearSolver::Summary SchurComplementSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("SchurComplementSolver::Solve");

  if (eliminator_.get() == NULL) {
    InitStorage(A->block_structure());
    DetectStructure(*A->block_structure(),
                    options_.elimination_groups[0],
                    &options_.row_block_size,
                    &options_.e_block_size,
                    &options_.f_block_size);
    eliminator_.reset(CHECK_NOTNULL(SchurEliminatorBase::Create(options_)));
    eliminator_->Init(options_.elimination_groups[0], A->block_structure());
  };
  fill(x, x + A->num_cols(), 0.0);
  event_logger.AddEvent("Setup");

  eliminator_->Eliminate(A, b, per_solve_options.D, lhs_.get(), rhs_.get());
  event_logger.AddEvent("Eliminate");

  double* reduced_solution = x + A->num_cols() - lhs_->num_cols();
  const LinearSolver::Summary summary =
      SolveReducedLinearSystem(reduced_solution);
  event_logger.AddEvent("ReducedSolve");

  if (summary.termination_type == LINEAR_SOLVER_SUCCESS) {
    eliminator_->BackSubstitute(A, b, per_solve_options.D, reduced_solution, x);
    event_logger.AddEvent("BackSubstitute");
  }

  return summary;
}

// Initialize a BlockRandomAccessDenseMatrix to store the Schur
// complement.
void DenseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().elimination_groups[0];
  const int num_col_blocks = bs->cols.size();

  vector<int> blocks(num_col_blocks - num_eliminate_blocks, 0);
  for (int i = num_eliminate_blocks, j = 0;
       i < num_col_blocks;
       ++i, ++j) {
    blocks[j] = bs->cols[i].size;
  }

  set_lhs(new BlockRandomAccessDenseMatrix(blocks));
  set_rhs(new double[lhs()->num_rows()]);
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessDenseMatrix. The linear system is solved using
// Eigen's Cholesky factorization.
LinearSolver::Summary
DenseSchurComplementSolver::SolveReducedLinearSystem(double* solution) {
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  summary.message = "Success.";

  const BlockRandomAccessDenseMatrix* m =
      down_cast<const BlockRandomAccessDenseMatrix*>(lhs());
  const int num_rows = m->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return summary;
  }

  summary.num_iterations = 1;

  if (options().dense_linear_algebra_library_type == EIGEN) {
    Eigen::LLT<Matrix, Eigen::Upper> llt =
        ConstMatrixRef(m->values(), num_rows, num_rows)
        .selfadjointView<Eigen::Upper>()
        .llt();
    if (llt.info() != Eigen::Success) {
      summary.termination_type = LINEAR_SOLVER_FAILURE;
      summary.message = "Eigen LLT decomposition failed.";
      return summary;
    }

    VectorRef(solution, num_rows) = llt.solve(ConstVectorRef(rhs(), num_rows));
  } else {
    VectorRef(solution, num_rows) = ConstVectorRef(rhs(), num_rows);
    summary.termination_type =
        LAPACK::SolveInPlaceUsingCholesky(num_rows,
                                          m->values(),
                                          solution,
                                          &summary.message);
  }

  return summary;
}

#if !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARE)

SparseSchurComplementSolver::SparseSchurComplementSolver(
    const LinearSolver::Options& options)
    : SchurComplementSolver(options),
      factor_(NULL),
      cxsparse_factor_(NULL) {
}

SparseSchurComplementSolver::~SparseSchurComplementSolver() {
#ifndef CERES_NO_SUITESPARSE
  if (factor_ != NULL) {
    ss_.Free(factor_);
    factor_ = NULL;
  }
#endif  // CERES_NO_SUITESPARSE

#ifndef CERES_NO_CXSPARSE
  if (cxsparse_factor_ != NULL) {
    cxsparse_.Free(cxsparse_factor_);
    cxsparse_factor_ = NULL;
  }
#endif  // CERES_NO_CXSPARSE
}

// Determine the non-zero blocks in the Schur Complement matrix, and
// initialize a BlockRandomAccessSparseMatrix object.
void SparseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().elimination_groups[0];
  const int num_col_blocks = bs->cols.size();
  const int num_row_blocks = bs->rows.size();

  blocks_.resize(num_col_blocks - num_eliminate_blocks, 0);
  for (int i = num_eliminate_blocks; i < num_col_blocks; ++i) {
    blocks_[i - num_eliminate_blocks] = bs->cols[i].size;
  }

  set<pair<int, int> > block_pairs;
  for (int i = 0; i < blocks_.size(); ++i) {
    block_pairs.insert(make_pair(i, i));
  }

  int r = 0;
  while (r < num_row_blocks) {
    int e_block_id = bs->rows[r].cells.front().block_id;
    if (e_block_id >= num_eliminate_blocks) {
      break;
    }
    vector<int> f_blocks;

    // Add to the chunk until the first block in the row is
    // different than the one in the first row for the chunk.
    for (; r < num_row_blocks; ++r) {
      const CompressedRow& row = bs->rows[r];
      if (row.cells.front().block_id != e_block_id) {
        break;
      }

      // Iterate over the blocks in the row, ignoring the first
      // block since it is the one to be eliminated.
      for (int c = 1; c < row.cells.size(); ++c) {
        const Cell& cell = row.cells[c];
        f_blocks.push_back(cell.block_id - num_eliminate_blocks);
      }
    }

    sort(f_blocks.begin(), f_blocks.end());
    f_blocks.erase(unique(f_blocks.begin(), f_blocks.end()), f_blocks.end());
    for (int i = 0; i < f_blocks.size(); ++i) {
      for (int j = i + 1; j < f_blocks.size(); ++j) {
        block_pairs.insert(make_pair(f_blocks[i], f_blocks[j]));
      }
    }
  }

  // Remaing rows do not contribute to the chunks and directly go
  // into the schur complement via an outer product.
  for (; r < num_row_blocks; ++r) {
    const CompressedRow& row = bs->rows[r];
    CHECK_GE(row.cells.front().block_id, num_eliminate_blocks);
    for (int i = 0; i < row.cells.size(); ++i) {
      int r_block1_id = row.cells[i].block_id - num_eliminate_blocks;
      for (int j = 0; j < row.cells.size(); ++j) {
        int r_block2_id = row.cells[j].block_id - num_eliminate_blocks;
        if (r_block1_id <= r_block2_id) {
          block_pairs.insert(make_pair(r_block1_id, r_block2_id));
        }
      }
    }
  }

  set_lhs(new BlockRandomAccessSparseMatrix(blocks_, block_pairs));
  set_rhs(new double[lhs()->num_rows()]);
}

LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystem(double* solution) {
  switch (options().sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
      return SolveReducedLinearSystemUsingSuiteSparse(solution);
    case CX_SPARSE:
      return SolveReducedLinearSystemUsingCXSparse(solution);
    default:
      LOG(FATAL) << "Unknown sparse linear algebra library : "
                 << options().sparse_linear_algebra_library_type;
  }

  return LinearSolver::Summary();
}

#ifndef CERES_NO_SUITESPARSE
// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// CHOLMOD's sparse cholesky factorization routines.
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingSuiteSparse(
    double* solution) {
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  summary.message = "Success.";

  TripletSparseMatrix* tsm =
      const_cast<TripletSparseMatrix*>(
          down_cast<const BlockRandomAccessSparseMatrix*>(lhs())->matrix());
  const int num_rows = tsm->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return summary;
  }

  summary.num_iterations = 1;
  cholmod_sparse* cholmod_lhs = NULL;
  if (options().use_postordering) {
    // If we are going to do a full symbolic analysis of the schur
    // complement matrix from scratch and not rely on the
    // pre-ordering, then the fastest path in cholmod_factorize is the
    // one corresponding to upper triangular matrices.

    // Create a upper triangular symmetric matrix.
    cholmod_lhs = ss_.CreateSparseMatrix(tsm);
    cholmod_lhs->stype = 1;

    if (factor_ == NULL) {
      factor_ = ss_.BlockAnalyzeCholesky(cholmod_lhs,
                                         blocks_,
                                         blocks_,
                                         &summary.message);
    }
  } else {
    // If we are going to use the natural ordering (i.e. rely on the
    // pre-ordering computed by solver_impl.cc), then the fastest
    // path in cholmod_factorize is the one corresponding to lower
    // triangular matrices.

    // Create a upper triangular symmetric matrix.
    cholmod_lhs = ss_.CreateSparseMatrixTranspose(tsm);
    cholmod_lhs->stype = -1;

    if (factor_ == NULL) {
      factor_ = ss_.AnalyzeCholeskyWithNaturalOrdering(cholmod_lhs,
                                                       &summary.message);
    }
  }

  if (factor_ == NULL) {
    ss_.Free(cholmod_lhs);
    summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
    return summary;
  }

  summary.termination_type =
    ss_.Cholesky(cholmod_lhs, factor_, &summary.message);

  ss_.Free(cholmod_lhs);

  if (summary.termination_type != LINEAR_SOLVER_SUCCESS) {
    return summary;
  }

  cholmod_dense*  cholmod_rhs =
      ss_.CreateDenseVector(const_cast<double*>(rhs()), num_rows, num_rows);
  cholmod_dense* cholmod_solution = ss_.Solve(factor_,
                                              cholmod_rhs,
                                              &summary.message);
  ss_.Free(cholmod_rhs);

  if (cholmod_solution == NULL) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    return summary;
  }

  VectorRef(solution, num_rows)
      = VectorRef(static_cast<double*>(cholmod_solution->x), num_rows);
  ss_.Free(cholmod_solution);
  return summary;
}
#else
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingSuiteSparse(
    double* solution) {
  LOG(FATAL) << "No SuiteSparse support in Ceres.";
  return LinearSolver::Summary();
}
#endif  // CERES_NO_SUITESPARSE

#ifndef CERES_NO_CXSPARSE
// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// CXSparse's sparse cholesky factorization routines.
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingCXSparse(
    double* solution) {
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_SUCCESS;
  summary.message = "Success.";

  // Extract the TripletSparseMatrix that is used for actually storing S.
  TripletSparseMatrix* tsm =
      const_cast<TripletSparseMatrix*>(
          down_cast<const BlockRandomAccessSparseMatrix*>(lhs())->matrix());
  const int num_rows = tsm->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return summary;
  }

  cs_di* lhs = CHECK_NOTNULL(cxsparse_.CreateSparseMatrix(tsm));
  VectorRef(solution, num_rows) = ConstVectorRef(rhs(), num_rows);

  // Compute symbolic factorization if not available.
  if (cxsparse_factor_ == NULL) {
    cxsparse_factor_ = cxsparse_.BlockAnalyzeCholesky(lhs, blocks_, blocks_);
  }

  if (cxsparse_factor_ == NULL) {
    summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
    summary.message =
        "CXSparse failure. Unable to find symbolic factorization.";
  } else if (!cxsparse_.SolveCholesky(lhs, cxsparse_factor_, solution)) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    summary.message = "CXSparse::SolveCholesky failed.";
  }

  cxsparse_.Free(lhs);
  return summary;
}
#else
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingCXSparse(
    double* solution) {
  LOG(FATAL) << "No CXSparse support in Ceres.";
  return LinearSolver::Summary();
}
#endif  // CERES_NO_CXPARSE

#endif  // !defined(CERES_NO_SUITESPARSE) || !defined(CERES_NO_CXSPARE)
}  // namespace internal
}  // namespace ceres
