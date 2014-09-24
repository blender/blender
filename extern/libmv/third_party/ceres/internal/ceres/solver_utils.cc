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

#include <string>

#include "ceres/internal/port.h"
#include "ceres/version.h"

namespace ceres {
namespace internal {

string VersionString() {
  string value = string(CERES_VERSION_STRING);

#ifdef CERES_NO_LAPACK
  value += "-no_lapack";
#else
  value += "-lapack";
#endif

#ifndef CERES_NO_SUITESPARSE
  value += "-suitesparse";
#endif

#ifndef CERES_NO_CXSPARSE
  value += "-cxsparse";
#endif

#ifdef CERES_USE_EIGEN_SPARSE
  value += "-eigensparse";
#endif

#ifdef CERES_RESTRUCT_SCHUR_SPECIALIZATIONS
  value += "-no_schur_specializations";
#endif

#ifdef CERES_USE_OPENMP
  value += "-openmp";
#else
  value += "-no_openmp";
#endif

#ifdef CERES_NO_CUSTOM_BLAS
  value += "-no_custom_blas";
#endif

  return value;
}

}  // namespace internal
}  // namespace ceres
