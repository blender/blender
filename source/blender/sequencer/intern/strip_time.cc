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
#include "BKE_sound.hh"

#include "DNA_sound_types.h"

#include "MOV_read.hh"

#include "SEQ_animation.hh"
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

namespace blender::seq {

float time_media_playback_rate_factor_get(const Strip *strip, const float scene_fps)
{
  if ((strip->flag & SEQ_AUTO_PLAYBACK_RATE) == 0) {
    return 1.0f;
  }
  if (strip->media_playback_rate == 0.0f) {
    return 1.0f;
  }
  return strip->media_playback_rate / scene_fps;
}

float give_frame_index(const Scene *scene, const Strip *strip, float timeline_frame)
{
  float frame_index;
  float sta = time_start_frame_get(strip);
  float end = time_content_end_frame_get(scene, strip) - 1;
  float frame_index_max = strip->len - 1;

  if (strip->is_effect()) {
    end = time_right_handle_frame_get(scene, strip);
    frame_index_max = end - sta;
  }

  if (end < sta) {
    return -1;
  }

  if (strip->type == STRIP_TYPE_IMAGE && transform_single_image_check(strip)) {
    return 0;
  }

  if (strip->flag & SEQ_REVERSE_FRAMES) {
    frame_index = end - timeline_frame;
  }
  else {
    frame_index = timeline_frame - sta;
  }

  frame_index = max_ff(frame_index, 0);

  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  frame_index *= time_media_playback_rate_factor_get(strip, scene_fps);

  if (retiming_is_active(strip)) {
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

static int metastrip_start_get(Strip *strip_meta)
{
  return strip_meta->start + strip_meta->startofs;
}

static int metastrip_end_get(Strip *strip_meta)
{
  return strip_meta->start + strip_meta->len - strip_meta->endofs;
}

static void strip_update_sound_bounds_recursive_impl(const Scene *scene,
                                                     Strip *strip_meta,
                                                     int start,
                                                     int end)
{
  /* For sound we go over full meta tree to update bounds of the sound strips,
   * since sound is played outside of evaluating the image-buffers (#ImBuf). */
  LISTBASE_FOREACH (Strip *, strip, &strip_meta->seqbase) {
    if (strip->type == STRIP_TYPE_META) {
      strip_update_sound_bounds_recursive_impl(scene,
                                               strip,
                                               max_ii(start, metastrip_start_get(strip)),
                                               min_ii(end, metastrip_end_get(strip)));
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

void strip_update_sound_bounds_recursive(const Scene *scene, Strip *strip_meta)
{
  strip_update_sound_bounds_recursive_impl(
      scene, strip_meta, metastrip_start_get(strip_meta), metastrip_end_get(strip_meta));
}

void time_update_meta_strip_range(const Scene *scene, Strip *strip_meta)
{
  if (strip_meta == nullptr) {
    return;
  }

  if (BLI_listbase_is_empty(&strip_meta->seqbase)) {
    return;
  }

  const int strip_start = time_left_handle_frame_get(scene, strip_meta);
  const int strip_end = time_right_handle_frame_get(scene, strip_meta);

  int min = MAXFRAME * 2;
  int max = -MAXFRAME * 2;
  LISTBASE_FOREACH (Strip *, strip, &strip_meta->seqbase) {
    min = min_ii(time_left_handle_frame_get(scene, strip), min);
    max = max_ii(time_right_handle_frame_get(scene, strip), max);
  }

  strip_meta->start = min + strip_meta->anim_startofs;
  strip_meta->len = max - strip_meta->anim_endofs - strip_meta->start;

  /* Functions `SEQ_time_*_handle_frame_set()` can not be used here, because they are clamped, so
   * change must be done at once. */
  strip_meta->startofs = strip_start - strip_meta->start;
  strip_meta->startdisp = strip_start; /* Only to make files usable in older versions. */
  strip_meta->endofs = strip_meta->start + time_strip_length_get(scene, strip_meta) - strip_end;
  strip_meta->enddisp = strip_end; /* Only to make files usable in older versions. */

  strip_update_sound_bounds_recursive(scene, strip_meta);
  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip_meta);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip_meta));
}

void strip_time_effect_range_set(const Scene *scene, Strip *strip)
{
  if (strip->input1 == nullptr && strip->input2 == nullptr) {
    return;
  }

  if (strip->input1 && strip->input2) { /* 2 - input effect. */
    strip->startdisp = max_ii(time_left_handle_frame_get(scene, strip->input1),
                              time_left_handle_frame_get(scene, strip->input2));
    strip->enddisp = min_ii(time_right_handle_frame_get(scene, strip->input1),
                            time_right_handle_frame_get(scene, strip->input2));
  }
  else if (strip->input1) { /* Single input effect. */
    strip->startdisp = time_right_handle_frame_get(scene, strip->input1);
    strip->enddisp = time_left_handle_frame_get(scene, strip->input1);
  }
  else if (strip->input2) { /* Strip may be missing one of inputs. */
    strip->startdisp = time_right_handle_frame_get(scene, strip->input2);
    strip->enddisp = time_left_handle_frame_get(scene, strip->input2);
  }

  if (strip->startdisp > strip->enddisp) {
    std::swap(strip->startdisp, strip->enddisp);
  }

  /* Values unusable for effects, these should be always 0. */
  strip->startofs = strip->endofs = strip->anim_startofs = strip->anim_endofs = 0;
  strip->start = strip->startdisp;
  strip->len = strip->enddisp - strip->startdisp;
}

void strip_time_update_effects_strip_range(const Scene *scene, const Span<Strip *> effects)
{
  /* First pass: Update length of immediate effects. */
  for (Strip *strip : effects) {
    strip_time_effect_range_set(scene, strip);
  }

  /* Second pass: Recursive call to update effects in chain and in order, so they inherit length
   * correctly. */
  for (Strip *strip : effects) {
    Span<Strip *> effects_recurse = SEQ_lookup_effects_by_strip(scene->ed, strip);
    strip_time_update_effects_strip_range(scene, effects_recurse);
  }
}

int time_find_next_prev_edit(Scene *scene,
                             int timeline_frame,
                             const short side,
                             const bool do_skip_mute,
                             const bool do_center,
                             const bool do_unselected)
{
  Editing *ed = editing_get(scene);
  ListBase *channels = channels_displayed_get(ed);

  int dist, best_dist, best_frame = timeline_frame;
  int strip_frames[2], strip_frames_tot;

  /* In case where both is passed,
   * frame just finds the nearest end while frame_left the nearest start. */

  best_dist = MAXFRAME * 2;

  if (ed == nullptr) {
    return timeline_frame;
  }

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    int i;

    if (do_skip_mute && render_is_muted(channels, strip)) {
      continue;
    }

    if (do_unselected && (strip->flag & SELECT)) {
      continue;
    }

    if (do_center) {
      strip_frames[0] = (time_left_handle_frame_get(scene, strip) +
                         time_right_handle_frame_get(scene, strip)) /
                        2;
      strip_frames_tot = 1;
    }
    else {
      strip_frames[0] = time_left_handle_frame_get(scene, strip);
      strip_frames[1] = time_right_handle_frame_get(scene, strip);

      strip_frames_tot = 2;
    }

    for (i = 0; i < strip_frames_tot; i++) {
      const int strip_frame = strip_frames[i];

      dist = MAXFRAME * 2;

      switch (side) {
        case SIDE_LEFT:
          if (strip_frame < timeline_frame) {
            dist = timeline_frame - strip_frame;
          }
          break;
        case SIDE_RIGHT:
          if (strip_frame > timeline_frame) {
            dist = strip_frame - timeline_frame;
          }
          break;
        case SIDE_BOTH:
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

float time_strip_fps_get(Scene *scene, Strip *strip)
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

void timeline_init_boundbox(const Scene *scene, rctf *r_rect)
{
  r_rect->xmin = scene->r.sfra;
  r_rect->xmax = scene->r.efra + 1;
  r_rect->ymin = 1.0f; /* The first strip is drawn at y == 1.0f */
  r_rect->ymax = 8.0f;
}

void timeline_expand_boundbox(const Scene *scene, const ListBase *seqbase, rctf *rect)
{
  if (seqbase == nullptr) {
    return;
  }

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    rect->xmin = std::min<float>(rect->xmin, time_left_handle_frame_get(scene, strip) - 1);
    rect->xmax = std::max<float>(rect->xmax, time_right_handle_frame_get(scene, strip) + 1);
    /* We do +1 here to account for the channel thickness. Channel n has range of <n, n+1>. */
    rect->ymax = std::max(rect->ymax, strip->channel + 1.0f);
  }
}

void timeline_boundbox(const Scene *scene, const ListBase *seqbase, rctf *r_rect)
{
  timeline_init_boundbox(scene, r_rect);
  timeline_expand_boundbox(scene, seqbase, r_rect);
}

static bool strip_exists_at_frame(const Scene *scene,
                                  Span<Strip *> strips,
                                  const int timeline_frame)
{
  for (Strip *strip : strips) {
    if (time_strip_intersects_frame(scene, strip, timeline_frame)) {
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
  timeline_boundbox(scene, seqbase, &rectf);
  const int sfra = int(rectf.xmin);
  const int efra = int(rectf.xmax);
  int timeline_frame = initial_frame;
  r_gap_info->gap_exists = false;

  VectorSet strips = query_all_strips(seqbase);

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

bool time_strip_intersects_frame(const Scene *scene, const Strip *strip, const int timeline_frame)
{
  return (time_left_handle_frame_get(scene, strip) <= timeline_frame) &&
         (time_right_handle_frame_get(scene, strip) > timeline_frame);
}

bool time_has_left_still_frames(const Scene *scene, const Strip *strip)
{
  return time_left_handle_frame_get(scene, strip) < time_start_frame_get(strip);
}

bool time_has_right_still_frames(const Scene *scene, const Strip *strip)
{
  return time_right_handle_frame_get(scene, strip) > time_content_end_frame_get(scene, strip);
}

bool time_has_still_frames(const Scene *scene, const Strip *strip)
{
  return time_has_right_still_frames(scene, strip) || time_has_left_still_frames(scene, strip);
}

int time_strip_length_get(const Scene *scene, const Strip *strip)
{
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  if (retiming_is_active(strip)) {
    const int last_key_frame = retiming_key_timeline_frame_get(
        scene, strip, retiming_last_key_get(strip));
    /* Last key is mapped to last frame index. Numbering starts from 0. */
    const int sound_offset = time_get_rounded_sound_offset(strip, scene_fps);
    return last_key_frame + 1 - time_start_frame_get(strip) - sound_offset;
  }

  return strip->len / time_media_playback_rate_factor_get(strip, scene_fps);
}

float time_start_frame_get(const Strip *strip)
{
  return strip->start;
}

void time_start_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  strip->start = timeline_frame;
  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));
}

float time_content_end_frame_get(const Scene *scene, const Strip *strip)
{
  return time_start_frame_get(strip) + time_strip_length_get(scene, strip);
}

int time_left_handle_frame_get(const Scene * /*scene*/, const Strip *strip)
{
  if (strip->input1 || strip->input2) {
    return strip->startdisp;
  }

  return strip->start + strip->startofs;
}

int time_right_handle_frame_get(const Scene *scene, const Strip *strip)
{
  if (strip->input1 || strip->input2) {
    return strip->enddisp;
  }

  return time_content_end_frame_get(scene, strip) - strip->endofs;
}

void time_left_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  const float right_handle_orig_frame = time_right_handle_frame_get(scene, strip);

  if (timeline_frame >= right_handle_orig_frame) {
    timeline_frame = right_handle_orig_frame - 1;
  }

  float offset = timeline_frame - time_start_frame_get(strip);

  if (transform_single_image_check(strip)) {
    /* This strip has only 1 frame of content that is always stretched to the whole strip length.
     * Move strip start left and adjust end offset to be negative (rightwards past the 1 frame). */
    time_start_frame_set(scene, strip, timeline_frame);
    strip->endofs += offset;
  }
  else {
    strip->startofs = offset;
  }

  strip->startdisp = timeline_frame; /* Only to make files usable in older versions. */

  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));
}

void time_right_handle_frame_set(const Scene *scene, Strip *strip, int timeline_frame)
{
  const float left_handle_orig_frame = time_left_handle_frame_get(scene, strip);

  if (timeline_frame <= left_handle_orig_frame) {
    timeline_frame = left_handle_orig_frame + 1;
  }

  strip->endofs = time_content_end_frame_get(scene, strip) - timeline_frame;
  strip->enddisp = timeline_frame; /* Only to make files usable in older versions. */

  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));
}

void time_handles_frame_set(const Scene *scene,
                            Strip *strip,
                            int left_handle_timeline_frame,
                            int right_handle_timeline_frame)
{
  time_right_handle_frame_set(scene, strip, right_handle_timeline_frame);
  time_left_handle_frame_set(scene, strip, left_handle_timeline_frame);
}

void strip_time_translate_handles(const Scene *scene, Strip *strip, const int offset)
{
  strip->startofs += offset;
  strip->endofs -= offset;
  strip->startdisp += offset; /* Only to make files usable in older versions. */
  strip->enddisp -= offset;   /* Only to make files usable in older versions. */

  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
  time_update_meta_strip_range(scene, lookup_meta_by_strip(scene->ed, strip));
}

static void strip_time_slip_strip_ex(const Scene *scene,
                                     Strip *strip,
                                     int delta,
                                     float subframe_delta,
                                     bool slip_keyframes,
                                     bool recursed)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM && subframe_delta != 0.0f) {
    strip->sound_offset += subframe_delta / scene->frames_per_second();
  }

  if (delta == 0 && (!slip_keyframes || subframe_delta == 0.0f)) {
    return;
  }

  /* Skip effect strips where the length is dependent on another strip,
   * as they are calculated with #strip_time_update_effects_strip_range. */
  if (strip->input1 != nullptr || strip->input2 != nullptr) {
    return;
  }

  /* Effects only have a start frame and a length, so unless we're inside
   * a meta strip, there's no need to do anything. */
  if (!recursed && strip->is_effect()) {
    return;
  }

  /* Move strips inside meta strip. */
  if (strip->type == STRIP_TYPE_META) {
    /* If the meta strip has no contents, don't do anything. */
    if (BLI_listbase_is_empty(&strip->seqbase)) {
      return;
    }

    LISTBASE_FOREACH (Strip *, strip_child, &strip->seqbase) {
      /* The keyframes of strips inside meta strips should always be moved. */
      strip_time_slip_strip_ex(scene, strip_child, delta, subframe_delta, true, true);
    }
  }

  strip->start = strip->start + delta;

  if (slip_keyframes) {
    float anim_offset = delta;
    if (strip->type == STRIP_TYPE_SOUND_RAM) {
      anim_offset += subframe_delta;
    }
    offset_animdata(scene, strip, anim_offset);
  }

  if (!recursed) {
    strip->startofs = strip->startofs - delta;
    strip->endofs = strip->endofs + delta;
  }

  /* Only to make files usable in older versions. */
  strip->startdisp = time_left_handle_frame_get(scene, strip);
  strip->enddisp = time_right_handle_frame_get(scene, strip);

  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
}

void time_slip_strip(
    const Scene *scene, Strip *strip, int frame_delta, float subframe_delta, bool slip_keyframes)
{
  strip_time_slip_strip_ex(scene, strip, frame_delta, subframe_delta, slip_keyframes, false);
}

int time_get_rounded_sound_offset(const Strip *strip, const float frames_per_second)
{
  if (strip->type == STRIP_TYPE_SOUND_RAM && strip->sound != nullptr) {
    return round_fl_to_int((strip->sound->offset_time + strip->sound_offset) * frames_per_second);
  }
  return 0;
}

}  // namespace blender::seq
