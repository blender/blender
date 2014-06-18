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

#include "libmv/multiview/test_data_sets.h"

#include <cmath>

#include "libmv/numeric/numeric.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/fundamental.h"

namespace libmv {

TwoViewDataSet TwoRealisticCameras(bool same_K) {
  TwoViewDataSet d;

  d.K1 << 320,   0, 160,
            0, 320, 120,
            0,   0,   1;
  if (same_K) {
    d.K2 = d.K1;
  } else {
    d.K2 << 360,   0, 170,
              0, 360, 110,
              0,   0,   1;
  }
  d.R1 = RotationAroundZ(-0.1);
  d.R2 = RotationAroundX(-0.1);
  d.t1 << 1, 1, 10;
  d.t2 << -2, -1, 10;
  P_From_KRt(d.K1, d.R1, d.t1, &d.P1);
  P_From_KRt(d.K2, d.R2, d.t2, &d.P2);

  FundamentalFromProjections(d.P1, d.P2, &d.F);

  d.X.resize(3, 30);
  d.X.setRandom();

  Project(d.P1, d.X, &d.x1);
  Project(d.P2, d.X, &d.x2);

  return d;
}

nViewDatasetConfigator::nViewDatasetConfigator(int fx ,  int fy,
                                               int cx,   int cy,
                                               double distance,
                                               double jitter_amount) {
  _fx = fx;
  _fy = fy;
  _cx = cx;
  _cy = cy;
  _dist = distance;
  _jitter_amount = jitter_amount;
}

NViewDataSet NRealisticCamerasFull(int nviews, int npoints,
                                   const nViewDatasetConfigator config) {
  NViewDataSet d;
  d.n = nviews;
  d.K.resize(nviews);
  d.R.resize(nviews);
  d.t.resize(nviews);
  d.C.resize(nviews);
  d.x.resize(nviews);
  d.x_ids.resize(nviews);

  d.X.resize(3, npoints);
  d.X.setRandom();
  d.X *= 0.6;

  Vecu all_point_ids(npoints);
  for (size_t j = 0; j < npoints; ++j)
    all_point_ids[j] = j;

  for (size_t i = 0; i < nviews; ++i) {
    Vec3 camera_center, t, jitter, lookdir;

    double theta = i * 2 * M_PI / nviews;
    camera_center << sin(theta), 0.0, cos(theta);
    camera_center *= config._dist;
    d.C[i] = camera_center;

    jitter.setRandom();
    jitter *= config._jitter_amount / camera_center.norm();
    lookdir = -camera_center + jitter;

    d.K[i] << config._fx, 0,          config._cx,
               0,         config._fy, config._cy,
               0,         0,          1;
    d.R[i] = LookAt(lookdir);
    d.t[i] = -d.R[i] * camera_center;
    d.x[i] = Project(d.P(i), d.X);
    d.x_ids[i] = all_point_ids;
  }
  return d;
}


NViewDataSet NRealisticCamerasSparse(int nviews, int npoints,
                                     float view_ratio, unsigned min_projections,
                                     const nViewDatasetConfigator config) {
  assert(view_ratio <= 1.0);
  assert(view_ratio > 0.0);
  assert(min_projections <= npoints);
  NViewDataSet d;
  d.n = nviews;
  d.K.resize(nviews);
  d.R.resize(nviews);
  d.t.resize(nviews);
  d.C.resize(nviews);
  d.x.resize(nviews);
  d.x_ids.resize(nviews);

  d.X.resize(3, npoints);
  d.X.setRandom();
  d.X *= 0.6;

  Mat visibility(nviews, npoints);
  visibility.setZero();
  Mat randoms(nviews, npoints);
  randoms.setRandom();
  randoms = (randoms.array() + 1)/2.0;
  unsigned num_visibles = 0;
  for (size_t i = 0; i < nviews; ++i) {
    num_visibles = 0;
    for (size_t j = 0; j < npoints; j++) {
      if (randoms(i, j) <= view_ratio) {
        visibility(i, j) = true;
        num_visibles++;
      }
    }
    if (num_visibles < min_projections) {
      unsigned num_projections_to_add = min_projections - num_visibles;
      for (size_t j = 0; j < npoints && num_projections_to_add > 0; ++j) {
        if (!visibility(i, j)) {
          num_projections_to_add--;
        }
      }
      num_visibles += num_projections_to_add;
    }
    d.x_ids[i].resize(num_visibles);
    d.x[i].resize(2, num_visibles);
  }

  size_t j_visible = 0;
  Vec3 X;
  for (size_t i = 0; i < nviews; ++i) {
    Vec3 camera_center, t, jitter, lookdir;

    double theta = i * 2 * M_PI / nviews;
    camera_center << sin(theta), 0.0, cos(theta);
    camera_center *= config._dist;
    d.C[i] = camera_center;

    jitter.setRandom();
    jitter *= config._jitter_amount / camera_center.norm();
    lookdir = -camera_center + jitter;

    d.K[i] << config._fx, 0,          config._cx,
               0,         config._fy, config._cy,
               0,         0,          1;
    d.R[i] = LookAt(lookdir);
    d.t[i] = -d.R[i] * camera_center;
    j_visible = 0;
    for (size_t j = 0; j < npoints; j++) {
      if (visibility(i, j)) {
        X =  d.X.col(j);
        d.x[i].col(j_visible) = Project(d.P(i), X);
        d.x_ids[i][j_visible] = j;
        j_visible++;
      }
    }
  }
  return d;
}


}  // namespace libmv
