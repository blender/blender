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
// Author: sergey.vfx@gmail.com (Sergey Sharybin)
//         mierle@gmail.com (Keir Mierle)
//         sameeragarwal@google.com (Sameer Agarwal)

#ifndef CERES_PUBLIC_AUTODIFF_LOCAL_PARAMETERIZATION_H_
#define CERES_PUBLIC_AUTODIFF_LOCAL_PARAMETERIZATION_H_

#include "ceres/internal/autodiff.h"
#include "ceres/internal/scoped_ptr.h"
#include "ceres/local_parameterization.h"

namespace ceres {

// Create local parameterization with Jacobians computed via automatic
// differentiation. For more information on local parameterizations,
// see include/ceres/local_parameterization.h
//
// To get an auto differentiated local parameterization, you must define
// a class with a templated operator() (a functor) that computes
//
//   x_plus_delta = Plus(x, delta);
//
// the template parameter T. The autodiff framework substitutes appropriate
// "Jet" objects for T in order to compute the derivative when necessary, but
// this is hidden, and you should write the function as if T were a scalar type
// (e.g. a double-precision floating point number).
//
// The function must write the computed value in the last argument (the only
// non-const one) and return true to indicate success.
//
// For example, Quaternions have a three dimensional local
// parameterization. It's plus operation can be implemented as (taken
// from internal/ceres/auto_diff_local_parameterization_test.cc)
//
//   struct QuaternionPlus {
//     template<typename T>
//     bool operator()(const T* x, const T* delta, T* x_plus_delta) const {
//       const T squared_norm_delta =
//           delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2];
//
//       T q_delta[4];
//       if (squared_norm_delta > T(0.0)) {
//         T norm_delta = sqrt(squared_norm_delta);
//         const T sin_delta_by_delta = sin(norm_delta) / norm_delta;
//         q_delta[0] = cos(norm_delta);
//         q_delta[1] = sin_delta_by_delta * delta[0];
//         q_delta[2] = sin_delta_by_delta * delta[1];
//         q_delta[3] = sin_delta_by_delta * delta[2];
//       } else {
//         // We do not just use q_delta = [1,0,0,0] here because that is a
//         // constant and when used for automatic differentiation will
//         // lead to a zero derivative. Instead we take a first order
//         // approximation and evaluate it at zero.
//         q_delta[0] = T(1.0);
//         q_delta[1] = delta[0];
//         q_delta[2] = delta[1];
//         q_delta[3] = delta[2];
//       }
//
//       QuaternionProduct(q_delta, x, x_plus_delta);
//       return true;
//     }
//   };
//
// Then given this struct, the auto differentiated local
// parameterization can now be constructed as
//
//   LocalParameterization* local_parameterization =
//     new AutoDiffLocalParameterization<QuaternionPlus, 4, 3>;
//                                                       |  |
//                            Global Size ---------------+  |
//                            Local Size -------------------+
//
// WARNING: Since the functor will get instantiated with different types for
// T, you must to convert from other numeric types to T before mixing
// computations with other variables of type T. In the example above, this is
// seen where instead of using k_ directly, k_ is wrapped with T(k_).

template <typename Functor, int kGlobalSize, int kLocalSize>
class AutoDiffLocalParameterization : public LocalParameterization {
 public:
  AutoDiffLocalParameterization() :
      functor_(new Functor()) {}

  // Takes ownership of functor.
  explicit AutoDiffLocalParameterization(Functor* functor) :
      functor_(functor) {}

  virtual ~AutoDiffLocalParameterization() {}
  virtual bool Plus(const double* x,
                    const double* delta,
                    double* x_plus_delta) const {
    return (*functor_)(x, delta, x_plus_delta);
  }

  virtual bool ComputeJacobian(const double* x, double* jacobian) const {
    double zero_delta[kLocalSize];
    for (int i = 0; i < kLocalSize; ++i) {
      zero_delta[i] = 0.0;
    }

    double x_plus_delta[kGlobalSize];
    for (int i = 0; i < kGlobalSize; ++i) {
      x_plus_delta[i] = 0.0;
    }

    const double* parameter_ptrs[2] = {x, zero_delta};
    double* jacobian_ptrs[2] = { NULL, jacobian };
    return internal::AutoDiff<Functor, double, kGlobalSize, kLocalSize>
        ::Differentiate(*functor_,
                        parameter_ptrs,
                        kGlobalSize,
                        x_plus_delta,
                        jacobian_ptrs);
  }

  virtual int GlobalSize() const { return kGlobalSize; }
  virtual int LocalSize() const { return kLocalSize; }

 private:
  internal::scoped_ptr<Functor> functor_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_AUTODIFF_LOCAL_PARAMETERIZATION_H_
