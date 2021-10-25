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
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_relations_scene.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>  /* required for STREQ later on. */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"

extern "C" {
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_main.h"
#include "BKE_node.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "intern/depsgraph_types.h"

#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphRelationBuilder::build_scene(Main *bmain, Scene *scene)
{
	if (scene->set) {
		build_scene(bmain, scene->set);
	}

	/* scene objects */
	LINKLIST_FOREACH (Base *, base, &scene->base) {
		Object *ob = base->object;
		build_object(bmain, scene, ob);
	}

	/* rigidbody */
	if (scene->rigidbody_world) {
		build_rigidbody(scene);
	}

	/* scene's animation and drivers */
	if (scene->adt) {
		build_animdata(&scene->id);
	}

	/* world */
	if (scene->world) {
		build_world(scene->world);
	}

	/* compo nodes */
	if (scene->nodetree) {
		build_compositor(scene);
	}

	/* grease pencil */
	if (scene->gpd) {
		build_gpencil(scene->gpd);
	}

	/* Masks. */
	LINKLIST_FOREACH (Mask *, mask, &bmain->mask) {
		build_mask(mask);
	}

	/* Movie clips. */
	LINKLIST_FOREACH (MovieClip *, clip, &bmain->movieclip) {
		build_movieclip(clip);
	}

	for (Depsgraph::OperationNodes::const_iterator it_op = m_graph->operations.begin();
	     it_op != m_graph->operations.end();
	     ++it_op)
	{
		OperationDepsNode *node = *it_op;
		IDDepsNode *id_node = node->owner->owner;
		ID *id = id_node->id;
		if (GS(id->name) == ID_OB) {
			Object *object = (Object *)id;
			object->customdata_mask |= node->customdata_mask;
		}
	}
}

}  // namespace DEG
