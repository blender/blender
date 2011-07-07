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

#include <cstdio>

#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/intersect.h"
#include "libmv/simple_pipeline/resect.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/camera_intrinsics.h"

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

namespace libmv {

void CompleteReconstruction(const Tracks &tracks,
                            Reconstruction *reconstruction) {
  int max_track = tracks.MaxTrack();
  int max_image = tracks.MaxImage();
  int num_resects = -1;
  int num_intersects = -1;
  LG << "Max track: " << max_track;
  LG << "Max image: " << max_image;
  LG << "Number of markers: " << tracks.NumMarkers();
  while (num_resects != 0 || num_intersects != 0) {
    // Do all possible intersections.
    num_intersects = 0;
    for (int track = 0; track <= max_track; ++track) {
      if (reconstruction->PointForTrack(track)) {
        LG << "Skipping point: " << track;
        continue;
      }
      vector<Marker> all_markers = tracks.MarkersForTrack(track);
      LG << "Got " << all_markers.size() << " markers for track " << track;

      vector<Marker> reconstructed_markers;
      for (int i = 0; i < all_markers.size(); ++i) {
        if (reconstruction->CameraForImage(all_markers[i].image)) {
          reconstructed_markers.push_back(all_markers[i]);
        }
      }
      LG << "Got " << reconstructed_markers.size()
         << " reconstructed markers for track " << track;
      if (reconstructed_markers.size() >= 2) {
        Intersect(reconstructed_markers, reconstruction);
        num_intersects++;
        LG << "Ran Intersect() for track " << track;
      }
    }
    if (num_intersects) {
      Bundle(tracks, reconstruction);
      LG << "Ran Bundle() after intersections.";
    }
    LG << "Did " << num_intersects << " intersects.";

    // Do all possible resections.
    num_resects = 0;
    for (int image = 0; image <= max_image; ++image) {
      if (reconstruction->CameraForImage(image)) {
        LG << "Skipping frame: " << image;
        continue;
      }
      vector<Marker> all_markers = tracks.MarkersInImage(image);
      LG << "Got " << all_markers.size() << " markers for image " << image;

      vector<Marker> reconstructed_markers;
      for (int i = 0; i < all_markers.size(); ++i) {
        if (reconstruction->PointForTrack(all_markers[i].track)) {
          reconstructed_markers.push_back(all_markers[i]);
        }
      }
      LG << "Got " << reconstructed_markers.size()
         << " reconstructed markers for image " << image;
      if (reconstructed_markers.size() >= 5) {
        if (Resect(reconstructed_markers, reconstruction)) {
          num_resects++;
          LG << "Ran Resect() for image " << image;
        } else {
          LG << "Failed Resect() for image " << image;
        }
      }
    }
    if (num_resects) {
      Bundle(tracks, reconstruction);
    }
    LG << "Did " << num_resects << " resects.";
  }
}

Marker ProjectMarker(const Point &point,
                     const Camera &camera,
                     const CameraIntrinsics &intrinsics) {
  Vec3 projected = camera.R * point.X + camera.t;
  projected /= projected(2);

  Marker reprojected_marker;
  intrinsics.ApplyIntrinsics(projected(0),
                             projected(1),
                             &reprojected_marker.x,
                             &reprojected_marker.y);

  reprojected_marker.image = camera.image;
  reprojected_marker.track = point.track;
  return reprojected_marker;
}

double ReprojectionError(const Tracks &image_tracks,
                         const Reconstruction &reconstruction,
                         const CameraIntrinsics &intrinsics) {
  int num_skipped = 0;
  int num_reprojected = 0;
  double total_error = 0.0;
  vector<Marker> markers = image_tracks.AllMarkers();
  for (int i = 0; i < markers.size(); ++i) {
    const Camera *camera = reconstruction.CameraForImage(markers[i].image);
    const Point *point = reconstruction.PointForTrack(markers[i].track);
    if (!camera || !point) {
      num_skipped++;
      continue;
    }
    num_reprojected++;

    Marker reprojected_marker = ProjectMarker(*point, *camera, intrinsics);
    double ex = reprojected_marker.x - markers[i].x;
    double ey = reprojected_marker.y - markers[i].y;

    const int N = 100;
    char line[N];
    snprintf(line, N,
           "image %-3d track %-3d "
           "x %7.1f y %7.1f "
           "rx %7.1f ry %7.1f "
           "ex %7.1f ey %7.1f"
           "    e %7.1f",
           markers[i].image,
           markers[i].track,
           markers[i].x,
           markers[i].y,
           reprojected_marker.x,
           reprojected_marker.y,
           ex,
           ey,
           sqrt(ex*ex + ey*ey));
    //LG << line;
    total_error += sqrt(ex*ex + ey*ey);
  }
  LG << "Skipped " << num_skipped << " markers.";
  LG << "Reprojected " << num_reprojected << " markers.";
  LG << "Total error: " << total_error;
  LG << "Average error: " << (total_error / num_reprojected) << " [pixels].";
  return total_error / num_reprojected;
}

}  // namespace libmv
