// Copyright (c) 2011, 2012, 2013 libmv authors.
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

#include "libmv/simple_pipeline/bundle.h"

#include <map>

#include "ceres/ceres.h"
#include "ceres/rotation.h"
#include "libmv/base/scoped_ptr.h"
#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace libmv {

// The intrinsics need to get combined into a single parameter block; use these
// enums to index instead of numeric constants.
enum {
  OFFSET_FOCAL_LENGTH,
  OFFSET_PRINCIPAL_POINT_X,
  OFFSET_PRINCIPAL_POINT_Y,
  OFFSET_K1,
  OFFSET_K2,
  OFFSET_K3,
  OFFSET_P1,
  OFFSET_P2,
};

namespace {

struct OpenCVReprojectionError {
  OpenCVReprojectionError(double observed_x, double observed_y)
      : observed_x(observed_x), observed_y(observed_y) {}

  template <typename T>
  bool operator()(const T* const intrinsics,
                  const T* const R_t,  // Rotation denoted by angle axis
                                       // followed with translation
                  const T* const X,  // Point coordinates 3x1.
                  T* residuals) const {
    // Unpack the intrinsics.
    const T& focal_length      = intrinsics[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = intrinsics[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = intrinsics[OFFSET_PRINCIPAL_POINT_Y];
    const T& k1                = intrinsics[OFFSET_K1];
    const T& k2                = intrinsics[OFFSET_K2];
    const T& k3                = intrinsics[OFFSET_K3];
    const T& p1                = intrinsics[OFFSET_P1];
    const T& p2                = intrinsics[OFFSET_P2];

    // Compute projective coordinates: x = RX + t.
    T x[3];

    ceres::AngleAxisRotatePoint(R_t, X, x);
    x[0] += R_t[3];
    x[1] += R_t[4];
    x[2] += R_t[5];

    // Compute normalized coordinates: x /= x[2].
    T xn = x[0] / x[2];
    T yn = x[1] / x[2];

    T predicted_x, predicted_y;

    // EuclideanBundle uses empty intrinsics, which breaks undistortion code;
    // so use an implied focal length of 1.0 if the focal length is exactly
    // zero.
    // TODO(keir): Figure out a better way to do this.
    if (focal_length != T(0)) {
      // Apply distortion to the normalized points to get (xd, yd).
      // TODO(keir): Do early bailouts for zero distortion; these are expensive
      // jet operations.

      ApplyRadialDistortionCameraIntrinsics(focal_length,
                                            focal_length,
                                            principal_point_x,
                                            principal_point_y,
                                            k1, k2, k3,
                                            p1, p2,
                                            xn, yn,
                                            &predicted_x,
                                            &predicted_y);
    } else {
      predicted_x = xn;
      predicted_y = yn;
    }

    // The error is the difference between the predicted and observed position.
    residuals[0] = predicted_x - T(observed_x);
    residuals[1] = predicted_y - T(observed_y);

    return true;
  }

  double observed_x;
  double observed_y;
};

void BundleIntrinsicsLogMessage(int bundle_intrinsics) {
  if (bundle_intrinsics == BUNDLE_NO_INTRINSICS) {
    LG << "Bundling only camera positions.";
  } else if (bundle_intrinsics == BUNDLE_FOCAL_LENGTH) {
    LG << "Bundling f.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT)) {
    LG << "Bundling f, px, py.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT |
                                   BUNDLE_RADIAL)) {
    LG << "Bundling f, px, py, k1, k2.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT |
                                   BUNDLE_RADIAL |
                                   BUNDLE_TANGENTIAL)) {
    LG << "Bundling f, px, py, k1, k2, p1, p2.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL |
                                   BUNDLE_TANGENTIAL)) {
    LG << "Bundling f, px, py, k1, k2, p1, p2.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL)) {
    LG << "Bundling f, k1, k2.";
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL_K1)) {
    LG << "Bundling f, k1.";
  } else if (bundle_intrinsics == (BUNDLE_RADIAL_K1 |
                                   BUNDLE_RADIAL_K2)) {
    LG << "Bundling k1, k2.";
  } else {
    LOG(FATAL) << "Unsupported bundle combination.";
  }
}

// Pack intrinsics from object to an array for easier
// and faster minimization
void PackIntrinisicsIntoArray(CameraIntrinsics *intrinsics,
                              double ceres_intrinsics[8]) {
  ceres_intrinsics[OFFSET_FOCAL_LENGTH]       = intrinsics->focal_length();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_X]  = intrinsics->principal_point_x();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_Y]  = intrinsics->principal_point_y();
  ceres_intrinsics[OFFSET_K1]                 = intrinsics->k1();
  ceres_intrinsics[OFFSET_K2]                 = intrinsics->k2();
  ceres_intrinsics[OFFSET_K3]                 = intrinsics->k3();
  ceres_intrinsics[OFFSET_P1]                 = intrinsics->p1();
  ceres_intrinsics[OFFSET_P2]                 = intrinsics->p2();
}

// Unpack intrinsics back from an array to an object
void UnpackIntrinsicsFromArray(CameraIntrinsics *intrinsics,
                               double ceres_intrinsics[8]) {
    intrinsics->SetFocalLength(ceres_intrinsics[OFFSET_FOCAL_LENGTH],
                               ceres_intrinsics[OFFSET_FOCAL_LENGTH]);

    intrinsics->SetPrincipalPoint(ceres_intrinsics[OFFSET_PRINCIPAL_POINT_X],
                                  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_Y]);

    intrinsics->SetRadialDistortion(ceres_intrinsics[OFFSET_K1],
                                    ceres_intrinsics[OFFSET_K2],
                                    ceres_intrinsics[OFFSET_K3]);

    intrinsics->SetTangentialDistortion(ceres_intrinsics[OFFSET_P1],
                                        ceres_intrinsics[OFFSET_P2]);
}

// Get a vector of camera's rotations denoted by angle axis
// conjuncted with translations into single block
//
// Element with index i matches to a rotation+translation for
// camera at image i.
vector<Vec6> PackCamerasRotationAndTranslation(
                                 const Tracks &tracks,
                                 EuclideanReconstruction *reconstruction) {
  vector<Vec6> cameras_R_t;
  int max_image = tracks.MaxImage();

  cameras_R_t.resize(max_image + 1);

  for (int i = 0; i <= max_image; i++) {
    EuclideanCamera *camera = reconstruction->CameraForImage(i);

    if (!camera)
      continue;

    ceres::RotationMatrixToAngleAxis(&camera->R(0, 0),
                                     &cameras_R_t[i](0));
    cameras_R_t[i].tail<3>() = camera->t;
  }

  return cameras_R_t;
}

// Convert cameras rotations fro mangle axis back to rotation matrix
void UnpackCamerasRotationAndTranslation(
                                  const Tracks &tracks,
                                  EuclideanReconstruction *reconstruction,
                                  vector<Vec6> cameras_R_t) {
  int max_image = tracks.MaxImage();

  for (int i = 0; i <= max_image; i++) {
    EuclideanCamera *camera = reconstruction->CameraForImage(i);

    if (!camera)
      continue;

    ceres::AngleAxisToRotationMatrix(&cameras_R_t[i](0),
                                     &camera->R(0, 0));
    camera->t = cameras_R_t[i].tail<3>();
  }
}

}  // namespace

void EuclideanBundle(const Tracks &tracks,
                     EuclideanReconstruction *reconstruction) {
  CameraIntrinsics intrinsics;
  EuclideanBundleCommonIntrinsics(tracks,
                                  BUNDLE_NO_INTRINSICS,
                                  reconstruction,
                                  &intrinsics);
}

void EuclideanBundleCommonIntrinsics(const Tracks &tracks,
                                     int bundle_intrinsics,
                                     EuclideanReconstruction *reconstruction,
                                     CameraIntrinsics *intrinsics,
                                     int bundle_constraints) {
  LG << "Original intrinsics: " << *intrinsics;
  vector<Marker> markers = tracks.AllMarkers();

  ceres::Problem::Options problem_options;
  ceres::Problem problem(problem_options);

  // Residual blocks with 10 parameters are unwieldly with Ceres, so pack the
  // intrinsics into a single block and rely on local parameterizations to
  // control which intrinsics are allowed to vary.
  double ceres_intrinsics[8];
  PackIntrinisicsIntoArray(intrinsics, ceres_intrinsics);

  // Convert cameras rotations to angle axis and merge with translation
  // into single parameter block for maximal minimization speed
  //
  // Block for minimization has got the following structure:
  //   <3 elements for angle-axis> <3 elements for translation>
  vector<Vec6> cameras_R_t =
    PackCamerasRotationAndTranslation(tracks, reconstruction);

  int num_residuals = 0;
  bool have_locked_camera = false;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    EuclideanCamera *camera = reconstruction->CameraForImage(marker.image);
    EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
    if (!camera || !point) {
      continue;
    }

    // Rotation of camera denoted in angle axis
    double *camera_R_t = &cameras_R_t[camera->image] (0);

    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<
        OpenCVReprojectionError, 2, 8, 6, 3>(
            new OpenCVReprojectionError(
                marker.x,
                marker.y)),
        NULL,
        ceres_intrinsics,
        camera_R_t,
        &point->X(0));

    // We lock first camera for better deal with
    // scene orientation ambiguity
    if (!have_locked_camera) {
      problem.SetParameterBlockConstant(camera_R_t);
      have_locked_camera = true;
    }

    if (bundle_constraints & BUNDLE_NO_TRANSLATION)
      problem.SetParameterBlockConstant(&camera->t(0));

    num_residuals++;
  }
  LG << "Number of residuals: " << num_residuals;

  if (!num_residuals) {
    LG << "Skipping running minimizer with zero residuals";
    return;
  }

  BundleIntrinsicsLogMessage(bundle_intrinsics);

  if (bundle_intrinsics == BUNDLE_NO_INTRINSICS) {
    // No camera intrinsics are refining,
    // set the whole parameter block as constant for best performance
    problem.SetParameterBlockConstant(ceres_intrinsics);
  } else {
    // Set intrinsics not being bundles as constant

    std::vector<int> constant_intrinsics;
#define MAYBE_SET_CONSTANT(bundle_enum, offset) \
    if (!(bundle_intrinsics & bundle_enum)) { \
      constant_intrinsics.push_back(offset); \
    }
    MAYBE_SET_CONSTANT(BUNDLE_FOCAL_LENGTH,    OFFSET_FOCAL_LENGTH);
    MAYBE_SET_CONSTANT(BUNDLE_PRINCIPAL_POINT, OFFSET_PRINCIPAL_POINT_X);
    MAYBE_SET_CONSTANT(BUNDLE_PRINCIPAL_POINT, OFFSET_PRINCIPAL_POINT_Y);
    MAYBE_SET_CONSTANT(BUNDLE_RADIAL_K1,       OFFSET_K1);
    MAYBE_SET_CONSTANT(BUNDLE_RADIAL_K2,       OFFSET_K2);
    MAYBE_SET_CONSTANT(BUNDLE_TANGENTIAL_P1,   OFFSET_P1);
    MAYBE_SET_CONSTANT(BUNDLE_TANGENTIAL_P2,   OFFSET_P2);
#undef MAYBE_SET_CONSTANT

    // Always set K3 constant, it's not used at the moment.
    constant_intrinsics.push_back(OFFSET_K3);

    ceres::SubsetParameterization *subset_parameterization =
      new ceres::SubsetParameterization(8, constant_intrinsics);

    problem.SetParameterization(ceres_intrinsics, subset_parameterization);
  }

  // Configure the solver
  ceres::Solver::Options options;
  options.use_nonmonotonic_steps = true;
  options.preconditioner_type = ceres::SCHUR_JACOBI;
  options.linear_solver_type = ceres::ITERATIVE_SCHUR;
  options.use_inner_iterations = true;
  options.max_num_iterations = 100;

#ifdef _OPENMP
  options.num_threads = omp_get_max_threads();
  options.num_linear_solver_threads = omp_get_max_threads();
#endif

  // Solve!
  ceres::Solver::Summary summary;
  ceres::Solve(options, &problem, &summary);

  LG << "Final report:\n" << summary.FullReport();

  // Copy rotations and translations back.
  UnpackCamerasRotationAndTranslation(tracks,
                                      reconstruction,
                                      cameras_R_t);

  // Copy intrinsics back.
  if (bundle_intrinsics != BUNDLE_NO_INTRINSICS)
    UnpackIntrinsicsFromArray(intrinsics, ceres_intrinsics);

  LG << "Final intrinsics: " << *intrinsics;
}

void ProjectiveBundle(const Tracks & /*tracks*/,
                      ProjectiveReconstruction * /*reconstruction*/) {
  // TODO(keir): Implement this! This can't work until we have a better bundler
  // than SSBA, since SSBA has no support for projective bundling.
}

}  // namespace libmv
