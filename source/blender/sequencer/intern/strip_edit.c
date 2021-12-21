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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

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
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

int SEQ_edit_sequence_swap(Sequence *seq_a, Sequence *seq_b, const char **error_str)
{
  char name[sizeof(seq_a->name)];

  if (seq_a->len != seq_b->len) {
    *error_str = N_("Strips must be the same length");
    return 0;
  }

  /* type checking, could be more advanced but disallow sound vs non-sound copy */
  if (seq_a->type != seq_b->type) {
    if (seq_a->type == SEQ_TYPE_SOUND_RAM || seq_b->type == SEQ_TYPE_SOUND_RAM) {
      *error_str = N_("Strips were not compatible");
      return 0;
    }

    /* disallow effects to swap with non-effects strips */
    if ((seq_a->type & SEQ_TYPE_EFFECT) != (seq_b->type & SEQ_TYPE_EFFECT)) {
      *error_str = N_("Strips were not compatible");
      return 0;
    }

    if ((seq_a->type & SEQ_TYPE_EFFECT) && (seq_b->type & SEQ_TYPE_EFFECT)) {
      if (SEQ_effect_get_num_inputs(seq_a->type) != SEQ_effect_get_num_inputs(seq_b->type)) {
        *error_str = N_("Strips must have the same number of inputs");
        return 0;
      }
    }
  }

  SWAP(Sequence, *seq_a, *seq_b);

  /* swap back names so animation fcurves don't get swapped */
  BLI_strncpy(name, seq_a->name + 2, sizeof(name));
  BLI_strncpy(seq_a->name + 2, seq_b->name + 2, sizeof(seq_b->name) - 2);
  BLI_strncpy(seq_b->name + 2, name, sizeof(seq_b->name) - 2);

  /* swap back opacity, and overlay mode */
  SWAP(int, seq_a->blend_mode, seq_b->blend_mode);
  SWAP(float, seq_a->blend_opacity, seq_b->blend_opacity);

  SWAP(Sequence *, seq_a->prev, seq_b->prev);
  SWAP(Sequence *, seq_a->next, seq_b->next);
  SWAP(int, seq_a->start, seq_b->start);
  SWAP(int, seq_a->startofs, seq_b->startofs);
  SWAP(int, seq_a->endofs, seq_b->endofs);
  SWAP(int, seq_a->startstill, seq_b->startstill);
  SWAP(int, seq_a->endstill, seq_b->endstill);
  SWAP(int, seq_a->machine, seq_b->machine);
  SWAP(int, seq_a->startdisp, seq_b->startdisp);
  SWAP(int, seq_a->enddisp, seq_b->enddisp);

  return 1;
}

static void seq_update_muting_recursive(ListBase *seqbasep, Sequence *metaseq, int mute)
{
  Sequence *seq;
  int seqmute;

  /* For sound we go over full meta tree to update muted state,
   * since sound is played outside of evaluating the imbufs. */
  for (seq = seqbasep->first; seq; seq = seq->next) {
    seqmute = (mute || (seq->flag & SEQ_MUTE));

    if (seq->type == SEQ_TYPE_META) {
      /* if this is the current meta sequence, unmute because
       * all sequences above this were set to mute */
      if (seq == metaseq) {
        seqmute = 0;
      }

      seq_update_muting_recursive(&seq->seqbase, metaseq, seqmute);
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
      seq_update_muting_recursive(&ed->seqbase, ms->parseq, 1);
    }
    else {
      seq_update_muting_recursive(&ed->seqbase, NULL, 0);
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
    if ((user_seq->seq1 && user_seq->seq1 == seq) || (user_seq->seq2 && user_seq->seq2 == seq) ||
        (user_seq->seq3 && user_seq->seq3 == seq)) {
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
      BLI_remlink(seqbase, seq);
      SEQ_sequence_free(scene, seq, true);
      SEQ_sequence_lookup_tag(scene, SEQ_LOOKUP_TAG_INVALID);
    }
  }
}

static bool seq_exists_in_seqbase(Sequence *seq, ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->type == SEQ_TYPE_META && seq_exists_in_seqbase(seq, &seq_test->seqbase)) {
      return true;
    }
    if (seq_test == seq) {
      return true;
    }
  }
  return false;
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
  if (SEQ_transform_test_overlap(dst_seqbase, seq)) {
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
  ListBase *seqbase = SEQ_get_seqbase_by_seq(&ed->seqbase, src_seq);

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

  if (src_seq->type == SEQ_TYPE_META && seq_exists_in_seqbase(dst_seqm, &src_seq->seqbase)) {
    *error_str = N_("Moved strip is parent of provided meta strip");
    return false;
  }

  if (!seq_exists_in_seqbase(dst_seqm, &ed->seqbase)) {
    *error_str = N_("Can not move strip to different scene");
    return false;
  }

  SeqCollection *collection = SEQ_collection_create(__func__);
  SEQ_collection_append_strip(src_seq, collection);
  SEQ_collection_expand(seqbase, collection, SEQ_query_strip_effect_chain);

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    /* Move to meta. */
    SEQ_edit_move_strip_to_seqbase(scene, seqbase, seq, &dst_seqm->seqbase);
  }

  SEQ_collection_free(collection);

  return true;
}

static void seq_split_set_left_hold_offset(Sequence *seq, int timeline_frame)
{
  /* Adjust within range of extended stillframes before strip. */
  if (timeline_frame < seq->start) {
    seq->start = timeline_frame - 1;
    seq->anim_endofs += seq->len - 1;
    seq->startstill = timeline_frame - seq->startdisp - 1;
    seq->endstill = 0;
  }
  /* Adjust within range of strip contents. */
  else if ((timeline_frame >= seq->start) && (timeline_frame <= (seq->start + seq->len))) {
    seq->endofs = 0;
    seq->endstill = 0;
    seq->anim_endofs += (seq->start + seq->len) - timeline_frame;
  }
  /* Adjust within range of extended stillframes after strip. */
  else if ((seq->start + seq->len) < timeline_frame) {
    seq->endstill = timeline_frame - seq->start - seq->len;
  }
}

static void seq_split_set_right_hold_offset(Sequence *seq, int timeline_frame)
{
  /* Adjust within range of extended stillframes before strip. */
  if (timeline_frame < seq->start) {
    seq->startstill = seq->start - timeline_frame;
  }
  /* Adjust within range of strip contents. */
  else if ((timeline_frame >= seq->start) && (timeline_frame <= (seq->start + seq->len))) {
    seq->anim_startofs += timeline_frame - seq->start;
    seq->start = timeline_frame;
    seq->startstill = 0;
    seq->startofs = 0;
  }
  /* Adjust within range of extended stillframes after strip. */
  else if ((seq->start + seq->len) < timeline_frame) {
    seq->start = timeline_frame;
    seq->startofs = 0;
    seq->anim_startofs += seq->len - 1;
    seq->endstill = seq->enddisp - timeline_frame - 1;
    seq->startstill = 0;
  }
}

static void seq_split_set_right_offset(Sequence *seq, int timeline_frame)
{
  /* Adjust within range of extended stillframes before strip. */
  if (timeline_frame < seq->start) {
    seq->start = timeline_frame - 1;
    seq->startstill = timeline_frame - seq->startdisp - 1;
    seq->endofs = seq->len - 1;
  }
  /* Adjust within range of extended stillframes after strip. */
  else if ((seq->start + seq->len) < timeline_frame) {
    seq->endstill -= seq->enddisp - timeline_frame;
  }
  SEQ_transform_set_right_handle_frame(seq, timeline_frame);
}

static void seq_split_set_left_offset(Sequence *seq, int timeline_frame)
{
  /* Adjust within range of extended stillframes before strip. */
  if (timeline_frame < seq->start) {
    seq->startstill = seq->start - timeline_frame;
  }
  /* Adjust within range of extended stillframes after strip. */
  if ((seq->start + seq->len) < timeline_frame) {
    seq->start = timeline_frame - seq->len + 1;
    seq->endstill = seq->enddisp - timeline_frame - 1;
  }
  SEQ_transform_set_left_handle_frame(seq, timeline_frame);
}

static bool seq_edit_split_effect_intersect_check(const Sequence *seq, const int timeline_frame)
{
  return timeline_frame > seq->startdisp && timeline_frame < seq->enddisp;
}

static void seq_edit_split_handle_strip_offsets(Main *bmain,
                                                Scene *scene,
                                                ListBase *seqbase,
                                                Sequence *left_seq,
                                                Sequence *right_seq,
                                                const int timeline_frame,
                                                const eSeqSplitMethod method)
{
  if (seq_edit_split_effect_intersect_check(right_seq, timeline_frame)) {
    switch (method) {
      case SEQ_SPLIT_SOFT:
        seq_split_set_left_offset(right_seq, timeline_frame);
        break;
      case SEQ_SPLIT_HARD:
        seq_split_set_left_hold_offset(right_seq, timeline_frame);
        SEQ_add_reload_new_file(bmain, scene, right_seq, false);
        break;
    }
    SEQ_time_update_sequence(scene, seqbase, right_seq);
  }

  if (seq_edit_split_effect_intersect_check(left_seq, timeline_frame)) {
    switch (method) {
      case SEQ_SPLIT_SOFT:
        seq_split_set_right_offset(left_seq, timeline_frame);
        break;
      case SEQ_SPLIT_HARD:
        seq_split_set_right_hold_offset(left_seq, timeline_frame);
        SEQ_add_reload_new_file(bmain, scene, left_seq, false);
        break;
    }
    SEQ_time_update_sequence(scene, seqbase, left_seq);
  }
}

static bool seq_edit_split_effect_inputs_intersect(const Sequence *seq, const int timeline_frame)
{
  bool input_does_intersect = false;
  if (seq->seq1) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(seq->seq1, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(seq->seq1, timeline_frame);
    }
  }
  if (seq->seq2) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(seq->seq2, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(seq->seq2, timeline_frame);
    }
  }
  if (seq->seq3) {
    input_does_intersect |= seq_edit_split_effect_intersect_check(seq->seq3, timeline_frame);
    if ((seq->seq1->type & SEQ_TYPE_EFFECT) != 0) {
      input_does_intersect |= seq_edit_split_effect_inputs_intersect(seq->seq3, timeline_frame);
    }
  }
  return input_does_intersect;
}

static bool seq_edit_split_operation_permitted_check(SeqCollection *strips,
                                                     const int timeline_frame,
                                                     const char **r_error)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, strips) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
      continue;
    }
    if (!seq_edit_split_effect_intersect_check(seq, timeline_frame)) {
      continue;
    }
    if (SEQ_effect_get_num_inputs(seq->type) <= 1) {
      continue;
    }
    if (ELEM(seq->type, SEQ_TYPE_CROSS, SEQ_TYPE_GAMCROSS, SEQ_TYPE_WIPE)) {
      *r_error = "Splitting transition effect is not permitted.";
      return false;
    }
    if (!seq_edit_split_effect_inputs_intersect(seq, timeline_frame)) {
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
  if (!seq_edit_split_effect_intersect_check(seq, timeline_frame)) {
    return NULL;
  }

  /* Whole strip chain must be duplicated in order to preserve relationships. */
  SeqCollection *collection = SEQ_collection_create(__func__);
  SEQ_collection_append_strip(seq, collection);
  SEQ_collection_expand(seqbase, collection, SEQ_query_strip_effect_chain);

  if (!seq_edit_split_operation_permitted_check(collection, timeline_frame, r_error)) {
    SEQ_collection_free(collection);
    return NULL;
  }

  /* Move strips in collection from seqbase to new ListBase. */
  ListBase left_strips = {NULL, NULL};
  SEQ_ITERATOR_FOREACH (seq, collection) {
    BLI_remlink(seqbase, seq);
    BLI_addtail(&left_strips, seq);
  }

  SEQ_collection_free(collection);

  /* Sort list, so that no strip can depend on next strip in list.
   * This is important for SEQ_time_update_sequence functionality. */
  SEQ_sort(&left_strips);

  /* Duplicate ListBase. */
  ListBase right_strips = {NULL, NULL};
  SEQ_sequence_base_dupli_recursive(scene, scene, &right_strips, &left_strips, SEQ_DUPE_ALL, 0);

  Sequence *left_seq = left_strips.first;
  Sequence *right_seq = right_strips.first;
  Sequence *return_seq = NULL;

  /* Move strips from detached `ListBase`, otherwise they can't be flagged for removal,
   * SEQ_time_update_sequence can fail to update meta strips and they can't be renamed.
   * This is because these functions check all strips in `Editing` to manage relationships. */
  BLI_movelisttolist(seqbase, &left_strips);
  BLI_movelisttolist(seqbase, &right_strips);

  /* Split strips. */
  while (left_seq && right_seq) {
    if (left_seq->startdisp >= timeline_frame) {
      SEQ_edit_flag_for_removal(scene, seqbase, left_seq);
    }
    if (right_seq->enddisp <= timeline_frame) {
      SEQ_edit_flag_for_removal(scene, seqbase, right_seq);
    }
    else if (return_seq == NULL) {
      /* Store return value - pointer to strip that will not be removed. */
      return_seq = right_seq;
    }

    seq_edit_split_handle_strip_offsets(
        bmain, scene, seqbase, left_seq, right_seq, timeline_frame, method);
    left_seq = left_seq->next;
    right_seq = right_seq->next;
  }

  SEQ_edit_remove_flagged_sequences(scene, seqbase);

  /* Rename duplicated strips. */
  Sequence *seq_rename = return_seq;
  for (; seq_rename; seq_rename = seq_rename->next) {
    SEQ_ensure_unique_name(seq_rename, scene);
  }

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
