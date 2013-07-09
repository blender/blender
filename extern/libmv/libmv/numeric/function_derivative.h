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

#ifndef LIBMV_NUMERIC_DERIVATIVE_H
#define LIBMV_NUMERIC_DERIVATIVE_H

#include <cmath>

#include "libmv/numeric/numeric.h"
#include "libmv/logging/logging.h"

namespace libmv {

// Numeric derivative of a function.
// TODO(keir): Consider adding a quadratic approximation.

enum NumericJacobianMode {
  CENTRAL,
  FORWARD,
};

template<typename Function, NumericJacobianMode mode = CENTRAL>
class NumericJacobian {
 public:
  typedef typename Function::XMatrixType Parameters;
  typedef typename Function::XMatrixType::RealScalar XScalar;
  typedef typename Function::FMatrixType FMatrixType;
  typedef Matrix<typename Function::FMatrixType::RealScalar,
                 Function::FMatrixType::RowsAtCompileTime,
                 Function::XMatrixType::RowsAtCompileTime>
          JMatrixType;

  NumericJacobian(const Function &f) : f_(f) {}

  // TODO(keir): Perhaps passing the jacobian back by value is not a good idea.
  JMatrixType operator()(const Parameters &x) {
    // Empirically determined constant.
    Parameters eps = x.array().abs() * XScalar(1e-5);
    // To handle cases where a paremeter is exactly zero, instead use the mean
    // eps for the other dimensions.
    XScalar mean_eps = eps.sum() / eps.rows();
    if (mean_eps == XScalar(0)) {
      // TODO(keir): Do something better here.
      mean_eps = 1e-8;  // ~sqrt(machine precision).
    }
    // TODO(keir): Elimininate this needless function evaluation for the
    // central difference case.
    FMatrixType fx = f_(x);
    const int rows = fx.rows();
    const int cols = x.rows();
    JMatrixType jacobian(rows, cols);
    Parameters x_plus_delta = x;
    for (int c = 0; c < cols; ++c) {
      if (eps(c) == XScalar(0)) {
        eps(c) = mean_eps;
      }
      x_plus_delta(c) = x(c) + eps(c);
      jacobian.col(c) = f_(x_plus_delta);

      XScalar one_over_h = 1 / eps(c);
      if (mode == CENTRAL) {
        x_plus_delta(c) = x(c) - eps(c);
        jacobian.col(c) -= f_(x_plus_delta);
        one_over_h /= 2;
      } else {
        jacobian.col(c) -= fx;
      }
      x_plus_delta(c) = x(c);
      jacobian.col(c) = jacobian.col(c) * one_over_h;
    }
    return jacobian;
  }
 private:
  const Function &f_;
};

template<typename Function, typename Jacobian>
bool CheckJacobian(const Function &f, const typename Function::XMatrixType &x) {
  Jacobian j_analytic(f);
  NumericJacobian<Function> j_numeric(f);

  typename NumericJacobian<Function>::JMatrixType J_numeric = j_numeric(x);
  typename NumericJacobian<Function>::JMatrixType J_analytic = j_analytic(x);
  LG << J_numeric - J_analytic;
  return true;
}

}  // namespace libmv

#endif  // LIBMV_NUMERIC_DERIVATIVE_H
