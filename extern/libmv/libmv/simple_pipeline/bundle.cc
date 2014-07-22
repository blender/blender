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
#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/distortion_models.h"

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace libmv {

// The intrinsics need to get combined into a single parameter block; use these
// enums to index instead of numeric constants.
enum {
  // Camera calibration values.
  OFFSET_FOCAL_LENGTH,
  OFFSET_PRINCIPAL_POINT_X,
  OFFSET_PRINCIPAL_POINT_Y,

  // Distortion model coefficients.
  OFFSET_K1,
  OFFSET_K2,
  OFFSET_K3,
  OFFSET_P1,
  OFFSET_P2,

  // Maximal possible offset.
  OFFSET_MAX,
};

#define FIRST_DISTORTION_COEFFICIENT OFFSET_K1
#define LAST_DISTORTION_COEFFICIENT OFFSET_P2
#define NUM_DISTORTION_COEFFICIENTS  \
  (LAST_DISTORTION_COEFFICIENT - FIRST_DISTORTION_COEFFICIENT + 1)

namespace {

// Cost functor which computes reprojection error of 3D point X
// on camera defined by angle-axis rotation and it's translation
// (which are in the same block due to optimization reasons).
//
// This functor uses a radial distortion model.
struct OpenCVReprojectionError {
  OpenCVReprojectionError(const DistortionModelType distortion_model,
                          const double observed_x,
                          const double observed_y,
                          const double weight)
      : distortion_model_(distortion_model),
        observed_x_(observed_x), observed_y_(observed_y),
        weight_(weight) {}

  template <typename T>
  bool operator()(const T* const intrinsics,
                  const T* const R_t,  // Rotation denoted by angle axis
                                       // followed with translation
                  const T* const X,    // Point coordinates 3x1.
                  T* residuals) const {
    // Unpack the intrinsics.
    const T& focal_length      = intrinsics[OFFSET_FOCAL_LENGTH];
    const T& principal_point_x = intrinsics[OFFSET_PRINCIPAL_POINT_X];
    const T& principal_point_y = intrinsics[OFFSET_PRINCIPAL_POINT_Y];

    // Compute projective coordinates: x = RX + t.
    T x[3];

    ceres::AngleAxisRotatePoint(R_t, X, x);
    x[0] += R_t[3];
    x[1] += R_t[4];
    x[2] += R_t[5];

    // Prevent points from going behind the camera.
    if (x[2] < T(0)) {
      return false;
    }

    // Compute normalized coordinates: x /= x[2].
    T xn = x[0] / x[2];
    T yn = x[1] / x[2];

    T predicted_x, predicted_y;

    // Apply distortion to the normalized points to get (xd, yd).
    // TODO(keir): Do early bailouts for zero distortion; these are expensive
    // jet operations.
    switch (distortion_model_) {
      case DISTORTION_MODEL_POLYNOMIAL:
        {
          const T& k1 = intrinsics[OFFSET_K1];
          const T& k2 = intrinsics[OFFSET_K2];
          const T& k3 = intrinsics[OFFSET_K3];
          const T& p1 = intrinsics[OFFSET_P1];
          const T& p2 = intrinsics[OFFSET_P2];

          ApplyPolynomialDistortionModel(focal_length,
                                         focal_length,
                                         principal_point_x,
                                         principal_point_y,
                                         k1, k2, k3,
                                         p1, p2,
                                         xn, yn,
                                         &predicted_x,
                                         &predicted_y);
          break;
        }
      case DISTORTION_MODEL_DIVISION:
        {
          const T& k1 = intrinsics[OFFSET_K1];
          const T& k2 = intrinsics[OFFSET_K2];

          ApplyDivisionDistortionModel(focal_length,
                                       focal_length,
                                       principal_point_x,
                                       principal_point_y,
                                       k1, k2,
                                       xn, yn,
                                       &predicted_x,
                                       &predicted_y);
          break;
        }
      default:
        LOG(FATAL) << "Unknown distortion model";
    }

    // The error is the difference between the predicted and observed position.
    residuals[0] = (predicted_x - T(observed_x_)) * weight_;
    residuals[1] = (predicted_y - T(observed_y_)) * weight_;
    return true;
  }

  const DistortionModelType distortion_model_;
  const double observed_x_;
  const double observed_y_;
  const double weight_;
};

// Print a message to the log which camera intrinsics are gonna to be optimixed.
void BundleIntrinsicsLogMessage(const int bundle_intrinsics) {
  if (bundle_intrinsics == BUNDLE_NO_INTRINSICS) {
    LOG(INFO) << "Bundling only camera positions.";
  } else {
    std::string bundling_message = "";

#define APPEND_BUNDLING_INTRINSICS(name, flag) \
    if (bundle_intrinsics & flag) { \
      if (!bundling_message.empty()) { \
        bundling_message += ", "; \
      } \
      bundling_message += name; \
    } (void)0

    APPEND_BUNDLING_INTRINSICS("f",      BUNDLE_FOCAL_LENGTH);
    APPEND_BUNDLING_INTRINSICS("px, py", BUNDLE_PRINCIPAL_POINT);
    APPEND_BUNDLING_INTRINSICS("k1",     BUNDLE_RADIAL_K1);
    APPEND_BUNDLING_INTRINSICS("k2",     BUNDLE_RADIAL_K2);
    APPEND_BUNDLING_INTRINSICS("p1",     BUNDLE_TANGENTIAL_P1);
    APPEND_BUNDLING_INTRINSICS("p2",     BUNDLE_TANGENTIAL_P2);

    LOG(INFO) << "Bundling " << bundling_message << ".";
  }
}

// Pack intrinsics from object to an array for easier
// and faster minimization.
void PackIntrinisicsIntoArray(const CameraIntrinsics &intrinsics,
                              double ceres_intrinsics[OFFSET_MAX]) {
  ceres_intrinsics[OFFSET_FOCAL_LENGTH]       = intrinsics.focal_length();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_X]  = intrinsics.principal_point_x();
  ceres_intrinsics[OFFSET_PRINCIPAL_POINT_Y]  = intrinsics.principal_point_y();

  int num_distortion_parameters = intrinsics.num_distortion_parameters();
  assert(num_distortion_parameters <= NUM_DISTORTION_COEFFICIENTS);

  const double *distortion_parameters = intrinsics.distortion_parameters();
  for (int i = 0; i < num_distortion_parameters; ++i) {
    ceres_intrinsics[FIRST_DISTORTION_COEFFICIENT + i] =
        distortion_parameters[i];
  }
}

// Unpack intrinsics back from an array to an object.
void UnpackIntrinsicsFromArray(const double ceres_intrinsics[OFFSET_MAX],
                               CameraIntrinsics *intrinsics) {
  intrinsics->SetFocalLength(ceres_intrinsics[OFFSET_FOCAL_LENGTH],
                             ceres_intrinsics[OFFSET_FOCAL_LENGTH]);

  intrinsics->SetPrincipalPoint(ceres_intrinsics[OFFSET_PRINCIPAL_POINT_X],
                                ceres_intrinsics[OFFSET_PRINCIPAL_POINT_Y]);

  int num_distortion_parameters = intrinsics->num_distortion_parameters();
  assert(num_distortion_parameters <= NUM_DISTORTION_COEFFICIENTS);

  double *distortion_parameters = intrinsics->distortion_parameters();
  for (int i = 0; i < num_distortion_parameters; ++i) {
    distortion_parameters[i] =
        ceres_intrinsics[FIRST_DISTORTION_COEFFICIENT + i];
  }
}

// Get a vector of camera's rotations denoted by angle axis
// conjuncted with translations into single block
//
// Element with index i matches to a rotation+translation for
// camera at image i.
vector<Vec6> PackCamerasRotationAndTranslation(
    const Tracks &tracks,
    const EuclideanReconstruction &reconstruction) {
  vector<Vec6> all_cameras_R_t;
  int max_image = tracks.MaxImage();

  all_cameras_R_t.resize(max_image + 1);

  for (int i = 0; i <= max_image; i++) {
    const EuclideanCamera *camera = reconstruction.CameraForImage(i);

    if (!camera) {
      continue;
    }

    ceres::RotationMatrixToAngleAxis(&camera->R(0, 0),
                                     &all_cameras_R_t[i](0));
    all_cameras_R_t[i].tail<3>() = camera->t;
  }
  return all_cameras_R_t;
}

// Convert cameras rotations fro mangle axis back to rotation matrix.
void UnpackCamerasRotationAndTranslation(
    const Tracks &tracks,
    const vector<Vec6> &all_cameras_R_t,
    EuclideanReconstruction *reconstruction) {
  int max_image = tracks.MaxImage();

  for (int i = 0; i <= max_image; i++) {
    EuclideanCamera *camera = reconstruction->CameraForImage(i);

    if (!camera) {
      continue;
    }

    ceres::AngleAxisToRotationMatrix(&all_cameras_R_t[i](0),
                                     &camera->R(0, 0));
    camera->t = all_cameras_R_t[i].tail<3>();
  }
}

// Converts sparse CRSMatrix to Eigen matrix, so it could be used
// all over in the pipeline.
//
// TODO(sergey): currently uses dense Eigen matrices, best would
//               be to use sparse Eigen matrices
void CRSMatrixToEigenMatrix(const ceres::CRSMatrix &crs_matrix,
                            Mat *eigen_matrix) {
  eigen_matrix->resize(crs_matrix.num_rows, crs_matrix.num_cols);
  eigen_matrix->setZero();

  for (int row = 0; row < crs_matrix.num_rows; ++row) {
    int start = crs_matrix.rows[row];
    int end = crs_matrix.rows[row + 1] - 1;

    for (int i = start; i <= end; i++) {
      int col = crs_matrix.cols[i];
      double value = crs_matrix.values[i];

      (*eigen_matrix)(row, col) = value;
    }
  }
}

void EuclideanBundlerPerformEvaluation(const Tracks &tracks,
                                       EuclideanReconstruction *reconstruction,
                                       vector<Vec6> *all_cameras_R_t,
                                       ceres::Problem *problem,
                                       BundleEvaluation *evaluation) {
    int max_track = tracks.MaxTrack();
    // Number of camera rotations equals to number of translation,
    int num_cameras = all_cameras_R_t->size();
    int num_points = 0;

    vector<EuclideanPoint*> minimized_points;
    for (int i = 0; i <= max_track; i++) {
      EuclideanPoint *point = reconstruction->PointForTrack(i);
      if (point) {
        // We need to know whether the track is constant zero weight,
        // and it so it wouldn't have parameter block in the problem.
        //
        // Getting all markers for track is not so bac currently since
        // this code is only used by keyframe selection when there are
        // not so much tracks and only 2 frames anyway.
        vector<Marker> markera_of_track = tracks.MarkersForTrack(i);
        for (int j = 0; j < markera_of_track.size(); j++) {
          if (markera_of_track.at(j).weight != 0.0) {
            minimized_points.push_back(point);
            num_points++;
            break;
          }
        }
      }
    }

    LG << "Number of cameras " << num_cameras;
    LG << "Number of points " << num_points;

    evaluation->num_cameras = num_cameras;
    evaluation->num_points = num_points;

    if (evaluation->evaluate_jacobian) {      // Evaluate jacobian matrix.
      ceres::CRSMatrix evaluated_jacobian;
      ceres::Problem::EvaluateOptions eval_options;

      // Cameras goes first in the ordering.
      int max_image = tracks.MaxImage();
      for (int i = 0; i <= max_image; i++) {
        const EuclideanCamera *camera = reconstruction->CameraForImage(i);
        if (camera) {
          double *current_camera_R_t = &(*all_cameras_R_t)[i](0);

          // All cameras are variable now.
          problem->SetParameterBlockVariable(current_camera_R_t);

          eval_options.parameter_blocks.push_back(current_camera_R_t);
        }
      }

      // Points goes at the end of ordering,
      for (int i = 0; i < minimized_points.size(); i++) {
        EuclideanPoint *point = minimized_points.at(i);
        eval_options.parameter_blocks.push_back(&point->X(0));
      }

      problem->Evaluate(eval_options,
                        NULL, NULL, NULL,
                        &evaluated_jacobian);

      CRSMatrixToEigenMatrix(evaluated_jacobian, &evaluation->jacobian);
    }
}

// This is an utility function to only bundle 3D position of
// given markers list.
//
// Main purpose of this function is to adjust positions of tracks
// which does have constant zero weight and so far only were using
// algebraic intersection to obtain their 3D positions.
//
// At this point we only need to bundle points positions, cameras
// are to be totally still here.
void EuclideanBundlePointsOnly(const DistortionModelType distortion_model,
                               const vector<Marker> &markers,
                               vector<Vec6> &all_cameras_R_t,
                               double ceres_intrinsics[OFFSET_MAX],
                               EuclideanReconstruction *reconstruction) {
  ceres::Problem::Options problem_options;
  ceres::Problem problem(problem_options);
  int num_residuals = 0;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    EuclideanCamera *camera = reconstruction->CameraForImage(marker.image);
    EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
    if (camera == NULL || point == NULL) {
      continue;
    }

    // Rotation of camera denoted in angle axis followed with
    // camera translaiton.
    double *current_camera_R_t = &all_cameras_R_t[camera->image](0);

    problem.AddResidualBlock(new ceres::AutoDiffCostFunction<
        OpenCVReprojectionError, 2, OFFSET_MAX, 6, 3>(
            new OpenCVReprojectionError(
                distortion_model,
                marker.x,
                marker.y,
                1.0)),
        NULL,
        ceres_intrinsics,
        current_camera_R_t,
        &point->X(0));

    problem.SetParameterBlockConstant(current_camera_R_t);
    num_residuals++;
  }

  LG << "Number of residuals: " << num_residuals;
  if (!num_residuals) {
    LG << "Skipping running minimizer with zero residuals";
    return;
  }

  problem.SetParameterBlockConstant(ceres_intrinsics);

  // Configure the solver.
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

}

}  // namespace

void EuclideanBundle(const Tracks &tracks,
                     EuclideanReconstruction *reconstruction) {
  PolynomialCameraIntrinsics empty_intrinsics;
  EuclideanBundleCommonIntrinsics(tracks,
                                  BUNDLE_NO_INTRINSICS,
                                  BUNDLE_NO_CONSTRAINTS,
                                  reconstruction,
                                  &empty_intrinsics,
                                  NULL);
}

void EuclideanBundleCommonIntrinsics(
    const Tracks &tracks,
    const int bundle_intrinsics,
    const int bundle_constraints,
    EuclideanReconstruction *reconstruction,
    CameraIntrinsics *intrinsics,
    BundleEvaluation *evaluation) {
  LG << "Original intrinsics: " << *intrinsics;
  vector<Marker> markers = tracks.AllMarkers();

  // N-th element denotes whether track N is a constant zero-weigthed track.
  vector<bool> zero_weight_tracks_flags(tracks.MaxTrack() + 1, true);

  // Residual blocks with 10 parameters are unwieldly with Ceres, so pack the
  // intrinsics into a single block and rely on local parameterizations to
  // control which intrinsics are allowed to vary.
  double ceres_intrinsics[OFFSET_MAX];
  PackIntrinisicsIntoArray(*intrinsics, ceres_intrinsics);

  // Convert cameras rotations to angle axis and merge with translation
  // into single parameter block for maximal minimization speed.
  //
  // Block for minimization has got the following structure:
  //   <3 elements for angle-axis> <3 elements for translation>
  vector<Vec6> all_cameras_R_t =
    PackCamerasRotationAndTranslation(tracks, *reconstruction);

  // Parameterization used to restrict camera motion for modal solvers.
  ceres::SubsetParameterization *constant_translation_parameterization = NULL;
  if (bundle_constraints & BUNDLE_NO_TRANSLATION) {
      std::vector<int> constant_translation;

      // First three elements are rotation, ast three are translation.
      constant_translation.push_back(3);
      constant_translation.push_back(4);
      constant_translation.push_back(5);

      constant_translation_parameterization =
        new ceres::SubsetParameterization(6, constant_translation);
  }

  // Add residual blocks to the problem.
  ceres::Problem::Options problem_options;
  ceres::Problem problem(problem_options);
  int num_residuals = 0;
  bool have_locked_camera = false;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    EuclideanCamera *camera = reconstruction->CameraForImage(marker.image);
    EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
    if (camera == NULL || point == NULL) {
      continue;
    }

    // Rotation of camera denoted in angle axis followed with
    // camera translaiton.
    double *current_camera_R_t = &all_cameras_R_t[camera->image](0);

    // Skip residual block for markers which does have absolutely
    // no affect on the final solution.
    // This way ceres is not gonna to go crazy.
    if (marker.weight != 0.0) {
      problem.AddResidualBlock(new ceres::AutoDiffCostFunction<
          OpenCVReprojectionError, 2, OFFSET_MAX, 6, 3>(
              new OpenCVReprojectionError(
                  intrinsics->GetDistortionModelType(),
                  marker.x,
                  marker.y,
                  marker.weight)),
          NULL,
          ceres_intrinsics,
          current_camera_R_t,
          &point->X(0));

      // We lock the first camera to better deal with scene orientation ambiguity.
      if (!have_locked_camera) {
        problem.SetParameterBlockConstant(current_camera_R_t);
        have_locked_camera = true;
      }

      if (bundle_constraints & BUNDLE_NO_TRANSLATION) {
        problem.SetParameterization(current_camera_R_t,
                                    constant_translation_parameterization);
      }

      zero_weight_tracks_flags[marker.track] = false;
      num_residuals++;
    }
  }
  LG << "Number of residuals: " << num_residuals;

  if (!num_residuals) {
    LG << "Skipping running minimizer with zero residuals";
    return;
  }

  if (intrinsics->GetDistortionModelType() == DISTORTION_MODEL_DIVISION &&
    (bundle_intrinsics & BUNDLE_TANGENTIAL) != 0) {
    LOG(FATAL) << "Division model doesn't support bundling "
                  "of tangential distortion";
  }

  BundleIntrinsicsLogMessage(bundle_intrinsics);

  if (bundle_intrinsics == BUNDLE_NO_INTRINSICS) {
    // No camera intrinsics are being refined,
    // set the whole parameter block as constant for best performance.
    problem.SetParameterBlockConstant(ceres_intrinsics);
  } else {
    // Set the camera intrinsics that are not to be bundled as
    // constant using some macro trickery.

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
      new ceres::SubsetParameterization(OFFSET_MAX, constant_intrinsics);

    problem.SetParameterization(ceres_intrinsics, subset_parameterization);
  }

  // Configure the solver.
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
                                      all_cameras_R_t,
                                      reconstruction);

  // Copy intrinsics back.
  if (bundle_intrinsics != BUNDLE_NO_INTRINSICS)
    UnpackIntrinsicsFromArray(ceres_intrinsics, intrinsics);

  LG << "Final intrinsics: " << *intrinsics;

  if (evaluation) {
    EuclideanBundlerPerformEvaluation(tracks, reconstruction, &all_cameras_R_t,
                                      &problem, evaluation);
  }

  // Separate step to adjust positions of tracks which are
  // constant zero-weighted.
  vector<Marker> zero_weight_markers;
  for (int track = 0; track < tracks.MaxTrack(); ++track) {
    if (zero_weight_tracks_flags[track]) {
      vector<Marker> current_markers = tracks.MarkersForTrack(track);
      zero_weight_markers.reserve(zero_weight_markers.size() +
                                  current_markers.size());
      for (int i = 0; i < current_markers.size(); ++i) {
        zero_weight_markers.push_back(current_markers[i]);
      }
    }
  }

  if (zero_weight_markers.size()) {
    LG << "Refining position of constant zero-weighted tracks";
    EuclideanBundlePointsOnly(intrinsics->GetDistortionModelType(),
                              zero_weight_markers,
                              all_cameras_R_t,
                              ceres_intrinsics,
                              reconstruction);
  }
}

void ProjectiveBundle(const Tracks & /*tracks*/,
                      ProjectiveReconstruction * /*reconstruction*/) {
  // TODO(keir): Implement this! This can't work until we have a better bundler
  // than SSBA, since SSBA has no support for projective bundling.
}

}  // namespace libmv
