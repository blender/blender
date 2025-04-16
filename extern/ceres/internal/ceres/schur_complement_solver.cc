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

#include "ceres/schur_complement_solver.h"

#include <algorithm>
#include <ctime>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "Eigen/Dense"
#include "Eigen/SparseCore"
#include "ceres/block_random_access_dense_matrix.h"
#include "ceres/block_random_access_matrix.h"
#include "ceres/block_random_access_sparse_matrix.h"
#include "ceres/block_sparse_matrix.h"
#include "ceres/block_structure.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/detect_structure.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/sparse_cholesky.h"
#include "ceres/triplet_sparse_matrix.h"
#include "ceres/types.h"
#include "ceres/wall_time.h"

namespace ceres::internal {
namespace {

class BlockRandomAccessSparseMatrixAdapter final
    : public ConjugateGradientsLinearOperator<Vector> {
 public:
  explicit BlockRandomAccessSparseMatrixAdapter(
      const BlockRandomAccessSparseMatrix& m)
      : m_(m) {}

  void RightMultiplyAndAccumulate(const Vector& x, Vector& y) final {
    m_.SymmetricRightMultiplyAndAccumulate(x.data(), y.data());
  }

 private:
  const BlockRandomAccessSparseMatrix& m_;
};

class BlockRandomAccessDiagonalMatrixAdapter final
    : public ConjugateGradientsLinearOperator<Vector> {
 public:
  explicit BlockRandomAccessDiagonalMatrixAdapter(
      const BlockRandomAccessDiagonalMatrix& m)
      : m_(m) {}

  // y = y + Ax;
  void RightMultiplyAndAccumulate(const Vector& x, Vector& y) final {
    m_.RightMultiplyAndAccumulate(x.data(), y.data());
  }

 private:
  const BlockRandomAccessDiagonalMatrix& m_;
};

}  // namespace

SchurComplementSolver::SchurComplementSolver(
    const LinearSolver::Options& options)
    : options_(options) {
  CHECK_GT(options.elimination_groups.size(), 1);
  CHECK_GT(options.elimination_groups[0], 0);
  CHECK(options.context != nullptr);
}

LinearSolver::Summary SchurComplementSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("SchurComplementSolver::Solve");

  const CompressedRowBlockStructure* bs = A->block_structure();
  if (eliminator_ == nullptr) {
    const int num_eliminate_blocks = options_.elimination_groups[0];
    const int num_f_blocks = bs->cols.size() - num_eliminate_blocks;

    InitStorage(bs);
    DetectStructure(*bs,
                    num_eliminate_blocks,
                    &options_.row_block_size,
                    &options_.e_block_size,
                    &options_.f_block_size);

    // For the special case of the static structure <2,3,6> with
    // exactly one f block use the SchurEliminatorForOneFBlock.
    //
    // TODO(sameeragarwal): A more scalable template specialization
    // mechanism that does not cause binary bloat.
    if (options_.row_block_size == 2 && options_.e_block_size == 3 &&
        options_.f_block_size == 6 && num_f_blocks == 1) {
      eliminator_ = std::make_unique<SchurEliminatorForOneFBlock<2, 3, 6>>();
    } else {
      eliminator_ = SchurEliminatorBase::Create(options_);
    }

    CHECK(eliminator_);
    const bool kFullRankETE = true;
    eliminator_->Init(num_eliminate_blocks, kFullRankETE, bs);
  }

  std::fill(x, x + A->num_cols(), 0.0);
  event_logger.AddEvent("Setup");

  eliminator_->Eliminate(BlockSparseMatrixData(*A),
                         b,
                         per_solve_options.D,
                         lhs_.get(),
                         rhs_.data());
  event_logger.AddEvent("Eliminate");

  double* reduced_solution = x + A->num_cols() - lhs_->num_cols();
  const LinearSolver::Summary summary =
      SolveReducedLinearSystem(per_solve_options, reduced_solution);
  event_logger.AddEvent("ReducedSolve");

  if (summary.termination_type == LinearSolverTerminationType::SUCCESS) {
    eliminator_->BackSubstitute(
        BlockSparseMatrixData(*A), b, per_solve_options.D, reduced_solution, x);
    event_logger.AddEvent("BackSubstitute");
  }

  return summary;
}
DenseSchurComplementSolver::DenseSchurComplementSolver(
    const LinearSolver::Options& options)
    : SchurComplementSolver(options),
      cholesky_(DenseCholesky::Create(options)) {}

DenseSchurComplementSolver::~DenseSchurComplementSolver() = default;

// Initialize a BlockRandomAccessDenseMatrix to store the Schur
// complement.
void DenseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().elimination_groups[0];
  const int num_col_blocks = bs->cols.size();
  auto blocks = Tail(bs->cols, num_col_blocks - num_eliminate_blocks);
  set_lhs(std::make_unique<BlockRandomAccessDenseMatrix>(
      blocks, options().context, options().num_threads));
  ResizeRhs(lhs()->num_rows());
}

// Solve the system Sx = r, assuming that the matrix S is stored in a
// BlockRandomAccessDenseMatrix. The linear system is solved using
// Eigen's Cholesky factorization.
LinearSolver::Summary DenseSchurComplementSolver::SolveReducedLinearSystem(
    const LinearSolver::PerSolveOptions& /*per_solve_options*/,
    double* solution) {
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::SUCCESS;
  summary.message = "Success.";

  auto* m = down_cast<BlockRandomAccessDenseMatrix*>(mutable_lhs());
  const int num_rows = m->num_rows();

  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    return summary;
  }

  summary.num_iterations = 1;
  summary.termination_type = cholesky_->FactorAndSolve(
      num_rows, m->mutable_values(), rhs().data(), solution, &summary.message);
  return summary;
}

SparseSchurComplementSolver::SparseSchurComplementSolver(
    const LinearSolver::Options& options)
    : SchurComplementSolver(options) {
  if (options.type != ITERATIVE_SCHUR) {
    sparse_cholesky_ = SparseCholesky::Create(options);
  }
}

SparseSchurComplementSolver::~SparseSchurComplementSolver() {
  for (int i = 0; i < 4; ++i) {
    if (scratch_[i]) {
      delete scratch_[i];
      scratch_[i] = nullptr;
    }
  }
}

// Determine the non-zero blocks in the Schur Complement matrix, and
// initialize a BlockRandomAccessSparseMatrix object.
void SparseSchurComplementSolver::InitStorage(
    const CompressedRowBlockStructure* bs) {
  const int num_eliminate_blocks = options().elimination_groups[0];
  const int num_col_blocks = bs->cols.size();
  const int num_row_blocks = bs->rows.size();

  blocks_ = Tail(bs->cols, num_col_blocks - num_eliminate_blocks);

  std::set<std::pair<int, int>> block_pairs;
  for (int i = 0; i < blocks_.size(); ++i) {
    block_pairs.emplace(i, i);
  }

  int r = 0;
  while (r < num_row_blocks) {
    int e_block_id = bs->rows[r].cells.front().block_id;
    if (e_block_id >= num_eliminate_blocks) {
      break;
    }
    std::vector<int> f_blocks;

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
        block_pairs.emplace(f_blocks[i], f_blocks[j]);
      }
    }
  }

  // Remaining rows do not contribute to the chunks and directly go
  // into the schur complement via an outer product.
  for (; r < num_row_blocks; ++r) {
    const CompressedRow& row = bs->rows[r];
    CHECK_GE(row.cells.front().block_id, num_eliminate_blocks);
    for (int i = 0; i < row.cells.size(); ++i) {
      int r_block1_id = row.cells[i].block_id - num_eliminate_blocks;
      for (const auto& cell : row.cells) {
        int r_block2_id = cell.block_id - num_eliminate_blocks;
        if (r_block1_id <= r_block2_id) {
          block_pairs.emplace(r_block1_id, r_block2_id);
        }
      }
    }
  }

  set_lhs(std::make_unique<BlockRandomAccessSparseMatrix>(
      blocks_, block_pairs, options().context, options().num_threads));
  ResizeRhs(lhs()->num_rows());
}

LinearSolver::Summary SparseSchurComplementSolver::SolveReducedLinearSystem(
    const LinearSolver::PerSolveOptions& per_solve_options, double* solution) {
  if (options().type == ITERATIVE_SCHUR) {
    return SolveReducedLinearSystemUsingConjugateGradients(per_solve_options,
                                                           solution);
  }

  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::SUCCESS;
  summary.message = "Success.";

  const BlockSparseMatrix* bsm =
      down_cast<const BlockRandomAccessSparseMatrix*>(lhs())->matrix();
  if (bsm->num_rows() == 0) {
    return summary;
  }

  const CompressedRowSparseMatrix::StorageType storage_type =
      sparse_cholesky_->StorageType();
  if (storage_type ==
      CompressedRowSparseMatrix::StorageType::UPPER_TRIANGULAR) {
    if (!crs_lhs_) {
      crs_lhs_ = bsm->ToCompressedRowSparseMatrix();
      crs_lhs_->set_storage_type(
          CompressedRowSparseMatrix::StorageType::UPPER_TRIANGULAR);
    } else {
      bsm->UpdateCompressedRowSparseMatrix(crs_lhs_.get());
    }
  } else {
    if (!crs_lhs_) {
      crs_lhs_ = bsm->ToCompressedRowSparseMatrixTranspose();
      crs_lhs_->set_storage_type(
          CompressedRowSparseMatrix::StorageType::LOWER_TRIANGULAR);
    } else {
      bsm->UpdateCompressedRowSparseMatrixTranspose(crs_lhs_.get());
    }
  }

  summary.num_iterations = 1;
  summary.termination_type = sparse_cholesky_->FactorAndSolve(
      crs_lhs_.get(), rhs().data(), solution, &summary.message);
  return summary;
}

LinearSolver::Summary
SparseSchurComplementSolver::SolveReducedLinearSystemUsingConjugateGradients(
    const LinearSolver::PerSolveOptions& per_solve_options, double* solution) {
  CHECK(options().use_explicit_schur_complement);
  const int num_rows = lhs()->num_rows();
  // The case where there are no f blocks, and the system is block
  // diagonal.
  if (num_rows == 0) {
    LinearSolver::Summary summary;
    summary.num_iterations = 0;
    summary.termination_type = LinearSolverTerminationType::SUCCESS;
    summary.message = "Success.";
    return summary;
  }

  // Only SCHUR_JACOBI is supported over here right now.
  CHECK_EQ(options().preconditioner_type, SCHUR_JACOBI);

  if (preconditioner_ == nullptr) {
    preconditioner_ = std::make_unique<BlockRandomAccessDiagonalMatrix>(
        blocks_, options().context, options().num_threads);
  }

  auto* sc = down_cast<BlockRandomAccessSparseMatrix*>(mutable_lhs());

  // Extract block diagonal from the Schur complement to construct the
  // schur_jacobi preconditioner.
  for (int i = 0; i < blocks_.size(); ++i) {
    const int block_size = blocks_[i].size;

    int sc_r, sc_c, sc_row_stride, sc_col_stride;
    CellInfo* sc_cell_info =
        sc->GetCell(i, i, &sc_r, &sc_c, &sc_row_stride, &sc_col_stride);
    CHECK(sc_cell_info != nullptr);
    MatrixRef sc_m(sc_cell_info->values, sc_row_stride, sc_col_stride);

    int pre_r, pre_c, pre_row_stride, pre_col_stride;
    CellInfo* pre_cell_info = preconditioner_->GetCell(
        i, i, &pre_r, &pre_c, &pre_row_stride, &pre_col_stride);
    CHECK(pre_cell_info != nullptr);
    MatrixRef pre_m(pre_cell_info->values, pre_row_stride, pre_col_stride);

    pre_m.block(pre_r, pre_c, block_size, block_size) =
        sc_m.block(sc_r, sc_c, block_size, block_size);
  }
  preconditioner_->Invert();

  VectorRef(solution, num_rows).setZero();

  auto lhs = std::make_unique<BlockRandomAccessSparseMatrixAdapter>(*sc);
  auto preconditioner =
      std::make_unique<BlockRandomAccessDiagonalMatrixAdapter>(
          *preconditioner_);

  ConjugateGradientsSolverOptions cg_options;
  cg_options.min_num_iterations = options().min_num_iterations;
  cg_options.max_num_iterations = options().max_num_iterations;
  cg_options.residual_reset_period = options().residual_reset_period;
  cg_options.q_tolerance = per_solve_options.q_tolerance;
  cg_options.r_tolerance = per_solve_options.r_tolerance;

  cg_solution_ = Vector::Zero(sc->num_rows());
  for (int i = 0; i < 4; ++i) {
    if (scratch_[i] == nullptr) {
      scratch_[i] = new Vector(sc->num_rows());
    }
  }
  auto summary = ConjugateGradientsSolver<Vector>(
      cg_options, *lhs, rhs(), *preconditioner, scratch_, cg_solution_);
  VectorRef(solution, sc->num_rows()) = cg_solution_;
  return summary;
}

}  // namespace ceres::internal
