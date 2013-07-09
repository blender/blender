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
//
// A wrapper class that takes a variadic functor evaluating a
// function, numerically differentiates it and makes it available as a
// templated functor so that it can be easily used as part of Ceres'
// automatic differentiation framework.
//
// For example:
//
// For example, let us assume that
//
//  struct IntrinsicProjection
//    IntrinsicProjection(const double* observations);
//    bool operator()(const double* calibration,
//                    const double* point,
//                    double* residuals);
//  };
//
// is a functor that implements the projection of a point in its local
// coordinate system onto its image plane and subtracts it from the
// observed point projection.
//
// Now we would like to compose the action of this functor with the
// action of camera extrinsics, i.e., rotation and translation, which
// is given by the following templated function
//
//   template<typename T>
//   void RotateAndTranslatePoint(const T* rotation,
//                                const T* translation,
//                                const T* point,
//                                T* result);
//
// To compose the extrinsics and intrinsics, we can construct a
// CameraProjection functor as follows.
//
// struct CameraProjection {
//    typedef NumericDiffFunctor<IntrinsicProjection, CENTRAL, 2, 5, 3>
//       IntrinsicProjectionFunctor;
//
//   CameraProjection(double* observation) {
//     intrinsic_projection_.reset(
//         new IntrinsicProjectionFunctor(observation)) {
//   }
//
//   template <typename T>
//   bool operator()(const T* rotation,
//                   const T* translation,
//                   const T* intrinsics,
//                   const T* point,
//                   T* residuals) const {
//     T transformed_point[3];
//     RotateAndTranslatePoint(rotation, translation, point, transformed_point);
//     return (*intrinsic_projection_)(intrinsics, transformed_point, residual);
//   }
//
//  private:
//   scoped_ptr<IntrinsicProjectionFunctor> intrinsic_projection_;
// };
//
// Here, we made the choice of using CENTRAL differences to compute
// the jacobian of IntrinsicProjection.
//
// Now, we are ready to construct an automatically differentiated cost
// function as
//
// CostFunction* cost_function =
//    new AutoDiffCostFunction<CameraProjection, 2, 3, 3, 5>(
//        new CameraProjection(observations));
//
// cost_function now seamlessly integrates automatic differentiation
// of RotateAndTranslatePoint with a numerically differentiated
// version of IntrinsicProjection.

#ifndef CERES_PUBLIC_NUMERIC_DIFF_FUNCTOR_H_
#define CERES_PUBLIC_NUMERIC_DIFF_FUNCTOR_H_

#include "ceres/numeric_diff_cost_function.h"
#include "ceres/types.h"
#include "ceres/cost_function_to_functor.h"

namespace ceres {

template<typename Functor,
         NumericDiffMethod kMethod = CENTRAL,
         int kNumResiduals = 0,
         int N0 = 0, int N1 = 0 , int N2 = 0, int N3 = 0, int N4 = 0,
         int N5 = 0, int N6 = 0 , int N7 = 0, int N8 = 0, int N9 = 0>
class NumericDiffFunctor {
 public:
  // relative_step_size controls the step size used by the numeric
  // differentiation process.
  explicit NumericDiffFunctor(double relative_step_size = 1e-6)
      : functor_(
          new NumericDiffCostFunction<Functor,
                                      kMethod,
                                      kNumResiduals,
                                      N0, N1, N2, N3, N4,
                                      N5, N6, N7, N8, N9>(new Functor,
                                                          relative_step_size)) {
  }

  NumericDiffFunctor(Functor* functor, double relative_step_size = 1e-6)
      : functor_(new NumericDiffCostFunction<Functor,
                                             kMethod,
                                             kNumResiduals,
                                             N0, N1, N2, N3, N4,
                                             N5, N6, N7, N8, N9>(
                                                 functor, relative_step_size)) {
  }

  bool operator()(const double* x0, double* residuals) const {
    return functor_(x0, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  double* residuals) const {
    return functor_(x0, x1, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  double* residuals) const {
    return functor_(x0, x1, x2, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  const double* x5,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  const double* x5,
                  const double* x6,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  const double* x5,
                  const double* x6,
                  const double* x7,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  const double* x5,
                  const double* x6,
                  const double* x7,
                  const double* x8,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, x8, residuals);
  }

  bool operator()(const double* x0,
                  const double* x1,
                  const double* x2,
                  const double* x3,
                  const double* x4,
                  const double* x5,
                  const double* x6,
                  const double* x7,
                  const double* x8,
                  const double* x9,
                  double* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, residuals);
  }

  template <typename T>
  bool operator()(const T* x0, T* residuals) const {
    return functor_(x0, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  T* residuals) const {
    return functor_(x0, x1, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  T* residuals) const {
    return functor_(x0, x1, x2, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  const T* x5,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  const T* x5,
                  const T* x6,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  const T* x5,
                  const T* x6,
                  const T* x7,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  const T* x5,
                  const T* x6,
                  const T* x7,
                  const T* x8,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, x8, residuals);
  }

  template <typename T>
  bool operator()(const T* x0,
                  const T* x1,
                  const T* x2,
                  const T* x3,
                  const T* x4,
                  const T* x5,
                  const T* x6,
                  const T* x7,
                  const T* x8,
                  const T* x9,
                  T* residuals) const {
    return functor_(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, residuals);
  }


 private:
  CostFunctionToFunctor<kNumResiduals,
                        N0, N1, N2, N3, N4,
                        N5, N6, N7, N8, N9> functor_;
};

}  // namespace ceres

#endif  // CERES_PUBLIC_NUMERIC_DIFF_FUNCTOR_H_
