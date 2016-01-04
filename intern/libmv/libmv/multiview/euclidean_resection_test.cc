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

#include "libmv/multiview/euclidean_resection.h"
#include "libmv/numeric/numeric.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/projection.h"
#include "testing/testing.h"

using namespace libmv::euclidean_resection;
using namespace libmv;

// Generates all necessary inputs and expected outputs for EuclideanResection.
void CreateCameraSystem(const Mat3& KK,
                        const Mat3X& x_image,
                        const Vec& X_distances,
                        const Mat3& R_input,
                        const Vec3& T_input,
                        Mat2X *x_camera,
                        Mat3X *X_world,
                        Mat3  *R_expected,
                        Vec3  *T_expected) {
  int num_points = x_image.cols();

  Mat3X x_unit_cam(3, num_points);
  x_unit_cam = KK.inverse() * x_image;

  // Create normalized camera coordinates to be used as an input to the PnP
  // function, instead of using NormalizeColumnVectors(&x_unit_cam).
  *x_camera = x_unit_cam.block(0, 0, 2, num_points);
  for (int i = 0; i < num_points; ++i) {
    x_unit_cam.col(i).normalize();
  }

  // Create the 3D points in the camera system.
  Mat X_camera(3, num_points);
  for (int i = 0; i < num_points; ++i) {
    X_camera.col(i) = X_distances(i) * x_unit_cam.col(i);
  }

  // Apply the transformation to the camera 3D points
  Mat translation_matrix(3, num_points);
  translation_matrix.row(0).setConstant(T_input(0));
  translation_matrix.row(1).setConstant(T_input(1));
  translation_matrix.row(2).setConstant(T_input(2));

  *X_world = R_input * X_camera + translation_matrix;

  // Create the expected result for comparison.
  *R_expected = R_input.transpose();
  *T_expected = *R_expected * (-T_input);
};

TEST(AbsoluteOrientation, QuaternionSolution) {
  int num_points = 4;
  Mat X;
  Mat Xp;
  X = 100 * Mat::Random(3, num_points);

  // Create a random translation and rotation.
  Mat3 R_input;
  R_input = Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ());

  Vec3 t_input;
  t_input.setRandom();
  t_input = 100 * t_input;

  Mat translation_matrix(3, num_points);
  translation_matrix.row(0).setConstant(t_input(0));
  translation_matrix.row(1).setConstant(t_input(1));
  translation_matrix.row(2).setConstant(t_input(2));

  // Create the transformed 3D points Xp as Xp = R * X + t.
  Xp = R_input * X + translation_matrix;

  // Output variables.
  Mat3 R;
  Vec3 t;

  AbsoluteOrientation(X, Xp, &R, &t);

  EXPECT_MATRIX_NEAR(t, t_input, 1e-6);
  EXPECT_MATRIX_NEAR(R, R_input, 1e-8);
}

TEST(EuclideanResection, Points4KnownImagePointsRandomTranslationRotation) {
  // In this test only the translation and rotation are random. The image
  // points are selected from a real case and are well conditioned.
  Vec2i image_dimensions;
  image_dimensions << 1600, 1200;

  Mat3 KK;
  KK << 2796, 0,     804,
        0 ,   2796,  641,
        0,    0,     1;

  // The real image points.
  int num_points = 4;
  Mat3X x_image(3, num_points);
  x_image << 1164.06, 734.948, 749.599, 430.727,
             681.386, 844.59, 496.315,  580.775,
             1,       1,      1,        1;


  // A vector of the 4 distances to the 3D points.
  Vec X_distances = 100 * Vec::Random(num_points).array().abs();

  // Create the random camera motion R and t that resection should recover.
  Mat3 R_input;
  R_input = Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ());

  Vec3 T_input;
  T_input.setRandom();
  T_input = 100 * T_input;

  // Create the camera system, also getting the expected result of the
  // transformation.
  Mat3 R_expected;
  Vec3 T_expected;
  Mat3X X_world;
  Mat2X x_camera;
  CreateCameraSystem(KK, x_image, X_distances, R_input, T_input,
                     &x_camera, &X_world, &R_expected, &T_expected);

  // Finally, run the code under test.
  Mat3 R_output;
  Vec3 T_output;
  EuclideanResection(x_camera, X_world,
                     &R_output, &T_output,
                     RESECTION_ANSAR_DANIILIDIS);

  EXPECT_MATRIX_NEAR(T_output, T_expected, 1e-5);
  EXPECT_MATRIX_NEAR(R_output, R_expected, 1e-7);

  // For now, the EPnP doesn't have a non-linear optimization step and so is
  // not precise enough with only 4 points.
  //
  // TODO(jmichot): Reenable this test when there is nonlinear refinement.
#if 0
  R_output.setIdentity();
  T_output.setZero();

  EuclideanResection(x_camera, X_world,
                     &R_output, &T_output,
                     RESECTION_EPNP);

  EXPECT_MATRIX_NEAR(T_output, T_expected, 1e-5);
  EXPECT_MATRIX_NEAR(R_output, R_expected, 1e-7);*/
#endif
}

// TODO(jmichot): Reduce the code duplication here with the code above.
TEST(EuclideanResection, Points6AllRandomInput) {
  Mat3 KK;
  KK << 2796, 0,    804,
        0 ,   2796, 641,
        0,    0,    1;

  // Create random image points for a 1600x1200 image.
  int w = 1600;
  int h = 1200;
  int num_points = 6;
  Mat3X x_image(3, num_points);
  x_image.row(0) = w * Vec::Random(num_points).array().abs();
  x_image.row(1) = h * Vec::Random(num_points).array().abs();
  x_image.row(2).setOnes();

  // Normalized camera coordinates to be used as an input to the PnP function.
  Mat2X x_camera;
  Vec X_distances = 100 * Vec::Random(num_points).array().abs();

  // Create the random camera motion R and t that resection should recover.
  Mat3 R_input;
  R_input = Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitY())
          * Eigen::AngleAxisd(rand(), Eigen::Vector3d::UnitZ());

  Vec3 T_input;
  T_input.setRandom();
  T_input = 100 * T_input;

  // Create the camera system.
  Mat3 R_expected;
  Vec3 T_expected;
  Mat3X X_world;
  CreateCameraSystem(KK, x_image, X_distances, R_input, T_input,
                     &x_camera, &X_world, &R_expected, &T_expected);

  // Test each of the resection methods.
  {
    Mat3 R_output;
    Vec3 T_output;
    EuclideanResection(x_camera, X_world,
                       &R_output, &T_output,
                       RESECTION_ANSAR_DANIILIDIS);
    EXPECT_MATRIX_NEAR(T_output, T_expected, 1e-5);
    EXPECT_MATRIX_NEAR(R_output, R_expected, 1e-7);
  }
  {
    Mat3 R_output;
    Vec3 T_output;
    EuclideanResection(x_camera, X_world,
                       &R_output, &T_output,
                       RESECTION_EPNP);
    EXPECT_MATRIX_NEAR(T_output, T_expected, 1e-5);
    EXPECT_MATRIX_NEAR(R_output, R_expected, 1e-7);
  }
  {
    Mat3 R_output;
    Vec3 T_output;
    EuclideanResection(x_image, X_world, KK,
                       &R_output, &T_output);
    EXPECT_MATRIX_NEAR(T_output, T_expected, 1e-5);
    EXPECT_MATRIX_NEAR(R_output, R_expected, 1e-7);
  }
}
