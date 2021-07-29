/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "intern/reconstruction.h"
#include "intern/camera_intrinsics.h"
#include "intern/tracks.h"
#include "intern/utildefines.h"

#include "libmv/logging/logging.h"
#include "libmv/simple_pipeline/bundle.h"
#include "libmv/simple_pipeline/keyframe_selection.h"
#include "libmv/simple_pipeline/initialize_reconstruction.h"
#include "libmv/simple_pipeline/modal_solver.h"
#include "libmv/simple_pipeline/pipeline.h"
#include "libmv/simple_pipeline/reconstruction_scale.h"
#include "libmv/simple_pipeline/tracks.h"

using libmv::CameraIntrinsics;
using libmv::EuclideanCamera;
using libmv::EuclideanPoint;
using libmv::EuclideanReconstruction;
using libmv::EuclideanScaleToUnity;
using libmv::Marker;
using libmv::ProgressUpdateCallback;

using libmv::PolynomialCameraIntrinsics;
using libmv::Tracks;
using libmv::EuclideanBundle;
using libmv::EuclideanCompleteReconstruction;
using libmv::EuclideanReconstructTwoFrames;
using libmv::EuclideanReprojectionError;

struct libmv_Reconstruction {
  EuclideanReconstruction reconstruction;

  /* Used for per-track average error calculation after reconstruction */
  Tracks tracks;
  CameraIntrinsics *intrinsics;

  double error;
  bool is_valid;
};

namespace {

class ReconstructUpdateCallback : public ProgressUpdateCallback {
 public:
  ReconstructUpdateCallback(
      reconstruct_progress_update_cb progress_update_callback,
      void *callback_customdata) {
    progress_update_callback_ = progress_update_callback;
    callback_customdata_ = callback_customdata;
  }

  void invoke(double progress, const char* message) {
    if (progress_update_callback_) {
      progress_update_callback_(callback_customdata_, progress, message);
    }
  }
 protected:
  reconstruct_progress_update_cb progress_update_callback_;
  void* callback_customdata_;
};

void libmv_solveRefineIntrinsics(
    const Tracks &tracks,
    const int refine_intrinsics,
    const int bundle_constraints,
    reconstruct_progress_update_cb progress_update_callback,
    void* callback_customdata,
    EuclideanReconstruction* reconstruction,
    CameraIntrinsics* intrinsics) {
  /* only a few combinations are supported but trust the caller/ */
  int bundle_intrinsics = 0;

  if (refine_intrinsics & LIBMV_REFINE_FOCAL_LENGTH) {
    bundle_intrinsics |= libmv::BUNDLE_FOCAL_LENGTH;
  }
  if (refine_intrinsics & LIBMV_REFINE_PRINCIPAL_POINT) {
    bundle_intrinsics |= libmv::BUNDLE_PRINCIPAL_POINT;
  }
  if (refine_intrinsics & LIBMV_REFINE_RADIAL_DISTORTION_K1) {
    bundle_intrinsics |= libmv::BUNDLE_RADIAL_K1;
  }
  if (refine_intrinsics & LIBMV_REFINE_RADIAL_DISTORTION_K2) {
    bundle_intrinsics |= libmv::BUNDLE_RADIAL_K2;
  }

  progress_update_callback(callback_customdata, 1.0, "Refining solution");

  EuclideanBundleCommonIntrinsics(tracks,
                                  bundle_intrinsics,
                                  bundle_constraints,
                                  reconstruction,
                                  intrinsics);
}

void finishReconstruction(
    const Tracks &tracks,
    const CameraIntrinsics &camera_intrinsics,
    libmv_Reconstruction *libmv_reconstruction,
    reconstruct_progress_update_cb progress_update_callback,
    void *callback_customdata) {
  EuclideanReconstruction &reconstruction =
    libmv_reconstruction->reconstruction;

  /* Reprojection error calculation. */
  progress_update_callback(callback_customdata, 1.0, "Finishing solution");
  libmv_reconstruction->tracks = tracks;
  libmv_reconstruction->error = EuclideanReprojectionError(tracks,
                                                           reconstruction,
                                                           camera_intrinsics);
}

bool selectTwoKeyframesBasedOnGRICAndVariance(
    Tracks& tracks,
    Tracks& normalized_tracks,
    CameraIntrinsics& camera_intrinsics,
    int& keyframe1,
    int& keyframe2) {
  libmv::vector<int> keyframes;

  /* Get list of all keyframe candidates first. */
  SelectKeyframesBasedOnGRICAndVariance(normalized_tracks,
                                        camera_intrinsics,
                                        keyframes);

  if (keyframes.size() < 2) {
    LG << "Not enough keyframes detected by GRIC";
    return false;
  } else if (keyframes.size() == 2) {
    keyframe1 = keyframes[0];
    keyframe2 = keyframes[1];
    return true;
  }

  /* Now choose two keyframes with minimal reprojection error after initial
   * reconstruction choose keyframes with the least reprojection error after
   * solving from two candidate keyframes.
   *
   * In fact, currently libmv returns single pair only, so this code will
   * not actually run. But in the future this could change, so let's stay
   * prepared.
   */
  int previous_keyframe = keyframes[0];
  double best_error = std::numeric_limits<double>::max();
  for (int i = 1; i < keyframes.size(); i++) {
    EuclideanReconstruction reconstruction;
    int current_keyframe = keyframes[i];
    libmv::vector<Marker> keyframe_markers =
      normalized_tracks.MarkersForTracksInBothImages(previous_keyframe,
                                                     current_keyframe);

    Tracks keyframe_tracks(keyframe_markers);

    /* get a solution from two keyframes only */
    EuclideanReconstructTwoFrames(keyframe_markers, &reconstruction);
    EuclideanBundle(keyframe_tracks, &reconstruction);
    EuclideanCompleteReconstruction(keyframe_tracks,
                                    &reconstruction,
                                    NULL);

    double current_error = EuclideanReprojectionError(tracks,
                                                      reconstruction,
                                                      camera_intrinsics);

    LG << "Error between " << previous_keyframe
       << " and " << current_keyframe
       << ": " << current_error;

    if (current_error < best_error) {
      best_error = current_error;
      keyframe1 = previous_keyframe;
      keyframe2 = current_keyframe;
    }

    previous_keyframe = current_keyframe;
  }

  return true;
}

Marker libmv_projectMarker(const EuclideanPoint& point,
                           const EuclideanCamera& camera,
                           const CameraIntrinsics& intrinsics) {
  libmv::Vec3 projected = camera.R * point.X + camera.t;
  projected /= projected(2);

  libmv::Marker reprojected_marker;
  intrinsics.ApplyIntrinsics(projected(0), projected(1),
                             &reprojected_marker.x,
                             &reprojected_marker.y);

  reprojected_marker.image = camera.image;
  reprojected_marker.track = point.track;
  return reprojected_marker;
}

void libmv_getNormalizedTracks(const Tracks &tracks,
                               const CameraIntrinsics &camera_intrinsics,
                               Tracks *normalized_tracks) {
  libmv::vector<Marker> markers = tracks.AllMarkers();
  for (int i = 0; i < markers.size(); ++i) {
    Marker &marker = markers[i];
    camera_intrinsics.InvertIntrinsics(marker.x, marker.y,
                                       &marker.x, &marker.y);
    normalized_tracks->Insert(marker.image,
                              marker.track,
                              marker.x, marker.y,
                              marker.weight);
  }
}

}  // namespace

libmv_Reconstruction *libmv_solveReconstruction(
    const libmv_Tracks* libmv_tracks,
    const libmv_CameraIntrinsicsOptions* libmv_camera_intrinsics_options,
    libmv_ReconstructionOptions* libmv_reconstruction_options,
    reconstruct_progress_update_cb progress_update_callback,
    void* callback_customdata) {
  libmv_Reconstruction *libmv_reconstruction =
    LIBMV_OBJECT_NEW(libmv_Reconstruction);

  Tracks &tracks = *((Tracks *) libmv_tracks);
  EuclideanReconstruction &reconstruction =
    libmv_reconstruction->reconstruction;

  ReconstructUpdateCallback update_callback =
    ReconstructUpdateCallback(progress_update_callback,
                              callback_customdata);

  /* Retrieve reconstruction options from C-API to libmv API. */
  CameraIntrinsics *camera_intrinsics;
  camera_intrinsics = libmv_reconstruction->intrinsics =
    libmv_cameraIntrinsicsCreateFromOptions(libmv_camera_intrinsics_options);

  /* Invert the camera intrinsics/ */
  Tracks normalized_tracks;
  libmv_getNormalizedTracks(tracks, *camera_intrinsics, &normalized_tracks);

  /* keyframe selection. */
  int keyframe1 = libmv_reconstruction_options->keyframe1,
      keyframe2 = libmv_reconstruction_options->keyframe2;

  if (libmv_reconstruction_options->select_keyframes) {
    LG << "Using automatic keyframe selection";

    update_callback.invoke(0, "Selecting keyframes");

    selectTwoKeyframesBasedOnGRICAndVariance(tracks,
                                             normalized_tracks,
                                             *camera_intrinsics,
                                             keyframe1,
                                             keyframe2);

    /* so keyframes in the interface would be updated */
    libmv_reconstruction_options->keyframe1 = keyframe1;
    libmv_reconstruction_options->keyframe2 = keyframe2;
  }

  /* Actual reconstruction. */
  LG << "frames to init from: " << keyframe1 << " " << keyframe2;

  libmv::vector<Marker> keyframe_markers =
    normalized_tracks.MarkersForTracksInBothImages(keyframe1, keyframe2);

  LG << "number of markers for init: " << keyframe_markers.size();

  if (keyframe_markers.size() < 8) {
    LG << "No enough markers to initialize from";
    libmv_reconstruction->is_valid = false;
    return libmv_reconstruction;
  }

  update_callback.invoke(0, "Initial reconstruction");

  EuclideanReconstructTwoFrames(keyframe_markers, &reconstruction);
  EuclideanBundle(normalized_tracks, &reconstruction);
  EuclideanCompleteReconstruction(normalized_tracks,
                                  &reconstruction,
                                  &update_callback);

  /* Refinement/ */
  if (libmv_reconstruction_options->refine_intrinsics) {
    libmv_solveRefineIntrinsics(
                                tracks,
                                libmv_reconstruction_options->refine_intrinsics,
                                libmv::BUNDLE_NO_CONSTRAINTS,
                                progress_update_callback,
                                callback_customdata,
                                &reconstruction,
                                camera_intrinsics);
  }

  /* Set reconstruction scale to unity. */
  EuclideanScaleToUnity(&reconstruction);

  /* Finish reconstruction. */
  finishReconstruction(tracks,
                       *camera_intrinsics,
                       libmv_reconstruction,
                       progress_update_callback,
                       callback_customdata);

  libmv_reconstruction->is_valid = true;
  return (libmv_Reconstruction *) libmv_reconstruction;
}

libmv_Reconstruction *libmv_solveModal(
    const libmv_Tracks *libmv_tracks,
    const libmv_CameraIntrinsicsOptions *libmv_camera_intrinsics_options,
    const libmv_ReconstructionOptions *libmv_reconstruction_options,
    reconstruct_progress_update_cb progress_update_callback,
    void *callback_customdata) {
  libmv_Reconstruction *libmv_reconstruction =
    LIBMV_OBJECT_NEW(libmv_Reconstruction);

  Tracks &tracks = *((Tracks *) libmv_tracks);
  EuclideanReconstruction &reconstruction =
    libmv_reconstruction->reconstruction;

  ReconstructUpdateCallback update_callback =
    ReconstructUpdateCallback(progress_update_callback,
                              callback_customdata);

  /* Retrieve reconstruction options from C-API to libmv API. */
  CameraIntrinsics *camera_intrinsics;
  camera_intrinsics = libmv_reconstruction->intrinsics =
    libmv_cameraIntrinsicsCreateFromOptions(
                                            libmv_camera_intrinsics_options);

  /* Invert the camera intrinsics. */
  Tracks normalized_tracks;
  libmv_getNormalizedTracks(tracks, *camera_intrinsics, &normalized_tracks);

  /* Actual reconstruction. */
  ModalSolver(normalized_tracks, &reconstruction, &update_callback);

  PolynomialCameraIntrinsics empty_intrinsics;
  EuclideanBundleCommonIntrinsics(normalized_tracks,
                                  libmv::BUNDLE_NO_INTRINSICS,
                                  libmv::BUNDLE_NO_TRANSLATION,
                                  &reconstruction,
                                  &empty_intrinsics);

  /* Refinement. */
  if (libmv_reconstruction_options->refine_intrinsics) {
    libmv_solveRefineIntrinsics(
                                tracks,
                                libmv_reconstruction_options->refine_intrinsics,
                                libmv::BUNDLE_NO_TRANSLATION,
                                progress_update_callback, callback_customdata,
                                &reconstruction,
                                camera_intrinsics);
  }

  /* Finish reconstruction. */
  finishReconstruction(tracks,
                       *camera_intrinsics,
                       libmv_reconstruction,
                       progress_update_callback,
                       callback_customdata);

  libmv_reconstruction->is_valid = true;
  return (libmv_Reconstruction *) libmv_reconstruction;
}

int libmv_reconstructionIsValid(libmv_Reconstruction *libmv_reconstruction) {
  return libmv_reconstruction->is_valid;
}

void libmv_reconstructionDestroy(libmv_Reconstruction *libmv_reconstruction) {
  LIBMV_OBJECT_DELETE(libmv_reconstruction->intrinsics, CameraIntrinsics);
  LIBMV_OBJECT_DELETE(libmv_reconstruction, libmv_Reconstruction);
}

int libmv_reprojectionPointForTrack(
    const libmv_Reconstruction *libmv_reconstruction,
    int track,
    double pos[3]) {
  const EuclideanReconstruction *reconstruction =
    &libmv_reconstruction->reconstruction;
  const EuclideanPoint *point =
    reconstruction->PointForTrack(track);
  if (point) {
    pos[0] = point->X[0];
    pos[1] = point->X[2];
    pos[2] = point->X[1];
    return 1;
  }
  return 0;
}

double libmv_reprojectionErrorForTrack(
    const libmv_Reconstruction *libmv_reconstruction,
    int track) {
  const EuclideanReconstruction *reconstruction =
    &libmv_reconstruction->reconstruction;
  const CameraIntrinsics *intrinsics = libmv_reconstruction->intrinsics;
  libmv::vector<Marker> markers =
    libmv_reconstruction->tracks.MarkersForTrack(track);

  int num_reprojected = 0;
  double total_error = 0.0;

  for (int i = 0; i < markers.size(); ++i) {
    double weight = markers[i].weight;
    const EuclideanCamera *camera =
      reconstruction->CameraForImage(markers[i].image);
    const EuclideanPoint *point =
      reconstruction->PointForTrack(markers[i].track);

    if (!camera || !point || weight == 0.0) {
      continue;
    }

    num_reprojected++;

    Marker reprojected_marker =
      libmv_projectMarker(*point, *camera, *intrinsics);
    double ex = (reprojected_marker.x - markers[i].x) * weight;
    double ey = (reprojected_marker.y - markers[i].y) * weight;

    total_error += sqrt(ex * ex + ey * ey);
  }

  return total_error / num_reprojected;
}

double libmv_reprojectionErrorForImage(
    const libmv_Reconstruction *libmv_reconstruction,
    int image) {
  const EuclideanReconstruction *reconstruction =
    &libmv_reconstruction->reconstruction;
  const CameraIntrinsics *intrinsics = libmv_reconstruction->intrinsics;
  libmv::vector<Marker> markers =
    libmv_reconstruction->tracks.MarkersInImage(image);
  const EuclideanCamera *camera = reconstruction->CameraForImage(image);
  int num_reprojected = 0;
  double total_error = 0.0;

  if (!camera) {
    return 0.0;
  }

  for (int i = 0; i < markers.size(); ++i) {
    const EuclideanPoint *point =
      reconstruction->PointForTrack(markers[i].track);

    if (!point) {
      continue;
    }

    num_reprojected++;

    Marker reprojected_marker =
      libmv_projectMarker(*point, *camera, *intrinsics);
    double ex = (reprojected_marker.x - markers[i].x) * markers[i].weight;
    double ey = (reprojected_marker.y - markers[i].y) * markers[i].weight;

    total_error += sqrt(ex * ex + ey * ey);
  }

  return total_error / num_reprojected;
}

int libmv_reprojectionCameraForImage(
    const libmv_Reconstruction *libmv_reconstruction,
    int image,
    double mat[4][4]) {
  const EuclideanReconstruction *reconstruction =
    &libmv_reconstruction->reconstruction;
  const EuclideanCamera *camera =
    reconstruction->CameraForImage(image);

  if (camera) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        int l = k;

        if (k == 1) {
          l = 2;
        } else if (k == 2) {
          l = 1;
        }

        if (j == 2) {
          mat[j][l] = -camera->R(j, k);
        } else {
          mat[j][l] = camera->R(j, k);
        }
      }
      mat[j][3] = 0.0;
    }

    libmv::Vec3 optical_center = -camera->R.transpose() * camera->t;

    mat[3][0] = optical_center(0);
    mat[3][1] = optical_center(2);
    mat[3][2] = optical_center(1);

    mat[3][3] = 1.0;

    return 1;
  }

  return 0;
}

double libmv_reprojectionError(
    const libmv_Reconstruction *libmv_reconstruction) {
  return libmv_reconstruction->error;
}

libmv_CameraIntrinsics *libmv_reconstructionExtractIntrinsics(
    libmv_Reconstruction *libmv_reconstruction) {
  return (libmv_CameraIntrinsics *) libmv_reconstruction->intrinsics;
}
