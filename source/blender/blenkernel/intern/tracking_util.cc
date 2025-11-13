/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains implementation of function which are used
 * by multiple tracking files but which should not be public.
 */

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_threads.h"

#include "BLT_translation.hh"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "tracking_private.h"

#include "libmv-capi.h"

/* Uncomment this to have caching-specific debug prints. */
// #define DEBUG_CACHE

#ifdef DEBUG_CACHE
#  define CACHE_PRINTF(...) printf(__VA_ARGS__)
#else
#  define CACHE_PRINTF(...)
#endif

/* -------------------------------------------------------------------- */
/** \name Tracks Map
 * \{ */

TracksMap *tracks_map_new(const char *object_name, int num_tracks)
{
  TracksMap *map = MEM_callocN<TracksMap>("TrackingsMap");

  STRNCPY(map->object_name, object_name);

  map->num_tracks = num_tracks;

  map->tracks = MEM_calloc_arrayN<MovieTrackingTrack>(num_tracks, "TrackingsMap tracks");

  map->hash = BLI_ghash_ptr_new("TracksMap hash");

  BLI_spin_init(&map->spin_lock);

  return map;
}

int tracks_map_get_size(TracksMap *map)
{
  return map->num_tracks;
}

void tracks_map_insert(TracksMap *map, MovieTrackingTrack *track)
{
  MovieTrackingTrack new_track = *track;

  new_track.markers = static_cast<MovieTrackingMarker *>(MEM_dupallocN(new_track.markers));

  map->tracks[map->ptr] = new_track;

  BLI_ghash_insert(map->hash, &map->tracks[map->ptr], track);

  map->ptr++;
}

void tracks_map_merge(TracksMap *map, MovieTracking *tracking)
{
  MovieTrackingTrack *track;
  ListBase tracks = {nullptr, nullptr}, new_tracks = {nullptr, nullptr};
  ListBase *old_tracks;

  MovieTrackingObject *tracking_object = BKE_tracking_object_get_named(tracking, map->object_name);
  if (!tracking_object) {
    /* object was deleted by user, create new one */
    tracking_object = BKE_tracking_object_add(tracking, map->object_name);
  }

  old_tracks = &tracking_object->tracks;

  /* duplicate currently operating tracks to temporary list.
   * this is needed to keep names in unique state and it's faster to change names
   * of currently operating tracks (if needed)
   */
  for (int a = 0; a < map->num_tracks; a++) {
    MovieTrackingTrack *old_track;
    bool mapped_to_old = false;

    track = &map->tracks[a];

    /* find original of operating track in list of previously displayed tracks */
    old_track = static_cast<MovieTrackingTrack *>(BLI_ghash_lookup(map->hash, track));
    if (old_track) {
      if (BLI_findindex(old_tracks, old_track) != -1) {
        BLI_remlink(old_tracks, old_track);

        BLI_spin_lock(&map->spin_lock);

        /* Copy flags like selection back to the track map. */
        track->flag = old_track->flag;
        track->pat_flag = old_track->pat_flag;
        track->search_flag = old_track->search_flag;

        /* Copy all the rest settings back from the map to the actual tracks. */
        MEM_freeN(old_track->markers);
        *old_track = *track;
        old_track->markers = static_cast<MovieTrackingMarker *>(MEM_dupallocN(old_track->markers));

        BLI_spin_unlock(&map->spin_lock);

        BLI_addtail(&tracks, old_track);

        mapped_to_old = true;
      }
    }

    if (mapped_to_old == false) {
      MovieTrackingTrack *new_track = BKE_tracking_track_duplicate(track);

      /* Update old-new track mapping */
      BLI_ghash_reinsert(map->hash, track, new_track, nullptr, nullptr);

      BLI_addtail(&tracks, new_track);
    }
  }

  /* move all tracks, which aren't operating */
  track = static_cast<MovieTrackingTrack *>(old_tracks->first);
  while (track) {
    MovieTrackingTrack *next = track->next;
    BLI_addtail(&new_tracks, track);
    track = next;
  }

  /* now move all tracks which are currently operating and keep their names unique */
  track = static_cast<MovieTrackingTrack *>(tracks.first);
  while (track) {
    MovieTrackingTrack *next = track->next;

    BLI_remlink(&tracks, track);

    track->next = track->prev = nullptr;
    BLI_addtail(&new_tracks, track);

    BLI_uniquename(&new_tracks,
                   track,
                   CTX_DATA_(BLT_I18NCONTEXT_ID_MOVIECLIP, "Track"),
                   '.',
                   offsetof(MovieTrackingTrack, name),
                   sizeof(track->name));

    track = next;
  }

  *old_tracks = new_tracks;
}

void tracks_map_free(TracksMap *map)
{
  BLI_ghash_free(map->hash, nullptr, nullptr);

  for (int i = 0; i < map->num_tracks; i++) {
    BKE_tracking_track_free(&map->tracks[i]);
  }

  MEM_freeN(map->tracks);

  BLI_spin_end(&map->spin_lock);

  MEM_freeN(map);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Transformation Functions
 * \{ */

/* Three coordinate frames: Frame, Search, and Marker
 * Two units: Pixels, Unified
 * Notation: {coordinate frame}_{unit}; for example, "search_pixel" are search
 * window relative coordinates in pixels, and "frame_unified" are unified 0..1
 * coordinates relative to the entire frame.
 */
static void unified_to_pixel(int frame_width,
                             int frame_height,
                             const float unified_coords[2],
                             float pixel_coords[2])
{
  pixel_coords[0] = unified_coords[0] * frame_width;
  pixel_coords[1] = unified_coords[1] * frame_height;
}

static void marker_to_frame_unified(const MovieTrackingMarker *marker,
                                    const float marker_unified_coords[2],
                                    float frame_unified_coords[2])
{
  frame_unified_coords[0] = marker_unified_coords[0] + marker->pos[0];
  frame_unified_coords[1] = marker_unified_coords[1] + marker->pos[1];
}

static void marker_unified_to_frame_pixel_coordinates(int frame_width,
                                                      int frame_height,
                                                      const MovieTrackingMarker *marker,
                                                      const float marker_unified_coords[2],
                                                      float frame_pixel_coords[2])
{
  marker_to_frame_unified(marker, marker_unified_coords, frame_pixel_coords);
  unified_to_pixel(frame_width, frame_height, frame_pixel_coords, frame_pixel_coords);
}

void tracking_get_search_origin_frame_pixel(int frame_width,
                                            int frame_height,
                                            const MovieTrackingMarker *marker,
                                            float frame_pixel[2])
{
  /* Get the lower left coordinate of the search window and snap to pixel coordinates */
  marker_unified_to_frame_pixel_coordinates(
      frame_width, frame_height, marker, marker->search_min, frame_pixel);
  frame_pixel[0] = int(frame_pixel[0]);
  frame_pixel[1] = int(frame_pixel[1]);
}

static void pixel_to_unified(int frame_width,
                             int frame_height,
                             const float pixel_coords[2],
                             float unified_coords[2])
{
  unified_coords[0] = pixel_coords[0] / frame_width;
  unified_coords[1] = pixel_coords[1] / frame_height;
}

static void marker_unified_to_search_pixel(int frame_width,
                                           int frame_height,
                                           const MovieTrackingMarker *marker,
                                           const float marker_unified[2],
                                           float search_pixel[2])
{
  float frame_pixel[2];
  float search_origin_frame_pixel[2];

  marker_unified_to_frame_pixel_coordinates(
      frame_width, frame_height, marker, marker_unified, frame_pixel);
  tracking_get_search_origin_frame_pixel(
      frame_width, frame_height, marker, search_origin_frame_pixel);
  sub_v2_v2v2(search_pixel, frame_pixel, search_origin_frame_pixel);
}

static void search_pixel_to_marker_unified(int frame_width,
                                           int frame_height,
                                           const MovieTrackingMarker *marker,
                                           const float search_pixel[2],
                                           float marker_unified[2])
{
  float frame_unified[2];
  float search_origin_frame_pixel[2];

  tracking_get_search_origin_frame_pixel(
      frame_width, frame_height, marker, search_origin_frame_pixel);
  add_v2_v2v2(frame_unified, search_pixel, search_origin_frame_pixel);
  pixel_to_unified(frame_width, frame_height, frame_unified, frame_unified);

  /* marker pos is in frame unified */
  sub_v2_v2v2(marker_unified, frame_unified, marker->pos);
}

void tracking_get_marker_coords_for_tracking(int frame_width,
                                             int frame_height,
                                             const MovieTrackingMarker *marker,
                                             double search_pixel_x[5],
                                             double search_pixel_y[5])
{
  float unified_coords[2];
  float pixel_coords[2];

  /* Convert the corners into search space coordinates. */
  for (int i = 0; i < 4; i++) {
    marker_unified_to_search_pixel(
        frame_width, frame_height, marker, marker->pattern_corners[i], pixel_coords);
    search_pixel_x[i] = pixel_coords[0] - 0.5f;
    search_pixel_y[i] = pixel_coords[1] - 0.5f;
  }

  /* Convert the center position (aka "pos"); this is the origin */
  unified_coords[0] = 0.0f;
  unified_coords[1] = 0.0f;
  marker_unified_to_search_pixel(frame_width, frame_height, marker, unified_coords, pixel_coords);

  search_pixel_x[4] = pixel_coords[0] - 0.5f;
  search_pixel_y[4] = pixel_coords[1] - 0.5f;
}

void tracking_set_marker_coords_from_tracking(int frame_width,
                                              int frame_height,
                                              MovieTrackingMarker *marker,
                                              const double search_pixel_x[5],
                                              const double search_pixel_y[5])
{
  float marker_unified[2];
  float search_pixel[2];

  /* Convert the corners into search space coordinates. */
  for (int i = 0; i < 4; i++) {
    search_pixel[0] = search_pixel_x[i] + 0.5;
    search_pixel[1] = search_pixel_y[i] + 0.5;
    search_pixel_to_marker_unified(
        frame_width, frame_height, marker, search_pixel, marker->pattern_corners[i]);
  }

  /* Convert the center position (aka "pos"); this is the origin */
  search_pixel[0] = search_pixel_x[4] + 0.5;
  search_pixel[1] = search_pixel_y[4] + 0.5;
  search_pixel_to_marker_unified(frame_width, frame_height, marker, search_pixel, marker_unified);

  /* If the tracker tracked nothing, then "marker_unified" would be zero.
   * Otherwise, the entire patch shifted, and that delta should be applied to
   * all the coordinates.
   */
  for (int i = 0; i < 4; i++) {
    marker->pattern_corners[i][0] -= marker_unified[0];
    marker->pattern_corners[i][1] -= marker_unified[1];
  }

  marker->pos[0] += marker_unified[0];
  marker->pos[1] += marker_unified[1];
}

void tracking_principal_point_normalized_to_pixel(const float principal_point_normalized[2],
                                                  const int frame_width,
                                                  const int frame_height,
                                                  float r_principal_point_pixel[2])
{
  const float frame_center_x = float(frame_width) / 2;
  const float frame_center_y = float(frame_height) / 2;

  r_principal_point_pixel[0] = frame_center_x + principal_point_normalized[0] * frame_center_x;
  r_principal_point_pixel[1] = frame_center_y + principal_point_normalized[1] * frame_center_y;
}

void tracking_principal_point_pixel_to_normalized(const float principal_point_pixel[2],
                                                  const int frame_width,
                                                  const int frame_height,
                                                  float r_principal_point_normalized[2])
{
  const float frame_center_x = float(frame_width) / 2;
  const float frame_center_y = float(frame_height) / 2;

  r_principal_point_normalized[0] = (principal_point_pixel[0] - frame_center_x) / frame_center_x;
  r_principal_point_normalized[1] = (principal_point_pixel[1] - frame_center_y) / frame_center_y;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Purpose Utility Functions
 * \{ */

void tracking_marker_insert_disabled(MovieTrackingTrack *track,
                                     const MovieTrackingMarker *ref_marker,
                                     bool before,
                                     bool overwrite)
{
  MovieTrackingMarker marker_new;

  marker_new = *ref_marker;
  marker_new.flag &= ~MARKER_TRACKED;
  marker_new.flag |= MARKER_DISABLED;

  if (before) {
    marker_new.framenr--;
  }
  else {
    marker_new.framenr++;
  }

  if (overwrite || !BKE_tracking_track_has_marker_at_frame(track, marker_new.framenr)) {
    BKE_tracking_marker_insert(track, &marker_new);
  }
}

static void distortion_model_parameters_from_tracking(
    const MovieTrackingCamera *camera, libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
  switch (camera->distortion_model) {
    case TRACKING_DISTORTION_MODEL_POLYNOMIAL:
      camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_POLYNOMIAL;
      camera_intrinsics_options->polynomial_k1 = camera->k1;
      camera_intrinsics_options->polynomial_k2 = camera->k2;
      camera_intrinsics_options->polynomial_k3 = camera->k3;
      camera_intrinsics_options->polynomial_p1 = 0.0;
      camera_intrinsics_options->polynomial_p2 = 0.0;
      return;

    case TRACKING_DISTORTION_MODEL_DIVISION:
      camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_DIVISION;
      camera_intrinsics_options->division_k1 = camera->division_k1;
      camera_intrinsics_options->division_k2 = camera->division_k2;
      return;

    case TRACKING_DISTORTION_MODEL_NUKE:
      camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_NUKE;
      camera_intrinsics_options->nuke_k1 = camera->nuke_k1;
      camera_intrinsics_options->nuke_k2 = camera->nuke_k2;
      camera_intrinsics_options->nuke_p1 = camera->nuke_p1;
      camera_intrinsics_options->nuke_p2 = camera->nuke_p2;
      return;
    case TRACKING_DISTORTION_MODEL_BROWN:
      camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_BROWN;
      camera_intrinsics_options->brown_k1 = camera->brown_k1;
      camera_intrinsics_options->brown_k2 = camera->brown_k2;
      camera_intrinsics_options->brown_k3 = camera->brown_k3;
      camera_intrinsics_options->brown_k4 = camera->brown_k4;
      camera_intrinsics_options->brown_p1 = camera->brown_p1;
      camera_intrinsics_options->brown_p2 = camera->brown_p2;
      return;
  }

  /* Unknown distortion model, which might be due to opening newer file in older Blender.
   * Fall back to a known and supported model with 0 distortion. */
  camera_intrinsics_options->distortion_model = LIBMV_DISTORTION_MODEL_POLYNOMIAL;
  camera_intrinsics_options->polynomial_k1 = 0.0;
  camera_intrinsics_options->polynomial_k2 = 0.0;
  camera_intrinsics_options->polynomial_k3 = 0.0;
  camera_intrinsics_options->polynomial_p1 = 0.0;
  camera_intrinsics_options->polynomial_p2 = 0.0;
}

static void distortion_model_parameters_from_options(
    const libmv_CameraIntrinsicsOptions *camera_intrinsics_options, MovieTrackingCamera *camera)
{
  switch (camera_intrinsics_options->distortion_model) {
    case LIBMV_DISTORTION_MODEL_POLYNOMIAL:
      camera->distortion_model = TRACKING_DISTORTION_MODEL_POLYNOMIAL;
      camera->k1 = camera_intrinsics_options->polynomial_k1;
      camera->k2 = camera_intrinsics_options->polynomial_k2;
      camera->k3 = camera_intrinsics_options->polynomial_k3;
      return;

    case LIBMV_DISTORTION_MODEL_DIVISION:
      camera->distortion_model = TRACKING_DISTORTION_MODEL_DIVISION;
      camera->division_k1 = camera_intrinsics_options->division_k1;
      camera->division_k2 = camera_intrinsics_options->division_k2;
      return;

    case LIBMV_DISTORTION_MODEL_NUKE:
      camera->distortion_model = TRACKING_DISTORTION_MODEL_NUKE;
      camera->nuke_k1 = camera_intrinsics_options->nuke_k1;
      camera->nuke_k2 = camera_intrinsics_options->nuke_k2;
      camera->nuke_p1 = camera_intrinsics_options->nuke_p1;
      camera->nuke_p2 = camera_intrinsics_options->nuke_p2;
      return;
    case LIBMV_DISTORTION_MODEL_BROWN:
      camera->distortion_model = TRACKING_DISTORTION_MODEL_BROWN;
      camera->brown_k1 = camera_intrinsics_options->brown_k1;
      camera->brown_k2 = camera_intrinsics_options->brown_k2;
      camera->brown_k3 = camera_intrinsics_options->brown_k3;
      camera->brown_k4 = camera_intrinsics_options->brown_k4;
      camera->brown_p1 = camera_intrinsics_options->brown_p1;
      camera->brown_p2 = camera_intrinsics_options->brown_p2;
      return;
  }

  /* Libmv returned distortion model which is not known to Blender. This is a logical error in code
   * and Blender side is to be updated to match Libmv. */
  BLI_assert_msg(0, "Unknown distortion model");
}

void tracking_cameraIntrinscisOptionsFromTracking(
    MovieTracking *tracking,
    const int calibration_width,
    const int calibration_height,
    libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
  MovieTrackingCamera *camera = &tracking->camera;
  const float aspy = 1.0f / tracking->camera.pixel_aspect;

  float principal_px[2];
  tracking_principal_point_normalized_to_pixel(
      camera->principal_point, calibration_width, calibration_height, principal_px);

  camera_intrinsics_options->focal_length = camera->focal;

  camera_intrinsics_options->principal_point_x = principal_px[0];
  camera_intrinsics_options->principal_point_y = principal_px[1] * aspy;

  distortion_model_parameters_from_tracking(camera, camera_intrinsics_options);

  camera_intrinsics_options->image_width = calibration_width;
  camera_intrinsics_options->image_height = int(calibration_height * aspy);
}

void tracking_trackingCameraFromIntrinscisOptions(
    MovieTracking *tracking, const libmv_CameraIntrinsicsOptions *camera_intrinsics_options)
{
  MovieTrackingCamera *camera = &tracking->camera;

  camera->focal = camera_intrinsics_options->focal_length;

  /* NOTE: The image size stored in the `camera_intrinsics_options` is aspect-ratio corrected,
   * so there is no need to "un-apply" it from the principal point. */
  const float principal_px[2] = {float(camera_intrinsics_options->principal_point_x),
                                 float(camera_intrinsics_options->principal_point_y)};

  tracking_principal_point_pixel_to_normalized(principal_px,
                                               camera_intrinsics_options->image_width,
                                               camera_intrinsics_options->image_height,
                                               camera->principal_point);

  distortion_model_parameters_from_options(camera_intrinsics_options, camera);
}

MovieTrackingMarker *tracking_get_keyframed_marker(MovieTrackingTrack *track,
                                                   int current_frame,
                                                   bool backwards)
{
  MovieTrackingMarker *marker_keyed = nullptr;
  MovieTrackingMarker *marker_keyed_fallback = nullptr;
  int a = BKE_tracking_marker_get(track, current_frame) - track->markers;

  while (a >= 0 && a < track->markersnr) {
    int next = backwards ? a + 1 : a - 1;
    bool is_keyframed = false;
    MovieTrackingMarker *cur_marker = &track->markers[a];
    MovieTrackingMarker *next_marker = nullptr;

    if (next >= 0 && next < track->markersnr) {
      next_marker = &track->markers[next];
    }

    if ((cur_marker->flag & MARKER_DISABLED) == 0) {
      /* If it'll happen so we didn't find a real keyframe marker,
       * fall back to the first marker in current tracked segment
       * as a keyframe. */
      if (next_marker == nullptr) {
        /* Could happen when trying to get reference marker for the fist
         * one on the segment which isn't surrounded by disabled markers.
         *
         * There's no really good choice here, just use the reference
         * marker which looks correct.. */
        if (marker_keyed_fallback == nullptr) {
          marker_keyed_fallback = cur_marker;
        }
      }
      else if (next_marker->flag & MARKER_DISABLED) {
        if (marker_keyed_fallback == nullptr) {
          marker_keyed_fallback = cur_marker;
        }
      }

      is_keyframed |= (cur_marker->flag & MARKER_TRACKED) == 0;
    }

    if (is_keyframed) {
      marker_keyed = cur_marker;

      break;
    }

    a = next;
  }

  if (marker_keyed == nullptr) {
    marker_keyed = marker_keyed_fallback;
  }

  return marker_keyed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Accessor
 * \{ */

static ImBuf *accessor_get_preprocessed_ibuf(TrackingImageAccessor *accessor,
                                             int clip_index,
                                             int frame)
{
  MovieClip *clip;
  MovieClipUser user;
  ImBuf *ibuf;
  int scene_frame;

  BLI_assert(clip_index < accessor->num_clips);

  clip = accessor->clips[clip_index];
  scene_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, frame);
  BKE_movieclip_user_set_frame(&user, scene_frame);
  user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
  user.render_flag = 0;
  ibuf = BKE_movieclip_get_ibuf(clip, &user);

  return ibuf;
}

static ImBuf *make_grayscale_ibuf_copy(ImBuf *ibuf)
{
  ImBuf *grayscale = IMB_allocImBuf(ibuf->x, ibuf->y, 32, 0);

  BLI_assert(ELEM(ibuf->channels, 3, 4));

  /* TODO(sergey): Bummer, currently IMB API only allows to create 4 channels
   * float buffer, so we do it manually here.
   *
   * Will generalize it later.
   */
  const size_t num_pixels = size_t(grayscale->x) * size_t(grayscale->y);
  grayscale->channels = 1;
  float *rect_float = MEM_calloc_arrayN<float>(num_pixels, "tracking grayscale image");
  if (rect_float != nullptr) {
    IMB_assign_float_buffer(grayscale, rect_float, IB_TAKE_OWNERSHIP);

    for (int i = 0; i < grayscale->x * grayscale->y; i++) {
      const float *pixel = ibuf->float_buffer.data + ibuf->channels * i;

      rect_float[i] = 0.2126f * pixel[0] + 0.7152f * pixel[1] + 0.0722f * pixel[2];
    }
  }

  return grayscale;
}

static void ibuf_to_float_image(const ImBuf *ibuf, libmv_FloatImage *float_image)
{
  BLI_assert(ibuf->float_buffer.data != nullptr);
  float_image->buffer = ibuf->float_buffer.data;
  float_image->width = ibuf->x;
  float_image->height = ibuf->y;
  float_image->channels = ibuf->channels;
}

static ImBuf *float_image_to_ibuf(libmv_FloatImage *float_image)
{
  ImBuf *ibuf = IMB_allocImBuf(float_image->width, float_image->height, 32, 0);
  size_t num_total_channels = size_t(ibuf->x) * size_t(ibuf->y) * float_image->channels;
  ibuf->channels = float_image->channels;
  float *rect_float = MEM_calloc_arrayN<float>(num_total_channels, "tracking grayscale image");
  if (rect_float != nullptr) {
    IMB_assign_float_buffer(ibuf, rect_float, IB_TAKE_OWNERSHIP);

    memcpy(rect_float, float_image->buffer, num_total_channels * sizeof(float));
  }
  return ibuf;
}

static ImBuf *accessor_get_ibuf(TrackingImageAccessor *accessor,
                                int clip_index,
                                int frame,
                                libmv_InputMode input_mode,
                                int downscale,
                                const libmv_Region *region,
                                const libmv_FrameTransform *transform)
{
  /* First try to get fully processed image from the cache. */
  CACHE_PRINTF("Calculate new buffer for frame %d\n", frame);
  /* And now we do post-processing of the original frame. */
  ImBuf *orig_ibuf = accessor_get_preprocessed_ibuf(accessor, clip_index, frame);
  if (orig_ibuf == nullptr) {
    return nullptr;
  }
  ImBuf *final_ibuf;
  /* Cut a region if requested. */
  if (region != nullptr) {
    int width = region->max[0] - region->min[0], height = region->max[1] - region->min[1];

    /* If the requested region goes outside of the actual frame we still
     * return the requested region size, but only fill it's partially with
     * the data we can.
     */
    int clamped_origin_x = max_ii(int(region->min[0]), 0),
        clamped_origin_y = max_ii(int(region->min[1]), 0);
    int dst_offset_x = clamped_origin_x - int(region->min[0]),
        dst_offset_y = clamped_origin_y - int(region->min[1]);
    int clamped_width = width - dst_offset_x, clamped_height = height - dst_offset_y;
    clamped_width = min_ii(clamped_width, orig_ibuf->x - clamped_origin_x);
    clamped_height = min_ii(clamped_height, orig_ibuf->y - clamped_origin_y);

    final_ibuf = IMB_allocImBuf(width, height, 32, IB_float_data);

    if (orig_ibuf->float_buffer.data != nullptr) {
      IMB_rectcpy(final_ibuf,
                  orig_ibuf,
                  dst_offset_x,
                  dst_offset_y,
                  clamped_origin_x,
                  clamped_origin_y,
                  clamped_width,
                  clamped_height);
    }
    else {
      /* TODO(sergey): We don't do any color space or alpha conversion
       * here. Probably Libmv is better to work in the linear space,
       * but keep sRGB space here for compatibility for now.
       */
      for (int y = 0; y < clamped_height; y++) {
        for (int x = 0; x < clamped_width; x++) {
          int src_x = x + clamped_origin_x, src_y = y + clamped_origin_y;
          int dst_x = x + dst_offset_x, dst_y = y + dst_offset_y;
          int dst_index = (dst_y * width + dst_x) * 4,
              src_index = (src_y * orig_ibuf->x + src_x) * 4;
          rgba_uchar_to_float(final_ibuf->float_buffer.data + dst_index,
                              orig_ibuf->byte_buffer.data + src_index);
        }
      }
    }
  }
  else {
    /* Libmv only works with float images,
     *
     * This would likely make it so loads of float buffers are being stored
     * in the cache which is nice on the one hand (faster re-use of the
     * frames) but on the other hand it bumps the memory usage up.
     */
    BLI_thread_lock(LOCK_MOVIECLIP);
    IMB_float_from_byte(orig_ibuf);
    BLI_thread_unlock(LOCK_MOVIECLIP);
    final_ibuf = orig_ibuf;
  }
  /* Downscale if needed. */
  if (downscale > 0) {
    if (final_ibuf == orig_ibuf) {
      final_ibuf = IMB_dupImBuf(orig_ibuf);
    }
    IMB_scale(final_ibuf,
              orig_ibuf->x / (1 << downscale),
              orig_ibuf->y / (1 << downscale),
              IMBScaleFilter::Box,
              false);
  }
  /* Apply possible transformation. */
  if (transform != nullptr) {
    libmv_FloatImage input_image, output_image;
    ibuf_to_float_image(final_ibuf, &input_image);
    libmv_frameAccessorgetTransformRun(transform, &input_image, &output_image);
    if (final_ibuf != orig_ibuf) {
      IMB_freeImBuf(final_ibuf);
    }
    final_ibuf = float_image_to_ibuf(&output_image);
    libmv_floatImageDestroy(&output_image);
  }
  /* Transform number of channels. */
  if (input_mode == LIBMV_IMAGE_MODE_RGBA) {
    BLI_assert(ELEM(orig_ibuf->channels, 3, 4));
    /* pass */
  }
  else /* if (input_mode == LIBMV_IMAGE_MODE_MONO) */ {
    BLI_assert(input_mode == LIBMV_IMAGE_MODE_MONO);
    if (final_ibuf->channels != 1) {
      ImBuf *grayscale_ibuf = make_grayscale_ibuf_copy(final_ibuf);
      if (final_ibuf != orig_ibuf) {
        /* We dereference original frame later. */
        IMB_freeImBuf(final_ibuf);
      }
      final_ibuf = grayscale_ibuf;
    }
  }
  /* It's possible processing still didn't happen at this point,
   * but we really need a copy of the buffer to be transformed
   * and to be put to the cache.
   */
  if (final_ibuf == orig_ibuf) {
    final_ibuf = IMB_dupImBuf(orig_ibuf);
  }
  IMB_freeImBuf(orig_ibuf);
  return final_ibuf;
}

static libmv_CacheKey accessor_get_image_callback(libmv_FrameAccessorUserData *user_data,
                                                  int clip_index,
                                                  int frame,
                                                  libmv_InputMode input_mode,
                                                  int downscale,
                                                  const libmv_Region *region,
                                                  const libmv_FrameTransform *transform,
                                                  float **destination,
                                                  int *width,
                                                  int *height,
                                                  int *channels)
{
  TrackingImageAccessor *accessor = (TrackingImageAccessor *)user_data;
  ImBuf *ibuf;

  BLI_assert(clip_index >= 0 && clip_index < accessor->num_clips);

  ibuf = accessor_get_ibuf(accessor, clip_index, frame, input_mode, downscale, region, transform);

  if (ibuf) {
    *destination = ibuf->float_buffer.data;
    *width = ibuf->x;
    *height = ibuf->y;
    *channels = ibuf->channels;
  }
  else {
    *destination = nullptr;
    *width = 0;
    *height = 0;
    *channels = 0;
  }

  return ibuf;
}

static void accessor_release_image_callback(libmv_CacheKey cache_key)
{
  ImBuf *ibuf = (ImBuf *)cache_key;
  IMB_freeImBuf(ibuf);
}

static libmv_CacheKey accessor_get_mask_for_track_callback(libmv_FrameAccessorUserData *user_data,
                                                           int clip_index,
                                                           int frame,
                                                           int track_index,
                                                           const libmv_Region *region,
                                                           float **r_destination,
                                                           int *r_width,
                                                           int *r_height)
{
  /* Perform sanity checks first. */
  TrackingImageAccessor *accessor = (TrackingImageAccessor *)user_data;
  BLI_assert(clip_index < accessor->num_clips);
  BLI_assert(track_index < accessor->num_tracks);
  MovieTrackingTrack *track = accessor->tracks[track_index];
  /* Early output, track does not use mask. */
  if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) == 0) {
    return nullptr;
  }
  MovieClip *clip = accessor->clips[clip_index];
  /* Construct fake user so we can access movie clip. */
  MovieClipUser user;
  int scene_frame = BKE_movieclip_remap_clip_to_scene_frame(clip, frame);
  BKE_movieclip_user_set_frame(&user, scene_frame);
  user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
  user.render_flag = 0;
  /* Get frame width and height so we can convert stroke coordinates
   * and other things from normalized to pixel space.
   */
  int frame_width, frame_height;
  BKE_movieclip_get_size(clip, &user, &frame_width, &frame_height);
  /* Actual mask sampling. */
  MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, frame);
  const float region_min[2] = {
      region->min[0] - marker->pos[0] * frame_width,
      region->min[1] - marker->pos[1] * frame_height,
  };
  const float region_max[2] = {
      region->max[0] - marker->pos[0] * frame_width,
      region->max[1] - marker->pos[1] * frame_height,
  };
  *r_destination = tracking_track_get_mask_for_region(
      frame_width, frame_height, region_min, region_max, track);
  *r_width = region->max[0] - region->min[0];
  *r_height = region->max[1] - region->min[1];
  return *r_destination;
}

static void accessor_release_mask_callback(libmv_CacheKey cache_key)
{
  if (cache_key != nullptr) {
    float *mask = (float *)cache_key;
    MEM_freeN(mask);
  }
}

TrackingImageAccessor *tracking_image_accessor_new(MovieClip *clips[MAX_ACCESSOR_CLIP],
                                                   int num_clips,
                                                   MovieTrackingTrack **tracks,
                                                   int num_tracks)
{
  TrackingImageAccessor *accessor = MEM_callocN<TrackingImageAccessor>("tracking image accessor");

  BLI_assert(num_clips <= MAX_ACCESSOR_CLIP);

  memcpy(accessor->clips, clips, num_clips * sizeof(MovieClip *));
  accessor->num_clips = num_clips;

  accessor->tracks = MEM_calloc_arrayN<MovieTrackingTrack *>(num_tracks, "image accessor tracks");
  memcpy(accessor->tracks, tracks, num_tracks * sizeof(MovieTrackingTrack *));
  accessor->num_tracks = num_tracks;

  accessor->libmv_accessor = libmv_FrameAccessorNew((libmv_FrameAccessorUserData *)accessor,
                                                    accessor_get_image_callback,
                                                    accessor_release_image_callback,
                                                    accessor_get_mask_for_track_callback,
                                                    accessor_release_mask_callback);

  BLI_spin_init(&accessor->cache_lock);

  return accessor;
}

void tracking_image_accessor_destroy(TrackingImageAccessor *accessor)
{
  libmv_FrameAccessorDestroy(accessor->libmv_accessor);
  BLI_spin_end(&accessor->cache_lock);
  MEM_freeN(accessor->tracks);
  MEM_freeN(accessor);
}

/** \} */
