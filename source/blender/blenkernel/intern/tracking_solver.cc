/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains blender-side implementation of camera solver.
 */

#include <climits>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_fcurve.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "libmv-capi.h"
#include "tracking_private.h"

struct MovieReconstructContext {
  libmv_Tracks *tracks;
  bool select_keyframes;
  int keyframe1, keyframe2;
  int refine_flags;

  libmv_Reconstruction *reconstruction;

  char object_name[MAX_NAME];
  short motion_flag;

  libmv_CameraIntrinsicsOptions camera_intrinsics_options;

  float reprojection_error;

  TracksMap *tracks_map;

  int sfra, efra;

  /* Details about reconstruction error, reported by Libmv. */
  char error_message[1024];
};

struct ReconstructProgressData {
  bool *stop;
  bool *do_update;
  float *progress;
  char *stats_message;
  int message_size;
};

/* Create new libmv Tracks structure from blender's tracks list. */
static libmv_Tracks *libmv_tracks_new(MovieClip *clip, ListBase *tracksbase, int width, int height)
{
  int tracknr = 0;
  MovieTrackingTrack *track;
  libmv_Tracks *tracks = libmv_tracksNew();

  track = static_cast<MovieTrackingTrack *>(tracksbase->first);
  while (track) {
    FCurve *weight_fcurve = id_data_find_fcurve(
        &clip->id, track, &RNA_MovieTrackingTrack, "weight", 0, nullptr);

    for (int a = 0; a < track->markersnr; a++) {
      MovieTrackingMarker *marker = &track->markers[a];

      if ((marker->flag & MARKER_DISABLED) == 0) {
        float weight = track->weight;

        if (weight_fcurve) {
          int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);
          weight = evaluate_fcurve(weight_fcurve, scene_framenr);
        }

        libmv_tracksInsert(tracks,
                           marker->framenr,
                           tracknr,
                           (marker->pos[0] + track->offset[0]) * width,
                           (marker->pos[1] + track->offset[1]) * height,
                           weight);
      }
    }

    track = track->next;
    tracknr++;
  }

  return tracks;
}

/* Retrieve refined camera intrinsics from libmv to blender. */
static void reconstruct_retrieve_libmv_intrinsics(MovieReconstructContext *context,
                                                  MovieTracking *tracking)
{
  libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
  libmv_CameraIntrinsics *libmv_intrinsics = libmv_reconstructionExtractIntrinsics(
      libmv_reconstruction);

  libmv_CameraIntrinsicsOptions camera_intrinsics_options;
  libmv_cameraIntrinsicsExtractOptions(libmv_intrinsics, &camera_intrinsics_options);

  tracking_trackingCameraFromIntrinscisOptions(tracking, &camera_intrinsics_options);
}

/* Retrieve reconstructed tracks from libmv to blender.
 * Actually, this also copies reconstructed cameras
 * from libmv to movie clip datablock.
 */
static bool reconstruct_retrieve_libmv_tracks(MovieReconstructContext *context,
                                              MovieTracking *tracking)
{
  libmv_Reconstruction *libmv_reconstruction = context->reconstruction;
  bool ok = true;
  bool origin_set = false;
  int sfra = context->sfra, efra = context->efra;
  float imat[4][4];

  MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(tracking,
                                                                       context->object_name);
  MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

  unit_m4(imat);

  {
    int track_index = 0;
    LISTBASE_FOREACH_INDEX (MovieTrackingTrack *, track, &tracking_object->tracks, track_index) {
      double pos[3];

      if (libmv_reprojectionPointForTrack(libmv_reconstruction, track_index, pos)) {
        track->bundle_pos[0] = pos[0];
        track->bundle_pos[1] = pos[1];
        track->bundle_pos[2] = pos[2];

        track->flag |= TRACK_HAS_BUNDLE;
        track->error = libmv_reprojectionErrorForTrack(libmv_reconstruction, track_index);
      }
      else {
        track->flag &= ~TRACK_HAS_BUNDLE;
        ok = false;

        printf("Unable to reconstruct position for track #%d '%s'\n", track_index, track->name);
      }
    }
  }

  if (reconstruction->cameras) {
    MEM_freeN(reconstruction->cameras);
  }

  reconstruction->camnr = 0;
  reconstruction->cameras = nullptr;

  MovieReconstructedCamera *reconstructed_cameras = MEM_cnew_array<MovieReconstructedCamera>(
      (efra - sfra + 1), "temp reconstructed camera");

  for (int a = sfra; a <= efra; a++) {
    double matd[4][4];

    if (libmv_reprojectionCameraForImage(libmv_reconstruction, a, matd)) {
      float mat[4][4];
      float error = libmv_reprojectionErrorForImage(libmv_reconstruction, a);

      /* TODO(sergey): Use transpose utility. */
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
          mat[i][j] = matd[i][j];
        }
      }

      /* Ensure first camera has got zero rotation and transform.
       * This is essential for object tracking to work -- this way
       * we'll always know object and environment are properly
       * oriented.
       *
       * There's one weak part tho, which is requirement object
       * motion starts at the same frame as camera motion does,
       * otherwise that;' be a Russian roulette whether object is
       * aligned correct or not.
       */
      if (!origin_set) {
        invert_m4_m4(imat, mat);
        unit_m4(mat);
        origin_set = true;
      }
      else {
        mul_m4_m4m4(mat, imat, mat);
      }

      MovieReconstructedCamera *reconstructed_camera =
          &reconstructed_cameras[reconstruction->camnr];
      copy_m4_m4(reconstructed_camera->mat, mat);
      reconstructed_camera->framenr = a;
      reconstructed_camera->error = error;
      reconstruction->camnr++;
    }
    else {
      ok = false;
      printf("No camera for frame %d\n", a);
    }
  }

  if (reconstruction->camnr) {
    const size_t size = reconstruction->camnr * sizeof(MovieReconstructedCamera);
    reconstruction->cameras = MEM_cnew_array<MovieReconstructedCamera>(reconstruction->camnr,
                                                                       "reconstructed camera");
    memcpy(reconstruction->cameras, reconstructed_cameras, size);
  }

  if (origin_set) {
    LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
      if (track->flag & TRACK_HAS_BUNDLE) {
        mul_v3_m4v3(track->bundle_pos, imat, track->bundle_pos);
      }
    }
  }

  MEM_freeN(reconstructed_cameras);

  return ok;
}

/* Retrieve all the libmv data from context to blender's side data blocks. */
static int reconstruct_retrieve_libmv(MovieReconstructContext *context, MovieTracking *tracking)
{
  /* take the intrinsics back from libmv */
  reconstruct_retrieve_libmv_intrinsics(context, tracking);

  return reconstruct_retrieve_libmv_tracks(context, tracking);
}

/* Convert blender's refinement flags to libmv's. */
static int reconstruct_refine_intrinsics_get_flags(MovieTracking *tracking,
                                                   MovieTrackingObject *tracking_object)
{
  const int refine = tracking->settings.refine_camera_intrinsics;
  int flags = 0;

  if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
    return 0;
  }

  if (refine & REFINE_FOCAL_LENGTH) {
    flags |= LIBMV_REFINE_FOCAL_LENGTH;
  }

  if (refine & REFINE_PRINCIPAL_POINT) {
    flags |= LIBMV_REFINE_PRINCIPAL_POINT;
  }

  if (refine & REFINE_RADIAL_DISTORTION) {
    flags |= LIBMV_REFINE_RADIAL_DISTORTION;
  }

  if (refine & REFINE_TANGENTIAL_DISTORTION) {
    flags |= LIBMV_REFINE_TANGENTIAL_DISTORTION;
  }

  return flags;
}

/* Count tracks which has markers at both of keyframes. */
static int reconstruct_count_tracks_on_both_keyframes(MovieTrackingObject *tracking_object)
{
  const int frame1 = tracking_object->keyframe1, frame2 = tracking_object->keyframe2;

  int tot = 0;
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (BKE_tracking_track_has_enabled_marker_at_frame(track, frame1)) {
      if (BKE_tracking_track_has_enabled_marker_at_frame(track, frame2)) {
        tot++;
      }
    }
  }

  return tot;
}

bool BKE_tracking_reconstruction_check(MovieTracking *tracking,
                                       MovieTrackingObject *tracking_object,
                                       char *error_msg,
                                       int error_size)
{
  if (tracking->settings.motion_flag & TRACKING_MOTION_MODAL) {
    /* TODO: check for number of tracks? */
    return true;
  }
  if ((tracking->settings.reconstruction_flag & TRACKING_USE_KEYFRAME_SELECTION) == 0) {
    /* automatic keyframe selection does not require any pre-process checks */
    if (reconstruct_count_tracks_on_both_keyframes(tracking_object) < 8) {
      BLI_strncpy(error_msg,
                  N_("At least 8 common tracks on both keyframes are needed for reconstruction"),
                  error_size);

      return false;
    }
  }

#ifndef WITH_LIBMV
  BLI_strncpy(error_msg, N_("Blender is compiled without motion tracking library"), error_size);
  return false;
#endif

  return true;
}

MovieReconstructContext *BKE_tracking_reconstruction_context_new(
    MovieClip *clip,
    MovieTrackingObject *tracking_object,
    int keyframe1,
    int keyframe2,
    int width,
    int height)
{
  MovieTracking *tracking = &clip->tracking;
  MovieReconstructContext *context = MEM_cnew<MovieReconstructContext>(
      "MovieReconstructContext data");
  const float aspy = 1.0f / tracking->camera.pixel_aspect;
  const int num_tracks = BLI_listbase_count(&tracking_object->tracks);
  int sfra = INT_MAX, efra = INT_MIN;

  STRNCPY(context->object_name, tracking_object->name);
  context->motion_flag = tracking->settings.motion_flag;

  context->select_keyframes = (tracking->settings.reconstruction_flag &
                               TRACKING_USE_KEYFRAME_SELECTION) != 0;

  tracking_cameraIntrinscisOptionsFromTracking(
      tracking, width, height, &context->camera_intrinsics_options);

  context->tracks_map = tracks_map_new(context->object_name, num_tracks);

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    int first = 0, last = track->markersnr - 1;
    MovieTrackingMarker *first_marker = &track->markers[0];
    MovieTrackingMarker *last_marker = &track->markers[track->markersnr - 1];

    /* find first not-disabled marker */
    while (first <= track->markersnr - 1 && first_marker->flag & MARKER_DISABLED) {
      first++;
      first_marker++;
    }

    /* find last not-disabled marker */
    while (last >= 0 && last_marker->flag & MARKER_DISABLED) {
      last--;
      last_marker--;
    }

    if (first <= track->markersnr - 1) {
      sfra = min_ii(sfra, first_marker->framenr);
    }

    if (last >= 0) {
      efra = max_ii(efra, last_marker->framenr);
    }

    tracks_map_insert(context->tracks_map, track);
  }

  context->sfra = sfra;
  context->efra = efra;

  context->tracks = libmv_tracks_new(clip, &tracking_object->tracks, width, height * aspy);
  context->keyframe1 = keyframe1;
  context->keyframe2 = keyframe2;
  context->refine_flags = reconstruct_refine_intrinsics_get_flags(tracking, tracking_object);

  context->error_message[0] = '\0';

  return context;
}

void BKE_tracking_reconstruction_report_error_message(MovieReconstructContext *context,
                                                      const char *error_message)
{
  if (context->error_message[0]) {
    /* Only keep initial error message, the rest are inducted ones. */
    return;
  }
  STRNCPY(context->error_message, error_message);
}

const char *BKE_tracking_reconstruction_error_message_get(const MovieReconstructContext *context)
{
  return context->error_message;
}

void BKE_tracking_reconstruction_context_free(MovieReconstructContext *context)
{
  if (context->reconstruction) {
    libmv_reconstructionDestroy(context->reconstruction);
  }

  libmv_tracksDestroy(context->tracks);

  tracks_map_free(context->tracks_map);

  MEM_freeN(context);
}

/* Callback which is called from libmv side to update progress in the interface. */
static void reconstruct_update_solve_cb(void *customdata, double progress, const char *message)
{
  ReconstructProgressData *progressdata = static_cast<ReconstructProgressData *>(customdata);

  if (progressdata->progress) {
    *progressdata->progress = progress;
    *progressdata->do_update = true;
  }

  BLI_snprintf(
      progressdata->stats_message, progressdata->message_size, "Solving camera | %s", message);
}

/* Fill in reconstruction options structure from reconstruction context. */
static void reconstructionOptionsFromContext(libmv_ReconstructionOptions *reconstruction_options,
                                             MovieReconstructContext *context)
{
  reconstruction_options->select_keyframes = context->select_keyframes;

  reconstruction_options->keyframe1 = context->keyframe1;
  reconstruction_options->keyframe2 = context->keyframe2;

  reconstruction_options->refine_intrinsics = context->refine_flags;
}

void BKE_tracking_reconstruction_solve(MovieReconstructContext *context,
                                       bool *stop,
                                       bool *do_update,
                                       float *progress,
                                       char *stats_message,
                                       int message_size)
{
  float error;

  ReconstructProgressData progressdata;

  libmv_ReconstructionOptions reconstruction_options;

  progressdata.stop = stop;
  progressdata.do_update = do_update;
  progressdata.progress = progress;
  progressdata.stats_message = stats_message;
  progressdata.message_size = message_size;

  reconstructionOptionsFromContext(&reconstruction_options, context);

  if (context->motion_flag & TRACKING_MOTION_MODAL) {
    context->reconstruction = libmv_solveModal(context->tracks,
                                               &context->camera_intrinsics_options,
                                               &reconstruction_options,
                                               reconstruct_update_solve_cb,
                                               &progressdata);
  }
  else {
    context->reconstruction = libmv_solveReconstruction(context->tracks,
                                                        &context->camera_intrinsics_options,
                                                        &reconstruction_options,
                                                        reconstruct_update_solve_cb,
                                                        &progressdata);

    if (context->select_keyframes) {
      /* store actual keyframes used for reconstruction to update them in the interface later */
      context->keyframe1 = reconstruction_options.keyframe1;
      context->keyframe2 = reconstruction_options.keyframe2;
    }
  }

  error = libmv_reprojectionError(context->reconstruction);

  context->reprojection_error = error;
}

bool BKE_tracking_reconstruction_finish(MovieReconstructContext *context, MovieTracking *tracking)
{
  if (!libmv_reconstructionIsValid(context->reconstruction)) {
    BKE_tracking_reconstruction_report_error_message(
        context, "Failed to solve the motion: most likely there are no good keyframes");
    return false;
  }

  tracks_map_merge(context->tracks_map, tracking);
  BKE_tracking_dopesheet_tag_update(tracking);

  MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(tracking,
                                                                       context->object_name);
  MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

  /* update keyframe in the interface */
  if (context->select_keyframes) {
    tracking_object->keyframe1 = context->keyframe1;
    tracking_object->keyframe2 = context->keyframe2;
  }

  reconstruction->error = context->reprojection_error;
  reconstruction->flag |= TRACKING_RECONSTRUCTED;

  if (!reconstruct_retrieve_libmv(context, tracking)) {
    return false;
  }

  return true;
}

static void tracking_scale_reconstruction(ListBase *tracksbase,
                                          MovieTrackingReconstruction *reconstruction,
                                          const float scale[3])
{
  float first_camera_delta[3] = {0.0f, 0.0f, 0.0f};

  if (reconstruction->camnr > 0) {
    mul_v3_v3v3(first_camera_delta, reconstruction->cameras[0].mat[3], scale);
  }

  for (int i = 0; i < reconstruction->camnr; i++) {
    MovieReconstructedCamera *camera = &reconstruction->cameras[i];
    mul_v3_v3(camera->mat[3], scale);
    sub_v3_v3(camera->mat[3], first_camera_delta);
  }

  LISTBASE_FOREACH (MovieTrackingTrack *, track, tracksbase) {
    if (track->flag & TRACK_HAS_BUNDLE) {
      mul_v3_v3(track->bundle_pos, scale);
      sub_v3_v3(track->bundle_pos, first_camera_delta);
    }
  }
}

void BKE_tracking_reconstruction_scale(MovieTracking *tracking, float scale[3])
{
  LISTBASE_FOREACH (MovieTrackingObject *, object, &tracking->objects) {
    tracking_scale_reconstruction(&object->tracks, &object->reconstruction, scale);
  }
}
