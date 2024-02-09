/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_map.hh"
#include "BLI_timeit.hh"

#include "DNA_node_types.h"

struct bNodeTree;

namespace blender::compositor {

class NodeOperation;

/* Profiling ddata gathered during execution of a compositing node tree. */
class ProfilerData {
 public:
  /* Per-node accumulated execution time. Includes execution time of all operations the node was
   * broken down into. */
  Map<bNodeInstanceKey, timeit::Nanoseconds> per_node_execution_time;
};

/* Profiler implementation which is used by the node execution system. */
class Profiler {
  /* Local copy of the profiling data, which is known to not cause threading conflicts with the
   * interface thread while the compositing tree is evaluated in the background. */
  ProfilerData data_;

 public:
  void add_operation_execution_time(const NodeOperation &operation,
                                    const timeit::TimePoint &start,
                                    const timeit::TimePoint &end);

  void finalize(const bNodeTree &node_tree);

  const ProfilerData &get_data() const
  {
    return data_;
  }

 private:
  /* Add execution time to the node denoted by its key. */
  void add_execution_time(const bNodeInstanceKey parent_key,
                          const timeit::Nanoseconds &execution_time);

  /* Accumulate execution time of the group node instances, and store their execution time in the
   * per_node_execution_time_.
   * Returns total execution time of the given node tree. */
  timeit::Nanoseconds accumulate_node_group_times(
      const bNodeTree &node_tree, const bNodeInstanceKey parent_key = NODE_INSTANCE_KEY_BASE);
};

}  // namespace blender::compositor