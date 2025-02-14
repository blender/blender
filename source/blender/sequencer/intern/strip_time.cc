/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"

#include "BKE_movieclip.h"
#include "BKE_sound.h"

#include "DNA_sound_types.h"

#include "MOV_read.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_render.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "sequencer.hh"
#include "strip_time.hh"
#include "utils.hh"

float SEQ_time_media_playback_rate_factor_get(const Scene *scene, const Strip *strip)
{
  if ((strip->flag & SEQ_AUTO_PLAYBACK_RATE) == 0) {
    return 1.0f;
  }
  if (strip->media_playback_rate == 0.0f) {
    return 1.0f;
  }

  float scene_playback_rate = float(scene->r.frs_sec) / scene->r.frs_sec_base;
  return strip->media_playback_rate / scene_playback_rate;
}

int seq_time_strip_original_content_length_get(const Scene *scene, const Strip *strip)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    return strip->len;
  }

  return strip->len / SEQ_time_media_playback_rate_factor_get(scene, strip);
}

float SEQ_give_frame_index(const Scene *scene, const Strip *strip, float timeline_frame)
{
  float frame_index;
  float sta = SEQ_time_start_frame_get(strip);
  float end = SEQ_time_content_end_frame_get(scene, strip) - 1;
  float frame_index_max = strip->len - 1;

  if (strip->type & STRIP_TYPE_EFFECT) {
    end = SEQ_time_right_handle_frame_get(scene, strip);
    frame_index_max = end - sta;
  }

  if (end < sta) {
    return -1;
  }

  if (strip->type == STRIP_TYPE_IMAGE && SEQ_transform_single_image_check(strip)) {
    return 0;
  }

  if (strip->flag & SEQ_REVERSE_FRAMES) {
    frame_index = end - timeline_frame;
  }
  else {
    frame_index = timeline_frame - sta;
  }

  frame_index = max_ff(frame_index, 0);

  frame_index *= SEQ_time_media_playback_rate_factor_get(scene, strip);

  if (SEQ_retiming_is_active(strip)) {
    const float retiming_factor = strip_retiming_evaluate(strip, frame_index);
    frame_index = retiming_factor * frame_index_max;
  }
  /* Clamp frame index to strip content frame range. */
  frame_index = clamp_f(frame_index, 0, frame_index_max);

  if (strip->strobe > 1.0f) {
    frame_index -= fmodf(double(frame_index), double(strip->strobe));
  }

  return frame_index;
}

static int metaseq_start(Strip *metaseq)
{
  return metaseq->start + metaseq->startofs;
}

static int metaseq_end(Strip *metaseq)
{
  return metaseq->start + metaseq->len - metaseq->endofs;
}

static void strip_update_sound_bounds_recursive_impl(const Scene *scene,
                                                     Strip *metaseq,
                                                     int start,
                                                     int end)
{
  /* For sound we go over full meta tree to update bounds of the sound strips,
   * since sound is played outside of evaluating the image-buffers (#ImBuf). */
  LISTBASE_FOREACH (Strip *, strip, &metaseq->seqbase) {
    if (strip->type == STRIP_TYPE_META) {
      strip_update_sound_bounds_recursive_impl(
          scene, strip, max_ii(start, metaseq_start(strip)), min_ii(end, metaseq_end(strip)));
    }
    else if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SCENE)) {
      if (strip->scene_sound) {
        int startofs = strip->startofs;
        int endofs = strip->endofs;
        if (strip->startofs + strip->start < start) {
          startofs = start - strip->start;
        }

        if (strip->start + strip->len - strip->endofs > end) {
          endofs = strip->start + strip->len - end;
        }

        double offset_time = 0.0f;
        if (strip->sound != nullptr) {
          offset_time = strip->sound->offset_time + strip->sound_offset;
        }

        BKE_sound_move_scene_sound(scene,
                                   strip->scene_sound,
                                   strip->start + startofs,
                                   strip->start + strip->len - endofs,
                                   startofs + strip->anim_startofs,
                                   offset_time);
      }
    }
  }
}

void strip_update_sound_bounds_recursive(const Scene *scene, Strip *metaseq)
{
  strip_update_sound_bounds_recursive_impl(
      scene, metaseq, metaseq_start(metaseq), metaseq_end(metaseq));
}

void SEQ_time_update_meta_strip_range(const Scene *scene, Strip *strip_meta)
{
  if (strip_meta == nullptr) {
    return;
  }

  if (BLI_listbase_is_empty(&strip_meta->seqbase)) {
    return;
  }

  const int strip_start = SEQ_time_left_handle_frame_get(scene, strip_meta);
  const int strip_end = SEQ_time_right_handle_frame_get(scene, strip_meta);

  int min = MAXFRAME * 2;
  int max = -MAXFRAME * 2;
  LISTBASE_FOREACH (Strip *, strip, &strip_meta->seqbase) {
    min = min_ii(SEQ_time_left_handle_frame_get(scene, strip), min);
    max = max_ii(SEQ_time_right_handle_frame_get(scene, strip), max);
  }

  strip_meta->start = min + strip_meta->anim_startofs;
  strip_meta->len = max - min;
  strip_meta->len -= strip_meta->anim_startofs;
  strip_meta->len -= strip_meta->anim_endofs;

  /* Functions `SEQ_time_*_handle_frame_set()` can not be used here, because they are clamped, so
   * change must be done at once. */
  strip_meta->startofs = strip_start - strip_meta->start;
  strip_meta->startdisp = strip_start; /* Only to make files usable in older versions. */
  strip_meta->endofs = strip_meta->start + SEQ_time_strip_length_get(scene, strip_meta) -
                       strip_end;
  strip_meta->enddisp = strip_end; /* Only to make files usable in older versions. */

  strip_update_sound_bounds_recursive(scene, strip_meta);
  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip_meta);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip_meta));
}

void strip_time_effect_range_set(const Scene *scene, Strip *strip)
{
  if (strip->seq1 == nullptr && strip->seq2 == nullptr) {
    return;
  }

  if (strip->seq1 && strip->seq2) { /* 2 - input effect. */
    strip->startdisp = max_ii(SEQ_time_left_handle_frame_get(scene, strip->seq1),
                              SEQ_time_left_handle_frame_get(scene, strip->seq2));
    strip->enddisp = min_ii(SEQ_time_right_handle_frame_get(scene, strip->seq1),
                            SEQ_time_right_handle_frame_get(scene, strip->seq2));
  }
  else if (strip->seq1) { /* Single input effect. */
    strip->startdisp = SEQ_time_right_handle_frame_get(scene, strip->seq1);
    strip->enddisp = SEQ_time_left_handle_frame_get(scene, strip->seq1);
  }
  else if (strip->seq2) { /* Strip may be missing one of inputs. */
    strip->startdisp = SEQ_time_right_handle_frame_get(scene, strip->seq2);
    strip->enddisp = SEQ_time_left_handle_frame_get(scene, strip->seq2);
  }

  if (strip->startdisp > strip->enddisp) {
    std::swap(strip->startdisp, strip->enddisp);
  }

  /* Values unusable for effects, these should be always 0. */
  strip->startofs = strip->endofs = strip->anim_startofs = strip->anim_endofs = 0;
  strip->start = strip->startdisp;
  strip->len = strip->enddisp - strip->startdisp;
}

void strip_time_update_effects_strip_range(const Scene *scene,
                                           const blender::Span<Strip *> effects)
{
  /* First pass: Update length of immediate effects. */
  for (Strip *strip : effects) {
    strip_time_effect_range_set(scene, strip);
  }

  /* Second pass: Recursive call to update effects in chain and in order, so they inherit length
   * correctly. */
  for (Strip *strip : effects) {
    blender::Span<Strip *> effects_recurse = SEQ_lookup_effects_by_strip(scene, strip);
    strip_time_update_effects_strip_range(scene, effects_recurse);
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
  int strip_frames[2], strip_frames_tot;

  /* In case where both is passed,
   * frame just finds the nearest end while frame_left the nearest start. */

  best_dist = MAXFRAME * 2;

  if (ed == nullptr) {
    return timeline_frame;
  }

  LISTBASE_FOREACH (Strip *, strip, ed->seqbasep) {
    int i;

    if (do_skip_mute && SEQ_render_is_muted(channels, strip)) {
      continue;
    }

    if (do_unselected && (strip->flag & SELECT)) {
      continue;
    }

    if (do_center) {
      strip_frames[0] = (SEQ_time_left_handle_frame_get(scene, strip) +
                         SEQ_time_right_handle_frame_get(scene, strip)) /
                        2;
      strip_frames_tot = 1;
    }
    else {
      strip_frames[0] = SEQ_time_left_handle_frame_get(scene, strip);
      strip_frames[1] = SEQ_time_right_handle_frame_get(scene, strip);

      strip_frames_tot = 2;
    }

    for (i = 0; i < strip_frames_tot; i++) {
      const int strip_frame = strip_frames[i];

      dist = MAXFRAME * 2;

      switch (side) {
        case SEQ_SIDE_LEFT:
          if (strip_frame < timeline_frame) {
            dist = timeline_frame - strip_frame;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (strip_frame > timeline_frame) {
            dist = strip_frame - timeline_frame;
          }
          break;
        case SEQ_SIDE_BOTH:
          dist = abs(strip_frame - timeline_frame);
          break;
      }

      if (dist < best_dist) {
        best_frame = strip_frame;
        best_dist = dist;
      }
    }
  }

  return best_frame;
}

float SEQ_time_sequence_get_fps(Scene *scene, Strip *strip)
{
  switch (strip->type) {
    case STRIP_TYPE_MOVIE: {
      strip_open_anim_file(scene, strip, true);
      if (BLI_listbase_is_empty(&strip->anims)) {
        return 0.0f;
      }
      StripAnim *strip_anim = static_cast<StripAnim *>(strip->anims.first);
      if (strip_anim->anim == nullptr) {
        return 0.0f;
      }
      return MOV_get_fps(strip_anim->anim);
    }
    case STRIP_TYPE_MOVIECLIP:
      if (strip->clip != nullptr) {
        return BKE_movieclip_get_fps(strip->clip);
      }
      break;
    case STRIP_TYPE_SCENE:
      if (strip->scene != nullptr) {
        return float(strip->scene->r.frs_sec) / strip->scene->r.frs_sec_base;
      }
      break;
  }
  return 0.0f;
}

void SEQ_timeline_init_boundbox(const Scene *scene, rctf *r_rect)
{
  r_rect->xmin = scene->r.sfra;
  r_rect->xmax = scene->r.efra + 1;
  r_rect->ymin = 1.0f; /* The first strip is drawn at y == 1.0f */
  r_rect->ymax = 8.0f;
}

void SEQ_timeline_expand_boundbox(const Scene *scene, const ListBase *seqbase, rctf *rect)
{
  if (seqbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    rect->xmin = std::min<float>(rect->xmin, SEQ_time_left_handle_frame_get(scene, strip) - 1);
    rect->xmax = std::max<float>(rect->xmax, SEQ_time_right_handle_frame_get(scene, strip) + 1);
    /* We do +1 here to account for the channel thickness. Channel n has range of <n, n+1>. */
    rect->ymax = std::max(rect->ymax, strip->machine + 1.0f);
  }
}

void SEQ_timeline_boundbox(const Scene *scene, const ListBase *seqbase, rctf *r_rect)
{
  SEQ_timeline_init_boundbox(scene, r_rect);
  SEQ_timeline_expand_boundbox(scene, seqbase, r_rect);
}

static bool strip_exists_at_frame(const Scene *scene,
                                  blender::Span<Strip *> strips,
                                  const int timeline_frame)
{
  for (Strip *strip : strips) {
    if (SEQ_time_strip_intersects_frame(scene, strip, timeline_frame)) {
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

  blender::VectorSet strips = SEQ_query_all_strips(seqbase);

  if (!strip_exists_at_frame(scene, strips, initial_frame)) {
    /* Search backward for gap_start_frame. */
    for (; timeline_frame >= sfra; timeline_frame--) {
      if (strip_exists_at_frame(scene, strips, timeline_frame)) {
        break;
      }
    }
    r_gap_info->gap_start_frame = timeline_frame + 1;
    timeline_frame = initial_frame;
  }
  else {
    /* Search forward for gap_start_frame. */
    for (; timeline_frame <= efra; timeline_frame++) {
      if (!strip_exists_at_frame(scene, strips, timeline_frame)) {
        r_gap_info->gap_start_frame = timeline_frame;
        break;
      }
    }
  }
  /* Search forward for gap_end_frame. */
  for (; timeline_frame <= efra; timeline_frame++) {
    if (strip_exists_at_frame(scene, strips, timeline_frame)) {
      const int gap_end_frame = timeline_frame;
      r_gap_info->gap_length = gap_end_frame - r_gap_info->gap_start_frame;
      r_gap_info->gap_exists = true;
      break;
    }
  }
}

bool SEQ_time_strip_intersects_frame(const Scene *scene,
                                     const Strip *strip,
                                     const int timeline_frame)
{
  return (SEQ_time_left_handle_frame_get(scene, strip) <= timeline_frame) &&
         (SEQ_time_right_handle_frame_get(scene, strip) > timeline_frame);
}

bool SEQ_time_has_left_still_frames(const Scene *scene, const Strip *strip)
{
  return SEQ_time_left_handle_frame_get(scene, strip) < SEQ_time_start_frame_get(strip);
}

bool SEQ_time_has_right_still_frames(const Scene *scene, const Strip *strip)
{
  return SEQ_time_right_handle_frame_get(scene, strip) >
         SEQ_time_content_end_frame_get(scene, strip);
}

bool SEQ_time_has_still_frames(const Scene *scene, const Strip *strip)
{
  return SEQ_time_has_right_still_frames(scene, strip) ||
         SEQ_time_has_left_still_frames(scene, strip);
}

int SEQ_time_strip_length_get(const Scene *scene, const Strip *strip)
{
  if (SEQ_retiming_is_active(strip)) {
    const int last_key_frame = SEQ_retiming_key_timeline_frame_get(
        scene, strip, SEQ_retiming_last_key_get(strip));
    /* Last key is mapped to last frame index. Numbering starts from 0. */
    int sound_offset = SEQ_time_get_rounded_sound_offset(scene, strip);
    return last_key_frame + 1 - SEQ_time_start_frame_get(strip) - sound_offset;
  }

  return strip->len / SEQ_time_media_playback_rate_factor_get(scene, strip);
}

float SEQ_time_start_frame_get(const Strip *strip)
{
  return strip->start;
}

void SEQ_time_start_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  strip->start = timeline_frame;
  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

float SEQ_time_content_end_frame_get(const Scene *scene, const Strip *strip)
{
  return SEQ_time_start_frame_get(strip) + SEQ_time_strip_length_get(scene, strip);
}

int SEQ_time_left_handle_frame_get(const Scene * /*scene*/, const Strip *strip)
{
  if (strip->seq1 || strip->seq2) {
    return strip->startdisp;
  }

  return strip->start + strip->startofs;
}

int SEQ_time_right_handle_frame_get(const Scene *scene, const Strip *strip)
{
  if (strip->seq1 || strip->seq2) {
    return strip->enddisp;
  }

  return SEQ_time_content_end_frame_get(scene, strip) - strip->endofs;
}

void SEQ_time_left_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  const float right_handle_orig_frame = SEQ_time_right_handle_frame_get(scene, strip);

  if (timeline_frame >= right_handle_orig_frame) {
    timeline_frame = right_handle_orig_frame - 1;
  }

  float offset = timeline_frame - SEQ_time_start_frame_get(strip);

  if (SEQ_transform_single_image_check(strip)) {
    /* This strip has only 1 frame of content, that is always stretched to whole strip length.
     * Therefore, strip start should be moved instead of adjusting offset. */
    SEQ_time_start_frame_set(scene, strip, timeline_frame);
    strip->endofs += offset;
  }
  else {
    strip->startofs = offset;
  }

  strip->startdisp = timeline_frame; /* Only to make files usable in older versions. */

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

void SEQ_time_right_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  const float left_handle_orig_frame = SEQ_time_left_handle_frame_get(scene, strip);

  if (timeline_frame <= left_handle_orig_frame) {
    timeline_frame = left_handle_orig_frame + 1;
  }

  strip->endofs = SEQ_time_content_end_frame_get(scene, strip) - timeline_frame;
  strip->enddisp = timeline_frame; /* Only to make files usable in older versions. */

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

void strip_time_translate_handles(const Scene *scene, Strip *strip, const int offset)
{
  strip->startofs += offset;
  strip->endofs -= offset;
  strip->startdisp += offset; /* Only to make files usable in older versions. */
  strip->enddisp -= offset;   /* Only to make files usable in older versions. */

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

static void strip_time_slip_strip_ex(
    const Scene *scene, Strip *strip, int delta, float subframe_delta, bool recursed)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM && subframe_delta != 0.0f) {
    strip->sound_offset += subframe_delta / FPS;
  }

  if (delta == 0) {
    return;
  }

  /* Skip effect strips where the length is dependent on another strip,
   * as they are calculated with #strip_time_update_effects_strip_range. */
  if (strip->seq1 != nullptr || strip->seq2 != nullptr) {
    return;
  }

  /* Effects only have a start frame and a length, so unless we're inside
   * a meta strip, there's no need to do anything. */
  if (!recursed && (strip->type & STRIP_TYPE_EFFECT)) {
    return;
  }

  /* Move strips inside meta strip. */
  if (strip->type == STRIP_TYPE_META) {
    /* If the meta strip has no contents, don't do anything. */
    if (BLI_listbase_is_empty(&strip->seqbase)) {
      return;
    }
    LISTBASE_FOREACH (Strip *, strip_child, &strip->seqbase) {
      strip_time_slip_strip_ex(scene, strip_child, delta, subframe_delta, true);
    }
  }

  strip->start = strip->start + delta;
  if (!recursed) {
    strip->startofs = strip->startofs - delta;
    strip->endofs = strip->endofs + delta;
  }

  /* Only to make files usable in older versions. */
  strip->startdisp = SEQ_time_left_handle_frame_get(scene, strip);
  strip->enddisp = SEQ_time_right_handle_frame_get(scene, strip);

  blender::Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene, strip);
  strip_time_update_effects_strip_range(scene, effects);
}

void SEQ_time_slip_strip(const Scene *scene, Strip *strip, int delta, float subframe_delta)
{
  strip_time_slip_strip_ex(scene, strip, delta, subframe_delta, false);
}

int SEQ_time_get_rounded_sound_offset(const Scene *scene, const Strip *strip)
{
  int sound_offset = 0;
  if (strip->type == STRIP_TYPE_SOUND_RAM && strip->sound != nullptr) {
    sound_offset = round_fl_to_int((strip->sound->offset_time + strip->sound_offset) * FPS);
  }
  return sound_offset;
}
