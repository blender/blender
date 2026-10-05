/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>
#include <optional>

#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_bounds.hh"
#include "BLI_listbase.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.hh"

#include "BLF_api.hh"

#include "SEQ_animation.hh"
#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "effects/effects.hh"
#include "sequencer.hh"
#include "strip_time.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */
/** \name Transform Utilities
 * \{ */

bool transform_single_image_check(const Strip *strip)
{
  return (strip->flag & SEQ_SINGLE_FRAME_CONTENT) != 0;
}

bool transform_is_locked(const ListBaseT<SeqTimelineChannel> *channels, const Strip *strip)
{
  const SeqTimelineChannel *channel = channel_get_by_index(channels, strip->channel);
  return strip->flag & SEQ_LOCK ||
         (channel->is_locked() &&
          !flag_is_set(strip->runtime->flag, StripRuntimeFlag::IgnoreChannelLock));
}

bool transform_strip_can_be_translated(const Strip *strip)
{
  return !strip->is_effect_with_inputs();
}

bool transform_test_overlap(const Scene *scene, Strip *strip1, Strip *strip2)
{
  return (strip1 != strip2 && strip1->channel == strip2->channel &&
          ((strip1->right_handle(scene) <= strip2->left_handle()) ||
           (strip1->left_handle() >= strip2->right_handle(scene))) == 0);
}

bool transform_test_overlap(const Scene *scene, ListBaseT<Strip> *seqbasep, Strip *test)
{
  for (Strip &strip : *seqbasep) {
    if (transform_test_overlap(scene, test, &strip)) {
      return true;
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Timeline Strip Transform
 * \{ */

void transform_translate_strip(Scene *evil_scene, Strip *strip, int delta)
{
  if (delta == 0) {
    return;
  }

  /* Meta strips requires their content is to be translated, and then frame range of the meta is
   * updated based on nested strips. This won't work for empty meta-strips,
   * so they can be treated as normal strip. */
  if (strip->type == STRIP_TYPE_META && !strip->seqbase.is_empty()) {
    for (Strip &strip_child : strip->seqbase) {
      transform_translate_strip(evil_scene, &strip_child, delta);
    }
    /* Move meta start/end points. */
    const int left_handle = strip->left_handle();
    const int right_handle = strip->right_handle(evil_scene);
    strip->handles_set(evil_scene, left_handle + delta, right_handle + delta);
  }
  else if (strip->input1 == nullptr && strip->input2 == nullptr) { /* All other strip types. */
    strip->start += delta;
    /* Only to make files usable in older versions. */
    strip->startdisp = strip->left_handle();
    strip->enddisp = strip->right_handle(evil_scene);
  }

  offset_animdata(evil_scene, strip, delta);
  Span<Strip *> effects = lookup_effects_by_strip(evil_scene->ed, strip);
  strip_time_update_effects_strip_range(evil_scene, effects);
  time_update_meta_strip_range(evil_scene, lookup_meta_by_strip(evil_scene->ed, strip));
}

static bool strips_overlap_at_offset(const Scene *scene,
                                     const Strip *strip,
                                     const Strip *other,
                                     const int offset)
{
  BLI_assert(strip != other);
  return strip->channel == other->channel &&
         strip->left_handle() + offset < other->right_handle(scene) &&
         strip->right_handle(scene) + offset > other->left_handle();
}

static int shuffle_frame_offset_get(const Scene *scene,
                                    Span<Strip *> strips_to_shuffle,
                                    ListBaseT<Strip> *seqbasep,
                                    const Side side)
{
  BLI_assert(ELEM(side, Side::Left, Side::Right));

  int offset = 0;
  int offset_prev;

  do {
    offset_prev = offset;
    for (Strip *strip : strips_to_shuffle) {
      for (Strip &other : *seqbasep) {
        if (strips_to_shuffle.contains(&other) || relation_is_effect_of_strip(&other, strip) ||
            !strips_overlap_at_offset(scene, strip, &other, offset))
        {
          continue;
        }

        offset = (side == Side::Left) ?
                     min_ii(offset, other.left_handle() - strip->right_handle(scene)) :
                     max_ii(offset, other.right_handle(scene) - strip->left_handle());
      }
    }
  } while (offset != offset_prev);

  return offset;
}

static void transform_shuffle_horizontal(Scene *scene,
                                         ListBaseT<Strip> *seqbasep,
                                         Span<Strip *> strips_to_shuffle,
                                         Span<Strip *> time_dependent_strips,
                                         const bool use_sync_markers)
{
  const int offset_l = shuffle_frame_offset_get(scene, strips_to_shuffle, seqbasep, Side::Left);
  const int offset_r = shuffle_frame_offset_get(scene, strips_to_shuffle, seqbasep, Side::Right);
  const int offset = (-offset_l < offset_r) ? offset_l : offset_r;

  if (offset == 0) {
    return;
  }

  for (Strip *strip : strips_to_shuffle) {
    transform_translate_strip(scene, strip, offset);
  }
  for (Strip *strip : time_dependent_strips) {
    offset_animdata(scene, strip, offset);
  }

  if (use_sync_markers && !scene->toolsettings->lock_markers) {
    /* affect selected markers - it's unlikely that we will want to affect all in this way? */
    for (TimeMarker &marker : scene->markers) {
      if (marker.flag & SELECT) {
        marker.frame += offset;
      }
    }
  }
}

static bool shuffle_channel_is_free(const Scene *scene,
                                    ListBaseT<Strip> *seqbasep,
                                    const ListBaseT<SeqTimelineChannel> *channels,
                                    Strip *strip)
{
  const SeqTimelineChannel *channel = channel_get_by_index(channels, strip->channel);
  return !channel->is_muted() && !channel->is_locked() &&
         !transform_test_overlap(scene, seqbasep, strip);
}

void transform_shuffle_vertical(ListBaseT<Strip> *seqbasep,
                                Span<Strip *> strips,
                                Scene *scene,
                                const int channel_delta)
{
  BLI_assert(ELEM(channel_delta, -1, 1));

  if (!std::ranges::any_of(
          strips, [&](Strip *strip) { return transform_test_overlap(scene, seqbasep, strip); }))
  {
    return;
  }

  const ListBaseT<SeqTimelineChannel> *channels = channels_displayed_get(editing_get(scene));
  Vector<int> orig_channels;
  int min_channel = MAX_CHANNELS;
  int max_channel = 1;
  for (Strip *strip : strips) {
    orig_channels.append(strip->channel);
    min_channel = math::min(min_channel, strip->channel);
    max_channel = math::max(max_channel, strip->channel);
  }

  for (int delta = channel_delta; min_channel + delta >= 1 && max_channel + delta <= MAX_CHANNELS;
       delta += channel_delta)
  {
    /* Temporarily override the strips' channels to easily check for overlaps. */
    for (const int i : strips.index_range()) {
      strips[i]->channel_set(orig_channels[i] + delta);
    }
    if (std::ranges::all_of(strips,
                            [&](Strip *strip) {
                              return shuffle_channel_is_free(scene, seqbasep, channels, strip);
                            }))
    {
      return;
    }
  }

  /* Strips cannot be moved to next free channel, fallback to horizontal shuffling. */
  for (const int i : strips.index_range()) {
    strips[i]->channel_set(orig_channels[i]);
  }
  transform_shuffle_horizontal(scene, seqbasep, strips, {}, false);
}

static VectorSet<Strip *> extract_standalone_strips(Span<Strip *> transformed_strips)
{
  VectorSet<Strip *> standalone_strips;

  for (Strip *strip : transformed_strips) {
    if (!strip->is_effect_with_inputs()) {
      standalone_strips.add(strip);
    }
  }
  return standalone_strips;
}

rcti strip_int_bounds_get(const Scene *scene, const Strip *strip)
{
  rcti bounds;
  bounds.xmin = strip->left_handle();
  bounds.xmax = strip->right_handle(scene);
  bounds.ymin = strip->channel;
  bounds.ymax = strip->channel;
  return bounds;
}

Vector<RippleRange> transform_ripple_ranges_get(const Scene *scene, Span<Strip *> strips)
{
  Vector<Strip *> sorted_strips(strips);
  std::ranges::sort(sorted_strips, [](const Strip *a, const Strip *b) {
    return a->left_handle() < b->left_handle();
  });

  Vector<RippleRange> ranges;
  for (const Strip *strip : sorted_strips) {
    if (!ranges.is_empty() && strip->left_handle() <= ranges.last().end) {
      /* Attempt to extend an existing range. */
      ranges.last().end = math::max(ranges.last().end, strip->right_handle(scene));
      ranges.last().channels.add(strip->channel);
    }
    else {
      /* Start a new range. */
      ranges.append({strip->left_handle(), strip->right_handle(scene), {strip->channel}});
    }
  }
  return ranges;
}

bool transform_strip_is_on_rippled_channel(const RippleRange &range,
                                           const Strip *strip,
                                           const bool all_channels)
{
  return all_channels || range.channels.contains(strip->channel);
}

/* Determine whether we insert #source_strips into others, splitting them at #insert_frame.
 *
 * If the "ripple insert" option is disabled by the user, then #r_shuffle_delta will be filled
 * if the strip would otherwise be in a position to insert. In this case, we need to shuffle our
 * #source_strips forward to the nearest empty spot before rippling anything else.
 *
 * Possible outcomes:
 * - No insert, no shuffle. Strips stay fixed and ripple others out of the way without splitting.
 * - Insert, no shuffle. Strips stay fixed and split at `insert_frame` before rippling others.
 * - No insert, shuffle. Strips must move forward before rippling others.
 *
 * We assert that we never insert into other strips and shuffle ourselves at the same time!
 */
static bool should_ripple_insert(const Scene *scene,
                                 ListBaseT<Strip> *seqbasep,
                                 Span<Strip *> source_strips,
                                 const int insert_frame,
                                 const eSeqRippleFlag ripple_flag,
                                 std::optional<int> &r_shuffle_delta)
{
  bool do_insert = false;
  int shuffle_delta = 0;

  for (Strip &target : *seqbasep) {
    if (source_strips.contains(&target)) {
      continue;
    }
    /* The split point does not intersect in time with this strip. */
    if (target.left_handle() >= insert_frame || target.right_handle(scene) <= insert_frame) {
      continue;
    }
    for (Strip *source : source_strips) {
      if (target.channel == source->channel) {
        if (!(ripple_flag & SEQ_RIPPLE_INSERT)) {
          shuffle_delta = math::max(shuffle_delta, target.right_handle(scene) - insert_frame);
        }
        else {
          do_insert = true;
        }
        break;
      }
    }
  }

  BLI_assert(!(do_insert && (shuffle_delta > 0)));

  if (shuffle_delta > 0) {
    r_shuffle_delta = shuffle_delta;
  }

  return do_insert;
}

static bool sources_overlap_target(const Scene *scene, Span<Strip *> source_strips, Strip *target)
{
  for (Strip *source : source_strips) {
    if (transform_test_overlap(scene, source, target)) {
      return true;
    }
  }
  return false;
}

static Vector<Strip *> range_sources_get(const Scene *scene,
                                         Span<Strip *> source_strips,
                                         const RippleRange &range)
{
  Vector<Strip *> range_sources;
  for (Strip *source : source_strips) {
    if (source->left_handle() < range.end && source->right_handle(scene) > range.start) {
      range_sources.append(source);
    }
  }
  return range_sources;
}

static void strip_transform_ripple_range(Scene *scene,
                                         ListBaseT<Strip> *seqbasep,
                                         Span<Strip *> source_strips,
                                         const RippleRange &range,
                                         const eSeqRippleFlag ripple_flag)
{
  const bool all_channels = (ripple_flag & SEQ_RIPPLE_ALL_CHANNELS) != 0;
  const Vector<Strip *> range_sources = range_sources_get(scene, source_strips, range);

  int start_frame = range.start;
  int end_frame = range.end;

  std::optional<int> shuffle_delta;
  const bool do_insert = should_ripple_insert(
      scene, seqbasep, range_sources, start_frame, ripple_flag, shuffle_delta);

  /* Source strips need to be shuffled out of the way before rippling anything else. */
  if (shuffle_delta.has_value()) {
    for (Strip *source : extract_standalone_strips(range_sources)) {
      transform_translate_strip(scene, source, *shuffle_delta);
    }
    start_frame += *shuffle_delta;
    end_frame += *shuffle_delta;
  }

  int delta = 0;
  int ripple_frame = start_frame;
  if (do_insert) {
    delta = end_frame - start_frame;

    /* Split across all channels at `start_frame`.
     * Collect candidates beforehand to avoid changes mid-iteration. */
    Vector<Strip *> strips_to_split;
    for (Strip &target : *seqbasep) {
      if (source_strips.contains(&target)) {
        continue;
      }
      if (target.left_handle() >= start_frame || target.right_handle(scene) <= start_frame) {
        continue;
      }
      if (!transform_strip_is_on_rippled_channel(range, &target, all_channels)) {
        continue;
      }
      strips_to_split.append(&target);
    }

    /* Since this is a soft split with no data duplication, we can pass a nullptr `bmain`. */
    Main *bmain = nullptr;
    for (Strip *target : strips_to_split) {
      const char *error_msg = nullptr;
      edit_strip_split(bmain, scene, seqbasep, target, start_frame, SPLIT_SOFT, true, &error_msg);
    }
  }
  else {
    /* Ripple just enough to resolve overlaps, keeping strip group directly adjacent. */
    for (Strip &target : *seqbasep) {
      if (source_strips.contains(&target) ||
          !sources_overlap_target(scene, range_sources, &target))
      {
        continue;
      }
      delta = math::max(delta, end_frame - target.left_handle());
      ripple_frame = math::min(ripple_frame, target.left_handle());
    }
  }

  if (delta == 0) {
    return;
  }

  /* Translate all strips by `delta`, including any right-half strips created from splits above. */
  for (Strip &target : *seqbasep) {
    if (source_strips.contains(&target)) {
      continue;
    }
    if (target.left_handle() < ripple_frame) {
      continue;
    }
    if (!transform_strip_is_on_rippled_channel(range, &target, all_channels)) {
      continue;
    }
    transform_translate_strip(scene, &target, delta);
  }

  if ((ripple_flag & SEQ_RIPPLE_MARKERS) != 0 && !scene->toolsettings->lock_markers) {
    for (TimeMarker &marker : scene->markers) {
      if (marker.frame >= ripple_frame) {
        marker.frame += delta;
      }
    }
  }
}

static void strip_transform_handle_ripple(Scene *scene,
                                          ListBaseT<Strip> *seqbasep,
                                          Span<Strip *> source_strips,
                                          const eSeqRippleFlag ripple_flag)
{
  for (const RippleRange &range : transform_ripple_ranges_get(scene, source_strips)) {
    strip_transform_ripple_range(scene, seqbasep, source_strips, range, ripple_flag);
  }
}

static VectorSet<Strip *> query_overwrite_targets(const Scene *scene,
                                                  ListBaseT<Strip> *seqbasep,
                                                  Span<Strip *> source_strips)
{
  VectorSet<Strip *> targets = query_unselected_strips(seqbasep);

  /* Effects of transformed strips can be unselected. These must not be included. */
  targets.remove_if([&](Strip *strip) { return source_strips.contains(strip); });
  targets.remove_if([&](Strip *strip) {
    bool does_overlap = false;
    for (Strip *source : source_strips) {
      if (transform_test_overlap(scene, strip, source)) {
        does_overlap = true;
      }
    }

    return !does_overlap;
  });

  return targets;
}

static void strip_transform_handle_overwrite_trim(Scene *scene,
                                                  const Strip *source,
                                                  Strip *target,
                                                  bool covers_left)
{
  Editing *ed = seq::editing_get(scene);
  VectorSet<Strip *> targets;
  targets.add(target);
  expand_strips(ed, targets, StripRelation::EffectChain);

  /* We can't just let `left/right_handle_set` trim effect chains, since `target` may be an
   * effect itself. So we need to find all non-effects that may be inputs to effects, and
   * trim those. */
  for (Strip *strip : targets) {
    if (strip->is_effect_with_inputs()) {
      continue;
    }
    if (covers_left) {
      strip->left_handle_set(scene, source->right_handle(scene));
    }
    else {
      strip->right_handle_set(scene, source->left_handle());
    }
  }
}

static Strip *strip_transform_handle_overwrite_split(Scene *scene,
                                                     ListBaseT<Strip> *seqbasep,
                                                     const Strip *source,
                                                     Strip *target)
{
  /*
   * Split the `target` strip, move the right half over, and fit `source` inside.
   * Return newly created strip if applicable to serve as a new overlap target.
   *
   *  The `bmain` argument is only needed for hard splits and data duplication cases in
   * `edit_strip_split`. Since this doesn't concern either case, we can pass `nullptr` here.
   */
  Main *bmain = nullptr;
  const char *error_msg = nullptr;
  Strip *right_strip = edit_strip_split(
      bmain, scene, seqbasep, target, source->left_handle(), SPLIT_SOFT, true, &error_msg);
  if (right_strip == nullptr) {
    return nullptr;
  }
  right_strip->left_handle_set(scene, source->right_handle(scene));
  return right_strip;
}

static void strip_transform_handle_overwrite(Scene *scene,
                                             ListBaseT<Strip> *seqbasep,
                                             Span<Strip *> source_strips)
{
  const ListBaseT<SeqTimelineChannel> *channels = channels_displayed_get(editing_get(scene));

  VectorSet<Strip *> targets = query_overwrite_targets(scene, seqbasep, source_strips);
  VectorSet<Strip *> to_delete;

  /* We must use index-based iteration since new targets may be added during overwrite splits. */
  for (int i = 0; i < targets.size(); i++) {
    Strip *target = targets[i];
    for (Strip *source : source_strips) {
      if (transform_is_locked(channels, target) || !transform_test_overlap(scene, source, target))
      {
        continue;
      }

      const bool covers_left = source->left_handle() <= target->left_handle();
      const bool covers_right = source->right_handle(scene) >= target->right_handle(scene);

      /* Source strip entirely overlaps target strip. */
      if (covers_left && covers_right) {
        to_delete.add(target);
      }
      /* Source strip only partially overlaps target strip. */
      else if (covers_left || covers_right) {
        strip_transform_handle_overwrite_trim(scene, source, target, covers_left);
      }
      /* Source strip exists entirely within target strip. */
      else {
        Strip *new_strip = strip_transform_handle_overwrite_split(scene, seqbasep, source, target);
        if (new_strip) {
          targets.add(new_strip);
        }
      }
    }
  }

  /* Remove all entirely overlapped strips. This must be done in a separate loop because a split
   * can invoke `edit_strip_split`, which also calls `edit_remove_flagged_strips` internally. */
  for (Strip *strip : to_delete) {
    edit_flag_for_removal(scene, strip);
  }
  edit_remove_flagged_strips(scene, seqbasep);
}

static Side strip_selected_handles_get(const Strip *strip)
{
  const bool left_selected = (strip->flag & SEQ_LEFTSEL) != 0;
  const bool right_selected = (strip->flag & SEQ_RIGHTSEL) != 0;

  if (left_selected && right_selected) {
    return Side::Both;
  }
  if (left_selected) {
    return Side::Left;
  }
  if (right_selected) {
    return Side::Right;
  }
  return Side::None;
}

static int shuffle_handle_offset_get(const Scene *scene,
                                     Span<Strip *> strips_to_shuffle,
                                     ListBaseT<Strip> *seqbasep)
{
  int offset = 0;

  /* Note that this algorithm looks similar to #shuffle_frame_offset_get, but the two are slightly
   * different: handles need only a single pass, and with strips, the direction to shuffle is the
   * same for all, but here handles can be both left & right (which we may eventually remove). */
  for (Strip *strip : strips_to_shuffle) {
    const bool is_left = strip_selected_handles_get(strip) == Side::Left;
    for (Strip &other : *seqbasep) {
      if (strips_to_shuffle.contains(&other) || relation_is_effect_of_strip(&other, strip) ||
          !transform_test_overlap(scene, strip, &other))
      {
        continue;
      }

      offset = is_left ? max_ii(offset, other.right_handle(scene) - strip->left_handle()) :
                         min_ii(offset, other.left_handle() - strip->right_handle(scene));
    }
  }

  return offset;
}

static void transform_shuffle_handles(const Scene *scene,
                                      ListBaseT<Strip> *seqbasep,
                                      Span<Strip *> strips_to_shuffle)
{
  const int offset = shuffle_handle_offset_get(scene, strips_to_shuffle, seqbasep);

  if (offset == 0) {
    return;
  }

  for (Strip *strip : strips_to_shuffle) {
    if (strip_selected_handles_get(strip) == Side::Left) {
      strip->left_handle_set(scene, strip->left_handle() + offset);
    }
    else {
      strip->right_handle_set(scene, strip->right_handle(scene) + offset);
    }
  }
}

static void strip_transform_handle_shuffle(Scene *scene,
                                           ListBaseT<Strip> *seqbasep,
                                           Span<Strip *> source_strips,
                                           Span<Strip *> time_dependent_strips,
                                           bool use_sync_markers)
{
  VectorSet<Strip *> strips_to_shuffle;
  VectorSet<Strip *> handles_to_shuffle;

  /* If neither or both handles are selected, then add to #strips_to_shuffle
   * for shuffling the strip as a whole. */
  for (Strip *strip : extract_standalone_strips(source_strips)) {
    if (ELEM(strip_selected_handles_get(strip), Side::Left, Side::Right)) {
      handles_to_shuffle.add(strip);
    }
    else {
      strips_to_shuffle.add(strip);
    }
  }

  /* First shuffle handles. */
  transform_shuffle_handles(scene, seqbasep, handles_to_shuffle);

  /* Shuffle entire strips. */
  transform_shuffle_horizontal(
      scene, seqbasep, strips_to_shuffle, time_dependent_strips, use_sync_markers);
}

void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> source_strips,
                              bool use_sync_markers,
                              Span<Strip *> time_dependent_strips)
{
  transform_handle_overlap(scene,
                           seqbasep,
                           source_strips,
                           use_sync_markers,
                           tool_settings_overlap_mode_get(scene),
                           tool_settings_ripple_flag_get(scene),
                           time_dependent_strips);
}

void transform_handle_overlap(Scene *scene,
                              ListBaseT<Strip> *seqbasep,
                              Span<Strip *> source_strips,
                              bool use_sync_markers,
                              const eSeqOverlapMode overlap_mode,
                              const eSeqRippleFlag ripple_flag,
                              Span<Strip *> time_dependent_strips)
{
  switch (overlap_mode) {
    case SEQ_OVERLAP_RIPPLE:
      strip_transform_handle_ripple(scene, seqbasep, source_strips, ripple_flag);
      break;
    case SEQ_OVERLAP_OVERWRITE:
      strip_transform_handle_overwrite(scene, seqbasep, source_strips);
      break;
    case SEQ_OVERLAP_SHUFFLE:
      strip_transform_handle_shuffle(
          scene, seqbasep, source_strips, time_dependent_strips, use_sync_markers);
      break;
  }

  /* Strips can still overlap even after being handled above. Examples: user tries to place a
   * strip on top of its effect, on top of a locked strip, or inside a transition (split fails).
   */
  for (Strip *strip : source_strips) {
    transform_shuffle_vertical(seqbasep, {strip}, scene);
    strip->runtime->flag &= ~StripRuntimeFlag::Overlap;
  }
}

void transform_strips_after_frame(Scene *scene,
                                  ListBaseT<Strip> *seqbase,
                                  const int timeline_frame,
                                  const int delta)
{
  for (Strip &strip : *seqbase) {
    if (strip.left_handle() >= timeline_frame) {
      transform_translate_strip(scene, &strip, delta);
      relations_invalidate_cache(scene, &strip);
    }
  }

  if (!scene->toolsettings->lock_markers) {
    for (TimeMarker &marker : scene->markers) {
      if (marker.frame >= timeline_frame) {
        marker.frame += delta;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview Image Transform
 * \{ */

/** Get size of strip's rendered `ImBuf` (may be different than drawn boundbox in preview). */
static int2 strip_source_size_get(const Scene *scene, const Strip *strip)
{
  const int2 parent_scene_size = int2(scene->r.xsch, scene->r.ysch);

  switch (strip->type) {
    case STRIP_TYPE_IMAGE: {
      const StripElem *se = seq::render_give_stripelem(scene, strip, scene->r.cfra);
      if (se == nullptr) {
        return parent_scene_size;
      }
      return int2(se->orig_width, se->orig_height);
    }
    case STRIP_TYPE_MOVIE: {
      const StripElem *se = strip->data->stripdata;
      return int2(se->orig_width, se->orig_height);
    }
    case STRIP_TYPE_MOVIECLIP: {
      const MovieClip *clip = strip->clip;
      if (clip && clip->lastsize[0] != 0 && clip->lastsize[1] != 0) {
        return int2(clip->lastsize[0], clip->lastsize[1]);
      }
      return parent_scene_size;
    }
    case STRIP_TYPE_SCENE: {
      /* TODO(@john): Sequencer-input scene strips render at their parent sequencer scene's
       * resolution; they should probably only render their own scene resolution. */
      if (strip->scene == nullptr || (strip->flag & SEQ_SCENE_STRIPS) != 0) {
        return parent_scene_size;
      }
      return int2(strip->scene->r.xsch, strip->scene->r.ysch);
    }
    case STRIP_TYPE_COLOR: {
      const SolidColorVars *cv = static_cast<const SolidColorVars *>(strip->effectdata);
      return int2(cv->width, cv->height);
    }
    default:
      return parent_scene_size;
  }
}

float2 image_transform_mirror_factor_get(const Strip *strip)
{
  float2 mirror(1.0f, 1.0f);

  if ((strip->flag & SEQ_FLIPX) != 0) {
    mirror.x = -1.0f;
  }
  if ((strip->flag & SEQ_FLIPY) != 0) {
    mirror.y = -1.0f;
  }
  return mirror;
}

int2 image_transform_box_size_get(const Scene *scene, const Strip *strip)
{
  const int2 source_size = strip_source_size_get(scene, strip);

  if (strip->type == STRIP_TYPE_TEXT) {
    TextVars *data = static_cast<TextVars *>(strip->effectdata);
    std::scoped_lock runtime_lock(text_runtime_mutex_get());
    text_effect_update_runtime(nullptr, *data, source_size);
    BLF_disable(data->runtime->font, BLF_BOLD | BLF_ITALIC);
    const int2 text_size(BLI_rcti_size_x(&data->runtime->text_boundbox),
                         BLI_rcti_size_y(&data->runtime->text_boundbox));
    return text_size;
  }

  return source_size;
}

/* Convert origin from a 0->1 range (where (0,0) is the bottom left of the image)
 * to the offset in image-space pixels from an image's center. */
static float2 convert_origin_to_image_offset(const Scene *scene, const Strip *strip, float2 origin)
{
  const float2 box_size = float2(image_transform_box_size_get(scene, strip));
  return box_size * origin - (box_size / 2.0f);
}

float2 image_transform_origin_get(const Scene *scene, const Strip *strip)
{

  const StripTransform *tr = strip->data->transform;
  if (strip->type != STRIP_TYPE_TEXT) {
    return tr->origin;
  }

  /* Text strips are the only type where their box is not the size of the rendered image. We must
   * convert from an origin relative to the text box -> an origin relative to the whole render. */
  const float2 text_size = float2(image_transform_box_size_get(scene, strip));
  const float2 render_size(scene->r.xsch, scene->r.ysch);

  /* Before we scale the text origin down to produce the render origin, we must offset the origin
   * so that (0,0) corresponds to the center instead of (0.5, 0.5) for correct math. */
  const float2 offset_text_origin = float2(tr->origin) - float2(0.5f);
  const float2 render_origin = float2(0.5f) + offset_text_origin * (text_size / render_size);
  return render_origin;
}

static float2 image_transform_viewport_scale_get(const Scene *scene, const Strip *strip)
{
  const float2 viewport_pixel_aspect(scene->r.xasp / scene->r.yasp, 1.0f);
  return image_transform_mirror_factor_get(strip) * viewport_pixel_aspect;
}

float2 image_transform_origin_preview_offset_get(const Scene *scene, const Strip *strip)
{
  const StripTransform *tr = strip->data->transform;
  const float2 origin_offset_imgpx = convert_origin_to_image_offset(scene, strip, tr->origin);

  /* Note that we do not have to consider the strip's rotation or scaling here, since they always
   * act with respect to the origin, and thus do not change the origin's position. */
  return (float2(tr->xofs, tr->yofs) + origin_offset_imgpx) *
         image_transform_viewport_scale_get(scene, strip);
}

float3x3 image_transform_matrix_get(const Scene *scene, const Strip *strip)
{
  const StripTransform *tr = strip->data->transform;
  const float3x3 matrix = math::from_loc_rot_scale<float3x3>(
      float2(tr->xofs, tr->yofs), tr->rotation, float2(tr->scale_x, tr->scale_y));
  const float2 origin_offset_imgpx = convert_origin_to_image_offset(scene, strip, tr->origin);

  return math::from_scale<float3x3>(image_transform_viewport_scale_get(scene, strip)) *
         math::from_origin_transform(matrix, origin_offset_imgpx);
}

Array<float2> image_transform_quad_get(const Scene *scene, const Strip *strip)
{
  constexpr int num_corners = 4;
  const float2 box_size = float2(image_transform_box_size_get(scene, strip));

  /* Raw quad before any rotation/scaling or text anchoring is applied.
   *
   * NOTE: For text strips, crops should only affect their visible result and not their bounding
   * box. Text effects can stray outside, so crop works on the full render buffer. */
  const StripCrop no_crop{};
  const StripCrop *crop = (strip->type == STRIP_TYPE_TEXT) ? &no_crop : strip->data->crop;
  float2 quad[num_corners]{
      {(box_size.x / 2) - crop->right, (box_size.y / 2) - crop->top},     /* Top right. */
      {(box_size.x / 2) - crop->right, (-box_size.y / 2) + crop->bottom}, /* Bottom right. */
      {(-box_size.x / 2) + crop->left, (-box_size.y / 2) + crop->bottom}, /* Bottom left. */
      {(-box_size.x / 2) + crop->left, (box_size.y / 2) - crop->top},     /* Top left. */
  };

  if (strip->type == STRIP_TYPE_TEXT) {
    const TextVars *data = static_cast<TextVars *>(strip->effectdata);
    float2 offset(0, 0);

    switch (data->anchor_x) {
      case SEQ_TEXT_ANCHOR_X_LEFT:
        offset.x += box_size.x / 2.0f;
        break;
      case SEQ_TEXT_ANCHOR_X_CENTER:
        break;
      case SEQ_TEXT_ANCHOR_X_RIGHT:
        offset.x += -box_size.x / 2.0f;
        break;
      default:
        break;
    }
    switch (data->anchor_y) {
      case SEQ_TEXT_ANCHOR_Y_BOTTOM:
        offset.y += box_size.y / 2.0f;
        break;
      case SEQ_TEXT_ANCHOR_Y_CENTER:
        break;
      case SEQ_TEXT_ANCHOR_Y_TOP:
        offset.y += -box_size.y / 2.0f;
        break;
      default:
        break;
    }

    for (float2 &corner : quad) {
      corner += offset;
    }
  }

  const float3x3 matrix = image_transform_matrix_get(scene, strip);

  Array<float2> quad_final(num_corners);
  for (const int i : IndexRange(num_corners)) {
    quad_final[i] = math::transform_point(matrix, quad[i]);
  }
  return quad_final;
}

float2 image_preview_unit_to_px(const Scene *scene, const float2 co_src)
{
  return {co_src.x * scene->r.xsch, co_src.y * scene->r.ysch};
}

float2 image_preview_unit_from_px(const Scene *scene, const float2 co_src)
{
  return {co_src.x / scene->r.xsch, co_src.y / scene->r.ysch};
}

static Bounds<float2> negative_bounds()
{
  return {float2(std::numeric_limits<float>::max()), float2(std::numeric_limits<float>::lowest())};
}

Bounds<float2> image_transform_bounding_box_from_strips_get(Scene *scene, Span<Strip *> strips)
{
  Bounds<float2> bounding_box = negative_bounds();

  for (Strip *strip : strips) {
    const Array<float2> quad = image_transform_quad_get(scene, strip);
    const Bounds<float2> strip_bounding_box = *bounds::min_max(quad.as_span());
    bounding_box = bounds::merge(bounding_box, strip_bounding_box);
  }

  return bounding_box;
}

/** \} */

}  // namespace blender::seq
