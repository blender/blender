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

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_ID.h"

extern "C" {
#include "BKE_animsys.h"
}

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_id.h"

#include "util/deg_util_foreach.h"

#include "DEG_depsgraph.h"

namespace DEG {

void deg_graph_build_finalize(Main *bmain, Depsgraph *graph)
{
	/* Re-tag IDs for update if it was tagged before the relations
	 * update tag.
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		ID *id = id_node->id_orig;
		id_node->finalize_build(graph);
		int flag = 0;
		if ((id->recalc & ID_RECALC_ALL)) {
			AnimData *adt = BKE_animdata_from_id(id);
			if (adt != NULL && (adt->recalc & ADT_RECALC_ANIM) != 0) {
				flag |= DEG_TAG_TIME;
			}
		}
		if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
			flag |= DEG_TAG_COPY_ON_WRITE;
			/* This means ID is being added to the dependency graph first
			 * time, which is similar to "ob-visible-change"
			 */
			if (GS(id->name) == ID_OB) {
				flag |= OB_RECALC_OB | OB_RECALC_DATA;
			}
		}
		if (flag != 0) {
			DEG_graph_id_tag_update(bmain,
			                        (::Depsgraph *)graph,
			                        id_node->id_orig,
			                        flag);
		}
	}
}

}  // namespace DEG
