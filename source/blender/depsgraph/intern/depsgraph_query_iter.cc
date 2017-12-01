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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Dalai Felinto
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_query_iter.cc
 *  \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BKE_anim.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
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

/* ************************ DEG ITERATORS ********************* */

static bool deg_objects_dupli_iterator_next(BLI_Iterator *iter)
{
	DEGOIterObjectData *data = (DEGOIterObjectData *)iter->data;
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
		Object *dupli_parent = data->dupli_parent;
		Object *temp_dupli_object = &data->temp_dupli_object;
		*temp_dupli_object = *dob->ob;
		temp_dupli_object->select_color = dupli_parent->select_color;
		temp_dupli_object->base_flag = dupli_parent->base_flag | BASE_FROMDUPLI;
		BLI_assert(dob->collection_properties != NULL);
		temp_dupli_object->base_collection_properties = dob->collection_properties;
		IDP_MergeGroup(temp_dupli_object->base_collection_properties, dupli_parent->base_collection_properties, false);
		copy_m4_m4(data->temp_dupli_object.obmat, dob->mat);
		iter->current = &data->temp_dupli_object;
		BLI_assert(
		        DEG::deg_validate_copy_on_write_datablock(
		                &data->temp_dupli_object.id));
		return true;
	}

	return false;
}

static void DEG_iterator_objects_step(BLI_Iterator *iter, DEG::IDDepsNode *id_node)
{
	/* Reset the skip in case we are running from within a loop. */
	iter->skip = false;

	DEGOIterObjectData *data = (DEGOIterObjectData *)iter->data;
	const ID_Type id_type = GS(id_node->id_orig->name);

	if (id_type != ID_OB) {
		iter->skip = true;
		return;
	}

	switch (id_node->linked_state) {
		case DEG::DEG_ID_LINKED_DIRECTLY:
			break;
		case DEG::DEG_ID_LINKED_VIA_SET:
			if (data->flag & DEG_ITER_OBJECT_FLAG_SET) {
				break;
			}
			else {
				ATTR_FALLTHROUGH;
			}
		case DEG::DEG_ID_LINKED_INDIRECTLY:
			iter->skip = true;
			return;
	}

	Object *object = (Object *)id_node->id_cow;
	BLI_assert(DEG::deg_validate_copy_on_write_datablock(&object->id));

	if ((data->flag & DEG_ITER_OBJECT_FLAG_DUPLI) && (object->transflag & OB_DUPLI)) {
		data->dupli_parent = object;
		data->dupli_list = object_duplilist(&data->eval_ctx, data->scene, object);
		data->dupli_object_next = (DupliObject *)data->dupli_list->first;
	}

	iter->current = object;
}

void DEG_iterator_objects_begin(BLI_Iterator *iter, DEGOIterObjectData *data)
{
	Depsgraph *depsgraph = data->graph;
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
	const size_t num_id_nodes = deg_graph->id_nodes.size();

	if (num_id_nodes == 0) {
		iter->valid = false;
		return;
	}

	/* TODO(sergey): What evaluation type we want here? */
	DEG_evaluation_context_init(&data->eval_ctx, DAG_EVAL_RENDER);
	data->eval_ctx.view_layer = DEG_get_evaluated_view_layer(depsgraph);

	iter->data = data;
	data->dupli_parent = NULL;
	data->dupli_list = NULL;
	data->dupli_object_next = NULL;
	data->dupli_object_current = NULL;
	data->scene = DEG_get_evaluated_scene(depsgraph);
	data->id_node_index = 0;
	data->num_id_nodes = num_id_nodes;

	DEG::IDDepsNode *id_node = deg_graph->id_nodes[data->id_node_index];
	DEG_iterator_objects_step(iter, id_node);

	if (iter->skip) {
		DEG_iterator_objects_next(iter);
	}
}

void DEG_iterator_objects_next(BLI_Iterator *iter)
{
	DEGOIterObjectData *data = (DEGOIterObjectData *)iter->data;
	Depsgraph *depsgraph = data->graph;
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
	do {
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

		++data->id_node_index;
		if (data->id_node_index == data->num_id_nodes) {
			iter->valid = false;
			return;
		}

		DEG::IDDepsNode *id_node = deg_graph->id_nodes[data->id_node_index];
		DEG_iterator_objects_step(iter, id_node);
	} while (iter->skip);
}

void DEG_iterator_objects_end(BLI_Iterator *iter)
{
#ifndef NDEBUG
	DEGOIterObjectData *data = (DEGOIterObjectData *)iter->data;
	/* Force crash in case the iterator data is referenced and accessed down the line. (T51718) */
	memset(&data->temp_dupli_object, 0xff, sizeof(data->temp_dupli_object));
#else
	(void) iter;
#endif
}
