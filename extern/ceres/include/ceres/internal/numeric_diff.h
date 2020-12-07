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
// Author: sameeragarwal@google.com (Sameer Agarwal)
//         mierle@gmail.com (Keir Mierle)
//         tbennun@gmail.com (Tal Ben-Nun)
//
// Finite differencing routines used by NumericDiffCostFunction.

#ifndef CERES_PUBLIC_INTERNAL_NUMERIC_DIFF_H_
#define CERES_PUBLIC_INTERNAL_NUMERIC_DIFF_H_

#include <cstring>
#include <utility>

#include "Eigen/Dense"
#include "Eigen/StdVector"
#include "ceres/cost_function.h"
#include "ceres/internal/fixed_array.h"
#include "ceres/internal/variadic_evaluate.h"
#include "ceres/numeric_diff_options.h"
#include "ceres/types.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

// This is split from the main class because C++ doesn't allow partial template
// specializations for member functions. The alternative is to repeat the main
// class for differing numbers of parameters, which is also unfortunate.
template <typename CostFunctor,
          NumericDiffMethodType kMethod,
          int kNumResiduals,
          typename ParameterDims,
          int kParameterBlock,
          int kParameterBlockSize>
struct NumericDiff {
  // Mutates parameters but must restore them before return.
  static bool EvaluateJacobianForParameterBlock(
      const CostFunctor* functor,
      const double* residuals_at_eval_point,
      const NumericDiffOptions& options,
      int num_residuals,
      int parameter_block_index,
      int parameter_block_size,
      double** parameters,
      double* jacobian) {
    using Eigen::ColMajor;
    using Eigen::Map;
    using Eigen::Matrix;
    using Eigen::RowMajor;

    DCHECK(jacobian);

    const int num_residuals_internal =
        (kNumResiduals != ceres::DYNAMIC ? kNumResiduals : num_residuals);
    const int parameter_block_index_internal =
        (kParameterBlock != ceres::DYNAMIC ? kParameterBlock
                                           : parameter_block_index);
    const int parameter_block_size_internal =
        (kParameterBlockSize != ceres::DYNAMIC ? kParameterBlockSize
                                               : parameter_block_size);

    typedef Matrix<double, kNumResiduals, 1> ResidualVector;
    typedef Matrix<double, kParameterBlockSize, 1> ParameterVector;

    // The convoluted reasoning for choosing the Row/Column major
    // ordering of the matrix is an artifact of the restrictions in
    // Eigen that prevent it from creating RowMajor matrices with a
    // single column. In these cases, we ask for a ColMajor matrix.
    typedef Matrix<double,
                   kNumResiduals,
                   kParameterBlockSize,
                   (kParameterBlockSize == 1) ? ColMajor : RowMajor>
        JacobianMatrix;

    Map<JacobianMatrix> parameter_jacobian(
        jacobian, num_residuals_internal, parameter_block_size_internal);

    Map<ParameterVector> x_plus_delta(
        parameters[parameter_block_index_internal],
        parameter_block_size_internal);
    ParameterVector x(x_plus_delta);
    ParameterVector step_size =
        x.array().abs() * ((kMethod == RIDDERS)
                               ? options.ridders_relative_initial_step_size
                               : options.relative_step_size);

    // It is not a good idea to make the step size arbitrarily
    // small. This will lead to problems with round off and numerical
    // instability when dividing by the step size. The general
    // recommendation is to not go down below sqrt(epsilon).
    double min_step_size = std::sqrt(std::numeric_limits<double>::epsilon());

    // For Ridders' method, the initial step size is required to be large,
    // thus ridders_relative_initial_step_size is used.
    if (kMethod == RIDDERS) {
      min_step_size =
          std::max(min_step_size, options.ridders_relative_initial_step_size);
    }

    // For each parameter in the parameter block, use finite differences to
    // compute the derivative for that parameter.
    FixedArray<double> temp_residual_array(num_residuals_internal);
    FixedArray<double> residual_array(num_residuals_internal);
    Map<ResidualVector> residuals(residual_array.data(),
                                  num_residuals_internal);

    for (int j = 0; j < parameter_block_size_internal; ++j) {
      const double delta = std::max(min_step_size, step_size(j));

      if (kMethod == RIDDERS) {
        if (!EvaluateRiddersJacobianColumn(functor,
                                           j,
                                           delta,
                                           options,
                                           num_residuals_internal,
                                           parameter_block_size_internal,
                                           x.data(),
                                           residuals_at_eval_point,
                                           parameters,
                                           x_plus_delta.data(),
                                           temp_residual_array.data(),
                                           residual_array.data())) {
          return false;
        }
      } else {
        if (!EvaluateJacobianColumn(functor,
                                    j,
                                    delta,
                                    num_residuals_internal,
                                    parameter_block_size_internal,
                                    x.data(),
                                    residuals_at_eval_point,
                                    parameters,
                                    x_plus_delta.data(),
                                    temp_residual_array.data(),
                                    residual_array.data())) {
          return false;
        }
      }

      parameter_jacobian.col(j).matrix() = residuals;
    }
    return true;
  }

  static bool EvaluateJacobianColumn(const CostFunctor* functor,
                                     int parameter_index,
                                     double delta,
                                     int num_residuals,
                                     int parameter_block_size,
                                     const double* x_ptr,
                                     const double* residuals_at_eval_point,
                                     double** parameters,
                                     double* x_plus_delta_ptr,
                                     double* temp_residuals_ptr,
                                     double* residuals_ptr) {
    using Eigen::Map;
    using Eigen::Matrix;

    typedef Matrix<double, kNumResiduals, 1> ResidualVector;
    typedef Matrix<double, kParameterBlockSize, 1> ParameterVector;

    Map<const ParameterVector> x(x_ptr, parameter_block_size);
    Map<ParameterVector> x_plus_delta(x_plus_delta_ptr, parameter_block_size);

    Map<ResidualVector> residuals(residuals_ptr, num_residuals);
    Map<ResidualVector> temp_residuals(temp_residuals_ptr, num_residuals);

    // Mutate 1 element at a time and then restore.
    x_plus_delta(parameter_index) = x(parameter_index) + delta;

    if (!VariadicEvaluate<ParameterDims>(
            *functor, parameters, residuals.data())) {
      return false;
    }

    // Compute this column of the jacobian in 3 steps:
    // 1. Store residuals for the forward part.
    // 2. Subtract residuals for the backward (or 0) part.
    // 3. Divide out the run.
    double one_over_delta = 1.0 / delta;
    if (kMethod == CENTRAL || kMethod == RIDDERS) {
      // Compute the function on the other side of x(parameter_index).
      x_plus_delta(parameter_index) = x(parameter_index) - delta;

      if (!VariadicEvaluate<ParameterDims>(
              *functor, parameters, temp_residuals.data())) {
        return false;
      }

      residuals -= temp_residuals;
      one_over_delta /= 2;
    } else {
      // Forward difference only; reuse existing residuals evaluation.
      residuals -=
          Map<const ResidualVector>(residuals_at_eval_point, num_residuals);
    }

    // Restore x_plus_delta.
    x_plus_delta(parameter_index) = x(parameter_index);

    // Divide out the run to get slope.
    residuals *= one_over_delta;

    return true;
  }

  // This numeric difference implementation uses adaptive differentiation
  // on the parameters to obtain the Jacobian matrix. The adaptive algorithm
  // is based on Ridders' method for adaptive differentiation, which creates
  // a Romberg tableau from varying step sizes and extrapolates the
  // intermediate results to obtain the current computational error.
  //
  // References:
  // C.J.F. Ridders, Accurate computation of F'(x) and F'(x) F"(x), Advances
  // in Engineering Software (1978), Volume 4, Issue 2, April 1982,
  // Pages 75-76, ISSN 0141-1195,
  // http://dx.doi.org/10.1016/S0141-1195(82)80057-0.
  static bool EvaluateRiddersJacobianColumn(
      const CostFunctor* functor,
      int parameter_index,
      double delta,
      const NumericDiffOptions& options,
      int num_residuals,
      int parameter_block_size,
      const double* x_ptr,
      const double* residuals_at_eval_point,
      double** parameters,
      double* x_plus_delta_ptr,
      double* temp_residuals_ptr,
      double* residuals_ptr) {
    using Eigen::aligned_allocator;
    using Eigen::Map;
    using Eigen::Matrix;

    typedef Matrix<double, kNumResiduals, 1> ResidualVector;
    typedef Matrix<double, kNumResiduals, Eigen::Dynamic>
        ResidualCandidateMatrix;
    typedef Matrix<double, kParameterBlockSize, 1> ParameterVector;

    Map<const ParameterVector> x(x_ptr, parameter_block_size);
    Map<ParameterVector> x_plus_delta(x_plus_delta_ptr, parameter_block_size);

    Map<ResidualVector> residuals(residuals_ptr, num_residuals);
    Map<ResidualVector> temp_residuals(temp_residuals_ptr, num_residuals);

    // In order for the algorithm to converge, the step size should be
    // initialized to a value that is large enough to produce a significant
    // change in the function.
    // As the derivative is estimated, the step size decreases.
    // By default, the step sizes are chosen so that the middle column
    // of the Romberg tableau uses the input delta.
    double current_step_size =
        delta * pow(options.ridders_step_shrink_factor,
                    options.max_num_ridders_extrapolations / 2);

    // Double-buffering temporary differential candidate vectors
    // from previous step size.
    ResidualCandidateMatrix stepsize_candidates_a(
        num_residuals, options.max_num_ridders_extrapolations);
    ResidualCandidateMatrix stepsize_candidates_b(
        num_residuals, options.max_num_ridders_extrapolations);
    ResidualCandidateMatrix* current_candidates = &stepsize_candidates_a;
    ResidualCandidateMatrix* previous_candidates = &stepsize_candidates_b;

    // Represents the computational error of the derivative. This variable is
    // initially set to a large value, and is set to the difference between
    // current and previous finite difference extrapolations.
    // norm_error is supposed to decrease as the finite difference tableau
    // generation progresses, serving both as an estimate for differentiation
    // error and as a measure of differentiation numerical stability.
    double norm_error = std::numeric_limits<double>::max();

    // Loop over decreasing step sizes until:
    //  1. Error is smaller than a given value (ridders_epsilon),
    //  2. Maximal order of extrapolation reached, or
    //  3. Extrapolation becomes numerically unstable.
    for (int i = 0; i < options.max_num_ridders_extrapolations; ++i) {
      // Compute the numerical derivative at this step size.
      if (!EvaluateJacobianColumn(functor,
                                  parameter_index,
                                  current_step_size,
                                  num_residuals,
                                  parameter_block_size,
                                  x.data(),
                                  residuals_at_eval_point,
                                  parameters,
                                  x_plus_delta.data(),
                                  temp_residuals.data(),
                                  current_candidates->col(0).data())) {
        // Something went wrong; bail.
        return false;
      }

      // Store initial results.
      if (i == 0) {
        residuals = current_candidates->col(0);
      }

      // Shrink differentiation step size.
      current_step_size /= options.ridders_step_shrink_factor;

      // Extrapolation factor for Richardson acceleration method (see below).
      double richardson_factor = options.ridders_step_shrink_factor *
                                 options.ridders_step_shrink_factor;
      for (int k = 1; k <= i; ++k) {
        // Extrapolate the various orders of finite differences using
        // the Richardson acceleration method.
        current_candidates->col(k) =
            (richardson_factor * current_candidates->col(k - 1) -
             previous_candidates->col(k - 1)) /
            (richardson_factor - 1.0);

        richardson_factor *= options.ridders_step_shrink_factor *
                             options.ridders_step_shrink_factor;

        // Compute the difference between the previous value and the current.
        double candidate_error = std::max(
            (current_candidates->col(k) - current_candidates->col(k - 1))
                .norm(),
            (current_candidates->col(k) - previous_candidates->col(k - 1))
                .norm());

        // If the error has decreased, update results.
        if (candidate_error <= norm_error) {
          norm_error = candidate_error;
          residuals = current_candidates->col(k);

          // If the error is small enough, stop.
          if (norm_error < options.ridders_epsilon) {
            break;
          }
        }
      }

      // After breaking out of the inner loop, declare convergence.
      if (norm_error < options.ridders_epsilon) {
        break;
      }

      // Check to see if the current gradient estimate is numerically unstable.
      // If so, bail out and return the last stable result.
      if (i > 0) {
        double tableau_error =
            (current_candidates->col(i) - previous_candidates->col(i - 1))
                .norm();

        // Compare current error to the chosen candidate's error.
        if (tableau_error >= 2 * norm_error) {
          break;
        }
      }

      std::swap(current_candidates, previous_candidates);
    }
    return true;
  }
};

// This function calls NumericDiff<...>::EvaluateJacobianForParameterBlock for
// each parameter block.
//
// Example:
// A call to
// EvaluateJacobianForParameterBlocks<StaticParameterDims<2, 3>>(
//        functor,
//        residuals_at_eval_point,
//        options,
//        num_residuals,
//        parameters,
//        jacobians);
// will result in the following calls to
// NumericDiff<...>::EvaluateJacobianForParameterBlock:
//
// if (jacobians[0] != nullptr) {
//   if (!NumericDiff<
//           CostFunctor,
//           method,
//           kNumResiduals,
//           StaticParameterDims<2, 3>,
//           0,
//           2>::EvaluateJacobianForParameterBlock(functor,
//                                                 residuals_at_eval_point,
//                                                 options,
//                                                 num_residuals,
//                                                 0,
//                                                 2,
//                                                 parameters,
//                                                 jacobians[0])) {
//     return false;
//   }
// }
// if (jacobians[1] != nullptr) {
//   if (!NumericDiff<
//           CostFunctor,
//           method,
//           kNumResiduals,
//           StaticParameterDims<2, 3>,
//           1,
//           3>::EvaluateJacobianForParameterBlock(functor,
//                                                 residuals_at_eval_point,
//                                                 options,
//                                                 num_residuals,
//                                                 1,
//                                                 3,
//                                                 parameters,
//                                                 jacobians[1])) {
//     return false;
//   }
// }
template <typename ParameterDims,
          typename Parameters = typename ParameterDims::Parameters,
          int ParameterIdx = 0>
struct EvaluateJacobianForParameterBlocks;

template <typename ParameterDims, int N, int... Ns, int ParameterIdx>
struct EvaluateJacobianForParameterBlocks<ParameterDims,
                                          std::integer_sequence<int, N, Ns...>,
                                          ParameterIdx> {
  template <NumericDiffMethodType method,
            int kNumResiduals,
            typename CostFunctor>
  static bool Apply(const CostFunctor* functor,
                    const double* residuals_at_eval_point,
                    const NumericDiffOptions& options,
                    int num_residuals,
                    double** parameters,
                    double** jacobians) {
    if (jacobians[ParameterIdx] != nullptr) {
      if (!NumericDiff<
              CostFunctor,
              method,
              kNumResiduals,
              ParameterDims,
              ParameterIdx,
              N>::EvaluateJacobianForParameterBlock(functor,
                                                    residuals_at_eval_point,
                                                    options,
                                                    num_residuals,
                                                    ParameterIdx,
                                                    N,
                                                    parameters,
                                                    jacobians[ParameterIdx])) {
        return false;
      }
    }

    return EvaluateJacobianForParameterBlocks<ParameterDims,
                                              std::integer_sequence<int, Ns...>,
                                              ParameterIdx + 1>::
        template Apply<method, kNumResiduals>(functor,
                                              residuals_at_eval_point,
                                              options,
                                              num_residuals,
                                              parameters,
                                              jacobians);
  }
};

// End of 'recursion'. Nothing more to do.
template <typename ParameterDims, int ParameterIdx>
struct EvaluateJacobianForParameterBlocks<ParameterDims,
                                          std::integer_sequence<int>,
                                          ParameterIdx> {
  template <NumericDiffMethodType method,
            int kNumResiduals,
            typename CostFunctor>
  static bool Apply(const CostFunctor* /* NOT USED*/,
                    const double* /* NOT USED*/,
                    const NumericDiffOptions& /* NOT USED*/,
                    int /* NOT USED*/,
                    double** /* NOT USED*/,
                    double** /* NOT USED*/) {
    return true;
  }
};

}  // namespace internal
}  // namespace ceres

#endif  // CERES_PUBLIC_INTERNAL_NUMERIC_DIFF_H_
