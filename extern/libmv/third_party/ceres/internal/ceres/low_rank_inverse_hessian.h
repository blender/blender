// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2012 Google Inc. All rights reserved.
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
// Limited memory positive definite approximation to the inverse
// Hessian, using the LBFGS algorithm

#ifndef CERES_INTERNAL_LOW_RANK_INVERSE_HESSIAN_H_
#define CERES_INTERNAL_LOW_RANK_INVERSE_HESSIAN_H_

#include <list>

#include "ceres/internal/eigen.h"
#include "ceres/linear_operator.h"

namespace ceres {
namespace internal {

// LowRankInverseHessian is a positive definite approximation to the
// Hessian using the limited memory variant of the
// Broyden-Fletcher-Goldfarb-Shanno (BFGS)secant formula for
// approximating the Hessian.
//
// Other update rules like the Davidon-Fletcher-Powell (DFP) are
// possible, but the BFGS rule is considered the best performing one.
//
// The limited memory variant was developed by Nocedal and further
// enhanced with scaling rule by Byrd, Nocedal and Schanbel.
//
// Nocedal, J. (1980). "Updating Quasi-Newton Matrices with Limited
// Storage". Mathematics of Computation 35 (151): 773–782.
//
// Byrd, R. H.; Nocedal, J.; Schnabel, R. B. (1994).
// "Representations of Quasi-Newton Matrices and their use in
// Limited Memory Methods". Mathematical Programming 63 (4):
class LowRankInverseHessian : public LinearOperator {
 public:
  // num_parameters is the row/column size of the Hessian.
  // max_num_corrections is the rank of the Hessian approximation.
  // use_approximate_eigenvalue_scaling controls whether the initial
  // inverse Hessian used during Right/LeftMultiply() is scaled by
  // the approximate eigenvalue of the true inverse Hessian at the
  // current operating point.
  // The approximation uses:
  // 2 * max_num_corrections * num_parameters + max_num_corrections
  // doubles.
  LowRankInverseHessian(int num_parameters,
                        int max_num_corrections,
                        bool use_approximate_eigenvalue_scaling);
  virtual ~LowRankInverseHessian() {}

  // Update the low rank approximation. delta_x is the change in the
  // domain of Hessian, and delta_gradient is the change in the
  // gradient.  The update copies the delta_x and delta_gradient
  // vectors, and gets rid of the oldest delta_x and delta_gradient
  // vectors if the number of corrections is already equal to
  // max_num_corrections.
  bool Update(const Vector& delta_x, const Vector& delta_gradient);

  // LinearOperator interface
  virtual void RightMultiply(const double* x, double* y) const;
  virtual void LeftMultiply(const double* x, double* y) const {
    RightMultiply(x, y);
  }
  virtual int num_rows() const { return num_parameters_; }
  virtual int num_cols() const { return num_parameters_; }

 private:
  const int num_parameters_;
  const int max_num_corrections_;
  const bool use_approximate_eigenvalue_scaling_;
  double approximate_eigenvalue_scale_;
  ColMajorMatrix delta_x_history_;
  ColMajorMatrix delta_gradient_history_;
  Vector delta_x_dot_delta_gradient_;
  std::list<int> indices_;
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_INTERNAL_LOW_RANK_INVERSE_HESSIAN_H_
