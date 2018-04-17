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

/** \file blender/depsgraph/intern/depsgraph_query.cc
 *  \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BKE_idcode.h"
#include "BKE_main.h"
#include "BLI_listbase.h"
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/depsgraph_intern.h"
#include "intern/nodes/deg_node_id.h"

struct Scene *DEG_get_input_scene(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	return deg_graph->scene;
}

struct ViewLayer *DEG_get_input_view_layer(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	return deg_graph->view_layer;
}

eEvaluationMode DEG_get_mode(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	return deg_graph->mode;
}

float DEG_get_ctime(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	return deg_graph->ctime;
}


bool DEG_id_type_tagged(Main *bmain, short id_type)
{
	return bmain->id_tag_update[BKE_idcode_to_index(id_type)] != 0;
}

short DEG_get_eval_flags_for_id(const Depsgraph *graph, ID *id)
{
	if (graph == NULL) {
		/* Happens when converting objects to mesh from a python script
		 * after modifying scene graph.
		 *
		 * Currently harmless because it's only called for temporary
		 * objects which are out of the DAG anyway.
		 */
		return 0;
	}

	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	const DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		/* TODO(sergey): Does it mean we need to check set scene? */
		return 0;
	}

	return id_node->eval_flags;
}

Scene *DEG_get_evaluated_scene(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph =
	        reinterpret_cast<const DEG::Depsgraph *>(graph);
	Scene *scene_orig = deg_graph->scene;
	Scene *scene_cow =
	        reinterpret_cast<Scene *>(deg_graph->get_cow_id(&scene_orig->id));
	/* TODO(sergey): Shall we expand datablock here? Or is it OK to assume
	 * that calleer is OK with just a pointer in case scene is not up[dated
	 * yet?
	 */
	return scene_cow;
}

ViewLayer *DEG_get_evaluated_view_layer(const Depsgraph *graph)
{
	const DEG::Depsgraph *deg_graph =
	        reinterpret_cast<const DEG::Depsgraph *>(graph);
	Scene *scene_cow = DEG_get_evaluated_scene(graph);
	/* We update copy-on-write scene in the following cases:
	 * - It was not expanded yet.
	 * - It was tagged for update of CoW component.
	 * This allows us to have proper view layer pointer.
	 */
	if (DEG_depsgraph_use_copy_on_write() &&
	    (!DEG::deg_copy_on_write_is_expanded(&scene_cow->id) ||
	     scene_cow->id.recalc & ID_RECALC_COPY_ON_WRITE))
	{
		const DEG::IDDepsNode *id_node =
		        deg_graph->find_id_node(&deg_graph->scene->id);
		DEG::deg_update_copy_on_write_datablock(deg_graph, id_node);
	}
	/* Do name-based lookup. */
	/* TODO(sergey): Can this be optimized? */
	ViewLayer *view_layer_orig = deg_graph->view_layer;
	ViewLayer *view_layer_cow =
	        (ViewLayer *)BLI_findstring(&scene_cow->view_layers,
	                                     view_layer_orig->name,
	                                     offsetof(ViewLayer, name));
	BLI_assert(view_layer_cow != NULL);
	return view_layer_cow;
}

Object *DEG_get_evaluated_object(const Depsgraph *depsgraph, Object *object)
{
	return (Object *)DEG_get_evaluated_id(depsgraph, &object->id);
}

ID *DEG_get_evaluated_id(const Depsgraph *depsgraph, ID *id)
{
	if (id == NULL) {
		return NULL;
	}
	/* TODO(sergey): This is a duplicate of Depsgraph::get_cow_id(),
	 * but here we never do assert, since we don't know nature of the
	 * incoming ID datablock.
	 */
	const DEG::Depsgraph *deg_graph = (const DEG::Depsgraph *)depsgraph;
	const DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		return id;
	}
	return id_node->id_cow;
}

