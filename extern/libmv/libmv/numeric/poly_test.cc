// Copyright (c) 2007, 2008 libmv authors.
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

#include "libmv/numeric/numeric.h"
#include "libmv/numeric/poly.h"
#include "testing/testing.h"

using namespace libmv;

namespace {

// Find the polynomial coefficients of x in the equation
//
//   (x - a)(x - b)(x - c) == 0
//
// by expanding to
//
//   x^3 - (c+b+a) * x^2 + (a*b+(b+a)*c) * x - a*b*c = 0.
//           = p               = q              = r
void CoeffsForCubicZeros(double a, double b, double c,
                    double *p, double *q, double *r) {
  *p = -(c + b + a);
  *q = (a * b + (b + a) * c);
  *r = -a * b * c;
}
// Find the polynomial coefficients of x in the equation
//
//   (x - a)(x - b)(x - c)(x - d) == 0
//
// by expanding to
//
//   x^4 - (d+c+b+a) * x^3 + (d*(c+b+a) + a*b+(b+a)*c) * x^2
//   - (d*(a*b+(b+a)*c)+a*b*c) * x + a*b*c*d = 0.
void CoeffsForQuarticZeros(double a, double b, double c, double d,
                    double *p, double *q, double *r, double *s) {
  *p = -(d + c + b + a);
  *q = (d * (c + b + a) + a * b + (b + a) * c);
  *r = -(d * (a * b + (b + a) * c) + a * b * c);
  *s = a * b * c *d;
}

TEST(Poly, SolveCubicPolynomial) {
  double a, b, c, aa, bb, cc;
  double p, q, r;

  a = 1; b = 2; c = 3;
  CoeffsForCubicZeros(a, b, c, &p, &q, &r);
  ASSERT_EQ(3, SolveCubicPolynomial(p, q, r, &aa, &bb, &cc));
  EXPECT_NEAR(a, aa, 1e-10);
  EXPECT_NEAR(b, bb, 1e-10);
  EXPECT_NEAR(c, cc, 1e-10);

  a = 0; b = 1; c = 3;
  CoeffsForCubicZeros(a, b, c, &p, &q, &r);
  ASSERT_EQ(3, SolveCubicPolynomial(p, q, r, &aa, &bb, &cc));
  EXPECT_NEAR(a, aa, 1e-10);
  EXPECT_NEAR(b, bb, 1e-10);
  EXPECT_NEAR(c, cc, 1e-10);

  a = -10; b = 0; c = 1;
  CoeffsForCubicZeros(a, b, c, &p, &q, &r);
  ASSERT_EQ(3, SolveCubicPolynomial(p, q, r, &aa, &bb, &cc));
  EXPECT_NEAR(a, aa, 1e-10);
  EXPECT_NEAR(b, bb, 1e-10);
  EXPECT_NEAR(c, cc, 1e-10);

  a = -8; b = 1; c = 3;
  CoeffsForCubicZeros(a, b, c, &p, &q, &r);
  ASSERT_EQ(3, SolveCubicPolynomial(p, q, r, &aa, &bb, &cc));
  EXPECT_NEAR(a, aa, 1e-10);
  EXPECT_NEAR(b, bb, 1e-10);
  EXPECT_NEAR(c, cc, 1e-10);

  a = 28; b = 28; c = 105;
  CoeffsForCubicZeros(a, b, c, &p, &q, &r);
  ASSERT_EQ(3, SolveCubicPolynomial(p, q, r, &aa, &bb, &cc));
  EXPECT_NEAR(a, aa, 1e-10);
  EXPECT_NEAR(b, bb, 1e-10);
  EXPECT_NEAR(c, cc, 1e-10);
}
}  // namespace
