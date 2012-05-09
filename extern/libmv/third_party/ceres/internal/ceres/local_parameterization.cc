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

#include <glog/logging.h>
#include "ceres/internal/eigen.h"
#include "ceres/local_parameterization.h"
#include "ceres/rotation.h"

namespace ceres {

IdentityParameterization::IdentityParameterization(const int size)
    : size_(size) {
  CHECK_GT(size, 0);
}

bool IdentityParameterization::Plus(const double* x,
                                    const double* delta,
                                    double* x_plus_delta) const {
  VectorRef(x_plus_delta, size_) =
      ConstVectorRef(x, size_) + ConstVectorRef(delta, size_);
  return true;
}

bool IdentityParameterization::ComputeJacobian(const double* x,
                                               double* jacobian) const {
  MatrixRef(jacobian, size_, size_) = Matrix::Identity(size_, size_);
  return true;
}

SubsetParameterization::SubsetParameterization(
    int size,
    const vector<int>& constant_parameters)
    : local_size_(size - constant_parameters.size()),
      constancy_mask_(size, 0) {
  CHECK_GT(constant_parameters.size(), 0)
      << "The set of constant parameters should contain at least "
      << "one element. If you do not wish to hold any parameters "
      << "constant, then do not use a SubsetParameterization";

  vector<int> constant = constant_parameters;
  sort(constant.begin(), constant.end());
  CHECK(unique(constant.begin(), constant.end()) == constant.end())
      << "The set of constant parameters cannot contain duplicates";
  CHECK_LT(constant_parameters.size(), size)
      << "Number of parameters held constant should be less "
      << "than the size of the parameter block. If you wish "
      << "to hold the entire parameter block constant, then a "
      << "efficient way is to directly mark it as constant "
      << "instead of using a LocalParameterization to do so.";
  CHECK_GE(*min_element(constant.begin(), constant.end()), 0);
  CHECK_LT(*max_element(constant.begin(), constant.end()), size);

  for (int i = 0; i < constant_parameters.size(); ++i) {
    constancy_mask_[constant_parameters[i]] = 1;
  }
}

bool SubsetParameterization::Plus(const double* x,
                                  const double* delta,
                                  double* x_plus_delta) const {
  for (int i = 0, j = 0; i < constancy_mask_.size(); ++i) {
    if (constancy_mask_[i]) {
      x_plus_delta[i] = x[i];
    } else {
      x_plus_delta[i] = x[i] + delta[j++];
    }
  }
  return true;
}

bool SubsetParameterization::ComputeJacobian(const double* x,
                                             double* jacobian) const {
  MatrixRef m(jacobian, constancy_mask_.size(), local_size_);
  m.setZero();
  for (int i = 0, j = 0; i < constancy_mask_.size(); ++i) {
    if (!constancy_mask_[i]) {
      m(i, j++) = 1.0;
    }
  }
  return true;
}

bool QuaternionParameterization::Plus(const double* x,
                                      const double* delta,
                                      double* x_plus_delta) const {
  const double norm_delta =
      sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]);
  if (norm_delta > 0.0) {
    const double sin_delta_by_delta = (sin(norm_delta) / norm_delta);
    double q_delta[4];
    q_delta[0] = cos(norm_delta);
    q_delta[1] = sin_delta_by_delta * delta[0];
    q_delta[2] = sin_delta_by_delta * delta[1];
    q_delta[3] = sin_delta_by_delta * delta[2];
    QuaternionProduct(q_delta, x, x_plus_delta);
  } else {
    for (int i = 0; i < 4; ++i) {
      x_plus_delta[i] = x[i];
    }
  }
  return true;
}

bool QuaternionParameterization::ComputeJacobian(const double* x,
                                                 double* jacobian) const {
  jacobian[0] = -x[1]; jacobian[1]  = -x[2]; jacobian[2]  = -x[3];  // NOLINT
  jacobian[3] =  x[0]; jacobian[4]  =  x[3]; jacobian[5]  = -x[2];  // NOLINT
  jacobian[6] = -x[3]; jacobian[7]  =  x[0]; jacobian[8]  =  x[1];  // NOLINT
  jacobian[9] =  x[2]; jacobian[10] = -x[1]; jacobian[11] =  x[0];  // NOLINT
  return true;
}

}  // namespace ceres
