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

#include "SEQ_sequencer.h"

static int seq_tx_get_start(Sequence *seq)
{
  return seq->start;
}
static int seq_tx_get_end(Sequence *seq)
{
  return seq->start + seq->len;
}

int BKE_sequence_tx_get_final_left(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return max_ii(BKE_sequence_tx_get_final_left(seq, false),
                  BKE_sequence_tx_get_final_left((Sequence *)seq->tmp, true));
  }

  return (seq->start - seq->startstill) + seq->startofs;
}
int BKE_sequence_tx_get_final_right(Sequence *seq, bool metaclip)
{
  if (metaclip && seq->tmp) {
    /* return the range clipped by the parents range */
    return min_ii(BKE_sequence_tx_get_final_right(seq, false),
                  BKE_sequence_tx_get_final_right((Sequence *)seq->tmp, true));
  }

  return ((seq->start + seq->len) + seq->endstill) - seq->endofs;
}

void BKE_sequence_tx_set_final_left(Sequence *seq, int val)
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

void BKE_sequence_tx_set_final_right(Sequence *seq, int val)
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
bool BKE_sequence_single_check(Sequence *seq)
{
  return ((seq->len == 1) &&
          (seq->type == SEQ_TYPE_IMAGE ||
           ((seq->type & SEQ_TYPE_EFFECT) && BKE_sequence_effect_get_num_inputs(seq->type) == 0)));
}

/* check if the selected seq's reference unselected seq's */
bool BKE_sequence_base_isolated_sel_check(ListBase *seqbase)
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
void BKE_sequence_tx_handle_xlimits(Sequence *seq, int leftflag, int rightflag)
{
  if (leftflag) {
    if (BKE_sequence_tx_get_final_left(seq, false) >=
        BKE_sequence_tx_get_final_right(seq, false)) {
      BKE_sequence_tx_set_final_left(seq, BKE_sequence_tx_get_final_right(seq, false) - 1);
    }

    if (BKE_sequence_single_check(seq) == 0) {
      if (BKE_sequence_tx_get_final_left(seq, false) >= seq_tx_get_end(seq)) {
        BKE_sequence_tx_set_final_left(seq, seq_tx_get_end(seq) - 1);
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
    if (BKE_sequence_tx_get_final_right(seq, false) <=
        BKE_sequence_tx_get_final_left(seq, false)) {
      BKE_sequence_tx_set_final_right(seq, BKE_sequence_tx_get_final_left(seq, false) + 1);
    }

    if (BKE_sequence_single_check(seq) == 0) {
      if (BKE_sequence_tx_get_final_right(seq, false) <= seq_tx_get_start(seq)) {
        BKE_sequence_tx_set_final_right(seq, seq_tx_get_start(seq) + 1);
      }
    }
  }

  /* sounds cannot be extended past their endpoints */
  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seq->startstill = 0;
    seq->endstill = 0;
  }
}

void BKE_sequence_single_fix(Sequence *seq)
{
  int left, start, offset;
  if (!BKE_sequence_single_check(seq)) {
    return;
  }

  /* make sure the image is always at the start since there is only one,
   * adjusting its start should be ok */
  left = BKE_sequence_tx_get_final_left(seq, false);
  start = seq->start;
  if (start != left) {
    offset = left - start;
    BKE_sequence_tx_set_final_left(seq, BKE_sequence_tx_get_final_left(seq, false) - offset);
    BKE_sequence_tx_set_final_right(seq, BKE_sequence_tx_get_final_right(seq, false) - offset);
    seq->start += offset;
  }
}

bool BKE_sequence_tx_test(Sequence *seq)
{
  return !(seq->type & SEQ_TYPE_EFFECT) || (BKE_sequence_effect_get_num_inputs(seq->type) == 0);
}

/**
 * Return \a true if given \a seq needs a complete cleanup of its cache when it is transformed.
 *
 * Some (effect) strip types need a complete re-cache of themselves when they are transformed,
 * because they do not 'contain' anything and do not have any explicit relations to other strips.
 */
bool BKE_sequence_tx_fullupdate_test(Sequence *seq)
{
  return BKE_sequence_tx_test(seq) && ELEM(seq->type, SEQ_TYPE_ADJUSTMENT, SEQ_TYPE_MULTICAM);
}

static bool seq_overlap(Sequence *seq1, Sequence *seq2)
{
  return (seq1 != seq2 && seq1->machine == seq2->machine &&
          ((seq1->enddisp <= seq2->startdisp) || (seq1->startdisp >= seq2->enddisp)) == 0);
}

bool BKE_sequence_test_overlap(ListBase *seqbasep, Sequence *test)
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

void BKE_sequence_translate(Scene *evil_scene, Sequence *seq, int delta)
{
  if (delta == 0) {
    return;
  }

  BKE_sequencer_offset_animdata(evil_scene, seq, delta);
  seq->start += delta;

  if (seq->type == SEQ_TYPE_META) {
    Sequence *seq_child;
    for (seq_child = seq->seqbase.first; seq_child; seq_child = seq_child->next) {
      BKE_sequence_translate(evil_scene, seq_child, delta);
    }
  }

  BKE_sequence_calc_disp(evil_scene, seq);
}

/* return 0 if there weren't enough space */
bool BKE_sequence_base_shuffle_ex(ListBase *seqbasep,
                                  Sequence *test,
                                  Scene *evil_scene,
                                  int channel_delta)
{
  const int orig_machine = test->machine;
  BLI_assert(ELEM(channel_delta, -1, 1));

  test->machine += channel_delta;
  BKE_sequence_calc(evil_scene, test);
  while (BKE_sequence_test_overlap(seqbasep, test)) {
    if ((channel_delta > 0) ? (test->machine >= MAXSEQ) : (test->machine < 1)) {
      break;
    }

    test->machine += channel_delta;
    BKE_sequence_calc(
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
    BKE_sequence_translate(evil_scene, test, new_frame - test->start);

    BKE_sequence_calc(evil_scene, test);
    return false;
  }

  return true;
}

bool BKE_sequence_base_shuffle(ListBase *seqbasep, Sequence *test, Scene *evil_scene)
{
  return BKE_sequence_base_shuffle_ex(seqbasep, test, evil_scene, 1);
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
      BKE_sequence_calc_disp(scene, seq); /* corrects dummy startdisp/enddisp values */
    }
  }

  return tot_ofs;
}

bool BKE_sequence_base_shuffle_time(ListBase *seqbasep,
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
        BKE_sequence_translate(evil_scene, seq, offset);
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
