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
// Author: sameeragarwal@google.com (Sameer Agarwal)

#include <algorithm>
#include <cctype>
#include <string>
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {

using std::string;

#define CASESTR(x) case x: return #x
#define STRENUM(x) if (value == #x) { *type = x; return true;}

static void UpperCase(string* input) {
  std::transform(input->begin(), input->end(), input->begin(), ::toupper);
}

const char* LinearSolverTypeToString(LinearSolverType type) {
  switch (type) {
    CASESTR(DENSE_NORMAL_CHOLESKY);
    CASESTR(DENSE_QR);
    CASESTR(SPARSE_NORMAL_CHOLESKY);
    CASESTR(DENSE_SCHUR);
    CASESTR(SPARSE_SCHUR);
    CASESTR(ITERATIVE_SCHUR);
    CASESTR(CGNR);
    default:
      return "UNKNOWN";
  }
}

bool StringToLinearSolverType(string value, LinearSolverType* type) {
  UpperCase(&value);
  STRENUM(DENSE_NORMAL_CHOLESKY);
  STRENUM(DENSE_QR);
  STRENUM(SPARSE_NORMAL_CHOLESKY);
  STRENUM(DENSE_SCHUR);
  STRENUM(SPARSE_SCHUR);
  STRENUM(ITERATIVE_SCHUR);
  STRENUM(CGNR);
  return false;
}

const char* PreconditionerTypeToString(PreconditionerType type) {
  switch (type) {
    CASESTR(IDENTITY);
    CASESTR(JACOBI);
    CASESTR(SCHUR_JACOBI);
    CASESTR(CLUSTER_JACOBI);
    CASESTR(CLUSTER_TRIDIAGONAL);
    default:
      return "UNKNOWN";
  }
}

bool StringToPreconditionerType(string value, PreconditionerType* type) {
  UpperCase(&value);
  STRENUM(IDENTITY);
  STRENUM(JACOBI);
  STRENUM(SCHUR_JACOBI);
  STRENUM(CLUSTER_JACOBI);
  STRENUM(CLUSTER_TRIDIAGONAL);
  return false;
}

const char* SparseLinearAlgebraLibraryTypeToString(
    SparseLinearAlgebraLibraryType type) {
  switch (type) {
    CASESTR(SUITE_SPARSE);
    CASESTR(CX_SPARSE);
    CASESTR(EIGEN_SPARSE);
    CASESTR(NO_SPARSE);
    default:
      return "UNKNOWN";
  }
}

bool StringToSparseLinearAlgebraLibraryType(
    string value,
    SparseLinearAlgebraLibraryType* type) {
  UpperCase(&value);
  STRENUM(SUITE_SPARSE);
  STRENUM(CX_SPARSE);
  STRENUM(EIGEN_SPARSE);
  STRENUM(NO_SPARSE);
  return false;
}

const char* DenseLinearAlgebraLibraryTypeToString(
    DenseLinearAlgebraLibraryType type) {
  switch (type) {
    CASESTR(EIGEN);
    CASESTR(LAPACK);
    default:
      return "UNKNOWN";
  }
}

bool StringToDenseLinearAlgebraLibraryType(
    string value,
    DenseLinearAlgebraLibraryType* type) {
  UpperCase(&value);
  STRENUM(EIGEN);
  STRENUM(LAPACK);
  return false;
}

const char* TrustRegionStrategyTypeToString(TrustRegionStrategyType type) {
  switch (type) {
    CASESTR(LEVENBERG_MARQUARDT);
    CASESTR(DOGLEG);
    default:
      return "UNKNOWN";
  }
}

bool StringToTrustRegionStrategyType(string value,
                                     TrustRegionStrategyType* type) {
  UpperCase(&value);
  STRENUM(LEVENBERG_MARQUARDT);
  STRENUM(DOGLEG);
  return false;
}

const char* DoglegTypeToString(DoglegType type) {
  switch (type) {
    CASESTR(TRADITIONAL_DOGLEG);
    CASESTR(SUBSPACE_DOGLEG);
    default:
      return "UNKNOWN";
  }
}

bool StringToDoglegType(string value, DoglegType* type) {
  UpperCase(&value);
  STRENUM(TRADITIONAL_DOGLEG);
  STRENUM(SUBSPACE_DOGLEG);
  return false;
}

const char* MinimizerTypeToString(MinimizerType type) {
  switch (type) {
    CASESTR(TRUST_REGION);
    CASESTR(LINE_SEARCH);
    default:
      return "UNKNOWN";
  }
}

bool StringToMinimizerType(string value, MinimizerType* type) {
  UpperCase(&value);
  STRENUM(TRUST_REGION);
  STRENUM(LINE_SEARCH);
  return false;
}

const char* LineSearchDirectionTypeToString(LineSearchDirectionType type) {
  switch (type) {
    CASESTR(STEEPEST_DESCENT);
    CASESTR(NONLINEAR_CONJUGATE_GRADIENT);
    CASESTR(LBFGS);
    CASESTR(BFGS);
    default:
      return "UNKNOWN";
  }
}

bool StringToLineSearchDirectionType(string value,
                                     LineSearchDirectionType* type) {
  UpperCase(&value);
  STRENUM(STEEPEST_DESCENT);
  STRENUM(NONLINEAR_CONJUGATE_GRADIENT);
  STRENUM(LBFGS);
  STRENUM(BFGS);
  return false;
}

const char* LineSearchTypeToString(LineSearchType type) {
  switch (type) {
    CASESTR(ARMIJO);
    CASESTR(WOLFE);
    default:
      return "UNKNOWN";
  }
}

bool StringToLineSearchType(string value, LineSearchType* type) {
  UpperCase(&value);
  STRENUM(ARMIJO);
  STRENUM(WOLFE);
  return false;
}

const char* LineSearchInterpolationTypeToString(
    LineSearchInterpolationType type) {
  switch (type) {
    CASESTR(BISECTION);
    CASESTR(QUADRATIC);
    CASESTR(CUBIC);
    default:
      return "UNKNOWN";
  }
}

bool StringToLineSearchInterpolationType(
    string value,
    LineSearchInterpolationType* type) {
  UpperCase(&value);
  STRENUM(BISECTION);
  STRENUM(QUADRATIC);
  STRENUM(CUBIC);
  return false;
}

const char* NonlinearConjugateGradientTypeToString(
    NonlinearConjugateGradientType type) {
  switch (type) {
    CASESTR(FLETCHER_REEVES);
    CASESTR(POLAK_RIBIERE);
    CASESTR(HESTENES_STIEFEL);
    default:
      return "UNKNOWN";
  }
}

bool StringToNonlinearConjugateGradientType(
    string value,
    NonlinearConjugateGradientType* type) {
  UpperCase(&value);
  STRENUM(FLETCHER_REEVES);
  STRENUM(POLAK_RIBIERE);
  STRENUM(HESTENES_STIEFEL);
  return false;
}

const char* CovarianceAlgorithmTypeToString(
    CovarianceAlgorithmType type) {
  switch (type) {
    CASESTR(DENSE_SVD);
    CASESTR(EIGEN_SPARSE_QR);
    CASESTR(SUITE_SPARSE_QR);
    default:
      return "UNKNOWN";
  }
}

bool StringToCovarianceAlgorithmType(
    string value,
    CovarianceAlgorithmType* type) {
  UpperCase(&value);
  STRENUM(DENSE_SVD);
  STRENUM(EIGEN_SPARSE_QR);
  STRENUM(SUITE_SPARSE_QR);
  return false;
}

const char* NumericDiffMethodTypeToString(
    NumericDiffMethodType type) {
  switch (type) {
    CASESTR(CENTRAL);
    CASESTR(FORWARD);
    CASESTR(RIDDERS);
    default:
      return "UNKNOWN";
  }
}

bool StringToNumericDiffMethodType(
    string value,
    NumericDiffMethodType* type) {
  UpperCase(&value);
  STRENUM(CENTRAL);
  STRENUM(FORWARD);
  STRENUM(RIDDERS);
  return false;
}

const char* VisibilityClusteringTypeToString(
    VisibilityClusteringType type) {
  switch (type) {
    CASESTR(CANONICAL_VIEWS);
    CASESTR(SINGLE_LINKAGE);
    default:
      return "UNKNOWN";
  }
}

bool StringToVisibilityClusteringType(
    string value,
    VisibilityClusteringType* type) {
  UpperCase(&value);
  STRENUM(CANONICAL_VIEWS);
  STRENUM(SINGLE_LINKAGE);
  return false;
}

const char* TerminationTypeToString(TerminationType type) {
  switch (type) {
    CASESTR(CONVERGENCE);
    CASESTR(NO_CONVERGENCE);
    CASESTR(FAILURE);
    CASESTR(USER_SUCCESS);
    CASESTR(USER_FAILURE);
    default:
      return "UNKNOWN";
  }
}

#undef CASESTR
#undef STRENUM

bool IsSchurType(LinearSolverType type) {
  return ((type == SPARSE_SCHUR) ||
          (type == DENSE_SCHUR)  ||
          (type == ITERATIVE_SCHUR));
}

bool IsSparseLinearAlgebraLibraryTypeAvailable(
    SparseLinearAlgebraLibraryType type) {
  if (type == SUITE_SPARSE) {
#ifdef CERES_NO_SUITESPARSE
    return false;
#else
    return true;
#endif
  }

  if (type == CX_SPARSE) {
#ifdef CERES_NO_CXSPARSE
    return false;
#else
    return true;
#endif
  }

  if (type == EIGEN_SPARSE) {
#ifdef CERES_USE_EIGEN_SPARSE
    return true;
#else
    return false;
#endif
  }

  LOG(WARNING) << "Unknown sparse linear algebra library " << type;
  return false;
}

bool IsDenseLinearAlgebraLibraryTypeAvailable(
    DenseLinearAlgebraLibraryType type) {
  if (type == EIGEN) {
    return true;
  }
  if (type == LAPACK) {
#ifdef CERES_NO_LAPACK
    return false;
#else
    return true;
#endif
  }

  LOG(WARNING) << "Unknown dense linear algebra library " << type;
  return false;
}

}  // namespace ceres
