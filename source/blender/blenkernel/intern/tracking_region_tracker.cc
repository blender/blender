/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file contains implementation of blender-side region tracker
 * which is used for 2D feature tracking.
 */

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"

#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "libmv-capi.h"
#include "tracking_private.h"

/* **** utility functions for tracking **** */

/** Convert from float and byte RGBA to gray-scale. Supports different coefficients for RGB. */
static void float_rgba_to_gray(const float *rgba,
                               float *gray,
                               int num_pixels,
                               float weight_red,
                               float weight_green,
                               float weight_blue)
{
  for (int i = 0; i < num_pixels; i++) {
    const float *pixel = rgba + 4 * i;

    gray[i] = weight_red * pixel[0] + weight_green * pixel[1] + weight_blue * pixel[2];
  }
}

static void uint8_rgba_to_float_gray(const uchar *rgba,
                                     float *gray,
                                     int num_pixels,
                                     float weight_red,
                                     float weight_green,
                                     float weight_blue)
{
  for (int i = 0; i < num_pixels; i++) {
    const uchar *pixel = rgba + i * 4;

    gray[i] = (weight_red * pixel[0] + weight_green * pixel[1] + weight_blue * pixel[2]) / 255.0f;
  }
}

/** Get gray-scale float search buffer for given marker and frame. */
static float *track_get_search_floatbuf(ImBuf *ibuf,
                                        MovieTrackingTrack *track,
                                        MovieTrackingMarker *marker,
                                        int *r_width,
                                        int *r_height)
{
  ImBuf *searchibuf;
  float *gray_pixels;
  int width, height;

  searchibuf = BKE_tracking_get_search_imbuf(ibuf, track, marker, false, true);

  if (!searchibuf) {
    *r_width = 0;
    *r_height = 0;
    return nullptr;
  }

  width = searchibuf->x;
  height = searchibuf->y;

  gray_pixels = MEM_cnew_array<float>(width * height, "tracking floatBuf");

  if (searchibuf->float_buffer.data) {
    float_rgba_to_gray(
        searchibuf->float_buffer.data, gray_pixels, width * height, 0.2126f, 0.7152f, 0.0722f);
  }
  else {
    uint8_rgba_to_float_gray(
        searchibuf->byte_buffer.data, gray_pixels, width * height, 0.2126f, 0.7152f, 0.0722f);
  }

  IMB_freeImBuf(searchibuf);

  *r_width = width;
  *r_height = height;

  return gray_pixels;
}

/* Get image buffer for a given frame
 *
 * Frame is in clip space.
 */
static ImBuf *tracking_context_get_frame_ibuf(MovieClip *clip,
                                              MovieClipUser *user,
                                              int clip_flag,
                                              int framenr)
{
  ImBuf *ibuf;
  MovieClipUser new_user = *user;

  new_user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, framenr);

  ibuf = BKE_movieclip_get_ibuf_flag(clip, &new_user, clip_flag, MOVIECLIP_CACHE_SKIP);

  return ibuf;
}

/* Get image buffer for previous marker's keyframe. */
static ImBuf *tracking_context_get_keyframed_ibuf(MovieClip *clip,
                                                  MovieClipUser *user,
                                                  int clip_flag,
                                                  MovieTrackingTrack *track,
                                                  int curfra,
                                                  bool backwards,
                                                  MovieTrackingMarker **r_marker_keyed)
{
  MovieTrackingMarker *marker_keyed;
  int keyed_framenr;

  marker_keyed = tracking_get_keyframed_marker(track, curfra, backwards);
  if (marker_keyed == nullptr) {
    return nullptr;
  }

  keyed_framenr = marker_keyed->framenr;

  *r_marker_keyed = marker_keyed;

  return tracking_context_get_frame_ibuf(clip, user, clip_flag, keyed_framenr);
}

/* Get image buffer which is used as reference for track. */
static ImBuf *tracking_context_get_reference_ibuf(MovieClip *clip,
                                                  MovieClipUser *user,
                                                  int clip_flag,
                                                  MovieTrackingTrack *track,
                                                  int curfra,
                                                  bool backwards,
                                                  MovieTrackingMarker **reference_marker)
{
  ImBuf *ibuf = nullptr;

  if (track->pattern_match == TRACK_MATCH_KEYFRAME) {
    ibuf = tracking_context_get_keyframed_ibuf(
        clip, user, clip_flag, track, curfra, backwards, reference_marker);
  }
  else {
    ibuf = tracking_context_get_frame_ibuf(clip, user, clip_flag, curfra);

    /* use current marker as keyframed position */
    *reference_marker = BKE_tracking_marker_get(track, curfra);
  }

  return ibuf;
}

void tracking_configure_tracker(const MovieTrackingTrack *track,
                                float *mask,
                                const bool is_backwards,
                                libmv_TrackRegionOptions *options)
{
  options->direction = is_backwards ? LIBMV_TRACK_REGION_BACKWARD : LIBMV_TRACK_REGION_FORWARD;

  /* TODO(sergey): Use explicit conversion, so that options are decoupled between the Libmv library
   * and enumerator values in DNA. */
  options->motion_model = track->motion_model;

  options->use_brute = ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_BRUTE) != 0);

  options->use_normalization = ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_NORMALIZATION) !=
                                0);

  options->num_iterations = 50;
  options->minimum_correlation = track->minimum_correlation;
  options->sigma = 0.9;

  if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0) {
    options->image1_mask = mask;
  }
  else {
    options->image1_mask = nullptr;
  }
}

/* Perform tracking from a reference_marker to destination_ibuf.
 * Uses marker as an initial position guess.
 *
 * Returns truth if tracker returned success, puts result
 * to dst_pixel_x and dst_pixel_y.
 */
static bool configure_and_run_tracker(ImBuf *destination_ibuf,
                                      MovieTrackingTrack *track,
                                      MovieTrackingMarker *reference_marker,
                                      MovieTrackingMarker *marker,
                                      float *reference_search_area,
                                      int reference_search_area_width,
                                      int reference_search_area_height,
                                      float *mask,
                                      const bool is_backward,
                                      double dst_pixel_x[5],
                                      double dst_pixel_y[5])
{
  /* To convert to the x/y split array format for libmv. */
  double src_pixel_x[5], src_pixel_y[5];

  /* Settings for the tracker */
  libmv_TrackRegionOptions options = {};
  libmv_TrackRegionResult result;

  float *patch_new;

  int new_search_area_width, new_search_area_height;
  int frame_width, frame_height;

  bool tracked;

  frame_width = destination_ibuf->x;
  frame_height = destination_ibuf->y;

  /* for now track to the same search area dimension as marker has got for current frame
   * will make all tracked markers in currently tracked segment have the same search area
   * size, but it's quite close to what is actually needed
   */
  patch_new = track_get_search_floatbuf(
      destination_ibuf, track, marker, &new_search_area_width, &new_search_area_height);

  /* configure the tracker */
  tracking_configure_tracker(track, mask, is_backward, &options);

  /* Convert the marker corners and center into pixel coordinates in the
   * search/destination images. */
  tracking_get_marker_coords_for_tracking(
      frame_width, frame_height, reference_marker, src_pixel_x, src_pixel_y);
  tracking_get_marker_coords_for_tracking(
      frame_width, frame_height, marker, dst_pixel_x, dst_pixel_y);

  if (patch_new == nullptr || reference_search_area == nullptr) {
    return false;
  }

  /* run the tracker! */
  tracked = libmv_trackRegion(&options,
                              reference_search_area,
                              reference_search_area_width,
                              reference_search_area_height,
                              patch_new,
                              new_search_area_width,
                              new_search_area_height,
                              src_pixel_x,
                              src_pixel_y,
                              &result,
                              dst_pixel_x,
                              dst_pixel_y);

  MEM_freeN(patch_new);

  return tracked;
}

static bool refine_marker_reference_frame_get(MovieTrackingTrack *track,
                                              MovieTrackingMarker *marker,
                                              bool backwards,
                                              int *reference_framenr)
{
  const MovieTrackingMarker *first_marker = track->markers;
  const MovieTrackingMarker *last_marker = track->markers + track->markersnr - 1;
  MovieTrackingMarker *reference = backwards ? marker + 1 : marker - 1;

  while (reference >= first_marker && reference <= last_marker &&
         (reference->flag & MARKER_DISABLED) != 0)
  {
    if (backwards) {
      reference++;
    }
    else {
      reference--;
    }
  }

  if (reference < first_marker || reference > last_marker) {
    return false;
  }

  *reference_framenr = reference->framenr;
  return (reference->flag & MARKER_DISABLED) == 0;
}

void BKE_tracking_refine_marker(MovieClip *clip,
                                MovieTrackingTrack *track,
                                MovieTrackingMarker *marker,
                                bool backwards)
{
  MovieTrackingMarker *reference_marker = nullptr;
  ImBuf *reference_ibuf, *destination_ibuf;
  float *search_area, *mask = nullptr;
  int frame_width, frame_height;
  int search_area_height, search_area_width;
  int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
  int reference_framenr;
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  double dst_pixel_x[5], dst_pixel_y[5];
  bool tracked;

  /* Construct a temporary clip used, used to acquire image buffers. */
  user.framenr = BKE_movieclip_remap_clip_to_scene_frame(clip, marker->framenr);

  BKE_movieclip_get_size(clip, &user, &frame_width, &frame_height);

  /* Get an image buffer for reference frame, also gets reference marker. */
  if (!refine_marker_reference_frame_get(track, marker, backwards, &reference_framenr)) {
    return;
  }

  reference_ibuf = tracking_context_get_reference_ibuf(
      clip, &user, clip_flag, track, reference_framenr, backwards, &reference_marker);
  if (reference_ibuf == nullptr) {
    return;
  }

  /* Could not refine with self. */
  if (reference_marker == marker) {
    return;
  }

  /* Destination image buffer has got frame number corresponding to refining marker. */
  destination_ibuf = BKE_movieclip_get_ibuf_flag(clip, &user, clip_flag, MOVIECLIP_CACHE_SKIP);
  if (destination_ibuf == nullptr) {
    IMB_freeImBuf(reference_ibuf);
    return;
  }

  /* Get search area from reference image. */
  search_area = track_get_search_floatbuf(
      reference_ibuf, track, reference_marker, &search_area_width, &search_area_height);

  /* If needed, compute track's mask. */
  if ((track->algorithm_flag & TRACK_ALGORITHM_FLAG_USE_MASK) != 0) {
    mask = BKE_tracking_track_get_mask(frame_width, frame_height, track, marker);
  }

  /* Run the tracker from reference frame to current one. */
  tracked = configure_and_run_tracker(destination_ibuf,
                                      track,
                                      reference_marker,
                                      marker,
                                      search_area,
                                      search_area_width,
                                      search_area_height,
                                      mask,
                                      backwards,
                                      dst_pixel_x,
                                      dst_pixel_y);

  /* Refine current marker's position if track was successful. */
  if (tracked) {
    tracking_set_marker_coords_from_tracking(
        frame_width, frame_height, marker, dst_pixel_x, dst_pixel_y);
    marker->flag |= MARKER_TRACKED;
  }

  /* Free memory used for refining */
  MEM_freeN(search_area);
  if (mask) {
    MEM_freeN(mask);
  }
  IMB_freeImBuf(reference_ibuf);
  IMB_freeImBuf(destination_ibuf);
}
