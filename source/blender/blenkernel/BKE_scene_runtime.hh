/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BKE_compositor.hh"
#include "BKE_sound_types.hh"

#include "BLI_set.hh"
#include "BLI_utility_mixins.hh"

namespace blender {

struct Depsgraph;

namespace nodes::eval_log {
class NodesEvalLog;
}  // namespace nodes::eval_log

namespace bke {

/* Runtime data specific to the compositing trees. */
class CompositorRuntime {
 public:
  /* A nodes log of the last compositor evaluation. */
  std::unique_ptr<nodes::eval_log::NodesEvalLog> nodes_evaluation_log;
  /* Cached data for the interactive compositor. */
  bke::compositor::Cache cache;
  /* A dependency graph used for interactive compositing. This is initialized the first time it is
   * needed, and then kept persistent for the lifetime of the scene. This is done to allow the
   * compositor to track changes to resources its uses as well as reduce the overhead of creating
   * the dependency graph every time it executes. */
  Depsgraph *preview_depsgraph = nullptr;

  ~CompositorRuntime();
};

/* Runtime data specific to the sequencer, e.g. when using scene strips. */
class SequencerRuntime {
 public:
  Depsgraph *depsgraph = nullptr;

  ~SequencerRuntime();
};

/* Audio runtime data. */
struct SceneAudioRuntime {
  AUD_Sequence sound_scene;
  AUD_Handle playback_handle;
  AUD_Handle sound_scrub_handle;
  Set<AUD_SequenceEntry> speaker_handles;
};

class SceneRuntime : NonCopyable, NonMovable {
 public:
  CompositorRuntime compositor;
  SequencerRuntime sequencer;
  SceneAudioRuntime audio;
};

}  // namespace bke
}  // namespace blender
