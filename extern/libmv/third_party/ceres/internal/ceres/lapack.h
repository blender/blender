// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2013 Google Inc. All rights reserved.
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

#ifndef CERES_INTERNAL_LAPACK_H_
#define CERES_INTERNAL_LAPACK_H_

namespace ceres {
namespace internal {

class LAPACK {
 public:
  // Solve
  //
  //  lhs * solution = rhs
  //
  // using a Cholesky factorization. Here
  // lhs is a symmetric positive definite matrix. It is assumed to be
  // column major and only the lower triangular part of the matrix is
  // referenced.
  //
  // This function uses the LAPACK dpotrf and dpotrs routines.
  //
  // The return value is zero if the solve is successful.
  static int SolveInPlaceUsingCholesky(int num_rows,
                                       const double* lhs,
                                       double* rhs_and_solution);

  // The SolveUsingQR function requires a buffer for its temporary
  // computation. This function given the size of the lhs matrix will
  // return the size of the buffer needed.
  static int EstimateWorkSizeForQR(int num_rows, int num_cols);

  // Solve
  //
  //  lhs * solution = rhs
  //
  // using a dense QR factorization. lhs is an arbitrary (possibly
  // rectangular) matrix with full column rank.
  //
  // work is an array of size work_size that this routine uses for its
  // temporary storage. The optimal size of this array can be obtained
  // by calling EstimateWorkSizeForQR.
  //
  // When calling, rhs_and_solution contains the rhs, and upon return
  // the first num_col entries are the solution.
  //
  // This function uses the LAPACK dgels routine.
  //
  // The return value is zero if the solve is successful.
  static int SolveUsingQR(int num_rows,
                          int num_cols,
                          const double* lhs,
                          int work_size,
                          double* work,
                          double* rhs_and_solution);
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LAPACK_H_
