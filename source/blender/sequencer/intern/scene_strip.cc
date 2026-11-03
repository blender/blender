/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <algorithm>
#include <utility>

#include "BLI_listbase.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_scene.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_render.hh"
#include "SEQ_scene_strip.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

namespace blender::seq {

const Strip *scene_strip_for_time_sync_get(const Scene *sequencer_scene)
{
  const Editing *ed = editing_get(sequencer_scene);
  if (!ed) {
    return nullptr;
  }
  ListBaseT<Strip> *seqbase = active_seqbase_get(ed);
  const ListBaseT<SeqTimelineChannel> *channels = channels_displayed_get(ed);
  VectorSet<Strip *> query_strips = query_strips_recursive_at_frame(
      sequencer_scene, seqbase, sequencer_scene->r.cfra);
  /* Ignore effect strips, sound strips and muted strips. */
  query_strips.remove_if([&](const Strip *strip) {
    return strip->is_effect() || strip->type == STRIP_TYPE_SOUND ||
           render_is_muted(channels, strip);
  });
  Vector<Strip *> strips = query_strips.extract_vector();
  /* Sort strips by channel. */
  std::ranges::sort(strips,
                    [](const Strip *a, const Strip *b) { return a->channel > b->channel; });
  /* Get the top-most scene strip. */
  for (const Strip *strip : strips) {
    if (strip->type == STRIP_TYPE_SCENE) {
      return strip;
    }
  }
  return nullptr;
}

std::optional<ScenePlaybackRange> scene_strip_frame_range_get(const Scene *sequencer_scene)
{
  const Strip *scene_strip = scene_strip_for_time_sync_get(sequencer_scene);
  if (!scene_strip || !scene_strip->scene) {
    return std::nullopt;
  }

  /* ..._handle are frames in "sequencer logic", meaning that on the right_handle point in time,
   * the strip is not visible any more. The last visible frame of the strip is actually on
   * (right_handle-1), hence the -1 when computing the end_frame. */
  const float left_handle = scene_strip->left_handle();
  const float right_handle = scene_strip->right_handle(sequencer_scene);
  float start_frame = give_frame_index(sequencer_scene, scene_strip, left_handle) +
                      scene_strip->scene->r.sfra + scene_strip->anim_startofs;
  float end_frame = give_frame_index(sequencer_scene, scene_strip, right_handle - 1) +
                    scene_strip->scene->r.sfra + scene_strip->anim_startofs;

  /* This can happen when the strip time is reversed. */
  if (start_frame > end_frame) {
    std::swap(start_frame, end_frame);
  }

  return ScenePlaybackRange{int(start_frame), int(end_frame)};
}

ScenePlaybackRange scene_playback_range_get(const Scene *sequencer_scene)
{
  if ((sequencer_scene->r.flag & SCER_PRV_RANGE) == 0 && sequencer_scene->ed &&
      sequencer_scene->ed->runtime && sequencer_scene->ed->runtime->use_scene_strip_range)
  {
    if (const std::optional<ScenePlaybackRange> range = scene_strip_frame_range_get(
            sequencer_scene))
    {
      return *range;
    }
  }

  /* Fall back to the scene playback range when strip-range playback does not apply. */
  return BKE_scene_get_playback_range(sequencer_scene);
}

}  // namespace blender::seq
