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

#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "IMB_imbuf.h"

#include "SEQ_sequencer.h"

#include "strip_time.h"
#include "utils.h"

float seq_give_frame_index(Sequence *seq, float timeline_frame)
{
  float frame_index;
  int sta = seq->start;
  int end = seq->start + seq->len - 1;

  if (seq->type & SEQ_TYPE_EFFECT) {
    end = seq->enddisp;
  }

  if (end < sta) {
    return -1;
  }

  if (seq->flag & SEQ_REVERSE_FRAMES) {
    /*reverse frame in this sequence */
    if (timeline_frame <= sta) {
      frame_index = end - sta;
    }
    else if (timeline_frame >= end) {
      frame_index = 0;
    }
    else {
      frame_index = end - timeline_frame;
    }
  }
  else {
    if (timeline_frame <= sta) {
      frame_index = 0;
    }
    else if (timeline_frame >= end) {
      frame_index = end - sta;
    }
    else {
      frame_index = timeline_frame - sta;
    }
  }

  if (seq->strobe < 1.0f) {
    seq->strobe = 1.0f;
  }

  if (seq->strobe > 1.0f) {
    frame_index -= fmodf((double)frame_index, (double)seq->strobe);
  }

  return frame_index;
}

static int metaseq_start(Sequence *metaseq)
{
  return metaseq->start + metaseq->startofs;
}

static int metaseq_end(Sequence *metaseq)
{
  return metaseq->start + metaseq->len - metaseq->endofs;
}

static void seq_update_sound_bounds_recursive_impl(Scene *scene,
                                                   Sequence *metaseq,
                                                   int start,
                                                   int end)
{
  Sequence *seq;

  /* for sound we go over full meta tree to update bounds of the sound strips,
   * since sound is played outside of evaluating the imbufs, */
  for (seq = metaseq->seqbase.first; seq; seq = seq->next) {
    if (seq->type == SEQ_TYPE_META) {
      seq_update_sound_bounds_recursive_impl(
          scene, seq, max_ii(start, metaseq_start(seq)), min_ii(end, metaseq_end(seq)));
    }
    else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      if (seq->scene_sound) {
        int startofs = seq->startofs;
        int endofs = seq->endofs;
        if (seq->startofs + seq->start < start) {
          startofs = start - seq->start;
        }

        if (seq->start + seq->len - seq->endofs > end) {
          endofs = seq->start + seq->len - end;
        }

        BKE_sound_move_scene_sound(scene,
                                   seq->scene_sound,
                                   seq->start + startofs,
                                   seq->start + seq->len - endofs,
                                   startofs + seq->anim_startofs);
      }
    }
  }
}

void seq_update_sound_bounds_recursive(Scene *scene, Sequence *metaseq)
{
  seq_update_sound_bounds_recursive_impl(
      scene, metaseq, metaseq_start(metaseq), metaseq_end(metaseq));
}

void BKE_sequence_calc_disp(Scene *scene, Sequence *seq)
{
  if (seq->startofs && seq->startstill) {
    seq->startstill = 0;
  }
  if (seq->endofs && seq->endstill) {
    seq->endstill = 0;
  }

  seq->startdisp = seq->start + seq->startofs - seq->startstill;
  seq->enddisp = seq->start + seq->len - seq->endofs + seq->endstill;

  if (seq->type == SEQ_TYPE_META) {
    seq_update_sound_bounds_recursive(scene, seq);
  }
}

void BKE_sequence_calc(Scene *scene, Sequence *seq)
{
  Sequence *seqm;
  int min, max;

  /* check all metas recursively */
  seqm = seq->seqbase.first;
  while (seqm) {
    if (seqm->seqbase.first) {
      BKE_sequence_calc(scene, seqm);
    }
    seqm = seqm->next;
  }

  /* effects and meta: automatic start and end */
  if (seq->type & SEQ_TYPE_EFFECT) {
    if (seq->seq1) {
      seq->startofs = seq->endofs = seq->startstill = seq->endstill = 0;
      if (seq->seq3) {
        seq->start = seq->startdisp = max_iii(
            seq->seq1->startdisp, seq->seq2->startdisp, seq->seq3->startdisp);
        seq->enddisp = min_iii(seq->seq1->enddisp, seq->seq2->enddisp, seq->seq3->enddisp);
      }
      else if (seq->seq2) {
        seq->start = seq->startdisp = max_ii(seq->seq1->startdisp, seq->seq2->startdisp);
        seq->enddisp = min_ii(seq->seq1->enddisp, seq->seq2->enddisp);
      }
      else {
        seq->start = seq->startdisp = seq->seq1->startdisp;
        seq->enddisp = seq->seq1->enddisp;
      }
      /* we cant help if strips don't overlap, it wont give useful results.
       * but at least ensure 'len' is never negative which causes bad bugs elsewhere. */
      if (seq->enddisp < seq->startdisp) {
        /* simple start/end swap */
        seq->start = seq->enddisp;
        seq->enddisp = seq->startdisp;
        seq->startdisp = seq->start;
        seq->flag |= SEQ_INVALID_EFFECT;
      }
      else {
        seq->flag &= ~SEQ_INVALID_EFFECT;
      }

      seq->len = seq->enddisp - seq->startdisp;
    }
    else {
      BKE_sequence_calc_disp(scene, seq);
    }
  }
  else {
    if (seq->type == SEQ_TYPE_META) {
      seqm = seq->seqbase.first;
      if (seqm) {
        min = MAXFRAME * 2;
        max = -MAXFRAME * 2;
        while (seqm) {
          if (seqm->startdisp < min) {
            min = seqm->startdisp;
          }
          if (seqm->enddisp > max) {
            max = seqm->enddisp;
          }
          seqm = seqm->next;
        }
        seq->start = min + seq->anim_startofs;
        seq->len = max - min;
        seq->len -= seq->anim_startofs;
        seq->len -= seq->anim_endofs;
      }
      seq_update_sound_bounds_recursive(scene, seq);
    }
    BKE_sequence_calc_disp(scene, seq);
  }
}

/** Comparison function suitable to be used with BLI_listbase_sort()... */
int BKE_sequencer_cmp_time_startdisp(const void *a, const void *b)
{
  const Sequence *seq_a = a;
  const Sequence *seq_b = b;

  return (seq_a->startdisp > seq_b->startdisp);
}

int BKE_sequencer_find_next_prev_edit(Scene *scene,
                                      int timeline_frame,
                                      const short side,
                                      const bool do_skip_mute,
                                      const bool do_center,
                                      const bool do_unselected)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;

  int dist, best_dist, best_frame = timeline_frame;
  int seq_frames[2], seq_frames_tot;

  /* In case where both is passed,
   * frame just finds the nearest end while frame_left the nearest start. */

  best_dist = MAXFRAME * 2;

  if (ed == NULL) {
    return timeline_frame;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    int i;

    if (do_skip_mute && (seq->flag & SEQ_MUTE)) {
      continue;
    }

    if (do_unselected && (seq->flag & SELECT)) {
      continue;
    }

    if (do_center) {
      seq_frames[0] = (seq->startdisp + seq->enddisp) / 2;
      seq_frames_tot = 1;
    }
    else {
      seq_frames[0] = seq->startdisp;
      seq_frames[1] = seq->enddisp;

      seq_frames_tot = 2;
    }

    for (i = 0; i < seq_frames_tot; i++) {
      const int seq_frame = seq_frames[i];

      dist = MAXFRAME * 2;

      switch (side) {
        case SEQ_SIDE_LEFT:
          if (seq_frame < timeline_frame) {
            dist = timeline_frame - seq_frame;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (seq_frame > timeline_frame) {
            dist = seq_frame - timeline_frame;
          }
          break;
        case SEQ_SIDE_BOTH:
          dist = abs(seq_frame - timeline_frame);
          break;
      }

      if (dist < best_dist) {
        best_frame = seq_frame;
        best_dist = dist;
      }
    }
  }

  return best_frame;
}

float BKE_sequence_get_fps(Scene *scene, Sequence *seq)
{
  switch (seq->type) {
    case SEQ_TYPE_MOVIE: {
      seq_open_anim_file(scene, seq, true);
      if (BLI_listbase_is_empty(&seq->anims)) {
        return 0.0f;
      }
      StripAnim *strip_anim = seq->anims.first;
      if (strip_anim->anim == NULL) {
        return 0.0f;
      }
      short frs_sec;
      float frs_sec_base;
      if (IMB_anim_get_fps(strip_anim->anim, &frs_sec, &frs_sec_base, true)) {
        return (float)frs_sec / frs_sec_base;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip != NULL) {
        return BKE_movieclip_get_fps(seq->clip);
      }
      break;
    case SEQ_TYPE_SCENE:
      if (seq->scene != NULL) {
        return (float)seq->scene->r.frs_sec / seq->scene->r.frs_sec_base;
      }
      break;
  }
  return 0.0f;
}
