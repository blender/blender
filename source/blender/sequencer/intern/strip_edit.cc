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
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_sound.hh"

#include "strip_time.hh"

#include "SEQ_add.hh"
#include "SEQ_animation.hh"
#include "SEQ_channels.hh"
#include "SEQ_connect.hh"

#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include <cstring>

namespace blender::seq {

bool edit_strip_swap(Scene *scene, Strip *strip_a, Strip *strip_b, const char **r_error_str)
{
  char name[sizeof(strip_a->name)];

  if (time_strip_length_get(scene, strip_a) != time_strip_length_get(scene, strip_b)) {
    *r_error_str = N_("Strips must be the same length");
    return false;
  }

  /* type checking, could be more advanced but disallow sound vs non-sound copy */
  if (strip_a->type != strip_b->type) {
    if (strip_a->type == STRIP_TYPE_SOUND_RAM || strip_b->type == STRIP_TYPE_SOUND_RAM) {
      *r_error_str = N_("Strips were not compatible");
      return false;
    }

    /* disallow effects to swap with non-effects strips */
    if (strip_a->is_effect() != strip_b->is_effect()) {
      *r_error_str = N_("Strips were not compatible");
      return false;
    }

    if (strip_a->is_effect() && strip_b->is_effect()) {
      if (effect_get_num_inputs(strip_a->type) != effect_get_num_inputs(strip_b->type)) {
        *r_error_str = N_("Strips must have the same number of inputs");
        return false;
      }
    }
  }

  dna::shallow_swap(*strip_a, *strip_b);

  /* swap back names so animation fcurves don't get swapped */
  STRNCPY(name, strip_a->name + 2);
  BLI_strncpy(strip_a->name + 2, strip_b->name + 2, sizeof(strip_b->name) - 2);
  BLI_strncpy(strip_b->name + 2, name, sizeof(strip_b->name) - 2);

  /* swap back opacity, and overlay mode */
  std::swap(strip_a->blend_mode, strip_b->blend_mode);
  std::swap(strip_a->blend_opacity, strip_b->blend_opacity);

  std::swap(strip_a->prev, strip_b->prev);
  std::swap(strip_a->next, strip_b->next);
  std::swap(strip_a->start, strip_b->start);
  std::swap(strip_a->startofs, strip_b->startofs);
  std::swap(strip_a->endofs, strip_b->endofs);
  std::swap(strip_a->channel, strip_b->channel);
  strip_time_effect_range_set(scene, strip_a);
  strip_time_effect_range_set(scene, strip_b);

  strip_lookup_invalidate(editing_get(scene));

  return true;
}

static void strip_update_muting_recursive(ListBase *channels,
                                          ListBase *seqbasep,
                                          Strip *strip_meta,
                                          const bool mute)
{
  /* For sound we go over full meta tree to update muted state,
   * since sound is played outside of evaluating the imbufs. */
  LISTBASE_FOREACH (Strip *, strip, seqbasep) {
    bool strip_mute = (mute || render_is_muted(channels, strip));

    if (strip->type == STRIP_TYPE_META) {
      /* if this is the current meta-strip, unmute because
       * all strips above this were set to mute */
      if (strip == strip_meta) {
        strip_mute = false;
      }

      strip_update_muting_recursive(&strip->channels, &strip->seqbase, strip_meta, strip_mute);
    }
    else if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SCENE)) {
      if (strip->scene_sound) {
        BKE_sound_mute_scene_sound(strip->scene_sound, strip_mute);
      }
    }
  }
}

void edit_update_muting(Editing *ed)
{
  if (ed) {
    /* mute all sounds up to current metastack list */
    MetaStack *ms = static_cast<MetaStack *>(ed->metastack.last);

    if (ms) {
      strip_update_muting_recursive(&ed->channels, &ed->seqbase, ms->parent_strip, true);
    }
    else {
      strip_update_muting_recursive(&ed->channels, &ed->seqbase, nullptr, false);
    }
  }
}

static void sequencer_flag_users_for_removal(Scene *scene, ListBase *seqbase, Strip *strip)
{
  LISTBASE_FOREACH (Strip *, user_strip, seqbase) {
    /* Look in meta-strips for usage of strip. */
    if (user_strip->type == STRIP_TYPE_META) {
      sequencer_flag_users_for_removal(scene, &user_strip->seqbase, strip);
    }

    /* Clear strip from modifiers. */
    LISTBASE_FOREACH (StripModifierData *, smd, &user_strip->modifiers) {
      if (smd->mask_strip == strip) {
        smd->mask_strip = nullptr;
      }
    }

    /* Mark effects for removal that use the strip. */
    if (relation_is_effect_of_strip(user_strip, strip)) {
      user_strip->runtime.flag |= STRIP_MARK_FOR_DELETE;
      /* Strips can be used as mask even if not in same seqbase. */
      sequencer_flag_users_for_removal(scene, &scene->ed->seqbase, user_strip);
    }
  }
}

void edit_flag_for_removal(Scene *scene, ListBase *seqbase, Strip *strip)
{
  if (strip == nullptr || (strip->runtime.flag & STRIP_MARK_FOR_DELETE) != 0) {
    return;
  }

  /* Flag and remove meta children. */
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, meta_child, &strip->seqbase) {
      edit_flag_for_removal(scene, &strip->seqbase, meta_child);
    }
  }

  strip->runtime.flag |= STRIP_MARK_FOR_DELETE;
  sequencer_flag_users_for_removal(scene, seqbase, strip);
}

void edit_remove_flagged_strips(Scene *scene, ListBase *seqbase)
{
  LISTBASE_FOREACH_MUTABLE (Strip *, strip, seqbase) {
    if (strip->runtime.flag & STRIP_MARK_FOR_DELETE) {
      if (strip->type == STRIP_TYPE_META) {
        edit_remove_flagged_strips(scene, &strip->seqbase);
      }
      free_animdata(scene, strip);
      BLI_remlink(seqbase, strip);
      strip_free(scene, strip);
      strip_lookup_invalidate(scene->ed);
    }
  }
}

bool edit_move_strip_to_seqbase(Scene *scene,
                                ListBase *seqbase,
                                Strip *strip,
                                ListBase *dst_seqbase)
{
  /* Move to meta. */
  BLI_remlink(seqbase, strip);
  BLI_addtail(dst_seqbase, strip);
  relations_invalidate_cache(scene, strip);

  /* Update meta. */
  if (transform_test_overlap(scene, dst_seqbase, strip)) {
    transform_seqbase_shuffle(dst_seqbase, strip, scene);
  }

  return true;
}

bool edit_move_strip_to_meta(Scene *scene,
                             Strip *src_strip,
                             Strip *dst_stripm,
                             const char **r_error_str)
{
  /* Find the appropriate seqbase */
  Editing *ed = editing_get(scene);
  ListBase *seqbase = get_seqbase_by_strip(scene, src_strip);

  if (dst_stripm->type != STRIP_TYPE_META) {
    *r_error_str = N_("Cannot move strip to non-meta strip");
    return false;
  }

  if (src_strip == dst_stripm) {
    *r_error_str = N_("Strip cannot be moved into itself");
    return false;
  }

  if (seqbase == &dst_stripm->seqbase) {
    *r_error_str = N_("Moved strip is already inside provided meta strip");
    return false;
  }

  if (src_strip->type == STRIP_TYPE_META && exists_in_seqbase(dst_stripm, &src_strip->seqbase)) {
    *r_error_str = N_("Moved strip is parent of provided meta strip");
    return false;
  }

  if (!exists_in_seqbase(dst_stripm, &ed->seqbase)) {
    *r_error_str = N_("Cannot move strip to different scene");
    return false;
  }

  VectorSet<Strip *> strips;
  strips.add(src_strip);
  iterator_set_expand(scene, seqbase, strips, query_strip_effect_chain);

  for (Strip *strip : strips) {
    /* Move to meta. */
    edit_move_strip_to_seqbase(scene, seqbase, strip, &dst_stripm->seqbase);
  }

  time_update_meta_strip_range(scene, dst_stripm);

  return true;
}

static void seq_split_set_right_hold_offset(Main *bmain,
                                            Scene *scene,
                                            Strip *strip,
                                            int timeline_frame)
{
  const float content_start = time_start_frame_get(strip);
  const float content_end = time_content_end_frame_get(scene, strip);

  /* Adjust within range of extended still-frames before strip. */
  if (timeline_frame < content_start) {
    const float offset = content_start + 1 - timeline_frame;
    strip->start -= offset;
    strip->startofs += offset;
  }
  /* Adjust within range of strip contents. */
  else if ((timeline_frame >= content_start) && (timeline_frame <= content_end)) {
    strip->endofs = 0;
    const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
    const float speed_factor = time_media_playback_rate_factor_get(strip, scene_fps);
    strip->anim_endofs += round_fl_to_int((content_end - timeline_frame) * speed_factor);
  }

  /* Needed only to set `strip->len`. */
  add_reload_new_file(bmain, scene, strip, false);
  time_right_handle_frame_set(scene, strip, timeline_frame);
}

static void seq_split_set_left_hold_offset(Main *bmain,
                                           Scene *scene,
                                           Strip *strip,
                                           int timeline_frame)
{
  const float content_start = time_start_frame_get(strip);
  const float content_end = time_content_end_frame_get(scene, strip);

  /* Adjust within range of strip contents. */
  if ((timeline_frame >= content_start) && (timeline_frame <= content_end)) {
    const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
    const float speed_factor = time_media_playback_rate_factor_get(strip, scene_fps);
    strip->anim_startofs += round_fl_to_int((timeline_frame - content_start) * speed_factor);
    strip->start = timeline_frame;
    strip->startofs = 0;
  }
  /* Adjust within range of extended still-frames after strip. */
  else if (timeline_frame > content_end) {
    const float offset = timeline_frame - content_end + 1;
    strip->start += offset;
    strip->endofs += offset;
  }

  /* Needed only to set `strip->len`. */
  add_reload_new_file(bmain, scene, strip, false);
  time_left_handle_frame_set(scene, strip, timeline_frame);
}

static bool seq_edit_split_intersect_check(const Scene *scene,
                                           const Strip *strip,
                                           const int timeline_frame)
{
  return timeline_frame > time_left_handle_frame_get(scene, strip) &&
         timeline_frame < time_right_handle_frame_get(scene, strip);
}

static void seq_edit_split_handle_strip_offsets(Main *bmain,
                                                Scene *scene,
                                                Strip *left_strip,
                                                Strip *right_strip,
                                                const int timeline_frame,
                                                const eSplitMethod method)
{
  if (seq_edit_split_intersect_check(scene, right_strip, timeline_frame)) {
    switch (method) {
      case SPLIT_SOFT:
        time_left_handle_frame_set(scene, right_strip, timeline_frame);
        break;
      case SPLIT_HARD:
        seq_split_set_left_hold_offset(bmain, scene, right_strip, timeline_frame);
        break;
    }
  }

  if (seq_edit_split_intersect_check(scene, left_strip, timeline_frame)) {
    switch (method) {
      case SPLIT_SOFT:
        time_right_handle_frame_set(scene, left_strip, timeline_frame);
        break;
      case SPLIT_HARD:
        seq_split_set_right_hold_offset(bmain, scene, left_strip, timeline_frame);
        break;
    }
  }
}

static bool seq_edit_split_effect_inputs_intersect(const Scene *scene,
                                                   const Strip *strip,
                                                   const int timeline_frame)
{
  bool input_does_intersect = false;
  if (strip->input1) {
    input_does_intersect |= seq_edit_split_intersect_check(scene, strip->input1, timeline_frame);
    if (strip->input1->is_effect()) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(
          scene, strip->input1, timeline_frame);
    }
  }
  if (strip->input2) {
    input_does_intersect |= seq_edit_split_intersect_check(scene, strip->input2, timeline_frame);
    if (strip->input2->is_effect()) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(
          scene, strip->input2, timeline_frame);
    }
  }
  return input_does_intersect;
}

static bool seq_edit_split_operation_permitted_check(const Scene *scene,
                                                     Span<Strip *> strips,
                                                     const int timeline_frame,
                                                     const char **r_error)
{
  for (Strip *strip : strips) {
    ListBase *channels = channels_displayed_get(editing_get(scene));
    if (transform_is_locked(channels, strip)) {
      *r_error = "Strip is locked.";
      return false;
    }
    if (!strip->is_effect()) {
      continue;
    }
    if (!seq_edit_split_intersect_check(scene, strip, timeline_frame)) {
      continue;
    }
    if (effect_get_num_inputs(strip->type) <= 1) {
      continue;
    }
    if (effect_is_transition(StripType(strip->type))) {
      *r_error = "Splitting transition effect is not permitted.";
      return false;
    }
    if (!seq_edit_split_effect_inputs_intersect(scene, strip, timeline_frame)) {
      *r_error = "Effect inputs don't overlap. Can not split such effect.";
      return false;
    }
  }
  return true;
}

Strip *edit_strip_split(Main *bmain,
                        Scene *scene,
                        ListBase *seqbase,
                        Strip *strip,
                        const int timeline_frame,
                        const eSplitMethod method,
                        const bool ignore_connections,
                        const char **r_error)
{
  if (!seq_edit_split_intersect_check(scene, strip, timeline_frame)) {
    return nullptr;
  }

  /* Whole strip effect chain must be duplicated in order to preserve relationships. */
  VectorSet<Strip *> strips;
  strips.add(strip);
  iterator_set_expand(scene,
                      seqbase,
                      strips,
                      ignore_connections ? query_strip_effect_chain :
                                           query_strip_connected_and_effect_chain);

  if (!seq_edit_split_operation_permitted_check(scene, strips, timeline_frame, r_error)) {
    return nullptr;
  }

  /* Store `F-Curves`, so original ones aren't renamed. */
  AnimationBackup animation_backup{};
  animation_backup_original(scene, &animation_backup);

  ListBase left_strips = {nullptr, nullptr};
  for (Strip *strip_iter : strips) {
    /* Move strips in collection from seqbase to new ListBase. */
    BLI_remlink(seqbase, strip_iter);
    BLI_addtail(&left_strips, strip_iter);

    if (ignore_connections) {
      disconnect(strip_iter);
    }

    /* Duplicate curves from backup, so they can be renamed along with split strips. */
    animation_duplicate_backup_to_scene(scene, strip_iter, &animation_backup);
  }

  /* Duplicate ListBase. */
  ListBase right_strips = {nullptr, nullptr};
  seqbase_duplicate_recursive(
      bmain, scene, scene, &right_strips, &left_strips, StripDuplicate::All, 0);

  Strip *left_strip = static_cast<Strip *>(left_strips.first);
  Strip *right_strip = static_cast<Strip *>(right_strips.first);
  Strip *return_strip = nullptr;

  /* Move strips from detached `ListBase`, otherwise they can't be flagged for removal. */
  BLI_movelisttolist(seqbase, &left_strips);
  BLI_movelisttolist(seqbase, &right_strips);

  /* Rename duplicated strips. This has to be done immediately after adding
   * strips to seqbase, for lookup cache to work correctly. */
  Strip *strip_rename = right_strip;
  for (; strip_rename; strip_rename = strip_rename->next) {
    ensure_unique_name(strip_rename, scene);
  }

  /* Split strips. */
  while (left_strip && right_strip) {
    if (time_left_handle_frame_get(scene, left_strip) >= timeline_frame) {
      edit_flag_for_removal(scene, seqbase, left_strip);
    }
    else if (time_right_handle_frame_get(scene, right_strip) <= timeline_frame) {
      edit_flag_for_removal(scene, seqbase, right_strip);
    }
    else if (return_strip == nullptr) {
      /* Store return value - pointer to strip that will not be removed. */
      return_strip = right_strip;
    }

    seq_edit_split_handle_strip_offsets(
        bmain, scene, left_strip, right_strip, timeline_frame, method);
    left_strip = left_strip->next;
    right_strip = right_strip->next;
  }

  edit_remove_flagged_strips(scene, seqbase);
  animation_restore_original(scene, &animation_backup);

  return return_strip;
}

bool edit_remove_gaps(Scene *scene,
                      ListBase *seqbase,
                      const int initial_frame,
                      const bool remove_all_gaps)
{
  GapInfo gap_info = {0};
  seq_time_gap_info_get(scene, seqbase, initial_frame, &gap_info);

  if (!gap_info.gap_exists) {
    return false;
  }

  if (remove_all_gaps) {
    while (gap_info.gap_exists) {
      transform_offset_after_frame(scene, seqbase, -gap_info.gap_length, gap_info.gap_start_frame);
      seq_time_gap_info_get(scene, seqbase, initial_frame, &gap_info);
    }
  }
  else {
    transform_offset_after_frame(scene, seqbase, -gap_info.gap_length, gap_info.gap_start_frame);
  }
  return true;
}

void edit_strip_name_set(Scene *scene, Strip *strip, const char *new_name)
{
  BLI_strncpy_utf8(strip->name + 2, new_name, MAX_NAME - 2);
  BLI_str_utf8_invalid_strip(strip->name + 2, strlen(strip->name + 2));
  strip_lookup_invalidate(scene->ed);
}

}  // namespace blender::seq
