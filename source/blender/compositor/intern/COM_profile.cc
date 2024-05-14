/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_profile.hh"

#include "BKE_node_runtime.hh"

#include "COM_NodeOperation.h"

#include "DNA_node_types.h"

namespace blender::compositor {

void Profiler::add_operation_execution_time(const NodeOperation &operation,
                                            const timeit::TimePoint &start,
                                            const timeit::TimePoint &end)
{
  const timeit::Nanoseconds execution_time = end - start;

  const bNodeInstanceKey key = operation.get_node_instance_key();
  if (key.value == bke::NODE_INSTANCE_KEY_NONE.value) {
    /* The operation does not come from any node. It was, for example, added to convert data type.
     * Do not accumulate time from its execution. */
    return;
  }

  this->add_execution_time(key, execution_time);
}

void Profiler::add_execution_time(const bNodeInstanceKey key,
                                  const timeit::Nanoseconds &execution_time)
{
  data_.per_node_execution_time.lookup_or_add(key, timeit::Nanoseconds(0)) += execution_time;
}

void Profiler::finalize(const bNodeTree &node_tree)
{
  this->accumulate_node_group_times(node_tree);
}

timeit::Nanoseconds Profiler::accumulate_node_group_times(const bNodeTree &node_tree,
                                                          const bNodeInstanceKey parent_key)
{
  timeit::Nanoseconds tree_execution_time(0);

  for (const bNode *node : node_tree.all_nodes()) {
    const bNodeInstanceKey key = bke::BKE_node_instance_key(parent_key, &node_tree, node);

    if (node->type != NODE_GROUP) {
      /* Non-group node, no need to recurse into. Simply accumulate the node's execution time to
       * the current tree's execution time. */
      tree_execution_time += data_.per_node_execution_time.lookup_default(key,
                                                                          timeit::Nanoseconds(0));
      continue;
    }

    if (node->id == nullptr) {
      /* Node group has lost link to its node tree. For example, due to missing linked file. */
      continue;
    }

    const timeit::Nanoseconds group_execution_time = this->accumulate_node_group_times(
        *reinterpret_cast<const bNodeTree *>(node->id), key);

    /* Store execution time of the group node. */
    this->add_execution_time(key, group_execution_time);

    /* Add group execution time to the overall tree execution time. */
    tree_execution_time += group_execution_time;
  }

  return tree_execution_time;
}

}  // namespace blender::compositor
