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
#include "BLI_math.h"
#include "BKE_anim.h"
#include "BKE_idcode.h"
#include "BKE_layer.h"
#include "BKE_main.h"
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

#ifndef NDEBUG
#  include "intern/eval/deg_eval_copy_on_write.h"
#endif

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

Scene *DEG_get_scene(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	Scene *scene_orig = deg_graph->scene;
	return reinterpret_cast<Scene *>(deg_graph->get_cow_id(&scene_orig->id));
}

SceneLayer *DEG_get_scene_layer(Depsgraph *graph)
{
	Scene *scene = DEG_get_scene(graph);
	if (scene) {
		return BKE_scene_layer_render_active(scene);
	}
	return NULL;
}

Object *DEG_get_object(Depsgraph *depsgraph, Object *ob)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
	return (Object *)deg_graph->get_cow_id(&ob->id);
}

ID *DEG_get_evaluated_id(struct Depsgraph *depsgraph, ID *id)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
	return deg_graph->get_cow_id(id);
}

/* ************************ DAG ITERATORS ********************* */

#define BASE_FLUSH_FLAGS (BASE_FROM_SET | BASE_FROMDUPLI)

void DEG_objects_iterator_begin(BLI_Iterator *iter, DEGObjectsIteratorData *data)
{
	Depsgraph *graph = data->graph;
	SceneLayer *scene_layer = DEG_get_scene_layer(graph);

	iter->data = data;
	iter->valid = true;

	data->scene = DEG_get_scene(graph);
	DEG_evaluation_context_init(&data->eval_ctx, DAG_EVAL_RENDER);

	/* TODO(sergey): It's really confusing to store pointer to a local data. */
	Base base = {(Base *)scene_layer->object_bases.first, NULL};
	data->base = &base;

	data->base_flag = ~(BASE_FLUSH_FLAGS);

	data->dupli_parent = NULL;
	data->dupli_list = NULL;
	data->dupli_object_next = NULL;
	data->dupli_object_current = NULL;

	DEG_objects_iterator_next(iter);
}

/**
 * Temporary function to flush depsgraph until we get copy on write (CoW)
 */
static void deg_flush_base_flags_and_settings(Object *ob, Base *base, const int flag)
{
	ob->base_flag = (base->flag | BASE_FLUSH_FLAGS) & flag;
	ob->base_collection_properties = base->collection_properties;
}

static bool deg_objects_dupli_iterator_next(BLI_Iterator *iter)
{
	DEGObjectsIteratorData *data = (DEGObjectsIteratorData *)iter->data;
	while (data->dupli_object_next != NULL) {
		DupliObject *dob = data->dupli_object_next;
		Object *obd = dob->ob;

		data->dupli_object_next = data->dupli_object_next->next;

		/* Group duplis need to set ob matrices correct, for deform. so no_draw
		 * is part handled.
		 */
		if ((obd->transflag & OB_RENDER_DUPLI) == 0 && dob->no_draw) {
			continue;
		}

		if (obd->type == OB_MBALL) {
			continue;
		}

		data->dupli_object_current = dob;

		/* Temporary object to evaluate. */
		data->temp_dupli_object = *dob->ob;
		data->temp_dupli_object.select_color = data->dupli_parent->select_color;
		copy_m4_m4(data->temp_dupli_object.obmat, dob->mat);

		deg_flush_base_flags_and_settings(&data->temp_dupli_object,
		                                  data->base,
		                                  data->base_flag | BASE_FROMDUPLI);
		iter->current = &data->temp_dupli_object;
		BLI_assert(DEG::deg_validate_copy_on_write_datablock(&data->temp_dupli_object.id));
		return true;
	}

	return false;
}

void DEG_objects_iterator_next(BLI_Iterator *iter)
{
	DEGObjectsIteratorData *data = (DEGObjectsIteratorData *)iter->data;
	Base *base;

	if (data->dupli_list) {
		if (deg_objects_dupli_iterator_next(iter)) {
			return;
		}
		else {
			free_object_duplilist(data->dupli_list);
			data->dupli_parent = NULL;
			data->dupli_list = NULL;
			data->dupli_object_next = NULL;
			data->dupli_object_current = NULL;
		}
	}

	base = data->base->next;
	while (base != NULL) {
		if ((base->flag & BASE_VISIBLED) != 0) {
			// Object *ob = DEG_get_object(data->graph, base->object);
			Object *ob = base->object;
			iter->current = ob;
			data->base = base;

			BLI_assert(DEG::deg_validate_copy_on_write_datablock(&ob->id));

			/* Make sure we have the base collection settings is already populated.
			 * This will fail when BKE_layer_eval_layer_collection_pre hasn't run yet
			 * Which usually means a missing call to DAG_id_tag_update(). */
			BLI_assert(!BLI_listbase_is_empty(&base->collection_properties->data.group));

			/* Flushing depsgraph data. */
			deg_flush_base_flags_and_settings(ob,
			                                  base,
			                                  data->base_flag);

			if ((data->flag & DEG_OBJECT_ITER_FLAG_DUPLI) && (ob->transflag & OB_DUPLI)) {
				data->dupli_parent = ob;
				data->dupli_list = object_duplilist(&data->eval_ctx, data->scene, ob);
				data->dupli_object_next = (DupliObject *)data->dupli_list->first;
			}
			return;
		}
		base = base->next;
	}

	/* Look for an object in the next set. */
	if ((data->flag & DEG_OBJECT_ITER_FLAG_SET) && data->scene->set) {
		SceneLayer *scene_layer;
		data->scene = data->scene->set;
		data->base_flag = ~(BASE_SELECTED | BASE_SELECTABLED);

		/* For the sets we use the layer used for rendering. */
		scene_layer = BKE_scene_layer_render_active(data->scene);

		/* TODO(sergey): It's really confusing to store pointer to a local data. */
		Base base = {(Base *)scene_layer->object_bases.first, NULL};
		data->base = &base;
		DEG_objects_iterator_next(iter);
		return;
	}

	iter->current = NULL;
	iter->valid = false;
}

void DEG_objects_iterator_end(BLI_Iterator *iter)
{
#ifndef NDEBUG
	DEGObjectsIteratorData *data = (DEGObjectsIteratorData *)iter->data;
	/* Force crash in case the iterator data is referenced and accessed down the line. (T51718) */
	memset(&data->temp_dupli_object, 0xff, sizeof(data->temp_dupli_object));
#else
	(void) iter;
#endif
}
