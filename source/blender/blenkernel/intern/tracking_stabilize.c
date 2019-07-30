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
 *
 * This file contains implementation of 2D image stabilization.
 */

#include <limits.h>

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"
#include "RNA_access.h"

#include "BLI_utildefines.h"
#include "BLI_sort_utils.h"
#include "BLI_ghash.h"
#include "BLI_math_vector.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_tracking.h"
#include "BKE_movieclip.h"
#include "BKE_fcurve.h"

#include "MEM_guardedalloc.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

/* == Parameterization constants == */

/* When measuring the scale changes relative to the rotation pivot point, it
 * might happen accidentally that a probe point (tracking point), which doesn't
 * actually move on a circular path, gets very close to the pivot point, causing
 * the measured scale contribution to go toward infinity. We damp this undesired
 * effect by adding a bias (floor) to the measured distances, which will
 * dominate very small distances and thus cause the corresponding track's
 * contribution to diminish.
 * Measurements happen in normalized (0...1) coordinates within a frame.
 */
static float SCALE_ERROR_LIMIT_BIAS = 0.01f;

/* When to consider a track as completely faded out.
 * This is used in conjunction with the "disabled" flag of the track
 * to determine start positions, end positions and gaps
 */
static float EPSILON_WEIGHT = 0.005f;

/* == private working data == */

/* Per track baseline for stabilization, defined at reference frame.
 * A track's reference frame is chosen as close as possible to the (global)
 * anchor_frame. Baseline holds the constant part of each track's contribution
 * to the observed movement; it is calculated at initialization pass, using the
 * measurement value at reference frame plus the average contribution to fill
 * the gap between global anchor_frame and the reference frame for this track.
 * This struct with private working data is associated to the local call context
 * via `StabContext::private_track_data`
 */
typedef struct TrackStabilizationBase {
  float stabilization_offset_base[2];

  /* measured relative to translated pivot */
  float stabilization_rotation_base[2][2];

  /* measured relative to translated pivot */
  float stabilization_scale_base;

  bool is_init_for_stabilization;
  FCurve *track_weight_curve;
} TrackStabilizationBase;

/* Tracks are reordered for initialization, starting as close as possible to
 * anchor_frame
 */
typedef struct TrackInitOrder {
  int sort_value;
  int reference_frame;
  MovieTrackingTrack *data;
} TrackInitOrder;

/* Per frame private working data, for accessing possibly animated values. */
typedef struct StabContext {
  MovieClip *clip;
  MovieTracking *tracking;
  MovieTrackingStabilization *stab;
  GHash *private_track_data;
  FCurve *locinf;
  FCurve *rotinf;
  FCurve *scaleinf;
  FCurve *target_pos[2];
  FCurve *target_rot;
  FCurve *target_scale;
  bool use_animation;
} StabContext;

static TrackStabilizationBase *access_stabilization_baseline_data(StabContext *ctx,
                                                                  MovieTrackingTrack *track)
{
  return BLI_ghash_lookup(ctx->private_track_data, track);
}

static void attach_stabilization_baseline_data(StabContext *ctx,
                                               MovieTrackingTrack *track,
                                               TrackStabilizationBase *private_data)
{
  BLI_ghash_insert(ctx->private_track_data, track, private_data);
}

static void discard_stabilization_baseline_data(void *val)
{
  if (val != NULL) {
    MEM_freeN(val);
  }
}

/* == access animated values for given frame == */

static FCurve *retrieve_stab_animation(MovieClip *clip, const char *data_path, int idx)
{
  return id_data_find_fcurve(&clip->id,
                             &clip->tracking.stabilization,
                             &RNA_MovieTrackingStabilization,
                             data_path,
                             idx,
                             NULL);
}

static FCurve *retrieve_track_weight_animation(MovieClip *clip, MovieTrackingTrack *track)
{
  return id_data_find_fcurve(&clip->id, track, &RNA_MovieTrackingTrack, "weight_stab", 0, NULL);
}

static float fetch_from_fcurve(FCurve *animationCurve,
                               int framenr,
                               StabContext *ctx,
                               float default_value)
{
  if (ctx && ctx->use_animation && animationCurve) {
    int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(ctx->clip, framenr);
    return evaluate_fcurve(animationCurve, scene_framenr);
  }
  return default_value;
}

static float get_animated_locinf(StabContext *ctx, int framenr)
{
  return fetch_from_fcurve(ctx->locinf, framenr, ctx, ctx->stab->locinf);
}

static float get_animated_rotinf(StabContext *ctx, int framenr)
{
  return fetch_from_fcurve(ctx->rotinf, framenr, ctx, ctx->stab->rotinf);
}

static float get_animated_scaleinf(StabContext *ctx, int framenr)
{
  return fetch_from_fcurve(ctx->scaleinf, framenr, ctx, ctx->stab->scaleinf);
}

static void get_animated_target_pos(StabContext *ctx, int framenr, float target_pos[2])
{
  target_pos[0] = fetch_from_fcurve(ctx->target_pos[0], framenr, ctx, ctx->stab->target_pos[0]);
  target_pos[1] = fetch_from_fcurve(ctx->target_pos[1], framenr, ctx, ctx->stab->target_pos[1]);
}

static float get_animated_target_rot(StabContext *ctx, int framenr)
{
  return fetch_from_fcurve(ctx->target_rot, framenr, ctx, ctx->stab->target_rot);
}

static float get_animated_target_scale(StabContext *ctx, int framenr)
{
  return fetch_from_fcurve(ctx->target_scale, framenr, ctx, ctx->stab->scale);
}

static float get_animated_weight(StabContext *ctx, MovieTrackingTrack *track, int framenr)
{
  TrackStabilizationBase *working_data = access_stabilization_baseline_data(ctx, track);
  if (working_data && working_data->track_weight_curve) {
    int scene_framenr = BKE_movieclip_remap_clip_to_scene_frame(ctx->clip, framenr);
    return evaluate_fcurve(working_data->track_weight_curve, scene_framenr);
  }
  /* Use weight at global 'current frame' as fallback default. */
  return track->weight_stab;
}

static void use_values_from_fcurves(StabContext *ctx, bool toggle)
{
  if (ctx != NULL) {
    ctx->use_animation = toggle;
  }
}

/* Prepare per call private working area.
 * Used for access to possibly animated values: retrieve available F-curves.
 */
static StabContext *initialize_stabilization_working_context(MovieClip *clip)
{
  StabContext *ctx = MEM_callocN(sizeof(StabContext), "2D stabilization animation runtime data");
  ctx->clip = clip;
  ctx->tracking = &clip->tracking;
  ctx->stab = &clip->tracking.stabilization;
  ctx->private_track_data = BLI_ghash_ptr_new("2D stabilization per track private working data");
  ctx->locinf = retrieve_stab_animation(clip, "influence_location", 0);
  ctx->rotinf = retrieve_stab_animation(clip, "influence_rotation", 0);
  ctx->scaleinf = retrieve_stab_animation(clip, "influence_scale", 0);
  ctx->target_pos[0] = retrieve_stab_animation(clip, "target_pos", 0);
  ctx->target_pos[1] = retrieve_stab_animation(clip, "target_pos", 1);
  ctx->target_rot = retrieve_stab_animation(clip, "target_rot", 0);
  ctx->target_scale = retrieve_stab_animation(clip, "target_zoom", 0);
  ctx->use_animation = true;
  return ctx;
}

/**
 * Discard all private working data attached to this call context.
 *
 * \note We allocate the record for the per track baseline contribution
 * locally for each call context (i.e. call to #BKE_tracking_stabilization_data_get)
 * Thus it is correct to discard all allocations found within the
 * corresponding _local_ GHash.
 */
static void discard_stabilization_working_context(StabContext *ctx)
{
  if (ctx != NULL) {
    BLI_ghash_free(ctx->private_track_data, NULL, discard_stabilization_baseline_data);
    MEM_freeN(ctx);
  }
}

static bool is_init_for_stabilization(StabContext *ctx, MovieTrackingTrack *track)
{
  TrackStabilizationBase *working_data = access_stabilization_baseline_data(ctx, track);
  return (working_data != NULL && working_data->is_init_for_stabilization);
}

static bool is_usable_for_stabilization(StabContext *ctx, MovieTrackingTrack *track)
{
  return (track->flag & TRACK_USE_2D_STAB) && is_init_for_stabilization(ctx, track);
}

static bool is_effectively_disabled(StabContext *ctx,
                                    MovieTrackingTrack *track,
                                    MovieTrackingMarker *marker)
{
  return (marker->flag & MARKER_DISABLED) ||
         (EPSILON_WEIGHT > get_animated_weight(ctx, track, marker->framenr));
}

static int search_closest_marker_index(MovieTrackingTrack *track, int ref_frame)
{
  MovieTrackingMarker *markers = track->markers;
  int end = track->markersnr;
  int i = track->last_marker;

  i = MAX2(0, i);
  i = MIN2(i, end - 1);
  for (; i < end - 1 && markers[i].framenr <= ref_frame; ++i) {
    /* pass */
  }
  for (; 0 < i && markers[i].framenr > ref_frame; --i) {
    /* pass */
  }

  track->last_marker = i;
  return i;
}

static void retrieve_next_higher_usable_frame(
    StabContext *ctx, MovieTrackingTrack *track, int i, int ref_frame, int *next_higher)
{
  MovieTrackingMarker *markers = track->markers;
  int end = track->markersnr;
  BLI_assert(0 <= i && i < end);

  while (i < end &&
         (markers[i].framenr < ref_frame || is_effectively_disabled(ctx, track, &markers[i]))) {
    ++i;
  }
  if (i < end && markers[i].framenr < *next_higher) {
    BLI_assert(markers[i].framenr >= ref_frame);
    *next_higher = markers[i].framenr;
  }
}

static void retrieve_next_lower_usable_frame(
    StabContext *ctx, MovieTrackingTrack *track, int i, int ref_frame, int *next_lower)
{
  MovieTrackingMarker *markers = track->markers;
  BLI_assert(0 <= i && i < track->markersnr);
  while (i >= 0 &&
         (markers[i].framenr > ref_frame || is_effectively_disabled(ctx, track, &markers[i]))) {
    --i;
  }
  if (0 <= i && markers[i].framenr > *next_lower) {
    BLI_assert(markers[i].framenr <= ref_frame);
    *next_lower = markers[i].framenr;
  }
}

/* Find closest frames with usable stabilization data.
 * A frame counts as _usable_ when there is at least one track marked for
 * translation stabilization, which has an enabled tracking marker at this very
 * frame. We search both for the next lower and next higher position, to allow
 * the caller to interpolate gaps and to extrapolate at the ends of the
 * definition range.
 *
 * NOTE: Regarding performance note that the individual tracks will cache the
 *       last search position.
 */
static void find_next_working_frames(StabContext *ctx,
                                     int framenr,
                                     int *next_lower,
                                     int *next_higher)
{
  for (MovieTrackingTrack *track = ctx->tracking->tracks.first; track != NULL;
       track = track->next) {
    if (is_usable_for_stabilization(ctx, track)) {
      int startpoint = search_closest_marker_index(track, framenr);
      retrieve_next_higher_usable_frame(ctx, track, startpoint, framenr, next_higher);
      retrieve_next_lower_usable_frame(ctx, track, startpoint, framenr, next_lower);
    }
  }
}

/* Find active (enabled) marker closest to the reference frame. */
static MovieTrackingMarker *get_closest_marker(StabContext *ctx,
                                               MovieTrackingTrack *track,
                                               int ref_frame)
{
  int next_lower = MINAFRAME;
  int next_higher = MAXFRAME;
  int i = search_closest_marker_index(track, ref_frame);
  retrieve_next_higher_usable_frame(ctx, track, i, ref_frame, &next_higher);
  retrieve_next_lower_usable_frame(ctx, track, i, ref_frame, &next_lower);

  if ((next_higher - ref_frame) < (ref_frame - next_lower)) {
    return BKE_tracking_marker_get_exact(track, next_higher);
  }
  else {
    return BKE_tracking_marker_get_exact(track, next_lower);
  }
}

/* Retrieve tracking data, if available and applicable for this frame.
 * The returned weight value signals the validity; data recorded for this
 * tracking marker on the exact requested frame is output with the full weight
 * of this track, while gaps in the data sequence cause the weight to go to zero.
 */
static MovieTrackingMarker *get_tracking_data_point(StabContext *ctx,
                                                    MovieTrackingTrack *track,
                                                    int framenr,
                                                    float *r_weight)
{
  MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, framenr);
  if (marker != NULL && !(marker->flag & MARKER_DISABLED)) {
    *r_weight = get_animated_weight(ctx, track, framenr);
    return marker;
  }
  else {
    /* No marker at this frame (=gap) or marker disabled. */
    *r_weight = 0.0f;
    return NULL;
  }
}

/* Define the reference point for rotation/scale measurement and compensation.
 * The stabilizator works by assuming the image was distorted by a affine linear
 * transform, i.e. it was rotated and stretched around this reference point
 * (pivot point) and then shifted laterally. Any scale and orientation changes
 * will be picked up relative to this point. And later the image will be
 * stabilized by rotating around this point. The result can only be as
 * accurate as this pivot point actually matches the real rotation center
 * of the actual movements. Thus any scheme to define a pivot point is
 * always guesswork.
 *
 * As a simple default, we use the weighted average of the location markers
 * of the current frame as pivot point. TODO It is planned to add further
 * options,  like e.g. anchoring the pivot point at the canvas. Moreover,
 * it is planned to allow for a user controllable offset.
 */
static void setup_pivot(const float ref_pos[2], float r_pivot[2])
{
  zero_v2(r_pivot); /* TODO: add an animated offset position here. */
  add_v2_v2(r_pivot, ref_pos);
}

/* Calculate the contribution of a single track at the time position (frame) of
 * the given marker. Each track has a local reference frame, which is as close
 * as possible to the global anchor_frame. Thus the translation contribution is
 * comprised of the offset relative to the image position at that reference
 * frame, plus a guess of the contribution for the time span between the
 * anchor_frame and the local reference frame of this track. The constant part
 * of this contribution is precomputed initially. At the anchor_frame, by
 * definition the contribution of all tracks is zero, keeping the frame in place.
 *
 * track_ref is per track baseline contribution at reference frame; filled in at
 *           initialization
 * marker is tracking data to use as contribution for current frame.
 * result_offset is a total cumulated contribution of this track,
 *               relative to the stabilization anchor_frame,
 *               in normalized (0...1) coordinates.
 */
static void translation_contribution(TrackStabilizationBase *track_ref,
                                     MovieTrackingMarker *marker,
                                     float result_offset[2])
{
  add_v2_v2v2(result_offset, track_ref->stabilization_offset_base, marker->pos);
}

/* Similar to the ::translation_contribution(), the rotation contribution is
 * comprised of the contribution by this individual track, and the averaged
 * contribution from anchor_frame to the ref point of this track.
 * - Contribution is in terms of angles, -pi < angle < +pi, and all averaging
 *   happens in this domain.
 * - Yet the actual measurement happens as vector between pivot and the current
 *   tracking point
 * - Currently we use the center of frame as approximation for the rotation pivot
 *   point.
 * - Moreover, the pivot point has to be compensated for the already determined
 *   shift offset, in order to get the pure rotation around the pivot.
 *   To turn this into a _contribution_, the likewise corrected angle at the
 *   reference frame has to be subtracted, to get only the pure angle difference
 *   this tracking point has captured.
 * - To get from vectors to angles, we have to go through an arcus tangens,
 *   which involves the issue of the definition range: the resulting angles will
 *   flip by 360deg when the measured vector passes from the 2nd to the third
 *   quadrant, thus messing up the average calculation. Since _any_ tracking
 *   point might be used, these problems are quite common in practice.
 * - Thus we perform the subtraction of the reference and the addition of the
 *   baseline contribution in polar coordinates as simple addition of angles;
 *   since these parts are fixed, we can bake them into a rotation matrix.
 *   With this approach, the border of the arcus tangens definition range will
 *   be reached only, when the _whole_ contribution approaches +- 180deg,
 *   meaning we've already tilted the frame upside down. This situation is way
 *   less common and can be tolerated.
 * - As an additional feature, when activated, also changes in image scale
 *   relative to the rotation center can be picked up. To handle those values
 *   in the same framework, we average the scales as logarithms.
 *
 * aspect is a total aspect ratio of the undistorted image (includes fame and
 * pixel aspect). The function returns a quality factor, which can be used
 * to damp the contributions of points in close proximity to the pivot point,
 * since such contributions might be dominated by rounding errors and thus
 * poison the calculated average. When the quality factor goes towards zero,
 * the weight of this contribution should be reduced accordingly.
 */
static float rotation_contribution(TrackStabilizationBase *track_ref,
                                   MovieTrackingMarker *marker,
                                   const float aspect,
                                   const float pivot[2],
                                   float *result_angle,
                                   float *result_scale)
{
  float len, quality;
  float pos[2];
  sub_v2_v2v2(pos, marker->pos, pivot);

  pos[0] *= aspect;
  mul_m2v2(track_ref->stabilization_rotation_base, pos);

  *result_angle = atan2f(pos[1], pos[0]);

  len = len_v2(pos);

  /* prevent points very close to the pivot point from poisoning the result */
  quality = 1 - expf(-len * len / SCALE_ERROR_LIMIT_BIAS * SCALE_ERROR_LIMIT_BIAS);
  len += SCALE_ERROR_LIMIT_BIAS;

  *result_scale = len * track_ref->stabilization_scale_base;
  BLI_assert(0.0 < *result_scale);

  return quality;
}

/* Workaround to allow for rotation around an arbitrary pivot point.
 * Currently, the public API functions do not support this flexibility.
 * Rather, rotation will always be applied around a fixed origin.
 * As a workaround, we shift the image after rotation to match the
 * desired rotation center. And since this offset needs to be applied
 * after the rotation and scaling, we can collapse it with the
 * translation compensation, which is also a lateral shift (offset).
 * The offset to apply is intended_pivot - rotated_pivot
 */
static void compensate_rotation_center(const int size,
                                       float aspect,
                                       const float angle,
                                       const float scale,
                                       const float pivot[2],
                                       float result_translation[2])
{
  const float origin[2] = {0.5f * aspect * size, 0.5f * size};
  float intended_pivot[2], rotated_pivot[2];
  float rotation_mat[2][2];

  copy_v2_v2(intended_pivot, pivot);
  copy_v2_v2(rotated_pivot, pivot);
  angle_to_mat2(rotation_mat, +angle);
  sub_v2_v2(rotated_pivot, origin);
  mul_m2v2(rotation_mat, rotated_pivot);
  mul_v2_fl(rotated_pivot, scale);
  add_v2_v2(rotated_pivot, origin);
  add_v2_v2(result_translation, intended_pivot);
  sub_v2_v2(result_translation, rotated_pivot);
}

/* Weighted average of the per track cumulated contributions at given frame.
 * Returns truth if all desired calculations could be done and all averages are
 * available.
 *
 * NOTE: Even if the result is not `true`, the returned translation and angle
 *       are always sensible and as good as can be. Especially in the
 *       initialization phase we might not be able to get any average (yet) or
 *       get only a translation value. Since initialization visits tracks in a
 *       specific order, starting from anchor_frame, the result is logically
 *       correct non the less. But under normal operation conditions,
 *       a result of `false` should disable the stabilization function
 */
static bool average_track_contributions(StabContext *ctx,
                                        int framenr,
                                        float aspect,
                                        float r_translation[2],
                                        float r_pivot[2],
                                        float *r_angle,
                                        float *r_scale_step)
{
  bool ok;
  float weight_sum;
  MovieTrackingTrack *track;
  MovieTracking *tracking = ctx->tracking;
  MovieTrackingStabilization *stab = &tracking->stabilization;
  float ref_pos[2];
  BLI_assert(stab->flag & TRACKING_2D_STABILIZATION);

  zero_v2(r_translation);
  *r_scale_step = 0.0f; /* logarithm */
  *r_angle = 0.0f;

  zero_v2(ref_pos);

  ok = false;
  weight_sum = 0.0f;
  for (track = tracking->tracks.first; track; track = track->next) {
    if (!is_init_for_stabilization(ctx, track)) {
      continue;
    }
    if (track->flag & TRACK_USE_2D_STAB) {
      float weight = 0.0f;
      MovieTrackingMarker *marker = get_tracking_data_point(ctx, track, framenr, &weight);
      if (marker) {
        TrackStabilizationBase *stabilization_base = access_stabilization_baseline_data(ctx,
                                                                                        track);
        BLI_assert(stabilization_base != NULL);
        float offset[2];
        weight_sum += weight;
        translation_contribution(stabilization_base, marker, offset);
        r_translation[0] += weight * offset[0];
        r_translation[1] += weight * offset[1];
        ref_pos[0] += weight * marker->pos[0];
        ref_pos[1] += weight * marker->pos[1];
        ok |= (weight_sum > EPSILON_WEIGHT);
      }
    }
  }
  if (!ok) {
    return false;
  }

  ref_pos[0] /= weight_sum;
  ref_pos[1] /= weight_sum;
  r_translation[0] /= weight_sum;
  r_translation[1] /= weight_sum;
  setup_pivot(ref_pos, r_pivot);

  if (!(stab->flag & TRACKING_STABILIZE_ROTATION)) {
    return ok;
  }

  ok = false;
  weight_sum = 0.0f;
  for (track = tracking->tracks.first; track; track = track->next) {
    if (!is_init_for_stabilization(ctx, track)) {
      continue;
    }
    if (track->flag & TRACK_USE_2D_STAB_ROT) {
      float weight = 0.0f;
      MovieTrackingMarker *marker = get_tracking_data_point(ctx, track, framenr, &weight);
      if (marker) {
        TrackStabilizationBase *stabilization_base = access_stabilization_baseline_data(ctx,
                                                                                        track);
        BLI_assert(stabilization_base != NULL);
        float rotation, scale, quality;
        quality = rotation_contribution(
            stabilization_base, marker, aspect, r_pivot, &rotation, &scale);
        const float quality_weight = weight * quality;
        weight_sum += quality_weight;
        *r_angle += rotation * quality_weight;
        if (stab->flag & TRACKING_STABILIZE_SCALE) {
          *r_scale_step += logf(scale) * quality_weight;
        }
        else {
          *r_scale_step = 0;
        }
        /* NOTE: Use original marker weight and not the scaled one with the proximity here to allow
         * simple stabilization setups when there is a single track in a close proximity of the
         * center. */
        ok |= (weight > EPSILON_WEIGHT);
      }
    }
  }
  if (ok) {
    *r_scale_step /= weight_sum;
    *r_angle /= weight_sum;
  }
  else {
    /* We reach this point because translation could be calculated,
     * but rotation/scale found no data to work on.
     */
    *r_scale_step = 0.0f;
    *r_angle = 0.0f;
  }
  return true;
}

/* Calculate weight center of location tracks for given frame.
 * This function performs similar calculations as average_track_contributions(),
 * but does not require the tracks to be initialized for stabilization. Moreover,
 * when there is no usable tracking data for the given frame number, data from
 * a neighboring frame is used. Thus this function can be used to calculate
 * a starting point on initialization.
 */
static void average_marker_positions(StabContext *ctx, int framenr, float r_ref_pos[2])
{
  bool ok = false;
  float weight_sum;
  MovieTrackingTrack *track;
  MovieTracking *tracking = ctx->tracking;

  zero_v2(r_ref_pos);
  weight_sum = 0.0f;
  for (track = tracking->tracks.first; track; track = track->next) {
    if (track->flag & TRACK_USE_2D_STAB) {
      float weight = 0.0f;
      MovieTrackingMarker *marker = get_tracking_data_point(ctx, track, framenr, &weight);
      if (marker) {
        weight_sum += weight;
        r_ref_pos[0] += weight * marker->pos[0];
        r_ref_pos[1] += weight * marker->pos[1];
        ok |= (weight_sum > EPSILON_WEIGHT);
      }
    }
  }
  if (ok) {
    r_ref_pos[0] /= weight_sum;
    r_ref_pos[1] /= weight_sum;
  }
  else {
    /* No usable tracking data on any track on this frame.
     * Use data from neighboring frames to extrapolate...
     */
    int next_lower = MINAFRAME;
    int next_higher = MAXFRAME;
    use_values_from_fcurves(ctx, true);
    for (track = tracking->tracks.first; track; track = track->next) {
      /* Note: we deliberately do not care if this track
       *       is already initialized for stabilization. */
      if (track->flag & TRACK_USE_2D_STAB) {
        int startpoint = search_closest_marker_index(track, framenr);
        retrieve_next_higher_usable_frame(ctx, track, startpoint, framenr, &next_higher);
        retrieve_next_lower_usable_frame(ctx, track, startpoint, framenr, &next_lower);
      }
    }
    if (next_lower >= MINFRAME) {
      /* use next usable frame to the left.
       * Also default to this frame when we're in a gap */
      average_marker_positions(ctx, next_lower, r_ref_pos);
    }
    else if (next_higher < MAXFRAME) {
      average_marker_positions(ctx, next_higher, r_ref_pos);
    }
    use_values_from_fcurves(ctx, false);
  }
}

/* Linear interpolation of data retrieved at two measurement points.
 * This function is used to fill gaps in the middle of the covered area,
 * at frames without any usable tracks for stabilization.
 *
 * framenr is a position to interpolate for.
 * frame_a is a valid measurement point below framenr
 * frame_b is a valid measurement point above framenr
 * Returns truth if both measurements could actually be retrieved.
 * Otherwise output parameters remain unaltered
 */
static bool interpolate_averaged_track_contributions(StabContext *ctx,
                                                     int framenr,
                                                     int frame_a,
                                                     int frame_b,
                                                     const float aspect,
                                                     float r_translation[2],
                                                     float r_pivot[2],
                                                     float *r_angle,
                                                     float *r_scale_step)
{
  float t, s;
  float trans_a[2], trans_b[2];
  float angle_a, angle_b;
  float scale_a, scale_b;
  float pivot_a[2], pivot_b[2];
  bool success = false;

  BLI_assert(frame_a <= frame_b);
  BLI_assert(frame_a <= framenr);
  BLI_assert(framenr <= frame_b);

  t = ((float)framenr - frame_a) / (frame_b - frame_a);
  s = 1.0f - t;

  success = average_track_contributions(
      ctx, frame_a, aspect, trans_a, pivot_a, &angle_a, &scale_a);
  if (!success) {
    return false;
  }
  success = average_track_contributions(
      ctx, frame_b, aspect, trans_b, pivot_b, &angle_b, &scale_b);
  if (!success) {
    return false;
  }

  interp_v2_v2v2(r_translation, trans_a, trans_b, t);
  interp_v2_v2v2(r_pivot, pivot_a, pivot_b, t);
  *r_scale_step = s * scale_a + t * scale_b;
  *r_angle = s * angle_a + t * angle_b;
  return true;
}

/* Reorder tracks starting with those providing a tracking data frame
 * closest to the global anchor_frame. Tracks with a gap at anchor_frame or
 * starting farer away from anchor_frame altogether will be visited later.
 * This allows to build up baseline contributions incrementally.
 *
 * order is an array for sorting the tracks. Must be of suitable size to hold
 * all tracks.
 * Returns number of actually usable tracks, can be less than the overall number
 * of tracks.
 *
 * NOTE: After returning, the order array holds entries up to the number of
 *       usable tracks, appropriately sorted starting with the closest tracks.
 *       Initialization includes disabled tracks, since they might be enabled
 *       through automation later.
 */
static int establish_track_initialization_order(StabContext *ctx, TrackInitOrder *order)
{
  size_t tracknr = 0;
  MovieTrackingTrack *track;
  MovieTracking *tracking = ctx->tracking;
  int anchor_frame = tracking->stabilization.anchor_frame;

  for (track = tracking->tracks.first; track != NULL; track = track->next) {
    MovieTrackingMarker *marker;
    order[tracknr].data = track;
    marker = get_closest_marker(ctx, track, anchor_frame);
    if (marker != NULL && (track->flag & (TRACK_USE_2D_STAB | TRACK_USE_2D_STAB_ROT))) {
      order[tracknr].sort_value = abs(marker->framenr - anchor_frame);
      order[tracknr].reference_frame = marker->framenr;
      ++tracknr;
    }
  }
  if (tracknr) {
    qsort(order, tracknr, sizeof(TrackInitOrder), BLI_sortutil_cmp_int);
  }
  return tracknr;
}

/* Setup the constant part of this track's contribution to the determined frame
 * movement. Tracks usually don't provide tracking data for every frame. Thus,
 * for determining data at a given frame, we split up the contribution into a
 * part covered by actual measurements on this track, and the initial gap
 * between this track's reference frame and the global anchor_frame.
 * The (missing) data for the gap can be substituted by the average offset
 * observed by the other tracks covering the gap. This approximation doesn't
 * introduce wrong data, but it records data with incorrect weight. A totally
 * correct solution would require us to average the contribution per frame, and
 * then integrate stepwise over all frames -- which of course would be way more
 * expensive, especially for longer clips. To the contrary, our solution
 * cumulates the total contribution per track and averages afterwards over all
 * tracks; it can thus be calculated just based on the data of a single frame,
 * plus the "baseline" for the reference frame, which is what we are computing
 * here.
 *
 * Since we're averaging _contributions_, we have to calculate the _difference_
 * of the measured position at current frame and the position at the reference
 * frame. But the "reference" part of this difference is constant and can thus
 * be packed together with the baseline contribution into a single precomputed
 * vector per track.
 *
 * In case of the rotation contribution, the principle is the same, but we have
 * to compensate for the already determined translation and measure the pure
 * rotation, simply because this is how we model the offset: shift plus rotation
 * around the shifted rotation center. To circumvent problems with the
 * definition range of the arcus tangens function, we perform this baseline
 * addition and reference angle subtraction in polar coordinates and bake this
 * operation into a precomputed rotation matrix.
 *
 * track is a track to be initialized to initialize
 * reference_frame is a local frame for this track, the closest pick to the
 *                 global anchor_frame.
 * aspect is a total aspect ratio of the undistorted image (includes fame and
 *        pixel aspect).
 * target_pos is a possibly animated target position as set by the user for
 *            the reference_frame
 * average_translation is a value observed by the _other_ tracks for the gap
 *                     between reference_frame and anchor_frame. This
 *                     average must not contain contributions of frames
 *                     not yet initialized
 * average_angle in a similar way, the rotation value observed by the
 *               _other_ tracks.
 * average_scale_step is an image scale factor observed on average by the other
 *                    tracks for this frame. This value is recorded and
 *                    averaged as logarithm. The recorded scale changes
 *                    are damped for very small contributions, to limit
 *                    the effect of probe points approaching the pivot
 *                    too closely.
 *
 * NOTE: when done, this track is marked as initialized
 */
static void initialize_track_for_stabilization(StabContext *ctx,
                                               MovieTrackingTrack *track,
                                               int reference_frame,
                                               float aspect,
                                               const float average_translation[2],
                                               const float pivot[2],
                                               const float average_angle,
                                               const float average_scale_step)
{
  float pos[2], angle, len;
  TrackStabilizationBase *local_data = access_stabilization_baseline_data(ctx, track);
  MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, reference_frame);
  /* Logic for initialization order ensures there *is* a marker on that
   * very frame.
   */
  BLI_assert(marker != NULL);
  BLI_assert(local_data != NULL);

  /* Per track baseline value for translation. */
  sub_v2_v2v2(local_data->stabilization_offset_base, average_translation, marker->pos);

  /* Per track baseline value for rotation. */
  sub_v2_v2v2(pos, marker->pos, pivot);

  pos[0] *= aspect;
  angle = average_angle - atan2f(pos[1], pos[0]);
  angle_to_mat2(local_data->stabilization_rotation_base, angle);

  /* Per track baseline value for zoom. */
  len = len_v2(pos) + SCALE_ERROR_LIMIT_BIAS;
  local_data->stabilization_scale_base = expf(average_scale_step) / len;

  local_data->is_init_for_stabilization = true;
}

static void initialize_all_tracks(StabContext *ctx, float aspect)
{
  size_t i, track_len = 0;
  MovieClip *clip = ctx->clip;
  MovieTracking *tracking = ctx->tracking;
  MovieTrackingTrack *track;
  TrackInitOrder *order;

  /* Attempt to start initialization at anchor_frame.
   * By definition, offset contribution is zero there.
   */
  int reference_frame = tracking->stabilization.anchor_frame;
  float average_angle = 0, average_scale_step = 0;
  float average_translation[2], average_pos[2], pivot[2];
  zero_v2(average_translation);
  zero_v2(pivot);

  /* Initialize private working data. */
  for (track = tracking->tracks.first; track != NULL; track = track->next) {
    TrackStabilizationBase *local_data = access_stabilization_baseline_data(ctx, track);
    if (!local_data) {
      local_data = MEM_callocN(sizeof(TrackStabilizationBase),
                               "2D stabilization per track baseline data");
      attach_stabilization_baseline_data(ctx, track, local_data);
    }
    BLI_assert(local_data != NULL);
    local_data->track_weight_curve = retrieve_track_weight_animation(clip, track);
    local_data->is_init_for_stabilization = false;

    ++track_len;
  }
  if (!track_len) {
    return;
  }

  order = MEM_mallocN(track_len * sizeof(TrackInitOrder), "stabilization track order");
  if (!order) {
    return;
  }

  track_len = establish_track_initialization_order(ctx, order);
  if (track_len == 0) {
    goto cleanup;
  }

  /* starting point for pivot, before having initialized any track */
  average_marker_positions(ctx, reference_frame, average_pos);
  setup_pivot(average_pos, pivot);

  for (i = 0; i < track_len; ++i) {
    track = order[i].data;
    if (reference_frame != order[i].reference_frame) {
      reference_frame = order[i].reference_frame;
      average_track_contributions(ctx,
                                  reference_frame,
                                  aspect,
                                  average_translation,
                                  pivot,
                                  &average_angle,
                                  &average_scale_step);
    }
    initialize_track_for_stabilization(ctx,
                                       track,
                                       reference_frame,
                                       aspect,
                                       average_translation,
                                       pivot,
                                       average_angle,
                                       average_scale_step);
  }

cleanup:
  MEM_freeN(order);
}

/* Retrieve the measurement of frame movement by averaging contributions of
 * active tracks.
 *
 * translation is a measurement in normalized 0..1 coordinates.
 * angle is a measurement in radians -pi..+pi counter clockwise relative to
 *       translation compensated frame center
 * scale_step is a measurement of image scale changes, in logarithmic scale
 *            (zero means scale == 1)
 * Returns calculation enabled and all data retrieved as expected for this frame.
 *
 * NOTE: when returning `false`, output parameters are reset to neutral values.
 */
static bool stabilization_determine_offset_for_frame(StabContext *ctx,
                                                     int framenr,
                                                     float aspect,
                                                     float r_translation[2],
                                                     float r_pivot[2],
                                                     float *r_angle,
                                                     float *r_scale_step)
{
  bool success = false;

  /* Early output if stabilization is disabled. */
  if ((ctx->stab->flag & TRACKING_2D_STABILIZATION) == 0) {
    zero_v2(r_translation);
    *r_scale_step = 0.0f;
    *r_angle = 0.0f;
    return false;
  }

  success = average_track_contributions(
      ctx, framenr, aspect, r_translation, r_pivot, r_angle, r_scale_step);
  if (!success) {
    /* Try to hold extrapolated settings beyond the definition range
     * and to interpolate in gaps without any usable tracking data
     * to prevent sudden jump to image zero position.
     */
    int next_lower = MINAFRAME;
    int next_higher = MAXFRAME;
    use_values_from_fcurves(ctx, true);
    find_next_working_frames(ctx, framenr, &next_lower, &next_higher);
    if (next_lower >= MINFRAME && next_higher < MAXFRAME) {
      success = interpolate_averaged_track_contributions(ctx,
                                                         framenr,
                                                         next_lower,
                                                         next_higher,
                                                         aspect,
                                                         r_translation,
                                                         r_pivot,
                                                         r_angle,
                                                         r_scale_step);
    }
    else if (next_higher < MAXFRAME) {
      /* Before start of stabilized range: extrapolate start point
       * settings.
       */
      success = average_track_contributions(
          ctx, next_higher, aspect, r_translation, r_pivot, r_angle, r_scale_step);
    }
    else if (next_lower >= MINFRAME) {
      /* After end of stabilized range: extrapolate end point settings. */
      success = average_track_contributions(
          ctx, next_lower, aspect, r_translation, r_pivot, r_angle, r_scale_step);
    }
    use_values_from_fcurves(ctx, false);
  }
  return success;
}

/* Calculate stabilization data (translation, scale and rotation) from given raw
 * measurements. Result is in absolute image dimensions (expanded image, square
 * pixels), includes automatic or manual scaling and compensates for a target
 * frame position, if given.
 *
 * size is a size of the expanded image, the width in pixels is size * aspect.
 * aspect is a ratio (width / height) of the effective canvas (square pixels).
 * do_compensate denotes whether to actually output values necessary to
 *               _compensate_ the determined frame movement.
 *               Otherwise, the effective target movement is returned.
 */
static void stabilization_calculate_data(StabContext *ctx,
                                         int framenr,
                                         int size,
                                         float aspect,
                                         bool do_compensate,
                                         float scale_step,
                                         float r_translation[2],
                                         float r_pivot[2],
                                         float *r_scale,
                                         float *r_angle)
{
  float target_pos[2], target_scale;
  float scaleinf = get_animated_scaleinf(ctx, framenr);

  if (ctx->stab->flag & TRACKING_STABILIZE_SCALE) {
    *r_scale = expf(scale_step * scaleinf); /* Averaged in log scale */
  }
  else {
    *r_scale = 1.0f;
  }

  mul_v2_fl(r_translation, get_animated_locinf(ctx, framenr));
  *r_angle *= get_animated_rotinf(ctx, framenr);

  /* Compensate for a target frame position.
   * This allows to follow tracking / panning shots in a semi manual fashion,
   * when animating the settings for the target frame position.
   */
  get_animated_target_pos(ctx, framenr, target_pos);
  sub_v2_v2(r_translation, target_pos);
  *r_angle -= get_animated_target_rot(ctx, framenr);
  target_scale = get_animated_target_scale(ctx, framenr);
  if (target_scale != 0.0f) {
    *r_scale /= target_scale;
    /* target_scale is an expected/intended reference zoom value */
  }

  /* Convert from relative to absolute coordinates, square pixels. */
  r_translation[0] *= (float)size * aspect;
  r_translation[1] *= (float)size;
  r_pivot[0] *= (float)size * aspect;
  r_pivot[1] *= (float)size;

  /* Output measured data, or inverse of the measured values for
   * compensation?
   */
  if (do_compensate) {
    mul_v2_fl(r_translation, -1.0f);
    *r_angle *= -1.0f;
    if (*r_scale != 0.0f) {
      *r_scale = 1.0f / *r_scale;
    }
  }
}

static void stabilization_data_to_mat4(float pixel_aspect,
                                       const float pivot[2],
                                       const float translation[2],
                                       float scale,
                                       float angle,
                                       float r_mat[4][4])
{
  float translation_mat[4][4], rotation_mat[4][4], scale_mat[4][4], pivot_mat[4][4],
      inv_pivot_mat[4][4], aspect_mat[4][4], inv_aspect_mat[4][4];
  float scale_vector[3] = {scale, scale, 1.0f};

  unit_m4(translation_mat);
  unit_m4(rotation_mat);
  unit_m4(scale_mat);
  unit_m4(aspect_mat);
  unit_m4(pivot_mat);
  unit_m4(inv_pivot_mat);

  /* aspect ratio correction matrix */
  aspect_mat[0][0] /= pixel_aspect;
  invert_m4_m4(inv_aspect_mat, aspect_mat);

  add_v2_v2(pivot_mat[3], pivot);
  sub_v2_v2(inv_pivot_mat[3], pivot);

  size_to_mat4(scale_mat, scale_vector);      /* scale matrix */
  add_v2_v2(translation_mat[3], translation); /* translation matrix */
  rotate_m4(rotation_mat, 'Z', angle);        /* rotation matrix */

  /* Compose transformation matrix. */
  mul_m4_series(r_mat,
                aspect_mat,
                translation_mat,
                pivot_mat,
                scale_mat,
                rotation_mat,
                inv_pivot_mat,
                inv_aspect_mat);
}

/* Calculate scale factor necessary to eliminate black image areas
 * caused by the compensating movements of the stabilizator.
 * This function visits every frame where stabilisation data is
 * available and determines the factor for this frame. The overall
 * largest factor found is returned as result.
 *
 * NOTE: all tracks need to be initialized before calling this function.
 */
static float calculate_autoscale_factor(StabContext *ctx, int size, float aspect)
{
  MovieTrackingStabilization *stab = ctx->stab;
  float pixel_aspect = ctx->tracking->camera.pixel_aspect;
  int height = size, width = aspect * size;

  int sfra = INT_MAX, efra = INT_MIN, cfra;
  float scale = 1.0f, scale_step = 0.0f;
  MovieTrackingTrack *track;

  /* Calculate maximal frame range of tracks where stabilization is active. */
  for (track = ctx->tracking->tracks.first; track; track = track->next) {
    if ((track->flag & TRACK_USE_2D_STAB) ||
        ((stab->flag & TRACKING_STABILIZE_ROTATION) && (track->flag & TRACK_USE_2D_STAB_ROT))) {
      int first_frame = track->markers[0].framenr;
      int last_frame = track->markers[track->markersnr - 1].framenr;
      sfra = min_ii(sfra, first_frame);
      efra = max_ii(efra, last_frame);
    }
  }

  use_values_from_fcurves(ctx, true);
  for (cfra = sfra; cfra <= efra; cfra++) {
    float translation[2], pivot[2], angle, tmp_scale;
    float mat[4][4];
    const float points[4][2] = {{0.0f, 0.0f}, {0.0f, height}, {width, height}, {width, 0.0f}};
    const bool do_compensate = true;
    /* Calculate stabilization parameters for the current frame. */
    stabilization_determine_offset_for_frame(
        ctx, cfra, aspect, translation, pivot, &angle, &scale_step);
    stabilization_calculate_data(ctx,
                                 cfra,
                                 size,
                                 aspect,
                                 do_compensate,
                                 scale_step,
                                 translation,
                                 pivot,
                                 &tmp_scale,
                                 &angle);
    /* Compose transformation matrix. */
    /* NOTE: Here we operate in NON-COMPENSATED coordinates, meaning we have
     * to construct transformation matrix using proper pivot point.
     * Compensation for that will happen later on.
     */
    stabilization_data_to_mat4(pixel_aspect, pivot, translation, tmp_scale, angle, mat);
    /* Investigate the transformed border lines for this frame;
     * find out, where it cuts the original frame.
     */
    for (int edge_index = 0; edge_index < 4; edge_index++) {
      /* Calculate coordinates of stabilized frame edge points.
       * Use matrix multiplication here so we operate in homogeneous
       * coordinates.
       */
      float stable_edge_p1[3], stable_edge_p2[3];
      copy_v2_v2(stable_edge_p1, points[edge_index]);
      copy_v2_v2(stable_edge_p2, points[(edge_index + 1) % 4]);
      stable_edge_p1[2] = stable_edge_p2[2] = 0.0f;
      mul_m4_v3(mat, stable_edge_p1);
      mul_m4_v3(mat, stable_edge_p2);
      /* Now we iterate over all original frame corners (we call them
       * 'point' here) to see if there's black area between stabilized
       * frame edge and original point.
       */
      for (int point_index = 0; point_index < 4; point_index++) {
        const float point[3] = {points[point_index][0], points[point_index][1], 0.0f};
        /* Calculate vector which goes from first edge point to
         * second one.
         */
        float stable_edge_vec[3];
        sub_v3_v3v3(stable_edge_vec, stable_edge_p2, stable_edge_p1);
        /* Calculate vector which connects current frame point to
         * first edge point.
         */
        float point_to_edge_start_vec[3];
        sub_v3_v3v3(point_to_edge_start_vec, point, stable_edge_p1);
        /* Use this two vectors to check whether frame point is inside
         * of the stabilized frame or not.
         * If the point is inside, there is no black area happening
         * and no scaling required for it.
         */
        if (cross_v2v2(stable_edge_vec, point_to_edge_start_vec) >= 0.0f) {
          /* We are scaling around motion-compensated pivot point. */
          float scale_pivot[2];
          add_v2_v2v2(scale_pivot, pivot, translation);
          /* Calculate line which goes via `point` and parallel to
           * the stabilized frame edge. This line is coming via
           * `point` and `point2` at the end.
           */
          float point2[2];
          add_v2_v2v2(point2, point, stable_edge_vec);
          /* Calculate actual distance between pivot point and
           * the stabilized frame edge. Then calculate distance
           * between pivot point and line which goes via actual
           * corner and is parallel to the edge.
           *
           * Dividing one by another will give us required scale
           * factor to get rid of black areas.
           */
          float real_dist = dist_to_line_v2(scale_pivot, stable_edge_p1, stable_edge_p2);
          float required_dist = dist_to_line_v2(scale_pivot, point, point2);
          const float S = required_dist / real_dist;
          scale = max_ff(scale, S);
        }
      }
    }
  }
  if (stab->maxscale > 0.0f) {
    scale = min_ff(scale, stab->maxscale);
  }
  use_values_from_fcurves(ctx, false);

  return scale;
}

/* Prepare working data and determine reference point for each track.
 *
 * NOTE: These calculations _could_ be cached and reused for all frames of the
 *       same clip. However, since proper initialization depends on (weight)
 *       animation and setup of tracks, ensuring consistency of cached init data
 *       turns out to be tricky, hard to maintain and generally not worth the
 *       effort. Thus we'll re-initialize on every frame.
 */
static StabContext *init_stabilizer(MovieClip *clip, int size, float aspect)
{
  StabContext *ctx = initialize_stabilization_working_context(clip);
  BLI_assert(ctx != NULL);
  initialize_all_tracks(ctx, aspect);
  if (ctx->stab->flag & TRACKING_AUTOSCALE) {
    ctx->stab->scale = 1.0;
    ctx->stab->scale = calculate_autoscale_factor(ctx, size, aspect);
  }
  /* By default, just use values for the global current frame. */
  use_values_from_fcurves(ctx, false);
  return ctx;
}

/* === public interface functions === */

/* Get stabilization data (translation, scaling and angle) for a given frame.
 * Returned data describes how to compensate the detected movement, but with any
 * chosen scale factor already applied and any target frame position already
 * compensated. In case stabilization fails or is disabled, neutral values are
 * returned.
 *
 * framenr is a frame number, relative to the clip (not relative to the scene
 *         timeline)
 * width is an effective width of the canvas (square pixels), used to scale the
 *       determined translation
 *
 * Outputs:
 * - translation of the lateral shift, absolute canvas coordinates
 *   (square pixels).
 * - scale of the scaling to apply
 * - angle of the rotation angle, relative to the frame center
 */
/* TODO(sergey): Use r_ prefix for output parameters here. */
void BKE_tracking_stabilization_data_get(MovieClip *clip,
                                         int framenr,
                                         int width,
                                         int height,
                                         float translation[2],
                                         float *scale,
                                         float *angle)
{
  StabContext *ctx = NULL;
  MovieTracking *tracking = &clip->tracking;
  bool enabled = (tracking->stabilization.flag & TRACKING_2D_STABILIZATION);
  /* Might become a parameter of a stabilization compositor node. */
  bool do_compensate = true;
  float scale_step = 0.0f;
  float pixel_aspect = tracking->camera.pixel_aspect;
  float aspect = (float)width * pixel_aspect / height;
  int size = height;
  float pivot[2];

  if (enabled) {
    ctx = init_stabilizer(clip, size, aspect);
  }

  if (enabled && stabilization_determine_offset_for_frame(
                     ctx, framenr, aspect, translation, pivot, angle, &scale_step)) {
    stabilization_calculate_data(
        ctx, framenr, size, aspect, do_compensate, scale_step, translation, pivot, scale, angle);
    compensate_rotation_center(size, aspect, *angle, *scale, pivot, translation);
  }
  else {
    zero_v2(translation);
    *scale = 1.0f;
    *angle = 0.0f;
  }
  discard_stabilization_working_context(ctx);
}

typedef void (*interpolation_func)(struct ImBuf *, struct ImBuf *, float, float, int, int);

typedef struct TrackingStabilizeFrameInterpolationData {
  ImBuf *ibuf;
  ImBuf *tmpibuf;
  float (*mat)[4];

  interpolation_func interpolation;
} TrackingStabilizeFrameInterpolationData;

static void tracking_stabilize_frame_interpolation_cb(
    void *__restrict userdata, const int j, const TaskParallelTLS *__restrict UNUSED(tls))
{
  TrackingStabilizeFrameInterpolationData *data = userdata;
  ImBuf *ibuf = data->ibuf;
  ImBuf *tmpibuf = data->tmpibuf;
  float(*mat)[4] = data->mat;

  interpolation_func interpolation = data->interpolation;

  for (int i = 0; i < tmpibuf->x; i++) {
    float vec[3] = {i, j, 0.0f};

    mul_v3_m4v3(vec, mat, vec);

    interpolation(ibuf, tmpibuf, vec[0], vec[1], i, j);
  }
}

/* Stabilize given image buffer using stabilization data for a specified
 * frame number.
 *
 * NOTE: frame number should be in clip space, not scene space.
 */
/* TODO(sergey): Use r_ prefix for output parameters here. */
ImBuf *BKE_tracking_stabilize_frame(
    MovieClip *clip, int framenr, ImBuf *ibuf, float translation[2], float *scale, float *angle)
{
  float tloc[2], tscale, tangle;
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingStabilization *stab = &tracking->stabilization;
  ImBuf *tmpibuf;
  int width = ibuf->x, height = ibuf->y;
  float pixel_aspect = tracking->camera.pixel_aspect;
  float mat[4][4];
  int filter = tracking->stabilization.filter;
  interpolation_func interpolation = NULL;
  int ibuf_flags;

  if (translation) {
    copy_v2_v2(tloc, translation);
  }

  if (scale) {
    tscale = *scale;
  }

  /* Perform early output if no stabilization is used. */
  if ((stab->flag & TRACKING_2D_STABILIZATION) == 0) {
    if (translation) {
      zero_v2(translation);
    }

    if (scale) {
      *scale = 1.0f;
    }

    if (angle) {
      *angle = 0.0f;
    }

    return ibuf;
  }

  /* Allocate frame for stabilization result. */
  ibuf_flags = 0;
  if (ibuf->rect) {
    ibuf_flags |= IB_rect;
  }
  if (ibuf->rect_float) {
    ibuf_flags |= IB_rectfloat;
  }

  tmpibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, ibuf_flags);

  /* Calculate stabilization matrix. */
  BKE_tracking_stabilization_data_get(clip, framenr, width, height, tloc, &tscale, &tangle);
  BKE_tracking_stabilization_data_to_mat4(
      ibuf->x, ibuf->y, pixel_aspect, tloc, tscale, tangle, mat);

  /* The following code visits each nominal target grid position
   * and picks interpolated data "backwards" from source.
   * thus we need the inverse of the transformation to apply. */
  invert_m4(mat);

  if (filter == TRACKING_FILTER_NEAREST) {
    interpolation = nearest_interpolation;
  }
  else if (filter == TRACKING_FILTER_BILINEAR) {
    interpolation = bilinear_interpolation;
  }
  else if (filter == TRACKING_FILTER_BICUBIC) {
    interpolation = bicubic_interpolation;
  }
  else {
    /* fallback to default interpolation method */
    interpolation = nearest_interpolation;
  }

  TrackingStabilizeFrameInterpolationData data = {
      .ibuf = ibuf,
      .tmpibuf = tmpibuf,
      .mat = mat,
      .interpolation = interpolation,
  };

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (tmpibuf->y > 128);
  BLI_task_parallel_range(
      0, tmpibuf->y, &data, tracking_stabilize_frame_interpolation_cb, &settings);

  if (tmpibuf->rect_float) {
    tmpibuf->userflags |= IB_RECT_INVALID;
  }

  if (translation) {
    copy_v2_v2(translation, tloc);
  }

  if (scale) {
    *scale = tscale;
  }

  if (angle) {
    *angle = tangle;
  }

  return tmpibuf;
}

/* Build a 4x4 transformation matrix based on the given 2D stabilization data.
 * mat is a 4x4 matrix in homogeneous coordinates, adapted to the
 *     final image buffer size and compensated for pixel aspect ratio,
 *     ready for direct OpenGL drawing.
 *
 * TODO(sergey): The signature of this function should be changed. we actually
 *               don't need the dimensions of the image buffer. Instead we
 *               should consider to provide the pivot point of the rotation as a
 *               further stabilization data parameter.
 */
void BKE_tracking_stabilization_data_to_mat4(int buffer_width,
                                             int buffer_height,
                                             float pixel_aspect,
                                             float translation[2],
                                             float scale,
                                             float angle,
                                             float r_mat[4][4])
{
  /* Since we cannot receive the real pivot point coordinates (API limitation),
   * we perform the rotation/scale around the center of frame.
   * Then we correct by an additional shift, which was calculated in
   * compensate_rotation_center() and "sneaked in" as additional offset
   * in the translation parameter. This works, since translation needs to be
   * applied after rotation/scale anyway. Thus effectively the image gets
   * rotated around the desired pivot point
   */
  /* TODO(sergey) pivot shouldn't be calculated here, rather received
   * as a parameter.
   */
  float pivot[2];
  pivot[0] = 0.5f * pixel_aspect * buffer_width;
  pivot[1] = 0.5f * buffer_height;
  /* Compose transformation matrix. */
  stabilization_data_to_mat4(pixel_aspect, pivot, translation, scale, angle, r_mat);
}
