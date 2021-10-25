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

#ifndef LIBMV_MULTIVIEW_TEST_DATA_SETS_H_
#define LIBMV_MULTIVIEW_TEST_DATA_SETS_H_

#include "libmv/base/vector.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

struct TwoViewDataSet {
  Mat3 K1, K2;   // Internal parameters.
  Mat3 R1, R2;   // Rotation.
  Vec3 t1, t2;   // Translation.
  Mat34 P1, P2;  // Projection matrix, P = K(R|t)
  Mat3 F;        // Fundamental matrix.
  Mat3X X;        // 3D points.
  Mat2X x1, x2;   // Projected points.
};

// Two cameras at (-1,-1,-10) and (2,1,-10) looking approximately towards z+.
TwoViewDataSet TwoRealisticCameras(bool same_K = false);

// An N-view metric dataset . An important difference between this
// and the other reconstruction data types is that all points are seen by all
// cameras.
struct NViewDataSet {
  vector<Mat3> K;   // Internal parameters (fx, fy, etc).
  vector<Mat3> R;   // Rotation.
  vector<Vec3> t;   // Translation.
  vector<Vec3> C;   // Camera centers.
  Mat3X X;     // 3D points.
  vector<Mat2X> x;  // Projected points; may have noise added.
  vector<Vecu>  x_ids;  // Indexes of points corresponding to the projections

  int n;  // Actual number of cameras.

  Mat34 P(int i) {
    assert(i < n);
    return K[i] * HStack(R[i], t[i]);
  }
  Mat3 F(int i, int j) {
    Mat3 F_;
    FundamentalFromProjections(P(i), P(j), &F_);
    return F_;
  }
  void Reproject() {
    for (int i = 0; i < n; ++i) {
      x[i] = Project(P(i), X);
    }
  }
  // TODO(keir): Add gaussian jitter functions.
};

struct nViewDatasetConfigator {
  /// Internal camera parameters
  int _fx;
  int _fy;
  int _cx;
  int _cy;

  /// Camera random position parameters
  double _dist;
  double _jitter_amount;

  nViewDatasetConfigator(int fx = 1000,  int fy = 1000,
                         int cx = 500,   int cy  = 500,
                         double distance = 1.5,
                         double jitter_amount = 0.01);
};

NViewDataSet NRealisticCamerasFull(int nviews, int npoints,
                                   const nViewDatasetConfigator
                                     config = nViewDatasetConfigator());

// Generates sparse projections (not all points are projected)
NViewDataSet NRealisticCamerasSparse(int nviews, int npoints,
                                     float view_ratio = 0.6,
                                     unsigned min_projections = 3,
                                     const nViewDatasetConfigator
                                       config = nViewDatasetConfigator());

}  // namespace libmv

#endif  // LIBMV_MULTIVIEW_TEST_DATA_SETS_H_
