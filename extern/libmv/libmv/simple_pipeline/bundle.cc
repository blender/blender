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

#define V3DLIB_ENABLE_SUITESPARSE 1

#include <map>

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"
#include "third_party/ssba/Geometry/v3d_cameramatrix.h"
#include "third_party/ssba/Geometry/v3d_metricbundle.h"
#include "third_party/ssba/Math/v3d_linear.h"
#include "third_party/ssba/Math/v3d_linear_utils.h"

namespace libmv {

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

  // "index" in this context is the index that V3D's optimizer will see. The
  // V3D index must be dense in that the cameras are numbered 0...n-1, which is
  // not the case for the "image" numbering that arises from the tracks
  // structure. The complicated mapping is necessary to convert between the two
  // representations.
  std::map<EuclideanCamera *, int> camera_to_index;
  std::map<EuclideanPoint *, int> point_to_index;
  vector<EuclideanCamera *> index_to_camera;
  vector<EuclideanPoint *> index_to_point;
  int num_cameras = 0;
  int num_points = 0;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    EuclideanCamera *camera = reconstruction->CameraForImage(marker.image);
    EuclideanPoint *point = reconstruction->PointForTrack(marker.track);
    if (camera && point) {
      if (camera_to_index.find(camera) == camera_to_index.end()) {
        camera_to_index[camera] = num_cameras;
        index_to_camera.push_back(camera);
        num_cameras++;
      }
      if (point_to_index.find(point) == point_to_index.end()) {
        point_to_index[point] = num_points;
        index_to_point.push_back(point);
        num_points++;
      }
    }
  }

  // Convert libmv's K matrix to V3d's K matrix.
  V3D::Matrix3x3d v3d_K;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      v3d_K[i][j] = intrinsics->K()(i, j);
    }
  }

  // Convert libmv's distortion to v3d distortion.
  V3D::StdDistortionFunction v3d_distortion;
  v3d_distortion.k1 = intrinsics->k1();
  v3d_distortion.k2 = intrinsics->k2();
  v3d_distortion.p1 = intrinsics->p1();
  v3d_distortion.p2 = intrinsics->p2();

  // Convert libmv's cameras to V3D's cameras.
  std::vector<V3D::CameraMatrix> v3d_cameras(index_to_camera.size());
  for (int k = 0; k < index_to_camera.size(); ++k) {
    V3D::Matrix3x3d R;
    V3D::Vector3d t;

    // Libmv's rotation matrix type.
    const Mat3 &R_libmv = index_to_camera[k]->R;
    const Vec3 &t_libmv = index_to_camera[k]->t;

    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        R[i][j] = R_libmv(i, j);
      }
      t[i] = t_libmv(i);
    }
    v3d_cameras[k].setIntrinsic(v3d_K);
    v3d_cameras[k].setRotation(R);
    v3d_cameras[k].setTranslation(t);
  }
  LG << "Number of cameras: " << index_to_camera.size();

  // Convert libmv's points to V3D's points.
  std::vector<V3D::Vector3d> v3d_points(index_to_point.size());
  for (int i = 0; i < index_to_point.size(); i++) {
    v3d_points[i][0] = index_to_point[i]->X(0);
    v3d_points[i][1] = index_to_point[i]->X(1);
    v3d_points[i][2] = index_to_point[i]->X(2);
  }
  LG << "Number of points: " << index_to_point.size();

  // Convert libmv's measurements to v3d measurements.
  int num_residuals = 0;
  std::vector<V3D::Vector2d> v3d_measurements;
  std::vector<int> v3d_camera_for_measurement;
  std::vector<int> v3d_point_for_measurement;
  for (int i = 0; i < markers.size(); ++i) {
    EuclideanCamera *camera = reconstruction->CameraForImage(markers[i].image);
    EuclideanPoint *point = reconstruction->PointForTrack(markers[i].track);
    if (!camera || !point) {
      continue;
    }
    V3D::Vector2d v3d_point;
    v3d_point[0] = markers[i].x;
    v3d_point[1] = markers[i].y;
    v3d_measurements.push_back(v3d_point);
    v3d_camera_for_measurement.push_back(camera_to_index[camera]);
    v3d_point_for_measurement.push_back(point_to_index[point]);
    num_residuals++;
  }
  LG << "Number of residuals: " << num_residuals;
  
  // Convert from libmv's specification for which intrinsics to bundle to V3D's.
  int v3d_bundle_intrinsics;
  if (bundle_intrinsics == BUNDLE_NO_INTRINSICS) {
    LG << "Bundling only camera positions.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_METRIC;
  } else if (bundle_intrinsics == BUNDLE_FOCAL_LENGTH) {
    LG << "Bundling f.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_FOCAL_LENGTH;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT)) {
    LG << "Bundling f, px, py.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_FOCAL_LENGTH_PP;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT |
                                   BUNDLE_RADIAL)) {
    LG << "Bundling f, px, py, k1, k2.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_RADIAL;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_PRINCIPAL_POINT |
                                   BUNDLE_RADIAL |
                                   BUNDLE_TANGENTIAL)) {
    LG << "Bundling f, px, py, k1, k2, p1, p2.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_RADIAL_TANGENTIAL;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL |
                                   BUNDLE_TANGENTIAL)) {
    LG << "Bundling f, px, py, k1, k2, p1, p2.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_RADIAL_TANGENTIAL;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL)) {
    LG << "Bundling f, k1, k2.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_FOCAL_AND_RADIAL;
  } else if (bundle_intrinsics == (BUNDLE_FOCAL_LENGTH |
                                   BUNDLE_RADIAL_K1)) {
    LG << "Bundling f, k1.";
    v3d_bundle_intrinsics = V3D::FULL_BUNDLE_FOCAL_AND_RADIAL_K1;
  } else {
    LOG(FATAL) << "Unsupported bundle combination.";
  }

  // Ignore any outliers; assume supervised tracking.
  double v3d_inlier_threshold = 500000.0;

  // Finally, run the bundle adjustment.
  V3D::CommonInternalsMetricBundleOptimizer opt(v3d_bundle_intrinsics,
                                                v3d_inlier_threshold,
                                                v3d_K,
                                                v3d_distortion,
                                                v3d_cameras,
                                                v3d_points,
                                                v3d_measurements,
                                                v3d_camera_for_measurement,
                                                v3d_point_for_measurement);
  opt.maxIterations = 500;
  opt.minimize();
  if (opt.status == V3D::LEVENBERG_OPTIMIZER_TIMEOUT) {
    LG << "Bundle status: Timed out.";
  } else if (opt.status == V3D::LEVENBERG_OPTIMIZER_SMALL_UPDATE) {
    LG << "Bundle status: Small update.";
  } else if (opt.status == V3D::LEVENBERG_OPTIMIZER_CONVERGED) {
    LG << "Bundle status: Converged.";
  }

  // Convert V3D's K matrix back to libmv's K matrix.
  Mat3 K;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      K(i, j) = v3d_K[i][j];
    }
  }
  intrinsics->SetK(K);

  // Convert V3D's distortion back to libmv's distortion.
  intrinsics->SetRadialDistortion(v3d_distortion.k1, v3d_distortion.k2, 0.0);
  intrinsics->SetTangentialDistortion(v3d_distortion.p1, v3d_distortion.p2);

  // Convert V3D's cameras back to libmv's cameras.
  for (int k = 0; k < num_cameras; k++) {
    V3D::Matrix3x4d const Rt = v3d_cameras[k].getOrientation();
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        index_to_camera[k]->R(i, j) = Rt[i][j];
      }
      index_to_camera[k]->t(i) = Rt[i][3];
    }
  }

  // Convert V3D's points back to libmv's points.
  for (int k = 0; k < num_points; k++) {
    for (int i = 0; i < 3; ++i) {
      index_to_point[k]->X(i) = v3d_points[k][i];
    }
  }
  LG << "Final intrinsics: " << *intrinsics;
}

void ProjectiveBundle(const Tracks & /*tracks*/,
                      ProjectiveReconstruction * /*reconstruction*/) {
  // TODO(keir): Implement this! This can't work until we have a better bundler
  // than SSBA, since SSBA has no support for projective bundling.
}

}  // namespace libmv
