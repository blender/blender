/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"

#include "BKE_fcurve.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "DNA_anim_types.h"
#include "DNA_sound_types.h"

#include "IMB_imbuf.h"

#include "RNA_prototypes.h"

#include "SEQ_channels.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_retiming.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"

#include "sequencer.h"
#include "strip_time.h"
#include "utils.h"

float seq_time_media_playback_rate_factor_get(const Scene *scene, const Sequence *seq)
{
  if ((seq->flag & SEQ_AUTO_PLAYBACK_RATE) == 0) {
    return 1.0f;
  }
  if (seq->media_playback_rate == 0.0f) {
    return 1.0f;
  }

  float scene_playback_rate = float(scene->r.frs_sec) / scene->r.frs_sec_base;
  return seq->media_playback_rate / scene_playback_rate;
}

int seq_time_strip_original_content_length_get(const Scene *scene, const Sequence *seq)
{
  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    return seq->len;
  }

  return seq->len / seq_time_media_playback_rate_factor_get(scene, seq);
}

float SEQ_give_frame_index(const Scene *scene, Sequence *seq, float timeline_frame)
{
  float frame_index;
  float sta = SEQ_time_start_frame_get(seq);
  float end = SEQ_time_content_end_frame_get(scene, seq) - 1;
  const float length = seq->len;

  if (seq->type & SEQ_TYPE_EFFECT) {
    end = SEQ_time_right_handle_frame_get(scene, seq);
  }

  if (end < sta) {
    return -1;
  }

  if (seq->type == SEQ_TYPE_IMAGE && SEQ_transform_single_image_check(seq)) {
    return 0;
  }

  if (seq->flag & SEQ_REVERSE_FRAMES) {
    frame_index = end - timeline_frame;
  }
  else {
    frame_index = timeline_frame - sta;
  }

  frame_index = max_ff(frame_index, 0);

  frame_index *= seq_time_media_playback_rate_factor_get(scene, seq);

  if (SEQ_retiming_is_active(seq)) {
    const float retiming_factor = seq_retiming_evaluate(seq, frame_index);
    frame_index = retiming_factor * (length);
  }
  /* Clamp frame index to strip content frame range. */
  frame_index = clamp_f(frame_index, 0, length);

  if (seq->strobe < 1.0f) {
    seq->strobe = 1.0f;
  }

  if (seq->strobe > 1.0f) {
    frame_index -= fmodf(double(frame_index), double(seq->strobe));
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

static void seq_update_sound_bounds_recursive_impl(const Scene *scene,
                                                   Sequence *metaseq,
                                                   int start,
                                                   int end)
{
  /* For sound we go over full meta tree to update bounds of the sound strips,
   * since sound is played outside of evaluating the image-buffers (#ImBuf). */
  LISTBASE_FOREACH (Sequence *, seq, &metaseq->seqbase) {
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

        double offset_time = 0.0f;
        if (seq->sound != nullptr) {
          offset_time = seq->sound->offset_time;
        }

        BKE_sound_move_scene_sound(scene,
                                   seq->scene_sound,
                                   seq->start + startofs,
                                   seq->start + seq->len - endofs,
                                   startofs + seq->anim_startofs,
                                   offset_time);
      }
    }
  }
}

void seq_update_sound_bounds_recursive(const Scene *scene, Sequence *metaseq)
{
  seq_update_sound_bounds_recursive_impl(
      scene, metaseq, metaseq_start(metaseq), metaseq_end(metaseq));
}

void SEQ_time_update_meta_strip_range(const Scene *scene, Sequence *seq_meta)
{
  if (seq_meta == nullptr) {
    return;
  }

  if (BLI_listbase_is_empty(&seq_meta->seqbase)) {
    return;
  }

  const int strip_start = SEQ_time_left_handle_frame_get(scene, seq_meta);
  const int strip_end = SEQ_time_right_handle_frame_get(scene, seq_meta);

  int min = MAXFRAME * 2;
  int max = -MAXFRAME * 2;
  LISTBASE_FOREACH (Sequence *, seq, &seq_meta->seqbase) {
    min = min_ii(SEQ_time_left_handle_frame_get(scene, seq), min);
    max = max_ii(SEQ_time_right_handle_frame_get(scene, seq), max);
  }

  seq_meta->start = min + seq_meta->anim_startofs;
  seq_meta->len = max - min;
  seq_meta->len -= seq_meta->anim_startofs;
  seq_meta->len -= seq_meta->anim_endofs;

  /* Functions `SEQ_time_*_handle_frame_set()` can not be used here, because they are clamped, so
   * change must be done at once. */
  seq_meta->startofs = strip_start - seq_meta->start;
  seq_meta->startdisp = strip_start; /* Only to make files usable in older versions. */
  seq_meta->endofs = seq_meta->start + SEQ_time_strip_length_get(scene, seq_meta) - strip_end;
  seq_meta->enddisp = strip_end; /* Only to make files usable in older versions. */

  seq_update_sound_bounds_recursive(scene, seq_meta);
  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq_meta));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq_meta));
}

void seq_time_effect_range_set(const Scene *scene, Sequence *seq)
{
  if (seq->seq1 == nullptr && seq->seq2 == nullptr) {
    return;
  }

  if (seq->seq1 && seq->seq2) { /* 2 - input effect. */
    seq->startdisp = max_ii(SEQ_time_left_handle_frame_get(scene, seq->seq1),
                            SEQ_time_left_handle_frame_get(scene, seq->seq2));
    seq->enddisp = min_ii(SEQ_time_right_handle_frame_get(scene, seq->seq1),
                          SEQ_time_right_handle_frame_get(scene, seq->seq2));
  }
  else if (seq->seq1) { /* Single input effect. */
    seq->startdisp = SEQ_time_right_handle_frame_get(scene, seq->seq1);
    seq->enddisp = SEQ_time_left_handle_frame_get(scene, seq->seq1);
  }
  else if (seq->seq2) { /* Strip may be missing one of inputs. */
    seq->startdisp = SEQ_time_right_handle_frame_get(scene, seq->seq2);
    seq->enddisp = SEQ_time_left_handle_frame_get(scene, seq->seq2);
  }

  if (seq->startdisp > seq->enddisp) {
    SWAP(int, seq->startdisp, seq->enddisp);
  }

  /* Values unusable for effects, these should be always 0. */
  seq->startofs = seq->endofs = seq->anim_startofs = seq->anim_endofs = 0;
  seq->start = seq->startdisp;
  seq->len = seq->enddisp - seq->startdisp;
}

void seq_time_update_effects_strip_range(const Scene *scene, SeqCollection *effects)
{
  if (effects == nullptr) {
    return;
  }

  Sequence *seq;
  /* First pass: Update length of immediate effects. */
  SEQ_ITERATOR_FOREACH (seq, effects) {
    seq_time_effect_range_set(scene, seq);
  }

  /* Second pass: Recursive call to update effects in chain and in order, so they inherit length
   * correctly. */
  SEQ_ITERATOR_FOREACH (seq, effects) {
    seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
  }
}

int SEQ_time_find_next_prev_edit(Scene *scene,
                                 int timeline_frame,
                                 const short side,
                                 const bool do_skip_mute,
                                 const bool do_center,
                                 const bool do_unselected)
{
  Editing *ed = SEQ_editing_get(scene);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  int dist, best_dist, best_frame = timeline_frame;
  int seq_frames[2], seq_frames_tot;

  /* In case where both is passed,
   * frame just finds the nearest end while frame_left the nearest start. */

  best_dist = MAXFRAME * 2;

  if (ed == nullptr) {
    return timeline_frame;
  }

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    int i;

    if (do_skip_mute && SEQ_render_is_muted(channels, seq)) {
      continue;
    }

    if (do_unselected && (seq->flag & SELECT)) {
      continue;
    }

    if (do_center) {
      seq_frames[0] = (SEQ_time_left_handle_frame_get(scene, seq) +
                       SEQ_time_right_handle_frame_get(scene, seq)) /
                      2;
      seq_frames_tot = 1;
    }
    else {
      seq_frames[0] = SEQ_time_left_handle_frame_get(scene, seq);
      seq_frames[1] = SEQ_time_right_handle_frame_get(scene, seq);

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

float SEQ_time_sequence_get_fps(Scene *scene, Sequence *seq)
{
  switch (seq->type) {
    case SEQ_TYPE_MOVIE: {
      seq_open_anim_file(scene, seq, true);
      if (BLI_listbase_is_empty(&seq->anims)) {
        return 0.0f;
      }
      StripAnim *strip_anim = static_cast<StripAnim *>(seq->anims.first);
      if (strip_anim->anim == nullptr) {
        return 0.0f;
      }
      short frs_sec;
      float frs_sec_base;
      if (IMB_anim_get_fps(strip_anim->anim, &frs_sec, &frs_sec_base, true)) {
        return float(frs_sec) / frs_sec_base;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip != nullptr) {
        return BKE_movieclip_get_fps(seq->clip);
      }
      break;
    case SEQ_TYPE_SCENE:
      if (seq->scene != nullptr) {
        return float(seq->scene->r.frs_sec) / seq->scene->r.frs_sec_base;
      }
      break;
  }
  return 0.0f;
}

void SEQ_timeline_init_boundbox(const Scene *scene, rctf *rect)
{
  rect->xmin = scene->r.sfra;
  rect->xmax = scene->r.efra + 1;
  rect->ymin = 0.0f;
  rect->ymax = 8.0f;
}

void SEQ_timeline_expand_boundbox(const Scene *scene, const ListBase *seqbase, rctf *rect)
{
  if (seqbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (rect->xmin > SEQ_time_left_handle_frame_get(scene, seq) - 1) {
      rect->xmin = SEQ_time_left_handle_frame_get(scene, seq) - 1;
    }
    if (rect->xmax < SEQ_time_right_handle_frame_get(scene, seq) + 1) {
      rect->xmax = SEQ_time_right_handle_frame_get(scene, seq) + 1;
    }
    if (rect->ymax < seq->machine) {
      rect->ymax = seq->machine;
    }
  }
}

void SEQ_timeline_boundbox(const Scene *scene, const ListBase *seqbase, rctf *rect)
{
  SEQ_timeline_init_boundbox(scene, rect);
  SEQ_timeline_expand_boundbox(scene, seqbase, rect);
}

static bool strip_exists_at_frame(const Scene *scene,
                                  SeqCollection *all_strips,
                                  const int timeline_frame)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, all_strips) {
    if (SEQ_time_strip_intersects_frame(scene, seq, timeline_frame)) {
      return true;
    }
  }
  return false;
}

void seq_time_gap_info_get(const Scene *scene,
                           ListBase *seqbase,
                           const int initial_frame,
                           GapInfo *r_gap_info)
{
  rctf rectf;
  /* Get first and last frame. */
  SEQ_timeline_boundbox(scene, seqbase, &rectf);
  const int sfra = int(rectf.xmin);
  const int efra = int(rectf.xmax);
  int timeline_frame = initial_frame;
  r_gap_info->gap_exists = false;

  SeqCollection *collection = SEQ_query_all_strips(seqbase);

  if (!strip_exists_at_frame(scene, collection, initial_frame)) {
    /* Search backward for gap_start_frame. */
    for (; timeline_frame >= sfra; timeline_frame--) {
      if (strip_exists_at_frame(scene, collection, timeline_frame)) {
        break;
      }
    }
    r_gap_info->gap_start_frame = timeline_frame + 1;
    timeline_frame = initial_frame;
  }
  else {
    /* Search forward for gap_start_frame. */
    for (; timeline_frame <= efra; timeline_frame++) {
      if (!strip_exists_at_frame(scene, collection, timeline_frame)) {
        r_gap_info->gap_start_frame = timeline_frame;
        break;
      }
    }
  }
  /* Search forward for gap_end_frame. */
  for (; timeline_frame <= efra; timeline_frame++) {
    if (strip_exists_at_frame(scene, collection, timeline_frame)) {
      const int gap_end_frame = timeline_frame;
      r_gap_info->gap_length = gap_end_frame - r_gap_info->gap_start_frame;
      r_gap_info->gap_exists = true;
      break;
    }
  }
}

bool SEQ_time_strip_intersects_frame(const Scene *scene,
                                     const Sequence *seq,
                                     const int timeline_frame)
{
  return (SEQ_time_left_handle_frame_get(scene, seq) <= timeline_frame) &&
         (SEQ_time_right_handle_frame_get(scene, seq) > timeline_frame);
}

void SEQ_time_speed_factor_set(const Scene *scene, Sequence *seq, const float speed_factor)
{

  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seq->speed_factor = speed_factor;
  }
  else {
    const float left_handle_frame = SEQ_time_left_handle_frame_get(scene, seq);
    const float unity_start_offset = seq->startofs * seq->speed_factor;
    const float unity_end_offset = seq->endofs * seq->speed_factor;
    /* Left handle is pivot point for content scaling - it must always show same frame. */
    seq->speed_factor = speed_factor;
    seq->startofs = unity_start_offset / speed_factor;
    seq->start = left_handle_frame - seq->startofs;
    seq->endofs = unity_end_offset / speed_factor;
  }

  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}

bool SEQ_time_has_left_still_frames(const Scene *scene, const Sequence *seq)
{
  return SEQ_time_left_handle_frame_get(scene, seq) < SEQ_time_start_frame_get(seq);
}

bool SEQ_time_has_right_still_frames(const Scene *scene, const Sequence *seq)
{
  return SEQ_time_right_handle_frame_get(scene, seq) > SEQ_time_content_end_frame_get(scene, seq);
}

bool SEQ_time_has_still_frames(const Scene *scene, const Sequence *seq)
{
  return SEQ_time_has_right_still_frames(scene, seq) || SEQ_time_has_left_still_frames(scene, seq);
}

int SEQ_time_strip_length_get(const Scene *scene, const Sequence *seq)
{
  if (SEQ_retiming_is_active(seq)) {
    SeqRetimingHandle *handle_start = seq->retiming_handles;
    SeqRetimingHandle *handle_end = seq->retiming_handles + (SEQ_retiming_handles_count(seq) - 1);
    return handle_end->strip_frame_index / seq_time_media_playback_rate_factor_get(scene, seq) -
           (handle_start->strip_frame_index) / seq_time_media_playback_rate_factor_get(scene, seq);
  }

  return seq->len / seq_time_media_playback_rate_factor_get(scene, seq);
}

float SEQ_time_start_frame_get(const Sequence *seq)
{
  return seq->start;
}

void SEQ_time_start_frame_set(const Scene *scene, Sequence *seq, int timeline_frame)
{
  seq->start = timeline_frame;
  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}

float SEQ_time_content_end_frame_get(const Scene *scene, const Sequence *seq)
{
  return SEQ_time_start_frame_get(seq) + SEQ_time_strip_length_get(scene, seq);
}

int SEQ_time_left_handle_frame_get(const Scene * /*scene*/, const Sequence *seq)
{
  if (seq->seq1 || seq->seq2) {
    return seq->startdisp;
  }

  return seq->start + seq->startofs;
}

int SEQ_time_right_handle_frame_get(const Scene *scene, const Sequence *seq)
{
  if (seq->seq1 || seq->seq2) {
    return seq->enddisp;
  }

  return SEQ_time_content_end_frame_get(scene, seq) - seq->endofs;
}

void SEQ_time_left_handle_frame_set(const Scene *scene, Sequence *seq, int timeline_frame)
{
  const float right_handle_orig_frame = SEQ_time_right_handle_frame_get(scene, seq);

  if (timeline_frame >= right_handle_orig_frame) {
    timeline_frame = right_handle_orig_frame - 1;
  }

  float offset = timeline_frame - SEQ_time_start_frame_get(seq);

  if (SEQ_transform_single_image_check(seq)) {
    /* This strip has only 1 frame of content, that is always stretched to whole strip length.
     * Therefore, strip start should be moved instead of adjusting offset. */
    SEQ_time_start_frame_set(scene, seq, timeline_frame);
    seq->endofs += offset;
  }
  else {
    seq->startofs = offset;
  }

  seq->startdisp = timeline_frame; /* Only to make files usable in older versions. */

  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}

void SEQ_time_right_handle_frame_set(const Scene *scene, Sequence *seq, int timeline_frame)
{
  const float left_handle_orig_frame = SEQ_time_left_handle_frame_get(scene, seq);

  if (timeline_frame <= left_handle_orig_frame) {
    timeline_frame = left_handle_orig_frame + 1;
  }

  seq->endofs = SEQ_time_content_end_frame_get(scene, seq) - timeline_frame;
  seq->enddisp = timeline_frame; /* Only to make files usable in older versions. */

  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}

void seq_time_translate_handles(const Scene *scene, Sequence *seq, const int offset)
{
  seq->startofs += offset;
  seq->endofs -= offset;
  seq->startdisp += offset; /* Only to make files usable in older versions. */
  seq->enddisp -= offset;   /* Only to make files usable in older versions. */

  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
  seq_time_update_effects_strip_range(scene, seq_sequence_lookup_effects_by_seq(scene, seq));
}
