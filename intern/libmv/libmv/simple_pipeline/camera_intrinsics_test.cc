// Copyright (c) 2011 libmv authors.
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

#include "libmv/simple_pipeline/camera_intrinsics.h"

#include <iostream>

#include "testing/testing.h"
#include "libmv/image/image.h"
#include "libmv/image/image_drawing.h"
#include "libmv/logging/logging.h"

namespace libmv {

TEST(PolynomialCameraIntrinsics2, ApplyOnFocalCenter) {
  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(1300.0, 1300.0);
  intrinsics.SetPrincipalPoint(640.0, 540.0);
  intrinsics.SetRadialDistortion(-0.2, -0.1, -0.05);

  double distorted_x, distorted_y;
  intrinsics.ApplyIntrinsics(0.0, 0.0, &distorted_x, &distorted_y);

  EXPECT_NEAR(640.0, distorted_x, 1e-8);
  EXPECT_NEAR(540.0, distorted_y, 1e-8);
}

TEST(PolynomialCameraIntrinsics, InvertOnFocalCenter) {
  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(1300.0, 1300.0);
  intrinsics.SetPrincipalPoint(640.0, 540.0);
  intrinsics.SetRadialDistortion(-0.2, -0.1, -0.05);

  double normalized_x, normalized_y;
  intrinsics.InvertIntrinsics(640.0, 540.0, &normalized_x, &normalized_y);

  EXPECT_NEAR(0.0, normalized_x, 1e-8);
  EXPECT_NEAR(0.0, normalized_y, 1e-8);
}

TEST(PolynomialCameraIntrinsics, ApplyIntrinsics) {
  const int N = 5;

  double expected[N][N][2] = {
      { {75.312500,   -24.687500}, {338.982239, -62.035522},
        {640.000000,  -72.929688}, {941.017761, -62.035522},
        {1204.687500, -24.687500}},

      { {37.964478,  238.982239}, {323.664551, 223.664551},
        {640.000000, 219.193420}, {956.335449, 223.664551},
        {1242.035522, 238.982239}},

      { {27.070312,  540.000000}, {319.193420, 540.000000},
        {640.000000, 540.000000}, {960.806580, 540.000000},
        {1252.929688, 540.000000}},

      { {37.964478,  841.017761}, {323.664551, 856.335449},
        {640.000000, 860.806580}, {956.335449, 856.335449},
        {1242.035522, 841.017761}},

      { {75.312500,  1104.687500}, {338.982239, 1142.035522},
        {640.000000, 1152.929688}, {941.017761, 1142.035522},
        {1204.687500, 1104.687500}}
    };

  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(1300.0, 1300.0);
  intrinsics.SetPrincipalPoint(640.0, 540.0);
  intrinsics.SetRadialDistortion(-0.2, -0.1, -0.05);

  double step = 1.0 / (N - 1);

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      double normalized_x = j * step - 0.5,
             normalized_y = i * step - 0.5;

      double distorted_x, distorted_y;
      intrinsics.ApplyIntrinsics(normalized_x, normalized_y,
                                 &distorted_x, &distorted_y);

      EXPECT_NEAR(expected[i][j][0], distorted_x, 1e-6);
      EXPECT_NEAR(expected[i][j][1], distorted_y, 1e-6);
    }
  }
}

TEST(PolynomialCameraIntrinsics, InvertIntrinsics) {
  const int N = 5;

  double expected[N][N][2] = {
      { {-0.524482, -0.437069}, {-0.226237, -0.403994},
        { 0.031876, -0.398446}, { 0.293917, -0.408218},
        { 0.632438, -0.465028}},

      { {-0.493496, -0.189173}, {-0.219052, -0.179936},
        { 0.030975, -0.178107}, { 0.283742, -0.181280},
        { 0.574557, -0.194335}},

      { {-0.488013, 0.032534}, {-0.217537, 0.031077},
        { 0.030781, 0.030781}, { 0.281635, 0.031293},
        { 0.566344, 0.033314}},

      { {-0.498696, 0.257660}, {-0.220424, 0.244041},
        { 0.031150, 0.241409}, { 0.285660, 0.245985},
        { 0.582670, 0.265629}},

      { {-0.550617, 0.532263}, {-0.230399, 0.477255},
        { 0.032380, 0.469510}, { 0.299986, 0.483311},
        { 0.684740, 0.584043}}
    };

  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(1300.0, 1300.0);
  intrinsics.SetPrincipalPoint(600.0, 500.0);
  intrinsics.SetRadialDistortion(-0.2, -0.1, -0.05);

  double step_x = 1280.0 / (N - 1),
         step_y = 1080.0 / (N - 1);

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      double distorted_x = j * step_x,
             distorted_y = i * step_y;

      double normalized_x, normalized_y;
      intrinsics.InvertIntrinsics(distorted_x, distorted_y,
                                  &normalized_x, &normalized_y);

      EXPECT_NEAR(expected[i][j][0], normalized_x, 1e-6);
      EXPECT_NEAR(expected[i][j][1], normalized_y, 1e-6);
    }
  }
}

TEST(PolynomialCameraIntrinsics, ApplyIsInvertibleSimple) {
  PolynomialCameraIntrinsics intrinsics;
  intrinsics.SetFocalLength(1300.0, 1300.0);
  intrinsics.SetPrincipalPoint(640.0, 540.0);
  intrinsics.SetRadialDistortion(-0.2, -0.1, -0.05);

  // Scan over image coordinates, invert the intrinsics, then re-apply them to
  // make sure the cycle gets back where it started.
  for (double y = 0; y < 1000; y += 100) {
    for (double x = 0; x < 1000; x += 100) {
      double normalized_x, normalized_y;
      intrinsics.InvertIntrinsics(x, y, &normalized_x, &normalized_y);

      double xp, yp;
      intrinsics.ApplyIntrinsics(normalized_x, normalized_y, &xp, &yp);

      EXPECT_NEAR(x, xp, 1e-8) << "y: " << y;
      EXPECT_NEAR(y, yp, 1e-8) << "x: " << x;
      LG << "Error x: " << (x - xp);
      LG << "Error y: " << (y - yp);
    }
  }
}

TEST(PolynomialCameraIntrinsics, IdentityDistortBuffer) {
  const int w = 101, h = 101;
  FloatImage image(h, w);
  image.Fill(0);

  DrawLine(0.0, h / 2.0, w - 1, h / 2.0, 1.0, &image);
  DrawLine(0.0, h / 4.0, w - 1, h / 4.0, 1.0, &image);
  DrawLine(0.0, h / 4.0 * 3.0, w - 1.0, h / 4.0 * 3.0, 1.0, &image);
  DrawLine(w / 2.0, 0.0, w / 2.0, h - 1.0, 1.0, &image);
  DrawLine(w / 4.0, 0.0, w / 4.0, h - 1.0, 1.0, &image);
  DrawLine(w / 4.0 * 3.0, 0.0, w / 4.0 * 3.0, h - 1.0, 1.0, &image);

  PolynomialCameraIntrinsics intrinsics;
  FloatImage distorted_image(h, w);
  intrinsics.SetImageSize(w, h);
  intrinsics.SetFocalLength(10.0, 10.0);
  intrinsics.SetPrincipalPoint((double) w / 2.0, (double) h / 2.0);
  intrinsics.SetRadialDistortion(0.0, 0.0, 0.0);
  intrinsics.DistortBuffer(image.Data(),
                           image.Width(), image.Height(),
                           0.0,
                           image.Depth(),
                           distorted_image.Data());

  for (int x = 0; x < image.Width(); ++x) {
    for (int y = 0; y < image.Height(); ++y) {
      EXPECT_EQ(image(y, x), distorted_image(y, x));
    }
  }
}

TEST(PolynomialCameraIntrinsics, IdentityUndistortBuffer) {
  const int w = 101, h = 101;
  FloatImage image(h, w);
  image.Fill(0);

  DrawLine(0.0, h / 2.0, w - 1, h / 2.0, 1.0, &image);
  DrawLine(0.0, h / 4.0, w - 1, h / 4.0, 1.0, &image);
  DrawLine(0.0, h / 4.0 * 3.0, w - 1.0, h / 4.0 * 3.0, 1.0, &image);
  DrawLine(w / 2.0, 0.0, w / 2.0, h - 1.0, 1.0, &image);
  DrawLine(w / 4.0, 0.0, w / 4.0, h - 1.0, 1.0, &image);
  DrawLine(w / 4.0 * 3.0, 0.0, w / 4.0 * 3.0, h - 1.0, 1.0, &image);

  PolynomialCameraIntrinsics intrinsics;
  FloatImage distorted_image(h, w);
  intrinsics.SetImageSize(w, h);
  intrinsics.SetFocalLength(10.0, 10.0);
  intrinsics.SetPrincipalPoint((double) w / 2.0, (double) h / 2.0);
  intrinsics.SetRadialDistortion(0.0, 0.0, 0.0);
  intrinsics.UndistortBuffer(image.Data(),
                             image.Width(), image.Height(),
                             0.0,
                             image.Depth(),
                             distorted_image.Data());

  for (int x = 0; x < image.Width(); ++x) {
    for (int y = 0; y < image.Height(); ++y) {
      EXPECT_EQ(image(y, x), distorted_image(y, x));
    }
  }
}

}  // namespace libmv
