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

#ifndef CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_
#define CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_

#include <memory>
#include <string>
#include <vector>

#include "ceres/internal/disable_warnings.h"
#include "ceres/internal/export.h"
#include "ceres/sparse_matrix.h"

namespace ceres::internal {

// Structure defining a linear least squares problem and if possible
// ground truth solutions. To be used by various LinearSolver tests.
struct CERES_NO_EXPORT LinearLeastSquaresProblem {
  LinearLeastSquaresProblem() = default;

  std::unique_ptr<SparseMatrix> A;
  std::unique_ptr<double[]> b;
  std::unique_ptr<double[]> D;
  // If using the schur eliminator then how many of the variable
  // blocks are e_type blocks.
  int num_eliminate_blocks{0};

  // Solution to min_x |Ax - b|^2
  std::unique_ptr<double[]> x;
  // Solution to min_x |Ax - b|^2 + |Dx|^2
  std::unique_ptr<double[]> x_D;
};

// Factories for linear least squares problem.
CERES_NO_EXPORT std::unique_ptr<LinearLeastSquaresProblem>
CreateLinearLeastSquaresProblemFromId(int id);

CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem0();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem1();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem2();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem3();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem4();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem5();
CERES_NO_EXPORT
std::unique_ptr<LinearLeastSquaresProblem> LinearLeastSquaresProblem6();

// Write the linear least squares problem to disk. The exact format
// depends on dump_format_type.
CERES_NO_EXPORT
bool DumpLinearLeastSquaresProblem(const std::string& filename_base,
                                   DumpFormatType dump_format_type,
                                   const SparseMatrix* A,
                                   const double* D,
                                   const double* b,
                                   const double* x,
                                   int num_eliminate_blocks);
}  // namespace ceres::internal

#include "ceres/internal/reenable_warnings.h"

#endif  // CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_
