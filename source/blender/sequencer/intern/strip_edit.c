/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Foundation.
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
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.h"

#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "strip_time.h"
#include "utils.h"

#include "SEQ_add.h"
#include "SEQ_animation.h"
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

bool SEQ_edit_sequence_swap(Scene *scene, Sequence *seq_a, Sequence *seq_b, const char **error_str)
{
  char name[sizeof(seq_a->name)];

  if (SEQ_time_strip_length_get(scene, seq_a) != SEQ_time_strip_length_get(scene, seq_b)) {
    *error_str = N_("Strips must be the same length");
    return false;
  }

  /* type checking, could be more advanced but disallow sound vs non-sound copy */
  if (seq_a->type != seq_b->type) {
    if (seq_a->type == SEQ_TYPE_SOUND_RAM || seq_b->type == SEQ_TYPE_SOUND_RAM) {
      *error_str = N_("Strips were not compatible");
      return false;
    }

    /* disallow effects to swap with non-effects strips */
    if ((seq_a->type & SEQ_TYPE_EFFECT) != (seq_b->type & SEQ_TYPE_EFFECT)) {
      *error_str = N_("Strips were not compatible");
      return false;
    }

    if ((seq_a->type & SEQ_TYPE_EFFECT) && (seq_b->type & SEQ_TYPE_EFFECT)) {
      if (SEQ_effect_get_num_inputs(seq_a->type) != SEQ_effect_get_num_inputs(seq_b->type)) {
        *error_str = N_("Strips must have the same number of inputs");
        return false;
      }
    }
  }

  SWAP(Sequence, *seq_a, *seq_b);

  /* swap back names so animation fcurves don't get swapped */
  STRNCPY(name, seq_a->name + 2);
  BLI_strncpy(seq_a->name + 2, seq_b->name + 2, sizeof(seq_b->name) - 2);
  BLI_strncpy(seq_b->name + 2, name, sizeof(seq_b->name) - 2);

  /* swap back opacity, and overlay mode */
  SWAP(int, seq_a->blend_mode, seq_b->blend_mode);
  SWAP(float, seq_a->blend_opacity, seq_b->blend_opacity);

  SWAP(Sequence *, seq_a->prev, seq_b->prev);
  SWAP(Sequence *, seq_a->next, seq_b->next);
  SWAP(float, seq_a->start, seq_b->start);
  SWAP(float, seq_a->startofs, seq_b->startofs);
  SWAP(float, seq_a->endofs, seq_b->endofs);
  SWAP(int, seq_a->machine, seq_b->machine);
  seq_time_effect_range_set(scene, seq_a);
  seq_time_effect_range_set(scene, seq_b);

  return true;
}

static void seq_update_muting_recursive(ListBase *channels,
                                        ListBase *seqbasep,
                                        Sequence *metaseq,
                                        const bool mute)
{
  Sequence *seq;

  /* For sound we go over full meta tree to update muted state,
   * since sound is played outside of evaluating the imbufs. */
  for (seq = seqbasep->first; seq; seq = seq->next) {
    bool seqmute = (mute || SEQ_render_is_muted(channels, seq));

    if (seq->type == SEQ_TYPE_META) {
      /* if this is the current meta sequence, unmute because
       * all sequences above this were set to mute */
      if (seq == metaseq) {
        seqmute = false;
      }

      seq_update_muting_recursive(&seq->channels, &seq->seqbase, metaseq, seqmute);
    }
    else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      if (seq->scene_sound) {
        BKE_sound_mute_scene_sound(seq->scene_sound, seqmute);
      }
    }
  }
}

void SEQ_edit_update_muting(Editing *ed)
{
  if (ed) {
    /* mute all sounds up to current metastack list */
    MetaStack *ms = ed->metastack.last;

    if (ms) {
      seq_update_muting_recursive(&ed->channels, &ed->seqbase, ms->parseq, true);
    }
    else {
      seq_update_muting_recursive(&ed->channels, &ed->seqbase, NULL, false);
    }
  }
}

static void sequencer_flag_users_for_removal(Scene *scene, ListBase *seqbase, Sequence *seq)
{
  LISTBASE_FOREACH (Sequence *, user_seq, seqbase) {
    /* Look in meta-strips for usage of seq. */
    if (user_seq->type == SEQ_TYPE_META) {
      sequencer_flag_users_for_removal(scene, &user_seq->seqbase, seq);
    }

    /* Clear seq from modifiers. */
    SequenceModifierData *smd;
    for (smd = user_seq->modifiers.first; smd; smd = smd->next) {
      if (smd->mask_sequence == seq) {
        smd->mask_sequence = NULL;
      }
    }

    /* Remove effects, that use seq. */
    if (SEQ_relation_is_effect_of_strip(user_seq, seq)) {
      user_seq->flag |= SEQ_FLAG_DELETE;
      /* Strips can be used as mask even if not in same seqbase. */
      sequencer_flag_users_for_removal(scene, &scene->ed->seqbase, user_seq);
    }
  }
}

void SEQ_edit_flag_for_removal(Scene *scene, ListBase *seqbase, Sequence *seq)
{
  if (seq == NULL || (seq->flag & SEQ_FLAG_DELETE) != 0) {
    return;
  }

  /* Flag and remove meta children. */
  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, meta_child, &seq->seqbase) {
      SEQ_edit_flag_for_removal(scene, &seq->seqbase, meta_child);
    }
  }

  seq->flag |= SEQ_FLAG_DELETE;
  sequencer_flag_users_for_removal(scene, seqbase, seq);
}

void SEQ_edit_remove_flagged_sequences(Scene *scene, ListBase *seqbase)
{
  LISTBASE_FOREACH_MUTABLE (Sequence *, seq, seqbase) {
    if (seq->flag & SEQ_FLAG_DELETE) {
      if (seq->type == SEQ_TYPE_META) {
        SEQ_edit_remove_flagged_sequences(scene, &seq->seqbase);
      }
      SEQ_free_animdata(scene, seq);
      BLI_remlink(seqbase, seq);
      SEQ_sequence_free(scene, seq);
      SEQ_sequence_lookup_tag(scene, SEQ_LOOKUP_TAG_INVALID);
    }
  }
}

bool SEQ_edit_move_strip_to_seqbase(Scene *scene,
                                    ListBase *seqbase,
                                    Sequence *seq,
                                    ListBase *dst_seqbase)
{
  /* Move to meta. */
  BLI_remlink(seqbase, seq);
  BLI_addtail(dst_seqbase, seq);
  SEQ_relations_invalidate_cache_preprocessed(scene, seq);

  /* Update meta. */
  if (SEQ_transform_test_overlap(scene, dst_seqbase, seq)) {
    SEQ_transform_seqbase_shuffle(dst_seqbase, seq, scene);
  }

  return true;
}

bool SEQ_edit_move_strip_to_meta(Scene *scene,
                                 Sequence *src_seq,
                                 Sequence *dst_seqm,
                                 const char **error_str)
{
  /* Find the appropriate seqbase */
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_get_seqbase_by_seq(scene, src_seq);

  if (dst_seqm->type != SEQ_TYPE_META) {
    *error_str = N_("Can not move strip to non-meta strip");
    return false;
  }

  if (src_seq == dst_seqm) {
    *error_str = N_("Strip can not be moved into itself");
    return false;
  }

  if (seqbase == &dst_seqm->seqbase) {
    *error_str = N_("Moved strip is already inside provided meta strip");
    return false;
  }

  if (src_seq->type == SEQ_TYPE_META && SEQ_exists_in_seqbase(dst_seqm, &src_seq->seqbase)) {
    *error_str = N_("Moved strip is parent of provided meta strip");
    return false;
  }

  if (!SEQ_exists_in_seqbase(dst_seqm, &ed->seqbase)) {
    *error_str = N_("Can not move strip to different scene");
    return false;
  }

  SeqCollection *collection = SEQ_collection_create(__func__);
  SEQ_collection_append_strip(src_seq, collection);
  SEQ_collection_expand(scene, seqbase, collection, SEQ_query_strip_effect_chain);

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    /* Move to meta. */
    SEQ_edit_move_strip_to_seqbase(scene, seqbase, seq, &dst_seqm->seqbase);
  }

  SEQ_collection_free(collection);

  return true;
}

static void seq_split_set_right_hold_offset(Main *bmain,
                                            Scene *scene,
                                            Sequence *seq,
                                            int timeline_frame)
{
  const float content_start = SEQ_time_start_frame_get(seq);
  const float content_end = SEQ_time_content_end_frame_get(scene, seq);

  /* Adjust within range of extended still-frames before strip. */
  if (timeline_frame < content_start) {
    const float offset = content_start + 1 - timeline_frame;
    seq->start -= offset;
    seq->startofs += offset;
  }
  /* Adjust within range of strip contents. */
  else if ((timeline_frame >= content_start) && (timeline_frame <= content_end)) {
    seq->endofs = 0;
    float speed_factor = (seq->type == SEQ_TYPE_SOUND_RAM) ?
                             seq_time_media_playback_rate_factor_get(scene, seq) :
                             seq_time_playback_rate_factor_get(scene, seq);
    seq->anim_endofs += round_fl_to_int((content_end - timeline_frame) * speed_factor);
  }

  /* Needed only to set `seq->len`. */
  SEQ_add_reload_new_file(bmain, scene, seq, false);
  SEQ_time_right_handle_frame_set(scene, seq, timeline_frame);
}

static void seq_split_set_left_hold_offset(Main *bmain,
                                           Scene *scene,
                                           Sequence *seq,
                                           int timeline_frame)
{
  const float content_start = SEQ_time_start_frame_get(seq);
  const float content_end = SEQ_time_content_end_frame_get(scene, seq);

  /* Adjust within range of strip contents. */
  if ((timeline_frame >= content_start) && (timeline_frame <= content_end)) {
    float speed_factor = (seq->type == SEQ_TYPE_SOUND_RAM) ?
                             seq_time_media_playback_rate_factor_get(scene, seq) :
                             seq_time_playback_rate_factor_get(scene, seq);
    seq->anim_startofs += round_fl_to_int((timeline_frame - content_start) * speed_factor);
    seq->start = timeline_frame;
    seq->startofs = 0;
  }
  /* Adjust within range of extended still-frames after strip. */
  else if (timeline_frame > content_end) {
    const float offset = timeline_frame - content_end + 1;
    seq->start += offset;
    seq->endofs += offset;
  }

  /* Needed only to set `seq->len`. */
  SEQ_add_reload_new_file(bmain, scene, seq, false);
  SEQ_time_left_handle_frame_set(scene, seq, timeline_frame);
}

static bool seq_edit_split_effect_intersect_check(const Scene *scene,
                                                  const Sequence *seq,
                                                  const int timeline_frame)
{
  return timeline_frame > SEQ_time_left_handle_frame_get(scene, seq) &&
         timeline_frame < SEQ_time_right_handle_frame_get(scene, seq);
}

static void seq_edit_split_handle_strip_offsets(Main *bmain,
                                                Scene *scene,
                                                Sequence *left_seq,
                                                Sequence *right_seq,
                                                const int timeline_frame,
                                                const eSeqSplitMethod method)
{
  if (seq_edit_split_effect_intersect_check(scene, right_seq, timeline_frame)) {
    switch (method) {
      case SEQ_SPLIT_SOFT:
        SEQ_time_left_handle_frame_set(scene, right_seq, timeline_frame);
        break;
      case SEQ_SPLIT_HARD:
        seq_split_set_left_hold_offset(bmain, scene, right_seq, timeline_frame);
        break;
    }
  }

  if (seq_edit_split_effect_intersect_check(scene, left_seq, timeline_frame)) {
    switch (method) {
      case SEQ_SPLIT_SOFT:
        SEQ_time_right_handle_frame_set(scene, left_seq, timeline_frame);
        break;
      case SEQ_SPLIT_HARD:
        seq_split_set_right_hold_offset(bmain, scene, left_seq, timeline_frame);
        break;
    }
  }
}

static bool seq_edit_split_effect_inputs_intersect(const Scene *scene,
                                                   const Sequence *seq,
                                                   const int timeline_frame)
{
  bool input_does_intersect = false;
  if (seq->seq1) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(
        scene, seq->seq1, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(
          scene, seq->seq1, timeline_frame);
    }
  }
  if (seq->seq2) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(
        scene, seq->seq2, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(
          scene, seq->seq2, timeline_frame);
    }
  }
  if (seq->seq3) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(
        scene, seq->seq3, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(
          scene, seq->seq3, timeline_frame);
    }
  }
  return input_does_intersect;
}

static bool seq_edit_split_operation_permitted_check(const Scene *scene,
                                                     SeqCollection *strips,
                                                     const int timeline_frame,
                                                     const char **r_error)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, strips) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
      continue;
    }
    if (!seq_edit_split_effect_intersect_check(scene, seq, timeline_frame)) {
      continue;
    }
    if (SEQ_effect_get_num_inputs(seq->type) <= 1) {
      continue;
    }
    if (ELEM(seq->type, SEQ_TYPE_CROSS, SEQ_TYPE_GAMCROSS, SEQ_TYPE_WIPE)) {
      *r_error = "Splitting transition effect is not permitted.";
      return false;
    }
    if (!seq_edit_split_effect_inputs_intersect(scene, seq, timeline_frame)) {
      *r_error = "Effect inputs don't overlap. Can not split such effect.";
      return false;
    }
  }
  return true;
}

Sequence *SEQ_edit_strip_split(Main *bmain,
                               Scene *scene,
                               ListBase *seqbase,
                               Sequence *seq,
                               const int timeline_frame,
                               const eSeqSplitMethod method,
                               const char **r_error)
{
  if (!seq_edit_split_effect_intersect_check(scene, seq, timeline_frame)) {
    return NULL;
  }

  /* Whole strip chain must be duplicated in order to preserve relationships. */
  SeqCollection *collection = SEQ_collection_create(__func__);
  SEQ_collection_append_strip(seq, collection);
  SEQ_collection_expand(scene, seqbase, collection, SEQ_query_strip_effect_chain);

  if (!seq_edit_split_operation_permitted_check(scene, collection, timeline_frame, r_error)) {
    SEQ_collection_free(collection);
    return NULL;
  }

  /* Store `F-Curves`, so original ones aren't renamed. */
  SeqAnimationBackup animation_backup = {0};
  SEQ_animation_backup_original(scene, &animation_backup);

  ListBase left_strips = {NULL, NULL};
  SEQ_ITERATOR_FOREACH (seq, collection) {
    /* Move strips in collection from seqbase to new ListBase. */
    BLI_remlink(seqbase, seq);
    BLI_addtail(&left_strips, seq);

    /* Duplicate curves from backup, so they can be renamed along with split strips. */
    SEQ_animation_duplicate_backup_to_scene(scene, seq, &animation_backup);
  }

  SEQ_collection_free(collection);

  /* Duplicate ListBase. */
  ListBase right_strips = {NULL, NULL};
  SEQ_sequence_base_dupli_recursive(scene, scene, &right_strips, &left_strips, SEQ_DUPE_ALL, 0);

  Sequence *left_seq = left_strips.first;
  Sequence *right_seq = right_strips.first;
  Sequence *return_seq = NULL;

  /* Move strips from detached `ListBase`, otherwise they can't be flagged for removal. */
  BLI_movelisttolist(seqbase, &left_strips);
  BLI_movelisttolist(seqbase, &right_strips);

  /* Rename duplicated strips. This has to be done immediately after adding
   * strips to seqbase, for lookup cache to work correctly. */
  Sequence *seq_rename = right_seq;
  for (; seq_rename; seq_rename = seq_rename->next) {
    SEQ_ensure_unique_name(seq_rename, scene);
  }

  /* Split strips. */
  while (left_seq && right_seq) {
    if (SEQ_time_left_handle_frame_get(scene, left_seq) >= timeline_frame) {
      SEQ_edit_flag_for_removal(scene, seqbase, left_seq);
    }
    else if (SEQ_time_right_handle_frame_get(scene, right_seq) <= timeline_frame) {
      SEQ_edit_flag_for_removal(scene, seqbase, right_seq);
    }
    else if (return_seq == NULL) {
      /* Store return value - pointer to strip that will not be removed. */
      return_seq = right_seq;
    }

    seq_edit_split_handle_strip_offsets(bmain, scene, left_seq, right_seq, timeline_frame, method);
    left_seq = left_seq->next;
    right_seq = right_seq->next;
  }

  SEQ_edit_remove_flagged_sequences(scene, seqbase);
  SEQ_animation_restore_original(scene, &animation_backup);

  return return_seq;
}

bool SEQ_edit_remove_gaps(Scene *scene,
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
      SEQ_transform_offset_after_frame(
          scene, seqbase, -gap_info.gap_length, gap_info.gap_start_frame);
      seq_time_gap_info_get(scene, seqbase, initial_frame, &gap_info);
    }
  }
  else {
    SEQ_transform_offset_after_frame(
        scene, seqbase, -gap_info.gap_length, gap_info.gap_start_frame);
  }
  return true;
}

void SEQ_edit_sequence_name_set(Scene *scene, Sequence *seq, const char *new_name)
{
  BLI_strncpy_utf8(seq->name + 2, new_name, MAX_NAME - 2);
  BLI_str_utf8_invalid_strip(seq->name + 2, strlen(seq->name + 2));
  SEQ_sequence_lookup_tag(scene, SEQ_LOOKUP_TAG_INVALID);
}
