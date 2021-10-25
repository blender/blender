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
// Author: moll.markus@arcor.de (Markus Moll)
//         sameeragarwal@google.com (Sameer Agarwal)

#include "ceres/polynomial.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "Eigen/Dense"
#include "ceres/internal/port.h"
#include "ceres/stringprintf.h"
#include "glog/logging.h"

namespace ceres {
namespace internal {

using std::string;
using std::vector;

namespace {

// Balancing function as described by B. N. Parlett and C. Reinsch,
// "Balancing a Matrix for Calculation of Eigenvalues and Eigenvectors".
// In: Numerische Mathematik, Volume 13, Number 4 (1969), 293-304,
// Springer Berlin / Heidelberg. DOI: 10.1007/BF02165404
void BalanceCompanionMatrix(Matrix* companion_matrix_ptr) {
  CHECK_NOTNULL(companion_matrix_ptr);
  Matrix& companion_matrix = *companion_matrix_ptr;
  Matrix companion_matrix_offdiagonal = companion_matrix;
  companion_matrix_offdiagonal.diagonal().setZero();

  const int degree = companion_matrix.rows();

  // gamma <= 1 controls how much a change in the scaling has to
  // lower the 1-norm of the companion matrix to be accepted.
  //
  // gamma = 1 seems to lead to cycles (numerical issues?), so
  // we set it slightly lower.
  const double gamma = 0.9;

  // Greedily scale row/column pairs until there is no change.
  bool scaling_has_changed;
  do {
    scaling_has_changed = false;

    for (int i = 0; i < degree; ++i) {
      const double row_norm = companion_matrix_offdiagonal.row(i).lpNorm<1>();
      const double col_norm = companion_matrix_offdiagonal.col(i).lpNorm<1>();

      // Decompose row_norm/col_norm into mantissa * 2^exponent,
      // where 0.5 <= mantissa < 1. Discard mantissa (return value
      // of frexp), as only the exponent is needed.
      int exponent = 0;
      std::frexp(row_norm / col_norm, &exponent);
      exponent /= 2;

      if (exponent != 0) {
        const double scaled_col_norm = std::ldexp(col_norm, exponent);
        const double scaled_row_norm = std::ldexp(row_norm, -exponent);
        if (scaled_col_norm + scaled_row_norm < gamma * (col_norm + row_norm)) {
          // Accept the new scaling. (Multiplication by powers of 2 should not
          // introduce rounding errors (ignoring non-normalized numbers and
          // over- or underflow))
          scaling_has_changed = true;
          companion_matrix_offdiagonal.row(i) *= std::ldexp(1.0, -exponent);
          companion_matrix_offdiagonal.col(i) *= std::ldexp(1.0, exponent);
        }
      }
    }
  } while (scaling_has_changed);

  companion_matrix_offdiagonal.diagonal() = companion_matrix.diagonal();
  companion_matrix = companion_matrix_offdiagonal;
  VLOG(3) << "Balanced companion matrix is\n" << companion_matrix;
}

void BuildCompanionMatrix(const Vector& polynomial,
                          Matrix* companion_matrix_ptr) {
  CHECK_NOTNULL(companion_matrix_ptr);
  Matrix& companion_matrix = *companion_matrix_ptr;

  const int degree = polynomial.size() - 1;

  companion_matrix.resize(degree, degree);
  companion_matrix.setZero();
  companion_matrix.diagonal(-1).setOnes();
  companion_matrix.col(degree - 1) = -polynomial.reverse().head(degree);
}

// Remove leading terms with zero coefficients.
Vector RemoveLeadingZeros(const Vector& polynomial_in) {
  int i = 0;
  while (i < (polynomial_in.size() - 1) && polynomial_in(i) == 0.0) {
    ++i;
  }
  return polynomial_in.tail(polynomial_in.size() - i);
}

void FindLinearPolynomialRoots(const Vector& polynomial,
                               Vector* real,
                               Vector* imaginary) {
  CHECK_EQ(polynomial.size(), 2);
  if (real != NULL) {
    real->resize(1);
    (*real)(0) = -polynomial(1) / polynomial(0);
  }

  if (imaginary != NULL) {
    imaginary->setZero(1);
  }
}

void FindQuadraticPolynomialRoots(const Vector& polynomial,
                                  Vector* real,
                                  Vector* imaginary) {
  CHECK_EQ(polynomial.size(), 3);
  const double a = polynomial(0);
  const double b = polynomial(1);
  const double c = polynomial(2);
  const double D = b * b - 4 * a * c;
  const double sqrt_D = sqrt(fabs(D));
  if (real != NULL) {
    real->setZero(2);
  }
  if (imaginary != NULL) {
    imaginary->setZero(2);
  }

  // Real roots.
  if (D >= 0) {
    if (real != NULL) {
      // Stable quadratic roots according to BKP Horn.
      // http://people.csail.mit.edu/bkph/articles/Quadratics.pdf
      if (b >= 0) {
        (*real)(0) = (-b - sqrt_D) / (2.0 * a);
        (*real)(1) = (2.0 * c) / (-b - sqrt_D);
      } else {
        (*real)(0) = (2.0 * c) / (-b + sqrt_D);
        (*real)(1) = (-b + sqrt_D) / (2.0 * a);
      }
    }
    return;
  }

  // Use the normal quadratic formula for the complex case.
  if (real != NULL) {
    (*real)(0) = -b / (2.0 * a);
    (*real)(1) = -b / (2.0 * a);
  }
  if (imaginary != NULL) {
    (*imaginary)(0) = sqrt_D / (2.0 * a);
    (*imaginary)(1) = -sqrt_D / (2.0 * a);
  }
}
}  // namespace

bool FindPolynomialRoots(const Vector& polynomial_in,
                         Vector* real,
                         Vector* imaginary) {
  if (polynomial_in.size() == 0) {
    LOG(ERROR) << "Invalid polynomial of size 0 passed to FindPolynomialRoots";
    return false;
  }

  Vector polynomial = RemoveLeadingZeros(polynomial_in);
  const int degree = polynomial.size() - 1;

  VLOG(3) << "Input polynomial: " << polynomial_in.transpose();
  if (polynomial.size() != polynomial_in.size()) {
    VLOG(3) << "Trimmed polynomial: " << polynomial.transpose();
  }

  // Is the polynomial constant?
  if (degree == 0) {
    LOG(WARNING) << "Trying to extract roots from a constant "
                 << "polynomial in FindPolynomialRoots";
    // We return true with no roots, not false, as if the polynomial is constant
    // it is correct that there are no roots. It is not the case that they were
    // there, but that we have failed to extract them.
    return true;
  }

  // Linear
  if (degree == 1) {
    FindLinearPolynomialRoots(polynomial, real, imaginary);
    return true;
  }

  // Quadratic
  if (degree == 2) {
    FindQuadraticPolynomialRoots(polynomial, real, imaginary);
    return true;
  }

  // The degree is now known to be at least 3. For cubic or higher
  // roots we use the method of companion matrices.

  // Divide by leading term
  const double leading_term = polynomial(0);
  polynomial /= leading_term;

  // Build and balance the companion matrix to the polynomial.
  Matrix companion_matrix(degree, degree);
  BuildCompanionMatrix(polynomial, &companion_matrix);
  BalanceCompanionMatrix(&companion_matrix);

  // Find its (complex) eigenvalues.
  Eigen::EigenSolver<Matrix> solver(companion_matrix, false);
  if (solver.info() != Eigen::Success) {
    LOG(ERROR) << "Failed to extract eigenvalues from companion matrix.";
    return false;
  }

  // Output roots
  if (real != NULL) {
    *real = solver.eigenvalues().real();
  } else {
    LOG(WARNING) << "NULL pointer passed as real argument to "
                 << "FindPolynomialRoots. Real parts of the roots will not "
                 << "be returned.";
  }
  if (imaginary != NULL) {
    *imaginary = solver.eigenvalues().imag();
  }
  return true;
}

Vector DifferentiatePolynomial(const Vector& polynomial) {
  const int degree = polynomial.rows() - 1;
  CHECK_GE(degree, 0);

  // Degree zero polynomials are constants, and their derivative does
  // not result in a smaller degree polynomial, just a degree zero
  // polynomial with value zero.
  if (degree == 0) {
    return Eigen::VectorXd::Zero(1);
  }

  Vector derivative(degree);
  for (int i = 0; i < degree; ++i) {
    derivative(i) = (degree - i) * polynomial(i);
  }

  return derivative;
}

void MinimizePolynomial(const Vector& polynomial,
                        const double x_min,
                        const double x_max,
                        double* optimal_x,
                        double* optimal_value) {
  // Find the minimum of the polynomial at the two ends.
  //
  // We start by inspecting the middle of the interval. Technically
  // this is not needed, but we do this to make this code as close to
  // the minFunc package as possible.
  *optimal_x = (x_min + x_max) / 2.0;
  *optimal_value = EvaluatePolynomial(polynomial, *optimal_x);

  const double x_min_value = EvaluatePolynomial(polynomial, x_min);
  if (x_min_value < *optimal_value) {
    *optimal_value = x_min_value;
    *optimal_x = x_min;
  }

  const double x_max_value = EvaluatePolynomial(polynomial, x_max);
  if (x_max_value < *optimal_value) {
    *optimal_value = x_max_value;
    *optimal_x = x_max;
  }

  // If the polynomial is linear or constant, we are done.
  if (polynomial.rows() <= 2) {
    return;
  }

  const Vector derivative = DifferentiatePolynomial(polynomial);
  Vector roots_real;
  if (!FindPolynomialRoots(derivative, &roots_real, NULL)) {
    LOG(WARNING) << "Unable to find the critical points of "
                 << "the interpolating polynomial.";
    return;
  }

  // This is a bit of an overkill, as some of the roots may actually
  // have a complex part, but its simpler to just check these values.
  for (int i = 0; i < roots_real.rows(); ++i) {
    const double root = roots_real(i);
    if ((root < x_min) || (root > x_max)) {
      continue;
    }

    const double value = EvaluatePolynomial(polynomial, root);
    if (value < *optimal_value) {
      *optimal_value = value;
      *optimal_x = root;
    }
  }
}

string FunctionSample::ToDebugString() const {
  return StringPrintf("[x: %.8e, value: %.8e, gradient: %.8e, "
                      "value_is_valid: %d, gradient_is_valid: %d]",
                      x, value, gradient, value_is_valid, gradient_is_valid);
}

Vector FindInterpolatingPolynomial(const vector<FunctionSample>& samples) {
  const int num_samples = samples.size();
  int num_constraints = 0;
  for (int i = 0; i < num_samples; ++i) {
    if (samples[i].value_is_valid) {
      ++num_constraints;
    }
    if (samples[i].gradient_is_valid) {
      ++num_constraints;
    }
  }

  const int degree = num_constraints - 1;

  Matrix lhs = Matrix::Zero(num_constraints, num_constraints);
  Vector rhs = Vector::Zero(num_constraints);

  int row = 0;
  for (int i = 0; i < num_samples; ++i) {
    const FunctionSample& sample = samples[i];
    if (sample.value_is_valid) {
      for (int j = 0; j <= degree; ++j) {
        lhs(row, j) = pow(sample.x, degree - j);
      }
      rhs(row) = sample.value;
      ++row;
    }

    if (sample.gradient_is_valid) {
      for (int j = 0; j < degree; ++j) {
        lhs(row, j) = (degree - j) * pow(sample.x, degree - j - 1);
      }
      rhs(row) = sample.gradient;
      ++row;
    }
  }

  return lhs.fullPivLu().solve(rhs);
}

void MinimizeInterpolatingPolynomial(const vector<FunctionSample>& samples,
                                     double x_min,
                                     double x_max,
                                     double* optimal_x,
                                     double* optimal_value) {
  const Vector polynomial = FindInterpolatingPolynomial(samples);
  MinimizePolynomial(polynomial, x_min, x_max, optimal_x, optimal_value);
  for (int i = 0; i < samples.size(); ++i) {
    const FunctionSample& sample = samples[i];
    if ((sample.x < x_min) || (sample.x > x_max)) {
      continue;
    }

    const double value = EvaluatePolynomial(polynomial, sample.x);
    if (value < *optimal_value) {
      *optimal_x = sample.x;
      *optimal_value = value;
    }
  }
}

}  // namespace internal
}  // namespace ceres
