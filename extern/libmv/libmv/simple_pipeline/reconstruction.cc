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

#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/numeric/numeric.h"
#include "libmv/logging/logging.h"

namespace libmv {

EuclideanReconstruction::EuclideanReconstruction() {}
EuclideanReconstruction::EuclideanReconstruction(
    const EuclideanReconstruction &other) {
  cameras_ = other.cameras_;
  points_ = other.points_;
}

EuclideanReconstruction &EuclideanReconstruction::operator=(
    const EuclideanReconstruction &other) {
  if (&other != this) {
    cameras_ = other.cameras_;
    points_ = other.points_;
  }
  return *this;
}

void EuclideanReconstruction::InsertCamera(int image,
                                           const Mat3 &R,
                                           const Vec3 &t) {
  LG << "InsertCamera " << image << ":\nR:\n"<< R << "\nt:\n" << t;
  if (image >= cameras_.size()) {
    cameras_.resize(image + 1);
  }
  cameras_[image].image = image;
  cameras_[image].R = R;
  cameras_[image].t = t;
}

void EuclideanReconstruction::InsertPoint(int track, const Vec3 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

EuclideanCamera *EuclideanReconstruction::CameraForImage(int image) {
  return const_cast<EuclideanCamera *>(
      static_cast<const EuclideanReconstruction *>(
          this)->CameraForImage(image));
}

const EuclideanCamera *EuclideanReconstruction::CameraForImage(
    int image) const {
  if (image < 0 || image >= cameras_.size()) {
    return NULL;
  }
  const EuclideanCamera *camera = &cameras_[image];
  if (camera->image == -1) {
    return NULL;
  }
  return camera;
}

vector<EuclideanCamera> EuclideanReconstruction::AllCameras() const {
  vector<EuclideanCamera> cameras;
  for (int i = 0; i < cameras_.size(); ++i) {
    if (cameras_[i].image != -1) {
      cameras.push_back(cameras_[i]);
    }
  }
  return cameras;
}

EuclideanPoint *EuclideanReconstruction::PointForTrack(int track) {
  return const_cast<EuclideanPoint *>(
      static_cast<const EuclideanReconstruction *>(this)->PointForTrack(track));
}

const EuclideanPoint *EuclideanReconstruction::PointForTrack(int track) const {
  if (track < 0 || track >= points_.size()) {
    return NULL;
  }
  const EuclideanPoint *point = &points_[track];
  if (point->track == -1) {
    return NULL;
  }
  return point;
}

vector<EuclideanPoint> EuclideanReconstruction::AllPoints() const {
  vector<EuclideanPoint> points;
  for (int i = 0; i < points_.size(); ++i) {
    if (points_[i].track != -1) {
      points.push_back(points_[i]);
    }
  }
  return points;
}

void ProjectiveReconstruction::InsertCamera(int image,
                                           const Mat34 &P) {
  LG << "InsertCamera " << image << ":\nP:\n"<< P;
  if (image >= cameras_.size()) {
    cameras_.resize(image + 1);
  }
  cameras_[image].image = image;
  cameras_[image].P = P;
}

void ProjectiveReconstruction::InsertPoint(int track, const Vec4 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

ProjectiveCamera *ProjectiveReconstruction::CameraForImage(int image) {
  return const_cast<ProjectiveCamera *>(
      static_cast<const ProjectiveReconstruction *>(
          this)->CameraForImage(image));
}

const ProjectiveCamera *ProjectiveReconstruction::CameraForImage(
    int image) const {
  if (image < 0 || image >= cameras_.size()) {
    return NULL;
  }
  const ProjectiveCamera *camera = &cameras_[image];
  if (camera->image == -1) {
    return NULL;
  }
  return camera;
}

vector<ProjectiveCamera> ProjectiveReconstruction::AllCameras() const {
  vector<ProjectiveCamera> cameras;
  for (int i = 0; i < cameras_.size(); ++i) {
    if (cameras_[i].image != -1) {
      cameras.push_back(cameras_[i]);
    }
  }
  return cameras;
}

ProjectivePoint *ProjectiveReconstruction::PointForTrack(int track) {
  return const_cast<ProjectivePoint *>(
      static_cast<const ProjectiveReconstruction *>(this)->PointForTrack(track));
}

const ProjectivePoint *ProjectiveReconstruction::PointForTrack(int track) const {
  if (track < 0 || track >= points_.size()) {
    return NULL;
  }
  const ProjectivePoint *point = &points_[track];
  if (point->track == -1) {
    return NULL;
  }
  return point;
}

vector<ProjectivePoint> ProjectiveReconstruction::AllPoints() const {
  vector<ProjectivePoint> points;
  for (int i = 0; i < points_.size(); ++i) {
    if (points_[i].track != -1) {
      points.push_back(points_[i]);
    }
  }
  return points;
}

}  // namespace libmv
