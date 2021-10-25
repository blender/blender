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
// A simple implementation of Powell's dogleg nonlinear minimization.
//
// [1] K. Madsen, H. Nielsen, O. Tingleoff. Methods for Non-linear Least
// Squares Problems.
// http://www2.imm.dtu.dk/pubdb/views/edoc_download.php/3215/pdf/imm3215.pdf
//
// TODO(keir): Cite the Lourakis' dogleg paper.

#ifndef LIBMV_NUMERIC_DOGLEG_H
#define LIBMV_NUMERIC_DOGLEG_H

#include <cmath>
#include <cstdio>

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
class Dogleg {
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

  enum Status {
    RUNNING,
    GRADIENT_TOO_SMALL,            // eps > max(J'*f(x))
    RELATIVE_STEP_SIZE_TOO_SMALL,  // eps > ||dx|| / ||x||
    TRUST_REGION_TOO_SMALL,        // eps > radius / ||x||
    ERROR_TOO_SMALL,               // eps > ||f(x)||
    HIT_MAX_ITERATIONS,
  };

  enum Step {
    DOGLEG,
    GAUSS_NEWTON,
    STEEPEST_DESCENT,
  };

  Dogleg(const Function &f)
      : f_(f), df_(f) {}

  struct SolverParameters {
    SolverParameters()
       : gradient_threshold(1e-16),
         relative_step_threshold(1e-16),
         error_threshold(1e-16),
         initial_trust_radius(1e0),
         max_iterations(500) {}
    Scalar gradient_threshold;       // eps > max(J'*f(x))
    Scalar relative_step_threshold;  // eps > ||dx|| / ||x||
    Scalar error_threshold;          // eps > ||f(x)||
    Scalar initial_trust_radius;     // Initial u for solving normal equations.
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
    // TODO(keir): In the case of m = n, avoid computing A and just do J^-1 directly.
    *A = (*J).transpose() * (*J);
    *error = f_(x);
    *g = (*J).transpose() * *error;
    if (g->array().abs().maxCoeff() < params.gradient_threshold) {
      return GRADIENT_TOO_SMALL;
    } else if (error->array().abs().maxCoeff() < params.error_threshold) {
      return ERROR_TOO_SMALL;
    }
    return RUNNING;
  }

  Step SolveDoglegDirection(const Parameters &dx_sd,
                            const Parameters &dx_gn,
                            Scalar radius,
                            Scalar alpha,
                            Parameters *dx_dl,
                            Scalar *beta) {
    Parameters a, b_minus_a;
    // Solve for Dogleg step dx_dl.
    if (dx_gn.norm() < radius) {
      *dx_dl = dx_gn;
      return GAUSS_NEWTON;

    } else if (alpha * dx_sd.norm() > radius) {
      *dx_dl = (radius / dx_sd.norm()) * dx_sd;
      return STEEPEST_DESCENT;

    } else {
      Parameters a = alpha * dx_sd;
      const Parameters &b = dx_gn;
      b_minus_a = a - b;
      Scalar Mbma2 = b_minus_a.squaredNorm();
      Scalar Ma2 = a.squaredNorm();
      Scalar c = a.dot(b_minus_a);
      Scalar radius2 = radius*radius;
      if (c <= 0) {
        *beta = (-c + sqrt(c*c + Mbma2*(radius2 - Ma2)))/(Mbma2);
      } else {
        *beta = (radius2 - Ma2) /
               (c + sqrt(c*c + Mbma2*(radius2 - Ma2)));
      }
      *dx_dl = alpha * dx_sd + (*beta) * (dx_gn - alpha*dx_sd);
      return DOGLEG;
    }
  }

  Results minimize(Parameters *x_and_min) {
    SolverParameters params;
    return minimize(params, x_and_min);
  }

  Results minimize(const SolverParameters &params, Parameters *x_and_min) {
    Parameters &x = *x_and_min;
    JMatrixType J;
    AMatrixType A;
    FVec error;
    Parameters g;

    Results results;
    results.status = Update(x, params, &J, &A, &error, &g);

    Scalar radius = params.initial_trust_radius;
    bool x_updated = true;

    Parameters x_new;
    Parameters dx_sd;  // Steepest descent step.
    Parameters dx_dl;  // Dogleg step.
    Parameters dx_gn;  // Gauss-Newton step.
      printf("iteration     ||f(x)||      max(g)       radius\n");
    int i = 0;
    for (; results.status == RUNNING && i < params.max_iterations; ++i) {
      printf("%9d %12g %12g %12g",
          i, f_(x).norm(), g.array().abs().maxCoeff(), radius);

      //LG << "iteration: " << i;
      //LG << "||f(x)||: " << f_(x).norm();
      //LG << "max(g): " << g.cwise().abs().maxCoeff();
      //LG << "radius: " << radius;
      // Eqn 3.19 from [1]
      Scalar alpha = g.squaredNorm() / (J*g).squaredNorm();

      // Solve for steepest descent direction dx_sd.
      dx_sd = -g;

      // Solve for Gauss-Newton direction dx_gn.
      if (x_updated) {
        // TODO(keir): See Appendix B of [1] for discussion of when A is
        // singular and there are many solutions. Solving that involves the SVD
        // and is slower, but should still work.
        Solver solver(A);
        dx_gn = solver.solve(-g);
        if (!(A * dx_gn).isApprox(-g)) {
          LOG(ERROR) << "Failed to solve normal eqns. TODO: Solve via SVD.";
          return results;
        }
        x_updated = false;
      }

      // Solve for dogleg direction dx_dl.
      Scalar beta = 0;
      Step step = SolveDoglegDirection(dx_sd, dx_gn, radius, alpha,
                                       &dx_dl, &beta);

      Scalar e3 = params.relative_step_threshold;
      if (dx_dl.norm() < e3*(x.norm() + e3)) {
        results.status = RELATIVE_STEP_SIZE_TOO_SMALL;
        break;
      }

      x_new = x + dx_dl;
      Scalar actual = f_(x).squaredNorm() - f_(x_new).squaredNorm();
      Scalar predicted = 0;
      if (step == GAUSS_NEWTON) {
        predicted = f_(x).squaredNorm();
      } else if (step == STEEPEST_DESCENT) {
        predicted = radius * (2*alpha*g.norm() - radius) / 2 / alpha;
      } else if (step == DOGLEG) {
        predicted = 0.5 * alpha * (1-beta)*(1-beta)*g.squaredNorm() +
                    beta*(2-beta)*f_(x).squaredNorm();
      }
      Scalar rho = actual / predicted;

      if (step == GAUSS_NEWTON) printf("  GAUSS");
      if (step == STEEPEST_DESCENT) printf("   STEE");
      if (step == DOGLEG) printf("   DOGL");

      printf(" %12g %12g %12g\n", rho, actual, predicted);

      if (rho > 0) {
        // Accept update because the linear model is a good fit.
        x = x_new;
        results.status = Update(x, params, &J, &A, &error, &g);
        x_updated = true;
      }
      if (rho > 0.75) {
        radius = std::max(radius, 3*dx_dl.norm());
      } else if (rho < 0.25) {
        radius /= 2;
        if (radius < e3 * (x.norm() + e3)) {
          results.status = TRUST_REGION_TOO_SMALL;
        }
      }
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

#endif  // LIBMV_NUMERIC_DOGLEG_H
