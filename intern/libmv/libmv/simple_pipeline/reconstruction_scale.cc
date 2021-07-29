// Copyright (c) 2013 libmv authors.
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

#include "libmv/simple_pipeline/reconstruction_scale.h"
#include "libmv/logging/logging.h"

namespace libmv {

void EuclideanScaleToUnity(EuclideanReconstruction *reconstruction) {
  vector<EuclideanCamera> all_cameras = reconstruction->AllCameras();
  vector<EuclideanPoint> all_points = reconstruction->AllPoints();

  // Calculate center of the mass of all cameras.
  Vec3 cameras_mass_center = Vec3::Zero();
  for (int i = 0; i < all_cameras.size(); ++i) {
    cameras_mass_center += all_cameras[i].t;
  }
  cameras_mass_center /= all_cameras.size();

  // Find the most distant camera from the mass center.
  double max_distance = 0.0;
  for (int i = 0; i < all_cameras.size(); ++i) {
    double distance = (all_cameras[i].t - cameras_mass_center).squaredNorm();
    if (distance > max_distance) {
      max_distance = distance;
    }
  }

  if (max_distance == 0.0) {
    LG << "Cameras position variance is too small, can not rescale";
    return;
  }

  double scale_factor = 1.0 / sqrt(max_distance);

  // Rescale cameras positions.
  for (int i = 0; i < all_cameras.size(); ++i) {
    int image = all_cameras[i].image;
    EuclideanCamera *camera = reconstruction->CameraForImage(image);
    camera->t = camera->t * scale_factor;
  }

  // Rescale points positions.
  for (int i = 0; i < all_points.size(); ++i) {
    int track = all_points[i].track;
    EuclideanPoint *point = reconstruction->PointForTrack(track);
    point->X = point->X * scale_factor;
  }
}

}  // namespace libmv
