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

void Reconstruction::InsertCamera(int image, const Mat3 &R, const Vec3 &t) {
  LG << "InsertCamera " << image << ":\nR:\n"<< R << "\nt:\n" << t;
  if (image >= cameras_.size()) {
    cameras_.resize(image + 1);
  }
  cameras_[image].image = image;
  cameras_[image].R = R;
  cameras_[image].t = t;
}

void Reconstruction::InsertPoint(int track, const Vec3 &X) {
  LG << "InsertPoint " << track << ":\n" << X;
  if (track >= points_.size()) {
    points_.resize(track + 1);
  }
  points_[track].track = track;
  points_[track].X = X;
}

Camera *Reconstruction::CameraForImage(int image) {
  return const_cast<Camera *>(
      static_cast<const Reconstruction *>(this)->CameraForImage(image));
}

const Camera *Reconstruction::CameraForImage(int image) const {
  if (image < 0 || image >= cameras_.size()) {
    return NULL;
  }
  const Camera *camera = &cameras_[image];
  if (camera->image == -1) {
    return NULL;
  }
  return camera;
}

vector<Camera> Reconstruction::AllCameras() const {
  vector<Camera> cameras;
  for (int i = 0; i < cameras_.size(); ++i) {
    if (cameras_[i].image != -1) {
      cameras.push_back(cameras_[i]);
    }
  }
  return cameras;
}

Point *Reconstruction::PointForTrack(int track) {
  return const_cast<Point *>(
      static_cast<const Reconstruction *>(this)->PointForTrack(track));
}

const Point *Reconstruction::PointForTrack(int track) const {
  if (track < 0 || track >= points_.size()) {
    return NULL;
  }
  const Point *point = &points_[track];
  if (point->track == -1) {
    return NULL;
  }
  return point;
}

vector<Point> Reconstruction::AllPoints() const {
  vector<Point> points;
  for (int i = 0; i < points_.size(); ++i) {
    if (points_[i].track != -1) {
      points.push_back(points_[i]);
    }
  }
  return points;
}

}  // namespace libmv
