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
#include "ceres/detect_structure.h"
#include "ceres/linear_solver.h"
#include "ceres/schur_complement_solver.h"
#include "ceres/suitesparse.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

LinearSolver::Summary SchurComplementSolver::SolveImpl(
    BlockSparseMatrixBase* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  const time_t start_time = time(NULL);
  if (!options_.constant_sparsity || (eliminator_.get() == NULL)) {
    InitStorage(A->block_structure());
    DetectStructure(*A->block_structure(),
                    options_.num_eliminate_blocks,
                    &options_.row_block_size,
                    &options_.e_block_size,
                    &options_.f_block_size);
    eliminator_.reset(CHECK_NOTNULL(SchurEliminatorBase::Create(options_)));
    eliminator_->Init(options_.num_eliminate_blocks, A->block_structure());
  };
  const time_t init_time = time(NULL);
  fill(x, x + A->num_cols(), 0.0);

  LinearSolver::Summary summary;
  summary.num_iterations = 1;
  summary.termination_type = FAILURE;
  eliminator_->Eliminate(A, b, per_solve_options.D, lhs_.get(), rhs_.get());
  const time_t eliminate_time = time(NULL);

  double* reduced_solution = x + A->num_cols() - lhs_->num_cols();
  const bool status = SolveReducedLinearSystem(reduced_solution);
  const time_t solve_time = time(NULL);

  if (!status) {
    return summary;
  }

  eliminator_->BackSubstitute(A, b, per_solve_options.D, reduced_solution, x);
  const time_t backsubstitute_time = time(NULL);
  summary.termination_type = TOLERANCE;

  VLOG(2) << "time (sec) total: " << backsubstitute_time - start_time
          << " init: " << init_time - start_time
          << " eliminate: " << eliminate_time - init_time
          << " solve: " << solve_time - eliminate_time
          << " backsubstitute: " << backsubstitute_time - solve_time;
  return summary;
}

// Initialize a BlockRandomAccessDenseMatrix to store the Schur
// complement.
void DenseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().num_eliminate_blocks;
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
bool DenseSchurComplementSolver::SolveReducedLinearSystem(double* solution) {
  const BlockRandomAccessDenseMatrix* m =
      down_cast<const BlockRandomAccessDenseMatrix*>(lhs());
  const int num_rows = m->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return true;
  }

  // TODO(sameeragarwal): Add proper error handling; this completely ignores
  // the quality of the solution to the solve.
  VectorRef(solution, num_rows) =
      ConstMatrixRef(m->values(), num_rows, num_rows)
      .selfadjointView<Eigen::Upper>()
      .ldlt()
      .solve(ConstVectorRef(rhs(), num_rows));

  return true;
}

#ifndef CERES_NO_SUITESPARSE
SparseSchurComplementSolver::SparseSchurComplementSolver(
    const LinearSolver::Options& options)
    : SchurComplementSolver(options),
      symbolic_factor_(NULL) {
}

SparseSchurComplementSolver::~SparseSchurComplementSolver() {
  if (symbolic_factor_ != NULL) {
    ss_.Free(symbolic_factor_);
    symbolic_factor_ = NULL;
  }
}

// Determine the non-zero blocks in the Schur Complement matrix, and
// initialize a BlockRandomAccessSparseMatrix object.
void SparseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().num_eliminate_blocks;
  const int num_col_blocks = bs->cols.size();
  const int num_row_blocks = bs->rows.size();

  vector<int> blocks(num_col_blocks - num_eliminate_blocks, 0);
  for (int i = num_eliminate_blocks; i < num_col_blocks; ++i) {
    blocks[i - num_eliminate_blocks] = bs->cols[i].size;
  }

  set<pair<int, int> > block_pairs;
  for (int i = 0; i < blocks.size(); ++i) {
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

  set_lhs(new BlockRandomAccessSparseMatrix(blocks, block_pairs));
  set_rhs(new double[lhs()->num_rows()]);
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessSparseMatrix.  The linear system is solved using
// CHOLMOD's sparse cholesky factorization routines.
bool SparseSchurComplementSolver::SolveReducedLinearSystem(double* solution) {
  // Extract the TripletSparseMatrix that is used for actually storing S.
  TripletSparseMatrix* tsm =
      const_cast<TripletSparseMatrix*>(
          down_cast<const BlockRandomAccessSparseMatrix*>(lhs())->matrix());

  const int num_rows = tsm->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return true;
  }

  cholmod_sparse* cholmod_lhs = ss_.CreateSparseMatrix(tsm);
  // The matrix is symmetric, and the upper triangular part of the
  // matrix contains the values.
  cholmod_lhs->stype = 1;

  cholmod_dense*  cholmod_rhs =
      ss_.CreateDenseVector(const_cast<double*>(rhs()), num_rows, num_rows);

  // Symbolic factorization is computed if we don't already have one handy.
  if (symbolic_factor_ == NULL) {
    symbolic_factor_ = ss_.AnalyzeCholesky(cholmod_lhs);
  }

  cholmod_dense* cholmod_solution =
      ss_.SolveCholesky(cholmod_lhs, symbolic_factor_, cholmod_rhs);

  ss_.Free(cholmod_lhs);
  cholmod_lhs = NULL;
  ss_.Free(cholmod_rhs);
  cholmod_rhs = NULL;

  // If sparsity is not constant across calls, then reset the symbolic
  // factorization.
  if (!options().constant_sparsity) {
    ss_.Free(symbolic_factor_);
    symbolic_factor_ = NULL;
  }

  if (cholmod_solution == NULL) {
    LOG(ERROR) << "CHOLMOD solve failed.";
    return false;
  }

  VectorRef(solution, num_rows)
      = VectorRef(static_cast<double*>(cholmod_solution->x), num_rows);
  ss_.Free(cholmod_solution);
  return true;
}
#endif  // CERES_NO_SUITESPARSE

}  // namespace internal
}  // namespace ceres
