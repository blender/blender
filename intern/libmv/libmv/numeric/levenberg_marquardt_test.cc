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

#include "testing/testing.h"
#include "libmv/numeric/levenberg_marquardt.h"

using namespace libmv;

namespace {

class F {
 public:
  typedef Vec4 FMatrixType;
  typedef Vec3 XMatrixType;
  Vec4 operator()(const Vec3 &x) const {
    double x1 = x.x() - 2;
    double y1 = x.y() - 5;
    double z1 = x.z();
    Vec4 fx; fx << x1*x1 + z1*z1,
                   y1*y1 + z1*z1,
                   z1*z1,
                   x1*x1;
    return fx;
  }
};

TEST(LevenbergMarquardt, SimpleCase) {
  Vec3 x(0.76026643, -30.01799744, 0.55192142);
  F f;
  LevenbergMarquardt<F>::SolverParameters params;
  LevenbergMarquardt<F> lm(f);
  /* TODO(sergey): Better error handling. */
  /* LevenbergMarquardt<F>::Results results = */ lm.minimize(params, &x);
  Vec3 expected_min_x(2, 5, 0);

  EXPECT_MATRIX_NEAR(expected_min_x, x, 1e-5);
}

}  // namespace
