/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"

#include "BKE_movieclip.hh"
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

float give_frame_index(const Scene *scene, const Strip *strip, float timeline_frame)
{
  float frame_index;
  float sta = strip->content_start();
  float end = strip->is_effect() ? strip->right_handle(scene) : strip->content_end(scene) - 1;

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
  frame_index *= strip->media_playback_rate_factor(scene_fps);

  if (retiming_is_active(strip)) {
    const float retiming_factor = strip_retiming_evaluate(strip, frame_index);
    /* Retiming maps frame index from 0 up to `strip->len`, because key is positioned at the end of
     * last frame. Otherwise the last frame could not be retimed. */
    frame_index = retiming_factor * strip->len;
  }
  /* Clamp frame index to strip content frame range. */
  float frame_index_max = strip->is_effect() ? end - sta : strip->len - 1;
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
    else if (ELEM(strip->type, STRIP_TYPE_SOUND, STRIP_TYPE_SCENE)) {
      if (strip->runtime->scene_sound) {
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
                                   strip->runtime->scene_sound,
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

  const int strip_start = strip_meta->left_handle();
  const int strip_end = strip_meta->right_handle(scene);

  int min = MAXFRAME * 2;
  int max = -MAXFRAME * 2;
  LISTBASE_FOREACH (Strip *, strip, &strip_meta->seqbase) {
    min = min_ii(strip->left_handle(), min);
    max = max_ii(strip->right_handle(scene), max);
  }

  strip_meta->start = min + strip_meta->anim_startofs;
  strip_meta->len = max - strip_meta->anim_endofs - strip_meta->start;

  /* Functions `SEQ_time_*_handle_frame_set()` can not be used here, because they are clamped, so
   * change must be done at once. */
  strip_meta->startofs = strip_start - strip_meta->start;
  strip_meta->startdisp = strip_start; /* Only to make files usable in older versions. */
  strip_meta->endofs = strip_meta->start + strip_meta->length(scene) - strip_end;
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
    strip->startdisp = max_ii(strip->input1->left_handle(), strip->input2->left_handle());
    strip->enddisp = min_ii(strip->input1->right_handle(scene),
                            strip->input2->right_handle(scene));
  }
  else if (strip->input1) { /* Single input effect. */
    strip->startdisp = strip->input1->right_handle(scene);
    strip->enddisp = strip->input1->left_handle();
  }
  else if (strip->input2) { /* Strip may be missing one of inputs. */
    strip->startdisp = strip->input2->right_handle(scene);
    strip->enddisp = strip->input2->left_handle();
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

    if (do_unselected && (strip->flag & SEQ_SELECT)) {
      continue;
    }

    if (do_center) {
      strip_frames[0] = (strip->left_handle() + strip->right_handle(scene)) / 2;
      strip_frames_tot = 1;
    }
    else {
      strip_frames[0] = strip->left_handle();
      strip_frames[1] = strip->right_handle(scene);

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
    rect->xmin = std::min<float>(rect->xmin, strip->left_handle() - 1);
    rect->xmax = std::max<float>(rect->xmax, strip->right_handle(scene) + 1);
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
    if (strip->intersects_frame(scene, timeline_frame)) {
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

static void strip_time_slip_strip_ex(const Scene *scene,
                                     Strip *strip,
                                     int delta,
                                     float subframe_delta,
                                     bool slip_keyframes,
                                     bool recursed)
{
  if (strip->type == STRIP_TYPE_SOUND && subframe_delta != 0.0f) {
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
    if (strip->type == STRIP_TYPE_SOUND) {
      anim_offset += subframe_delta;
    }
    offset_animdata(scene, strip, anim_offset);
  }

  if (!recursed) {
    strip->startofs = strip->startofs - delta;
    strip->endofs = strip->endofs + delta;
  }

  /* Only to make files usable in older versions. */
  strip->startdisp = strip->left_handle();
  strip->enddisp = strip->right_handle(scene);

  Span<Strip *> effects = SEQ_lookup_effects_by_strip(scene->ed, strip);
  strip_time_update_effects_strip_range(scene, effects);
}

void time_slip_strip(
    const Scene *scene, Strip *strip, int frame_delta, float subframe_delta, bool slip_keyframes)
{
  strip_time_slip_strip_ex(scene, strip, frame_delta, subframe_delta, slip_keyframes, false);
}

}  // namespace blender::seq

float Strip::media_playback_rate_factor(float scene_fps) const
{
  if ((this->flag & SEQ_AUTO_PLAYBACK_RATE) == 0) {
    return 1.0f;
  }
  if (this->media_playback_rate == 0.0f) {
    return 1.0f;
  }
  return this->media_playback_rate / scene_fps;
}

float Strip::media_fps(Scene *scene)
{
  switch (this->type) {
    case STRIP_TYPE_MOVIE: {
      blender::seq::strip_open_anim_file(scene, this, true);
      const MovieReader *anim = this->runtime->movie_reader_get();
      if (anim == nullptr) {
        return 0.0f;
      }
      return MOV_get_fps(anim);
    }
    case STRIP_TYPE_MOVIECLIP:
      if (this->clip != nullptr) {
        return BKE_movieclip_get_fps(this->clip);
      }
      break;
    case STRIP_TYPE_SCENE:
      if (this->scene != nullptr) {
        return float(this->scene->r.frs_sec) / this->scene->r.frs_sec_base;
      }
      break;
  }
  return 0.0f;
}

float Strip::content_start() const
{
  return this->start;
}

void Strip::content_start_set(const Scene *scene, int timeline_frame)
{
  this->start = timeline_frame;
  blender::Span<Strip *> effects = blender::seq::SEQ_lookup_effects_by_strip(scene->ed, this);
  blender::seq::strip_time_update_effects_strip_range(scene, effects);
  blender::seq::time_update_meta_strip_range(scene,
                                             blender::seq::lookup_meta_by_strip(scene->ed, this));
}

float Strip::content_end(const Scene *scene) const
{
  return this->content_start() + this->length(scene);
}

int Strip::length(const Scene *scene) const
{
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  if (blender::seq::retiming_is_active(this)) {
    const int last_key_frame = blender::seq::retiming_key_timeline_frame_get(
        scene, this, blender::seq::retiming_last_key_get(this));
    /* Last key is mapped to last frame index. Numbering starts from 0. */
    const int sound_offset = this->rounded_sound_offset(scene_fps);
    return last_key_frame - this->content_start() - sound_offset;
  }

  return this->len / this->media_playback_rate_factor(scene_fps);
}

int Strip::rounded_sound_offset(float scene_fps) const
{
  if (this->type == STRIP_TYPE_SOUND && this->sound != nullptr) {
    return round_fl_to_int((this->sound->offset_time + this->sound_offset) * scene_fps);
  }
  return 0;
}

int Strip::left_handle() const
{
  if (this->input1 || this->input2) {
    return this->startdisp;
  }

  return this->start + this->startofs;
}

int Strip::right_handle(const Scene *scene) const
{
  if (this->input1 || this->input2) {
    return this->enddisp;
  }

  return this->content_end(scene) - this->endofs;
}

void Strip::left_handle_set(const Scene *scene, int timeline_frame)
{
  const float right_handle_orig_frame = this->right_handle(scene);

  if (timeline_frame >= right_handle_orig_frame) {
    timeline_frame = right_handle_orig_frame - 1;
  }

  float offset = timeline_frame - this->content_start();

  if (blender::seq::transform_single_image_check(this)) {
    /* This strip has only 1 frame of content that is always stretched to the whole strip length.
     * Move strip start left and adjust end offset to be negative (rightwards past the 1 frame). */
    this->content_start_set(scene, timeline_frame);
    this->endofs += offset;
  }
  else {
    this->startofs = offset;
  }

  this->startdisp = timeline_frame; /* Only to make files usable in older versions. */

  blender::Span<Strip *> effects = blender::seq::SEQ_lookup_effects_by_strip(scene->ed, this);
  blender::seq::strip_time_update_effects_strip_range(scene, effects);
  blender::seq::time_update_meta_strip_range(scene,
                                             blender::seq::lookup_meta_by_strip(scene->ed, this));
}

void Strip::right_handle_set(const Scene *scene, int timeline_frame)
{
  const float left_handle_orig_frame = this->left_handle();

  if (timeline_frame <= left_handle_orig_frame) {
    timeline_frame = left_handle_orig_frame + 1;
  }

  this->endofs = this->content_end(scene) - timeline_frame;
  this->enddisp = timeline_frame; /* Only to make files usable in older versions. */

  blender::Span<Strip *> effects = blender::seq::SEQ_lookup_effects_by_strip(scene->ed, this);
  blender::seq::strip_time_update_effects_strip_range(scene, effects);
  blender::seq::time_update_meta_strip_range(scene,
                                             blender::seq::lookup_meta_by_strip(scene->ed, this));
}

void Strip::handles_set(const Scene *scene,
                        int left_handle_timeline_frame,
                        int right_handle_timeline_frame)
{
  this->right_handle_set(scene, right_handle_timeline_frame);
  this->left_handle_set(scene, left_handle_timeline_frame);
}

bool Strip::intersects_frame(const Scene *scene, const int timeline_frame) const
{
  return (this->left_handle() <= timeline_frame) && (this->right_handle(scene) > timeline_frame);
}
