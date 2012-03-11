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
#include "libmv/simple_pipeline/pipeline.h"
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
namespace {

// These are "strategy" classes which make it possible to use the same code for
// both projective and euclidean reconstruction.
// FIXME(MatthiasF): OOP would achieve the same goal while avoiding
// template bloat and making interface changes much easier.
struct EuclideanPipelineRoutines {
  typedef EuclideanReconstruction Reconstruction;
  typedef EuclideanCamera Camera;
  typedef EuclideanPoint Point;

  static void Bundle(const Tracks &tracks,
                     EuclideanReconstruction *reconstruction) {
    EuclideanBundle(tracks, reconstruction);
  }

  static bool Resect(const vector<Marker> &markers,
                     EuclideanReconstruction *reconstruction, bool final_pass) {
    return EuclideanResect(markers, reconstruction, final_pass);
  }

  static bool Intersect(const vector<Marker> &markers,
                        EuclideanReconstruction *reconstruction) {
    return EuclideanIntersect(markers, reconstruction);
  }

  static Marker ProjectMarker(const EuclideanPoint &point,
                              const EuclideanCamera &camera,
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
};

struct ProjectivePipelineRoutines {
  typedef ProjectiveReconstruction Reconstruction;
  typedef ProjectiveCamera Camera;
  typedef ProjectivePoint Point;

  static void Bundle(const Tracks &tracks,
                     ProjectiveReconstruction *reconstruction) {
    ProjectiveBundle(tracks, reconstruction);
  }

  static bool Resect(const vector<Marker> &markers,
                     ProjectiveReconstruction *reconstruction, bool final_pass) {
    return ProjectiveResect(markers, reconstruction);
  }

  static bool Intersect(const vector<Marker> &markers,
                        ProjectiveReconstruction *reconstruction) {
    return ProjectiveIntersect(markers, reconstruction);
  }

  static Marker ProjectMarker(const ProjectivePoint &point,
                              const ProjectiveCamera &camera,
                              const CameraIntrinsics &intrinsics) {
    Vec3 projected = camera.P * point.X;
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
};

}  // namespace

static void CompleteReconstructionLogProress(ProgressUpdateCallback *update_callback,
    double progress,
    const char *step = NULL)
{
  if(update_callback) {
    char message[256];

    if(step)
      snprintf(message, sizeof(message), "Completing solution %d%% | %s", (int)(progress*100), step);
    else
      snprintf(message, sizeof(message), "Completing solution %d%%", (int)(progress*100));

    update_callback->invoke(progress, message);
  }
}

template<typename PipelineRoutines>
void InternalCompleteReconstruction(
    const Tracks &tracks,
    typename PipelineRoutines::Reconstruction *reconstruction,
    ProgressUpdateCallback *update_callback = NULL) {
  int max_track = tracks.MaxTrack();
  int max_image = tracks.MaxImage();
  int num_resects = -1;
  int num_intersects = -1;
  int tot_resects = 0;
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
        CompleteReconstructionLogProress(update_callback,
                                         (double)tot_resects/(max_image));
        PipelineRoutines::Intersect(reconstructed_markers, reconstruction);
        num_intersects++;
        LG << "Ran Intersect() for track " << track;
      }
    }
    if (num_intersects) {
      CompleteReconstructionLogProress(update_callback,
                                       (double)tot_resects/(max_image),
                                       "Bundling...");
      PipelineRoutines::Bundle(tracks, reconstruction);
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
        CompleteReconstructionLogProress(update_callback,
                                         (double)tot_resects/(max_image));
        if (PipelineRoutines::Resect(reconstructed_markers, reconstruction, false)) {
          num_resects++;
          tot_resects++;
          LG << "Ran Resect() for image " << image;
        } else {
          LG << "Failed Resect() for image " << image;
        }
      }
    }
    if (num_resects) {
      CompleteReconstructionLogProress(update_callback,
                                       (double)tot_resects/(max_image),
                                       "Bundling...");
      PipelineRoutines::Bundle(tracks, reconstruction);
    }
    LG << "Did " << num_resects << " resects.";
  }

  // One last pass...
  num_resects = 0;
  for (int image = 0; image <= max_image; ++image) {
    if (reconstruction->CameraForImage(image)) {
      LG << "Skipping frame: " << image;
      continue;
    }
    vector<Marker> all_markers = tracks.MarkersInImage(image);

    vector<Marker> reconstructed_markers;
    for (int i = 0; i < all_markers.size(); ++i) {
      if (reconstruction->PointForTrack(all_markers[i].track)) {
        reconstructed_markers.push_back(all_markers[i]);
      }
    }
    if (reconstructed_markers.size() >= 5) {
      CompleteReconstructionLogProress(update_callback,
                                       (double)tot_resects/(max_image));
      if (PipelineRoutines::Resect(reconstructed_markers, reconstruction, true)) {
        num_resects++;
        LG << "Ran Resect() for image " << image;
      } else {
        LG << "Failed Resect() for image " << image;
      }
    }
  }
  if (num_resects) {
    CompleteReconstructionLogProress(update_callback,
                                     (double)tot_resects/(max_image),
                                     "Bundling...");
    PipelineRoutines::Bundle(tracks, reconstruction);
  }
}

template<typename PipelineRoutines>
double InternalReprojectionError(const Tracks &image_tracks,
                                 const typename PipelineRoutines::Reconstruction &reconstruction,
                                 const CameraIntrinsics &intrinsics) {
  int num_skipped = 0;
  int num_reprojected = 0;
  double total_error = 0.0;
  vector<Marker> markers = image_tracks.AllMarkers();
  for (int i = 0; i < markers.size(); ++i) {
    const typename PipelineRoutines::Camera *camera =
        reconstruction.CameraForImage(markers[i].image);
    const typename PipelineRoutines::Point *point =
        reconstruction.PointForTrack(markers[i].track);
    if (!camera || !point) {
      num_skipped++;
      continue;
    }
    num_reprojected++;

    Marker reprojected_marker =
        PipelineRoutines::ProjectMarker(*point, *camera, intrinsics);
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
    LG << line;

    total_error += sqrt(ex*ex + ey*ey);
  }
  LG << "Skipped " << num_skipped << " markers.";
  LG << "Reprojected " << num_reprojected << " markers.";
  LG << "Total error: " << total_error;
  LG << "Average error: " << (total_error / num_reprojected) << " [pixels].";
  return total_error / num_reprojected;
}

double EuclideanReprojectionError(const Tracks &image_tracks,
                                  const EuclideanReconstruction &reconstruction,
                                  const CameraIntrinsics &intrinsics) {
  return InternalReprojectionError<EuclideanPipelineRoutines>(image_tracks,
                                                              reconstruction,
                                                              intrinsics);
}

double ProjectiveReprojectionError(
    const Tracks &image_tracks,
    const ProjectiveReconstruction &reconstruction,
    const CameraIntrinsics &intrinsics) {
  return InternalReprojectionError<ProjectivePipelineRoutines>(image_tracks,
                                                               reconstruction,
                                                               intrinsics);
}

void EuclideanCompleteReconstruction(const Tracks &tracks,
                                     EuclideanReconstruction *reconstruction,
                                     ProgressUpdateCallback *update_callback) {
  InternalCompleteReconstruction<EuclideanPipelineRoutines>(tracks,
                                                            reconstruction,
                                                            update_callback);
}

void ProjectiveCompleteReconstruction(const Tracks &tracks,
                                      ProjectiveReconstruction *reconstruction) {
  InternalCompleteReconstruction<ProjectivePipelineRoutines>(tracks,
                                                             reconstruction);
}

void InvertIntrinsicsForTracks(const Tracks &raw_tracks,
                               const CameraIntrinsics &camera_intrinsics,
                               Tracks *calibrated_tracks) {
  vector<Marker> markers = raw_tracks.AllMarkers();
  for (int i = 0; i < markers.size(); ++i) {
    camera_intrinsics.InvertIntrinsics(markers[i].x,
                                       markers[i].y,
                                       &(markers[i].x),
                                       &(markers[i].y));
  }
  *calibrated_tracks = Tracks(markers);
}

}  // namespace libmv
