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
//
// Cost term that implements a prior on a parameter block using a
// normal distribution.

#ifndef CERES_PUBLIC_NORMAL_PRIOR_H_
#define CERES_PUBLIC_NORMAL_PRIOR_H_

#include "ceres/cost_function.h"
#include "ceres/internal/eigen.h"

namespace ceres {

// Implements a cost function of the form
//
//   cost(x) = ||A(x - b)||^2
//
// where, the matrix A and the vector b are fixed and x is the
// variable. In case the user is interested in implementing a cost
// function of the form
//
//   cost(x) = (x - mu)^T S^{-1} (x - mu)
//
// where, mu is a vector and S is a covariance matrix, then, A =
// S^{-1/2}, i.e the matrix A is the square root of the inverse of the
// covariance, also known as the stiffness matrix. There are however
// no restrictions on the shape of A. It is free to be rectangular,
// which would be the case if the covariance matrix S is rank
// deficient.

class NormalPrior: public CostFunction {
 public:
  // Check that the number of rows in the vector b are the same as the
  // number of columns in the matrix A, crash otherwise.
  NormalPrior(const Matrix& A, const Vector& b);

  virtual bool Evaluate(double const* const* parameters,
                        double* residuals,
                        double** jacobians) const;
 private:
  Matrix A_;
  Vector b_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NORMAL_PRIOR_H_
