/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "SEQ_animation.hh"
#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "sequencer.hh"
#include "strip_time.hh"

using namespace blender;

bool SEQ_transform_single_image_check(const Strip *strip)
{
  return (strip->flag & SEQ_SINGLE_FRAME_CONTENT) != 0;
}

bool SEQ_transform_sequence_can_be_translated(const Strip *strip)
{
  return !(strip->type & STRIP_TYPE_EFFECT) || (SEQ_effect_get_num_inputs(strip->type) == 0);
}

bool SEQ_transform_test_overlap_seq_seq(const Scene *scene, Strip *seq1, Strip *seq2)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((SEQ_time_right_handle_frame_get(scene, seq1) <=
            SEQ_time_left_handle_frame_get(scene, seq2)) ||
           (SEQ_time_left_handle_frame_get(scene, seq1) >=
            SEQ_time_right_handle_frame_get(scene, seq2))) == 0);
}

bool SEQ_transform_test_overlap(const Scene *scene, ListBase *seqbasep, Strip *test)
{
  Strip *strip;

  strip = static_cast<Strip *>(seqbasep->first);
  while (strip) {
    if (SEQ_transform_test_overlap_seq_seq(scene, test, strip)) {
      return true;
    }

    strip = strip->next;
  }
  return false;
}

void SEQ_transform_translate_sequence(Scene *evil_scene, Strip *strip, int delta)
{
  if (delta == 0) {
    return;
  }

  /* Meta strips requires their content is to be translated, and then frame range of the meta is
   * updated based on nested strips. This won't work for empty meta-strips,
   * so they can be treated as normal strip. */
  if (strip->type == STRIP_TYPE_META && !BLI_listbase_is_empty(&strip->seqbase)) {
    LISTBASE_FOREACH (Strip *, strip_child, &strip->seqbase) {
      SEQ_transform_translate_sequence(evil_scene, strip_child, delta);
    }
    /* Move meta start/end points. */
    strip_time_translate_handles(evil_scene, strip, delta);
  }
  else if (strip->seq1 == nullptr && strip->seq2 == nullptr) { /* All other strip types. */
    strip->start += delta;
    /* Only to make files usable in older versions. */
    strip->startdisp = SEQ_time_left_handle_frame_get(evil_scene, strip);
    strip->enddisp = SEQ_time_right_handle_frame_get(evil_scene, strip);
  }

  SEQ_offset_animdata(evil_scene, strip, delta);
  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(evil_scene, strip);
  strip_time_update_effects_strip_range(evil_scene, effects);
  SEQ_time_update_meta_strip_range(evil_scene, SEQ_lookup_meta_by_strip(evil_scene, strip));
}

bool SEQ_transform_seqbase_shuffle_ex(ListBase *seqbasep,
                                      Strip *test,
                                      Scene *evil_scene,
                                      int channel_delta)
{
  const int orig_machine = test->machine;
  BLI_assert(ELEM(channel_delta, -1, 1));

  test->machine += channel_delta;
  while (SEQ_transform_test_overlap(evil_scene, seqbasep, test)) {
    if ((channel_delta > 0) ? (test->machine >= SEQ_MAX_CHANNELS) : (test->machine < 1)) {
      break;
    }

    test->machine += channel_delta;
  }

  if (!SEQ_is_valid_strip_channel(test)) {
    /* Blender 2.4x would remove the strip.
     * nicer to move it to the end */

    int new_frame = SEQ_time_right_handle_frame_get(evil_scene, test);

    LISTBASE_FOREACH (Strip *, strip, seqbasep) {
      if (strip->machine == orig_machine) {
        new_frame = max_ii(new_frame, SEQ_time_right_handle_frame_get(evil_scene, strip));
      }
    }

    test->machine = orig_machine;
    new_frame = new_frame + (test->start - SEQ_time_left_handle_frame_get(
                                               evil_scene, test)); /* adjust by the startdisp */
    SEQ_transform_translate_sequence(evil_scene, test, new_frame - test->start);
    return false;
  }

  return true;
}

bool SEQ_transform_seqbase_shuffle(ListBase *seqbasep, Strip *test, Scene *evil_scene)
{
  return SEQ_transform_seqbase_shuffle_ex(seqbasep, test, evil_scene, 1);
}

static bool shuffle_seq_test_overlap(const Scene *scene,
                                     const Strip *seq1,
                                     const Strip *seq2,
                                     const int offset)
{
  BLI_assert(seq1 != seq2);
  return (seq1->machine == seq2->machine &&
          ((SEQ_time_right_handle_frame_get(scene, seq1) + offset <=
            SEQ_time_left_handle_frame_get(scene, seq2)) ||
           (SEQ_time_left_handle_frame_get(scene, seq1) + offset >=
            SEQ_time_right_handle_frame_get(scene, seq2))) == 0);
}

static int shuffle_seq_time_offset_get(const Scene *scene,
                                       blender::Span<Strip *> strips_to_shuffle,
                                       ListBase *seqbasep,
                                       char dir)
{
  int offset = 0;
  bool all_conflicts_resolved = false;

  while (!all_conflicts_resolved) {
    all_conflicts_resolved = true;
    for (Strip *strip : strips_to_shuffle) {
      LISTBASE_FOREACH (Strip *, strip_other, seqbasep) {
        if (strips_to_shuffle.contains(strip_other)) {
          continue;
        }
        if (SEQ_relation_is_effect_of_strip(strip_other, strip)) {
          continue;
        }
        if (!shuffle_seq_test_overlap(scene, strip, strip_other, offset)) {
          continue;
        }

        all_conflicts_resolved = false;

        if (dir == 'L') {
          offset = min_ii(offset,
                          SEQ_time_left_handle_frame_get(scene, strip_other) -
                              SEQ_time_right_handle_frame_get(scene, strip));
        }
        else {
          offset = max_ii(offset,
                          SEQ_time_right_handle_frame_get(scene, strip_other) -
                              SEQ_time_left_handle_frame_get(scene, strip));
        }
      }
    }
  }

  return offset;
}

bool SEQ_transform_seqbase_shuffle_time(blender::Span<Strip *> strips_to_shuffle,
                                        ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        const bool use_sync_markers)
{
  blender::VectorSet<Strip *> empty_set;
  return SEQ_transform_seqbase_shuffle_time(
      strips_to_shuffle, empty_set, seqbasep, evil_scene, markers, use_sync_markers);
}

bool SEQ_transform_seqbase_shuffle_time(blender::Span<Strip *> strips_to_shuffle,
                                        blender::Span<Strip *> time_dependent_strips,
                                        ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        const bool use_sync_markers)
{
  int offset_l = shuffle_seq_time_offset_get(evil_scene, strips_to_shuffle, seqbasep, 'L');
  int offset_r = shuffle_seq_time_offset_get(evil_scene, strips_to_shuffle, seqbasep, 'R');
  int offset = (-offset_l < offset_r) ? offset_l : offset_r;

  if (offset) {
    for (Strip *strip : strips_to_shuffle) {
      SEQ_transform_translate_sequence(evil_scene, strip, offset);
      strip->flag &= ~SEQ_OVERLAP;
    }

    if (!time_dependent_strips.is_empty()) {
      for (Strip *strip : time_dependent_strips) {
        SEQ_offset_animdata(evil_scene, strip, offset);
      }
    }

    if (use_sync_markers && !(evil_scene->toolsettings->lock_markers) && (markers != nullptr)) {
      /* affect selected markers - it's unlikely that we will want to affect all in this way? */
      LISTBASE_FOREACH (TimeMarker *, marker, markers) {
        if (marker->flag & SELECT) {
          marker->frame += offset;
        }
      }
    }
  }

  return offset ? false : true;
}

static blender::VectorSet<Strip *> extract_standalone_strips(
    blender::Span<Strip *> transformed_strips)
{
  blender::VectorSet<Strip *> standalone_strips;

  for (Strip *strip : transformed_strips) {
    if ((strip->type & STRIP_TYPE_EFFECT) == 0 || strip->seq1 == nullptr) {
      standalone_strips.add(strip);
    }
  }
  return standalone_strips;
}

/* Query strips positioned after left edge of transformed strips bound-box. */
static blender::VectorSet<Strip *> query_right_side_strips(
    const Scene *scene,
    ListBase *seqbase,
    blender::Span<Strip *> transformed_strips,
    blender::Span<Strip *> time_dependent_strips)
{
  int minframe = MAXFRAME;
  {
    for (Strip *strip : transformed_strips) {
      minframe = min_ii(minframe, SEQ_time_left_handle_frame_get(scene, strip));
    }
  }

  blender::VectorSet<Strip *> right_side_strips;
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (!time_dependent_strips.is_empty() && time_dependent_strips.contains(strip)) {
      continue;
    }
    if (transformed_strips.contains(strip)) {
      continue;
    }

    if ((strip->flag & SELECT) == 0 && SEQ_time_left_handle_frame_get(scene, strip) >= minframe) {
      right_side_strips.add(strip);
    }
  }
  return right_side_strips;
}

/* Offset all strips positioned after left edge of transformed strips bound-box by amount equal
 * to overlap of transformed strips. */
static void strip_transform_handle_expand_to_fit(Scene *scene,
                                                 ListBase *seqbasep,
                                                 blender::Span<Strip *> transformed_strips,
                                                 blender::Span<Strip *> time_dependent_strips,
                                                 bool use_sync_markers)
{
  ListBase *markers = &scene->markers;

  blender::VectorSet right_side_strips = query_right_side_strips(
      scene, seqbasep, transformed_strips, time_dependent_strips);

  /* Temporarily move right side strips beyond timeline boundary. */
  for (Strip *strip : right_side_strips) {
    strip->machine += SEQ_MAX_CHANNELS * 2;
  }

  /* Shuffle transformed standalone strips. This is because transformed strips can overlap with
   * strips on left side. */
  blender::VectorSet standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, time_dependent_strips, seqbasep, scene, markers, use_sync_markers);

  /* Move temporarily moved strips back to their original place and tag for shuffling. */
  for (Strip *strip : right_side_strips) {
    strip->machine -= SEQ_MAX_CHANNELS * 2;
  }
  /* Shuffle again to displace strips on right side. Final effect shuffling is done in
   * SEQ_transform_handle_overlap. */
  SEQ_transform_seqbase_shuffle_time(
      right_side_strips, seqbasep, scene, markers, use_sync_markers);
}

static blender::VectorSet<Strip *> query_overwrite_targets(
    const Scene *scene, ListBase *seqbasep, blender::Span<Strip *> transformed_strips)
{
  blender::VectorSet<Strip *> overwrite_targets = SEQ_query_unselected_strips(seqbasep);

  /* Effects of transformed strips can be unselected. These must not be included. */
  overwrite_targets.remove_if([&](Strip *strip) { return transformed_strips.contains(strip); });
  overwrite_targets.remove_if([&](Strip *strip) {
    bool does_overlap = false;
    for (Strip *strip_transformed : transformed_strips) {
      if (SEQ_transform_test_overlap_seq_seq(scene, strip, strip_transformed)) {
        does_overlap = true;
      }
    }

    return !does_overlap;
  });

  return overwrite_targets;
}

enum eOvelapDescrition {
  /* No overlap. */
  STRIP_OVERLAP_NONE,
  /* Overlapping strip covers overlapped completely. */
  STRIP_OVERLAP_IS_FULL,
  /* Overlapping strip is inside overlapped. */
  STRIP_OVERLAP_IS_INSIDE,
  /* Partial overlap between 2 strips. */
  STRIP_OVERLAP_LEFT_SIDE,
  STRIP_OVERLAP_RIGHT_SIDE,
};

static eOvelapDescrition overlap_description_get(const Scene *scene,
                                                 const Strip *transformed,
                                                 const Strip *target)
{
  if (SEQ_time_left_handle_frame_get(scene, transformed) <=
          SEQ_time_left_handle_frame_get(scene, target) &&
      SEQ_time_right_handle_frame_get(scene, transformed) >=
          SEQ_time_right_handle_frame_get(scene, target))
  {
    return STRIP_OVERLAP_IS_FULL;
  }
  if (SEQ_time_left_handle_frame_get(scene, transformed) >
          SEQ_time_left_handle_frame_get(scene, target) &&
      SEQ_time_right_handle_frame_get(scene, transformed) <
          SEQ_time_right_handle_frame_get(scene, target))
  {
    return STRIP_OVERLAP_IS_INSIDE;
  }
  if (SEQ_time_left_handle_frame_get(scene, transformed) <=
          SEQ_time_left_handle_frame_get(scene, target) &&
      SEQ_time_left_handle_frame_get(scene, target) <=
          SEQ_time_right_handle_frame_get(scene, transformed))
  {
    return STRIP_OVERLAP_LEFT_SIDE;
  }
  if (SEQ_time_left_handle_frame_get(scene, transformed) <=
          SEQ_time_right_handle_frame_get(scene, target) &&
      SEQ_time_right_handle_frame_get(scene, target) <=
          SEQ_time_right_handle_frame_get(scene, transformed))
  {
    return STRIP_OVERLAP_RIGHT_SIDE;
  }
  return STRIP_OVERLAP_NONE;
}

/* Split strip in 3 parts, remove middle part and fit transformed inside. */
static void strip_transform_handle_overwrite_split(Scene *scene,
                                                   ListBase *seqbasep,
                                                   const Strip *transformed,
                                                   Strip *target)
{
  /* Because we are doing a soft split, bmain is not used in SEQ_edit_strip_split, so we can
   * pass nullptr here. */
  Main *bmain = nullptr;

  Strip *split_strip = SEQ_edit_strip_split(bmain,
                                            scene,
                                            seqbasep,
                                            target,
                                            SEQ_time_left_handle_frame_get(scene, transformed),
                                            SEQ_SPLIT_SOFT,
                                            nullptr);
  SEQ_edit_strip_split(bmain,
                       scene,
                       seqbasep,
                       split_strip,
                       SEQ_time_right_handle_frame_get(scene, transformed),
                       SEQ_SPLIT_SOFT,
                       nullptr);
  SEQ_edit_flag_for_removal(scene, seqbasep, split_strip);
  SEQ_edit_remove_flagged_sequences(scene, seqbasep);
}

/* Trim strips by adjusting handle position.
 * This is bit more complicated in case overlap happens on effect. */
static void strip_transform_handle_overwrite_trim(Scene *scene,
                                                  ListBase *seqbasep,
                                                  const Strip *transformed,
                                                  Strip *target,
                                                  const eOvelapDescrition overlap)
{
  blender::VectorSet targets = SEQ_query_by_reference(
      target, scene, seqbasep, SEQ_query_strip_effect_chain);

  /* Expand collection by adding all target's children, effects and their children. */
  if ((target->type & STRIP_TYPE_EFFECT) != 0) {
    SEQ_iterator_set_expand(scene, seqbasep, targets, SEQ_query_strip_effect_chain);
  }

  /* Trim all non effects, that have influence on effect length which is overlapping. */
  for (Strip *strip : targets) {
    if ((strip->type & STRIP_TYPE_EFFECT) != 0 && SEQ_effect_get_num_inputs(strip->type) > 0) {
      continue;
    }
    if (overlap == STRIP_OVERLAP_LEFT_SIDE) {
      SEQ_time_left_handle_frame_set(
          scene, strip, SEQ_time_right_handle_frame_get(scene, transformed));
    }
    else {
      BLI_assert(overlap == STRIP_OVERLAP_RIGHT_SIDE);
      SEQ_time_right_handle_frame_set(
          scene, strip, SEQ_time_left_handle_frame_get(scene, transformed));
    }
  }
}

static void strip_transform_handle_overwrite(Scene *scene,
                                             ListBase *seqbasep,
                                             blender::Span<Strip *> transformed_strips)
{
  blender::VectorSet targets = query_overwrite_targets(scene, seqbasep, transformed_strips);
  blender::VectorSet<Strip *> strips_to_delete;

  for (Strip *target : targets) {
    for (Strip *transformed : transformed_strips) {
      if (transformed->machine != target->machine) {
        continue;
      }

      const eOvelapDescrition overlap = overlap_description_get(scene, transformed, target);

      if (overlap == STRIP_OVERLAP_IS_FULL) {
        strips_to_delete.add(target);
      }
      else if (overlap == STRIP_OVERLAP_IS_INSIDE) {
        strip_transform_handle_overwrite_split(scene, seqbasep, transformed, target);
      }
      else if (ELEM(overlap, STRIP_OVERLAP_LEFT_SIDE, STRIP_OVERLAP_RIGHT_SIDE)) {
        strip_transform_handle_overwrite_trim(scene, seqbasep, transformed, target, overlap);
      }
    }
  }

  /* Remove covered strips. This must be done in separate loop, because
   * `SEQ_edit_strip_split()` also uses `SEQ_edit_remove_flagged_sequences()`. See #91096. */
  if (!strips_to_delete.is_empty()) {
    for (Strip *strip : strips_to_delete) {
      SEQ_edit_flag_for_removal(scene, seqbasep, strip);
    }
    SEQ_edit_remove_flagged_sequences(scene, seqbasep);
  }
}

static void strip_transform_handle_overlap_shuffle(Scene *scene,
                                                   ListBase *seqbasep,
                                                   blender::Span<Strip *> transformed_strips,
                                                   blender::Span<Strip *> time_dependent_strips,
                                                   bool use_sync_markers)
{
  ListBase *markers = &scene->markers;

  /* Shuffle non strips with no effects attached. */
  blender::VectorSet standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, time_dependent_strips, seqbasep, scene, markers, use_sync_markers);
}

void SEQ_transform_handle_overlap(Scene *scene,
                                  ListBase *seqbasep,
                                  blender::Span<Strip *> transformed_strips,
                                  bool use_sync_markers)
{
  blender::VectorSet<Strip *> empty_set;
  SEQ_transform_handle_overlap(scene, seqbasep, transformed_strips, empty_set, use_sync_markers);
}

void SEQ_transform_handle_overlap(Scene *scene,
                                  ListBase *seqbasep,
                                  blender::Span<Strip *> transformed_strips,
                                  blender::Span<Strip *> time_dependent_strips,
                                  bool use_sync_markers)
{
  const eSeqOverlapMode overlap_mode = SEQ_tool_settings_overlap_mode_get(scene);

  switch (overlap_mode) {
    case SEQ_OVERLAP_EXPAND:
      strip_transform_handle_expand_to_fit(
          scene, seqbasep, transformed_strips, time_dependent_strips, use_sync_markers);
      break;
    case SEQ_OVERLAP_OVERWRITE:
      strip_transform_handle_overwrite(scene, seqbasep, transformed_strips);
      break;
    case SEQ_OVERLAP_SHUFFLE:
      strip_transform_handle_overlap_shuffle(
          scene, seqbasep, transformed_strips, time_dependent_strips, use_sync_markers);
      break;
  }

  /* If any effects still overlap, we need to move them up.
   * In some cases other strips can be overlapping still, see #90646. */
  for (Strip *strip : transformed_strips) {
    if (SEQ_transform_test_overlap(scene, seqbasep, strip)) {
      SEQ_transform_seqbase_shuffle(seqbasep, strip, scene);
    }
    strip->flag &= ~SEQ_OVERLAP;
  }
}

void SEQ_transform_offset_after_frame(Scene *scene,
                                      ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (SEQ_time_left_handle_frame_get(scene, strip) >= timeline_frame) {
      SEQ_transform_translate_sequence(scene, strip, delta);
      SEQ_relations_invalidate_cache_preprocessed(scene, strip);
    }
  }

  if (!scene->toolsettings->lock_markers) {
    LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
      if (marker->frame >= timeline_frame) {
        marker->frame += delta;
      }
    }
  }
}

bool SEQ_transform_is_locked(ListBase *channels, const Strip *strip)
{
  const SeqTimelineChannel *channel = SEQ_channel_get_by_index(channels, strip->machine);
  return strip->flag & SEQ_LOCK ||
         (SEQ_channel_is_locked(channel) && ((strip->flag & SEQ_IGNORE_CHANNEL_LOCK) == 0));
}

void SEQ_image_transform_mirror_factor_get(const Strip *strip, float r_mirror[2])
{
  r_mirror[0] = 1.0f;
  r_mirror[1] = 1.0f;

  if ((strip->flag & SEQ_FLIPX) != 0) {
    r_mirror[0] = -1.0f;
  }
  if ((strip->flag & SEQ_FLIPY) != 0) {
    r_mirror[1] = -1.0f;
  }
}

void SEQ_image_transform_origin_offset_pixelspace_get(const Scene *scene,
                                                      const Strip *strip,
                                                      float r_origin[2])
{
  float image_size[2];
  const StripElem *strip_elem = strip->data->stripdata;
  if (strip_elem == nullptr) {
    image_size[0] = scene->r.xsch;
    image_size[1] = scene->r.ysch;
  }
  else {
    image_size[0] = strip_elem->orig_width;
    image_size[1] = strip_elem->orig_height;
  }

  const StripTransform *transform = strip->data->transform;
  r_origin[0] = (image_size[0] * transform->origin[0]) - (image_size[0] * 0.5f) + transform->xofs;
  r_origin[1] = (image_size[1] * transform->origin[1]) - (image_size[1] * 0.5f) + transform->yofs;

  const float viewport_pixel_aspect[2] = {scene->r.xasp / scene->r.yasp, 1.0f};
  float mirror[2];
  SEQ_image_transform_mirror_factor_get(strip, mirror);
  mul_v2_v2(r_origin, mirror);
  mul_v2_v2(r_origin, viewport_pixel_aspect);
}

static float4x4 seq_image_transform_matrix_get_ex(const Scene *scene,
                                                  const Strip *strip,
                                                  bool apply_rotation = true)
{
  float3 image_size(float(scene->r.xsch), float(scene->r.ysch), 0.0f);
  if (ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_IMAGE)) {
    image_size.x = strip->data->stripdata->orig_width;
    image_size.y = strip->data->stripdata->orig_height;
  }

  const StripTransform *transform = strip->data->transform;
  const float3 origin(
      image_size.x * transform->origin[0], image_size[1] * transform->origin[1], 0.0f);
  const float3 translation(transform->xofs, transform->yofs, 0.0f);
  const float3 rotation(0.0f, 0.0f, apply_rotation ? transform->rotation : 0.0f);
  const float2 scale(transform->scale_x, transform->scale_y);
  const float3 pivot = origin - (image_size / 2);

  const float4x4 matrix = math::from_loc_rot_scale<float4x4>(translation, rotation, scale);
  return math::from_origin_transform(matrix, pivot);
}

float4x4 SEQ_image_transform_matrix_get(const Scene *scene, const Strip *strip)
{
  return seq_image_transform_matrix_get_ex(scene, strip);
}

static Array<float2> strip_image_transform_quad_get_ex(const Scene *scene,
                                                       const Strip *strip,
                                                       bool apply_rotation)
{

  float3 image_size(float(scene->r.xsch), float(scene->r.ysch), 0.0f);
  if (ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_IMAGE)) {
    image_size.x = strip->data->stripdata->orig_width;
    image_size.y = strip->data->stripdata->orig_height;
  }

  const StripCrop *crop = strip->data->crop;
  float3 quad[4]{
      {(image_size[0] / 2) - crop->right, (image_size[1] / 2) - crop->top, 0.0f},
      {(image_size[0] / 2) - crop->right, (-image_size[1] / 2) + crop->bottom, 0.0f},
      {(-image_size[0] / 2) + crop->left, (-image_size[1] / 2) + crop->bottom, 0.0f},
      {(-image_size[0] / 2) + crop->left, (image_size[1] / 2) - crop->top, 0.0f},
  };

  const float3 viewport_pixel_aspect(scene->r.xasp / scene->r.yasp, 1.0f, 1.0f);
  const float4x4 matrix = seq_image_transform_matrix_get_ex(scene, strip, apply_rotation);
  float3 mirror;
  SEQ_image_transform_mirror_factor_get(strip, mirror);

  Array<float2> quad_transformed;
  quad_transformed.reinitialize(4);

  for (int i = 0; i < 4; i++) {
    float3 point = math::transform_point(matrix, quad[i]);
    point *= mirror;
    point *= viewport_pixel_aspect;
    copy_v2_v2(quad_transformed[i], point);
  }
  return quad_transformed;
}

Array<float2> SEQ_image_transform_quad_get(const Scene *scene,
                                           const Strip *strip,
                                           bool apply_rotation)
{
  return strip_image_transform_quad_get_ex(scene, strip, apply_rotation);
}

Array<float2> SEQ_image_transform_final_quad_get(const Scene *scene, const Strip *strip)
{
  return strip_image_transform_quad_get_ex(scene, strip, true);
}

void SEQ_image_preview_unit_to_px(const Scene *scene, const float co_src[2], float co_dst[2])
{
  co_dst[0] = co_src[0] * scene->r.xsch;
  co_dst[1] = co_src[1] * scene->r.ysch;
}

void SEQ_image_preview_unit_from_px(const Scene *scene, const float co_src[2], float co_dst[2])
{
  co_dst[0] = co_src[0] / scene->r.xsch;
  co_dst[1] = co_src[1] / scene->r.ysch;
}

void SEQ_image_transform_bounding_box_from_collection(Scene *scene,
                                                      blender::Span<Strip *> strips,
                                                      bool apply_rotation,
                                                      float r_min[2],
                                                      float r_max[2])
{
  INIT_MINMAX2(r_min, r_max);
  for (Strip *strip : strips) {
    Array<float2> quad = SEQ_image_transform_quad_get(scene, strip, apply_rotation);
    for (int i = 0; i < 4; i++) {
      minmax_v2v2_v2(r_min, r_max, quad[i]);
    }
  }
}
