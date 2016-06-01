/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/build/deg_builder.cc
 *  \ingroup depsgraph
 */

#include "intern/builder/deg_builder.h"

// TODO(sergey): Use own wrapper over STD.
#include <stack>

#include "DNA_anim_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "util/deg_util_foreach.h"

namespace DEG {

string deg_fcurve_id_name(const FCurve *fcu)
{
	char index_buf[32];
	// TODO(sergey): Use int-to-string utility or so.
	BLI_snprintf(index_buf, sizeof(index_buf), "[%d]", fcu->array_index);
	return string(fcu->rna_path) + index_buf;
}

void deg_graph_build_finalize(Depsgraph *graph)
{
	std::stack<OperationDepsNode *> stack;

	foreach (OperationDepsNode *node, graph->operations) {
		IDDepsNode *id_node = node->owner->owner;
		node->done = 0;
		node->num_links_pending = 0;
		foreach (DepsRelation *rel, node->outlinks) {
			if ((rel->from->type == DEPSNODE_TYPE_OPERATION) &&
			    (rel->flag & DEPSREL_FLAG_CYCLIC) == 0)
			{
				++node->num_links_pending;
			}
		}
		if (node->num_links_pending == 0) {
			stack.push(node);
			node->done = 1;
		}
		node->owner->layers = id_node->layers;
		id_node->id->tag |= LIB_TAG_DOIT;
	}

	while (!stack.empty()) {
		OperationDepsNode *node = stack.top();
		stack.pop();
		/* Flush layers to parents. */
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
				OperationDepsNode *from = (OperationDepsNode *)rel->from;
				from->owner->layers |= node->owner->layers;
			}
		}
		/* Schedule parent nodes. */
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
				OperationDepsNode *from = (OperationDepsNode *)rel->from;
				if ((rel->flag & DEPSREL_FLAG_CYCLIC) == 0) {
					BLI_assert(from->num_links_pending > 0);
					--from->num_links_pending;
				}
				if (from->num_links_pending == 0 && from->done == 0) {
					stack.push(from);
					from->done = 1;
				}
			}
		}
	}

	/* Re-tag IDs for update if it was tagged before the relations update tag. */
	GHASH_FOREACH_BEGIN(IDDepsNode *, id_node, graph->id_hash)
	{
		GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp, id_node->components)
		{
			id_node->layers |= comp->layers;
		}
		GHASH_FOREACH_END();

		ID *id = id_node->id;
		if (id->tag & LIB_TAG_ID_RECALC_ALL &&
		    id->tag & LIB_TAG_DOIT)
		{
			id_node->tag_update(graph);
			id->tag &= ~LIB_TAG_DOIT;
		}
		id_node->finalize_build();
	}
	GHASH_FOREACH_END();
}

}  // namespace DEG
