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

/** \file blender/depsgraph/intern/builder/deg_builder.cc
 *  \ingroup depsgraph
 */

#include "intern/builder/deg_builder.h"

// TODO(sergey): Use own wrapper over STD.
#include <stack>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_ID.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "util/deg_util_foreach.h"

#include <cstdio>

namespace DEG {

void deg_graph_build_finalize(Depsgraph *graph)
{
	/* Re-tag IDs for update if it was tagged before the relations
	 * update tag.
	 */
	GHASH_FOREACH_BEGIN(IDDepsNode *, id_node, graph->id_hash)
	{
		ID *id = id_node->id;
		if ((id->tag & LIB_TAG_ID_RECALC_ALL)) {
			id_node->tag_update(graph);
		}
		else if (GS(id->name) == ID_OB) {
			Object *object = (Object *)id;
			if (object->recalc & OB_RECALC_ALL) {
				id_node->tag_update(graph);
			}
		}
		id_node->finalize_build();
	}
	GHASH_FOREACH_END();
}

}  // namespace DEG
