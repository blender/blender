/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Foundation
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"

#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "SEQ_animation.h"
#include "SEQ_channels.h"
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

#include "sequencer.h"
#include "strip_time.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"seq.strip_transform"};

bool SEQ_transform_single_image_check(Sequence *seq)
{
  return (seq->flag & SEQ_SINGLE_FRAME_CONTENT) != 0;
}

bool SEQ_transform_seqbase_isolated_sel_check(ListBase *seqbase)
{
  Sequence *seq;
  /* is there more than 1 select */
  bool ok = false;

  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      ok = true;
      break;
    }
  }

  if (ok == false) {
    return false;
  }

  /* test relationships */
  for (seq = static_cast<Sequence *>(seqbase->first); seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
      continue;
    }

    if (seq->flag & SELECT) {
      if ((seq->seq1 && (seq->seq1->flag & SELECT) == 0) ||
          (seq->seq2 && (seq->seq2->flag & SELECT) == 0) ||
          (seq->seq3 && (seq->seq3->flag & SELECT) == 0))
      {
        return false;
      }
    }
    else {
      if ((seq->seq1 && (seq->seq1->flag & SELECT)) || (seq->seq2 && (seq->seq2->flag & SELECT)) ||
          (seq->seq3 && (seq->seq3->flag & SELECT)))
      {
        return false;
      }
    }
  }

  return true;
}

bool SEQ_transform_sequence_can_be_translated(Sequence *seq)
{
  return !(seq->type & SEQ_TYPE_EFFECT) || (SEQ_effect_get_num_inputs(seq->type) == 0);
}

bool SEQ_transform_test_overlap_seq_seq(const Scene *scene, Sequence *seq1, Sequence *seq2)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((SEQ_time_right_handle_frame_get(scene, seq1) <=
            SEQ_time_left_handle_frame_get(scene, seq2)) ||
           (SEQ_time_left_handle_frame_get(scene, seq1) >=
            SEQ_time_right_handle_frame_get(scene, seq2))) == 0);
}

bool SEQ_transform_test_overlap(const Scene *scene, ListBase *seqbasep, Sequence *test)
{
  Sequence *seq;

  seq = static_cast<Sequence *>(seqbasep->first);
  while (seq) {
    if (SEQ_transform_test_overlap_seq_seq(scene, test, seq)) {
      return true;
    }

    seq = seq->next;
  }
  return false;
}

void SEQ_transform_translate_sequence(Scene *evil_scene, Sequence *seq, int delta)
{
  if (delta == 0) {
    return;
  }

  /* Meta strips requires their content is to be translated, and then frame range of the meta is
   * updated based on nested strips. This won't work for empty meta-strips,
   * so they can be treated as normal strip. */
  if (seq->type == SEQ_TYPE_META && !BLI_listbase_is_empty(&seq->seqbase)) {
    Sequence *seq_child;
    for (seq_child = static_cast<Sequence *>(seq->seqbase.first); seq_child;
         seq_child = seq_child->next)
    {
      SEQ_transform_translate_sequence(evil_scene, seq_child, delta);
    }
    /* Move meta start/end points. */
    seq_time_translate_handles(evil_scene, seq, delta);
  }
  else if (seq->seq1 == nullptr && seq->seq2 == nullptr) { /* All other strip types. */
    seq->start += delta;
    /* Only to make files usable in older versions. */
    seq->startdisp = SEQ_time_left_handle_frame_get(evil_scene, seq);
    seq->enddisp = SEQ_time_right_handle_frame_get(evil_scene, seq);
  }

  SEQ_offset_animdata(evil_scene, seq, delta);
  SEQ_time_update_meta_strip_range(evil_scene, seq_sequence_lookup_meta_by_seq(evil_scene, seq));
  seq_time_update_effects_strip_range(evil_scene,
                                      seq_sequence_lookup_effects_by_seq(evil_scene, seq));
}

bool SEQ_transform_seqbase_shuffle_ex(ListBase *seqbasep,
                                      Sequence *test,
                                      Scene *evil_scene,
                                      int channel_delta)
{
  const int orig_machine = test->machine;
  BLI_assert(ELEM(channel_delta, -1, 1));

  test->machine += channel_delta;
  while (SEQ_transform_test_overlap(evil_scene, seqbasep, test)) {
    if ((channel_delta > 0) ? (test->machine >= MAXSEQ) : (test->machine < 1)) {
      break;
    }

    test->machine += channel_delta;
  }

  if (!SEQ_valid_strip_channel(test)) {
    /* Blender 2.4x would remove the strip.
     * nicer to move it to the end */

    Sequence *seq;
    int new_frame = SEQ_time_right_handle_frame_get(evil_scene, test);

    for (seq = static_cast<Sequence *>(seqbasep->first); seq; seq = seq->next) {
      if (seq->machine == orig_machine) {
        new_frame = max_ii(new_frame, SEQ_time_right_handle_frame_get(evil_scene, seq));
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

bool SEQ_transform_seqbase_shuffle(ListBase *seqbasep, Sequence *test, Scene *evil_scene)
{
  return SEQ_transform_seqbase_shuffle_ex(seqbasep, test, evil_scene, 1);
}

static bool shuffle_seq_test_overlap(const Scene *scene,
                                     const Sequence *seq1,
                                     const Sequence *seq2,
                                     const int offset)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((SEQ_time_right_handle_frame_get(scene, seq1) + offset <=
            SEQ_time_left_handle_frame_get(scene, seq2)) ||
           (SEQ_time_left_handle_frame_get(scene, seq1) + offset >=
            SEQ_time_right_handle_frame_get(scene, seq2))) == 0);
}

static int shuffle_seq_time_offset_get(const Scene *scene,
                                       SeqCollection *strips_to_shuffle,
                                       ListBase *seqbasep,
                                       char dir)
{
  int offset = 0;
  Sequence *seq;
  bool all_conflicts_resolved = false;

  while (!all_conflicts_resolved) {
    all_conflicts_resolved = true;
    SEQ_ITERATOR_FOREACH (seq, strips_to_shuffle) {
      LISTBASE_FOREACH (Sequence *, seq_other, seqbasep) {
        if (!shuffle_seq_test_overlap(scene, seq, seq_other, offset)) {
          continue;
        }
        if (SEQ_relation_is_effect_of_strip(seq_other, seq)) {
          continue;
        }
        if (UNLIKELY(SEQ_collection_has_strip(seq_other, strips_to_shuffle))) {
          CLOG_WARN(&LOG,
                    "Strip overlaps with itself or another strip, that is to be shuffled. "
                    "This should never happen.");
          continue;
        }

        all_conflicts_resolved = false;

        if (dir == 'L') {
          offset = min_ii(offset,
                          SEQ_time_left_handle_frame_get(scene, seq_other) -
                              SEQ_time_right_handle_frame_get(scene, seq));
        }
        else {
          offset = max_ii(offset,
                          SEQ_time_right_handle_frame_get(scene, seq_other) -
                              SEQ_time_left_handle_frame_get(scene, seq));
        }
      }
    }
  }

  return offset;
}

bool SEQ_transform_seqbase_shuffle_time(SeqCollection *strips_to_shuffle,
                                        SeqCollection *time_dependent_strips,
                                        ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        const bool use_sync_markers)
{
  int offset_l = shuffle_seq_time_offset_get(evil_scene, strips_to_shuffle, seqbasep, 'L');
  int offset_r = shuffle_seq_time_offset_get(evil_scene, strips_to_shuffle, seqbasep, 'R');
  int offset = (-offset_l < offset_r) ? offset_l : offset_r;

  if (offset) {
    Sequence *seq;
    SEQ_ITERATOR_FOREACH (seq, strips_to_shuffle) {
      SEQ_transform_translate_sequence(evil_scene, seq, offset);
      seq->flag &= ~SEQ_OVERLAP;
    }

    if (time_dependent_strips != nullptr) {
      SEQ_ITERATOR_FOREACH (seq, time_dependent_strips) {
        SEQ_offset_animdata(evil_scene, seq, offset);
      }
    }

    if (use_sync_markers && !(evil_scene->toolsettings->lock_markers) && (markers != nullptr)) {
      TimeMarker *marker;
      /* affect selected markers - it's unlikely that we will want to affect all in this way? */
      for (marker = static_cast<TimeMarker *>(markers->first); marker; marker = marker->next) {
        if (marker->flag & SELECT) {
          marker->frame += offset;
        }
      }
    }
  }

  return offset ? false : true;
}

static SeqCollection *extract_standalone_strips(SeqCollection *transformed_strips)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 || seq->seq1 == nullptr) {
      SEQ_collection_append_strip(seq, collection);
    }
  }
  return collection;
}

/* Query strips positioned after left edge of transformed strips bound-box. */
static SeqCollection *query_right_side_strips(const Scene *scene,
                                              ListBase *seqbase,
                                              SeqCollection *transformed_strips,
                                              SeqCollection *time_dependent_strips)
{
  int minframe = MAXFRAME;
  {
    Sequence *seq;
    SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
      minframe = min_ii(minframe, SEQ_time_left_handle_frame_get(scene, seq));
    }
  }

  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (time_dependent_strips != nullptr && SEQ_collection_has_strip(seq, time_dependent_strips)) {
      continue;
    }
    if (SEQ_collection_has_strip(seq, transformed_strips)) {
      continue;
    }

    if ((seq->flag & SELECT) == 0 && SEQ_time_left_handle_frame_get(scene, seq) >= minframe) {
      SEQ_collection_append_strip(seq, collection);
    }
  }
  return collection;
}

/* Offset all strips positioned after left edge of transformed strips bound-box by amount equal
 * to overlap of transformed strips. */
static void seq_transform_handle_expand_to_fit(Scene *scene,
                                               ListBase *seqbasep,
                                               SeqCollection *transformed_strips,
                                               SeqCollection *time_dependent_strips,
                                               bool use_sync_markers)
{
  ListBase *markers = &scene->markers;

  SeqCollection *right_side_strips = query_right_side_strips(
      scene, seqbasep, transformed_strips, time_dependent_strips);

  /* Temporarily move right side strips beyond timeline boundary. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, right_side_strips) {
    seq->machine += MAXSEQ * 2;
  }

  /* Shuffle transformed standalone strips. This is because transformed strips can overlap with
   * strips on left side. */
  SeqCollection *standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, time_dependent_strips, seqbasep, scene, markers, use_sync_markers);
  SEQ_collection_free(standalone_strips);

  /* Move temporarily moved strips back to their original place and tag for shuffling. */
  SEQ_ITERATOR_FOREACH (seq, right_side_strips) {
    seq->machine -= MAXSEQ * 2;
  }
  /* Shuffle again to displace strips on right side. Final effect shuffling is done in
   * SEQ_transform_handle_overlap. */
  SEQ_transform_seqbase_shuffle_time(
      right_side_strips, nullptr, seqbasep, scene, markers, use_sync_markers);
  SEQ_collection_free(right_side_strips);
}

static SeqCollection *query_overwrite_targets(const Scene *scene,
                                              ListBase *seqbasep,
                                              SeqCollection *transformed_strips)
{
  SeqCollection *collection = SEQ_query_unselected_strips(seqbasep);

  Sequence *seq, *seq_transformed;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    bool does_overlap = false;

    SEQ_ITERATOR_FOREACH (seq_transformed, transformed_strips) {
      /* Effects of transformed strips can be unselected. These must not be included. */
      if (seq == seq_transformed) {
        SEQ_collection_remove_strip(seq, collection);
      }
      if (SEQ_transform_test_overlap_seq_seq(scene, seq, seq_transformed)) {
        does_overlap = true;
      }
    }

    if (!does_overlap) {
      SEQ_collection_remove_strip(seq, collection);
    }
  }

  return collection;
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
                                                 const Sequence *transformed,
                                                 const Sequence *target)
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
static void seq_transform_handle_overwrite_split(Scene *scene,
                                                 ListBase *seqbasep,
                                                 const Sequence *transformed,
                                                 Sequence *target)
{
  /* Because we are doing a soft split, bmain is not used in SEQ_edit_strip_split, so we can pass
   * nullptr here. */
  Main *bmain = nullptr;

  Sequence *split_strip = SEQ_edit_strip_split(bmain,
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
static void seq_transform_handle_overwrite_trim(Scene *scene,
                                                ListBase *seqbasep,
                                                const Sequence *transformed,
                                                Sequence *target,
                                                const eOvelapDescrition overlap)
{
  SeqCollection *targets = SEQ_query_by_reference(
      target, scene, seqbasep, SEQ_query_strip_effect_chain);

  /* Expand collection by adding all target's children, effects and their children. */
  if ((target->type & SEQ_TYPE_EFFECT) != 0) {
    SEQ_collection_expand(scene, seqbasep, targets, SEQ_query_strip_effect_chain);
  }

  /* Trim all non effects, that have influence on effect length which is overlapping. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, targets) {
    if ((seq->type & SEQ_TYPE_EFFECT) != 0 && SEQ_effect_get_num_inputs(seq->type) > 0) {
      continue;
    }
    if (overlap == STRIP_OVERLAP_LEFT_SIDE) {
      SEQ_time_left_handle_frame_set(
          scene, seq, SEQ_time_right_handle_frame_get(scene, transformed));
    }
    else {
      BLI_assert(overlap == STRIP_OVERLAP_RIGHT_SIDE);
      SEQ_time_right_handle_frame_set(
          scene, seq, SEQ_time_left_handle_frame_get(scene, transformed));
    }
  }
  SEQ_collection_free(targets);
}

static void seq_transform_handle_overwrite(Scene *scene,
                                           ListBase *seqbasep,
                                           SeqCollection *transformed_strips)
{
  SeqCollection *targets = query_overwrite_targets(scene, seqbasep, transformed_strips);
  SeqCollection *strips_to_delete = SEQ_collection_create(__func__);

  Sequence *target;
  Sequence *transformed;
  SEQ_ITERATOR_FOREACH (target, targets) {
    SEQ_ITERATOR_FOREACH (transformed, transformed_strips) {
      if (transformed->machine != target->machine) {
        continue;
      }

      const eOvelapDescrition overlap = overlap_description_get(scene, transformed, target);

      if (overlap == STRIP_OVERLAP_IS_FULL) {
        SEQ_collection_append_strip(target, strips_to_delete);
      }
      else if (overlap == STRIP_OVERLAP_IS_INSIDE) {
        seq_transform_handle_overwrite_split(scene, seqbasep, transformed, target);
      }
      else if (ELEM(overlap, STRIP_OVERLAP_LEFT_SIDE, STRIP_OVERLAP_RIGHT_SIDE)) {
        seq_transform_handle_overwrite_trim(scene, seqbasep, transformed, target, overlap);
      }
    }
  }

  SEQ_collection_free(targets);

  /* Remove covered strips. This must be done in separate loop, because `SEQ_edit_strip_split()`
   * also uses `SEQ_edit_remove_flagged_sequences()`. See #91096. */
  if (SEQ_collection_len(strips_to_delete) > 0) {
    Sequence *seq;
    SEQ_ITERATOR_FOREACH (seq, strips_to_delete) {
      SEQ_edit_flag_for_removal(scene, seqbasep, seq);
    }
    SEQ_edit_remove_flagged_sequences(scene, seqbasep);
  }
  SEQ_collection_free(strips_to_delete);
}

static void seq_transform_handle_overlap_shuffle(Scene *scene,
                                                 ListBase *seqbasep,
                                                 SeqCollection *transformed_strips,
                                                 SeqCollection *time_dependent_strips,
                                                 bool use_sync_markers)
{
  ListBase *markers = &scene->markers;

  /* Shuffle non strips with no effects attached. */
  SeqCollection *standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, time_dependent_strips, seqbasep, scene, markers, use_sync_markers);
  SEQ_collection_free(standalone_strips);
}

void SEQ_transform_handle_overlap(Scene *scene,
                                  ListBase *seqbasep,
                                  SeqCollection *transformed_strips,
                                  SeqCollection *time_dependent_strips,
                                  bool use_sync_markers)
{
  const eSeqOverlapMode overlap_mode = SEQ_tool_settings_overlap_mode_get(scene);

  switch (overlap_mode) {
    case SEQ_OVERLAP_EXPAND:
      seq_transform_handle_expand_to_fit(
          scene, seqbasep, transformed_strips, time_dependent_strips, use_sync_markers);
      break;
    case SEQ_OVERLAP_OVERWRITE:
      seq_transform_handle_overwrite(scene, seqbasep, transformed_strips);
      break;
    case SEQ_OVERLAP_SHUFFLE:
      seq_transform_handle_overlap_shuffle(
          scene, seqbasep, transformed_strips, time_dependent_strips, use_sync_markers);
      break;
  }

  /* If any effects still overlap, we need to move them up.
   * In some cases other strips can be overlapping still, see #90646. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if (SEQ_transform_test_overlap(scene, seqbasep, seq)) {
      SEQ_transform_seqbase_shuffle(seqbasep, seq, scene);
    }
    seq->flag &= ~SEQ_OVERLAP;
  }
}

void SEQ_transform_offset_after_frame(Scene *scene,
                                      ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (SEQ_time_left_handle_frame_get(scene, seq) >= timeline_frame) {
      SEQ_transform_translate_sequence(scene, seq, delta);
      SEQ_relations_invalidate_cache_preprocessed(scene, seq);
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

bool SEQ_transform_is_locked(ListBase *channels, Sequence *seq)
{
  const SeqTimelineChannel *channel = SEQ_channel_get_by_index(channels, seq->machine);
  return seq->flag & SEQ_LOCK ||
         (SEQ_channel_is_locked(channel) && ((seq->flag & SEQ_IGNORE_CHANNEL_LOCK) == 0));
}

void SEQ_image_transform_mirror_factor_get(const Sequence *seq, float r_mirror[2])
{
  r_mirror[0] = 1.0f;
  r_mirror[1] = 1.0f;

  if ((seq->flag & SEQ_FLIPX) != 0) {
    r_mirror[0] = -1.0f;
  }
  if ((seq->flag & SEQ_FLIPY) != 0) {
    r_mirror[1] = -1.0f;
  }
}

void SEQ_image_transform_origin_offset_pixelspace_get(const Scene *scene,
                                                      const Sequence *seq,
                                                      float r_origin[2])
{
  float image_size[2];
  const StripElem *strip_elem = seq->strip->stripdata;
  if (strip_elem == nullptr) {
    image_size[0] = scene->r.xsch;
    image_size[1] = scene->r.ysch;
  }
  else {
    image_size[0] = strip_elem->orig_width;
    image_size[1] = strip_elem->orig_height;
  }

  const StripTransform *transform = seq->strip->transform;
  r_origin[0] = (image_size[0] * transform->origin[0]) - (image_size[0] * 0.5f) + transform->xofs;
  r_origin[1] = (image_size[1] * transform->origin[1]) - (image_size[1] * 0.5f) + transform->yofs;

  const float viewport_pixel_aspect[2] = {scene->r.xasp / scene->r.yasp, 1.0f};
  float mirror[2];
  SEQ_image_transform_mirror_factor_get(seq, mirror);
  mul_v2_v2(r_origin, mirror);
  mul_v2_v2(r_origin, viewport_pixel_aspect);
}

static void seq_image_transform_quad_get_ex(const Scene *scene,
                                            const Sequence *seq,
                                            bool apply_rotation,
                                            float r_quad[4][2])
{
  StripTransform *transform = seq->strip->transform;
  const StripCrop *crop = seq->strip->crop;

  int image_size[2] = {scene->r.xsch, scene->r.ysch};
  if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE)) {
    image_size[0] = seq->strip->stripdata->orig_width;
    image_size[1] = seq->strip->stripdata->orig_height;
  }

  float transform_matrix[4][4];
  float rotation_matrix[3][3];
  axis_angle_to_mat3_single(rotation_matrix, 'Z', apply_rotation ? transform->rotation : 0.0f);
  loc_rot_size_to_mat4(transform_matrix,
                       blender::float3{transform->xofs, transform->yofs, 0.0f},
                       rotation_matrix,
                       blender::float3{transform->scale_x, transform->scale_y, 1.0f});
  const float origin[2] = {image_size[0] * transform->origin[0],
                           image_size[1] * transform->origin[1]};
  const float pivot[3] = {origin[0] - (image_size[0] / 2), origin[1] - (image_size[1] / 2), 0.0f};
  transform_pivot_set_m4(transform_matrix, pivot);

  float quad_temp[4][3];
  for (int i = 0; i < 4; i++) {
    zero_v2(quad_temp[i]);
  }

  quad_temp[0][0] = (image_size[0] / 2) - crop->right;
  quad_temp[0][1] = (image_size[1] / 2) - crop->top;
  quad_temp[1][0] = (image_size[0] / 2) - crop->right;
  quad_temp[1][1] = (-image_size[1] / 2) + crop->bottom;
  quad_temp[2][0] = (-image_size[0] / 2) + crop->left;
  quad_temp[2][1] = (-image_size[1] / 2) + crop->bottom;
  quad_temp[3][0] = (-image_size[0] / 2) + crop->left;
  quad_temp[3][1] = (image_size[1] / 2) - crop->top;

  float mirror[2];
  SEQ_image_transform_mirror_factor_get(seq, mirror);

  const float viewport_pixel_aspect[2] = {scene->r.xasp / scene->r.yasp, 1.0f};

  for (int i = 0; i < 4; i++) {
    mul_m4_v3(transform_matrix, quad_temp[i]);
    mul_v2_v2(quad_temp[i], mirror);
    mul_v2_v2(quad_temp[i], viewport_pixel_aspect);
    copy_v2_v2(r_quad[i], quad_temp[i]);
  }
}

void SEQ_image_transform_quad_get(const Scene *scene,
                                  const Sequence *seq,
                                  bool apply_rotation,
                                  float r_quad[4][2])
{
  seq_image_transform_quad_get_ex(scene, seq, apply_rotation, r_quad);
}

void SEQ_image_transform_final_quad_get(const Scene *scene,
                                        const Sequence *seq,
                                        float r_quad[4][2])
{
  seq_image_transform_quad_get_ex(scene, seq, true, r_quad);
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

void SEQ_image_transform_bounding_box_from_collection(
    Scene *scene, SeqCollection *strips, bool apply_rotation, float r_min[2], float r_max[2])
{
  Sequence *seq;

  INIT_MINMAX2(r_min, r_max);
  SEQ_ITERATOR_FOREACH (seq, strips) {
    float quad[4][2];
    SEQ_image_transform_quad_get(scene, seq, apply_rotation, quad);
    for (int i = 0; i < 4; i++) {
      minmax_v2v2_v2(r_min, r_max, quad[i]);
    }
  }
}
