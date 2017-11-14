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

/** \file blender/depsgraph/intern/builder/deg_builder_nodes_layer.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_layer.h"

#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
} /* extern "C" */

#include "intern/builder/deg_builder.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphNodeBuilder::build_layer_collection(
        LayerCollection *layer_collection,
        LayerCollectionState *state)
{
	/* TODO(sergey): This will attempt to create component for each collection.
	 * Harmless but could be optimized.
	 */
	ComponentDepsNode *comp = add_component_node(
	        &scene_->id,
	        DEG_NODE_TYPE_LAYER_COLLECTIONS);

	add_operation_node(comp,
	                   function_bind(BKE_layer_eval_layer_collection,
	                                 _1,
	                                 layer_collection,
	                                 state->parent),
	                   DEG_OPCODE_SCENE_LAYER_EVAL,
	                   layer_collection->scene_collection->name,
	                   state->index);
	++state->index;

	/* Recurs into nested layer collections. */
	LayerCollection *parent = state->parent;
	state->parent = layer_collection;
	build_layer_collections(&layer_collection->layer_collections, state);
	state->parent = parent;
}

void DepsgraphNodeBuilder::build_layer_collections(ListBase *layer_collections,
                                                   LayerCollectionState *state)
{
	LINKLIST_FOREACH (LayerCollection *, layer_collection, layer_collections) {
		build_layer_collection(layer_collection, state);
	}
}

void DepsgraphNodeBuilder::build_scene_layer_collections(
        SceneLayer *scene_layer)
{
	Scene *scene_cow;
	SceneLayer *scene_layer_cow;
	if (DEG_depsgraph_use_copy_on_write()) {
		/* Make sure we've got ID node, so we can get pointer to CoW datablock.
		 */
		scene_cow = expand_cow_datablock(scene_);
		scene_layer_cow = (SceneLayer *)BLI_findstring(
		        &scene_cow->render_layers,
		        scene_layer->name,
		        offsetof(SceneLayer, name));
	}
	else {
		scene_cow = scene_;
		scene_layer_cow = scene_layer;
	}

	LayerCollectionState state;
	state.index = 0;
	ComponentDepsNode *comp = add_component_node(
	        &scene_->id,
	        DEG_NODE_TYPE_LAYER_COLLECTIONS);
	add_operation_node(comp,
	                   function_bind(BKE_layer_eval_layer_collection_pre,
	                                 _1,
	                                 scene_cow,
	                                 scene_layer_cow),
	                   DEG_OPCODE_SCENE_LAYER_INIT,
	                   scene_layer->name);
	add_operation_node(comp,
	                   function_bind(BKE_layer_eval_layer_collection_post,
	                                 _1,
	                                 scene_layer_cow),
	                   DEG_OPCODE_SCENE_LAYER_DONE,
	                   scene_layer->name);
	state.parent = NULL;
	build_layer_collections(&scene_layer_cow->layer_collections, &state);
}

}  // namespace DEG
