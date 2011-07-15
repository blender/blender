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

#include "libmv/numeric/numeric.h"

namespace libmv {

class CameraIntrinsics {
 public:
  CameraIntrinsics();

  const Mat3 &K()                 const { return K_;            }
  // FIXME(MatthiasF): these should be CamelCase methods
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

  /// Set both x and y focal length in pixels.
  void SetFocalLength(double focal_x, double focal_y) {
    K_(0, 0) = focal_x;
    K_(1, 1) = focal_y;
  }

  void SetPrincipalPoint(double cx, double cy) {
    K_(0, 2) = cx;
    K_(1, 2) = cy;
  }

  void SetImageSize(int width, int height) {
    image_width_ = width;
    image_height_ = height;
  }

  void SetRadialDistortion(double k1, double k2, double k3 = 0) {
    k1_ = k1;
    k2_ = k2;
    k3_ = k3;
  }

  /*!
      Apply camera intrinsics to the normalized point to get image coordinates.

      This applies the camera intrinsics to a point which is in normalized
      camera coordinates (i.e. the principal point is at (0, 0)) to get image
      coordinates in pixels.
  */
  void ApplyIntrinsics(double normalized_x, double normalized_y,
                       double *image_x, double *image_y) const;

  /*!
      Invert camera intrinsics on the image point to get normalized coordinates.

      This reverses the effect of camera intrinsics on a point which is in image
      coordinates to get normalized camera coordinates.
  */
  void InvertIntrinsics(double image_x, double image_y,
                        double *normalized_x, double *normalized_y) const;

 private:
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
};

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_CAMERA_INTRINSICS_H_
