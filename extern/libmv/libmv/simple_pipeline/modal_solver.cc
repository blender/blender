// Copyright (c) 2012 libmv authors.
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

#include <cstdio>

#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/modal_solver.h"
#include "libmv/simple_pipeline/rigid_registration.h"

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

namespace libmv {

static void ProjectMarkerOnSphere(Marker &marker, Vec3 &X) {
  X(0) = marker.x;
  X(1) = marker.y;
  X(2) = 1.0;

  X *= 5.0 / X.norm();
}

static void ModalSolverLogProress(ProgressUpdateCallback *update_callback,
    double progress)
{
  if (update_callback) {
    char message[256];

    snprintf(message, sizeof(message), "Solving progress %d%%", (int)(progress * 100));

    update_callback->invoke(progress, message);
  }
}

void ModalSolver(Tracks &tracks,
                 EuclideanReconstruction *reconstruction,
                 ProgressUpdateCallback *update_callback) {
  int max_image = tracks.MaxImage();
  int max_track = tracks.MaxTrack();

  LG << "Max image: " << max_image;
  LG << "Max track: " << max_track;

  Mat3 R = Mat3::Identity();

  for (int image = 0; image <= max_image; ++image) {
    vector<Marker> all_markers = tracks.MarkersInImage(image);

    ModalSolverLogProress(update_callback, (float) image / max_image);

    // Skip empty frames without doing anything
    if (all_markers.size() == 0) {
      LG << "Skipping frame: " << image;
      continue;
    }

    vector<Vec3> points, reference_points;

    // Cnstruct pairs of markers from current and previous image,
    // to reproject them and find rigid transformation between
    // previous and current image
    for (int track = 0; track <= max_track; ++track) {
      EuclideanPoint *point = reconstruction->PointForTrack(track);

      if (point) {
        Marker marker = tracks.MarkerInImageForTrack(image, track);

        if (marker.image == image) {
          Vec3 X;

          LG << "Use track " << track << " for rigid registration between image " <<
            image - 1 << " and " << image;

          ProjectMarkerOnSphere(marker, X);

          points.push_back(point->X);
          reference_points.push_back(X);
        }
      }
    }

    if (points.size()) {
      // Find rigid delta transformation to current image
      RigidRegistration(reference_points, points, R);
    }

    reconstruction->InsertCamera(image, R, Vec3::Zero());

    // Review if there's new tracks for which position might be reconstructed
    for (int track = 0; track <= max_track; ++track) {
      if (!reconstruction->PointForTrack(track)) {
        Marker marker = tracks.MarkerInImageForTrack(image, track);

        if (marker.image == image) {
          // New track appeared on this image, project it's position onto sphere

          LG << "Projecting track " << track << " at image " << image;

          Vec3 X;
          ProjectMarkerOnSphere(marker, X);
          reconstruction->InsertPoint(track, R.inverse() * X);
        }
      }
    }
  }
}

}  // namespace libmv
