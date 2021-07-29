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

#include "libmv/multiview/triangulation.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/test_data_sets.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"

namespace {
using namespace libmv;

TEST(Triangulation, TriangulateDLT) {
  TwoViewDataSet d = TwoRealisticCameras();

  for (int i = 0; i < d.X.cols(); ++i) {
    Vec2 x1, x2;
    MatrixColumn(d.x1, i, &x1);
    MatrixColumn(d.x2, i, &x2);
    Vec3 X_estimated, X_gt;
    MatrixColumn(d.X, i, &X_gt);
    TriangulateDLT(d.P1, x1, d.P2, x2, &X_estimated);
    EXPECT_NEAR(0, DistanceLInfinity(X_estimated, X_gt), 1e-8);
  }
}

}  // namespace
