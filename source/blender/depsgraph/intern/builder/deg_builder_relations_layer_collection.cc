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

/** \file blender/depsgraph/intern/builder/deg_builder_relations_layer_collection.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring>  /* required for STREQ later on. */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
} /* extern "C" */

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "intern/depsgraph_types.h"

#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphRelationBuilder::build_layer_collection(
        ID *owner_id,
        LayerCollection *layer_collection,
        LayerCollectionState *state)
{
	OperationKey layer_key(owner_id,
	                       DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                       DEG_OPCODE_VIEW_LAYER_EVAL,
	                       layer_collection->scene_collection->name,
	                       state->index);
	add_relation(state->prev_key, layer_key, "Layer collection order");

	++state->index;
	state->prev_key = layer_key;

	/* Recurs into nested layer collections. */
	build_layer_collections(owner_id, &layer_collection->layer_collections, state);
}

void DepsgraphRelationBuilder::build_layer_collections(
        ID *owner_id,
        ListBase *layer_collections,
        LayerCollectionState *state)
{
	LISTBASE_FOREACH (LayerCollection *, layer_collection, layer_collections) {
		/* Recurs into the layer. */
		build_layer_collection(owner_id, layer_collection, state);
	}
}

void DepsgraphRelationBuilder::build_view_layer_collections(
        ID *owner_id,
        ViewLayer *view_layer)
{
	LayerCollectionState state;
	state.index = 0;

	OperationKey init_key(owner_id,
	                      DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                      DEG_OPCODE_VIEW_LAYER_INIT);
	OperationKey done_key(owner_id,
	                      DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                      DEG_OPCODE_VIEW_LAYER_DONE);

	state.init_key = init_key;
	state.done_key = done_key;
	state.prev_key = init_key;

	build_layer_collections(owner_id, &view_layer->layer_collections, &state);

	add_relation(state.prev_key, done_key, "Layer collection order");
}

}  // namespace DEG
