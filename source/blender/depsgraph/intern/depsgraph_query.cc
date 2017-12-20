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

#include "intern/depsgraph_intern.h"
#include "intern/nodes/deg_node_id.h"

bool DEG_id_type_tagged(Main *bmain, short id_type)
{
	return bmain->id_tag_update[BKE_idcode_to_index(id_type)] != 0;
}

short DEG_get_eval_flags_for_id(Depsgraph *graph, ID *id)
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

	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);

	DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		/* TODO(sergey): Does it mean we need to check set scene? */
		return 0;
	}

	return id_node->eval_flags;
}

Scene *DEG_get_evaluated_scene(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	Scene *scene_orig = deg_graph->scene;
	return reinterpret_cast<Scene *>(deg_graph->get_cow_id(&scene_orig->id));
}

ViewLayer *DEG_get_evaluated_view_layer(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	Scene *scene_cow = DEG_get_evaluated_scene(graph);
	ViewLayer *view_layer_orig = deg_graph->view_layer;
	ViewLayer *view_layer_cow =
	        (ViewLayer *)BLI_findstring(&scene_cow->view_layers,
	                                     view_layer_orig->name,
	                                     offsetof(ViewLayer, name));
	return view_layer_cow;
}

Object *DEG_get_evaluated_object(Depsgraph *depsgraph, Object *object)
{
	return (Object *)DEG_get_evaluated_id(depsgraph, &object->id);
}

ID *DEG_get_evaluated_id(struct Depsgraph *depsgraph, ID *id)
{
	/* TODO(sergey): This is a duplicate of Depsgraph::get_cow_id(),
	 * but here we never do assert, since we don't know nature of the
	 * incoming ID datablock.
	 */
	DEG::Depsgraph *deg_graph = (DEG::Depsgraph *)depsgraph;
	DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		return id;
	}
	return id_node->id_cow;
}

