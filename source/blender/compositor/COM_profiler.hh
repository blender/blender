/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_timeit.hh"

#include "DNA_node_types.h"

namespace blender::compositor {

class Context;

/* -------------------------------------------------------------------------------------------------
 * Profiler
 *
 * A class that profiles the evaluation of the compositor and tracks information like the
 * evaluation time of every node. */
class Profiler {
 private:
  /* Stores the evaluation time of each node instance keyed by its instance key. Note that
   * pixel-wise nodes like Math nodes will not be measured, that's because they are compiled
   * together with other pixel-wise operations in a single operation, so we can't measure the
   * evaluation time of each individual node. */
  Map<bNodeInstanceKey, timeit::Nanoseconds> nodes_evaluation_times_;

 public:
  /* Returns a reference to the nodes evaluation times. */
  Map<bNodeInstanceKey, timeit::Nanoseconds> &get_nodes_evaluation_times();

  /* Set the evaluation time of the node identified by the given node instance key. */
  void set_node_evaluation_time(bNodeInstanceKey node_instance_key, timeit::Nanoseconds time);
};

}  // namespace blender::compositor
