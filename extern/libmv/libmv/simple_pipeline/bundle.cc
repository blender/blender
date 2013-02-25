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
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"

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
                  const T* const R,  // Rotation 3x3 column-major.
                  const T* const t,  // Translation 3x1.
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
    x[0] = R[0]*X[0] + R[3]*X[1] + R[6]*X[2] + t[0];
    x[1] = R[1]*X[0] + R[4]*X[1] + R[7]*X[2] + t[1];
    x[2] = R[2]*X[0] + R[5]*X[1] + R[8]*X[2] + t[2];

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
      T r2 = xn*xn + yn*yn;
      T r4 = r2 * r2;
      T r6 = r4 * r2;
      T r_coeff = T(1) + k1*r2 + k2*r4 + k3*r6;
      T xd = xn * r_coeff + T(2)*p1*xn*yn + p2*(r2 + T(2)*xn*xn);
      T yd = yn * r_coeff + T(2)*p2*xn*yn + p1*(r2 + T(2)*yn*yn);

      // Apply focal length and principal point to get the final
      // image coordinates.
      predicted_x = focal_length * xd + principal_point_x;
      predicted_y = focal_length * yd + principal_point_y;
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

// TODO(keir): Get rid of the parameterization! Ceres will work much faster if
// the rotation block is angle-axis and also the translation is merged into a
// single parameter block.
struct RotationMatrixPlus {
  template<typename T>
  bool operator()(const T* R_array, // Rotation 3x3 col-major.
                  const T* delta,   // Angle-axis delta
                  T* R_plus_delta_array) const {
    T angle_axis[3];

    ceres::RotationMatrixToAngleAxis(R_array, angle_axis);

    angle_axis[0] += delta[0];
    angle_axis[1] += delta[1];
    angle_axis[2] += delta[2];

    ceres::AngleAxisToRotationMatrix(angle_axis, R_plus_delta_array);

    return true;
  }
};

// TODO(sergey): would be nice to have this in Ceres upstream
template<typename PlusFunctor, int kGlobalSize, int kLocalSize>
class AutodiffParameterization : public ceres::LocalParameterization {
 public:
  AutodiffParameterization(const PlusFunctor &plus_functor)
    : plus_functor_(plus_functor) {}

  virtual ~AutodiffParameterization() {}

  virtual bool Plus(const double* x,
                    const double* delta,
                    double* x_plus_delta) const {
    return plus_functor_(x, delta, x_plus_delta);
  }

  virtual bool ComputeJacobian(const double* x, double* jacobian) const {
    double zero_delta[kLocalSize] = { 0.0 };
    double x_plus_delta[kGlobalSize];
    const double* parameters[2] = { x, zero_delta };
    double* jacobians_array[2] = { NULL, jacobian };

    Plus(x, zero_delta, x_plus_delta);

    return ceres::internal::AutoDiff<PlusFunctor,
                              double,
                              kGlobalSize, kLocalSize>
        ::Differentiate(plus_functor_,
                        parameters,
                        kGlobalSize,
                        x_plus_delta,
                        jacobians_array);

    return true;
  }

  virtual int GlobalSize() const { return kGlobalSize; }
  virtual int LocalSize() const { return kLocalSize; }

 private:
  const PlusFunctor &plus_functor_;
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
  } else {
    LOG(FATAL) << "Unsupported bundle combination.";
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
                                     CameraIntrinsics *intrinsics) {
  LG << "Original intrinsics: " << *intrinsics;
  vector<Marker> markers = tracks.AllMarkers();

  ceres::Problem::Options problem_options;
  problem_options.local_parameterization_ownership =
    ceres::DO_NOT_TAKE_OWNERSHIP;

  ceres::Problem problem(problem_options);

  // Residual blocks with 10 parameters are unwieldly with Ceres, so pack the
  // intrinsics into a single block and rely on local parameterizations to
  // control which intrinsics are allowed to vary.
  double ceres_intrinsics[8];
  ceres_intrinsics[OFFSET_FOCAL_LENGTH]       = intrinsics->focal_length();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_X]  = intrinsics->principal_point_x();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_Y]  = intrinsics->principal_point_y();
  ceres_intrinsics[OFFSET_K1]                 = intrinsics->k1();
  ceres_intrinsics[OFFSET_K2]                 = intrinsics->k2();
  ceres_intrinsics[OFFSET_K3]                 = intrinsics->k3();
  ceres_intrinsics[OFFSET_P1]                 = intrinsics->p1();
  ceres_intrinsics[OFFSET_P2]                 = intrinsics->p2();

  RotationMatrixPlus rotation_matrix_plus;
  AutodiffParameterization<RotationMatrixPlus, 9, 3>
      rotation_parameterization(rotation_matrix_plus);

  int num_residuals = 0;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    EuclideanCamera *camera = reconstruction->CameraForImage(marker.image);
    EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
    if (!camera || !point) {
      continue;
    }

    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<
        OpenCVReprojectionError, 2, 8, 9 /* 3 */, 3, 3>(
            new OpenCVReprojectionError(
                marker.x,
                marker.y)),
        NULL,
        ceres_intrinsics,
        &camera->R(0, 0),
        &camera->t(0),
        &point->X(0));

    // It's fine if the parameterization for one camera is set repeatedly.
    problem.SetParameterization(&camera->R(0, 0),
                                &rotation_parameterization);

    num_residuals++;
  }
  LG << "Number of residuals: " << num_residuals;

  if(!num_residuals) {
    LG << "Skipping running minimizer with zero residuals";
    return;
  }

  BundleIntrinsicsLogMessage(bundle_intrinsics);

  scoped_ptr<ceres::SubsetParameterization>
      subset_parameterization(NULL);

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

    subset_parameterization.reset(
        new ceres::SubsetParameterization(8, constant_intrinsics));

    problem.SetParameterization(ceres_intrinsics, subset_parameterization.get());
  }

  ceres::Solver::Options options;
  options.use_nonmonotonic_steps = true;
  options.preconditioner_type = ceres::SCHUR_JACOBI;
  options.linear_solver_type = ceres::ITERATIVE_SCHUR;
  options.use_inner_iterations = true;
  options.max_num_iterations = 100;

  ceres::Solver::Summary summary;
  ceres::Solve(options, &problem, &summary);

  LG << "Final report:\n" << summary.FullReport();

  // Copy intrinsics back.
  if (bundle_intrinsics != BUNDLE_NO_INTRINSICS) {
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

  LG << "Final intrinsics: " << *intrinsics;
}

void ProjectiveBundle(const Tracks & /*tracks*/,
                      ProjectiveReconstruction * /*reconstruction*/) {
  // TODO(keir): Implement this! This can't work until we have a better bundler
  // than SSBA, since SSBA has no support for projective bundling.
}

}  // namespace libmv
