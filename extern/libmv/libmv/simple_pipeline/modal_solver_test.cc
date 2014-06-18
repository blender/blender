// Copyright (c) 2013 libmv authors.
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

#include "libmv/simple_pipeline/modal_solver.h"

#include "testing/testing.h"
#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

#include <stdio.h>

namespace libmv {

TEST(ModalSolver, SyntheticCubeSceneMotion) {
  double kTolerance = 1e-8;

  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(658.286, 658.286);
  intrinsics.SetPrincipalPoint(480.0, 270.0);
  intrinsics.SetRadialDistortion(0.0, 0.0, 0.0);

  Marker markers[] = {
      {1, 0, 212.172775, 354.713538, 1.0}, {2, 0, 773.468399, 358.735306, 1.0},
      {1, 1, 62.415197, 287.905354, 1.0},  {2, 1, 619.103336, 324.402537, 1.0},
      {1, 2, 206.847939, 237.567925, 1.0}, {2, 2, 737.496986, 247.881383, 1.0},
      {1, 3, 351.743889, 316.415906, 1.0}, {2, 3, 908.779621, 290.703617, 1.0},
      {1, 4, 232.941413, 54.265443, 1.0},  {2, 4, 719.444847, 63.062531, 1.0},
      {1, 5, 96.391611, 119.283537, 1.0},  {2, 5, 611.413136, 160.890715, 1.0},
      {1, 6, 363.444958, 150.838144, 1.0}, {2, 6, 876.374531, 114.916206, 1.0},
    };
  int num_markers = sizeof(markers) / sizeof(Marker);

  Tracks tracks;
  for (int i = 0; i < num_markers; i++) {
    double x = markers[i].x, y = markers[i].y;
    intrinsics.InvertIntrinsics(x, y, &x, &y);
    tracks.Insert(markers[i].image, markers[i].track, x, y);
  }

  EuclideanReconstruction reconstruction;
  ModalSolver(tracks, &reconstruction);
  EuclideanBundleCommonIntrinsics(tracks,
                                  BUNDLE_NO_INTRINSICS,
                                  BUNDLE_NO_TRANSLATION,
                                  &reconstruction,
                                  &intrinsics,
                                  NULL);

  Mat3 expected_rotation;
  expected_rotation << 0.98215101299251, 0.17798357184544,  0.06083778292258,
                      -0.16875286001759, 0.97665299913606, -0.13293378620359,
                      -0.08307743323957, 0.12029450291547,  0.98925596922871;

  Mat3 &first_camera_R = reconstruction.CameraForImage(1)->R;
  Mat3 &second_camera_R = reconstruction.CameraForImage(2)->R;

  EXPECT_TRUE(Mat3::Identity().isApprox(first_camera_R, kTolerance));
  EXPECT_TRUE(expected_rotation.isApprox(second_camera_R, kTolerance));
}

}  // namespace libmv
