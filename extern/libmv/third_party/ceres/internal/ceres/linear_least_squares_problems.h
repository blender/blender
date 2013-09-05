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

#ifndef CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_
#define CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_

#include <string>
#include <vector>
#include "ceres/sparse_matrix.h"
#include "ceres/internal/port.h"
#include "ceres/internal/scoped_ptr.h"

namespace ceres {
namespace internal {

// Structure defining a linear least squares problem and if possible
// ground truth solutions. To be used by various LinearSolver tests.
struct LinearLeastSquaresProblem {
  LinearLeastSquaresProblem()
      : A(NULL), b(NULL), D(NULL), num_eliminate_blocks(0),
        x(NULL), x_D(NULL) {
  }

  scoped_ptr<SparseMatrix> A;
  scoped_array<double> b;
  scoped_array<double> D;
  // If using the schur eliminator then how many of the variable
  // blocks are e_type blocks.
  int num_eliminate_blocks;

  // Solution to min_x |Ax - b|^2
  scoped_array<double> x;
  // Solution to min_x |Ax - b|^2 + |Dx|^2
  scoped_array<double> x_D;
};

// Factories for linear least squares problem.
LinearLeastSquaresProblem* CreateLinearLeastSquaresProblemFromId(int id);

LinearLeastSquaresProblem* LinearLeastSquaresProblem0();
LinearLeastSquaresProblem* LinearLeastSquaresProblem1();
LinearLeastSquaresProblem* LinearLeastSquaresProblem2();
LinearLeastSquaresProblem* LinearLeastSquaresProblem3();

// Write the linear least squares problem to disk. The exact format
// depends on dump_format_type.
bool DumpLinearLeastSquaresProblem(const string& filename_base,
                                   DumpFormatType dump_format_type,
                                   const SparseMatrix* A,
                                   const double* D,
                                   const double* b,
                                   const double* x,
                                   int num_eliminate_blocks);
}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LINEAR_LEAST_SQUARES_PROBLEMS_H_
