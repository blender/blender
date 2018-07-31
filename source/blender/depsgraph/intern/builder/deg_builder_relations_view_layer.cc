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

/** \file blender/depsgraph/intern/builder/deg_builder_relations_view_layer.cc
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

#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_node.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "intern/depsgraph_types.h"

#include "util/deg_util_foreach.h"

namespace DEG {

void DepsgraphRelationBuilder::build_layer_collections(ListBase *lb)
{
	const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ?
		COLLECTION_RESTRICT_VIEW : COLLECTION_RESTRICT_RENDER;

	for (LayerCollection *lc = (LayerCollection *)lb->first; lc; lc = lc->next) {
		if ((lc->collection->flag & restrict_flag)) {
			continue;
		}
		if ((lc->flag & LAYER_COLLECTION_EXCLUDE) == 0) {
			build_collection(DEG_COLLECTION_OWNER_SCENE, NULL, lc->collection);
		}
		build_layer_collections(&lc->layer_collections);
	}
}

void DepsgraphRelationBuilder::build_view_layer(Scene *scene, ViewLayer *view_layer)
{
	/* Setup currently building context. */
	scene_ = scene;
	/* Scene objects. */
	/* NOTE: Nodes builder requires us to pass CoW base because it's being
	 * passed to the evaluation functions. During relations builder we only
	 * do NULL-pointer check of the base, so it's fine to pass original one.
	 */
	const int base_flag = (graph_->mode == DAG_EVAL_VIEWPORT) ?
		BASE_ENABLED_VIEWPORT : BASE_ENABLED_RENDER;
	LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
		if (base->flag & base_flag) {
			build_object(base, base->object);
		}
	}

	build_layer_collections(&view_layer->layer_collections);

	if (scene->camera != NULL) {
		build_object(NULL, scene->camera);
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
	/* Masks. */
	LISTBASE_FOREACH (Mask *, mask, &bmain_->mask) {
		build_mask(mask);
	}
	/* Movie clips. */
	LISTBASE_FOREACH (MovieClip *, clip, &bmain_->movieclip) {
		build_movieclip(clip);
	}
	/* TODO(sergey): Do this flush on CoW object? */
	foreach (OperationDepsNode *node, graph_->operations) {
		IDDepsNode *id_node = node->owner->owner;
		ID *id = id_node->id_orig;
		if (GS(id->name) == ID_OB) {
			Object *object = (Object *)id;
			object->customdata_mask |= node->customdata_mask;
		}
	}
	/* Build all set scenes. */
	if (scene->set != NULL) {
		ViewLayer *set_view_layer = BKE_view_layer_default_render(scene->set);
		build_view_layer(scene->set, set_view_layer);
	}
}

}  // namespace DEG
