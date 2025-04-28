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
// Author: keir@google.com (Keir Mierle)

#include "ceres/cgnr_solver.h"

#include <memory>
#include <utility>

#include "ceres/block_jacobi_preconditioner.h"
#include "ceres/conjugate_gradients_solver.h"
#include "ceres/cuda_sparse_matrix.h"
#include "ceres/cuda_vector.h"
#include "ceres/internal/eigen.h"
#include "ceres/linear_solver.h"
#include "ceres/subset_preconditioner.h"
#include "ceres/wall_time.h"
#include "glog/logging.h"

namespace ceres::internal {

// A linear operator which takes a matrix A and a diagonal vector D and
// performs products of the form
//
//   (A^T A + D^T D)x
//
// This is used to implement iterative general sparse linear solving with
// conjugate gradients, where A is the Jacobian and D is a regularizing
// parameter. A brief proof that D^T D is the correct regularizer:
//
// Given a regularized least squares problem:
//
//   min  ||Ax - b||^2 + ||Dx||^2
//    x
//
// First expand into matrix notation:
//
//   (Ax - b)^T (Ax - b) + xD^TDx
//
// Then multiply out to get:
//
//   = xA^TAx - 2b^T Ax + b^Tb + xD^TDx
//
// Take the derivative:
//
//   0 = 2A^TAx - 2A^T b + 2 D^TDx
//   0 = A^TAx - A^T b + D^TDx
//   0 = (A^TA + D^TD)x - A^T b
//
// Thus, the symmetric system we need to solve for CGNR is
//
//   Sx = z
//
// with S = A^TA + D^TD
//  and z = A^T b
//
// Note: This class is not thread safe, since it uses some temporary storage.
class CERES_NO_EXPORT CgnrLinearOperator final
    : public ConjugateGradientsLinearOperator<Vector> {
 public:
  CgnrLinearOperator(const LinearOperator& A,
                     const double* D,
                     ContextImpl* context,
                     int num_threads)
      : A_(A),
        D_(D),
        z_(Vector::Zero(A.num_rows())),
        context_(context),
        num_threads_(num_threads) {}

  void RightMultiplyAndAccumulate(const Vector& x, Vector& y) final {
    // z = Ax
    // y = y + Atz
    z_.setZero();
    A_.RightMultiplyAndAccumulate(x, z_, context_, num_threads_);
    A_.LeftMultiplyAndAccumulate(z_, y, context_, num_threads_);

    // y = y + DtDx
    if (D_ != nullptr) {
      int n = A_.num_cols();
      ParallelAssign(
          context_,
          num_threads_,
          y,
          y.array() + ConstVectorRef(D_, n).array().square() * x.array());
    }
  }

 private:
  const LinearOperator& A_;
  const double* D_;
  Vector z_;

  ContextImpl* context_;
  int num_threads_;
};

CgnrSolver::CgnrSolver(LinearSolver::Options options)
    : options_(std::move(options)) {
  if (options_.preconditioner_type != JACOBI &&
      options_.preconditioner_type != IDENTITY &&
      options_.preconditioner_type != SUBSET) {
    LOG(FATAL)
        << "Preconditioner = "
        << PreconditionerTypeToString(options_.preconditioner_type) << ". "
        << "Congratulations, you found a bug in Ceres. Please report it.";
  }
}

CgnrSolver::~CgnrSolver() {
  for (int i = 0; i < 4; ++i) {
    if (scratch_[i]) {
      delete scratch_[i];
      scratch_[i] = nullptr;
    }
  }
}

LinearSolver::Summary CgnrSolver::SolveImpl(
    BlockSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("CgnrSolver::Solve");
  if (!preconditioner_) {
    Preconditioner::Options preconditioner_options;
    preconditioner_options.type = options_.preconditioner_type;
    preconditioner_options.subset_preconditioner_start_row_block =
        options_.subset_preconditioner_start_row_block;
    preconditioner_options.sparse_linear_algebra_library_type =
        options_.sparse_linear_algebra_library_type;
    preconditioner_options.ordering_type = options_.ordering_type;
    preconditioner_options.num_threads = options_.num_threads;
    preconditioner_options.context = options_.context;

    if (options_.preconditioner_type == JACOBI) {
      preconditioner_ = std::make_unique<BlockSparseJacobiPreconditioner>(
          preconditioner_options, *A);
    } else if (options_.preconditioner_type == SUBSET) {
      preconditioner_ =
          std::make_unique<SubsetPreconditioner>(preconditioner_options, *A);
    } else {
      preconditioner_ = std::make_unique<IdentityPreconditioner>(A->num_cols());
    }
  }
  preconditioner_->Update(*A, per_solve_options.D);

  ConjugateGradientsSolverOptions cg_options;
  cg_options.min_num_iterations = options_.min_num_iterations;
  cg_options.max_num_iterations = options_.max_num_iterations;
  cg_options.residual_reset_period = options_.residual_reset_period;
  cg_options.q_tolerance = per_solve_options.q_tolerance;
  cg_options.r_tolerance = per_solve_options.r_tolerance;
  cg_options.context = options_.context;
  cg_options.num_threads = options_.num_threads;

  // lhs = AtA + DtD
  CgnrLinearOperator lhs(
      *A, per_solve_options.D, options_.context, options_.num_threads);
  // rhs = Atb.
  Vector rhs(A->num_cols());
  rhs.setZero();
  A->LeftMultiplyAndAccumulate(
      b, rhs.data(), options_.context, options_.num_threads);

  cg_solution_ = Vector::Zero(A->num_cols());
  for (int i = 0; i < 4; ++i) {
    if (scratch_[i] == nullptr) {
      scratch_[i] = new Vector(A->num_cols());
    }
  }
  event_logger.AddEvent("Setup");

  LinearOperatorAdapter preconditioner(*preconditioner_);
  auto summary = ConjugateGradientsSolver(
      cg_options, lhs, rhs, preconditioner, scratch_, cg_solution_);
  VectorRef(x, A->num_cols()) = cg_solution_;
  event_logger.AddEvent("Solve");
  return summary;
}

#ifndef CERES_NO_CUDA

// A linear operator which takes a matrix A and a diagonal vector D and
// performs products of the form
//
//   (A^T A + D^T D)x
//
// This is used to implement iterative general sparse linear solving with
// conjugate gradients, where A is the Jacobian and D is a regularizing
// parameter. A brief proof is included in cgnr_linear_operator.h.
class CERES_NO_EXPORT CudaCgnrLinearOperator final
    : public ConjugateGradientsLinearOperator<CudaVector> {
 public:
  CudaCgnrLinearOperator(CudaSparseMatrix& A,
                         const CudaVector& D,
                         CudaVector* z)
      : A_(A), D_(D), z_(z) {}

  void RightMultiplyAndAccumulate(const CudaVector& x, CudaVector& y) final {
    // z = Ax
    z_->SetZero();
    A_.RightMultiplyAndAccumulate(x, z_);

    // y = y + Atz
    //   = y + AtAx
    A_.LeftMultiplyAndAccumulate(*z_, &y);

    // y = y + DtDx
    y.DtDxpy(D_, x);
  }

 private:
  CudaSparseMatrix& A_;
  const CudaVector& D_;
  CudaVector* z_ = nullptr;
};

class CERES_NO_EXPORT CudaIdentityPreconditioner final
    : public CudaPreconditioner {
 public:
  void Update(const CompressedRowSparseMatrix& A, const double* D) final {}
  void RightMultiplyAndAccumulate(const CudaVector& x, CudaVector& y) final {
    y.Axpby(1.0, x, 1.0);
  }
};

// This class wraps the existing CPU Jacobi preconditioner, caches the structure
// of the block diagonal, and for each CGNR solve updates the values on the CPU
// and then copies them over to the GPU.
class CERES_NO_EXPORT CudaJacobiPreconditioner final
    : public CudaPreconditioner {
 public:
  explicit CudaJacobiPreconditioner(Preconditioner::Options options,
                                    const CompressedRowSparseMatrix& A)
      : options_(std::move(options)),
        cpu_preconditioner_(options_, A),
        m_(options_.context, cpu_preconditioner_.matrix()) {}
  ~CudaJacobiPreconditioner() = default;

  void Update(const CompressedRowSparseMatrix& A, const double* D) final {
    cpu_preconditioner_.Update(A, D);
    m_.CopyValuesFromCpu(cpu_preconditioner_.matrix());
  }

  void RightMultiplyAndAccumulate(const CudaVector& x, CudaVector& y) final {
    m_.RightMultiplyAndAccumulate(x, &y);
  }

 private:
  Preconditioner::Options options_;
  BlockCRSJacobiPreconditioner cpu_preconditioner_;
  CudaSparseMatrix m_;
};

CudaCgnrSolver::CudaCgnrSolver(LinearSolver::Options options)
    : options_(std::move(options)) {}

CudaCgnrSolver::~CudaCgnrSolver() {
  for (int i = 0; i < 4; ++i) {
    if (scratch_[i]) {
      delete scratch_[i];
      scratch_[i] = nullptr;
    }
  }
}

std::unique_ptr<CudaCgnrSolver> CudaCgnrSolver::Create(
    LinearSolver::Options options, std::string* error) {
  CHECK(error != nullptr);
  if (options.preconditioner_type != IDENTITY &&
      options.preconditioner_type != JACOBI) {
    *error =
        "CudaCgnrSolver does not support preconditioner type " +
        std::string(PreconditionerTypeToString(options.preconditioner_type)) +
        ". ";
    return nullptr;
  }
  CHECK(options.context->IsCudaInitialized())
      << "CudaCgnrSolver requires CUDA initialization.";
  auto solver = std::make_unique<CudaCgnrSolver>(options);
  return solver;
}

void CudaCgnrSolver::CpuToGpuTransfer(const CompressedRowSparseMatrix& A,
                                      const double* b,
                                      const double* D) {
  if (A_ == nullptr) {
    // Assume structure is not cached, do an initialization and structural copy.
    A_ = std::make_unique<CudaSparseMatrix>(options_.context, A);
    b_ = std::make_unique<CudaVector>(options_.context, A.num_rows());
    x_ = std::make_unique<CudaVector>(options_.context, A.num_cols());
    Atb_ = std::make_unique<CudaVector>(options_.context, A.num_cols());
    Ax_ = std::make_unique<CudaVector>(options_.context, A.num_rows());
    D_ = std::make_unique<CudaVector>(options_.context, A.num_cols());

    Preconditioner::Options preconditioner_options;
    preconditioner_options.type = options_.preconditioner_type;
    preconditioner_options.subset_preconditioner_start_row_block =
        options_.subset_preconditioner_start_row_block;
    preconditioner_options.sparse_linear_algebra_library_type =
        options_.sparse_linear_algebra_library_type;
    preconditioner_options.ordering_type = options_.ordering_type;
    preconditioner_options.num_threads = options_.num_threads;
    preconditioner_options.context = options_.context;

    if (options_.preconditioner_type == JACOBI) {
      preconditioner_ =
          std::make_unique<CudaJacobiPreconditioner>(preconditioner_options, A);
    } else {
      preconditioner_ = std::make_unique<CudaIdentityPreconditioner>();
    }
    for (int i = 0; i < 4; ++i) {
      scratch_[i] = new CudaVector(options_.context, A.num_cols());
    }
  } else {
    // Assume structure is cached, do a value copy.
    A_->CopyValuesFromCpu(A);
  }
  b_->CopyFromCpu(ConstVectorRef(b, A.num_rows()));
  D_->CopyFromCpu(ConstVectorRef(D, A.num_cols()));
}

LinearSolver::Summary CudaCgnrSolver::SolveImpl(
    CompressedRowSparseMatrix* A,
    const double* b,
    const LinearSolver::PerSolveOptions& per_solve_options,
    double* x) {
  EventLogger event_logger("CudaCgnrSolver::Solve");
  LinearSolver::Summary summary;
  summary.num_iterations = 0;
  summary.termination_type = LinearSolverTerminationType::FATAL_ERROR;

  CpuToGpuTransfer(*A, b, per_solve_options.D);
  event_logger.AddEvent("CPU to GPU Transfer");
  preconditioner_->Update(*A, per_solve_options.D);
  event_logger.AddEvent("Preconditioner Update");

  // Form z = Atb.
  Atb_->SetZero();
  A_->LeftMultiplyAndAccumulate(*b_, Atb_.get());

  // Solve (AtA + DtD)x = z (= Atb).
  x_->SetZero();
  CudaCgnrLinearOperator lhs(*A_, *D_, Ax_.get());

  event_logger.AddEvent("Setup");

  ConjugateGradientsSolverOptions cg_options;
  cg_options.min_num_iterations = options_.min_num_iterations;
  cg_options.max_num_iterations = options_.max_num_iterations;
  cg_options.residual_reset_period = options_.residual_reset_period;
  cg_options.q_tolerance = per_solve_options.q_tolerance;
  cg_options.r_tolerance = per_solve_options.r_tolerance;

  summary = ConjugateGradientsSolver(
      cg_options, lhs, *Atb_, *preconditioner_, scratch_, *x_);
  x_->CopyTo(x);
  event_logger.AddEvent("Solve");
  return summary;
}

#endif  // CERES_NO_CUDA

}  // namespace ceres::internal
