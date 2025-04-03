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

#ifndef CERES_PUBLIC_AUTODIFF_FIRST_ORDER_FUNCTION_H_
#define CERES_PUBLIC_AUTODIFF_FIRST_ORDER_FUNCTION_H_

#include <memory>

#include "ceres/first_order_function.h"
#include "ceres/internal/eigen.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/jet.h"
#include "ceres/types.h"

namespace ceres {

// Create FirstOrderFunctions as needed by the GradientProblem
// framework, with gradients computed via automatic
// differentiation. For more information on automatic differentiation,
// see the wikipedia article at
// http://en.wikipedia.org/wiki/Automatic_differentiation
//
// To get an auto differentiated function, you must define a class
// with a templated operator() (a functor) that computes the cost
// function in terms of the template parameter T. The autodiff
// framework substitutes appropriate "jet" objects for T in order to
// compute the derivative when necessary, but this is hidden, and you
// should write the function as if T were a scalar type (e.g. a
// double-precision floating point number).
//
// The function must write the computed value in the last argument
// (the only non-const one) and return true to indicate
// success.
//
// For example, consider a scalar error e = x'y - a, where both x and y are
// two-dimensional column vector parameters, the prime sign indicates
// transposition, and a is a constant.
//
// To write an auto-differentiable FirstOrderFunction for the above model, first
// define the object
//
//  class QuadraticCostFunctor {
//   public:
//    explicit QuadraticCostFunctor(double a) : a_(a) {}
//    template <typename T>
//    bool operator()(const T* const xy, T* cost) const {
//      const T* const x = xy;
//      const T* const y = xy + 2;
//      *cost = x[0] * y[0] + x[1] * y[1] - T(a_);
//      return true;
//    }
//
//   private:
//    double a_;
//  };
//
// Note that in the declaration of operator() the input parameters xy come
// first, and are passed as const pointers to arrays of T. The
// output is the last parameter.
//
// Then given this class definition, the auto differentiated FirstOrderFunction
// for it can be constructed as follows.
//
//    FirstOrderFunction* function =
//      new AutoDiffFirstOrderFunction<QuadraticCostFunctor, 4>(
//          new QuadraticCostFunctor(1.0)));
//
// In the instantiation above, the template parameters following
// "QuadraticCostFunctor", "4", describe the functor as computing a
// 1-dimensional output from a four dimensional vector.
//
// WARNING: Since the functor will get instantiated with different types for
// T, you must convert from other numeric types to T before mixing
// computations with other variables of type T. In the example above, this is
// seen where instead of using a_ directly, a_ is wrapped with T(a_).

template <typename FirstOrderFunctor, int kNumParameters>
class AutoDiffFirstOrderFunction final : public FirstOrderFunction {
 public:
  // Takes ownership of functor.
  explicit AutoDiffFirstOrderFunction(FirstOrderFunctor* functor)
      : functor_(functor) {
    static_assert(kNumParameters > 0, "kNumParameters must be positive");
  }

  bool Evaluate(const double* const parameters,
                double* cost,
                double* gradient) const override {
    if (gradient == nullptr) {
      return (*functor_)(parameters, cost);
    }

    using JetT = Jet<double, kNumParameters>;
    internal::FixedArray<JetT, (256 * 7) / sizeof(JetT)> x(kNumParameters);
    for (int i = 0; i < kNumParameters; ++i) {
      x[i].a = parameters[i];
      x[i].v.setZero();
      x[i].v[i] = 1.0;
    }

    JetT output;
    output.a = kImpossibleValue;
    output.v.setConstant(kImpossibleValue);

    if (!(*functor_)(x.data(), &output)) {
      return false;
    }

    *cost = output.a;
    VectorRef(gradient, kNumParameters) = output.v;
    return true;
  }

  int NumParameters() const override { return kNumParameters; }

  const FirstOrderFunctor& functor() const { return *functor_; }

 private:
  std::unique_ptr<FirstOrderFunctor> functor_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_AUTODIFF_FIRST_ORDER_FUNCTION_H_
