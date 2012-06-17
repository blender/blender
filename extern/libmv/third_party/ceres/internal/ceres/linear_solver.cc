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

#include "ceres/linear_solver.h"

#include <glog/logging.h>
#include "ceres/cgnr_solver.h"
#include "ceres/dense_qr_solver.h"
#include "ceres/iterative_schur_complement_solver.h"
#include "ceres/schur_complement_solver.h"
#include "ceres/sparse_normal_cholesky_solver.h"
#include "ceres/types.h"

namespace ceres {
namespace internal {

LinearSolver::~LinearSolver() {
}

LinearSolver* LinearSolver::Create(const LinearSolver::Options& options) {
  switch (options.type) {
    case CGNR:
      return new CgnrSolver(options);

    case SPARSE_NORMAL_CHOLESKY:
#ifndef CERES_NO_SUITESPARSE
      return new SparseNormalCholeskySolver(options);
#else
      LOG(WARNING) << "SPARSE_NORMAL_CHOLESKY is not available. Please "
                   << "build Ceres with SuiteSparse. Returning NULL.";
      return NULL;
#endif  // CERES_NO_SUITESPARSE

    case SPARSE_SCHUR:
#ifndef CERES_NO_SUITESPARSE
      return new SparseSchurComplementSolver(options);
#else
      LOG(WARNING) << "SPARSE_SCHUR is not available. Please "
                   << "build Ceres with SuiteSparse. Returning NULL.";
      return NULL;
#endif  // CERES_NO_SUITESPARSE

    case DENSE_SCHUR:
      return new DenseSchurComplementSolver(options);

    case ITERATIVE_SCHUR:
      return new IterativeSchurComplementSolver(options);

    case DENSE_QR:
      return new DenseQRSolver(options);

    default:
      LOG(FATAL) << "Unknown linear solver type :"
                 << options.type;
	  return NULL;  // MSVC doesn't understand that LOG(FATAL) never returns.
  }
}

}  // namespace internal
}  // namespace ceres
