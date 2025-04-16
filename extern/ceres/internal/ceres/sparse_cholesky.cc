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

#include "ceres/sparse_cholesky.h"

#include <memory>
#include <utility>

#include "ceres/accelerate_sparse.h"
#include "ceres/eigensparse.h"
#include "ceres/float_suitesparse.h"
#include "ceres/iterative_refiner.h"
#include "ceres/suitesparse.h"

namespace ceres::internal {

std::unique_ptr<SparseCholesky> SparseCholesky::Create(
    const LinearSolver::Options& options) {
  std::unique_ptr<SparseCholesky> sparse_cholesky;

  switch (options.sparse_linear_algebra_library_type) {
    case SUITE_SPARSE:
#ifndef CERES_NO_SUITESPARSE
      if (options.use_mixed_precision_solves) {
        sparse_cholesky =
            FloatSuiteSparseCholesky::Create(options.ordering_type);
      } else {
        sparse_cholesky = SuiteSparseCholesky::Create(options.ordering_type);
      }
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for SuiteSparse.";
#endif

    case EIGEN_SPARSE:
#ifdef CERES_USE_EIGEN_SPARSE
      if (options.use_mixed_precision_solves) {
        sparse_cholesky =
            FloatEigenSparseCholesky::Create(options.ordering_type);
      } else {
        sparse_cholesky = EigenSparseCholesky::Create(options.ordering_type);
      }
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for "
                 << "Eigen's sparse Cholesky factorization routines.";
#endif

    case ACCELERATE_SPARSE:
#ifndef CERES_NO_ACCELERATE_SPARSE
      if (options.use_mixed_precision_solves) {
        sparse_cholesky =
            AppleAccelerateCholesky<float>::Create(options.ordering_type);
      } else {
        sparse_cholesky =
            AppleAccelerateCholesky<double>::Create(options.ordering_type);
      }
      break;
#else
      LOG(FATAL) << "Ceres was compiled without support for Apple's Accelerate "
                 << "framework solvers.";
#endif

    default:
      LOG(FATAL) << "Unknown sparse linear algebra library type : "
                 << SparseLinearAlgebraLibraryTypeToString(
                        options.sparse_linear_algebra_library_type);
  }

  if (options.max_num_refinement_iterations > 0) {
    auto refiner = std::make_unique<SparseIterativeRefiner>(
        options.max_num_refinement_iterations);
    sparse_cholesky = std::make_unique<RefinedSparseCholesky>(
        std::move(sparse_cholesky), std::move(refiner));
  }
  return sparse_cholesky;
}

SparseCholesky::~SparseCholesky() = default;

LinearSolverTerminationType SparseCholesky::FactorAndSolve(
    CompressedRowSparseMatrix* lhs,
    const double* rhs,
    double* solution,
    std::string* message) {
  LinearSolverTerminationType termination_type = Factorize(lhs, message);
  if (termination_type == LinearSolverTerminationType::SUCCESS) {
    termination_type = Solve(rhs, solution, message);
  }
  return termination_type;
}

RefinedSparseCholesky::RefinedSparseCholesky(
    std::unique_ptr<SparseCholesky> sparse_cholesky,
    std::unique_ptr<SparseIterativeRefiner> iterative_refiner)
    : sparse_cholesky_(std::move(sparse_cholesky)),
      iterative_refiner_(std::move(iterative_refiner)) {}

RefinedSparseCholesky::~RefinedSparseCholesky() = default;

CompressedRowSparseMatrix::StorageType RefinedSparseCholesky::StorageType()
    const {
  return sparse_cholesky_->StorageType();
}

LinearSolverTerminationType RefinedSparseCholesky::Factorize(
    CompressedRowSparseMatrix* lhs, std::string* message) {
  lhs_ = lhs;
  return sparse_cholesky_->Factorize(lhs, message);
}

LinearSolverTerminationType RefinedSparseCholesky::Solve(const double* rhs,
                                                         double* solution,
                                                         std::string* message) {
  CHECK(lhs_ != nullptr);
  auto termination_type = sparse_cholesky_->Solve(rhs, solution, message);
  if (termination_type != LinearSolverTerminationType::SUCCESS) {
    return termination_type;
  }

  iterative_refiner_->Refine(*lhs_, rhs, sparse_cholesky_.get(), solution);
  return LinearSolverTerminationType::SUCCESS;
}

}  // namespace ceres::internal
