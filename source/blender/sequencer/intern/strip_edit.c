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

#include "BLT_translation.h"

#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "SEQ_sequencer.h"

int BKE_sequence_swap(Sequence *seq_a, Sequence *seq_b, const char **error_str)
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
      if (BKE_sequence_effect_get_num_inputs(seq_a->type) !=
          BKE_sequence_effect_get_num_inputs(seq_b->type)) {
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

  /* for sound we go over full meta tree to update muted state,
   * since sound is played outside of evaluating the imbufs, */
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

void BKE_sequencer_update_muting(Editing *ed)
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
    /* Look in metas for usage of seq. */
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

/* Flag seq and its users (effects) for removal. */
void BKE_sequencer_flag_for_removal(Scene *scene, ListBase *seqbase, Sequence *seq)
{
  if (seq == NULL || (seq->flag & SEQ_FLAG_DELETE) != 0) {
    return;
  }

  /* Flag and remove meta children. */
  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, meta_child, &seq->seqbase) {
      BKE_sequencer_flag_for_removal(scene, &seq->seqbase, meta_child);
    }
  }

  seq->flag |= SEQ_FLAG_DELETE;
  sequencer_flag_users_for_removal(scene, seqbase, seq);
}

/* Remove all flagged sequences, return true if sequence is removed. */
void BKE_sequencer_remove_flagged_sequences(Scene *scene, ListBase *seqbase)
{
  LISTBASE_FOREACH_MUTABLE (Sequence *, seq, seqbase) {
    if (seq->flag & SEQ_FLAG_DELETE) {
      if (seq->type == SEQ_TYPE_META) {
        BKE_sequencer_remove_flagged_sequences(scene, &seq->seqbase);
      }
      BLI_remlink(seqbase, seq);
      BKE_sequence_free(scene, seq, true);
    }
  }
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
  BKE_sequence_tx_set_final_right(seq, timeline_frame);
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
  BKE_sequence_tx_set_final_left(seq, timeline_frame);
}

/**
 * Split Sequence at timeline_frame in two.
 *
 * \param bmain: Main in which Sequence is located
 * \param scene: Scene in which Sequence is located
 * \param seqbase: ListBase in which Sequence is located
 * \param seq: Sequence to be split
 * \param timeline_frame: frame at which seq is split.
 * \param method: affects type of offset to be applied to resize Sequence
 * \return The newly created sequence strip. This is always Sequence on right side.
 */
Sequence *SEQ_edit_strip_split(Main *bmain,
                               Scene *scene,
                               ListBase *seqbase,
                               Sequence *seq,
                               const int timeline_frame,
                               const eSeqSplitMethod method)
{
  if (timeline_frame <= seq->startdisp || timeline_frame >= seq->enddisp) {
    return NULL;
  }

  if (method == SEQ_SPLIT_HARD) {
    /* Precaution, needed because the length saved on-disk may not match the length saved in the
     * blend file, or our code may have minor differences reading file length between versions.
     * This causes hard-split to fail, see: T47862. */
    BKE_sequence_reload_new_file(bmain, scene, seq, true);
    BKE_sequence_calc(scene, seq);
  }

  Sequence *left_seq = seq;
  Sequence *right_seq = BKE_sequence_dupli_recursive(
      scene, scene, seqbase, seq, SEQ_DUPE_UNIQUE_NAME | SEQ_DUPE_ANIM);

  switch (method) {
    case SEQ_SPLIT_SOFT:
      seq_split_set_left_offset(right_seq, timeline_frame);
      seq_split_set_right_offset(left_seq, timeline_frame);
      break;
    case SEQ_SPLIT_HARD:
      seq_split_set_right_hold_offset(left_seq, timeline_frame);
      seq_split_set_left_hold_offset(right_seq, timeline_frame);
      BKE_sequence_reload_new_file(bmain, scene, left_seq, false);
      BKE_sequence_reload_new_file(bmain, scene, right_seq, false);
      break;
  }
  BKE_sequence_calc(scene, left_seq);
  BKE_sequence_calc(scene, right_seq);
  return right_seq;
}
