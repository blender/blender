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

/** \file blender/depsgraph/intern/builder/deg_builder_nodes.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph's nodes
 */

#include "intern/builder/deg_builder_nodes.h"

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_particle_types.h"
#include "DNA_object_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_idcode.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_sound.h"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "util/deg_util_foreach.h"

namespace DEG {

namespace {

void free_copy_on_write_datablock(void *id_v)
{
	ID *id = (ID *)id_v;
	deg_free_copy_on_write_datablock(id);
	MEM_freeN(id);
}

}  /* namespace */

/* ************ */
/* Node Builder */

/* **** General purpose functions **** */

DepsgraphNodeBuilder::DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph)
    : bmain_(bmain),
      graph_(graph),
      scene_(NULL),
      view_layer_(NULL),
      cow_id_hash_(NULL)
{
}

DepsgraphNodeBuilder::~DepsgraphNodeBuilder()
{
	if (cow_id_hash_ != NULL) {
		BLI_ghash_free(cow_id_hash_, NULL, free_copy_on_write_datablock);
	}
}

IDDepsNode *DepsgraphNodeBuilder::add_id_node(ID *id)
{
	IDDepsNode *id_node = NULL;
	ID *id_cow = (ID *)BLI_ghash_lookup(cow_id_hash_, id);
	if (id_cow != NULL) {
		/* TODO(sergey): Is it possible to lookup and pop element from GHash
		 * at the same time?
		 */
		BLI_ghash_remove(cow_id_hash_, id, NULL, NULL);
	}
	id_node = graph_->add_id_node(id, id_cow);
	/* Currently all ID nodes are supposed to have copy-on-write logic.
	 *
	 * NOTE: Zero number of components indicates that ID node was just created.
	 */
	if (BLI_ghash_len(id_node->components) == 0) {
		ComponentDepsNode *comp_cow =
		        id_node->add_component(DEG_NODE_TYPE_COPY_ON_WRITE);
		OperationDepsNode *op_cow = comp_cow->add_operation(
		        function_bind(deg_evaluate_copy_on_write, _1, id_node),
		        DEG_OPCODE_COPY_ON_WRITE,
		        "", -1);
		graph_->operations.push_back(op_cow);
	}
	return id_node;
}

IDDepsNode *DepsgraphNodeBuilder::find_id_node(ID *id)
{
	return graph_->find_id_node(id);
}

TimeSourceDepsNode *DepsgraphNodeBuilder::add_time_source()
{
	return graph_->add_time_source();
}

ComponentDepsNode *DepsgraphNodeBuilder::add_component_node(
        ID *id,
        eDepsNode_Type comp_type,
        const char *comp_name)
{
	IDDepsNode *id_node = add_id_node(id);
	ComponentDepsNode *comp_node = id_node->add_component(comp_type, comp_name);
	comp_node->owner = id_node;
	return comp_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(
        ComponentDepsNode *comp_node,
        const DepsEvalOperationCb& op,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	OperationDepsNode *op_node = comp_node->find_operation(opcode,
	                                                       name,
	                                                       name_tag);
	if (op_node == NULL) {
		op_node = comp_node->add_operation(op, opcode, name, name_tag);
		graph_->operations.push_back(op_node);
	}
	else {
		fprintf(stderr,
		        "add_operation: Operation already exists - %s has %s at %p\n",
		        comp_node->identifier().c_str(),
		        op_node->identifier().c_str(),
		        op_node);
		BLI_assert(!"Should not happen!");
	}
	return op_node;
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(
        ID *id,
        eDepsNode_Type comp_type,
        const char *comp_name,
        const DepsEvalOperationCb& op,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	ComponentDepsNode *comp_node = add_component_node(id, comp_type, comp_name);
	return add_operation_node(comp_node, op, opcode, name, name_tag);
}

OperationDepsNode *DepsgraphNodeBuilder::add_operation_node(
        ID *id,
        eDepsNode_Type comp_type,
        const DepsEvalOperationCb& op,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	return add_operation_node(id,
	                          comp_type,
	                          "",
	                          op,
	                          opcode,
	                          name,
	                          name_tag);
}

OperationDepsNode *DepsgraphNodeBuilder::ensure_operation_node(
        ID *id,
        eDepsNode_Type comp_type,
        const DepsEvalOperationCb& op,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	OperationDepsNode *operation =
	        find_operation_node(id, comp_type, opcode, name, name_tag);
	if (operation != NULL) {
		return operation;
	}
	return add_operation_node(id, comp_type, op, opcode, name, name_tag);
}

bool DepsgraphNodeBuilder::has_operation_node(ID *id,
                                              eDepsNode_Type comp_type,
                                              const char *comp_name,
                                              eDepsOperation_Code opcode,
                                              const char *name,
                                              int name_tag)
{
	return find_operation_node(id,
	                           comp_type,
	                           comp_name,
	                           opcode,
	                           name,
	                           name_tag) != NULL;
}

OperationDepsNode *DepsgraphNodeBuilder::find_operation_node(
        ID *id,
        eDepsNode_Type comp_type,
        const char *comp_name,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	ComponentDepsNode *comp_node = add_component_node(id, comp_type, comp_name);
	return comp_node->find_operation(opcode, name, name_tag);
}

OperationDepsNode *DepsgraphNodeBuilder::find_operation_node(
        ID *id,
        eDepsNode_Type comp_type,
        eDepsOperation_Code opcode,
        const char *name,
        int name_tag)
{
	return find_operation_node(id, comp_type, "", opcode, name, name_tag);
}

ID *DepsgraphNodeBuilder::get_cow_id(const ID *id_orig) const
{
	return graph_->get_cow_id(id_orig);
}

ID *DepsgraphNodeBuilder::ensure_cow_id(ID *id_orig)
{
	if (id_orig->tag & LIB_TAG_COPIED_ON_WRITE) {
		/* ID is already remapped to copy-on-write. */
		return id_orig;
	}
	IDDepsNode *id_node = add_id_node(id_orig);
	return id_node->id_cow;
}

/* **** Build functions for entity nodes **** */

void DepsgraphNodeBuilder::begin_build()
{
	/* Store existing copy-on-write versions of datablock, so we can re-use
	 * them for new ID nodes.
	 */
	cow_id_hash_ = BLI_ghash_ptr_new("Depsgraph id hash");
	foreach (IDDepsNode *id_node, graph_->id_nodes) {
		if (deg_copy_on_write_is_expanded(id_node->id_cow)) {
			if (id_node->id_orig == id_node->id_cow) {
				continue;
			}
			BLI_ghash_insert(cow_id_hash_,
			                 id_node->id_orig,
			                 id_node->id_cow);
			id_node->id_cow = NULL;
		}
	}

	GSET_FOREACH_BEGIN(OperationDepsNode *, op_node, graph_->entry_tags)
	{
		ComponentDepsNode *comp_node = op_node->owner;
		IDDepsNode *id_node = comp_node->owner;

		SavedEntryTag entry_tag;
		entry_tag.id = id_node->id_orig;
		entry_tag.component_type = comp_node->type;
		entry_tag.opcode = op_node->opcode;
		saved_entry_tags_.push_back(entry_tag);
	};
	GSET_FOREACH_END();

	/* Make sure graph has no nodes left from previous state. */
	graph_->clear_all_nodes();
	graph_->operations.clear();
	BLI_gset_clear(graph_->entry_tags, NULL);
}

void DepsgraphNodeBuilder::end_build()
{
	foreach (const SavedEntryTag& entry_tag, saved_entry_tags_) {
		IDDepsNode *id_node = find_id_node(entry_tag.id);
		if (id_node == NULL) {
			continue;
		}
		ComponentDepsNode *comp_node =
		        id_node->find_component(entry_tag.component_type);
		if (comp_node == NULL) {
			continue;
		}
		OperationDepsNode *op_node = comp_node->find_operation(entry_tag.opcode);
		if (op_node == NULL) {
			continue;
		}
		op_node->tag_update(graph_);
	}
}

void DepsgraphNodeBuilder::build_id(ID *id) {
	if (id == NULL) {
		return;
	}
	switch (GS(id->name)) {
		case ID_AR:
			build_armature((bArmature *)id);
			break;
		case ID_CA:
			build_camera((Camera *)id);
			break;
		case ID_GR:
			build_collection(DEG_COLLECTION_OWNER_UNKNOWN, (Collection *)id);
			break;
		case ID_OB:
			build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY);
			break;
		case ID_KE:
			build_shapekeys((Key *)id);
			break;
		case ID_LA:
			build_lamp((Lamp *)id);
			break;
		case ID_LP:
			build_lightprobe((LightProbe *)id);
			break;
		case ID_NT:
			build_nodetree((bNodeTree *)id);
			break;
		case ID_MA:
			build_material((Material *)id);
			break;
		case ID_TE:
			build_texture((Tex *)id);
			break;
		case ID_IM:
			build_image((Image *)id);
			break;
		case ID_WO:
			build_world((World *)id);
			break;
		case ID_MSK:
			build_mask((Mask *)id);
			break;
		case ID_MC:
			build_movieclip((MovieClip *)id);
			break;
		case ID_ME:
		case ID_CU:
		case ID_MB:
		case ID_LT:
			build_object_data_geometry_datablock(id);
			break;
		case ID_SPK:
			build_speaker((Speaker *)id);
			break;
		default:
			fprintf(stderr, "Unhandled ID %s\n", id->name);
			BLI_assert(!"Should never happen");
			break;
	}
}

void DepsgraphNodeBuilder::build_collection(
        eDepsNode_CollectionOwner owner_type,
        Collection *collection)
{
	if (built_map_.checkIsBuiltAndTag(collection)) {
		return;
	}
	const bool allow_restrict_flags = (owner_type == DEG_COLLECTION_OWNER_SCENE);
	if (allow_restrict_flags) {
		const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT)
		        ? COLLECTION_RESTRICT_VIEW
		        : COLLECTION_RESTRICT_RENDER;
		if (collection->flag & restrict_flag) {
			return;
		}
	}
	/* Collection itself. */
	add_id_node(&collection->id);
	/* Build collection objects. */
	LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
		if (allow_restrict_flags) {
			const int restrict_flag = (
			        (graph_->mode == DAG_EVAL_VIEWPORT) ?
			        OB_RESTRICT_VIEW :
			        OB_RESTRICT_RENDER);
			if (cob->ob->restrictflag & restrict_flag) {
				continue;
			}
		}
		build_object(-1, cob->ob, DEG_ID_LINKED_INDIRECTLY);
	}
	/* Build child collections. */
	LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
		build_collection(owner_type, child->collection);
	}
}

void DepsgraphNodeBuilder::build_object(int base_index,
                                        Object *object,
                                        eDepsNode_LinkedState_Type linked_state)
{
	const bool has_object = built_map_.checkIsBuiltAndTag(object);
	/* Skip rest of components if the ID node was already there. */
	if (has_object) {
		IDDepsNode *id_node = find_id_node(&object->id);
		/* We need to build some extra stuff if object becomes linked
		 * directly.
		 */
		if (id_node->linked_state == DEG_ID_LINKED_INDIRECTLY) {
			build_object_flags(base_index, object, linked_state);
		}
		id_node->linked_state = max(id_node->linked_state, linked_state);
		return;
	}
	/* Create ID node for object and begin init. */
	IDDepsNode *id_node = add_id_node(&object->id);
	id_node->linked_state = linked_state;
	object->customdata_mask = 0;
	/* Various flags, flushing from bases/collections. */
	build_object_flags(base_index, object, linked_state);
	/* Transform. */
	build_object_transform(object);
	/* Parent. */
	if (object->parent != NULL) {
		build_object(-1, object->parent, DEG_ID_LINKED_INDIRECTLY);
	}
	/* Modifiers. */
	if (object->modifiers.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		modifiers_foreachIDLink(object, modifier_walk, &data);
	}
	/* Constraints. */
	if (object->constraints.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		BKE_constraints_id_loop(&object->constraints, constraint_walk, &data);
	}
	/* Object data. */
	build_object_data(object);
	/* Build animation data,
	 *
	 * Do it now because it's possible object data will affect
	 * on object's level animation, for example in case of rebuilding
	 * pose for proxy.
	 */
	OperationDepsNode *op_node = add_operation_node(&object->id,
	                                                DEG_NODE_TYPE_PARAMETERS,
	                                                NULL,
	                                                DEG_OPCODE_PARAMETERS_EVAL);
	op_node->set_as_exit();
	build_animdata(&object->id);
	/* Particle systems. */
	if (object->particlesystem.first != NULL) {
		build_particles(object);
	}
	/* Grease pencil. */
	if (object->gpd != NULL) {
		build_gpencil(object->gpd);
	}
	/* Proxy object to copy from. */
	if (object->proxy_from != NULL) {
		build_object(-1, object->proxy_from, DEG_ID_LINKED_INDIRECTLY);
	}
	if (object->proxy_group != NULL) {
		build_object(-1, object->proxy_group, DEG_ID_LINKED_INDIRECTLY);
	}
	/* Object dupligroup. */
	if (object->dup_group != NULL) {
		build_collection(DEG_COLLECTION_OWNER_OBJECT, object->dup_group);
	}
}

void DepsgraphNodeBuilder::build_object_flags(
        int base_index,
        Object *object,
        eDepsNode_LinkedState_Type linked_state)
{
	if (base_index == -1) {
		return;
	}
	Scene *scene_cow = get_cow_datablock(scene_);
	Object *object_cow = get_cow_datablock(object);
	const bool is_from_set = (linked_state == DEG_ID_LINKED_VIA_SET);
	/* TODO(sergey): Is this really best component to be used? */
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_OBJECT_FROM_LAYER,
	                   function_bind(BKE_object_eval_flush_base_flags,
	                                 _1,
	                                 scene_cow,
	                                 view_layer_index_,
	                                 object_cow, base_index,
	                                 is_from_set),
	                   DEG_OPCODE_OBJECT_BASE_FLAGS);
}

void DepsgraphNodeBuilder::build_object_data(Object *object)
{
	if (object->data == NULL) {
		return;
	}
	IDDepsNode *id_node = graph_->find_id_node(&object->id);
	/* type-specific data. */
	switch (object->type) {
		case OB_MESH:
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		case OB_MBALL:
		case OB_LATTICE:
			build_object_data_geometry(object);
			/* TODO(sergey): Only for until we support granular
			 * update of curves.
			 */
			if (object->type == OB_FONT) {
				Curve *curve = (Curve *)object->data;
				if (curve->textoncurve) {
					id_node->eval_flags |= DAG_EVAL_NEED_CURVE_PATH;
				}
			}
			break;
		case OB_ARMATURE:
			if (ID_IS_LINKED(object) && object->proxy_from != NULL) {
				build_proxy_rig(object);
			}
			else {
				build_rig(object);
			}
			break;
		case OB_LAMP:
			build_object_data_lamp(object);
			break;
		case OB_CAMERA:
			build_object_data_camera(object);
			break;
		case OB_LIGHTPROBE:
			build_object_data_lightprobe(object);
			break;
		case OB_SPEAKER:
			build_object_data_speaker(object);
			break;
		default:
		{
			ID *obdata = (ID *)object->data;
			if (built_map_.checkIsBuilt(obdata) == 0) {
				build_animdata(obdata);
			}
			break;
		}
	}
}

void DepsgraphNodeBuilder::build_object_data_camera(Object *object)
{
	Camera *camera = (Camera *)object->data;
	build_camera(camera);
}

void DepsgraphNodeBuilder::build_object_data_lamp(Object *object)
{
	Lamp *lamp = (Lamp *)object->data;
	build_lamp(lamp);
}

void DepsgraphNodeBuilder::build_object_data_lightprobe(Object *object)
{
	LightProbe *probe = (LightProbe *)object->data;
	build_lightprobe(probe);
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_LIGHT_PROBE_EVAL);
}

void DepsgraphNodeBuilder::build_object_data_speaker(Object *object)
{
	Speaker *speaker = (Speaker *)object->data;
	build_speaker(speaker);
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_SPEAKER_EVAL);
}

void DepsgraphNodeBuilder::build_object_transform(Object *object)
{
	OperationDepsNode *op_node;
	Scene *scene_cow = get_cow_datablock(scene_);
	Object *ob_cow = get_cow_datablock(object);

	/* local transforms (from transform channels - loc/rot/scale + deltas) */
	op_node = add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                             function_bind(BKE_object_eval_local_transform,
	                                           _1,
	                                           ob_cow),
	                             DEG_OPCODE_TRANSFORM_LOCAL);
	op_node->set_as_entry();

	/* object parent */
	if (object->parent != NULL) {
		add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
		                   function_bind(BKE_object_eval_parent,
		                                 _1,
		                                 scene_cow,
		                                 ob_cow),
		                   DEG_OPCODE_TRANSFORM_PARENT);
	}

	/* object constraints */
	if (object->constraints.first != NULL) {
		build_object_constraints(object);
	}

	/* Rest of transformation update. */
	add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                   function_bind(BKE_object_eval_uber_transform,
	                                 _1,
	                                 ob_cow),
	                   DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL);

	/* object transform is done */
	op_node = add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                             function_bind(BKE_object_eval_done,
	                                           _1,
	                                           ob_cow),
	                             DEG_OPCODE_TRANSFORM_FINAL);
	op_node->set_as_exit();
}

/**
 * Constraints Graph Notes
 *
 * For constraints, we currently only add a operation node to the Transform
 * or Bone components (depending on whichever type of owner we have).
 * This represents the entire constraints stack, which is for now just
 * executed as a single monolithic block. At least initially, this should
 * be sufficient for ensuring that the porting/refactoring process remains
 * manageable.
 *
 * However, when the time comes for developing "node-based" constraints,
 * we'll need to split this up into pre/post nodes for "constraint stack
 * evaluation" + operation nodes for each constraint (i.e. the contents
 * of the loop body used in the current "solve_constraints()" operation).
 *
 * -- Aligorith, August 2013
 */
void DepsgraphNodeBuilder::build_object_constraints(Object *object)
{
	/* create node for constraint stack */
	add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                   function_bind(BKE_object_eval_constraints,
	                                 _1,
	                                 get_cow_datablock(scene_),
	                                 get_cow_datablock(object)),
	                   DEG_OPCODE_TRANSFORM_CONSTRAINTS);
}

/**
 * Build graph nodes for AnimData block
 * \param id: ID-Block which hosts the AnimData
 */
void DepsgraphNodeBuilder::build_animdata(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	if (adt == NULL) {
		return;
	}
	if (adt->action != NULL) {
		build_action(adt->action);
	}
	/* animation */
	if (adt->action || adt->nla_tracks.first || adt->drivers.first) {
		(void) add_id_node(id);
		ID *id_cow = get_cow_id(id);

		// XXX: Hook up specific update callbacks for special properties which
		// may need it...

		/* actions and NLA - as a single unit for now, as it gets complicated to
		 * schedule otherwise.
		 */
		if ((adt->action) || (adt->nla_tracks.first)) {
			/* create the node */
			add_operation_node(id, DEG_NODE_TYPE_ANIMATION,
			                   function_bind(BKE_animsys_eval_animdata,
			                                 _1,
			                                 id_cow),
			                   DEG_OPCODE_ANIMATION,
			                   id->name);

			/* TODO: for each channel affected, we might also want to add some
			 * support for running RNA update callbacks on them
			 * (which will be needed for proper handling of drivers later)
			 */
		}

		/* drivers */
		int driver_index = 0;
		LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
			/* create driver */
			build_driver(id, fcu, driver_index++);
		}
	}
}

void DepsgraphNodeBuilder::build_action(bAction *action)
{
	if (built_map_.checkIsBuiltAndTag(action)) {
		return;
	}
	add_operation_node(&action->id,
	                   DEG_NODE_TYPE_ANIMATION,
	                   NULL,
	                   DEG_OPCODE_ANIMATION);
}

/**
 * Build graph node(s) for Driver
 * \param id: ID-Block that driver is attached to
 * \param fcu: Driver-FCurve
 * \param driver_index: Index in animation data drivers list
 */
void DepsgraphNodeBuilder::build_driver(ID *id, FCurve *fcurve, int driver_index)
{
	/* Create data node for this driver */
	ID *id_cow = get_cow_id(id);
	ChannelDriver *driver_orig = fcurve->driver;

	/* TODO(sergey): ideally we could pass the COW of fcu, but since it
	 * has not yet been allocated at this point we can't. As a workaround
	 * the animation systems allocates an array so we can do a fast lookup
	 * with the driver index. */
	ensure_operation_node(id,
	                      DEG_NODE_TYPE_PARAMETERS,
	                      function_bind(BKE_animsys_eval_driver, _1, id_cow, driver_index, driver_orig),
	                      DEG_OPCODE_DRIVER,
	                      fcurve->rna_path ? fcurve->rna_path : "",
	                      fcurve->array_index);
	build_driver_variables(id, fcurve);
}

void DepsgraphNodeBuilder::build_driver_variables(ID * id, FCurve *fcurve)
{
	build_driver_id_property(id, fcurve->rna_path);
	LISTBASE_FOREACH (DriverVar *, dvar, &fcurve->driver->variables) {
		DRIVER_TARGETS_USED_LOOPER(dvar)
		{
			if (dtar->id == NULL) {
				continue;
			}
			build_id(dtar->id);
			build_driver_id_property(dtar->id, dtar->rna_path);
			/* Corresponds to dtar_id_ensure_proxy_from(). */
			if ((GS(dtar->id->name) == ID_OB) &&
			    (((Object *)dtar->id)->proxy_from != NULL))
			{
				Object *proxy_from = ((Object *)dtar->id)->proxy_from;
				build_id(&proxy_from->id);
				build_driver_id_property(&proxy_from->id, dtar->rna_path);
			}
		}
		DRIVER_TARGETS_LOOPER_END
	}
}

void DepsgraphNodeBuilder::build_driver_id_property(ID *id,
                                                    const char *rna_path)
{
	if (id == NULL || rna_path == NULL) {
		return;
	}
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	RNA_id_pointer_create(id, &id_ptr);
	if (!RNA_path_resolve_full(&id_ptr, rna_path, &ptr, &prop, NULL)) {
		return;
	}
	if (prop == NULL) {
		return;
	}
	if (!RNA_property_is_idprop(prop)) {
		return;
	}
	const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);
	ensure_operation_node(id,
	                      DEG_NODE_TYPE_PARAMETERS,
	                      NULL,
	                      DEG_OPCODE_ID_PROPERTY,
	                      prop_identifier);
}

/* Recursively build graph for world */
void DepsgraphNodeBuilder::build_world(World *world)
{
	if (built_map_.checkIsBuiltAndTag(world)) {
		return;
	}
	/* Animation. */
	build_animdata(&world->id);
	/* world itself */
	add_operation_node(&world->id,
	                   DEG_NODE_TYPE_SHADING,
	                   NULL,
	                   DEG_OPCODE_WORLD_UPDATE);
	/* world's nodetree */
	if (world->nodetree != NULL) {
		build_nodetree(world->nodetree);
	}
}

/* Rigidbody Simulation - Scene Level */
void DepsgraphNodeBuilder::build_rigidbody(Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;
	Scene *scene_cow = get_cow_datablock(scene);

	/**
	 * Rigidbody Simulation Nodes
	 * ==========================
	 *
	 * There are 3 nodes related to Rigidbody Simulation:
	 * 1) "Initialize/Rebuild World" - this is called sparingly, only when the
	 *    simulation needs to be rebuilt (mainly after file reload, or moving
	 *    back to start frame)
	 * 2) "Do Simulation" - perform a simulation step - interleaved between the
	 *    evaluation steps for clusters of objects (i.e. between those affected
	 *    and/or not affected by the sim for instance).
	 *
	 * 3) "Pull Results" - grab the specific transforms applied for a specific
	 *    object - performed as part of object's transform-stack building.
	 */

	/* Create nodes --------------------------------------------------------- */

	/* XXX: is this the right component, or do we want to use another one
	 * instead?
	 */

	/* init/rebuild operation */
	/*OperationDepsNode *init_node =*/ add_operation_node(
	        &scene->id, DEG_NODE_TYPE_TRANSFORM,
	        function_bind(BKE_rigidbody_rebuild_sim, _1, scene_cow),
	        DEG_OPCODE_RIGIDBODY_REBUILD);

	/* do-sim operation */
	// XXX: what happens if we need to split into several groups?
	OperationDepsNode *sim_node = add_operation_node(
	        &scene->id, DEG_NODE_TYPE_TRANSFORM,
	        function_bind(BKE_rigidbody_eval_simulation, _1, scene_cow),
	        DEG_OPCODE_RIGIDBODY_SIM);

	/* XXX: For now, the sim node is the only one that really matters here.
	 * If any other sims get added later, we may have to remove these hacks...
	 */
	sim_node->owner->entry_operation = sim_node;
	sim_node->owner->exit_operation  = sim_node;

	/* objects - simulation participants */
	if (rbw->group) {
		build_collection(DEG_COLLECTION_OWNER_OBJECT, rbw->group);

		FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
		{
			if (object->type != OB_MESH)
				continue;

			/* 2) create operation for flushing results */
			/* object's transform component - where the rigidbody operation
			 * lives. */
			add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
			                   function_bind(
			                           BKE_rigidbody_object_sync_transforms,
			                           _1,
			                           scene_cow,
			                           get_cow_datablock(object)),
			                   DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY);
		}
		FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
	}
}

void DepsgraphNodeBuilder::build_particles(Object *object)
{
	/**
	 * Particle Systems Nodes
	 * ======================
	 *
	 * There are two types of nodes associated with representing
	 * particle systems:
	 *  1) Component (EVAL_PARTICLES) - This is the particle-system
	 *     evaluation context for an object. It acts as the container
	 *     for all the nodes associated with a particular set of particle
	 *     systems.
	 *  2) Particle System Eval Operation - This operation node acts as a
	 *     blackbox evaluation step for one particle system referenced by
	 *     the particle systems stack. All dependencies link to this operation.
	 */
	/* Component for all particle systems. */
	ComponentDepsNode *psys_comp =
	        add_component_node(&object->id, DEG_NODE_TYPE_EVAL_PARTICLES);

	/* TODO(sergey): Need to get COW of PSYS. */
	Scene *scene_cow = get_cow_datablock(scene_);
	Object *ob_cow = get_cow_datablock(object);

	add_operation_node(psys_comp,
	                   function_bind(BKE_particle_system_eval_init,
	                                 _1,
	                                 scene_cow,
	                                 ob_cow),
	                   DEG_OPCODE_PARTICLE_SYSTEM_EVAL_INIT);
	/* Build all particle systems. */
	LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
		ParticleSettings *part = psys->part;
		/* Build particle settings operations.
		 *
		 * NOTE: The call itself ensures settings are only build once.
		 */
		build_particle_settings(part);
		/* Particle system evaluation. */
		add_operation_node(psys_comp,
		                   NULL,
		                   DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
		                   psys->name);
		/* Visualization of particle system. */
		switch (part->ren_as) {
			case PART_DRAW_OB:
				if (part->dup_ob != NULL) {
					build_object(-1,
					             part->dup_ob,
					             DEG_ID_LINKED_INDIRECTLY);
				}
				break;
			case PART_DRAW_GR:
				if (part->dup_group != NULL) {
					build_collection(DEG_COLLECTION_OWNER_OBJECT, part->dup_group);
				}
				break;
		}
	}

	/* TODO(sergey): Do we need a point cache operations here? */
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_CACHE,
	                   function_bind(BKE_ptcache_object_reset,
	                                 scene_cow,
	                                 ob_cow,
	                                 PTCACHE_RESET_DEPSGRAPH),
	                   DEG_OPCODE_POINT_CACHE_RESET);
}

void DepsgraphNodeBuilder::build_particle_settings(ParticleSettings *part) {
	if (built_map_.checkIsBuiltAndTag(part)) {
		return;
	}
	/* Animation data. */
	build_animdata(&part->id);
	/* Parameters change. */
	add_operation_node(&part->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARTICLE_SETTINGS_EVAL);
}

void DepsgraphNodeBuilder::build_cloth(Object *object)
{
	Scene *scene_cow = get_cow_datablock(scene_);
	Object *object_cow = get_cow_datablock(object);
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_CACHE,
	                   function_bind(BKE_object_eval_cloth,
	                                 _1,
	                                 scene_cow,
	                                 object_cow),
	                   DEG_OPCODE_GEOMETRY_CLOTH_MODIFIER);
}

/* Shapekeys */
void DepsgraphNodeBuilder::build_shapekeys(Key *key)
{
	if (built_map_.checkIsBuiltAndTag(key)) {
		return;
	}
	build_animdata(&key->id);
	add_operation_node(&key->id,
	                   DEG_NODE_TYPE_GEOMETRY,
	                   NULL,
	                   DEG_OPCODE_GEOMETRY_SHAPEKEY);
}

/* ObData Geometry Evaluation */
// XXX: what happens if the datablock is shared!
void DepsgraphNodeBuilder::build_object_data_geometry(Object *object)
{
	OperationDepsNode *op_node;
	Scene *scene_cow = get_cow_datablock(scene_);
	Object *object_cow = get_cow_datablock(object);
	/* Temporary uber-update node, which does everything.
	 * It is for the being we're porting old dependencies into the new system.
	 * We'll get rid of this node as soon as all the granular update functions
	 * are filled in.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_GEOMETRY,
	                             function_bind(BKE_object_eval_uber_data,
	                                           _1,
	                                           scene_cow,
	                                           object_cow),
	                             DEG_OPCODE_GEOMETRY_UBEREVAL);
	op_node->set_as_exit();

	op_node = add_operation_node(&object->id,
	                             DEG_NODE_TYPE_GEOMETRY,
	                             NULL,
	                             DEG_OPCODE_PLACEHOLDER,
	                             "Eval Init");
	op_node->set_as_entry();
	// TODO: "Done" operation
	/* Cloth modifier. */
	LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
		if (md->type == eModifierType_Cloth) {
			build_cloth(object);
		}
	}
	/* Materials. */
	if (object->totcol != 0) {
		if (object->type == OB_MESH) {
			add_operation_node(&object->id,
			                   DEG_NODE_TYPE_SHADING,
			                   function_bind(BKE_object_eval_update_shading,
			                                 _1,
			                                 object_cow),
			                   DEG_OPCODE_SHADING);
		}

		for (int a = 1; a <= object->totcol; a++) {
			Material *ma = give_current_material(object, a);
			if (ma != NULL) {
				build_material(ma);
			}
		}
	}
	/* Geometry collision. */
	if (ELEM(object->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}
	build_object_data_geometry_datablock((ID *)object->data);
}

void DepsgraphNodeBuilder::build_object_data_geometry_datablock(ID *obdata)
{
	if (built_map_.checkIsBuiltAndTag(obdata)) {
		return;
	}
	OperationDepsNode *op_node;
	/* Make sure we've got an ID node before requesting CoW pointer. */
	(void) add_id_node((ID *)obdata);
	ID *obdata_cow = get_cow_id(obdata);
	/* Animation. */
	build_animdata(obdata);
	/* ShapeKeys */
	Key *key = BKE_key_from_id(obdata);
	if (key) {
		build_shapekeys(key);
	}
	/* Nodes for result of obdata's evaluation, and geometry
	 * evaluation on object.
	 */
	const ID_Type id_type = GS(obdata->name);
	switch (id_type) {
		case ID_ME:
		{
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_mesh_eval_geometry,
			                                           _1,
			                                           (Mesh *)obdata_cow),
			                             DEG_OPCODE_PLACEHOLDER,
			                             "Geometry Eval");
			op_node->set_as_entry();
			break;
		}
		case ID_MB:
		{
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             NULL,
			                             DEG_OPCODE_PLACEHOLDER,
			                             "Geometry Eval");
			op_node->set_as_entry();
			break;
		}
		case ID_CU:
		{
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_curve_eval_geometry,
			                                           _1,
			                                           (Curve *)obdata_cow),
			                                           DEG_OPCODE_PLACEHOLDER,
			                                           "Geometry Eval");
			op_node->set_as_entry();
			/* Make sure objects used for bevel.taper are in the graph.
			 * NOTE: This objects might be not linked to the scene.
			 */
			Curve *cu = (Curve *)obdata;
			if (cu->bevobj != NULL) {
				build_object(-1, cu->bevobj, DEG_ID_LINKED_INDIRECTLY);
			}
			if (cu->taperobj != NULL) {
				build_object(-1, cu->taperobj, DEG_ID_LINKED_INDIRECTLY);
			}
			if (cu->textoncurve != NULL) {
				build_object(-1, cu->textoncurve, DEG_ID_LINKED_INDIRECTLY);
			}
			break;
		}
		case ID_LT:
		{
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_lattice_eval_geometry,
			                                           _1,
			                                           (Lattice *)obdata_cow),
			                                           DEG_OPCODE_PLACEHOLDER,
			                                           "Geometry Eval");
			op_node->set_as_entry();
			break;
		}
		default:
			BLI_assert(!"Should not happen");
			break;
	}
	op_node = add_operation_node(obdata, DEG_NODE_TYPE_GEOMETRY, NULL,
	                             DEG_OPCODE_PLACEHOLDER, "Eval Done");
	op_node->set_as_exit();
	/* Parameters for driver sources. */
	add_operation_node(obdata,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARAMETERS_EVAL);
	/* Batch cache. */
	add_operation_node(obdata,
	                   DEG_NODE_TYPE_BATCH_CACHE,
	                   function_bind(BKE_object_data_select_update,
	                                 _1,
	                                 obdata_cow),
	                   DEG_OPCODE_GEOMETRY_SELECT_UPDATE);
}

void DepsgraphNodeBuilder::build_armature(bArmature *armature)
{
	if (built_map_.checkIsBuiltAndTag(armature)) {
		return;
	}
	build_animdata(&armature->id);
	/* Make sure pose is up-to-date with armature updates. */
	add_operation_node(&armature->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER,
	                   "Armature Eval");
}

void DepsgraphNodeBuilder::build_camera(Camera *camera)
{
	if (built_map_.checkIsBuiltAndTag(camera)) {
		return;
	}
	OperationDepsNode *op_node;
	build_animdata(&camera->id);
	op_node = add_operation_node(&camera->id,
	                             DEG_NODE_TYPE_PARAMETERS,
	                             NULL,
	                             DEG_OPCODE_PARAMETERS_EVAL);
	op_node->set_as_exit();
}

void DepsgraphNodeBuilder::build_lamp(Lamp *lamp)
{
	if (built_map_.checkIsBuiltAndTag(lamp)) {
		return;
	}
	OperationDepsNode *op_node;
	build_animdata(&lamp->id);
	op_node = add_operation_node(&lamp->id,
	                             DEG_NODE_TYPE_PARAMETERS,
	                             NULL,
	                             DEG_OPCODE_PARAMETERS_EVAL);
	op_node->set_as_exit();
	/* lamp's nodetree */
	build_nodetree(lamp->nodetree);
}

void DepsgraphNodeBuilder::build_nodetree(bNodeTree *ntree)
{
	if (ntree == NULL) {
		return;
	}
	if (built_map_.checkIsBuiltAndTag(ntree)) {
		return;
	}
	/* nodetree itself */
	add_id_node(&ntree->id);
	bNodeTree *ntree_cow = get_cow_datablock(ntree);
	/* Animation, */
	build_animdata(&ntree->id);
	/* Shading update. */
	add_operation_node(&ntree->id,
	                   DEG_NODE_TYPE_SHADING,
	                   NULL,
	                   DEG_OPCODE_MATERIAL_UPDATE);
	/* NOTE: We really pass original and CoW node trees here, this is how the
	 * callback works. Ideally we need to find a better way for that.
	 */
	add_operation_node(&ntree->id,
	                   DEG_NODE_TYPE_SHADING_PARAMETERS,
	                   function_bind(BKE_nodetree_shading_params_eval,
	                                 _1,
	                                 ntree_cow,
	                                 ntree),
	                   DEG_OPCODE_MATERIAL_UPDATE);
	/* nodetree's nodes... */
	LISTBASE_FOREACH (bNode *, bnode, &ntree->nodes) {
		ID *id = bnode->id;
		if (id == NULL) {
			continue;
		}
		ID_Type id_type = GS(id->name);
		if (id_type == ID_MA) {
			build_material((Material *)id);
		}
		else if (id_type == ID_TE) {
			build_texture((Tex *)id);
		}
		else if (id_type == ID_IM) {
			build_image((Image *)id);
		}
		else if (id_type == ID_OB) {
			build_object(-1, (Object *)id, DEG_ID_LINKED_INDIRECTLY);
		}
		else if (id_type == ID_SCE) {
			/* Scenes are used by compositor trees, and handled by render
			 * pipeline. No need to build dependencies for them here.
			 */
		}
		else if (id_type == ID_TXT) {
			/* Ignore script nodes. */
		}
		else if (bnode->type == NODE_GROUP) {
			bNodeTree *group_ntree = (bNodeTree *)id;
			build_nodetree(group_ntree);
		}
		else {
			BLI_assert(!"Unknown ID type used for node");
		}
	}

	// TODO: link from nodetree to owner_component?
}

/* Recursively build graph for material */
void DepsgraphNodeBuilder::build_material(Material *material)
{
	if (built_map_.checkIsBuiltAndTag(material)) {
		return;
	}
	/* Material itself. */
	add_id_node(&material->id);
	Material *material_cow = get_cow_datablock(material);
	/* Shading update. */
	add_operation_node(&material->id,
	                   DEG_NODE_TYPE_SHADING,
	                   function_bind(BKE_material_eval,
	                                 _1,
	                                 material_cow),
	                   DEG_OPCODE_MATERIAL_UPDATE);
	/* Material animation. */
	build_animdata(&material->id);
	/* Material's nodetree. */
	build_nodetree(material->nodetree);
}

/* Recursively build graph for texture */
void DepsgraphNodeBuilder::build_texture(Tex *texture)
{
	if (built_map_.checkIsBuiltAndTag(texture)) {
		return;
	}
	/* Texture itself. */
	build_animdata(&texture->id);
	/* Texture's nodetree. */
	build_nodetree(texture->nodetree);
	/* Special cases for different IDs which texture uses. */
	if (texture->type == TEX_IMAGE) {
		if (texture->ima != NULL) {
			build_image(texture->ima);
		}
	}
	/* Placeholder so we can add relations and tag ID node for update. */
	add_operation_node(&texture->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER);
}

void DepsgraphNodeBuilder::build_image(Image *image) {
	if (built_map_.checkIsBuiltAndTag(image)) {
		return;
	}
	/* Placeholder so we can add relations and tag ID node for update. */
	add_operation_node(&image->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PLACEHOLDER,
	                   "Image Eval");
}

void DepsgraphNodeBuilder::build_compositor(Scene *scene)
{
	/* For now, just a plain wrapper? */
	// TODO: create compositing component?
	// XXX: component type undefined!
	//graph->get_node(&scene->id, NULL, DEG_NODE_TYPE_COMPOSITING, NULL);

	/* for now, nodetrees are just parameters; compositing occurs in internals
	 * of renderer...
	 */
	add_component_node(&scene->id, DEG_NODE_TYPE_PARAMETERS);
	build_nodetree(scene->nodetree);
}

void DepsgraphNodeBuilder::build_gpencil(bGPdata *gpd)
{
	if (built_map_.checkIsBuiltAndTag(gpd)) {
		return;
	}
	ID *gpd_id = &gpd->id;

	/* TODO(sergey): what about multiple users of same datablock? This should
	 * only get added once.
	 */

	/* The main reason Grease Pencil is included here is because the animation
	 * (and drivers) need to be hosted somewhere.
	 */
	build_animdata(gpd_id);
}

void DepsgraphNodeBuilder::build_cachefile(CacheFile *cache_file)
{
	if (built_map_.checkIsBuiltAndTag(cache_file)) {
		return;
	}
	ID *cache_file_id = &cache_file->id;
	/* Animation, */
	build_animdata(cache_file_id);
	/* Cache evaluation itself. */
	add_operation_node(cache_file_id, DEG_NODE_TYPE_CACHE, NULL,
	                   DEG_OPCODE_PLACEHOLDER, "Cache File Update");
}

void DepsgraphNodeBuilder::build_mask(Mask *mask)
{
	if (built_map_.checkIsBuiltAndTag(mask)) {
		return;
	}
	ID *mask_id = &mask->id;
	Mask *mask_cow = get_cow_datablock(mask);
	/* F-Curve based animation. */
	build_animdata(mask_id);
	/* Animation based on mask's shapes. */
	add_operation_node(mask_id,
	                   DEG_NODE_TYPE_ANIMATION,
	                   function_bind(BKE_mask_eval_animation, _1, mask_cow),
	                   DEG_OPCODE_MASK_ANIMATION);
	/* Final mask evaluation. */
	add_operation_node(mask_id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   function_bind(BKE_mask_eval_update, _1, mask_cow),
	                   DEG_OPCODE_MASK_EVAL);
}

void DepsgraphNodeBuilder::build_movieclip(MovieClip *clip)
{
	if (built_map_.checkIsBuiltAndTag(clip)) {
		return;
	}
	ID *clip_id = &clip->id;
	MovieClip *clip_cow = get_cow_datablock(clip);
	/* Animation. */
	build_animdata(clip_id);
	/* Movie clip evaluation. */
	add_operation_node(clip_id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   function_bind(BKE_movieclip_eval_update, _1, clip_cow),
	                   DEG_OPCODE_MOVIECLIP_EVAL);
}

void DepsgraphNodeBuilder::build_lightprobe(LightProbe *probe)
{
	if (built_map_.checkIsBuiltAndTag(probe)) {
		return;
	}
	/* Placeholder so we can add relations and tag ID node for update. */
	add_operation_node(&probe->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_LIGHT_PROBE_EVAL);

	build_animdata(&probe->id);
}

void DepsgraphNodeBuilder::build_speaker(Speaker *speaker)
{
	if (built_map_.checkIsBuiltAndTag(speaker)) {
		return;
	}
	/* Placeholder so we can add relations and tag ID node for update. */
	add_operation_node(&speaker->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_SPEAKER_EVAL);
	build_animdata(&speaker->id);
}

/* **** ID traversal callbacks functions **** */

void DepsgraphNodeBuilder::modifier_walk(void *user_data,
                                         struct Object * /*object*/,
                                         struct ID **idpoin,
                                         int /*cb_flag*/)
{
	BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
	ID *id = *idpoin;
	if (id == NULL) {
		return;
	}
	switch (GS(id->name)) {
		case ID_OB:
			data->builder->build_object(-1,
			                            (Object *)id,
			                            DEG_ID_LINKED_INDIRECTLY);
			break;
		case ID_TE:
			data->builder->build_texture((Tex *)id);
			break;
		default:
			/* pass */
			break;
	}
}

void DepsgraphNodeBuilder::constraint_walk(bConstraint * /*con*/,
                                           ID **idpoin,
                                           bool /*is_reference*/,
                                           void *user_data)
{
	BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
	ID *id = *idpoin;
	if (id == NULL) {
		return;
	}
	switch (GS(id->name)) {
		case ID_OB:
			data->builder->build_object(-1,
			                            (Object *)id,
			                            DEG_ID_LINKED_INDIRECTLY);
			break;
		default:
			/* pass */
			break;
	}
}

}  // namespace DEG
