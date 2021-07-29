// Copyright (c) 2007, 2008, 2009 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// A simple implementation of levenberg marquardt.
//
// [1] K. Madsen, H. Nielsen, O. Tingleoff. Methods for Non-linear Least
// Squares Problems.
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
//
// TODO(keir): Cite the Lourakis' dogleg paper.

#ifndef LIBMV_NUMERIC_LEVENBERG_MARQUARDT_H
#define LIBMV_NUMERIC_LEVENBERG_MARQUARDT_H

#include <cmath>

#include "libmv/numeric/numeric.h"
#include "libmv/numeric/function_derivative.h"
#include "libmv/logging/logging.h"

namespace libmv {

template<typename Function,
         typename Jacobian = NumericJacobian<Function>,
         typename Solver = Eigen::PartialPivLU<
           Matrix<typename Function::FMatrixType::RealScalar,
                  Function::XMatrixType::RowsAtCompileTime,
                  Function::XMatrixType::RowsAtCompileTime> > >
class LevenbergMarquardt {
 public:
  typedef typename Function::XMatrixType::RealScalar Scalar;
  typedef typename Function::FMatrixType FVec;
  typedef typename Function::XMatrixType Parameters;
  typedef Matrix<typename Function::FMatrixType::RealScalar,
                 Function::FMatrixType::RowsAtCompileTime,
                 Function::XMatrixType::RowsAtCompileTime> JMatrixType;
  typedef Matrix<typename JMatrixType::RealScalar,
                 JMatrixType::ColsAtCompileTime,
                 JMatrixType::ColsAtCompileTime> AMatrixType;

  // TODO(keir): Some of these knobs can be derived from each other and
  // removed, instead of requiring the user to set them.
  enum Status {
    RUNNING,
    GRADIENT_TOO_SMALL,            // eps > max(J'*f(x))
    RELATIVE_STEP_SIZE_TOO_SMALL,  // eps > ||dx|| / ||x||
    ERROR_TOO_SMALL,               // eps > ||f(x)||
    HIT_MAX_ITERATIONS,
  };

  LevenbergMarquardt(const Function &f)
      : f_(f), df_(f) {}

  struct SolverParameters {
    SolverParameters()
       : gradient_threshold(1e-16),
         relative_step_threshold(1e-16),
         error_threshold(1e-16),
         initial_scale_factor(1e-3),
         max_iterations(100) {}
    Scalar gradient_threshold;       // eps > max(J'*f(x))
    Scalar relative_step_threshold;  // eps > ||dx|| / ||x||
    Scalar error_threshold;          // eps > ||f(x)||
    Scalar initial_scale_factor;     // Initial u for solving normal equations.
    int    max_iterations;           // Maximum number of solver iterations.
  };

  struct Results {
    Scalar error_magnitude;     // ||f(x)||
    Scalar gradient_magnitude;  // ||J'f(x)||
    int    iterations;
    Status status;
  };

  Status Update(const Parameters &x, const SolverParameters &params,
                JMatrixType *J, AMatrixType *A, FVec *error, Parameters *g) {
    *J = df_(x);
    *A = (*J).transpose() * (*J);
    *error = -f_(x);
    *g = (*J).transpose() * *error;
    if (g->array().abs().maxCoeff() < params.gradient_threshold) {
      return GRADIENT_TOO_SMALL;
    } else if (error->norm() < params.error_threshold) {
      return ERROR_TOO_SMALL;
    }
    return RUNNING;
  }

  Results minimize(Parameters *x_and_min) {
    SolverParameters params;
    minimize(params, x_and_min);
  }

  Results minimize(const SolverParameters &params, Parameters *x_and_min) {
    Parameters &x = *x_and_min;
    JMatrixType J;
    AMatrixType A;
    FVec error;
    Parameters g;

    Results results;
    results.status = Update(x, params, &J, &A, &error, &g);

    Scalar u = Scalar(params.initial_scale_factor*A.diagonal().maxCoeff());
    Scalar v = 2;

    Parameters dx, x_new;
    int i;
    for (i = 0; results.status == RUNNING && i < params.max_iterations; ++i) {
      VLOG(3) << "iteration: " << i;
      VLOG(3) << "||f(x)||: " << f_(x).norm();
      VLOG(3) << "max(g): " << g.array().abs().maxCoeff();
      VLOG(3) << "u: " << u;
      VLOG(3) << "v: " << v;

      AMatrixType A_augmented = A + u*AMatrixType::Identity(J.cols(), J.cols());
      Solver solver(A_augmented);
      dx = solver.solve(g);
      bool solved = (A_augmented * dx).isApprox(g);
      if (!solved) {
        LOG(ERROR) << "Failed to solve";
      }
      if (solved && dx.norm() <= params.relative_step_threshold * x.norm()) {
        results.status = RELATIVE_STEP_SIZE_TOO_SMALL;
        break;
      }
      if (solved) {
        x_new = x + dx;
        // Rho is the ratio of the actual reduction in error to the reduction
        // in error that would be obtained if the problem was linear.
        // See [1] for details.
        Scalar rho((error.squaredNorm() - f_(x_new).squaredNorm())
                   / dx.dot(u*dx + g));
        if (rho > 0) {
          // Accept the Gauss-Newton step because the linear model fits well.
          x = x_new;
          results.status = Update(x, params, &J, &A, &error, &g);
          Scalar tmp = Scalar(2*rho-1);
          u = u*std::max(1/3., 1 - (tmp*tmp*tmp));
          v = 2;
          continue;
        }
      }
      // Reject the update because either the normal equations failed to solve
      // or the local linear model was not good (rho < 0). Instead, increase u
      // to move closer to gradient descent.
      u *= v;
      v *= 2;
    }
    if (results.status == RUNNING) {
      results.status = HIT_MAX_ITERATIONS;
    }
    results.error_magnitude = error.norm();
    results.gradient_magnitude = g.norm();
    results.iterations = i;
    return results;
  }

 private:
  const Function &f_;
  Jacobian df_;
};

}  // namespace mv

#endif  // LIBMV_NUMERIC_LEVENBERG_MARQUARDT_H
