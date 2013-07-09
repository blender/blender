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

namespace libmv {

typedef Eigen::Matrix<double, 3, 3> Mat3;

struct Grid;

class CameraIntrinsics {
 public:
  CameraIntrinsics();
  CameraIntrinsics(const CameraIntrinsics &from);
  ~CameraIntrinsics();

  const Mat3 &K()                 const { return K_;            }
  double      focal_length()      const { return K_(0, 0);      }
  double      focal_length_x()    const { return K_(0, 0);      }
  double      focal_length_y()    const { return K_(1, 1);      }
  double      principal_point_x() const { return K_(0, 2);      }
  double      principal_point_y() const { return K_(1, 2);      }
  int         image_width()       const { return image_width_;  }
  int         image_height()      const { return image_height_; }
  double      k1()                const { return k1_; }
  double      k2()                const { return k2_; }
  double      k3()                const { return k3_; }
  double      p1()                const { return p1_; }
  double      p2()                const { return p2_; }

  /// Set the entire calibration matrix at once.
  void SetK(const Mat3 new_k);

  /// Set both x and y focal length in pixels.
  void SetFocalLength(double focal_x, double focal_y);

  /// Set principal point in pixels.
  void SetPrincipalPoint(double cx, double cy);

  /// Set the image size in pixels.
  void SetImageSize(int width, int height);

  void SetRadialDistortion(double k1, double k2, double k3 = 0);

  void SetTangentialDistortion(double p1, double p2);

  /// Set number of threads using for buffer distortion/undistortion
  void SetThreads(int threads);

  /*!
      Apply camera intrinsics to the normalized point to get image coordinates.

      This applies the lens distortion to a point which is in normalized
      camera coordinates (i.e. the principal point is at (0, 0)) to get image
      coordinates in pixels.
  */
  void ApplyIntrinsics(double normalized_x, double normalized_y,
                       double *image_x, double *image_y) const;

  /*!
      Invert camera intrinsics on the image point to get normalized coordinates.

      This reverses the effect of lens distortion on a point which is in image
      coordinates to get normalized camera coordinates.
  */
  void InvertIntrinsics(double image_x, double image_y,
                        double *normalized_x, double *normalized_y) const;

  /*!
      Distort an image using the current camera instrinsics

      The distorted image is computed in \a dst using samples from \a src.
      both buffers should be \a width x \a height x \a channels sized.

      \note This is the reference implementation using floating point images.
  */
  void Distort(const float* src, float* dst,
               int width, int height, double overscan, int channels);

  /*!
      Distort an image using the current camera instrinsics

      The distorted image is computed in \a dst using samples from \a src.
      both buffers should be \a width x \a height x \a channels sized.

      \note This version is much faster.
  */
  void Distort(const unsigned char* src, unsigned char* dst,
               int width, int height, double overscan, int channels);

  /*!
      Undistort an image using the current camera instrinsics

      The undistorted image is computed in \a dst using samples from \a src.
      both buffers should be \a width x \a height x \a channels sized.

      \note This is the reference implementation using floating point images.
  */
  void Undistort(const float* src, float* dst,
                 int width, int height, double overscan, int channels);

  /*!
      Undistort an image using the current camera instrinsics

      The undistorted image is computed in \a dst using samples from \a src.
      both buffers should be \a width x \a height x \a channels sized.

      \note This version is much faster.
  */
  void Undistort(const unsigned char* src, unsigned char* dst,
                 int width, int height, double overscan, int channels);

 private:
  template<typename WarpFunction> void ComputeLookupGrid(struct Grid* grid,
                                                         int width,
                                                         int height,
                                                         double overscan);
  void CheckUndistortLookupGrid(int width, int height, double overscan);
  void CheckDistortLookupGrid(int width, int height, double overscan);
  void FreeLookupGrid();

  // The traditional intrinsics matrix from x = K[R|t]X.
  Mat3 K_;

  // This is the size of the image. This is necessary to, for example, handle
  // the case of processing a scaled image.
  int image_width_;
  int image_height_;

  // OpenCV's distortion model with third order polynomial radial distortion
  // terms and second order tangential distortion. The distortion is applied to
  // the normalized coordinates before the focal length, which makes them
  // independent of image size.
  double k1_, k2_, k3_, p1_, p2_;

  struct Grid *distort_;
  struct Grid *undistort_;

  int threads_;
};

/// A human-readable representation of the camera intrinsic parameters.
std::ostream& operator <<(std::ostream &os,
                          const CameraIntrinsics &intrinsics);

// Apply camera intrinsics to the normalized point to get image coordinates.
// This applies the radial lens distortion to a point which is in normalized
// camera coordinates (i.e. the principal point is at (0, 0)) to get image
// coordinates in pixels. Templated for use with autodifferentiation.
template <typename T>
inline void ApplyRadialDistortionCameraIntrinsics(const T &focal_length_x,
                                                  const T &focal_length_y,
                                                  const T &principal_point_x,
                                                  const T &principal_point_y,
                                                  const T &k1,
                                                  const T &k2,
                                                  const T &k3,
                                                  const T &p1,
                                                  const T &p2,
                                                  const T &normalized_x,
                                                  const T &normalized_y,
                                                  T *image_x,
                                                  T *image_y) {
  T x = normalized_x;
  T y = normalized_y;

  // Apply distortion to the normalized points to get (xd, yd).
  T r2 = x*x + y*y;
  T r4 = r2 * r2;
  T r6 = r4 * r2;
  T r_coeff = (T(1) + k1*r2 + k2*r4 + k3*r6);
  T xd = x * r_coeff + T(2)*p1*x*y + p2*(r2 + T(2)*x*x);
  T yd = y * r_coeff + T(2)*p2*x*y + p1*(r2 + T(2)*y*y);

  // Apply focal length and principal point to get the final image coordinates.
  *image_x = focal_length_x * xd + principal_point_x;
  *image_y = focal_length_y * yd + principal_point_y;
}

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_CAMERA_INTRINSICS_H_
