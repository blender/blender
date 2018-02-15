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

/** \file blender/depsgraph/intern/builder/deg_builder_nodes_scene.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_string.h"

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
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphNodeBuilder::build_scene(Scene *scene)
{
	/* Scene ID block. */
	add_id_node(&scene->id);
	/* timesource */
	add_time_source();
	/* build subgraph for set, and link this in... */
	// XXX: depending on how this goes, that scene itself could probably store its
	//      own little partial depsgraph?
	if (scene->set != NULL) {
		build_scene(scene->set);
	}
	/* Setup currently building context. */
	scene_ = scene;
	/* scene objects */
	LISTBASE_FOREACH (Base *, base, &scene->base) {
		Object *object = base->object;
		build_object(base, object);
	}
	/* Rigidbody. */
	if (scene->rigidbody_world != NULL) {
		build_rigidbody(scene);
	}
	/* Scene's animation and drivers. */
	if (scene->adt != NULL) {
		build_animdata(&scene->id);
	}
	/* World. */
	if (scene->world != NULL) {
		build_world(scene->world);
	}
	/* Compositor nodes. */
	if (scene->nodetree != NULL) {
		build_compositor(scene);
	}
	/* Grease pencil. */
	if (scene->gpd != NULL) {
		build_gpencil(scene->gpd);
	}
	/* Cache file. */
	LISTBASE_FOREACH (CacheFile *, cachefile, &bmain_->cachefiles) {
		build_cachefile(cachefile);
	}
	/* Masks. */
	LISTBASE_FOREACH (Mask *, mask, &bmain_->mask) {
		build_mask(mask);
	}
	/* Movie clips. */
	LISTBASE_FOREACH (MovieClip *, clip, &bmain_->movieclip) {
		build_movieclip(clip);
	}
	/* Parameters evaluation for scene relations mainly. */
	add_operation_node(&scene->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER,
	                   "Scene Eval");
}

}  // namespace DEG
