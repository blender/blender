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

/** \file blender/depsgraph/intern/builder/deg_builder_nodes_view_layer.cc
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

void DepsgraphNodeBuilder::build_layer_collections(ListBase *lb)
{
	const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ?
		COLLECTION_RESTRICT_VIEW : COLLECTION_RESTRICT_RENDER;

	for (LayerCollection *lc = (LayerCollection *)lb->first; lc; lc = lc->next) {
		if (lc->collection->flag & restrict_flag) {
			continue;
		}
		if ((lc->flag & LAYER_COLLECTION_EXCLUDE) == 0) {
			build_collection(DEG_COLLECTION_OWNER_SCENE, lc->collection);
		}
		build_layer_collections(&lc->layer_collections);
	}
}

void DepsgraphNodeBuilder::build_view_layer(
        Scene *scene,
        ViewLayer *view_layer,
        eDepsNode_LinkedState_Type linked_state)
{
	view_layer_index_ = BLI_findindex(&scene->view_layers, view_layer);
	BLI_assert(view_layer_index_ != -1);
	/* Scene ID block. */
	add_id_node(&scene->id);
	/* Time source. */
	add_time_source();
	/* Setup currently building context. */
	scene_ = scene;
	view_layer_ = view_layer;
	/* Get pointer to a CoW version of scene ID. */
	Scene *scene_cow = get_cow_datablock(scene);
	/* Scene objects. */
	int select_color = 1;
	/* NOTE: Base is used for function bindings as-is, so need to pass CoW base,
	 * but object is expected to be an original one. Hence we go into some
	 * tricks here iterating over the view layer.
	 */
	int base_index = 0;
	const int base_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ?
		BASE_ENABLED_VIEWPORT : BASE_ENABLED_RENDER;
	LISTBASE_FOREACH(Base *, base, &view_layer->object_bases) {
		/* object itself */
		if (base->flag & base_flag) {
			build_object(base_index, base->object, linked_state);
			base->object->select_color = select_color++;
		}
		++base_index;
	}
	build_layer_collections(&view_layer->layer_collections);
	if (scene->camera != NULL) {
		build_object(-1, scene->camera, DEG_ID_LINKED_INDIRECTLY);
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
	/* Collections. */
	add_operation_node(&scene->id,
	                   DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                   function_bind(BKE_layer_eval_view_layer_indexed,
	                                 _1,
	                                 scene_cow,
	                                 view_layer_index_),
	                   DEG_OPCODE_VIEW_LAYER_EVAL);
	/* Parameters evaluation for scene relations mainly. */
	add_operation_node(&scene->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER,
	                   "Scene Eval");
	/* Build all set scenes. */
	if (scene->set != NULL) {
		ViewLayer *set_view_layer = BKE_view_layer_default_render(scene->set);
		build_view_layer(scene->set, set_view_layer, DEG_ID_LINKED_VIA_SET);
	}
}

}  // namespace DEG
