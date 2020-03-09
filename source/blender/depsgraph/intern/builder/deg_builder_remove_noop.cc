/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_remove_noop.h"

#include "MEM_guardedalloc.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_type.h"
#include "intern/depsgraph_relation.h"
#include "intern/debug/deg_debug.h"

namespace DEG {

static inline bool is_unused_noop(OperationNode *op_node)
{
  if (op_node == nullptr) {
    return false;
  }
  if (op_node->flag & OperationFlag::DEPSOP_FLAG_PINNED) {
    return false;
  }
  return op_node->is_noop() && op_node->outlinks.empty();
}

void deg_graph_remove_unused_noops(Depsgraph *graph)
{
  int num_removed_relations = 0;
  deque<OperationNode *> queue;

  for (OperationNode *node : graph->operations) {
    if (is_unused_noop(node)) {
      queue.push_back(node);
    }
  }

  while (!queue.empty()) {
    OperationNode *to_remove = queue.front();
    queue.pop_front();

    while (!to_remove->inlinks.empty()) {
      Relation *rel_in = to_remove->inlinks[0];
      Node *dependency = rel_in->from;

      /* Remove the relation. */
      rel_in->unlink();
      OBJECT_GUARDED_DELETE(rel_in, Relation);
      num_removed_relations++;

      /* Queue parent no-op node that has now become unused. */
      OperationNode *operation = dependency->get_exit_operation();
      if (is_unused_noop(operation)) {
        queue.push_back(operation);
      }
    }

    /* TODO(Sybren): Remove the node itself. */
  }

  DEG_DEBUG_PRINTF(
      (::Depsgraph *)graph, BUILD, "Removed %d relations to no-op nodes\n", num_removed_relations);
}

}  // namespace DEG
