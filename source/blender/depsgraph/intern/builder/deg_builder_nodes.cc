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
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_effect.h"
#include "BKE_fcurve.h"
#include "BKE_idcode.h"
#include "BKE_group.h"
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
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "util/deg_util_foreach.h"

namespace DEG {

/* ************ */
/* Node Builder */

/* **** General purpose functions **** */

DepsgraphNodeBuilder::DepsgraphNodeBuilder(Main *bmain, Depsgraph *graph)
    : bmain_(bmain),
      graph_(graph),
      scene_(NULL)
{
}

DepsgraphNodeBuilder::~DepsgraphNodeBuilder()
{
}

IDDepsNode *DepsgraphNodeBuilder::add_id_node(ID *id)
{
	return graph_->add_id_node(id, id->name);
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

/* **** Build functions for entity nodes **** */

void DepsgraphNodeBuilder::begin_build() {
}

void DepsgraphNodeBuilder::build_id(ID *id)
{
	if (id == NULL) {
		return;
	}
	switch (GS(id->name)) {
		case ID_SCE:
			build_scene((Scene *)id);
			break;
		case ID_GR:
			build_group(NULL, (Group *)id);
			break;
		case ID_OB:
			build_object(NULL, (Object *)id);
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
		default:
			/* fprintf(stderr, "Unhandled ID %s\n", id->name); */
			break;
	}
}

void DepsgraphNodeBuilder::build_group(Base *base, Group *group)
{
	if (built_map_.checkIsBuiltAndTag(group)) {
		return;
	}
	LISTBASE_FOREACH (GroupObject *, go, &group->gobject) {
		build_object(base, go->ob);
	}
}

void DepsgraphNodeBuilder::build_object(Base *base, Object *object)
{
	const bool has_object = built_map_.checkIsBuiltAndTag(object);
	IDDepsNode *id_node = (has_object)
	        ? graph_->find_id_node(&object->id)
	        : add_id_node(&object->id);
	/* Update node layers.
	 * Do it for both new and existing ID nodes. This is so because several
	 * bases might be sharing same object.
	 */
	if (base != NULL) {
		id_node->layers |= base->lay;
	}
	if (object->type == OB_CAMERA) {
		/* Camera should always be updated, it used directly by viewport.
		 *
		 * TODO(sergey): Make it only for active scene camera.
		 */
		id_node->layers |= (unsigned int)(-1);
	}
	/* Skip rest of components if the ID node was already there. */
	if (has_object) {
		return;
	}
	object->customdata_mask = 0;
	/* Transform. */
	build_object_transform(object);
	/* Parent. */
	if (object->parent != NULL) {
		build_object(NULL, object->parent);
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
	/* Object that this is a proxy for. */
	if (object->proxy) {
		object->proxy->proxy_from = object;
		build_object(base, object->proxy);
	}
	/* Object dupligroup. */
	if (object->dup_group != NULL) {
		build_group(base, object->dup_group);
	}
}

void DepsgraphNodeBuilder::build_object_data(Object *object)
{
	if (object->data == NULL) {
		return;
	}
	IDDepsNode *id_node = graph_->find_id_node(&object->id);
	/* type-specific data... */
	switch (object->type) {
		case OB_MESH:     /* Geometry */
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		case OB_MBALL:
		case OB_LATTICE:
			build_obdata_geom(object);
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
			build_lamp(object);
			break;
		case OB_CAMERA:
			build_camera(object);
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

void DepsgraphNodeBuilder::build_object_transform(Object *object)
{
	OperationDepsNode *op_node;

	/* local transforms (from transform channels - loc/rot/scale + deltas) */
	op_node = add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                             function_bind(BKE_object_eval_local_transform, _1, object),
	                             DEG_OPCODE_TRANSFORM_LOCAL);
	op_node->set_as_entry();

	/* object parent */
	if (object->parent) {
		add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
		                   function_bind(BKE_object_eval_parent, _1, scene_, object),
		                   DEG_OPCODE_TRANSFORM_PARENT);
	}

	/* object constraints */
	if (object->constraints.first) {
		build_object_constraints(object);
	}

	/* Temporary uber-update node, which does everything.
	 * It is for the being we're porting old dependencies into the new system.
	 * We'll get rid of this node as soon as all the granular update functions
	 * are filled in.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                   function_bind(BKE_object_eval_uber_transform, _1, object),
	                   DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL);

	/* object transform is done */
	op_node = add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
	                             function_bind(BKE_object_eval_done, _1, object),
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
	                   function_bind(BKE_object_eval_constraints, _1, scene_, object),
	                   DEG_OPCODE_TRANSFORM_CONSTRAINTS);
}

/**
 * Build graph nodes for AnimData block
 * \param id: ID-Block which hosts the AnimData
 */
void DepsgraphNodeBuilder::build_animdata(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);

	if (adt == NULL)
		return;

	/* animation */
	if (adt->action || adt->nla_tracks.first || adt->drivers.first) {
		// XXX: Hook up specific update callbacks for special properties which may need it...

		/* actions and NLA - as a single unit for now, as it gets complicated to schedule otherwise */
		if ((adt->action) || (adt->nla_tracks.first)) {
			/* create the node */
			add_operation_node(id, DEG_NODE_TYPE_ANIMATION,
			                   function_bind(BKE_animsys_eval_animdata, _1, id),
			                   DEG_OPCODE_ANIMATION, id->name);

			// TODO: for each channel affected, we might also want to add some support for running RNA update callbacks on them
			// (which will be needed for proper handling of drivers later)
		}

		/* drivers */
		LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
			/* create driver */
			build_driver(id, fcu);
		}
	}
}

/**
 * Build graph node(s) for Driver
 * \param id: ID-Block that driver is attached to
 * \param fcu: Driver-FCurve
 */
void DepsgraphNodeBuilder::build_driver(ID *id, FCurve *fcurve)
{
	/* Create data node for this driver */
	ensure_operation_node(id,
	                      DEG_NODE_TYPE_PARAMETERS,
	                      function_bind(BKE_animsys_eval_driver, _1, id, fcurve),
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
			build_id(dtar->id);
			build_driver_id_property(dtar->id, dtar->rna_path);
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
	ID *world_id = &world->id;
	build_animdata(world_id);
	/* world itself */
	add_operation_node(world_id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARAMETERS_EVAL);
	/* textures */
	build_texture_stack(world->mtex);
	/* world's nodetree */
	if (world->nodetree) {
		build_nodetree(world->nodetree);
	}
}

/* Rigidbody Simulation - Scene Level */
void DepsgraphNodeBuilder::build_rigidbody(Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;

	/**
	 * Rigidbody Simulation Nodes
	 * ==========================
	 *
	 * There are 3 nodes related to Rigidbody Simulation:
	 * 1) "Initialize/Rebuild World" - this is called sparingly, only when the simulation
	 *    needs to be rebuilt (mainly after file reload, or moving back to start frame)
	 * 2) "Do Simulation" - perform a simulation step - interleaved between the evaluation
	 *    steps for clusters of objects (i.e. between those affected and/or not affected by
	 *    the sim for instance)
	 *
	 * 3) "Pull Results" - grab the specific transforms applied for a specific object -
	 *    performed as part of object's transform-stack building
	 */

	/* create nodes ------------------------------------------------------------------------ */
	/* XXX: is this the right component, or do we want to use another one instead? */

	/* init/rebuild operation */
	/*OperationDepsNode *init_node =*/ add_operation_node(&scene->id, DEG_NODE_TYPE_TRANSFORM,
	                                                      function_bind(BKE_rigidbody_rebuild_sim, _1, scene),
	                                                      DEG_OPCODE_RIGIDBODY_REBUILD);

	/* do-sim operation */
	// XXX: what happens if we need to split into several groups?
	OperationDepsNode *sim_node     = add_operation_node(&scene->id, DEG_NODE_TYPE_TRANSFORM,
	                                                     function_bind(BKE_rigidbody_eval_simulation, _1, scene),
	                                                     DEG_OPCODE_RIGIDBODY_SIM);

	/* XXX: For now, the sim node is the only one that really matters here. If any other
	 * sims get added later, we may have to remove these hacks...
	 */
	sim_node->owner->entry_operation = sim_node;
	sim_node->owner->exit_operation  = sim_node;


	/* objects - simulation participants */
	if (rbw->group) {
		LISTBASE_FOREACH (GroupObject *, go, &rbw->group->gobject) {
			Object *object = go->ob;

			if (!object || (object->type != OB_MESH))
				continue;

			/* 2) create operation for flushing results */
			/* object's transform component - where the rigidbody operation lives */
			add_operation_node(&object->id, DEG_NODE_TYPE_TRANSFORM,
			                   function_bind(BKE_rigidbody_object_sync_transforms, _1, scene, object),
			                   DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY);
		}
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
	add_operation_node(psys_comp,
	                   function_bind(BKE_particle_system_eval_init,
	                                 _1,
	                                 scene_,
	                                 object),
	                   DEG_OPCODE_PARTICLE_SYSTEM_EVAL_INIT);
	/* Build all particle systems. */
	LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
		ParticleSettings *part = psys->part;
		/* Particle settings. */
		// XXX: what if this is used more than once!
		build_animdata(&part->id);
		/* This particle system evaluation. */
		// TODO: for now, this will just be a placeholder "ubereval" node
		add_operation_node(psys_comp,
		                   NULL,
		                   DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
		                   psys->name);
		/* Visualization of particle system. */
		switch (part->ren_as) {
			case PART_DRAW_OB:
				if (part->dup_ob != NULL) {
					build_object(NULL, part->dup_ob);
				}
				break;
			case PART_DRAW_GR:
				if (part->dup_group != NULL) {
					build_group(NULL, part->dup_group);
				}
				break;
		}
	}

	/* pointcache */
	// TODO...
}

void DepsgraphNodeBuilder::build_cloth(Object *object)
{
	add_operation_node(&object->id,
	                   DEG_NODE_TYPE_CACHE,
	                   function_bind(BKE_object_eval_cloth,
	                                 _1,
	                                 scene_,
	                                 object),
	                   DEG_OPCODE_GEOMETRY_CLOTH_MODIFIER);
}

/* Shapekeys */
void DepsgraphNodeBuilder::build_shapekeys(Key *key)
{
	build_animdata(&key->id);
	add_operation_node(&key->id,
	                   DEG_NODE_TYPE_GEOMETRY,
	                   NULL,
	                   DEG_OPCODE_GEOMETRY_SHAPEKEY);
}

/* ObData Geometry Evaluation */
// XXX: what happens if the datablock is shared!
void DepsgraphNodeBuilder::build_obdata_geom(Object *object)
{
	ID *obdata = (ID *)object->data;
	OperationDepsNode *op_node;

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
	                                           bmain_,
	                                           _1,
	                                           scene_,
	                                           object),
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

	/* materials */
	for (int a = 1; a <= object->totcol; a++) {
		Material *ma = give_current_material(object, a);
		if (ma != NULL) {
			build_material(ma);
		}
	}

	/* geometry collision */
	if (ELEM(object->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}

	if (built_map_.checkIsBuiltAndTag(obdata)) {
		return;
	}

	/* ShapeKeys */
	Key *key = BKE_key_from_object(object);
	if (key) {
		build_shapekeys(key);
	}

	build_animdata(obdata);

	/* Nodes for result of obdata's evaluation, and geometry
	 * evaluation on object.
	 */
	switch (object->type) {
		case OB_MESH:
		{
			//Mesh *me = (Mesh *)object->data;

			/* evaluation operations */
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_mesh_eval_geometry,
			                                           _1,
			                                           (Mesh *)obdata),
			                             DEG_OPCODE_PLACEHOLDER,
			                             "Geometry Eval");
			op_node->set_as_entry();
			break;
		}

		case OB_MBALL:
		{
			Object *mom = BKE_mball_basis_find(bmain_, bmain_->eval_ctx, scene_, object);
			/* NOTE: Only the motherball gets evaluated, it's children are
			 * having empty placeholders for the correct relations being built.
			 */
			if (mom == object) {
				/* metaball evaluation operations */
				op_node = add_operation_node(obdata,
				                             DEG_NODE_TYPE_GEOMETRY,
				                             function_bind(BKE_mball_eval_geometry,
				                                           _1,
				                                           (MetaBall *)obdata),
				                             DEG_OPCODE_PLACEHOLDER,
				                             "Geometry Eval");
			}
			else {
				op_node = add_operation_node(obdata,
				                             DEG_NODE_TYPE_GEOMETRY,
				                             NULL,
				                             DEG_OPCODE_PLACEHOLDER,
				                             "Geometry Eval");
				op_node->set_as_entry();
			}
			break;
		}

		case OB_CURVE:
		case OB_SURF:
		case OB_FONT:
		{
			/* Curve/nurms evaluation operations. */
			/* - calculate curve geometry (including path) */
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_curve_eval_geometry,
			                                           _1,
			                                           (Curve *)obdata),
			                                           DEG_OPCODE_PLACEHOLDER,
			                                           "Geometry Eval");
			op_node->set_as_entry();

			/* Make sure objects used for bevel.taper are in the graph.
			 * NOTE: This objects might be not linked to the scene.
			 */
			Curve *cu = (Curve *)obdata;
			if (cu->bevobj != NULL) {
				build_object(NULL, cu->bevobj);
			}
			if (cu->taperobj != NULL) {
				build_object(NULL, cu->taperobj);
			}
			if (object->type == OB_FONT && cu->textoncurve != NULL) {
				build_object(NULL, cu->textoncurve);
			}
			break;
		}

		case OB_LATTICE:
		{
			/* Lattice evaluation operations. */
			op_node = add_operation_node(obdata,
			                             DEG_NODE_TYPE_GEOMETRY,
			                             function_bind(BKE_lattice_eval_geometry,
			                                           _1,
			                                           (Lattice *)obdata),
			                                           DEG_OPCODE_PLACEHOLDER,
			                                           "Geometry Eval");
			op_node->set_as_entry();
			break;
		}
	}

	op_node = add_operation_node(obdata, DEG_NODE_TYPE_GEOMETRY, NULL,
	                             DEG_OPCODE_PLACEHOLDER, "Eval Done");
	op_node->set_as_exit();

	/* Parameters for driver sources. */
	add_operation_node(obdata,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARAMETERS_EVAL);
}

/* Cameras */
void DepsgraphNodeBuilder::build_camera(Object *object)
{
	/* TODO: Link scene-camera links in somehow... */
	Camera *camera = (Camera *)object->data;
	if (built_map_.checkIsBuiltAndTag(camera)) {
		return;
	}
	build_animdata(&camera->id);
	add_operation_node(&camera->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARAMETERS_EVAL);
	if (camera->dof_ob != NULL) {
		/* TODO(sergey): For now parametrs are on object level. */
		add_operation_node(&object->id, DEG_NODE_TYPE_PARAMETERS, NULL,
		                   DEG_OPCODE_PLACEHOLDER, "Camera DOF");
	}
}

/* Lamps */
void DepsgraphNodeBuilder::build_lamp(Object *object)
{
	Lamp *lamp = (Lamp *)object->data;
	if (built_map_.checkIsBuiltAndTag(lamp)) {
		return;
	}
	build_animdata(&lamp->id);
	/* TODO(sergey): Is it really how we're supposed to work with drivers? */
	add_operation_node(&lamp->id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   NULL,
	                   DEG_OPCODE_PARAMETERS_EVAL);
	/* lamp's nodetree */
	build_nodetree(lamp->nodetree);
	/* textures */
	build_texture_stack(lamp->mtex);
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
	OperationDepsNode *op_node;
	build_animdata(&ntree->id);
	/* Parameters for drivers. */
	op_node = add_operation_node(&ntree->id,
	                             DEG_NODE_TYPE_PARAMETERS,
	                             NULL,
	                             DEG_OPCODE_PARAMETERS_EVAL);
	op_node->set_as_exit();
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
			build_object(NULL, (Object *)id);
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
	add_operation_node(&material->id, DEG_NODE_TYPE_SHADING, NULL,
	                   DEG_OPCODE_PLACEHOLDER, "Material Update");

	/* material animation */
	build_animdata(&material->id);
	/* textures */
	build_texture_stack(material->mtex);
	/* material's nodetree */
	build_nodetree(material->nodetree);
}

/* Texture-stack attached to some shading datablock */
void DepsgraphNodeBuilder::build_texture_stack(MTex **texture_stack)
{
	int i;

	/* for now assume that all texture-stacks have same number of max items */
	for (i = 0; i < MAX_MTEX; i++) {
		MTex *mtex = texture_stack[i];
		if (mtex && mtex->tex)
			build_texture(mtex->tex);
	}
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

	/* for now, nodetrees are just parameters; compositing occurs in internals of renderer... */
	add_component_node(&scene->id, DEG_NODE_TYPE_PARAMETERS);
	build_nodetree(scene->nodetree);
}

void DepsgraphNodeBuilder::build_gpencil(bGPdata *gpd)
{
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
	ID *cache_file_id = &cache_file->id;
	/* Animation, */
	build_animdata(cache_file_id);
	/* Cache evaluation itself. */
	add_operation_node(cache_file_id, DEG_NODE_TYPE_CACHE, NULL,
	                   DEG_OPCODE_PLACEHOLDER, "Cache File Update");
}

void DepsgraphNodeBuilder::build_mask(Mask *mask)
{
	ID *mask_id = &mask->id;
	/* F-Curve based animation. */
	build_animdata(mask_id);
	/* Animation based on mask's shapes. */
	add_operation_node(mask_id,
	                   DEG_NODE_TYPE_ANIMATION,
	                   function_bind(BKE_mask_eval_animation, _1, mask),
	                   DEG_OPCODE_MASK_ANIMATION);
	/* Final mask evaluation. */
	add_operation_node(mask_id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   function_bind(BKE_mask_eval_update, _1, mask),
	                   DEG_OPCODE_MASK_EVAL);
}

void DepsgraphNodeBuilder::build_movieclip(MovieClip *clip) {
	ID *clip_id = &clip->id;
	/* Animation. */
	build_animdata(clip_id);
	/* Movie clip evaluation. */
	add_operation_node(clip_id,
	                   DEG_NODE_TYPE_PARAMETERS,
	                   function_bind(BKE_movieclip_eval_update, _1, clip),
	                   DEG_OPCODE_MOVIECLIP_EVAL);
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
			data->builder->build_object(NULL, (Object *)id);
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
			data->builder->build_object(NULL, (Object *)id);
			break;
		default:
			/* pass */
			break;
	}
}


}  // namespace DEG
