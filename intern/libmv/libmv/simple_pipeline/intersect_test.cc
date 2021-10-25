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

#include "libmv/simple_pipeline/intersect.h"

#include <iostream>

#include "testing/testing.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"
#include "libmv/logging/logging.h"

namespace libmv {

TEST(Intersect, EuclideanIntersect) {
  Mat3 K1 = Mat3::Identity();
  // K1 << 320, 0, 160,
  //        0, 320, 120,
  //        0,   0,   1;
  Mat3 K2 = Mat3::Identity();
  //  K2 << 360, 0, 170,
  //        0, 360, 110,
  //        0,   0,   1;
  Mat3 R1 = RotationAroundZ(-0.1);
  Mat3 R2 = RotationAroundX(-0.1);
  Vec3 t1; t1 <<  1,  1, 10;
  Vec3 t2; t2 << -2, -1, 10;
  Mat34 P1, P2;
  P_From_KRt(K1, R1, t1, &P1);
  P_From_KRt(K2, R2, t2, &P2);

  //Mat3 F; FundamentalFromProjections(P1, P2, &F);

  Mat3X X;
  X.resize(3, 30);
  X.setRandom();

  Mat2X X1, X2;
  Project(P1, X, &X1);
  Project(P2, X, &X2);

  for (int i = 0; i < X.cols(); ++i) {
    Vec2 x1, x2;
    MatrixColumn(X1, i, &x1);
    MatrixColumn(X2, i, &x2);
    Vec3 expected;
    MatrixColumn(X, i, &expected);

    EuclideanReconstruction reconstruction;
    reconstruction.InsertCamera(1, R1, t1);
    reconstruction.InsertCamera(2, R2, t2);

    vector<Marker> markers;
    Marker a = { 1, 0, x1.x(), x1.y(), 1.0 };
    markers.push_back(a);
    Marker b = { 2, 0, x2.x(), x2.y(), 1.0 };
    markers.push_back(b);

    EuclideanIntersect(markers, &reconstruction);
    Vec3 estimated = reconstruction.PointForTrack(0)->X;
    EXPECT_NEAR(0, DistanceLInfinity(estimated, expected), 1e-8);
  }
}
}  // namespace
