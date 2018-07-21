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

/** \file blender/depsgraph/intern/depsgraph_tag.cc
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include <stdio.h>
#include <cstring>  /* required for memset */
#include <queue>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_math_bits.h"
#include "BLI_task.h"

extern "C" {
#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_animsys.h"
#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#define new new_
#include "BKE_screen.h"
#undef new
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/builder/deg_builder.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

/* *********************** */
/* Update Tagging/Flushing */

namespace DEG {

namespace {

void deg_graph_id_tag_update(Main *bmain, Depsgraph *graph, ID *id, int flag);

void depsgraph_geometry_tag_to_component(const ID *id,
                                         eDepsNode_Type *component_type)
{
	const ID_Type id_type = GS(id->name);
	switch (id_type) {
		case ID_OB:
		{
			const Object *object = (Object *)id;
			switch (object->type) {
				case OB_MESH:
				case OB_CURVE:
				case OB_SURF:
				case OB_FONT:
				case OB_LATTICE:
				case OB_MBALL:
					*component_type = DEG_NODE_TYPE_GEOMETRY;
					break;
				case OB_ARMATURE:
					*component_type = DEG_NODE_TYPE_EVAL_POSE;
					break;
					/* TODO(sergey): More cases here? */
			}
			break;
		}
		case ID_ME:
			*component_type = DEG_NODE_TYPE_GEOMETRY;
			break;
		case ID_PA:
			return;
		case ID_LP:
			*component_type = DEG_NODE_TYPE_PARAMETERS;
			break;
		default:
			break;
	}
}

void depsgraph_select_tag_to_component_opcode(
        const ID *id,
        eDepsNode_Type *component_type,
        eDepsOperation_Code *operation_code)
{
	const ID_Type id_type = GS(id->name);
	if (id_type == ID_SCE) {
		/* We need to flush base flags to all objects in a scene since we
		 * don't know which ones changed. However, we don't want to update
		 * the whole scene, so pick up some operation which will do as less
		 * as possible.
		 *
		 * TODO(sergey): We can introduce explicit exit operation which
		 * does nothing and which is only used to cascade flush down the
		 * road.
		 */
		*component_type = DEG_NODE_TYPE_LAYER_COLLECTIONS;
		*operation_code = DEG_OPCODE_VIEW_LAYER_EVAL;
	}
	else if (id_type == ID_OB) {
		*component_type = DEG_NODE_TYPE_OBJECT_FROM_LAYER;
		*operation_code = DEG_OPCODE_OBJECT_BASE_FLAGS;
	}
	else {
		*component_type = DEG_NODE_TYPE_BATCH_CACHE;
		*operation_code = DEG_OPCODE_GEOMETRY_SELECT_UPDATE;
	}
}

void depsgraph_base_flags_tag_to_component_opcode(
        const ID *id,
        eDepsNode_Type *component_type,
        eDepsOperation_Code *operation_code)
{
	const ID_Type id_type = GS(id->name);
	if (id_type == ID_SCE) {
		*component_type = DEG_NODE_TYPE_LAYER_COLLECTIONS;
		*operation_code = DEG_OPCODE_VIEW_LAYER_EVAL;
	}
	else if (id_type == ID_OB) {
		*component_type = DEG_NODE_TYPE_OBJECT_FROM_LAYER;
		*operation_code = DEG_OPCODE_OBJECT_BASE_FLAGS;
	}
}

void depsgraph_tag_to_component_opcode(const ID *id,
                                       eDepsgraph_Tag tag,
                                       eDepsNode_Type *component_type,
                                       eDepsOperation_Code *operation_code)
{
	const ID_Type id_type = GS(id->name);
	*component_type = DEG_NODE_TYPE_UNDEFINED;
	*operation_code = DEG_OPCODE_OPERATION;
	/* Special case for now, in the future we should get rid of this. */
	if (tag == 0) {
		*component_type = DEG_NODE_TYPE_ID_REF;
		*operation_code = DEG_OPCODE_OPERATION;
		return;
	}
	switch (tag) {
		case DEG_TAG_TRANSFORM:
			*component_type = DEG_NODE_TYPE_TRANSFORM;
			break;
		case DEG_TAG_GEOMETRY:
			depsgraph_geometry_tag_to_component(id, component_type);
			break;
		case DEG_TAG_TIME:
			*component_type = DEG_NODE_TYPE_ANIMATION;
			break;
		case DEG_TAG_PSYS_REDO:
		case DEG_TAG_PSYS_RESET:
		case DEG_TAG_PSYS_TYPE:
		case DEG_TAG_PSYS_CHILD:
		case DEG_TAG_PSYS_PHYS:
			if (id_type == ID_PA) {
				/* NOTES:
				 * - For particle settings node we need to use different
				 *   component. Will be nice to get this unified with object,
				 *   but we can survive for now with single exception here.
				 *   Particles needs reconsideration anyway,
				 */
				*component_type = DEG_NODE_TYPE_PARAMETERS;
			}
			else {
				*component_type = DEG_NODE_TYPE_EVAL_PARTICLES;
			}
			break;
		case DEG_TAG_COPY_ON_WRITE:
			*component_type = DEG_NODE_TYPE_COPY_ON_WRITE;
			break;
		case DEG_TAG_SHADING_UPDATE:
			if (id_type == ID_NT) {
				*component_type = DEG_NODE_TYPE_SHADING_PARAMETERS;
			}
			else {
				*component_type = DEG_NODE_TYPE_SHADING;
			}
			break;
		case DEG_TAG_SELECT_UPDATE:
			depsgraph_select_tag_to_component_opcode(id,
			                                         component_type,
			                                         operation_code);
			break;
		case DEG_TAG_BASE_FLAGS_UPDATE:
			depsgraph_base_flags_tag_to_component_opcode(id,
			                                             component_type,
			                                             operation_code);
		case DEG_TAG_EDITORS_UPDATE:
			/* There is no such node in depsgraph, this tag is to be handled
			 * separately.
			 */
			break;
		case DEG_TAG_PSYS_ALL:
			BLI_assert(!"Should not happen");
			break;
	}
}

void id_tag_update_ntree_special(Main *bmain, Depsgraph *graph, ID *id, int flag)
{
	bNodeTree *ntree = ntreeFromID(id);
	if (ntree == NULL) {
		return;
	}
	deg_graph_id_tag_update(bmain, graph, &ntree->id, flag);
}

void depsgraph_update_editors_tag(Main *bmain, Depsgraph *graph, ID *id)
{
	/* NOTE: We handle this immediately, without delaying anything, to be
	 * sure we don't cause threading issues with OpenGL.
	 */
	/* TODO(sergey): Make sure this works for CoW-ed datablocks as well. */
	DEGEditorUpdateContext update_ctx = {NULL};
	update_ctx.bmain = bmain;
	update_ctx.depsgraph = (::Depsgraph *)graph;
	update_ctx.scene = graph->scene;
	update_ctx.view_layer = graph->view_layer;
	deg_editors_id_update(&update_ctx, id);
}

void depsgraph_tag_component(Depsgraph *graph,
                             IDDepsNode *id_node,
                             eDepsNode_Type component_type,
                             eDepsOperation_Code operation_code)
{
	ComponentDepsNode *component_node =
	        id_node->find_component(component_type);
	if (component_node == NULL) {
		return;
	}
	if (operation_code == DEG_OPCODE_OPERATION) {
		component_node->tag_update(graph);
	}
	else {
		OperationDepsNode *operation_node =
		        component_node->find_operation(operation_code);
		if (operation_node != NULL) {
			operation_node->tag_update(graph);
		}
	}
	/* If component depends on copy-on-write, tag it as well. */
	if (component_node->need_tag_cow_before_update()) {
		ComponentDepsNode *cow_comp =
		        id_node->find_component(DEG_NODE_TYPE_COPY_ON_WRITE);
		cow_comp->tag_update(graph);
		id_node->id_orig->recalc |= ID_RECALC_COPY_ON_WRITE;
	}
}

/* This is a tag compatibility with legacy code.
 *
 * Mainly, old code was tagging object with OB_RECALC_DATA tag to inform
 * that object's data datablock changed. Now API expects that ID is given
 * explicitly, but not all areas are aware of this yet.
 */
void deg_graph_id_tag_legacy_compat(Main *bmain,
                                    Depsgraph *depsgraph,
                                    ID *id,
                                    eDepsgraph_Tag tag)
{
	if (tag == DEG_TAG_GEOMETRY || tag == 0) {
		switch (GS(id->name)) {
			case ID_OB:
			{
				Object *object = (Object *)id;
				ID *data_id = (ID *)object->data;
				if (data_id != NULL) {
					deg_graph_id_tag_update(bmain, depsgraph, data_id, 0);
				}
				break;
			}
			/* TODO(sergey): Shape keys are annoying, maybe we should find a
			 * way to chain geometry evaluation to them, so we don't need extra
			 * tagging here.
			 */
			case ID_ME:
			{
				Mesh *mesh = (Mesh *)id;
				ID *key_id = &mesh->key->id;
				if (key_id != NULL) {
					deg_graph_id_tag_update(bmain, depsgraph, key_id, 0);
				}
				break;
			}
			case ID_LT:
			{
				Lattice *lattice = (Lattice *)id;
				ID *key_id = &lattice->key->id;
				if (key_id != NULL) {
					deg_graph_id_tag_update(bmain, depsgraph, key_id, 0);
				}
				break;
			}
			case ID_CU:
			{
				Curve *curve = (Curve *)id;
				ID *key_id = &curve->key->id;
				if (key_id != NULL) {
					deg_graph_id_tag_update(bmain, depsgraph, key_id, 0);
				}
				break;
			}
			default:
				break;
		}
	}
}

static void deg_graph_id_tag_update_single_flag(Main *bmain,
                                                Depsgraph *graph,
                                                ID *id,
                                                IDDepsNode *id_node,
                                                eDepsgraph_Tag tag)
{
	if (tag == DEG_TAG_EDITORS_UPDATE) {
		if (graph != NULL) {
			depsgraph_update_editors_tag(bmain, graph, id);
		}
		return;
	}
	/* Get description of what is to be tagged. */
	eDepsNode_Type component_type;
	eDepsOperation_Code operation_code;
	depsgraph_tag_to_component_opcode(id,
	                                  tag,
	                                  &component_type,
	                                  &operation_code);
	/* Check whether we've got something to tag. */
	if (component_type == DEG_NODE_TYPE_UNDEFINED) {
		/* Given ID does not support tag. */
		/* TODO(sergey): Shall we raise some panic here? */
		return;
	}
	/* Tag ID recalc flag. */
	DepsNodeFactory *factory = deg_type_get_factory(component_type);
	BLI_assert(factory != NULL);
	id->recalc |= factory->id_recalc_tag();
	/* Some sanity checks before moving forward. */
	if (id_node == NULL) {
		/* Happens when object is tagged for update and not yet in the
		 * dependency graph (but will be after relations update).
		 */
		return;
	}
	/* Tag corresponding dependency graph operation for update. */
	if (component_type == DEG_NODE_TYPE_ID_REF) {
		id_node->tag_update(graph);
	}
	else {
		depsgraph_tag_component(graph, id_node, component_type, operation_code);
	}
	/* TODO(sergey): Get rid of this once all areas are using proper data ID
	 * for tagging.
	 */
	deg_graph_id_tag_legacy_compat(bmain, graph, id, tag);

}

string stringify_append_bit(const string& str, eDepsgraph_Tag tag)
{
	string result = str;
	if (!result.empty()) {
		result += ", ";
	}
	result += DEG_update_tag_as_string(tag);
	return result;
}

string stringify_update_bitfield(int flag)
{
	if (flag == 0) {
		return "LEGACY_0";
	}
	string result = "";
	int current_flag = flag;
	/* Special cases to avoid ALL flags form being split into
	 * individual bits.
	 */
	if ((current_flag & DEG_TAG_PSYS_ALL) == DEG_TAG_PSYS_ALL) {
		result = stringify_append_bit(result, DEG_TAG_PSYS_ALL);
	}
	/* Handle all the rest of the flags. */
	while (current_flag != 0) {
		eDepsgraph_Tag tag =
		        (eDepsgraph_Tag)(1 << bitscan_forward_clear_i(&current_flag));
		result = stringify_append_bit(result, tag);
	}
	return result;
}

/* Special tag function which tags all components which needs to be tagged
 * for update flag=0.
 *
 * TODO(sergey): This is something to be avoid in the future, make it more
 * explicit and granular for users to tag what they really need.
 */
void deg_graph_node_tag_zero(Main *bmain, Depsgraph *graph, IDDepsNode *id_node)
{
	if (id_node == NULL) {
		return;
	}
	ID *id = id_node->id_orig;
	/* TODO(sergey): Which recalc flags to set here? */
	id->recalc |= ID_RECALC_ALL & ~(DEG_TAG_PSYS_ALL | ID_RECALC_ANIMATION);
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
	{
		if (comp_node->type == DEG_NODE_TYPE_ANIMATION) {
			continue;
		}
		comp_node->tag_update(graph);
	}
	GHASH_FOREACH_END();
	deg_graph_id_tag_legacy_compat(bmain, graph, id, (eDepsgraph_Tag)0);
}

void deg_graph_id_tag_update(Main *bmain, Depsgraph *graph, ID *id, int flag)
{
	const int debug_flags = (graph != NULL)
	        ? DEG_debug_flags_get((::Depsgraph *)graph)
	        : G.debug;
	if (debug_flags & G_DEBUG_DEPSGRAPH_TAG) {
		printf("%s: id=%s flags=%s\n",
		       __func__,
		       id->name,
		       stringify_update_bitfield(flag).c_str());
	}
	IDDepsNode *id_node = (graph != NULL) ? graph->find_id_node(id)
	                                      : NULL;
	DEG_id_type_tag(bmain, GS(id->name));
	if (flag == 0) {
		deg_graph_node_tag_zero(bmain, graph, id_node);
	}
	id->recalc |= (flag & PSYS_RECALC);
	int current_flag = flag;
	while (current_flag != 0) {
		eDepsgraph_Tag tag =
		        (eDepsgraph_Tag)(1 << bitscan_forward_clear_i(&current_flag));
		deg_graph_id_tag_update_single_flag(bmain,
		                                    graph,
		                                    id,
		                                    id_node,
		                                    tag);
	}
	/* Special case for nested node tree datablocks. */
	id_tag_update_ntree_special(bmain, graph, id, flag);
}

void deg_id_tag_update(Main *bmain, ID *id, int flag)
{
	deg_graph_id_tag_update(bmain, NULL, id, flag);
	LISTBASE_FOREACH (Scene *, scene, &bmain->scene) {
		LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
			Depsgraph *depsgraph =
			        (Depsgraph *)BKE_scene_get_depsgraph(scene,
			                                             view_layer,
			                                             false);
			if (depsgraph != NULL) {
				deg_graph_id_tag_update(bmain, depsgraph, id, flag);
			}
		}
	}
}

void deg_graph_on_visible_update(Main *bmain, Depsgraph *graph)
{
	/* Make sure objects are up to date. */
	foreach (DEG::IDDepsNode *id_node, graph->id_nodes) {
		const ID_Type id_type = GS(id_node->id_orig->name);
		int flag = DEG_TAG_COPY_ON_WRITE;
		/* We only tag components which needs an update. Tagging everything is
		 * not a good idea because that might reset particles cache (or any
		 * other type of cache).
		 *
		 * TODO(sergey): Need to generalize this somehow.
		 */
		if (id_type == ID_OB) {
			flag |= OB_RECALC_OB | OB_RECALC_DATA;
		}
		deg_graph_id_tag_update(bmain, graph, id_node->id_orig, flag);
	}
	/* Make sure collection properties are up to date. */
	for (Scene *scene_iter = graph->scene;
	     scene_iter != NULL;
	     scene_iter = scene_iter->set)
	{
		IDDepsNode *scene_id_node = graph->find_id_node(&scene_iter->id);
		if (scene_id_node != NULL) {
			scene_id_node->tag_update(graph);
		}
		else {
			BLI_assert(graph->need_update);
		}
	}
}

}  /* namespace */

}  // namespace DEG

const char *DEG_update_tag_as_string(eDepsgraph_Tag flag)
{
	switch (flag) {
		case DEG_TAG_TRANSFORM: return "TRANSFORM";
		case DEG_TAG_GEOMETRY: return "GEOMETRY";
		case DEG_TAG_TIME: return "TIME";
		case DEG_TAG_PSYS_REDO: return "PSYS_REDO";
		case DEG_TAG_PSYS_RESET: return "PSYS_RESET";
		case DEG_TAG_PSYS_TYPE: return "PSYS_TYPE";
		case DEG_TAG_PSYS_CHILD: return "PSYS_CHILD";
		case DEG_TAG_PSYS_PHYS: return "PSYS_PHYS";
		case DEG_TAG_PSYS_ALL: return "PSYS_ALL";
		case DEG_TAG_COPY_ON_WRITE: return "COPY_ON_WRITE";
		case DEG_TAG_SHADING_UPDATE: return "SHADING_UPDATE";
		case DEG_TAG_SELECT_UPDATE: return "SELECT_UPDATE";
		case DEG_TAG_BASE_FLAGS_UPDATE: return "BASE_FLAGS_UPDATE";
		case DEG_TAG_EDITORS_UPDATE: return "EDITORS_UPDATE";
	}
	BLI_assert(!"Unhandled update flag, should never happen!");
	return "UNKNOWN";
}

/* Data-Based Tagging  */

/* Tag given ID for an update in all the dependency graphs. */
void DEG_id_tag_update(ID *id, int flag)
{
	DEG_id_tag_update_ex(G.main, id, flag);
}

void DEG_id_tag_update_ex(Main *bmain, ID *id, int flag)
{
	if (id == NULL) {
		/* Ideally should not happen, but old depsgraph allowed this. */
		return;
	}
	DEG::deg_id_tag_update(bmain, id, flag);
}

void DEG_graph_id_tag_update(struct Main *bmain,
                             struct Depsgraph *depsgraph,
                             struct ID *id,
                             int flag)
{
	DEG::Depsgraph *graph = (DEG::Depsgraph *)depsgraph;
	DEG::deg_graph_id_tag_update(bmain, graph, id, flag);
}

/* Mark a particular datablock type as having changing. */
void DEG_id_type_tag(Main *bmain, short id_type)
{
	if (id_type == ID_NT) {
		/* Stupid workaround so parent datablocks of nested nodetree get looped
		 * over when we loop over tagged datablock types.
		 */
		DEG_id_type_tag(bmain, ID_MA);
		DEG_id_type_tag(bmain, ID_TE);
		DEG_id_type_tag(bmain, ID_LA);
		DEG_id_type_tag(bmain, ID_WO);
		DEG_id_type_tag(bmain, ID_SCE);
	}

	int id_type_index = BKE_idcode_to_index(id_type);

	LISTBASE_FOREACH (Scene *, scene, &bmain->scene) {
		LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
			Depsgraph *depsgraph =
			        (Depsgraph *)BKE_scene_get_depsgraph(scene,
			                                             view_layer,
			                                             false);
			if (depsgraph != NULL) {
				DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
				deg_graph->id_type_updated[id_type_index] = 1;
			}
		}
	}
}

void DEG_graph_flush_update(Main *bmain, Depsgraph *depsgraph)
{
	if (depsgraph == NULL) {
		return;
	}
	DEG::deg_graph_flush_updates(bmain, (DEG::Depsgraph *)depsgraph);
}

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(Main *bmain, Depsgraph *depsgraph)
{
	DEG::Depsgraph *graph = (DEG::Depsgraph *)depsgraph;
	DEG::deg_graph_on_visible_update(bmain, graph);
}

void DEG_on_visible_update(Main *bmain, const bool UNUSED(do_time))
{
	LISTBASE_FOREACH (Scene *, scene, &bmain->scene) {
		LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
			Depsgraph *depsgraph =
			        (Depsgraph *)BKE_scene_get_depsgraph(scene,
			                                             view_layer,
			                                             false);
			if (depsgraph != NULL) {
				DEG_graph_on_visible_update(bmain, depsgraph);
			}
		}
	}
}

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(Main *bmain,
                          Depsgraph *depsgraph,
                          Scene *scene,
                          ViewLayer *view_layer,
                          bool time)
{
	bool updated = time || DEG_id_type_any_updated(depsgraph);

	DEGEditorUpdateContext update_ctx = {NULL};
	update_ctx.bmain = bmain;
	update_ctx.depsgraph = depsgraph;
	update_ctx.scene = scene;
	update_ctx.view_layer = view_layer;
	DEG::deg_editors_scene_update(&update_ctx, updated);
}

static void deg_graph_clear_id_node_func(
        void *__restrict data_v,
        const int i,
        const ParallelRangeTLS *__restrict /*tls*/)
{
	/* TODO: we clear original ID recalc flags here, but this may not work
	 * correctly when there are multiple depsgraph with others still using
	 * the recalc flag. */
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(data_v);
	DEG::IDDepsNode *id_node = deg_graph->id_nodes[i];
	id_node->id_cow->recalc &= ~ID_RECALC_ALL;
	id_node->id_orig->recalc &= ~ID_RECALC_ALL;

	/* Clear embedded node trees too. */
	bNodeTree *ntree_cow = ntreeFromID(id_node->id_cow);
	if (ntree_cow) {
		ntree_cow->id.recalc &= ~ID_RECALC_ALL;
	}
	bNodeTree *ntree_orig = ntreeFromID(id_node->id_orig);
	if (ntree_orig) {
		ntree_orig->id.recalc &= ~ID_RECALC_ALL;
	}
}

void DEG_ids_clear_recalc(Main *UNUSED(bmain),
                          Depsgraph *depsgraph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);

	/* TODO(sergey): Re-implement POST_UPDATE_HANDLER_WORKAROUND using entry_tags
	 * and id_tags storage from the new dependency graph.
	 */

	if (!DEG_id_type_any_updated(depsgraph)) {
		return;
	}

	/* Go over all ID nodes nodes, clearing tags. */
	const int num_id_nodes = deg_graph->id_nodes.size();
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.min_iter_per_thread = 1024;
	BLI_task_parallel_range(0, num_id_nodes,
	                        deg_graph,
	                        deg_graph_clear_id_node_func,
	                        &settings);

	memset(deg_graph->id_type_updated, 0, sizeof(deg_graph->id_type_updated));
}
