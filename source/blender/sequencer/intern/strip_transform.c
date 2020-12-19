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

#include "BKE_scene.h"
#include "BKE_sound.h"

#include "SEQ_effects.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

static int seq_tx_get_start(Sequence *seq)
{
  return seq->start;
}
static int seq_tx_get_end(Sequence *seq)
{
  return seq->start + seq->len;
}

int SEQ_transform_get_left_handle_frame(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return max_ii(SEQ_transform_get_left_handle_frame(seq, false),
                  SEQ_transform_get_left_handle_frame((Sequence *)seq->tmp, true));
  }

  return (seq->start - seq->startstill) + seq->startofs;
}
int SEQ_transform_get_right_handle_frame(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return min_ii(SEQ_transform_get_right_handle_frame(seq, false),
                  SEQ_transform_get_right_handle_frame((Sequence *)seq->tmp, true));
  }

  return ((seq->start + seq->len) + seq->endstill) - seq->endofs;
}

void SEQ_transform_set_left_handle_frame(Sequence *seq, int val)
{
  if (val < (seq)->start) {
    seq->startstill = abs(val - (seq)->start);
    seq->startofs = 0;
  }
  else {
    seq->startofs = abs(val - (seq)->start);
    seq->startstill = 0;
  }
}

void SEQ_transform_set_right_handle_frame(Sequence *seq, int val)
{
  if (val > (seq)->start + (seq)->len) {
    seq->endstill = abs(val - (seq->start + (seq)->len));
    seq->endofs = 0;
  }
  else {
    seq->endofs = abs(val - ((seq)->start + (seq)->len));
    seq->endstill = 0;
  }
}

/* used so we can do a quick check for single image seq
 * since they work a bit differently to normal image seq's (during transform) */
bool SEQ_transform_single_image_check(Sequence *seq)
{
  return ((seq->len == 1) &&
          (seq->type == SEQ_TYPE_IMAGE ||
           ((seq->type & SEQ_TYPE_EFFECT) && SEQ_effect_get_num_inputs(seq->type) == 0)));
}

/* check if the selected seq's reference unselected seq's */
bool SEQ_transform_seqbase_isolated_sel_check(ListBase *seqbase)
{
  Sequence *seq;
  /* is there more than 1 select */
  bool ok = false;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      ok = true;
      break;
    }
  }

  if (ok == false) {
    return false;
  }

  /* test relationships */
  for (seq = seqbase->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
      continue;
    }

    if (seq->flag & SELECT) {
      if ((seq->seq1 && (seq->seq1->flag & SELECT) == 0) ||
          (seq->seq2 && (seq->seq2->flag & SELECT) == 0) ||
          (seq->seq3 && (seq->seq3->flag & SELECT) == 0)) {
        return false;
      }
    }
    else {
      if ((seq->seq1 && (seq->seq1->flag & SELECT)) || (seq->seq2 && (seq->seq2->flag & SELECT)) ||
          (seq->seq3 && (seq->seq3->flag & SELECT))) {
        return false;
      }
    }
  }

  return true;
}

/**
 * Use to impose limits when dragging/extending - so impossible situations don't happen.
 * Cant use the #SEQ_LEFTSEL and #SEQ_LEFTSEL directly because the strip may be in a meta-strip.
 */
void SEQ_transform_handle_xlimits(Sequence *seq, int leftflag, int rightflag)
{
  if (leftflag) {
    if (SEQ_transform_get_left_handle_frame(seq, false) >=
        SEQ_transform_get_right_handle_frame(seq, false)) {
      SEQ_transform_set_left_handle_frame(seq,
                                          SEQ_transform_get_right_handle_frame(seq, false) - 1);
    }

    if (SEQ_transform_single_image_check(seq) == 0) {
      if (SEQ_transform_get_left_handle_frame(seq, false) >= seq_tx_get_end(seq)) {
        SEQ_transform_set_left_handle_frame(seq, seq_tx_get_end(seq) - 1);
      }

      /* doesn't work now - TODO */
#if 0
      if (seq_tx_get_start(seq) >= seq_tx_get_final_right(seq, 0)) {
        int ofs;
        ofs = seq_tx_get_start(seq) - seq_tx_get_final_right(seq, 0);
        seq->start -= ofs;
        seq_tx_set_final_left(seq, seq_tx_get_final_left(seq, 0) + ofs);
      }
#endif
    }
  }

  if (rightflag) {
    if (SEQ_transform_get_right_handle_frame(seq, false) <=
        SEQ_transform_get_left_handle_frame(seq, false)) {
      SEQ_transform_set_right_handle_frame(seq,
                                           SEQ_transform_get_left_handle_frame(seq, false) + 1);
    }

    if (SEQ_transform_single_image_check(seq) == 0) {
      if (SEQ_transform_get_right_handle_frame(seq, false) <= seq_tx_get_start(seq)) {
        SEQ_transform_set_right_handle_frame(seq, seq_tx_get_start(seq) + 1);
      }
    }
  }

  /* sounds cannot be extended past their endpoints */
  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seq->startstill = 0;
    seq->endstill = 0;
  }
}

void SEQ_transform_fix_single_image_seq_offsets(Sequence *seq)
{
  int left, start, offset;
  if (!SEQ_transform_single_image_check(seq)) {
    return;
  }

  /* make sure the image is always at the start since there is only one,
   * adjusting its start should be ok */
  left = SEQ_transform_get_left_handle_frame(seq, false);
  start = seq->start;
  if (start != left) {
    offset = left - start;
    SEQ_transform_set_left_handle_frame(seq,
                                        SEQ_transform_get_left_handle_frame(seq, false) - offset);
    SEQ_transform_set_right_handle_frame(
        seq, SEQ_transform_get_right_handle_frame(seq, false) - offset);
    seq->start += offset;
  }
}

bool SEQ_transform_sequence_can_be_translated(Sequence *seq)
{
  return !(seq->type & SEQ_TYPE_EFFECT) || (SEQ_effect_get_num_inputs(seq->type) == 0);
}

static bool seq_overlap(Sequence *seq1, Sequence *seq2)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((seq1->enddisp <= seq2->startdisp) || (seq1->startdisp >= seq2->enddisp)) == 0);
}

bool SEQ_transform_test_overlap(ListBase *seqbasep, Sequence *test)
{
  Sequence *seq;

  seq = seqbasep->first;
  while (seq) {
    if (seq_overlap(test, seq)) {
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

  SEQ_offset_animdata(evil_scene, seq, delta);
  seq->start += delta;

  if (seq->type == SEQ_TYPE_META) {
    Sequence *seq_child;
    for (seq_child = seq->seqbase.first; seq_child; seq_child = seq_child->next) {
      SEQ_transform_translate_sequence(evil_scene, seq_child, delta);
    }
  }

  SEQ_time_update_sequence_bounds(evil_scene, seq);
}

/* return 0 if there weren't enough space */
bool SEQ_transform_seqbase_shuffle_ex(ListBase *seqbasep,
                                      Sequence *test,
                                      Scene *evil_scene,
                                      int channel_delta)
{
  const int orig_machine = test->machine;
  BLI_assert(ELEM(channel_delta, -1, 1));

  test->machine += channel_delta;
  SEQ_time_update_sequence(evil_scene, test);
  while (SEQ_transform_test_overlap(seqbasep, test)) {
    if ((channel_delta > 0) ? (test->machine >= MAXSEQ) : (test->machine < 1)) {
      break;
    }

    test->machine += channel_delta;
    SEQ_time_update_sequence(
        evil_scene,
        test);  // XXX - I don't think this is needed since were only moving vertically, Campbell.
  }

  if ((test->machine < 1) || (test->machine > MAXSEQ)) {
    /* Blender 2.4x would remove the strip.
     * nicer to move it to the end */

    Sequence *seq;
    int new_frame = test->enddisp;

    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->machine == orig_machine) {
        new_frame = max_ii(new_frame, seq->enddisp);
      }
    }

    test->machine = orig_machine;
    new_frame = new_frame + (test->start - test->startdisp); /* adjust by the startdisp */
    SEQ_transform_translate_sequence(evil_scene, test, new_frame - test->start);

    SEQ_time_update_sequence(evil_scene, test);
    return false;
  }

  return true;
}

bool SEQ_transform_seqbase_shuffle(ListBase *seqbasep, Sequence *test, Scene *evil_scene)
{
  return SEQ_transform_seqbase_shuffle_ex(seqbasep, test, evil_scene, 1);
}

static int shuffle_seq_time_offset_test(ListBase *seqbasep, char dir)
{
  int offset = 0;
  Sequence *seq, *seq_other;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (seq->tmp) {
      for (seq_other = seqbasep->first; seq_other; seq_other = seq_other->next) {
        if (!seq_other->tmp && seq_overlap(seq, seq_other)) {
          if (dir == 'L') {
            offset = min_ii(offset, seq_other->startdisp - seq->enddisp);
          }
          else {
            offset = max_ii(offset, seq_other->enddisp - seq->startdisp);
          }
        }
      }
    }
  }
  return offset;
}

static int shuffle_seq_time_offset(Scene *scene, ListBase *seqbasep, char dir)
{
  int ofs = 0;
  int tot_ofs = 0;
  Sequence *seq;
  while ((ofs = shuffle_seq_time_offset_test(seqbasep, dir))) {
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->tmp) {
        /* seq_test_overlap only tests display values */
        seq->startdisp += ofs;
        seq->enddisp += ofs;
      }
    }

    tot_ofs += ofs;
  }

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (seq->tmp) {
      SEQ_time_update_sequence_bounds(scene, seq); /* corrects dummy startdisp/enddisp values */
    }
  }

  return tot_ofs;
}

bool SEQ_transform_seqbase_shuffle_time(ListBase *seqbasep,
                                        Scene *evil_scene,
                                        ListBase *markers,
                                        const bool use_sync_markers)
{
  /* note: seq->tmp is used to tag strips to move */

  Sequence *seq;

  int offset_l = shuffle_seq_time_offset(evil_scene, seqbasep, 'L');
  int offset_r = shuffle_seq_time_offset(evil_scene, seqbasep, 'R');
  int offset = (-offset_l < offset_r) ? offset_l : offset_r;

  if (offset) {
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->tmp) {
        SEQ_transform_translate_sequence(evil_scene, seq, offset);
        seq->flag &= ~SEQ_OVERLAP;
      }
    }

    if (use_sync_markers && !(evil_scene->toolsettings->lock_markers) && (markers != NULL)) {
      TimeMarker *marker;
      /* affect selected markers - it's unlikely that we will want to affect all in this way? */
      for (marker = markers->first; marker; marker = marker->next) {
        if (marker->flag & SELECT) {
          marker->frame += offset;
        }
      }
    }
  }

  return offset ? false : true;
}

/**
 * Move strips and markers (if not locked) that start after timeline_frame by delta frames
 *
 * \param scene: Scene in which strips are located
 * \param seqbase: ListBase in which strips are located
 * \param delta: offset in frames to be applied
 * \param timeline_frame: frame on timeline from where strips are moved
 */
void SEQ_transform_offset_after_frame(Scene *scene,
                                      ListBase *seqbase,
                                      const int delta,
                                      const int timeline_frame)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->startdisp >= timeline_frame) {
      SEQ_transform_translate_sequence(scene, seq, delta);
      SEQ_time_update_sequence(scene, seq);
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
