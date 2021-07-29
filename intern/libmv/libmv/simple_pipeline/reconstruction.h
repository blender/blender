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
    A EuclideanCamera is the location and rotation of the camera viewing \a image.

    \a image identify which image from \l Tracks this camera represents.
    \a R is a 3x3 matrix representing the rotation of the camera.
    \a t is a translation vector representing its positions.

    \sa Reconstruction
*/
struct EuclideanCamera {
  EuclideanCamera() : image(-1) {}
  EuclideanCamera(const EuclideanCamera &c) : image(c.image), R(c.R), t(c.t) {}

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
struct EuclideanPoint {
  EuclideanPoint() : track(-1) {}
  EuclideanPoint(const EuclideanPoint &p) : track(p.track), X(p.X) {}
  int track;
  Vec3 X;
};

/*!
    The EuclideanReconstruction class stores \link EuclideanCamera cameras
    \endlink and \link EuclideanPoint points \endlink.

    The EuclideanReconstruction container is intended as the store of 3D
    reconstruction data to be used with the MultiView API.

    The container has lookups to query a \a EuclideanCamera from an \a image or
    a \a EuclideanPoint from a \a track.

    \sa Camera, Point
*/
class EuclideanReconstruction {
 public:
  // Default constructor starts with no cameras.
  EuclideanReconstruction();

  /// Copy constructor.
  EuclideanReconstruction(const EuclideanReconstruction &other);

  EuclideanReconstruction &operator=(const EuclideanReconstruction &other);

  /*!
      Insert a camera into the set. If there is already a camera for the given
      \a image, the existing camera is replaced. If there is no camera for the
      given \a image, a new one is added.

      \a image is the key used to retrieve the cameras with the other methods
      in this class.

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
  EuclideanCamera *CameraForImage(int image);
  const EuclideanCamera *CameraForImage(int image) const;

  /// Returns all cameras.
  vector<EuclideanCamera> AllCameras() const;

  /// Returns a pointer to the point corresponding to \a track.
  EuclideanPoint *PointForTrack(int track);
  const EuclideanPoint *PointForTrack(int track) const;

  /// Returns all points.
  vector<EuclideanPoint> AllPoints() const;

 private:
  vector<EuclideanCamera> cameras_;
  vector<EuclideanPoint> points_;
};

/*!
    A ProjectiveCamera is the projection matrix for the camera of \a image.

    \a image identify which image from \l Tracks this camera represents.
    \a P is the 3x4 projection matrix.

    \sa ProjectiveReconstruction
*/
struct ProjectiveCamera {
  ProjectiveCamera() : image(-1) {}
  ProjectiveCamera(const ProjectiveCamera &c) : image(c.image), P(c.P) {}

  int image;
  Mat34 P;
};

/*!
    A Point is the 3D location of a track.

    \a track identifies which track from \l Tracks this point corresponds to.
    \a X is the homogeneous 3D position of the track.

    \sa Reconstruction
*/
struct ProjectivePoint {
  ProjectivePoint() : track(-1) {}
  ProjectivePoint(const ProjectivePoint &p) : track(p.track), X(p.X) {}
  int track;
  Vec4 X;
};

/*!
    The ProjectiveReconstruction class stores \link ProjectiveCamera cameras
    \endlink and \link ProjectivePoint points \endlink.

    The ProjectiveReconstruction container is intended as the store of 3D
    reconstruction data to be used with the MultiView API.

    The container has lookups to query a \a ProjectiveCamera from an \a image or
    a \a ProjectivePoint from a \a track.

    \sa Camera, Point
*/
class ProjectiveReconstruction {
 public:
  /*!
      Insert a camera into the set. If there is already a camera for the given
      \a image, the existing camera is replaced. If there is no camera for the
      given \a image, a new one is added.

      \a image is the key used to retrieve the cameras with the other methods
      in this class.

      \note You should use the same \a image identifier as in \l Tracks.
  */
  void InsertCamera(int image, const Mat34 &P);

  /*!
      Insert a point into the reconstruction. If there is already a point for
      the given \a track, the existing point is replaced. If there is no point
      for the given \a track, a new one is added.

      \a track is the key used to retrieve the points with the
      other methods in this class.

      \note You should use the same \a track identifier as in \l Tracks.
  */
  void InsertPoint(int track, const Vec4 &X);

  /// Returns a pointer to the camera corresponding to \a image.
  ProjectiveCamera *CameraForImage(int image);
  const ProjectiveCamera *CameraForImage(int image) const;

  /// Returns all cameras.
  vector<ProjectiveCamera> AllCameras() const;

  /// Returns a pointer to the point corresponding to \a track.
  ProjectivePoint *PointForTrack(int track);
  const ProjectivePoint *PointForTrack(int track) const;

  /// Returns all points.
  vector<ProjectivePoint> AllPoints() const;

 private:
  vector<ProjectiveCamera> cameras_;
  vector<ProjectivePoint> points_;
};

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_RECONSTRUCTION_H_
