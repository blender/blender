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
#include "libmv/simple_pipeline/packed_intrinsics.h"

namespace libmv {

namespace internal {

LookupWarpGrid::LookupWarpGrid()
    : offset_(NULL), width_(0), height_(0), overscan_(0.0) {
}

LookupWarpGrid::LookupWarpGrid(const LookupWarpGrid& from)
    : offset_(NULL),
      width_(from.width_),
      height_(from.height_),
      overscan_(from.overscan_) {
  if (from.offset_) {
    offset_ = new Offset[width_ * height_];
    memcpy(offset_, from.offset_, sizeof(Offset) * width_ * height_);
  }
}

LookupWarpGrid::~LookupWarpGrid() {
  delete[] offset_;
}

void LookupWarpGrid::Reset() {
  delete[] offset_;
  offset_ = NULL;
}

}  // namespace internal

CameraIntrinsics::CameraIntrinsics()
    : image_width_(0), image_height_(0), K_(Mat3::Identity()) {
}

CameraIntrinsics::CameraIntrinsics(const CameraIntrinsics& from)
    : image_width_(from.image_width_),
      image_height_(from.image_height_),
      K_(from.K_),
      distort_(from.distort_),
      undistort_(from.undistort_) {
}

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
void CameraIntrinsics::SetFocalLength(double focal_x, double focal_y) {
  K_(0, 0) = focal_x;
  K_(1, 1) = focal_y;
  ResetLookupGrids();
}

// Set principal point in pixels.
void CameraIntrinsics::SetPrincipalPoint(double cx, double cy) {
  K_(0, 2) = cx;
  K_(1, 2) = cy;
  ResetLookupGrids();
}

void CameraIntrinsics::ImageSpaceToNormalized(double image_x,
                                              double image_y,
                                              double* normalized_x,
                                              double* normalized_y) const {
  *normalized_x = (image_x - principal_point_x()) / focal_length_x();
  *normalized_y = (image_y - principal_point_y()) / focal_length_y();
}

void CameraIntrinsics::NormalizedToImageSpace(double normalized_x,
                                              double normalized_y,
                                              double* image_x,
                                              double* image_y) const {
  *image_x = normalized_x * focal_length_x() + principal_point_x();
  *image_y = normalized_y * focal_length_y() + principal_point_y();
}

// Reset lookup grids after changing the distortion model.
void CameraIntrinsics::ResetLookupGrids() {
  distort_.Reset();
  undistort_.Reset();
}

void CameraIntrinsics::Pack(PackedIntrinsics* packed_intrinsics) const {
  packed_intrinsics->SetFocalLength(focal_length());
  packed_intrinsics->SetPrincipalPoint(principal_point_x(),
                                       principal_point_y());
}

void CameraIntrinsics::Unpack(const PackedIntrinsics& packed_intrinsics) {
  SetFocalLength(packed_intrinsics.GetFocalLength(),
                 packed_intrinsics.GetFocalLength());

  SetPrincipalPoint(packed_intrinsics.GetPrincipalPointX(),
                    packed_intrinsics.GetPrincipalPointY());
}

// Polynomial model.

PolynomialCameraIntrinsics::PolynomialCameraIntrinsics() : CameraIntrinsics() {
  SetRadialDistortion(0.0, 0.0, 0.0);
  SetTangentialDistortion(0.0, 0.0);
}

PolynomialCameraIntrinsics::PolynomialCameraIntrinsics(
    const PolynomialCameraIntrinsics& from)
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

void PolynomialCameraIntrinsics::SetTangentialDistortion(double p1, double p2) {
  parameters_[OFFSET_P1] = p1;
  parameters_[OFFSET_P2] = p2;
  ResetLookupGrids();
}

void PolynomialCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                                 double normalized_y,
                                                 double* image_x,
                                                 double* image_y) const {
  ApplyPolynomialDistortionModel(focal_length_x(),
                                 focal_length_y(),
                                 principal_point_x(),
                                 principal_point_y(),
                                 k1(),
                                 k2(),
                                 k3(),
                                 p1(),
                                 p2(),
                                 normalized_x,
                                 normalized_y,
                                 image_x,
                                 image_y);
}

void PolynomialCameraIntrinsics::InvertIntrinsics(double image_x,
                                                  double image_y,
                                                  double* normalized_x,
                                                  double* normalized_y) const {
  InvertPolynomialDistortionModel(focal_length_x(),
                                  focal_length_y(),
                                  principal_point_x(),
                                  principal_point_y(),
                                  k1(),
                                  k2(),
                                  k3(),
                                  p1(),
                                  p2(),
                                  image_x,
                                  image_y,
                                  normalized_x,
                                  normalized_y);
}

void PolynomialCameraIntrinsics::Pack(
    PackedIntrinsics* packed_intrinsics) const {
  CameraIntrinsics::Pack(packed_intrinsics);

  packed_intrinsics->SetK1(k1());
  packed_intrinsics->SetK2(k2());
  packed_intrinsics->SetK3(k3());

  packed_intrinsics->SetP1(p1());
  packed_intrinsics->SetP2(p2());
}

void PolynomialCameraIntrinsics::Unpack(
    const PackedIntrinsics& packed_intrinsics) {
  CameraIntrinsics::Unpack(packed_intrinsics);

  SetRadialDistortion(packed_intrinsics.GetK1(),
                      packed_intrinsics.GetK2(),
                      packed_intrinsics.GetK3());

  SetTangentialDistortion(packed_intrinsics.GetP1(), packed_intrinsics.GetP2());
}

// Division model.

DivisionCameraIntrinsics::DivisionCameraIntrinsics() : CameraIntrinsics() {
  SetDistortion(0.0, 0.0);
}

DivisionCameraIntrinsics::DivisionCameraIntrinsics(
    const DivisionCameraIntrinsics& from)
    : CameraIntrinsics(from) {
  SetDistortion(from.k1(), from.k1());
}

void DivisionCameraIntrinsics::SetDistortion(double k1, double k2) {
  parameters_[OFFSET_K1] = k1;
  parameters_[OFFSET_K2] = k2;
  ResetLookupGrids();
}

void DivisionCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                               double normalized_y,
                                               double* image_x,
                                               double* image_y) const {
  ApplyDivisionDistortionModel(focal_length_x(),
                               focal_length_y(),
                               principal_point_x(),
                               principal_point_y(),
                               k1(),
                               k2(),
                               normalized_x,
                               normalized_y,
                               image_x,
                               image_y);
}

void DivisionCameraIntrinsics::InvertIntrinsics(double image_x,
                                                double image_y,
                                                double* normalized_x,
                                                double* normalized_y) const {
  InvertDivisionDistortionModel(focal_length_x(),
                                focal_length_y(),
                                principal_point_x(),
                                principal_point_y(),
                                k1(),
                                k2(),
                                image_x,
                                image_y,
                                normalized_x,
                                normalized_y);
}

void DivisionCameraIntrinsics::Pack(PackedIntrinsics* packed_intrinsics) const {
  CameraIntrinsics::Pack(packed_intrinsics);

  packed_intrinsics->SetK1(k1());
  packed_intrinsics->SetK2(k2());
}

void DivisionCameraIntrinsics::Unpack(
    const PackedIntrinsics& packed_intrinsics) {
  CameraIntrinsics::Unpack(packed_intrinsics);

  SetDistortion(packed_intrinsics.GetK1(), packed_intrinsics.GetK2());
}

// Nuke model.

NukeCameraIntrinsics::NukeCameraIntrinsics() : CameraIntrinsics() {
  SetRadialDistortion(0.0, 0.0);
  SetTangentialDistortion(0.0, 0.0);
}

NukeCameraIntrinsics::NukeCameraIntrinsics(const NukeCameraIntrinsics& from)
    : CameraIntrinsics(from) {
  SetRadialDistortion(from.k1(), from.k2());
  SetTangentialDistortion(from.p1(), from.p2());
}

void NukeCameraIntrinsics::SetRadialDistortion(double k1, double k2) {
  parameters_[OFFSET_K1] = k1;
  parameters_[OFFSET_K2] = k2;
  ResetLookupGrids();
}

void NukeCameraIntrinsics::SetTangentialDistortion(double p1, double p2) {
  parameters_[OFFSET_P1] = p1;
  parameters_[OFFSET_P2] = p2;
  ResetLookupGrids();
}

void NukeCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                           double normalized_y,
                                           double* image_x,
                                           double* image_y) const {
  ApplyNukeDistortionModel(focal_length_x(),
                           focal_length_y(),
                           principal_point_x(),
                           principal_point_y(),
                           image_width(),
                           image_height(),
                           k1(),
                           k2(),
                           p1(),
                           p2(),
                           normalized_x,
                           normalized_y,
                           image_x,
                           image_y);
}

void NukeCameraIntrinsics::InvertIntrinsics(double image_x,
                                            double image_y,
                                            double* normalized_x,
                                            double* normalized_y) const {
  InvertNukeDistortionModel(focal_length_x(),
                            focal_length_y(),
                            principal_point_x(),
                            principal_point_y(),
                            image_width(),
                            image_height(),
                            k1(),
                            k2(),
                            p1(),
                            p2(),
                            image_x,
                            image_y,
                            normalized_x,
                            normalized_y);
}

void NukeCameraIntrinsics::Pack(PackedIntrinsics* packed_intrinsics) const {
  CameraIntrinsics::Pack(packed_intrinsics);

  packed_intrinsics->SetK1(k1());
  packed_intrinsics->SetK2(k2());

  packed_intrinsics->SetP1(p1());
  packed_intrinsics->SetP2(p2());
}

void NukeCameraIntrinsics::Unpack(const PackedIntrinsics& packed_intrinsics) {
  CameraIntrinsics::Unpack(packed_intrinsics);

  SetRadialDistortion(packed_intrinsics.GetK1(), packed_intrinsics.GetK2());
  SetTangentialDistortion(packed_intrinsics.GetP1(), packed_intrinsics.GetP2());
}

// Brown model.

BrownCameraIntrinsics::BrownCameraIntrinsics() : CameraIntrinsics() {
  SetRadialDistortion(0.0, 0.0, 0.0, 0.0);
  SetTangentialDistortion(0.0, 0.0);
}

BrownCameraIntrinsics::BrownCameraIntrinsics(const BrownCameraIntrinsics& from)
    : CameraIntrinsics(from) {
  SetRadialDistortion(from.k1(), from.k2(), from.k3(), from.k4());
  SetTangentialDistortion(from.p1(), from.p2());
}

void BrownCameraIntrinsics::SetRadialDistortion(double k1,
                                                double k2,
                                                double k3,
                                                double k4) {
  parameters_[OFFSET_K1] = k1;
  parameters_[OFFSET_K2] = k2;
  parameters_[OFFSET_K3] = k3;
  parameters_[OFFSET_K4] = k4;
  ResetLookupGrids();
}

void BrownCameraIntrinsics::SetTangentialDistortion(double p1, double p2) {
  parameters_[OFFSET_P1] = p1;
  parameters_[OFFSET_P2] = p2;
  ResetLookupGrids();
}

void BrownCameraIntrinsics::ApplyIntrinsics(double normalized_x,
                                            double normalized_y,
                                            double* image_x,
                                            double* image_y) const {
  ApplyBrownDistortionModel(focal_length_x(),
                            focal_length_y(),
                            principal_point_x(),
                            principal_point_y(),
                            k1(),
                            k2(),
                            k3(),
                            k4(),
                            p1(),
                            p2(),
                            normalized_x,
                            normalized_y,
                            image_x,
                            image_y);
}

void BrownCameraIntrinsics::InvertIntrinsics(double image_x,
                                             double image_y,
                                             double* normalized_x,
                                             double* normalized_y) const {
  InvertBrownDistortionModel(focal_length_x(),
                             focal_length_y(),
                             principal_point_x(),
                             principal_point_y(),
                             k1(),
                             k2(),
                             k3(),
                             k4(),
                             p1(),
                             p2(),
                             image_x,
                             image_y,
                             normalized_x,
                             normalized_y);
}

void BrownCameraIntrinsics::Pack(PackedIntrinsics* packed_intrinsics) const {
  CameraIntrinsics::Pack(packed_intrinsics);

  packed_intrinsics->SetK1(k1());
  packed_intrinsics->SetK2(k2());
  packed_intrinsics->SetK3(k3());
  packed_intrinsics->SetK4(k4());

  packed_intrinsics->SetP1(p1());
  packed_intrinsics->SetP2(p2());
}

void BrownCameraIntrinsics::Unpack(const PackedIntrinsics& packed_intrinsics) {
  CameraIntrinsics::Unpack(packed_intrinsics);

  SetRadialDistortion(packed_intrinsics.GetK1(),
                      packed_intrinsics.GetK2(),
                      packed_intrinsics.GetK3(),
                      packed_intrinsics.GetK4());

  SetTangentialDistortion(packed_intrinsics.GetP1(), packed_intrinsics.GetP2());
}

std::ostream& operator<<(std::ostream& os, const CameraIntrinsics& intrinsics) {
  if (intrinsics.focal_length_x() == intrinsics.focal_length_x()) {
    os << "f=" << intrinsics.focal_length();
  } else {
    os << "fx=" << intrinsics.focal_length_x()
       << " fy=" << intrinsics.focal_length_y();
  }
  os << " cx=" << intrinsics.principal_point_x()
     << " cy=" << intrinsics.principal_point_y()
     << " w=" << intrinsics.image_width() << " h=" << intrinsics.image_height();

#define PRINT_NONZERO_COEFFICIENT(intrinsics, coeff)                           \
  {                                                                            \
    if (intrinsics->coeff() != 0.0) {                                          \
      os << " " #coeff "=" << intrinsics->coeff();                             \
    }                                                                          \
  }                                                                            \
  (void)0

  switch (intrinsics.GetDistortionModelType()) {
    case DISTORTION_MODEL_POLYNOMIAL: {
      const PolynomialCameraIntrinsics* polynomial_intrinsics =
          static_cast<const PolynomialCameraIntrinsics*>(&intrinsics);
      PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k1);
      PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k2);
      PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, k3);
      PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, p1);
      PRINT_NONZERO_COEFFICIENT(polynomial_intrinsics, p2);
      break;
    }
    case DISTORTION_MODEL_DIVISION: {
      const DivisionCameraIntrinsics* division_intrinsics =
          static_cast<const DivisionCameraIntrinsics*>(&intrinsics);
      PRINT_NONZERO_COEFFICIENT(division_intrinsics, k1);
      PRINT_NONZERO_COEFFICIENT(division_intrinsics, k2);
      break;
    }
    case DISTORTION_MODEL_NUKE: {
      const NukeCameraIntrinsics* nuke_intrinsics =
          static_cast<const NukeCameraIntrinsics*>(&intrinsics);
      PRINT_NONZERO_COEFFICIENT(nuke_intrinsics, k1);
      PRINT_NONZERO_COEFFICIENT(nuke_intrinsics, k2);
      PRINT_NONZERO_COEFFICIENT(nuke_intrinsics, p1);
      PRINT_NONZERO_COEFFICIENT(nuke_intrinsics, p2);
      break;
    }
    case DISTORTION_MODEL_BROWN: {
      const BrownCameraIntrinsics* brown_intrinsics =
          static_cast<const BrownCameraIntrinsics*>(&intrinsics);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, k1);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, k2);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, k3);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, k4);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, p1);
      PRINT_NONZERO_COEFFICIENT(brown_intrinsics, p2);
      break;
    }
    default: LOG(FATAL) << "Unknown distortion model.";
  }

#undef PRINT_NONZERO_COEFFICIENT

  return os;
}

}  // namespace libmv
