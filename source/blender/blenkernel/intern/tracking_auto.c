/*
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
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include "atomic_ops.h"

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"
#include "DNA_object_types.h" /* SELECT */

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_math.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "libmv-capi.h"
#include "tracking_private.h"

typedef struct AutoTrackOptions {
  int clip_index;            /** Index of the clip this track belogs to. */
  int track_index;           /* Index of the track in AutoTrack tracks structure. */
  MovieTrackingTrack *track; /* Pointer to an original track/ */
  libmv_TrackRegionOptions track_region_options; /* Options for the region tracker. */
  bool use_keyframe_match;                       /* Keyframe pattern matching. */

  /* TODO(sergey): A bit awkward to keep it in here, only used to
   * place a disabled marker once the tracking fails,
   * Either find a clearer way to do it or call it track context
   * or state, not options.
   */
  bool is_failed;
  int failed_frame;
} AutoTrackOptions;

typedef struct AutoTrackContext {
  MovieClip *clips[MAX_ACCESSOR_CLIP];
  int num_clips;

  MovieClipUser user;
  int frame_width, frame_height;

  struct libmv_AutoTrack *autotrack;
  TrackingImageAccessor *image_accessor;

  int num_tracks;            /* Number of tracks being tracked. */
  AutoTrackOptions *options; /* Per-tracking track options. */

  /* Array of all tracks, indexed by track_index. */
  MovieTrackingTrack **tracks;

  bool backwards;
  bool sequence;
  int first_frame;
  int sync_frame;
  bool first_sync;
  SpinLock spin_lock;

  bool step_ok;
} AutoTrackContext;

static void normalized_to_libmv_frame(const float normalized[2],
                                      const int frame_dimensions[2],
                                      float result[2])
{
  result[0] = normalized[0] * frame_dimensions[0] - 0.5f;
  result[1] = normalized[1] * frame_dimensions[1] - 0.5f;
}

static void normalized_relative_to_libmv_frame(const float normalized[2],
                                               const float origin[2],
                                               const int frame_dimensions[2],
                                               float result[2])
{
  result[0] = (normalized[0] + origin[0]) * frame_dimensions[0] - 0.5f;
  result[1] = (normalized[1] + origin[1]) * frame_dimensions[1] - 0.5f;
}

static void libmv_frame_to_normalized(const float frame_coord[2],
                                      const int frame_dimensions[2],
                                      float result[2])
{
  result[0] = (frame_coord[0] + 0.5f) / frame_dimensions[0];
  result[1] = (frame_coord[1] + 0.5f) / frame_dimensions[1];
}

static void libmv_frame_to_normalized_relative(const float frame_coord[2],
                                               const float origin[2],
                                               const int frame_dimensions[2],
                                               float result[2])
{
  result[0] = (frame_coord[0] - origin[0]) / frame_dimensions[0];
  result[1] = (frame_coord[1] - origin[1]) / frame_dimensions[1];
}

static void dna_marker_to_libmv_marker(/*const*/ MovieTrackingTrack *track,
                                       /*const*/ MovieTrackingMarker *marker,
                                       int clip,
                                       int track_index,
                                       int frame_width,
                                       int frame_height,
                                       bool backwards,
                                       libmv_Marker *libmv_marker)
{
  const int frame_dimensions[2] = {frame_width, frame_height};
  int i;
  libmv_marker->clip = clip;
  libmv_marker->frame = marker->framenr;
  libmv_marker->track = track_index;

  normalized_to_libmv_frame(marker->pos, frame_dimensions, libmv_marker->center);
  for (i = 0; i < 4; ++i) {
    normalized_relative_to_libmv_frame(
        marker->pattern_corners[i], marker->pos, frame_dimensions, libmv_marker->patch[i]);
  }

  normalized_relative_to_libmv_frame(
      marker->search_min, marker->pos, frame_dimensions, libmv_marker->search_region_min);

  normalized_relative_to_libmv_frame(
      marker->search_max, marker->pos, frame_dimensions, libmv_marker->search_region_max);

  /* TODO(sergey): All the markers does have 1.0 weight. */
  libmv_marker->weight = 1.0f;

  if (marker->flag & MARKER_TRACKED) {
    libmv_marker->source = LIBMV_MARKER_SOURCE_TRACKED;
  }
  else {
    libmv_marker->source = LIBMV_MARKER_SOURCE_MANUAL;
  }
  libmv_marker->status = LIBMV_MARKER_STATUS_UNKNOWN;
  libmv_marker->model_type = LIBMV_MARKER_MODEL_TYPE_POINT;
  libmv_marker->model_id = 0;

  /* TODO(sergey): We currently don't support reference marker from
   * different clip.
   */
  libmv_marker->reference_clip = clip;

  if (track->pattern_match == TRACK_MATCH_KEYFRAME) {
    MovieTrackingMarker *keyframe_marker = tracking_get_keyframed_marker(
        track, marker->framenr, backwards);
    libmv_marker->reference_frame = keyframe_marker->framenr;
  }
  else {
    libmv_marker->reference_frame = backwards ? marker->framenr - 1 : marker->framenr;
  }

  libmv_marker->disabled_channels =
      ((track->flag & TRACK_DISABLE_RED) ? LIBMV_MARKER_CHANNEL_R : 0) |
      ((track->flag & TRACK_DISABLE_GREEN) ? LIBMV_MARKER_CHANNEL_G : 0) |
      ((track->flag & TRACK_DISABLE_BLUE) ? LIBMV_MARKER_CHANNEL_B : 0);
}

static void libmv_marker_to_dna_marker(libmv_Marker *libmv_marker,
                                       int frame_width,
                                       int frame_height,
                                       MovieTrackingMarker *marker)
{
  const int frame_dimensions[2] = {frame_width, frame_height};
  int i;
  marker->framenr = libmv_marker->frame;

  libmv_frame_to_normalized(libmv_marker->center, frame_dimensions, marker->pos);
  for (i = 0; i < 4; ++i) {
    libmv_frame_to_normalized_relative(libmv_marker->patch[i],
                                       libmv_marker->center,
                                       frame_dimensions,
                                       marker->pattern_corners[i]);
  }

  libmv_frame_to_normalized_relative(
      libmv_marker->search_region_min, libmv_marker->center, frame_dimensions, marker->search_min);

  libmv_frame_to_normalized_relative(
      libmv_marker->search_region_max, libmv_marker->center, frame_dimensions, marker->search_max);

  marker->flag = 0;
  if (libmv_marker->source == LIBMV_MARKER_SOURCE_TRACKED) {
    marker->flag |= MARKER_TRACKED;
  }
  else {
    marker->flag &= ~MARKER_TRACKED;
  }
}

static bool check_track_trackable(const MovieClip *clip,
                                  MovieTrackingTrack *track,
                                  const MovieClipUser *user)
{
  if (TRACK_SELECTED(track) && (track->flag & (TRACK_LOCKED | TRACK_HIDDEN)) == 0) {
    int frame = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, frame);
    return (marker->flag & MARKER_DISABLED) == 0;
  }
  return false;
}

/* Returns false if marker crossed margin area from frame bounds. */
static bool tracking_check_marker_margin(libmv_Marker *libmv_marker,
                                         int margin,
                                         int frame_width,
                                         int frame_height)
{
  float patch_min[2], patch_max[2];
  float margin_left, margin_top, margin_right, margin_bottom;

  INIT_MINMAX2(patch_min, patch_max);
  minmax_v2v2_v2(patch_min, patch_max, libmv_marker->patch[0]);
  minmax_v2v2_v2(patch_min, patch_max, libmv_marker->patch[1]);
  minmax_v2v2_v2(patch_min, patch_max, libmv_marker->patch[2]);
  minmax_v2v2_v2(patch_min, patch_max, libmv_marker->patch[3]);

  margin_left = max_ff(libmv_marker->center[0] - patch_min[0], margin);
  margin_top = max_ff(patch_max[1] - libmv_marker->center[1], margin);
  margin_right = max_ff(patch_max[0] - libmv_marker->center[0], margin);
  margin_bottom = max_ff(libmv_marker->center[1] - patch_min[1], margin);

  if (libmv_marker->center[0] < margin_left ||
      libmv_marker->center[0] > frame_width - margin_right ||
      libmv_marker->center[1] < margin_bottom ||
      libmv_marker->center[1] > frame_height - margin_top) {
    return false;
  }

  return true;
}

/* Provide Libmv side of auto track all information about given tracks. */
static void fill_autotrack_tracks(const int frame_width,
                                  const int frame_height,
                                  const ListBase *tracksbase,
                                  const bool backwards,
                                  struct libmv_AutoTrack *autotrack)
{
  /* Count number of markers to be put to a context. */
  size_t num_trackable_markers = 0;
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    for (int i = 0; i < track->markersnr; ++i) {
      const MovieTrackingMarker *marker = track->markers + i;
      if ((marker->flag & MARKER_DISABLED) == 0) {
        num_trackable_markers++;
      }
    }
  }
  /* Early output if we don't have any markers. */
  if (num_trackable_markers == 0) {
    return;
  }
  /* Allocate memory for all the markers. */
  libmv_Marker *libmv_markers = MEM_mallocN(sizeof(libmv_Marker) * num_trackable_markers,
                                            "libmv markers array");
  /* Fill in markers array. */
  int track_index = 0, num_filled_libmv_markers = 0;
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    for (int i = 0; i < track->markersnr; ++i) {
      MovieTrackingMarker *marker = track->markers + i;
      if ((marker->flag & MARKER_DISABLED) != 0) {
        continue;
      }
      dna_marker_to_libmv_marker(track,
                                 marker,
                                 0,
                                 track_index,
                                 frame_width,
                                 frame_height,
                                 backwards,
                                 &libmv_markers[num_filled_libmv_markers++]);
    }
    /* Put all markers to autotrack at once. */
    track_index++;
  }
  /* Add all markers to autotrack. */
  libmv_autoTrackSetMarkers(autotrack, libmv_markers, num_trackable_markers);
  /* Free temporary memory. */
  MEM_freeN(libmv_markers);
}

static void create_per_track_tracking_options(const MovieClip *clip,
                                              const MovieClipUser *user,
                                              const ListBase *tracksbase,
                                              AutoTrackContext *context)
{
  /* Count number of trackable tracks. */
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (check_track_trackable(clip, track, user)) {
      context->num_tracks++;
    }
  }
  /* Allocate required memory. */
  context->options = MEM_callocN(sizeof(AutoTrackOptions) * context->num_tracks,
                                 "auto track options");
  /* Fill in all the settings. */
  int i = 0, track_index = 0;
  for (MovieTrackingTrack *track = tracksbase->first; track != NULL; track = track->next) {
    if (!check_track_trackable(clip, track, user)) {
      ++track_index;
      continue;
    }
    AutoTrackOptions *options = &context->options[i++];
    /* TODO(sergey): Single clip only for now. */
    options->clip_index = 0;
    options->track_index = track_index;
    options->track = track;
    tracking_configure_tracker(track, NULL, &options->track_region_options);
    options->use_keyframe_match = track->pattern_match == TRACK_MATCH_KEYFRAME;
    context->tracks[track_index] = track;
    ++track_index;
  }
}

AutoTrackContext *BKE_autotrack_context_new(MovieClip *clip,
                                            MovieClipUser *user,
                                            const bool backwards,
                                            const bool sequence)
{
  AutoTrackContext *context = MEM_callocN(sizeof(AutoTrackContext), "autotrack context");
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int frame_width, frame_height;
  /* get size of frame to convert normalized coordinates to a picture ones. */
  BKE_movieclip_get_size(clip, user, &frame_width, &frame_height);
  /* TODO(sergey): Currently using only a single clip. */
  context->clips[0] = clip;
  context->num_clips = 1;
  context->user = *user;
  context->user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;
  context->user.render_flag = 0;
  context->frame_width = frame_width;
  context->frame_height = frame_height;
  context->backwards = backwards;
  context->sequence = sequence;
  context->first_frame = user->framenr;
  context->sync_frame = user->framenr;
  context->first_sync = true;
  BLI_spin_init(&context->spin_lock);
  const int num_total_tracks = BLI_listbase_count(tracksbase);
  context->tracks = MEM_callocN(sizeof(MovieTrackingTrack *) * num_total_tracks,
                                "auto track pointers");
  /* Initialize image accessor. */
  context->image_accessor = tracking_image_accessor_new(
      context->clips, 1, context->tracks, num_total_tracks, user->framenr);
  /* Initialize auto track context and provide all information about currently
   * tracked markers.
   */
  context->autotrack = libmv_autoTrackNew(context->image_accessor->libmv_accessor);
  fill_autotrack_tracks(frame_width, frame_height, tracksbase, backwards, context->autotrack);
  /* Create per-track tracking options. */
  create_per_track_tracking_options(clip, user, tracksbase, context);
  return context;
}

static void autotrack_context_step_cb(void *__restrict userdata,
                                      const int track,
                                      const TaskParallelTLS *__restrict UNUSED(tls))
{
  AutoTrackContext *context = userdata;
  const int frame_delta = context->backwards ? -1 : 1;

  AutoTrackOptions *options = &context->options[track];
  if (options->is_failed) {
    return;
  }
  libmv_Marker libmv_current_marker, libmv_reference_marker, libmv_tracked_marker;
  libmv_TrackRegionResult libmv_result;
  const int frame = BKE_movieclip_remap_scene_to_clip_frame(context->clips[options->clip_index],
                                                            context->user.framenr);
  BLI_spin_lock(&context->spin_lock);
  const bool has_marker = libmv_autoTrackGetMarker(
      context->autotrack, options->clip_index, frame, options->track_index, &libmv_current_marker);
  BLI_spin_unlock(&context->spin_lock);
  /* Check whether we've got marker to sync with. */
  if (!has_marker) {
    return;
  }
  /* Check whether marker is going outside of allowed frame margin. */
  if (!tracking_check_marker_margin(&libmv_current_marker,
                                    options->track->margin,
                                    context->frame_width,
                                    context->frame_height)) {
    return;
  }
  libmv_tracked_marker = libmv_current_marker;
  libmv_tracked_marker.frame = frame + frame_delta;
  /* Update reference frame. */
  if (options->use_keyframe_match) {
    libmv_tracked_marker.reference_frame = libmv_current_marker.reference_frame;
    libmv_autoTrackGetMarker(context->autotrack,
                             options->clip_index,
                             libmv_tracked_marker.reference_frame,
                             options->track_index,
                             &libmv_reference_marker);
  }
  else {
    libmv_tracked_marker.reference_frame = frame;
    libmv_reference_marker = libmv_current_marker;
  }
  /* Perform actual tracking. */
  if (libmv_autoTrackMarker(context->autotrack,
                            &options->track_region_options,
                            &libmv_tracked_marker,
                            &libmv_result)) {
    BLI_spin_lock(&context->spin_lock);
    libmv_autoTrackAddMarker(context->autotrack, &libmv_tracked_marker);
    BLI_spin_unlock(&context->spin_lock);
  }
  else {
    options->is_failed = true;
    options->failed_frame = frame + frame_delta;
  }

  /* Note: Atomic is probably not actually needed here, I doubt we could get
   *       any other result than a true bool anyway.
   *       But for sake of consistency, and since it costs nothing...
   */
  atomic_fetch_and_or_uint8((uint8_t *)&context->step_ok, true);
}

bool BKE_autotrack_context_step(AutoTrackContext *context)
{
  const int frame_delta = context->backwards ? -1 : 1;
  context->step_ok = false;

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (context->num_tracks > 1);
  BLI_task_parallel_range(0, context->num_tracks, context, autotrack_context_step_cb, &settings);

  /* Advance the frame. */
  BLI_spin_lock(&context->spin_lock);
  context->user.framenr += frame_delta;
  BLI_spin_unlock(&context->spin_lock);
  return context->step_ok;
}

void BKE_autotrack_context_sync(AutoTrackContext *context)
{
  int newframe, frame_delta = context->backwards ? -1 : 1;
  int frame;

  BLI_spin_lock(&context->spin_lock);
  newframe = context->user.framenr;
  for (frame = context->sync_frame; frame != (context->backwards ? newframe - 1 : newframe + 1);
       frame += frame_delta) {
    MovieTrackingMarker marker;
    libmv_Marker libmv_marker;
    int clip = 0;
    int track;
    for (track = 0; track < context->num_tracks; ++track) {
      AutoTrackOptions *options = &context->options[track];
      int track_frame = BKE_movieclip_remap_scene_to_clip_frame(
          context->clips[options->clip_index], frame);
      if (options->is_failed) {
        if (options->failed_frame == track_frame) {
          MovieTrackingMarker *prev_marker = BKE_tracking_marker_get_exact(
              options->track, context->backwards ? frame + 1 : frame - 1);
          if (prev_marker) {
            marker = *prev_marker;
            marker.framenr = track_frame;
            marker.flag |= MARKER_DISABLED;
            BKE_tracking_marker_insert(options->track, &marker);
            continue;
          }
        }
        if ((context->backwards && options->failed_frame > track_frame) ||
            (!context->backwards && options->failed_frame < track_frame)) {
          continue;
        }
      }
      if (libmv_autoTrackGetMarker(
              context->autotrack, clip, track_frame, options->track_index, &libmv_marker)) {
        libmv_marker_to_dna_marker(
            &libmv_marker, context->frame_width, context->frame_height, &marker);
        if (context->first_sync && frame == context->sync_frame) {
          tracking_marker_insert_disabled(options->track, &marker, !context->backwards, false);
        }
        BKE_tracking_marker_insert(options->track, &marker);
        tracking_marker_insert_disabled(options->track, &marker, context->backwards, false);
      }
    }
  }
  BLI_spin_unlock(&context->spin_lock);

  for (int clip = 0; clip < context->num_clips; ++clip) {
    MovieTracking *tracking = &context->clips[clip]->tracking;
    BKE_tracking_dopesheet_tag_update(tracking);
  }

  context->sync_frame = newframe;
  context->first_sync = false;
}

void BKE_autotrack_context_sync_user(AutoTrackContext *context, MovieClipUser *user)
{
  user->framenr = context->sync_frame;
}

void BKE_autotrack_context_finish(AutoTrackContext *context)
{
  int clip_index;

  for (clip_index = 0; clip_index < context->num_clips; ++clip_index) {
    MovieClip *clip = context->clips[clip_index];
    ListBase *plane_tracks_base = BKE_tracking_get_active_plane_tracks(&clip->tracking);
    MovieTrackingPlaneTrack *plane_track;

    for (plane_track = plane_tracks_base->first; plane_track; plane_track = plane_track->next) {
      if ((plane_track->flag & PLANE_TRACK_AUTOKEY) == 0) {
        int track;
        for (track = 0; track < context->num_tracks; ++track) {
          if (BKE_tracking_plane_track_has_point_track(plane_track,
                                                       context->options[track].track)) {
            BKE_tracking_track_plane_from_existing_motion(plane_track, context->first_frame);
            break;
          }
        }
      }
    }
  }
}

void BKE_autotrack_context_free(AutoTrackContext *context)
{
  libmv_autoTrackDestroy(context->autotrack);
  tracking_image_accessor_destroy(context->image_accessor);
  MEM_freeN(context->options);
  MEM_freeN(context->tracks);
  BLI_spin_end(&context->spin_lock);
  MEM_freeN(context);
}
