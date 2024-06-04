/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_timeit.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"

namespace blender::realtime_compositor {

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

  /* Finalize profiling by computing node group times. This should be called after evaluation. */
  void finalize(const bNodeTree &node_tree);

 private:
  /* Computes the evaluation time of every group node inside the given tree recursively by
   * accumulating the evaluation time of its nodes, setting the computed time to the group nodes.
   * The time is returned since the method is called recursively. */
  timeit::Nanoseconds accumulate_node_group_times(const bNodeTree &node_tree,
                                                  bNodeInstanceKey instance_key);
};

}  // namespace blender::realtime_compositor
