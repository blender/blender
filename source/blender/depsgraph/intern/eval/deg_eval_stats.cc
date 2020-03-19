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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/eval/deg_eval_stats.h"

#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "intern/depsgraph.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace DEG {

void deg_eval_stats_aggregate(Depsgraph *graph)
{
  /* Reset current evaluation stats for ID and component nodes.
   * Those are not filled in by the evaluation engine. */
  for (Node *node : graph->id_nodes) {
    IDNode *id_node = (IDNode *)node;
    GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
      comp_node->stats.reset_current();
    }
    GHASH_FOREACH_END();
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

}  // namespace DEG
