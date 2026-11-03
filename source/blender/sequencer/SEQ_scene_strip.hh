/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BKE_scene.hh"

/** \file
 * \ingroup sequencer
 */

namespace blender {

struct Scene;
struct Strip;

namespace seq {

/**
 * Return the strip that is currently used for time synchronization in the given sequencer scene.
 */
const Strip *scene_strip_for_time_sync_get(const Scene *sequencer_scene);

/**
 * Get the frame range of the active scene strip for the given sequencer scene.
 */
std::optional<ScenePlaybackRange> scene_strip_frame_range_get(const Scene *sequencer_scene);

/**
 * Return the playback range for the given sequencer scene. The preview range takes precedence over
 * the scene strip range. Falls back to #BKE_scene_get_playback_range if the scene is not used in a
 * sequencer editor.
 */
ScenePlaybackRange scene_playback_range_get(const Scene *sequencer_scene);

}  // namespace seq
}  // namespace blender
