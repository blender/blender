// Copyright (c) 2009 libmv authors.
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
#include "libmv/multiview/resection.h"
#include "libmv/multiview/test_data_sets.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"
#include "libmv/logging/logging.h"

namespace {

using namespace libmv;
using namespace libmv::resection;

TEST(Resection, ThreeViews) {
  int nviews = 5;
  int npoints = 6;
  NViewDataSet d = NRealisticCamerasFull(nviews, npoints);
  for (int i = 0; i < nviews; ++i) {
    Mat4X X(4, npoints);
    X.block(0, 0, 3, npoints) = d.X;
    X.row(3).setOnes();
    const Mat2X &x = d.x[i];
    Mat34 P;
    Resection(x, X, &P);
    Mat34 P_expected = d.P(i);

    // Because the P matrices are homogeneous, it is necessary to be tricky
    // about the scale factor to make them match.
    P_expected *= 1/P_expected.array().abs().sum();
    P *= 1/P.array().abs().sum();
    if (!((P(0, 0) > 0 && P_expected(0, 0) > 0) ||
          (P(0, 0) < 0 && P_expected(0, 0) < 0))) {
      P *= -1;
    }

    EXPECT_MATRIX_NEAR(P_expected, P, 1e-9);
  }
}

}  // namespace
