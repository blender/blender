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

#ifndef LIBMV_SIMPLE_PIPELINE_CAMERA_INTRINSICS_H_
#define LIBMV_SIMPLE_PIPELINE_CAMERA_INTRINSICS_H_

#include <iostream>
#include <string>

#include <Eigen/Core>

#include "libmv/numeric/numeric.h"
#include "libmv/simple_pipeline/distortion_models.h"

namespace libmv {

class CameraIntrinsics;

namespace internal {

// This class is responsible to store a lookup grid to perform
// image warping using this lookup grid. Such a magic is needed
// to make (un)distortion as fast as possible in cases multiple
// images are to be processed.
class LookupWarpGrid {
 public:
  LookupWarpGrid();
  LookupWarpGrid(const LookupWarpGrid &from);
  ~LookupWarpGrid();

  // Width and height og the image, measured in pixels.
  int width()  const { return width_; }
  int height() const { return height_; }

  // Overscan factor of the image, so that
  // - 0.0 is overscan of 0 pixels,
  // - 1.0 is overscan of image weight pixels in horizontal direction
  //   and image height pixels in vertical direction.
  double overscan() const { return overscan_; }

  // Update lookup grid in order to be sure it's calculated for
  // given image width, height and overscan.
  //
  // See comment for CameraIntrinsics::DistortBuffer to get more
  // details about what overscan is.
  template<typename WarpFunction>
  void Update(const CameraIntrinsics &intrinsics,
              int width,
              int height,
              double overscan);

  // Apply coordinate lookup grid on a giver input buffer.
  //
  // See comment for CameraIntrinsics::DistortBuffer to get more
  // details about template type.
  template<typename PixelType>
  void Apply(const PixelType *input_buffer,
             int width,
             int height,
             int channels,
             PixelType *output_buffer);

  // Reset lookup grids.
  // This will tag the grid for update without re-computing it.
  void Reset();

  // Set number of threads used for threaded buffer distortion/undistortion.
  void SetThreads(int threads);

 private:
  // This structure contains an offset in both x,y directions
  // in an optimized way sawing some bytes per pixel in the memory.
  //
  // TODO(sergey): This is rather questionable optimizations, memory
  // is cheap nowadays and storing offset in such a way implies much
  // more operations comparing to using bare floats.
  struct Offset {
    // Integer part of the offset.
    short ix, iy;

    // Float part of an offset, to get a real float value divide this
    // value by 255.
    unsigned char fx, fy;
  };

  // Compute coordinate lookup grid using a giver warp functor.
  //
  // width and height corresponds to a size of buffer which will
  // be warped later.
  template<typename WarpFunction>
  void Compute(const CameraIntrinsics &intrinsics,
               int width,
               int height,
               double overscan);

  // This is a buffer which contains per-pixel offset of the
  // pixels from input buffer to correspond the warping function.
  Offset *offset_;

  // Dimensions of the image this lookup grid processes.
  int width_, height_;

  // Overscan of the image being processed by this grid.
  double overscan_;

  // Number of threads which will be used for buffer istortion/undistortion.
  int threads_;
};

}  // namespace internal

class CameraIntrinsics {
 public:
  CameraIntrinsics();
  CameraIntrinsics(const CameraIntrinsics &from);
  virtual ~CameraIntrinsics() {}

  virtual DistortionModelType GetDistortionModelType() const = 0;

  int image_width()  const { return image_width_;  }
  int image_height() const { return image_height_; }

  const Mat3 &K() const { return K_; }

  double focal_length()      const { return K_(0, 0); }
  double focal_length_x()    const { return K_(0, 0); }
  double focal_length_y()    const { return K_(1, 1); }

  double principal_point_x() const { return K_(0, 2); }
  double principal_point_y() const { return K_(1, 2); }

  virtual int num_distortion_parameters() const = 0;
  virtual double *distortion_parameters() = 0;
  virtual const double *distortion_parameters() const = 0;

  // Set the image size in pixels.
  // Image is the size of image camera intrinsics were calibrated with.
  void SetImageSize(int width, int height);

  // Set the entire calibration matrix at once.
  void SetK(const Mat3 new_k);

  // Set both x and y focal length in pixels.
  void SetFocalLength(double focal_x, double focal_y);

  // Set principal point in pixels.
  void SetPrincipalPoint(double cx, double cy);

  // Set number of threads used for threaded buffer distortion/undistortion.
  void SetThreads(int threads);

  // Convert image space coordinates to normalized.
  void ImageSpaceToNormalized(double image_x,
                              double image_y,
                              double *normalized_x,
                              double *normalized_y) const;

  // Convert normalized coordinates to image space.
  void NormalizedToImageSpace(double normalized_x,
                              double normalized_y,
                              double *image_x,
                              double *image_y) const;

  // Apply camera intrinsics to the normalized point to get image coordinates.
  //
  // This applies the lens distortion to a point which is in normalized
  // camera coordinates (i.e. the principal point is at (0, 0)) to get image
  // coordinates in pixels.
  virtual void ApplyIntrinsics(double normalized_x,
                               double normalized_y,
                               double *image_x,
                               double *image_y) const = 0;

  // Invert camera intrinsics on the image point to get normalized coordinates.
  //
  // This reverses the effect of lens distortion on a point which is in image
  // coordinates to get normalized camera coordinates.
  virtual void InvertIntrinsics(double image_x,
                                double image_y,
                                double *normalized_x,
                                double *normalized_y) const = 0;

  // Distort an image using the current camera instrinsics
  //
  // The distorted image is computed in output_buffer using samples from
  // input_buffer. Both buffers should be width x height x channels sized.
  //
  // Overscan is a percentage value of how much overcan the image have.
  // For example overscal value of 0.2 means 20% of overscan in the
  // buffers.
  //
  // Overscan is usually used in cases when one need to distort an image
  // and don't have a barrel in the distorted buffer. For example, when
  // one need to render properly distorted FullHD frame without barrel
  // visible. For such cases renderers usually renders bigger images and
  // crops them after the distortion.
  //
  // This method is templated to be able to distort byte and float buffers
  // without having separate methods for this two types. So basically only
  //
  // But in fact PixelType might be any type for which multiplication by
  // a scalar and addition are implemented. For example PixelType might be
  // Vec3 as well.
  template<typename PixelType>
  void DistortBuffer(const PixelType *input_buffer,
                     int width,
                     int height,
                     double overscan,
                     int channels,
                     PixelType *output_buffer);

  // Undistort an image using the current camera instrinsics
  //
  // The undistorted image is computed in output_buffer using samples from
  // input_buffer. Both buffers should be width x height x channels sized.
  //
  // Overscan is a percentage value of how much overcan the image have.
  // For example overscal value of 0.2 means 20% of overscan in the
  // buffers.
  //
  // Overscan is usually used in cases when one need to distort an image
  // and don't have a barrel in the distorted buffer. For example, when
  // one need to render properly distorted FullHD frame without barrel
  // visible. For such cases renderers usually renders bigger images and
  // crops them after the distortion.
  //
  // This method is templated to be able to distort byte and float buffers
  // without having separate methods for this two types. So basically only
  //
  // But in fact PixelType might be any type for which multiplication by
  // a scalar and addition are implemented. For example PixelType might be
  // Vec3 as well.
  template<typename PixelType>
  void UndistortBuffer(const PixelType *input_buffer,
                       int width,
                       int height,
                       double overscan,
                       int channels,
                       PixelType *output_buffer);

 private:
  // This is the size of the image. This is necessary to, for example, handle
  // the case of processing a scaled image.
  int image_width_;
  int image_height_;

  // The traditional intrinsics matrix from x = K[R|t]X.
  Mat3 K_;

  // Coordinate lookup grids for distortion and undistortion.
  internal::LookupWarpGrid distort_;
  internal::LookupWarpGrid undistort_;

 protected:
  // Reset lookup grids after changing the distortion model.
  void ResetLookupGrids();
};

class PolynomialCameraIntrinsics : public CameraIntrinsics {
 public:
  // This constants defines an offset of corresponding coefficients
  // in the arameters_ array.
  enum {
    OFFSET_K1,
    OFFSET_K2,
    OFFSET_K3,
    OFFSET_P1,
    OFFSET_P2,

    // This defines the size of array which we need to have in order
    // to store all the coefficients.
    NUM_PARAMETERS,
  };

  PolynomialCameraIntrinsics();
  PolynomialCameraIntrinsics(const PolynomialCameraIntrinsics &from);

  DistortionModelType GetDistortionModelType() const {
    return DISTORTION_MODEL_POLYNOMIAL;
  }

  int num_distortion_parameters() const { return NUM_PARAMETERS; }
  double *distortion_parameters() { return parameters_; };
  const double *distortion_parameters() const { return parameters_; };

  double k1() const { return parameters_[OFFSET_K1]; }
  double k2() const { return parameters_[OFFSET_K2]; }
  double k3() const { return parameters_[OFFSET_K3]; }
  double p1() const { return parameters_[OFFSET_P1]; }
  double p2() const { return parameters_[OFFSET_P2]; }

  // Set radial distortion coeffcients.
  void SetRadialDistortion(double k1, double k2, double k3);

  // Set tangential distortion coeffcients.
  void SetTangentialDistortion(double p1, double p2);

  // Apply camera intrinsics to the normalized point to get image coordinates.
  //
  // This applies the lens distortion to a point which is in normalized
  // camera coordinates (i.e. the principal point is at (0, 0)) to get image
  // coordinates in pixels.
  void ApplyIntrinsics(double normalized_x,
                       double normalized_y,
                       double *image_x,
                       double *image_y) const;

  // Invert camera intrinsics on the image point to get normalized coordinates.
  //
  // This reverses the effect of lens distortion on a point which is in image
  // coordinates to get normalized camera coordinates.
  void InvertIntrinsics(double image_x,
                        double image_y,
                        double *normalized_x,
                        double *normalized_y) const;

 private:
  // OpenCV's distortion model with third order polynomial radial distortion
  // terms and second order tangential distortion. The distortion is applied to
  // the normalized coordinates before the focal length, which makes them
  // independent of image size.
  double parameters_[NUM_PARAMETERS];
};

class DivisionCameraIntrinsics : public CameraIntrinsics {
 public:
  // This constants defines an offset of corresponding coefficients
  // in the arameters_ array.
  enum {
    OFFSET_K1,
    OFFSET_K2,

    // This defines the size of array which we need to have in order
    // to store all the coefficients.
    NUM_PARAMETERS,
  };

  DivisionCameraIntrinsics();
  DivisionCameraIntrinsics(const DivisionCameraIntrinsics &from);

  DistortionModelType GetDistortionModelType() const {
    return DISTORTION_MODEL_DIVISION;
  }

  int num_distortion_parameters() const { return NUM_PARAMETERS; }
  double *distortion_parameters() { return parameters_; };
  const double *distortion_parameters() const { return parameters_; };

  double k1() const { return parameters_[OFFSET_K1]; }
  double k2() const { return parameters_[OFFSET_K2]; }

  // Set radial distortion coeffcients.
  void SetDistortion(double k1, double k2);

  // Apply camera intrinsics to the normalized point to get image coordinates.
  //
  // This applies the lens distortion to a point which is in normalized
  // camera coordinates (i.e. the principal point is at (0, 0)) to get image
  // coordinates in pixels.
  void ApplyIntrinsics(double normalized_x,
                       double normalized_y,
                       double *image_x,
                       double *image_y) const;

  // Invert camera intrinsics on the image point to get normalized coordinates.
  //
  // This reverses the effect of lens distortion on a point which is in image
  // coordinates to get normalized camera coordinates.
  void InvertIntrinsics(double image_x,
                        double image_y,
                        double *normalized_x,
                        double *normalized_y) const;

 private:
  // Double-parameter division distortion model.
  double parameters_[NUM_PARAMETERS];
};

/// A human-readable representation of the camera intrinsic parameters.
std::ostream& operator <<(std::ostream &os,
                          const CameraIntrinsics &intrinsics);

}  // namespace libmv

// Include implementation of all templated methods here,
// so they're visible to the compiler.
#include "libmv/simple_pipeline/camera_intrinsics_impl.h"

#endif  // LIBMV_SIMPLE_PIPELINE_CAMERA_INTRINSICS_H_
