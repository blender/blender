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

#include <iostream>

#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"

namespace {
using namespace libmv;

TEST(Projection, P_From_KRt) {
  Mat3 K, Kp;
  K << 10,  1, 30,
        0, 20, 40,
        0,  0,  1;

  Mat3 R, Rp;
  R << 1, 0, 0,
       0, 1, 0,
       0, 0, 1;

  Vec3 t, tp;
  t << 1, 2, 3;

  Mat34 P;
  P_From_KRt(K, R, t, &P);
  KRt_From_P(P, &Kp, &Rp, &tp);

  EXPECT_MATRIX_NEAR(K, Kp, 1e-8);
  EXPECT_MATRIX_NEAR(R, Rp, 1e-8);
  EXPECT_MATRIX_NEAR(t, tp, 1e-8);

  // TODO(keir): Change the code to ensure det(R) == 1, which is not currently
  // the case. Also add a test for that here.
}

Vec4 GetRandomPoint() {
  Vec4 X;
  X.setRandom();
  X(3) = 1;
  return X;
}

TEST(Projection, isInFrontOfCamera) {
  Mat34 P;
  P << 1, 0, 0, 0,
       0, 1, 0, 0,
       0, 0, 1, 0;

  Vec4 X_front = GetRandomPoint();
  Vec4 X_back = GetRandomPoint();
  X_front(2) = 10;  // Any point in the positive Z direction
                    // where Z > 1 is infront of the camera.
  X_back(2) = -10;  // Any point int he negative Z dirstaion
                    // is behind the camera.

  bool res_front = isInFrontOfCamera(P, X_front);
  bool res_back = isInFrontOfCamera(P, X_back);

  EXPECT_EQ(true, res_front);
  EXPECT_EQ(false, res_back);
}

TEST(AutoCalibration, ProjectionShiftPrincipalPoint) {
  Mat34 P1, P2;
  P1 << 1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0;
  P2 << 1, 0, 3, 0,
        0, 1, 4, 0,
        0, 0, 1, 0;
  Mat34 P1_computed, P2_computed;
  ProjectionShiftPrincipalPoint(P1, Vec2(0, 0), Vec2(3, 4), &P2_computed);
  ProjectionShiftPrincipalPoint(P2, Vec2(3, 4), Vec2(0, 0), &P1_computed);

  EXPECT_MATRIX_EQ(P1, P1_computed);
  EXPECT_MATRIX_EQ(P2, P2_computed);
}

TEST(AutoCalibration, ProjectionChangeAspectRatio) {
  Mat34 P1, P2;
  P1 << 1, 0, 3, 0,
        0, 1, 4, 0,
        0, 0, 1, 0;
  P2 << 1, 0, 3, 0,
        0, 2, 4, 0,
        0, 0, 1, 0;
  Mat34 P1_computed, P2_computed;
  ProjectionChangeAspectRatio(P1, Vec2(3, 4), 1, 2, &P2_computed);
  ProjectionChangeAspectRatio(P2, Vec2(3, 4), 2, 1, &P1_computed);

  EXPECT_MATRIX_EQ(P1, P1_computed);
  EXPECT_MATRIX_EQ(P2, P2_computed);
}

}  // namespace
