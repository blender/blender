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
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_layer.h"
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

void DepsgraphNodeBuilder::build_view_layer(
        Scene *scene,
        ViewLayer *view_layer,
        eDepsNode_LinkedState_Type linked_state)
{
	/* Scene ID block. */
	add_id_node(&scene->id);
	/* Time source. */
	add_time_source();
	/* Setup currently building context. */
	scene_ = scene;
	/* Expand Scene Cow datablock to get proper pointers to bases. */
	Scene *scene_cow;
	ViewLayer *view_layer_cow;
	if (DEG_depsgraph_use_copy_on_write()) {
		/* NOTE: We need to create ID nodes for all objects coming from bases,
		 * otherwise remapping will not replace objects with their CoW versions
		 * for CoW bases.
		 */
		LINKLIST_FOREACH(Base *, base, &view_layer->object_bases) {
			Object *object = base->object;
			add_id_node(&object->id, false);
		}
		/* Create ID node for nested ID of nodetree as well, otherwise remapping
		 * will not work correct either.
		 */
		if (scene->nodetree != NULL) {
			add_id_node(&scene->nodetree->id, false);
		}
		/* Make sure we've got ID node, so we can get pointer to CoW datablock.
		 */
		scene_cow = expand_cow_datablock(scene);
		view_layer_cow = (ViewLayer *)BLI_findstring(
		        &scene_cow->view_layers,
		        view_layer->name,
		        offsetof(ViewLayer, name));
	}
	else {
		scene_cow = scene;
		view_layer_cow = view_layer;
	}
	/* Scene objects. */
	int select_color = 1;
	/* NOTE: Base is used for function bindings as-is, so need to pass CoW base,
	 * but object is expected to be an original one. Hence we go into some
	 * tricks here iterating over the view layer.
	 */
	for (Base *base_orig = (Base *)view_layer->object_bases.first,
	          *base_cow = (Base *)view_layer_cow->object_bases.first;
	     base_orig != NULL;
	     base_orig = base_orig->next, base_cow = base_cow->next)
	{
		/* object itself */
		build_object(base_cow, base_orig->object, linked_state);
		base_orig->object->select_color = select_color++;
	}
	if (scene->camera != NULL) {
		build_object(NULL, scene->camera, DEG_ID_LINKED_INDIRECTLY);
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
	/* Compositor nodes */
	if (scene->nodetree != NULL) {
		build_compositor(scene);
	}
	/* Grease pencil. */
	if (scene->gpd != NULL) {
		build_gpencil(scene->gpd);
	}
	/* Cache file. */
	LINKLIST_FOREACH (CacheFile *, cachefile, &bmain_->cachefiles) {
		build_cachefile(cachefile);
	}
	/* Masks. */
	LINKLIST_FOREACH (Mask *, mask, &bmain_->mask) {
		build_mask(mask);
	}
	/* Movie clips. */
	LINKLIST_FOREACH (MovieClip *, clip, &bmain_->movieclip) {
		build_movieclip(clip);
	}
	/* Collections. */
	build_view_layer_collections(&scene_cow->id, view_layer_cow);
	/* Parameters evaluation for scene relations mainly. */
	add_operation_node(&scene->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER,
	                   "Scene Eval");
	/* Build all set scenes. */
	if (scene->set != NULL) {
		ViewLayer *set_view_layer = BKE_view_layer_from_scene_get(scene->set);
		build_view_layer(scene->set, set_view_layer, DEG_ID_LINKED_VIA_SET);
	}
}

}  // namespace DEG
