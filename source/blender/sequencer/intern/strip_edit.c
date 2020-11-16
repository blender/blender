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
