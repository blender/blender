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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/nodes/deg_node_time.cc
 *  \ingroup depsgraph
 */

#include "intern/nodes/deg_node_time.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

#include "DNA_scene_types.h"

namespace DEG {

void TimeSourceDepsNode::tag_update(Depsgraph *graph,
                                    eDepsTag_Source /*source*/)
{
	foreach (DepsRelation *rel, outlinks) {
		DepsNode *node = rel->to;
		node->tag_update(graph, DEG_UPDATE_SOURCE_TIME);
	}
}

}  // namespace DEG
