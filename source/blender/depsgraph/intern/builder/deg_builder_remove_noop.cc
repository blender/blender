/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_remove_noop.h"

#include "MEM_guardedalloc.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_operation.h"

#include "intern/debug/deg_debug.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_type.h"

namespace blender::deg {

static inline bool is_unused_noop(OperationNode *op_node)
{
  if (op_node == nullptr) {
    return false;
  }
  if (op_node->flag & OperationFlag::DEPSOP_FLAG_PINNED) {
    return false;
  }
  return op_node->is_noop() && op_node->outlinks.is_empty();
}

static inline bool is_removable_relation(const Relation *relation)
{
  if (relation->from->type != NodeType::OPERATION || relation->to->type != NodeType::OPERATION) {
    return true;
  }

  const OperationNode *operation_from = static_cast<OperationNode *>(relation->from);
  const OperationNode *operation_to = static_cast<OperationNode *>(relation->to);

  /* If the relation connects two different IDs there is a high risk that the removal of the
   * relation will make it so visibility flushing is not possible at runtime. This happens with
   * relations like the DoF on camera of custom shape on bones: such relation do not lead to an
   * actual depsgraph evaluation operation as they are handled on render engine level.
   *
   * The indirectly linked objects could have some of their components invisible as well, so
   * also keep relations which connect different components of the same object so that visibility
   * tracking happens correct in those cases as well. */
  return operation_from->owner == operation_to->owner;
}

void deg_graph_remove_unused_noops(Depsgraph *graph)
{
  deque<OperationNode *> queue;

  for (OperationNode *node : graph->operations) {
    if (is_unused_noop(node)) {
      queue.push_back(node);
    }
  }

  Vector<Relation *> relations_to_remove;

  while (!queue.empty()) {
    OperationNode *to_remove = queue.front();
    queue.pop_front();

    for (Relation *rel_in : to_remove->inlinks) {
      if (!is_removable_relation(rel_in)) {
        continue;
      }

      Node *dependency = rel_in->from;
      relations_to_remove.append(rel_in);

      /* Queue parent no-op node that has now become unused. */
      OperationNode *operation = dependency->get_exit_operation();
      if (is_unused_noop(operation)) {
        queue.push_back(operation);
      }
    }

    /* TODO(Sybren): Remove the node itself. */
  }

  /* Remove the relations. */
  for (Relation *relation : relations_to_remove) {
    relation->unlink();
    delete relation;
  }

  DEG_DEBUG_PRINTF((::Depsgraph *)graph,
                   BUILD,
                   "Removed %d relations to no-op nodes\n",
                   int(relations_to_remove.size()));
}

}  // namespace blender::deg
