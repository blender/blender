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

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/nviewtriangulation.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/test_data_sets.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"

namespace {

using namespace libmv;

TEST(NViewTriangulate, FiveViews) {
  int nviews = 5;
  int npoints = 6;
  NViewDataSet d = NRealisticCamerasFull(nviews, npoints);

  // Collect P matrices together.
  vector<Mat34> Ps(nviews);
  for (int j = 0; j < nviews; ++j) {
    Ps[j] = d.P(j);
  }

  for (int i = 0; i < npoints; ++i) {
    // Collect the image of point i in each frame.
    Mat2X xs(2, nviews);
    for (int j = 0; j < nviews; ++j) {
      xs.col(j) = d.x[j].col(i);
    }
    Vec4 X;
    NViewTriangulate(xs, Ps, &X);

    // Check reprojection error. Should be nearly zero.
    for (int j = 0; j < nviews; ++j) {
      Vec3 x_reprojected = Ps[j]*X;
      x_reprojected /= x_reprojected(2);
      double error = (x_reprojected.head(2) - xs.col(j)).norm();
      EXPECT_NEAR(error, 0.0, 1e-9);
    }
  }
}

TEST(NViewTriangulateAlgebraic, FiveViews) {
  int nviews = 5;
  int npoints = 6;
  NViewDataSet d = NRealisticCamerasFull(nviews, npoints);

  // Collect P matrices together.
  vector<Mat34> Ps(nviews);
  for (int j = 0; j < nviews; ++j) {
    Ps[j] = d.P(j);
  }

  for (int i = 0; i < npoints; ++i) {
    // Collect the image of point i in each frame.
    Mat2X xs(2, nviews);
    for (int j = 0; j < nviews; ++j) {
      xs.col(j) = d.x[j].col(i);
    }
    Vec4 X;
    NViewTriangulate(xs, Ps, &X);

    // Check reprojection error. Should be nearly zero.
    for (int j = 0; j < nviews; ++j) {
      Vec3 x_reprojected = Ps[j]*X;
      x_reprojected /= x_reprojected(2);
      double error = (x_reprojected.head<2>() - xs.col(j)).norm();
      EXPECT_NEAR(error, 0.0, 1e-9);
    }
  }
}
}  // namespace
