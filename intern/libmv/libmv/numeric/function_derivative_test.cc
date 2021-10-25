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
#include "libmv/numeric/numeric.h"
#include "libmv/numeric/function_derivative.h"

using namespace libmv;

namespace {

class F {
 public:
  typedef Vec2 FMatrixType;
  typedef Vec3 XMatrixType;
  Vec2 operator()(const Vec3 &x) const {
    Vec2 fx;
    fx << 0.19*x(0) + 0.19*x(1)*x(1) + x(2),
          3*sin(x(0)) + 2*cos(x(1));
    return fx;
  }
  Mat23 J(const Vec3 &x) const {
    Mat23 jacobian;
    jacobian << 0.19, 2*0.19*x(1), 1.0,
                3*cos(x(0)), -2*sin(x(1)), 0;
    return jacobian;
  }
};

TEST(FunctionDerivative, SimpleCase) {
  Vec3 x; x << 0.76026643, 0.01799744, 0.55192142;
  F f;
  NumericJacobian<F, CENTRAL> J(f);
  EXPECT_MATRIX_NEAR(f.J(x), J(x), 1e-8);
  NumericJacobian<F, FORWARD> J_forward(f);
  // Forward difference is very inaccurate.
  EXPECT_MATRIX_NEAR(f.J(x), J_forward(x), 1e-5);
}

}  // namespace
