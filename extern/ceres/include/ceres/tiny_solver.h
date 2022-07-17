// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2021 Google Inc. All rights reserved.
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
// Author: mierle@gmail.com (Keir Mierle)
//
// WARNING WARNING WARNING
// WARNING WARNING WARNING  Tiny solver is experimental and will change.
// WARNING WARNING WARNING
//
// A tiny least squares solver using Levenberg-Marquardt, intended for solving
// small dense problems with low latency and low overhead. The implementation
// takes care to do all allocation up front, so that no memory is allocated
// during solving. This is especially useful when solving many similar problems;
// for example, inverse pixel distortion for every pixel on a grid.
//
// Note: This code has no dependencies beyond Eigen, including on other parts of
// Ceres, so it is possible to take this file alone and put it in another
// project without the rest of Ceres.
//
// Algorithm based off of:
//
// [1] K. Madsen, H. Nielsen, O. Tingleoff.
//     Methods for Non-linear Least Squares Problems.
//     http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf

#ifndef CERES_PUBLIC_TINY_SOLVER_H_
#define CERES_PUBLIC_TINY_SOLVER_H_

#include <cassert>
#include <cmath>

#include "Eigen/Dense"

namespace ceres {

// To use tiny solver, create a class or struct that allows computing the cost
// function (described below). This is similar to a ceres::CostFunction, but is
// different to enable statically allocating all memory for the solver
// (specifically, enum sizes). Key parts are the Scalar typedef, the enums to
// describe problem sizes (needed to remove all heap allocations), and the
// operator() overload to evaluate the cost and (optionally) jacobians.
//
//   struct TinySolverCostFunctionTraits {
//     typedef double Scalar;
//     enum {
//       NUM_RESIDUALS = <int> OR Eigen::Dynamic,
//       NUM_PARAMETERS = <int> OR Eigen::Dynamic,
//     };
//     bool operator()(const double* parameters,
//                     double* residuals,
//                     double* jacobian) const;
//
//     int NumResiduals() const;  -- Needed if NUM_RESIDUALS == Eigen::Dynamic.
//     int NumParameters() const; -- Needed if NUM_PARAMETERS == Eigen::Dynamic.
//   };
//
// For operator(), the size of the objects is:
//
//   double* parameters -- NUM_PARAMETERS or NumParameters()
//   double* residuals  -- NUM_RESIDUALS or NumResiduals()
//   double* jacobian   -- NUM_RESIDUALS * NUM_PARAMETERS in column-major format
//                         (Eigen's default); or nullptr if no jacobian
//                         requested.
//
// An example (fully statically sized):
//
//   struct MyCostFunctionExample {
//     typedef double Scalar;
//     enum {
//       NUM_RESIDUALS = 2,
//       NUM_PARAMETERS = 3,
//     };
//     bool operator()(const double* parameters,
//                     double* residuals,
//                     double* jacobian) const {
//       residuals[0] = x + 2*y + 4*z;
//       residuals[1] = y * z;
//       if (jacobian) {
//         jacobian[0 * 2 + 0] = 1;   // First column (x).
//         jacobian[0 * 2 + 1] = 0;
//
//         jacobian[1 * 2 + 0] = 2;   // Second column (y).
//         jacobian[1 * 2 + 1] = z;
//
//         jacobian[2 * 2 + 0] = 4;   // Third column (z).
//         jacobian[2 * 2 + 1] = y;
//       }
//       return true;
//     }
//   };
//
// The solver supports either statically or dynamically sized cost
// functions. If the number of residuals is dynamic then the Function
// must define:
//
//   int NumResiduals() const;
//
// If the number of parameters is dynamic then the Function must
// define:
//
//   int NumParameters() const;
//
template <typename Function,
          typename LinearSolver =
              Eigen::LDLT<Eigen::Matrix<typename Function::Scalar,  //
                                        Function::NUM_PARAMETERS,   //
                                        Function::NUM_PARAMETERS>>>
class TinySolver {
 public:
  // This class needs to have an Eigen aligned operator new as it contains
  // fixed-size Eigen types.
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  enum {
    NUM_RESIDUALS = Function::NUM_RESIDUALS,
    NUM_PARAMETERS = Function::NUM_PARAMETERS
  };
  using Scalar = typename Function::Scalar;
  using Parameters = typename Eigen::Matrix<Scalar, NUM_PARAMETERS, 1>;

  enum Status {
    // max_norm |J'(x) * f(x)| < gradient_tolerance
    GRADIENT_TOO_SMALL,
    //  ||dx|| <= parameter_tolerance * (||x|| + parameter_tolerance)
    RELATIVE_STEP_SIZE_TOO_SMALL,
    // cost_threshold > ||f(x)||^2 / 2
    COST_TOO_SMALL,
    // num_iterations >= max_num_iterations
    HIT_MAX_ITERATIONS,
    // (new_cost - old_cost) < function_tolerance * old_cost
    COST_CHANGE_TOO_SMALL,

    // TODO(sameeragarwal): Deal with numerical failures.
  };

  struct Options {
    int max_num_iterations = 50;

    // max_norm |J'(x) * f(x)| < gradient_tolerance
    Scalar gradient_tolerance = 1e-10;

    //  ||dx|| <= parameter_tolerance * (||x|| + parameter_tolerance)
    Scalar parameter_tolerance = 1e-8;

    // (new_cost - old_cost) < function_tolerance * old_cost
    Scalar function_tolerance = 1e-6;

    // cost_threshold > ||f(x)||^2 / 2
    Scalar cost_threshold = std::numeric_limits<Scalar>::epsilon();

    Scalar initial_trust_region_radius = 1e4;
  };

  struct Summary {
    // 1/2 ||f(x_0)||^2
    Scalar initial_cost = -1;
    // 1/2 ||f(x)||^2
    Scalar final_cost = -1;
    // max_norm(J'f(x))
    Scalar gradient_max_norm = -1;
    int iterations = -1;
    Status status = HIT_MAX_ITERATIONS;
  };

  bool Update(const Function& function, const Parameters& x) {
    if (!function(x.data(), residuals_.data(), jacobian_.data())) {
      return false;
    }

    residuals_ = -residuals_;

    // On the first iteration, compute a diagonal (Jacobi) scaling
    // matrix, which we store as a vector.
    if (summary.iterations == 0) {
      // jacobi_scaling = 1 / (1 + diagonal(J'J))
      //
      // 1 is added to the denominator to regularize small diagonal
      // entries.
      jacobi_scaling_ = 1.0 / (1.0 + jacobian_.colwise().norm().array());
    }

    // This explicitly computes the normal equations, which is numerically
    // unstable. Nevertheless, it is often good enough and is fast.
    //
    // TODO(sameeragarwal): Refactor this to allow for DenseQR
    // factorization.
    jacobian_ = jacobian_ * jacobi_scaling_.asDiagonal();
    jtj_ = jacobian_.transpose() * jacobian_;
    g_ = jacobian_.transpose() * residuals_;
    summary.gradient_max_norm = g_.array().abs().maxCoeff();
    cost_ = residuals_.squaredNorm() / 2;
    return true;
  }

  const Summary& Solve(const Function& function, Parameters* x_and_min) {
    Initialize<NUM_RESIDUALS, NUM_PARAMETERS>(function);
    assert(x_and_min);
    Parameters& x = *x_and_min;
    summary = Summary();
    summary.iterations = 0;

    // TODO(sameeragarwal): Deal with failure here.
    Update(function, x);
    summary.initial_cost = cost_;
    summary.final_cost = cost_;

    if (summary.gradient_max_norm < options.gradient_tolerance) {
      summary.status = GRADIENT_TOO_SMALL;
      return summary;
    }

    if (cost_ < options.cost_threshold) {
      summary.status = COST_TOO_SMALL;
      return summary;
    }

    Scalar u = 1.0 / options.initial_trust_region_radius;
    Scalar v = 2;

    for (summary.iterations = 1;
         summary.iterations < options.max_num_iterations;
         summary.iterations++) {
      jtj_regularized_ = jtj_;
      const Scalar min_diagonal = 1e-6;
      const Scalar max_diagonal = 1e32;
      for (int i = 0; i < lm_diagonal_.rows(); ++i) {
        lm_diagonal_[i] = std::sqrt(
            u * (std::min)((std::max)(jtj_(i, i), min_diagonal), max_diagonal));
        jtj_regularized_(i, i) += lm_diagonal_[i] * lm_diagonal_[i];
      }

      // TODO(sameeragarwal): Check for failure and deal with it.
      linear_solver_.compute(jtj_regularized_);
      lm_step_ = linear_solver_.solve(g_);
      dx_ = jacobi_scaling_.asDiagonal() * lm_step_;

      // Adding parameter_tolerance to x.norm() ensures that this
      // works if x is near zero.
      const Scalar parameter_tolerance =
          options.parameter_tolerance *
          (x.norm() + options.parameter_tolerance);
      if (dx_.norm() < parameter_tolerance) {
        summary.status = RELATIVE_STEP_SIZE_TOO_SMALL;
        break;
      }
      x_new_ = x + dx_;

      // TODO(keir): Add proper handling of errors from user eval of cost
      // functions.
      function(&x_new_[0], &f_x_new_[0], nullptr);

      const Scalar cost_change = (2 * cost_ - f_x_new_.squaredNorm());
      // TODO(sameeragarwal): Better more numerically stable evaluation.
      const Scalar model_cost_change = lm_step_.dot(2 * g_ - jtj_ * lm_step_);

      // rho is the ratio of the actual reduction in error to the reduction
      // in error that would be obtained if the problem was linear. See [1]
      // for details.
      Scalar rho(cost_change / model_cost_change);
      if (rho > 0) {
        // Accept the Levenberg-Marquardt step because the linear
        // model fits well.
        x = x_new_;

        if (std::abs(cost_change) < options.function_tolerance) {
          cost_ = f_x_new_.squaredNorm() / 2;
          summary.status = COST_CHANGE_TOO_SMALL;
          break;
        }

        // TODO(sameeragarwal): Deal with failure.
        Update(function, x);
        if (summary.gradient_max_norm < options.gradient_tolerance) {
          summary.status = GRADIENT_TOO_SMALL;
          break;
        }

        if (cost_ < options.cost_threshold) {
          summary.status = COST_TOO_SMALL;
          break;
        }

        Scalar tmp = Scalar(2 * rho - 1);
        u = u * (std::max)(Scalar(1 / 3.), Scalar(1) - tmp * tmp * tmp);
        v = 2;

      } else {
        // Reject the update because either the normal equations failed to solve
        // or the local linear model was not good (rho < 0).

        // Additionally if the cost change is too small, then terminate.
        if (std::abs(cost_change) < options.function_tolerance) {
          // Terminate
          summary.status = COST_CHANGE_TOO_SMALL;
          break;
        }

        // Reduce the size of the trust region.
        u *= v;
        v *= 2;
      }
    }

    summary.final_cost = cost_;
    return summary;
  }

  Options options;
  Summary summary;

 private:
  // Preallocate everything, including temporary storage needed for solving the
  // linear system. This allows reusing the intermediate storage across solves.
  LinearSolver linear_solver_;
  Scalar cost_;
  Parameters dx_, x_new_, g_, jacobi_scaling_, lm_diagonal_, lm_step_;
  Eigen::Matrix<Scalar, NUM_RESIDUALS, 1> residuals_, f_x_new_;
  Eigen::Matrix<Scalar, NUM_RESIDUALS, NUM_PARAMETERS> jacobian_;
  Eigen::Matrix<Scalar, NUM_PARAMETERS, NUM_PARAMETERS> jtj_, jtj_regularized_;

  // The following definitions are needed for template metaprogramming.
  template <bool Condition, typename T>
  struct enable_if;

  template <typename T>
  struct enable_if<true, T> {
    using type = T;
  };

  // The number of parameters and residuals are dynamically sized.
  template <int R, int P>
  typename enable_if<(R == Eigen::Dynamic && P == Eigen::Dynamic), void>::type
  Initialize(const Function& function) {
    Initialize(function.NumResiduals(), function.NumParameters());
  }

  // The number of parameters is dynamically sized and the number of
  // residuals is statically sized.
  template <int R, int P>
  typename enable_if<(R == Eigen::Dynamic && P != Eigen::Dynamic), void>::type
  Initialize(const Function& function) {
    Initialize(function.NumResiduals(), P);
  }

  // The number of parameters is statically sized and the number of
  // residuals is dynamically sized.
  template <int R, int P>
  typename enable_if<(R != Eigen::Dynamic && P == Eigen::Dynamic), void>::type
  Initialize(const Function& function) {
    Initialize(R, function.NumParameters());
  }

  // The number of parameters and residuals are statically sized.
  template <int R, int P>
  typename enable_if<(R != Eigen::Dynamic && P != Eigen::Dynamic), void>::type
  Initialize(const Function& /* function */) {}

  void Initialize(int num_residuals, int num_parameters) {
    dx_.resize(num_parameters);
    x_new_.resize(num_parameters);
    g_.resize(num_parameters);
    jacobi_scaling_.resize(num_parameters);
    lm_diagonal_.resize(num_parameters);
    lm_step_.resize(num_parameters);
    residuals_.resize(num_residuals);
    f_x_new_.resize(num_residuals);
    jacobian_.resize(num_residuals, num_parameters);
    jtj_.resize(num_parameters, num_parameters);
    jtj_regularized_.resize(num_parameters, num_parameters);
  }
};

}  // namespace ceres

#endif  // CERES_PUBLIC_TINY_SOLVER_H_
