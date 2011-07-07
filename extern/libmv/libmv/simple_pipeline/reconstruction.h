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

#ifndef LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_
#define LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_

#include "libmv/base/vector.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

/*!
    A Camera is the location and rotation of the camera viewing \a image.

    \a image identify which image from \l Tracks this camera represents.
    \a R is a 3x3 matrix representing the rotation of the camera.
    \a t is a translation vector representing its positions.

    \sa Reconstruction
*/
struct Camera {
  Camera() : image(-1) {}
  Camera(const Camera &c) : image(c.image), R(c.R), t(c.t) {}

  int image;
  Mat3 R;
  Vec3 t;
};

/*!
    A Point is the 3D location of a track.

    \a track identify which track from \l Tracks this point corresponds to.
    \a X represents the 3D position of the track.

    \sa Reconstruction
*/
struct Point {
  Point() : track(-1) {}
  Point(const Point &p) : track(p.track), X(p.X) {}
  int track;
  Vec3 X;
};

/*!
    The Reconstruction class stores \link Camera cameras \endlink and \link Point points \endlink.

    The Reconstruction container is intended as the store of 3D reconstruction data
    to be used with the MultiView API.

    The container has lookups to query a \a Camera from an \a image or a \a Point from a \a track.

    \sa Camera, Point
*/
class Reconstruction {
 public:
  /*!
      Insert a camera into the set. If there is already a camera for the given
      \a image, the existing camera is replaced. If there is no
      camera for the given \a image, a new one is added.

      \a image is the key used to retrieve the cameras with the
      other methods in this class.

      \note You should use the same \a image identifier as in \l Tracks.
  */
  void InsertCamera(int image, const Mat3 &R, const Vec3 &t);

  /*!
      Insert a point into the reconstruction. If there is already a point for
      the given \a track, the existing point is replaced. If there is no point
      for the given \a track, a new one is added.

      \a track is the key used to retrieve the points with the
      other methods in this class.

      \note You should use the same \a track identifier as in \l Tracks.
  */
  void InsertPoint(int track, const Vec3 &X);

  /// Returns a pointer to the camera corresponding to \a image.
  Camera *CameraForImage(int image);
  const Camera *CameraForImage(int image) const;

  /// Returns all cameras.
  vector<Camera> AllCameras() const;

  /// Returns a pointer to the point corresponding to \a track.
  Point *PointForTrack(int track);
  const Point *PointForTrack(int track) const;

  /// Returns all points.
  vector<Point> AllPoints() const;

 private:
  vector<Camera> cameras_;
  vector<Point> points_;
};

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_
