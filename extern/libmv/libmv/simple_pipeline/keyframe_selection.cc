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

#include "libmv/simple_pipeline/keyframe_selection.h"

#include "libmv/numeric/numeric.h"
#include "ceres/ceres.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/homography.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/simple_pipeline/intersect.h"
#include "libmv/simple_pipeline/bundle.h"

#include <Eigen/Eigenvalues>

namespace libmv {
namespace {

Vec2 NorrmalizedToPixelSpace(const Vec2 &vec,
                             const CameraIntrinsics &intrinsics) {
  Vec2 result;

  double focal_length_x = intrinsics.focal_length_x();
  double focal_length_y = intrinsics.focal_length_y();

  double principal_point_x = intrinsics.principal_point_x();
  double principal_point_y = intrinsics.principal_point_y();

  result(0) = vec(0) * focal_length_x + principal_point_x;
  result(1) = vec(1) * focal_length_y + principal_point_y;

  return result;
}

Mat3 IntrinsicsNormalizationMatrix(const CameraIntrinsics &intrinsics) {
  Mat3 T = Mat3::Identity(), S = Mat3::Identity();

  T(0, 2) = -intrinsics.principal_point_x();
  T(1, 2) = -intrinsics.principal_point_y();

  S(0, 0) /= intrinsics.focal_length_x();
  S(1, 1) /= intrinsics.focal_length_y();

  return S * T;
}

// P.H.S. Torr
// Geometric Motion Segmentation and Model Selection
//
// http://reference.kfupm.edu.sa/content/g/e/geometric_motion_segmentation_and_model__126445.pdf
//
// d is the number of dimensions modeled
//     (d = 3 for a fundamental matrix or 2 for a homography)
// k is the number of degrees of freedom in the model
//     (k = 7 for a fundamental matrix or 8 for a homography)
// r is the dimension of the data
//     (r = 4 for 2D correspondences between two frames)
double GRIC(const Vec &e, int d, int k, int r) {
  int n = e.rows();
  double lambda1 = log(static_cast<double>(r));
  double lambda2 = log(static_cast<double>(r * n));

  // lambda3 limits the residual error, and this paper
  // http://elvera.nue.tu-berlin.de/files/0990Knorr2006.pdf
  // suggests using lambda3 of 2
  // same value is used in Torr's Problem of degeneracy in structure
  // and motion recovery from uncalibrated image sequences
  // http://www.robots.ox.ac.uk/~vgg/publications/papers/torr99.ps.gz
  double lambda3 = 2.0;

  // measurement error of tracker
  double sigma2 = 0.01;

  // Actual GRIC computation
  double gric_result = 0.0;

  for (int i = 0; i < n; i++) {
    double rho = std::min(e(i) * e(i) / sigma2, lambda3 * (r - d));
    gric_result += rho;
  }

  gric_result += lambda1 * d * n;
  gric_result += lambda2 * k;

  return gric_result;
}

// Compute a generalized inverse using eigen value decomposition.
// It'll actually also zero 7 last eigen values to deal with
// gauges, since this function is used to compute variance of
// reconstructed 3D points.
//
// TODO(sergey): Could be generalized by making it so number
//               of values to be zeroed is passed by an argument
//               and moved to numeric module.
Mat pseudoInverse(const Mat &matrix) {
  Eigen::EigenSolver<Mat> eigenSolver(matrix);
  Mat D = eigenSolver.pseudoEigenvalueMatrix();
  Mat V = eigenSolver.pseudoEigenvectors();

  double epsilon = std::numeric_limits<double>::epsilon();

  for (int i = 0; i < D.cols(); ++i) {
    if (D(i, i) > epsilon)
      D(i, i) = 1.0 / D(i, i);
    else
      D(i, i) = 0.0;
  }

  // Zero last 7 (which corresponds to smallest eigen values).
  // 7 equals to the number of gauge freedoms.
  for (int i = D.cols() - 7; i < D.cols(); ++i)
    D(i, i) = 0.0;

  return V * D * V.inverse();
}

void filterZeroWeightMarkersFromTracks(const Tracks &tracks,
                                       Tracks *filtered_tracks) {
  vector<Marker> all_markers = tracks.AllMarkers();

  for (int i = 0; i < all_markers.size(); ++i) {
    Marker &marker = all_markers[i];
    if (marker.weight != 0.0) {
      filtered_tracks->Insert(marker.image,
                              marker.track,
                              marker.x,
                              marker.y,
                              marker.weight);
    }
  }
}

}  // namespace

void SelectKeyframesBasedOnGRICAndVariance(const Tracks &_tracks,
                                           CameraIntrinsics &intrinsics,
                                           vector<int> &keyframes) {
  // Mirza Tahir Ahmed, Matthew N. Dailey
  // Robust key frame extraction for 3D reconstruction from video streams
  //
  // http://www.cs.ait.ac.th/~mdailey/papers/Tahir-KeyFrame.pdf

  Tracks filtered_tracks;
  filterZeroWeightMarkersFromTracks(_tracks, &filtered_tracks);

  int max_image = filtered_tracks.MaxImage();
  int next_keyframe = 1;
  int number_keyframes = 0;

  // Limit correspondence ratio from both sides.
  // On the one hand if number of correspondent features is too low,
  // triangulation will suffer.
  // On the other hand high correspondence likely means short baseline.
  // which also will affect om accuracy
  const double Tmin = 0.8;
  const double Tmax = 1.0;

  Mat3 N = IntrinsicsNormalizationMatrix(intrinsics);
  Mat3 N_inverse = N.inverse();

  double Sc_best = std::numeric_limits<double>::max();
  double success_intersects_factor_best = 0.0f;

  while (next_keyframe != -1) {
    int current_keyframe = next_keyframe;
    double Sc_best_candidate = std::numeric_limits<double>::max();

    LG << "Found keyframe " << next_keyframe;

    number_keyframes++;
    next_keyframe = -1;

    for (int candidate_image = current_keyframe + 1;
         candidate_image <= max_image;
         candidate_image++) {
      // Conjunction of all markers from both keyframes
      vector<Marker> all_markers =
        filtered_tracks.MarkersInBothImages(current_keyframe,
                                            candidate_image);

      // Match keypoints between frames current_keyframe and candidate_image
      vector<Marker> tracked_markers =
        filtered_tracks.MarkersForTracksInBothImages(current_keyframe,
                                                     candidate_image);

      // Correspondences in normalized space
      Mat x1, x2;
      CoordinatesForMarkersInImage(tracked_markers, current_keyframe, &x1);
      CoordinatesForMarkersInImage(tracked_markers, candidate_image, &x2);

      LG << "Found " << x1.cols()
         << " correspondences between " << current_keyframe
         << " and " << candidate_image;

      // Not enough points to construct fundamental matrix
      if (x1.cols() < 8 || x2.cols() < 8)
        continue;

      // STEP 1: Correspondence ratio constraint
      int Tc = tracked_markers.size();
      int Tf = all_markers.size();
      double Rc = static_cast<double>(Tc) / Tf;

      LG << "Correspondence between " << current_keyframe
         << " and " << candidate_image
         << ": " << Rc;

      if (Rc < Tmin || Rc > Tmax)
        continue;

      Mat3 H, F;

      // Estimate homography using default options.
      EstimateHomographyOptions estimate_homography_options;
      EstimateHomography2DFromCorrespondences(x1,
                                              x2,
                                              estimate_homography_options,
                                              &H);

      // Convert homography to original pixel space.
      H = N_inverse * H * N;

      EstimateFundamentalOptions estimate_fundamental_options;
      EstimateFundamentalFromCorrespondences(x1,
                                        x2,
                                        estimate_fundamental_options,
                                        &F);

      // Convert fundamental to original pixel space.
      F = N_inverse * F * N;

      // TODO(sergey): STEP 2: Discard outlier matches

      // STEP 3: Geometric Robust Information Criteria

      // Compute error values for homography and fundamental matrices
      Vec H_e, F_e;
      H_e.resize(x1.cols());
      F_e.resize(x1.cols());
      for (int i = 0; i < x1.cols(); i++) {
        Vec2 current_x1 =
          NorrmalizedToPixelSpace(Vec2(x1(0, i), x1(1, i)), intrinsics);
        Vec2 current_x2 =
          NorrmalizedToPixelSpace(Vec2(x2(0, i), x2(1, i)), intrinsics);

        H_e(i) = SymmetricGeometricDistance(H, current_x1, current_x2);
        F_e(i) = SymmetricEpipolarDistance(F, current_x1, current_x2);
      }

      LG << "H_e: " << H_e.transpose();
      LG << "F_e: " << F_e.transpose();

      // Degeneracy constraint
      double GRIC_H = GRIC(H_e, 2, 8, 4);
      double GRIC_F = GRIC(F_e, 3, 7, 4);

      LG << "GRIC values for frames " << current_keyframe
         << " and " << candidate_image
         << ", H-GRIC: " << GRIC_H
         << ", F-GRIC: " << GRIC_F;

      if (GRIC_H <= GRIC_F)
        continue;

      // TODO(sergey): STEP 4: PELC criterion

      // STEP 5: Estimation of reconstruction error
      //
      // Uses paper Keyframe Selection for Camera Motion and Structure
      // Estimation from Multiple Views
      // Uses ftp://ftp.tnt.uni-hannover.de/pub/papers/2004/ECCV2004-TTHBAW.pdf
      // Basically, equation (15)
      //
      // TODO(sergey): separate all the constraints into functions,
      //               this one is getting to much cluttered already

      // Definitions in equation (15):
      // - I is the number of 3D feature points
      // - A is the number of essential parameters of one camera

      EuclideanReconstruction reconstruction;

      // The F matrix should be an E matrix, but squash it just to be sure

      // Reconstruction should happen using normalized fundamental matrix
      Mat3 F_normal = N * F * N_inverse;

      Mat3 E;
      FundamentalToEssential(F_normal, &E);

      // Recover motion between the two images. Since this function assumes a
      // calibrated camera, use the identity for K
      Mat3 R;
      Vec3 t;
      Mat3 K = Mat3::Identity();

      if (!MotionFromEssentialAndCorrespondence(E,
                                                K, x1.col(0),
                                                K, x2.col(0),
                                                &R, &t)) {
        LG << "Failed to compute R and t from E and K";
        continue;
      }

      LG << "Camera transform between frames " << current_keyframe
         << " and " << candidate_image
         << ":\nR:\n" << R
         << "\nt:" << t.transpose();

      // First camera is identity, second one is relative to it
      reconstruction.InsertCamera(current_keyframe,
                                  Mat3::Identity(),
                                  Vec3::Zero());
      reconstruction.InsertCamera(candidate_image, R, t);

      // Reconstruct 3D points
      int intersects_total = 0, intersects_success = 0;
      for (int i = 0; i < tracked_markers.size(); i++) {
        if (!reconstruction.PointForTrack(tracked_markers[i].track)) {
          vector<Marker> reconstructed_markers;

          int track = tracked_markers[i].track;

          reconstructed_markers.push_back(tracked_markers[i]);

          // We know there're always only two markers for a track
          // Also, we're using brute-force search because we don't
          // actually know about markers layout in a list, but
          // at this moment this cycle will run just once, which
          // is not so big deal

          for (int j = i + 1; j < tracked_markers.size(); j++) {
            if (tracked_markers[j].track == track) {
              reconstructed_markers.push_back(tracked_markers[j]);
              break;
            }
          }

          intersects_total++;

          if (EuclideanIntersect(reconstructed_markers, &reconstruction)) {
            LG << "Ran Intersect() for track " << track;
            intersects_success++;
          } else {
            LG << "Filed to intersect track " << track;
          }
        }
      }

      double success_intersects_factor =
          (double) intersects_success / intersects_total;

      if (success_intersects_factor < success_intersects_factor_best) {
        LG << "Skip keyframe candidate because of "
              "lower successful intersections ratio";

        continue;
      }

      success_intersects_factor_best = success_intersects_factor;

      Tracks two_frames_tracks(tracked_markers);
      CameraIntrinsics empty_intrinsics;
      BundleEvaluation evaluation;
      evaluation.evaluate_jacobian = true;

      EuclideanBundleCommonIntrinsics(two_frames_tracks,
                                      BUNDLE_NO_INTRINSICS,
                                      BUNDLE_NO_CONSTRAINTS,
                                      &reconstruction,
                                      &empty_intrinsics,
                                      &evaluation);

      Mat &jacobian = evaluation.jacobian;

      Mat JT_J = jacobian.transpose() * jacobian;
      Mat JT_J_inv = pseudoInverse(JT_J);

      Mat temp_derived = JT_J * JT_J_inv * JT_J;
      bool is_inversed = (temp_derived - JT_J).cwiseAbs2().sum() <
          1e-4 * std::min(temp_derived.cwiseAbs2().sum(),
                          JT_J.cwiseAbs2().sum());

      LG << "Check on inversed: " << (is_inversed ? "true" : "false" )
         << ", det(JT_J): " << JT_J.determinant();

      if (!is_inversed) {
        LG << "Ignoring candidature due to poor jacobian stability";
        continue;
      }

      Mat Sigma_P;
      Sigma_P = JT_J_inv.bottomRightCorner(evaluation.num_points * 3,
                                           evaluation.num_points * 3);

      int I = evaluation.num_points;
      int A = 12;

      double Sc = static_cast<double>(I + A) / Square(3 * I) * Sigma_P.trace();

      LG << "Expected estimation error between "
         << current_keyframe << " and "
         << candidate_image << ": " << Sc;

      // Pairing with a lower Sc indicates a better choice
      if (Sc > Sc_best_candidate)
        continue;

      Sc_best_candidate = Sc;

      next_keyframe = candidate_image;
    }

    // This is a bit arbitrary and main reason of having this is to deal
    // better with situations when there's no keyframes were found for
    // current keyframe this could happen when there's no so much parallax
    // in the beginning of image sequence and then most of features are
    // getting occluded. In this case there could be good keyframe pair in
    // the middle of the sequence
    //
    // However, it's just quick hack and smarter way to do this would be nice
    if (next_keyframe == -1) {
      next_keyframe = current_keyframe + 10;
      number_keyframes = 0;

      if (next_keyframe >= max_image)
        break;

      LG << "Starting searching for keyframes starting from " << next_keyframe;
    } else {
      // New pair's expected reconstruction error is lower
      // than existing pair's one.
      //
      // For now let's store just one candidate, easy to
      // store more candidates but needs some thoughts
      // how to choose best one automatically from them
      // (or allow user to choose pair manually).
      if (Sc_best > Sc_best_candidate) {
        keyframes.clear();
        keyframes.push_back(current_keyframe);
        keyframes.push_back(next_keyframe);
        Sc_best = Sc_best_candidate;
      }
    }
  }
}

}  // namespace libmv
