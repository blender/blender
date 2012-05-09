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

#include <string>
#include "ceres/types.h"

namespace ceres {

#define CASESTR(x) case x: return #x

const char* LinearSolverTypeToString(LinearSolverType solver_type) {
  switch (solver_type) {
    CASESTR(SPARSE_NORMAL_CHOLESKY);
    CASESTR(DENSE_QR);
    CASESTR(DENSE_SCHUR);
    CASESTR(SPARSE_SCHUR);
    CASESTR(ITERATIVE_SCHUR);
    CASESTR(CGNR);
    default:
      return "UNKNOWN";
  }
}

const char* PreconditionerTypeToString(
    PreconditionerType preconditioner_type) {
  switch (preconditioner_type) {
    CASESTR(IDENTITY);
    CASESTR(JACOBI);
    CASESTR(SCHUR_JACOBI);
    CASESTR(CLUSTER_JACOBI);
    CASESTR(CLUSTER_TRIDIAGONAL);
    default:
      return "UNKNOWN";
  }
}

const char* OrderingTypeToString(OrderingType ordering_type) {
  switch (ordering_type) {
    CASESTR(NATURAL);
    CASESTR(USER);
    CASESTR(SCHUR);
    default:
      return "UNKNOWN";
  }
}

const char* SolverTerminationTypeToString(
    SolverTerminationType termination_type) {
  switch (termination_type) {
    CASESTR(NO_CONVERGENCE);
    CASESTR(FUNCTION_TOLERANCE);
    CASESTR(GRADIENT_TOLERANCE);
    CASESTR(PARAMETER_TOLERANCE);
    CASESTR(NUMERICAL_FAILURE);
    CASESTR(USER_ABORT);
    CASESTR(USER_SUCCESS);
    CASESTR(DID_NOT_RUN);
    default:
      return "UNKNOWN";
  }
}

#undef CASESTR

bool IsSchurType(LinearSolverType type) {
  return ((type == SPARSE_SCHUR) ||
          (type == DENSE_SCHUR)  ||
          (type == ITERATIVE_SCHUR));
}

}  // namespace ceres
