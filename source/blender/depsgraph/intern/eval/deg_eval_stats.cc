/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2017 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_stats.h"

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace blender::deg {

void deg_eval_stats_aggregate(Depsgraph *graph)
{
  /* Reset current evaluation stats for ID and component nodes.
   * Those are not filled in by the evaluation engine. */
  for (Node *node : graph->id_nodes) {
    IDNode *id_node = (IDNode *)node;
    for (ComponentNode *comp_node : id_node->components.values()) {
      comp_node->stats.reset_current();
    }
    id_node->stats.reset_current();
  }
  /* Now accumulate operation timings to components and IDs. */
  for (OperationNode *op_node : graph->operations) {
    ComponentNode *comp_node = op_node->owner;
    IDNode *id_node = comp_node->owner;
    id_node->stats.current_time += op_node->stats.current_time;
    comp_node->stats.current_time += op_node->stats.current_time;
  }
}

}  // namespace blender::deg
