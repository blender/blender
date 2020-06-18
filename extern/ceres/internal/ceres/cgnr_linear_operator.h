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
// Author: keir@google.com (Keir Mierle)

#ifndef CERES_INTERNAL_CGNR_LINEAR_OPERATOR_H_
#define CERES_INTERNAL_CGNR_LINEAR_OPERATOR_H_

#include <algorithm>
#include <memory>
#include "ceres/linear_operator.h"
#include "ceres/internal/eigen.h"

namespace ceres {
namespace internal {

class SparseMatrix;

// A linear operator which takes a matrix A and a diagonal vector D and
// performs products of the form
//
//   (A^T A + D^T D)x
//
// This is used to implement iterative general sparse linear solving with
// conjugate gradients, where A is the Jacobian and D is a regularizing
// parameter. A brief proof that D^T D is the correct regularizer:
//
// Given a regularized least squares problem:
//
//   min  ||Ax - b||^2 + ||Dx||^2
//    x
//
// First expand into matrix notation:
//
//   (Ax - b)^T (Ax - b) + xD^TDx
//
// Then multiply out to get:
//
//   = xA^TAx - 2b^T Ax + b^Tb + xD^TDx
//
// Take the derivative:
//
//   0 = 2A^TAx - 2A^T b + 2 D^TDx
//   0 = A^TAx - A^T b + D^TDx
//   0 = (A^TA + D^TD)x - A^T b
//
// Thus, the symmetric system we need to solve for CGNR is
//
//   Sx = z
//
// with S = A^TA + D^TD
//  and z = A^T b
//
// Note: This class is not thread safe, since it uses some temporary storage.
class CgnrLinearOperator : public LinearOperator {
 public:
  CgnrLinearOperator(const LinearOperator& A, const double *D)
      : A_(A), D_(D), z_(new double[A.num_rows()]) {
  }
  virtual ~CgnrLinearOperator() {}

  void RightMultiply(const double* x, double* y) const final {
    std::fill(z_.get(), z_.get() + A_.num_rows(), 0.0);

    // z = Ax
    A_.RightMultiply(x, z_.get());

    // y = y + Atz
    A_.LeftMultiply(z_.get(), y);

    // y = y + DtDx
    if (D_ != NULL) {
      int n = A_.num_cols();
      VectorRef(y, n).array() += ConstVectorRef(D_, n).array().square() *
                                 ConstVectorRef(x, n).array();
    }
  }

  void LeftMultiply(const double* x, double* y) const final {
    RightMultiply(x, y);
  }

  int num_rows() const final { return A_.num_cols(); }
  int num_cols() const final { return A_.num_cols(); }

 private:
  const LinearOperator& A_;
  const double* D_;
  std::unique_ptr<double[]> z_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_CGNR_LINEAR_OPERATOR_H_
