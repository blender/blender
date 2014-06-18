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

#include "libmv/logging/logging.h"
#include "libmv/multiview/conditioning.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/multiview/test_data_sets.h"
#include "libmv/numeric/numeric.h"
#include "testing/testing.h"

namespace {

using namespace libmv;

TEST(Fundamental, FundamentalFromProjections) {
  Mat34 P1_gt, P2_gt;
  P1_gt << 1, 0, 0, 0,
           0, 1, 0, 0,
           0, 0, 1, 0;
  P2_gt << 1, 1, 1, 3,
           0, 2, 0, 3,
           0, 1, 1, 0;
  Mat3 F_gt;
  FundamentalFromProjections(P1_gt, P2_gt, &F_gt);

  Mat34 P1, P2;
  ProjectionsFromFundamental(F_gt, &P1, &P2);

  Mat3 F;
  FundamentalFromProjections(P1, P2, &F);

  EXPECT_MATRIX_PROP(F_gt, F, 1e-6);
}

TEST(Fundamental, PreconditionerFromPoints) {
  int n = 4;
  Mat points(2, n);
  points << 0, 0, 1, 1,
            0, 2, 1, 3;

  Mat3 T;
  PreconditionerFromPoints(points, &T);

  Mat normalized_points;
  ApplyTransformationToPoints(points, T, &normalized_points);

  Vec mean, variance;
  MeanAndVarianceAlongRows(normalized_points, &mean, &variance);

  EXPECT_NEAR(0, mean(0), 1e-8);
  EXPECT_NEAR(0, mean(1), 1e-8);
  EXPECT_NEAR(2, variance(0), 1e-8);
  EXPECT_NEAR(2, variance(1), 1e-8);
}

TEST(Fundamental, EssentialFromFundamental) {
  TwoViewDataSet d = TwoRealisticCameras();

  Mat3 E_from_Rt;
  EssentialFromRt(d.R1, d.t1, d.R2, d.t2, &E_from_Rt);

  Mat3 E_from_F;
  EssentialFromFundamental(d.F, d.K1, d.K2, &E_from_F);

  EXPECT_MATRIX_PROP(E_from_Rt, E_from_F, 1e-6);
}

TEST(Fundamental, MotionFromEssential) {
  TwoViewDataSet d = TwoRealisticCameras();

  Mat3 E;
  EssentialFromRt(d.R1, d.t1, d.R2, d.t2, &E);

  Mat3 R;
  Vec3 t;
  RelativeCameraMotion(d.R1, d.t1, d.R2, d.t2, &R, &t);
  NormalizeL2(&t);

  std::vector<Mat3> Rs;
  std::vector<Vec3> ts;
  MotionFromEssential(E, &Rs, &ts);
  bool one_solution_is_correct = false;
  for (size_t i = 0; i < Rs.size(); ++i) {
    if (FrobeniusDistance(Rs[i], R) < 1e-8 && DistanceL2(ts[i], t) < 1e-8) {
      one_solution_is_correct = true;
      break;
    }
  }
  EXPECT_TRUE(one_solution_is_correct);
}

TEST(Fundamental, MotionFromEssentialChooseSolution) {
  TwoViewDataSet d = TwoRealisticCameras();

  Mat3 E;
  EssentialFromRt(d.R1, d.t1, d.R2, d.t2, &E);

  Mat3 R;
  Vec3 t;
  RelativeCameraMotion(d.R1, d.t1, d.R2, d.t2, &R, &t);
  NormalizeL2(&t);

  std::vector<Mat3> Rs;
  std::vector<Vec3> ts;
  MotionFromEssential(E, &Rs, &ts);

  Vec2 x1, x2;
  MatrixColumn(d.x1, 0, &x1);
  MatrixColumn(d.x2, 0, &x2);
  int solution = MotionFromEssentialChooseSolution(Rs, ts, d.K1, x1, d.K2, x2);

  EXPECT_LE(0, solution);
  EXPECT_LE(solution, 3);
  EXPECT_LE(FrobeniusDistance(Rs[solution], R), 1e-8);
  EXPECT_LE(DistanceL2(ts[solution], t), 1e-8);
}

TEST(Fundamental, MotionFromEssentialAndCorrespondence) {
  TwoViewDataSet d = TwoRealisticCameras();

  Mat3 E;
  EssentialFromRt(d.R1, d.t1, d.R2, d.t2, &E);

  Mat3 R;
  Vec3 t;
  RelativeCameraMotion(d.R1, d.t1, d.R2, d.t2, &R, &t);
  NormalizeL2(&t);

  Vec2 x1, x2;
  MatrixColumn(d.x1, 0, &x1);
  MatrixColumn(d.x2, 0, &x2);

  Mat3 R_estimated;
  Vec3 t_estimated;
  MotionFromEssentialAndCorrespondence(E, d.K1, x1, d.K2, x2,
                                       &R_estimated, &t_estimated);

  EXPECT_LE(FrobeniusDistance(R_estimated, R), 1e-8);
  EXPECT_LE(DistanceL2(t_estimated, t), 1e-8);
}

}  // namespace
