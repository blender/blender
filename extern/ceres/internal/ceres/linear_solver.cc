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

#include "ceres/linear_solver.h"

#include <memory>

#include "ceres/cgnr_solver.h"
#include "ceres/dense_normal_cholesky_solver.h"
#include "ceres/dense_qr_solver.h"
#include "ceres/dynamic_sparse_normal_cholesky_solver.h"
#include "ceres/internal/config.h"
#include "ceres/iterative_schur_complement_solver.h"
#include "ceres/schur_complement_solver.h"
#include "ceres/sparse_normal_cholesky_solver.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres::internal {

LinearSolver::~LinearSolver() = default;

LinearSolverType LinearSolver::LinearSolverForZeroEBlocks(
    LinearSolverType linear_solver_type) {
  if (!IsSchurType(linear_solver_type)) {
    return linear_solver_type;
  }

  if (linear_solver_type == SPARSE_SCHUR) {
    return SPARSE_NORMAL_CHOLESKY;
  }

  if (linear_solver_type == DENSE_SCHUR) {
    // TODO(sameeragarwal): This is probably not a great choice.
    // Ideally, we should have a DENSE_NORMAL_CHOLESKY, that can take
    // a BlockSparseMatrix as input.
    return DENSE_QR;
  }

  if (linear_solver_type == ITERATIVE_SCHUR) {
    return CGNR;
  }

  return linear_solver_type;
}

std::unique_ptr<LinearSolver> LinearSolver::Create(
    const LinearSolver::Options& options) {
  CHECK(options.context != nullptr);

  switch (options.type) {
    case CGNR: {
#ifndef CERES_NO_CUDA
      if (options.sparse_linear_algebra_library_type == CUDA_SPARSE) {
        std::string error;
        return CudaCgnrSolver::Create(options, &error);
      }
#endif
      return std::make_unique<CgnrSolver>(options);
    } break;

    case SPARSE_NORMAL_CHOLESKY:
#if defined(CERES_NO_SPARSE)
      return nullptr;
#else
      if (options.dynamic_sparsity) {
        return std::make_unique<DynamicSparseNormalCholeskySolver>(options);
      }

      return std::make_unique<SparseNormalCholeskySolver>(options);
#endif

    case SPARSE_SCHUR:
#if defined(CERES_NO_SPARSE)
      return nullptr;
#else
      return std::make_unique<SparseSchurComplementSolver>(options);
#endif

    case DENSE_SCHUR:
      return std::make_unique<DenseSchurComplementSolver>(options);

    case ITERATIVE_SCHUR:
      if (options.use_explicit_schur_complement) {
        return std::make_unique<SparseSchurComplementSolver>(options);
      } else {
        return std::make_unique<IterativeSchurComplementSolver>(options);
      }

    case DENSE_QR:
      return std::make_unique<DenseQRSolver>(options);

    case DENSE_NORMAL_CHOLESKY:
      return std::make_unique<DenseNormalCholeskySolver>(options);

    default:
      LOG(FATAL) << "Unknown linear solver type :" << options.type;
      return nullptr;  // MSVC doesn't understand that LOG(FATAL) never returns.
  }
}

}  // namespace ceres::internal
