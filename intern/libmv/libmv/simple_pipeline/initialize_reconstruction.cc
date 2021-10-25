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

#include "libmv/simple_pipeline/initialize_reconstruction.h"

#include "libmv/base/vector.h"
#include "libmv/logging/logging.h"
#include "libmv/multiview/fundamental.h"
#include "libmv/multiview/projection.h"
#include "libmv/numeric/levenberg_marquardt.h"
#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"

namespace libmv {
namespace {

void GetImagesInMarkers(const vector<Marker> &markers,
                        int *image1, int *image2) {
  if (markers.size() < 2) {
    return;
  }
  *image1 = markers[0].image;
  for (int i = 1; i < markers.size(); ++i) {
    if (markers[i].image != *image1) {
      *image2 = markers[i].image;
      return;
    }
  }
  *image2 = -1;
  LOG(FATAL) << "Only one image in the markers.";
}

}  // namespace

bool EuclideanReconstructTwoFrames(const vector<Marker> &markers,
                                   EuclideanReconstruction *reconstruction) {
  if (markers.size() < 16) {
    LG << "Not enough markers to initialize from two frames: " << markers.size();
    return false;
  }

  int image1, image2;
  GetImagesInMarkers(markers, &image1, &image2);

  Mat x1, x2;
  CoordinatesForMarkersInImage(markers, image1, &x1);
  CoordinatesForMarkersInImage(markers, image2, &x2);

  Mat3 F;
  NormalizedEightPointSolver(x1, x2, &F);

  // The F matrix should be an E matrix, but squash it just to be sure.
  Mat3 E;
  FundamentalToEssential(F, &E);

  // Recover motion between the two images. Since this function assumes a
  // calibrated camera, use the identity for K.
  Mat3 R;
  Vec3 t;
  Mat3 K = Mat3::Identity();
  if (!MotionFromEssentialAndCorrespondence(E,
                                            K, x1.col(0),
                                            K, x2.col(0),
                                            &R, &t)) {
    LG << "Failed to compute R and t from E and K.";
    return false;
  }

  // Image 1 gets the reference frame, image 2 gets the relative motion.
  reconstruction->InsertCamera(image1, Mat3::Identity(), Vec3::Zero());
  reconstruction->InsertCamera(image2, R, t);

  LG << "From two frame reconstruction got:\nR:\n" << R
     << "\nt:" << t.transpose();
  return true;
}

namespace {

Mat3 DecodeF(const Vec9 &encoded_F) {
  // Decode F and force it to be rank 2.
  Map<const Mat3> full_rank_F(encoded_F.data(), 3, 3);
  Eigen::JacobiSVD<Mat3> svd(full_rank_F,
                             Eigen::ComputeFullU | Eigen::ComputeFullV);
  Vec3 diagonal = svd.singularValues();
  diagonal(2) = 0;
  Mat3 F = svd.matrixU() * diagonal.asDiagonal() * svd.matrixV().transpose();
  return F;
}

// This is the stupidest way to refine F known to mankind, since it requires
// doing a full SVD of F at each iteration. This uses sampson error.
struct FundamentalSampsonCostFunction {
 public:
  typedef Vec   FMatrixType;
  typedef Vec9 XMatrixType;

  // Assumes markers are ordered by track.
  explicit FundamentalSampsonCostFunction(const vector<Marker> &markers)
    : markers(markers) {}

  Vec operator()(const Vec9 &encoded_F) const {
    // Decode F and force it to be rank 2.
    Mat3 F = DecodeF(encoded_F);

    Vec residuals(markers.size() / 2);
    residuals.setZero();
    for (int i = 0; i < markers.size() / 2; ++i) {
      const Marker &marker1 = markers[2*i + 0];
      const Marker &marker2 = markers[2*i + 1];
      CHECK_EQ(marker1.track, marker2.track);
      Vec2 x1(marker1.x, marker1.y);
      Vec2 x2(marker2.x, marker2.y);

      residuals[i] = SampsonDistance(F, x1, x2);
    }
    return residuals;
  }
  const vector<Marker> &markers;
};

}  // namespace

bool ProjectiveReconstructTwoFrames(const vector<Marker> &markers,
                                    ProjectiveReconstruction *reconstruction) {
  if (markers.size() < 16) {
    return false;
  }

  int image1, image2;
  GetImagesInMarkers(markers, &image1, &image2);

  Mat x1, x2;
  CoordinatesForMarkersInImage(markers, image1, &x1);
  CoordinatesForMarkersInImage(markers, image2, &x2);

  Mat3 F;
  NormalizedEightPointSolver(x1, x2, &F);

  // XXX Verify sampson distance.
#if 0
  // Refine the resulting projection fundamental matrix using Sampson's
  // approximation of geometric error. This avoids having to do a full bundle
  // at the cost of some accuracy.
  //
  // TODO(keir): After switching to a better bundling library, use a proper
  // full bundle adjust here instead of this lame bundle adjustment.
  typedef LevenbergMarquardt<FundamentalSampsonCostFunction> Solver;

  FundamentalSampsonCostFunction fundamental_cost(markers);

  // Pack the initial P matrix into a size-12 vector..
  Vec9 encoded_F = Map<Vec9>(F.data(), 3, 3);

  Solver solver(fundamental_cost);

  Solver::SolverParameters params;
  Solver::Results results = solver.minimize(params, &encoded_F);
  // TODO(keir): Check results to ensure clean termination.

  // Recover F from the minimization.
  F = DecodeF(encoded_F);
#endif

  // Image 1 gets P = [I|0], image 2 gets arbitrary P.
  Mat34 P1 = Mat34::Zero();
  P1.block<3, 3>(0, 0) = Mat3::Identity();
  Mat34 P2;
  ProjectionsFromFundamental(F, &P1, &P2);

  reconstruction->InsertCamera(image1, P1);
  reconstruction->InsertCamera(image2, P2);

  LG << "From two frame reconstruction got P2:\n" << P2;
  return true;
}
}  // namespace libmv
