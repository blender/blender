// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2014 Google Inc. All rights reserved.
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

#include "ceres/internal/port.h"

#include <algorithm>
#include <ctime>
#include <set>
#include <vector>

#include "ceres/block_random_access_dense_matrix.h"
#include "ceres/block_random_access_matrix.h"
#include "ceres/block_random_access_sparse_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/cxsparse.h"
#include "ceres/detect_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/lapack.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_complement_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"
#include "Eigen/Dense"
#include "Eigen/SparseCore"

namespace ceres {
namespace internal {
namespace {

class BlockRandomAccessSparseMatrixAdapter : public LinearOperator {
  public:
  explicit BlockRandomAccessSparseMatrixAdapter(
      const BlockRandomAccessSparseMatrix& m)
      : m_(m) {
  }

  virtual ~BlockRandomAccessSparseMatrixAdapter() {}

  // y = y + Ax;
  virtual void RightMultiply(const double* x, double* y) const {
    m_.SymmetricRightMultiply(x, y);
  }

  // y = y + A'x;
  virtual void LeftMultiply(const double* x, double* y) const {
    m_.SymmetricRightMultiply(x, y);
  }

  virtual int num_rows() const { return m_.num_rows(); }
  virtual int num_cols() const { return m_.num_rows(); }

 private:
  const BlockRandomAccessSparseMatrix& m_;
};

class BlockRandomAccessDiagonalMatrixAdapter : public LinearOperator {
  public:
  explicit BlockRandomAccessDiagonalMatrixAdapter(
      const BlockRandomAccessDiagonalMatrix& m)
      : m_(m) {
  }

  virtual ~BlockRandomAccessDiagonalMatrixAdapter() {}

  // y = y + Ax;
  virtual void RightMultiply(const double* x, double* y) const {
    m_.RightMultiply(x, y);
  }

  // y = y + A'x;
  virtual void LeftMultiply(const double* x, double* y) const {
    m_.RightMultiply(x, y);
  }

  virtual int num_rows() const { return m_.num_rows(); }
  virtual int num_cols() const { return m_.num_rows(); }

 private:
  const BlockRandomAccessDiagonalMatrix& m_;
};

} // namespace

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
      SolveReducedLinearSystem(per_solve_options, reduced_solution);
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
DenseSchurComplementSolver::SolveReducedLinearSystem(
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* solution) {
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
      summary.message =
          "Eigen failure. Unable to perform dense Cholesky factorization.";
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

SparseSchurComplementSolver::SparseSchurComplementSolver(
    const LinearSolver::Options& options)
    : SchurComplementSolver(options),
      factor_(NULL),
      cxsparse_factor_(NULL) {
}

SparseSchurComplementSolver::~SparseSchurComplementSolver() {
  if (factor_ != NULL) {
    ss_.Free(factor_);
    factor_ = NULL;
  }

  if (cxsparse_factor_ != NULL) {
    cxsparse_.Free(cxsparse_factor_);
    cxsparse_factor_ = NULL;
  }
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
SparseSchurComplementSolver::SolveReducedLinearSystem(
          const LinearSolver::PerSolveOptions& per_solve_options,
          double* solution) {
  if (options().type == ITERATIVE_SCHUR) {
    CHECK(options().use_explicit_schur_complement);
    return SolveReducedLinearSystemUsingConjugateGradients(per_solve_options,
                                                           solution);
  }

  switch (options().sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
      return SolveReducedLinearSystemUsingSuiteSparse(per_solve_options,
                                                      solution);
    case CX_SPARSE:
      return SolveReducedLinearSystemUsingCXSparse(per_solve_options,
                                                   solution);
    case EIGEN_SPARSE:
      return SolveReducedLinearSystemUsingEigen(per_solve_options,
                                                solution);
    default:
      LOG(FATAL) << "Unknown sparse linear algebra library : "
                 << options().sparse_linear_algebra_library_type;
  }

  return LinearSolver::Summary();
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// CHOLMOD's sparse cholesky factorization routines.
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingSuiteSparse(
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* solution) {
#ifdef CERES_NO_SUITESPARSE

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
  summary.message = "Ceres was not built with SuiteSparse support. "
      "Therefore, SPARSE_SCHUR cannot be used with SUITE_SPARSE";
  return summary;

#else

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
    // No need to set message as it has already been set by the
    // symbolic analysis routines above.
    return summary;
  }

  summary.termination_type =
    ss_.Cholesky(cholmod_lhs, factor_, &summary.message);

  ss_.Free(cholmod_lhs);

  if (summary.termination_type != LINEAR_SOLVER_SUCCESS) {
    // No need to set message as it has already been set by the
    // numeric factorization routine above.
    return summary;
  }

  cholmod_dense*  cholmod_rhs =
      ss_.CreateDenseVector(const_cast<double*>(rhs()), num_rows, num_rows);
  cholmod_dense* cholmod_solution = ss_.Solve(factor_,
                                              cholmod_rhs,
                                              &summary.message);
  ss_.Free(cholmod_rhs);

  if (cholmod_solution == NULL) {
    summary.message =
        "SuiteSparse failure. Unable to perform triangular solve.";
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    return summary;
  }

  VectorRef(solution, num_rows)
      = VectorRef(static_cast<double*>(cholmod_solution->x), num_rows);
  ss_.Free(cholmod_solution);
  return summary;
#endif  // CERES_NO_SUITESPARSE
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// CXSparse's sparse cholesky factorization routines.
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingCXSparse(
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* solution) {
#ifdef CERES_NO_CXSPARSE

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
  summary.message = "Ceres was not built with CXSparse support. "
      "Therefore, SPARSE_SCHUR cannot be used with CX_SPARSE";
  return summary;

#else

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
#endif  // CERES_NO_CXPARSE
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// Eigen's sparse cholesky factorization routines.
LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingEigen(
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* solution) {
#ifndef CERES_USE_EIGEN_SPARSE

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
  summary.message =
      "SPARSE_SCHUR cannot be used with EIGEN_SPARSE. "
      "Ceres was not built with support for "
      "Eigen's SimplicialLDLT decomposition. "
      "This requires enabling building with -DEIGENSPARSE=ON.";
  return summary;

#else
  EventLogger event_logger("SchurComplementSolver::EigenSolve");
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

  // This is an upper triangular matrix.
  CompressedRowSparseMatrix crsm(*tsm);
  // Map this to a column major, lower triangular matrix.
  Eigen::MappedSparseMatrix<double, Eigen::ColMajor> eigen_lhs(
      crsm.num_rows(),
      crsm.num_rows(),
      crsm.num_nonzeros(),
      crsm.mutable_rows(),
      crsm.mutable_cols(),
      crsm.mutable_values());
  event_logger.AddEvent("ToCompressedRowSparseMatrix");

  // Compute symbolic factorization if one does not exist.
  if (simplicial_ldlt_.get() == NULL) {
    simplicial_ldlt_.reset(new SimplicialLDLT);
    // This ordering is quite bad. The scalar ordering produced by the
    // AMD algorithm is quite bad and can be an order of magnitude
    // worse than the one computed using the block version of the
    // algorithm.
    simplicial_ldlt_->analyzePattern(eigen_lhs);
    event_logger.AddEvent("Analysis");
    if (simplicial_ldlt_->info() != Eigen::Success) {
      summary.termination_type = LINEAR_SOLVER_FATAL_ERROR;
      summary.message =
          "Eigen failure. Unable to find symbolic factorization.";
      return summary;
    }
  }

  simplicial_ldlt_->factorize(eigen_lhs);
  event_logger.AddEvent("Factorize");
  if (simplicial_ldlt_->info() != Eigen::Success) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    summary.message = "Eigen failure. Unable to find numeric factoriztion.";
    return summary;
  }

  VectorRef(solution, num_rows) =
      simplicial_ldlt_->solve(ConstVectorRef(rhs(), num_rows));
  event_logger.AddEvent("Solve");
  if (simplicial_ldlt_->info() != Eigen::Success) {
    summary.termination_type = LINEAR_SOLVER_FAILURE;
    summary.message = "Eigen failure. Unable to do triangular solve.";
  }

  return summary;
#endif  // CERES_USE_EIGEN_SPARSE
}

LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingConjugateGradients(
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* solution) {
  const int num_rows = lhs()->num_rows();
  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    LinearSolver::Summary summary;
    summary.num_iterations = 0;
    summary.termination_type = LINEAR_SOLVER_SUCCESS;
    summary.message = "Success.";
    return summary;
  }

  // Only SCHUR_JACOBI is supported over here right now.
  CHECK_EQ(options().preconditioner_type, SCHUR_JACOBI);

  if (preconditioner_.get() == NULL) {
    preconditioner_.reset(new BlockRandomAccessDiagonalMatrix(blocks_));
  }

  BlockRandomAccessSparseMatrix* sc =
      down_cast<BlockRandomAccessSparseMatrix*>(
          const_cast<BlockRandomAccessMatrix*>(lhs()));

  // Extract block diagonal from the Schur complement to construct the
  // schur_jacobi preconditioner.
  for (int i = 0; i  < blocks_.size(); ++i) {
    const int block_size = blocks_[i];

    int sc_r, sc_c, sc_row_stride, sc_col_stride;
    CellInfo* sc_cell_info =
        CHECK_NOTNULL(sc->GetCell(i, i,
                                  &sc_r, &sc_c,
                                  &sc_row_stride, &sc_col_stride));
    MatrixRef sc_m(sc_cell_info->values, sc_row_stride, sc_col_stride);

    int pre_r, pre_c, pre_row_stride, pre_col_stride;
    CellInfo* pre_cell_info = CHECK_NOTNULL(
        preconditioner_->GetCell(i, i,
                                 &pre_r, &pre_c,
                                 &pre_row_stride, &pre_col_stride));
    MatrixRef pre_m(pre_cell_info->values, pre_row_stride, pre_col_stride);

    pre_m.block(pre_r, pre_c, block_size, block_size) =
        sc_m.block(sc_r, sc_c, block_size, block_size);
  }
  preconditioner_->Invert();

  VectorRef(solution, num_rows).setZero();

  scoped_ptr<LinearOperator> lhs_adapter(
      new BlockRandomAccessSparseMatrixAdapter(*sc));
  scoped_ptr<LinearOperator> preconditioner_adapter(
      new BlockRandomAccessDiagonalMatrixAdapter(*preconditioner_));


  LinearSolver::Options cg_options;
  cg_options.min_num_iterations = options().min_num_iterations;
  cg_options.max_num_iterations = options().max_num_iterations;
  ConjugateGradientsSolver cg_solver(cg_options);

  LinearSolver::PerSolveOptions cg_per_solve_options;
  cg_per_solve_options.r_tolerance = per_solve_options.r_tolerance;
  cg_per_solve_options.q_tolerance = per_solve_options.q_tolerance;
  cg_per_solve_options.preconditioner = preconditioner_adapter.get();

  return cg_solver.Solve(lhs_adapter.get(),
                         rhs(),
                         cg_per_solve_options,
                         solution);
}

}  // namespace internal
}  // namespace ceres
