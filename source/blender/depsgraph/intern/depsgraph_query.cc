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
#include "BKE_idcode.h"
#include "BKE_layer.h"
#include "BKE_main.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_query.h"
} /* extern "C" */

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

bool DEG_id_type_tagged(Main *bmain, short idtype)
{
	return bmain->id_tag_update[BKE_idcode_to_index(idtype)] != 0;
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

Scene *DAG_get_scene(Depsgraph *graph)
{
	Main *bmain = G.main;
	LINKLIST_FOREACH (Scene*, scene, &bmain->scene) {
		if (scene->depsgraph == graph) {
			/* Got the scene! */
			return scene;
		}
	}
	return NULL;
}

SceneLayer *DAG_get_scene_layer(Depsgraph *graph)
{
	Scene *scene = DAG_get_scene(graph);
	if (scene) {
		return BKE_scene_layer_context_active(scene);
	}
	return NULL;
}

Object *DAG_get_object(Depsgraph * /*depsgraph*/, Object *ob)
{
	/* XXX TODO */
	return ob;
}

/* ************************ DAG ITERATORS ********************* */

typedef struct DAGObjectsIteratorData {
	Depsgraph *graph;
	Scene *scene;
	SceneLayer *scene_layer;
	Base *base;
	int flag;
} DAGObjectsIteratorData;

void DAG_objects_iterator_begin(Iterator *iter, void *data_in)
{
	SceneLayer *scene_layer;
	Depsgraph *graph = (Depsgraph *) data_in;
	DAGObjectsIteratorData *data = (DAGObjectsIteratorData *)
	                               MEM_callocN(sizeof(DAGObjectsIteratorData), __func__);
	iter->data = data;
	iter->valid = true;

	data->graph = graph;
	data->scene = DAG_get_scene(graph);
	scene_layer = DAG_get_scene_layer(graph);
	data->flag = ~(BASE_FROM_SET);

	Base base = {(Base *)scene_layer->object_bases.first, NULL};
	data->base = &base;
	DAG_objects_iterator_next(iter);
}

void DAG_objects_iterator_next(Iterator *iter)
{
	DAGObjectsIteratorData *data = (DAGObjectsIteratorData *)iter->data;
	Base *base = data->base->next;

	while (base) {
		if ((base->flag & BASE_VISIBLED) != 0) {
			Object *ob = DAG_get_object(data->graph, base->object);
			iter->current = ob;

			/* Flushing depsgraph data. */
			ob->base_flag = (base->flag | BASE_FROM_SET) & data->flag;
			ob->base_collection_properties = base->collection_properties;
			data->base = base;
			return;
		}
		base = base->next;
	}

	/* Look for an object in the next set. */
	if (data->scene->set) {
		SceneLayer *scene_layer;
		data->scene = data->scene->set;
		data->flag = ~(BASE_SELECTED | BASE_SELECTABLED);

		/* For the sets we use the layer used for rendering. */
		scene_layer = BKE_scene_layer_render_active(data->scene);

		Base base = {(Base *)scene_layer->object_bases.first, NULL};
		data->base = &base;
		DAG_objects_iterator_next(iter);
		return;
	}

	iter->current = NULL;
	iter->valid = false;
}

void DAG_objects_iterator_end(Iterator *iter)
{
	DAGObjectsIteratorData *data = (DAGObjectsIteratorData *)iter->data;
	if (data) {
		MEM_freeN(data);
	}
}
