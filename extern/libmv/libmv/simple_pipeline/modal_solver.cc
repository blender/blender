// Copyright (c) 2012 libmv authors.
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

#include <cstdio>

#include "ceres/ceres.h"
#include "ceres/rotation.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/panography.h"

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

namespace libmv {

namespace {
void ProjectMarkerOnSphere(const Marker &marker, Vec3 &X) {
  X(0) = marker.x;
  X(1) = marker.y;
  X(2) = 1.0;

  X *= 5.0 / X.norm();
}

void ModalSolverLogProress(ProgressUpdateCallback *update_callback,
                           double progress) {
  if (update_callback) {
    char message[256];

    snprintf(message, sizeof(message), "Solving progress %d%%",
             (int)(progress * 100));

    update_callback->invoke(progress, message);
  }
}

struct ModalReprojectionError {
  ModalReprojectionError(double observed_x,
                         double observed_y,
                         const double weight,
                         const Vec3 &bundle)
    : observed_x_(observed_x), observed_y_(observed_y),
      weight_(weight), bundle_(bundle) { }

  template <typename T>
  bool operator()(const T* quaternion,   // Rotation quaternion
                  T* residuals) const {
    T R[9];
    ceres::QuaternionToRotation(quaternion, R);

    // Convert bundle position from double to T.
    T X[3];
    X[0] = T(bundle_(0));
    X[1] = T(bundle_(1));
    X[2] = T(bundle_(2));

    // Compute projective coordinates: x = RX.
    T x[3];
    x[0] = R[0]*X[0] + R[3]*X[1] + R[6]*X[2];
    x[1] = R[1]*X[0] + R[4]*X[1] + R[7]*X[2];
    x[2] = R[2]*X[0] + R[5]*X[1] + R[8]*X[2];

    // Compute normalized coordinates: x /= x[2].
    T xn = x[0] / x[2];
    T yn = x[1] / x[2];

    // The error is the difference between reprojected
    // and observed marker position.
    residuals[0] = xn - T(observed_x_);
    residuals[1] = yn - T(observed_y_);

    return true;
  }

  double observed_x_;
  double observed_y_;
  double weight_;
  Vec3 bundle_;
};
}  // namespace

void ModalSolver(const Tracks &tracks,
                 EuclideanReconstruction *reconstruction,
                 ProgressUpdateCallback *update_callback) {
  int max_image = tracks.MaxImage();
  int max_track = tracks.MaxTrack();

  LG << "Max image: " << max_image;
  LG << "Max track: " << max_track;

  // For minimization we're using quaternions.
  Vec3 zero_rotation = Vec3::Zero();
  Vec4 quaternion;
  ceres::AngleAxisToQuaternion(&zero_rotation(0), &quaternion(0));

  for (int image = 0; image <= max_image; ++image) {
    vector<Marker> all_markers = tracks.MarkersInImage(image);

    ModalSolverLogProress(update_callback, (float) image / max_image);

    // Skip empty images without doing anything.
    if (all_markers.size() == 0) {
      LG << "Skipping image: " << image;
      continue;
    }

    // STEP 1: Estimate rotation analytically.
    Mat3 current_R;
    ceres::QuaternionToRotation(&quaternion(0), &current_R(0, 0));

    // Construct point cloud for current and previous images,
    // using markers appear at current image for which we know
    // 3D positions.
    Mat x1, x2;
    for (int i = 0; i < all_markers.size(); ++i) {
      Marker &marker = all_markers[i];
      EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
      if (point) {
        Vec3 X;
        ProjectMarkerOnSphere(marker, X);

        int last_column = x1.cols();
        x1.conservativeResize(3, last_column + 1);
        x2.conservativeResize(3, last_column + 1);

        x1.col(last_column) = current_R * point->X;
        x2.col(last_column) = X;
      }
    }

    if (x1.cols() >= 2) {
      Mat3 delta_R;

      // Compute delta rotation matrix for two point clouds.
      // Could be a bit confusing at first glance, but order
      // of clouds is indeed so.
      GetR_FixedCameraCenter(x2, x1, 1.0, &delta_R);

      // Convert delta rotation form matrix to final image
      // rotation stored in a quaternion
      Vec3 delta_angle_axis;
      ceres::RotationMatrixToAngleAxis(&delta_R(0, 0), &delta_angle_axis(0));

      Vec3 current_angle_axis;
      ceres::QuaternionToAngleAxis(&quaternion(0), &current_angle_axis(0));

      Vec3 angle_axis = current_angle_axis + delta_angle_axis;

      ceres::AngleAxisToQuaternion(&angle_axis(0), &quaternion(0));

      LG << "Analytically computed quaternion "
         << quaternion.transpose();
    }

    // STEP 2: Refine rotation with Ceres.
    ceres::Problem problem;

    ceres::LocalParameterization* quaternion_parameterization =
        new ceres::QuaternionParameterization;

    int num_residuals = 0;
    for (int i = 0; i < all_markers.size(); ++i) {
      Marker &marker = all_markers[i];
      EuclideanPoint *point = reconstruction->PointForTrack(marker.track);

      if (point && marker.weight != 0.0) {
        problem.AddResidualBlock(new ceres::AutoDiffCostFunction<
            ModalReprojectionError,
            2, /* num_residuals */
            4>(new ModalReprojectionError(marker.x,
                                          marker.y,
                                          marker.weight,
                                          point->X)),
            NULL,
            &quaternion(0));
        num_residuals++;

        problem.SetParameterization(&quaternion(0),
                                    quaternion_parameterization);
      }
    }

    LG << "Number of residuals: " << num_residuals;

    if (num_residuals) {
      // Configure the solve.
      ceres::Solver::Options solver_options;
      solver_options.linear_solver_type = ceres::DENSE_QR;
      solver_options.max_num_iterations = 50;
      solver_options.update_state_every_iteration = true;
      solver_options.gradient_tolerance = 1e-36;
      solver_options.parameter_tolerance = 1e-36;
      solver_options.function_tolerance = 1e-36;

      // Run the solve.
      ceres::Solver::Summary summary;
      ceres::Solve(solver_options, &problem, &summary);

      LG << "Summary:\n" << summary.FullReport();
      LG << "Refined quaternion " << quaternion.transpose();
    }

    // Convert quaternion to rotation matrix.
    Mat3 R;
    ceres::QuaternionToRotation(&quaternion(0), &R(0, 0));
    reconstruction->InsertCamera(image, R, Vec3::Zero());

    // STEP 3: reproject all new markers appeared at image

    // Check if there're new markers appeared on current image
    // and reproject them on sphere to obtain 3D position/
    for (int track = 0; track <= max_track; ++track) {
      if (!reconstruction->PointForTrack(track)) {
        Marker marker = tracks.MarkerInImageForTrack(image, track);

        if (marker.image == image) {
          // New track appeared on this image,
          // project it's position onto sphere.

          LG << "Projecting track " << track << " at image " << image;

          Vec3 X;
          ProjectMarkerOnSphere(marker, X);
          reconstruction->InsertPoint(track, R.inverse() * X);
        }
      }
    }
  }
}

}  // namespace libmv
