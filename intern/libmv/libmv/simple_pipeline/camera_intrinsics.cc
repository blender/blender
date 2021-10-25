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

#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/distortion_models.h"

namespace libmv {

namespace internal {

LookupWarpGrid::LookupWarpGrid()
  : offset_(NULL),
    width_(0),
    height_(0),
    overscan_(0.0),
    threads_(1) {}

LookupWarpGrid::LookupWarpGrid(const LookupWarpGrid &from)
    : offset_(NULL),
      width_(from.width_),
      height_(from.height_),
      overscan_(from.overscan_),
      threads_(from.threads_) {
  if (from.offset_) {
    offset_ = new Offset[width_ * height_];
    memcpy(offset_, from.offset_, sizeof(Offset) * width_ * height_);
  }
}

LookupWarpGrid::~LookupWarpGrid() {
  delete [] offset_;
}

void LookupWarpGrid::Reset() {
  delete [] offset_;
  offset_ = NULL;
}

// Set number of threads used for threaded buffer distortion/undistortion.
void LookupWarpGrid::SetThreads(int threads) {
  threads_ = threads;
}

}  // namespace internal

CameraIntrinsics::CameraIntrinsics()
    : image_width_(0),
      image_height_(0),
      K_(Mat3::Identity()) {}

CameraIntrinsics::CameraIntrinsics(const CameraIntrinsics &from)
    : image_width_(from.image_width_),
      image_height_(from.image_height_),
      K_(from.K_),
      distort_(from.distort_),
      undistort_(from.undistort_) {}

// Set the image size in pixels.
void CameraIntrinsics::SetImageSize(int width, int height) {
  image_width_ = width;
  image_height_ = height;
  ResetLookupGrids();
}

// Set the entire calibration matrix at once.
void CameraIntrinsics::SetK(const Mat3 new_k) {
  K_ = new_k;
  ResetLookupGrids();
}

// Set both x and y focal length in pixels.
void CameraIntrinsics::SetFocalLength(double focal_x,
                                      double focal_y) {
  K_(0, 0) = focal_x;
  K_(1, 1) = focal_y;
  ResetLookupGrids();
}

// Set principal point in pixels.
void CameraIntrinsics::SetPrincipalPoint(double cx,
                                         double cy) {
  K_(0, 2) = cx;
  K_(1, 2) = cy;
  ResetLookupGrids();
}

// Set number of threads used for threaded buffer distortion/undistortion.
void CameraIntrinsics::SetThreads(int threads) {
  distort_.SetThreads(threads);
  undistort_.SetThreads(threads);
}

void CameraIntrinsics::ImageSpaceToNormalized(double image_x,
                                              double image_y,
                                              double *normalized_x,
                                              double *normalized_y) const {
  *normalized_x = (image_x - principal_point_x()) / focal_length_x();
  *normalized_y = (image_y - principal_point_y()) / focal_length_y();
}

void CameraIntrinsics::NormalizedToImageSpace(double normalized_x,
                                              double normalized_y,
                                              double *image_x,
                                              double *image_y) const {
  *image_x = normalized_x * focal_length_x() + principal_point_x();
  *image_y = normalized_y * focal_length_y() + principal_point_y();
}

// Reset lookup grids after changing the distortion model.
void CameraIntrinsics::ResetLookupGrids() {
  distort_.Reset();
  undistort_.Reset();
}

PolynomialCameraIntrinsics::PolynomialCameraIntrinsics()
    : CameraIntrinsics() {
  SetRadialDistortion(0.0, 0.0, 0.0);
  SetTangentialDistortion(0.0, 0.0);
}

PolynomialCameraIntrinsics::PolynomialCameraIntrinsics(
    const PolynomialCameraIntrinsics &from)
    : CameraIntrinsics(from) {
  SetRadialDistortion(from.k1(), from.k2(), from.k3());
  SetTangentialDistortion(from.p1(), from.p2());
}

void PolynomialCameraIntrinsics::SetRadialDistortion(double k1,
                                                     double k2,
                                                     double k3) {
  parameters_[OFFSET_K1] = k1;
  parameters_[OFFSET_K2] = k2;
  parameters_[OFFSET_K3] = k3;
  ResetLookupGrids();
}

void PolynomialCameraIntrinsics::SetTangentialDistortion(double p1,
                                                         double p2) {
  parameters_[OFFSET_P1] = p1;
  parameters_[OFFSET_P2] = p2;
  ResetLookupGrids();
}

void PolynomialCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                                 double normalized_y,
                                                 double *image_x,
                                                 double *image_y) const {
  ApplyPolynomialDistortionModel(focal_length_x(),
                                 focal_length_y(),
                                 principal_point_x(),
                                 principal_point_y(),
                                 k1(), k2(), k3(),
                                 p1(), p2(),
                                 normalized_x,
                                 normalized_y,
                                 image_x,
                                 image_y);
}

void PolynomialCameraIntrinsics::InvertIntrinsics(
    double image_x,
    double image_y,
    double *normalized_x,
    double *normalized_y) const {
  InvertPolynomialDistortionModel(focal_length_x(),
                                  focal_length_y(),
                                  principal_point_x(),
                                  principal_point_y(),
                                  k1(), k2(), k3(),
                                  p1(), p2(),
                                  image_x,
                                  image_y,
                                  normalized_x,
                                  normalized_y);
}

DivisionCameraIntrinsics::DivisionCameraIntrinsics()
    : CameraIntrinsics() {
  SetDistortion(0.0, 0.0);
}

DivisionCameraIntrinsics::DivisionCameraIntrinsics(
    const DivisionCameraIntrinsics &from)
    : CameraIntrinsics(from) {
  SetDistortion(from.k1(), from.k1());
}

void DivisionCameraIntrinsics::SetDistortion(double k1,
                                             double k2) {
  parameters_[OFFSET_K1] = k1;
  parameters_[OFFSET_K2] = k2;
  ResetLookupGrids();
}

void DivisionCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                               double normalized_y,
                                               double *image_x,
                                               double *image_y) const {
  ApplyDivisionDistortionModel(focal_length_x(),
                               focal_length_y(),
                               principal_point_x(),
                               principal_point_y(),
                               k1(), k2(),
                               normalized_x,
                               normalized_y,
                               image_x,
                               image_y);
}

void DivisionCameraIntrinsics::InvertIntrinsics(double image_x,
                                                double image_y,
                                                double *normalized_x,
                                                double *normalized_y) const {
  InvertDivisionDistortionModel(focal_length_x(),
                                focal_length_y(),
                                principal_point_x(),
                                principal_point_y(),
                                k1(), k2(),
                                image_x,
                                image_y,
                                normalized_x,
                                normalized_y);
}

std::ostream& operator <<(std::ostream &os,
                          const CameraIntrinsics &intrinsics) {
  if (intrinsics.focal_length_x() == intrinsics.focal_length_x()) {
    os << "f=" << intrinsics.focal_length();
  } else {
    os <<  "fx=" << intrinsics.focal_length_x()
       << " fy=" << intrinsics.focal_length_y();
  }
  os << " cx=" << intrinsics.principal_point_x()
     << " cy=" << intrinsics.principal_point_y()
     << " w=" << intrinsics.image_width()
     << " h=" << intrinsics.image_height();

#define PRINT_NONZERO_COEFFICIENT(intrinsics, coeff) \
    { \
      if (intrinsics->coeff() != 0.0) { \
        os << " " #coeff "=" << intrinsics->coeff(); \
      }                                              \
    } (void) 0

  switch (intrinsics.GetDistortionModelType()) {
    case DISTORTION_MODEL_POLYNOMIAL:
      {
        const PolynomialCameraIntrinsics *polynomial_intrinsics =
            static_cast<const PolynomialCameraIntrinsics *>(&intrinsics);
        PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k1);
        PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k2);
        PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k3);
        PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, p1);
        PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, p2);
        break;
      }
    case DISTORTION_MODEL_DIVISION:
      {
        const DivisionCameraIntrinsics *division_intrinsics =
            static_cast<const DivisionCameraIntrinsics *>(&intrinsics);
        PRINT_NONZERO_COEFFICIENT(division_intrinsics, k1);
        PRINT_NONZERO_COEFFICIENT(division_intrinsics, k2);
        break;
      }
    default:
      LOG(FATAL) << "Unknown distortion model.";
  }

#undef PRINT_NONZERO_COEFFICIENT

  return os;
}

}  // namespace libmv
