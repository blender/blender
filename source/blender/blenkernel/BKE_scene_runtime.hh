/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_timeit.hh"
#include "BLI_utility_mixins.hh"

namespace blender::bke {

/* Runtime data specific to the compositing trees. */
struct CompositorRuntime {
  /* Per-node instance total execution time for the corresponding node, during the last tree
   * evaluation. */
  Map<bNodeInstanceKey, timeit::Nanoseconds> per_node_execution_time;
};

class SceneRuntime : NonCopyable, NonMovable {
 public:
  CompositorRuntime compositor;
};

}  // namespace blender::bke
