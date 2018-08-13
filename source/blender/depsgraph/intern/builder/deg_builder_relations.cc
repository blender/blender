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

/** \file blender/depsgraph/intern/builder/deg_builder_relations.cc
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
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
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
#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_object_force_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_animsys.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_collision.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_rigidbody.h"
#include "BKE_shader_fx.h"
#include "BKE_sound.h"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/nodes/deg_node_time.h"

#include "intern/depsgraph_intern.h"
#include "intern/depsgraph_types.h"

#include "util/deg_util_foreach.h"

namespace DEG {

/* ***************** */
/* Relations Builder */

/* TODO(sergey): This is somewhat weak, but we don't want neither false-positive
 * time dependencies nor special exceptions in the depsgraph evaluation.
 */
static bool python_driver_depends_on_time(ChannelDriver *driver)
{
	if (driver->expression[0] == '\0') {
		/* Empty expression depends on nothing. */
		return false;
	}
	if (strchr(driver->expression, '(') != NULL) {
		/* Function calls are considered dependent on a time. */
		return true;
	}
	if (strstr(driver->expression, "frame") != NULL) {
		/* Variable `frame` depends on time. */
		/* TODO(sergey): This is a bit weak, but not sure about better way of
		 * handling this.
		 */
		return true;
	}
	/* Possible indirect time relation s should be handled via variable
	 * targets.
	 */
	return false;
}

static bool particle_system_depends_on_time(ParticleSystem *psys)
{
	ParticleSettings *part = psys->part;
	/* Non-hair particles we always consider dependent on time. */
	if (part->type != PART_HAIR) {
		return true;
	}
	/* Dynamics always depends on time. */
	if (psys->flag & PSYS_HAIR_DYNAMICS) {
		return true;
	}
	/* TODO(sergey): Check what else makes hair dependent on time. */
	return false;
}

static bool object_particles_depends_on_time(Object *object)
{
	if (object->type != OB_MESH) {
		return false;
	}
	LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
		if (particle_system_depends_on_time(psys)) {
			return true;
		}
	}
	return false;
}

static bool check_id_has_anim_component(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	if (adt == NULL) {
		return false;
	}
	return (adt->action != NULL) ||
	       (!BLI_listbase_is_empty(&adt->nla_tracks));
}

/* **** General purpose functions ****  */

DepsgraphRelationBuilder::DepsgraphRelationBuilder(Main *bmain,
                                                   Depsgraph *graph)
    : bmain_(bmain),
      graph_(graph),
      scene_(NULL)
{
}

TimeSourceDepsNode *DepsgraphRelationBuilder::get_node(
        const TimeSourceKey &key) const
{
	if (key.id) {
		/* XXX TODO */
		return NULL;
	}
	else {
		return graph_->time_source;
	}
}

ComponentDepsNode *DepsgraphRelationBuilder::get_node(
        const ComponentKey &key) const
{
	IDDepsNode *id_node = graph_->find_id_node(key.id);
	if (!id_node) {
		fprintf(stderr, "find_node component: Could not find ID %s\n",
		        (key.id != NULL) ? key.id->name : "<null>");
		return NULL;
	}

	ComponentDepsNode *node = id_node->find_component(key.type, key.name);
	return node;
}

OperationDepsNode *DepsgraphRelationBuilder::get_node(
        const OperationKey &key) const
{
	OperationDepsNode *op_node = find_node(key);
	if (op_node == NULL) {
		fprintf(stderr, "find_node_operation: Failed for (%s, '%s')\n",
		        operationCodeAsString(key.opcode), key.name);
	}
	return op_node;
}

DepsNode *DepsgraphRelationBuilder::get_node(const RNAPathKey &key) const
{
	return graph_->find_node_from_pointer(&key.ptr, key.prop);
}

OperationDepsNode *DepsgraphRelationBuilder::find_node(
        const OperationKey &key) const
{
	IDDepsNode *id_node = graph_->find_id_node(key.id);
	if (!id_node) {
		return NULL;
	}
	ComponentDepsNode *comp_node = id_node->find_component(key.component_type,
	                                                       key.component_name);
	if (!comp_node) {
		return NULL;
	}
	return comp_node->find_operation(key.opcode, key.name, key.name_tag);
}

bool DepsgraphRelationBuilder::has_node(const OperationKey &key) const
{
	return find_node(key) != NULL;
}

DepsRelation *DepsgraphRelationBuilder::add_time_relation(
        TimeSourceDepsNode *timesrc,
        DepsNode *node_to,
        const char *description,
        bool check_unique)
{
	if (timesrc && node_to) {
		return graph_->add_new_relation(timesrc, node_to, description, check_unique);
	}
	else {
		DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
		                 BUILD, "add_time_relation(%p = %s, %p = %s, %s) Failed\n",
		                 timesrc,   (timesrc) ? timesrc->identifier().c_str() : "<None>",
		                 node_to,   (node_to) ? node_to->identifier().c_str() : "<None>",
		                 description);
	}
	return NULL;
}

DepsRelation *DepsgraphRelationBuilder::add_operation_relation(
        OperationDepsNode *node_from,
        OperationDepsNode *node_to,
        const char *description,
        bool check_unique)
{
	if (node_from && node_to) {
		return graph_->add_new_relation(node_from,
		                                node_to,
		                                description,
		                                check_unique);
	}
	else {
		DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
		                 BUILD, "add_operation_relation(%p = %s, %p = %s, %s) Failed\n",
		                 node_from, (node_from) ? node_from->identifier().c_str() : "<None>",
		                 node_to,   (node_to)   ? node_to->identifier().c_str() : "<None>",
		                 description);
	}
	return NULL;
}

void DepsgraphRelationBuilder::add_collision_relations(
        const OperationKey &key,
        Object *object,
        Collection *collection,
        const char *name)
{
	ListBase *relations = deg_build_collision_relations(graph_, collection, eModifierType_Collision);

	LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
		if (relation->ob != object) {
			ComponentKey trf_key(&relation->ob->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(trf_key, key, name);

			ComponentKey coll_key(&relation->ob->id, DEG_NODE_TYPE_GEOMETRY);
			add_relation(coll_key, key, name);
		}
	}
}

void DepsgraphRelationBuilder::add_forcefield_relations(
        const OperationKey &key,
        Object *object,
        ParticleSystem *psys,
        EffectorWeights *eff,
        bool add_absorption,
        const char *name)
{
	ListBase *relations = deg_build_effector_relations(graph_, eff->group);

	LISTBASE_FOREACH (EffectorRelation *, relation, relations) {
		if (relation->ob != object) {
			ComponentKey eff_key(&relation->ob->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(eff_key, key, name);

			if (relation->pd->forcefield == PFIELD_SMOKEFLOW && relation->pd->f_source) {
				ComponentKey trf_key(&relation->pd->f_source->id,
				                     DEG_NODE_TYPE_TRANSFORM);
				add_relation(trf_key, key, "Smoke Force Domain");
				ComponentKey eff_key(&relation->pd->f_source->id,
				                     DEG_NODE_TYPE_GEOMETRY);
				add_relation(eff_key, key, "Smoke Force Domain");
			}
			if (add_absorption && (relation->pd->flag & PFIELD_VISIBILITY)) {
				add_collision_relations(key,
				                        object,
				                        NULL,
				                        "Force Absorption");
			}
		}
		if (relation->psys) {
			if (relation->ob != object) {
				ComponentKey eff_key(&relation->ob->id, DEG_NODE_TYPE_EVAL_PARTICLES);
				add_relation(eff_key, key, name);
				/* TODO: remove this when/if EVAL_PARTICLES is sufficient
				 * for up to date particles.
				 */
				ComponentKey mod_key(&relation->ob->id, DEG_NODE_TYPE_GEOMETRY);
				add_relation(mod_key, key, name);
			}
			else if (relation->psys != psys) {
				OperationKey eff_key(&relation->ob->id,
				                     DEG_NODE_TYPE_EVAL_PARTICLES,
				                     DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
				                     relation->psys->name);
				add_relation(eff_key, key, name);
			}
		}
	}
}

Depsgraph *DepsgraphRelationBuilder::getGraph()
{
	return graph_;
}

/* **** Functions to build relations between entities  **** */

void DepsgraphRelationBuilder::begin_build()
{
}

void DepsgraphRelationBuilder::build_id(ID *id)
{
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
			build_collection(DEG_COLLECTION_OWNER_UNKNOWN, NULL, (Collection *)id);
			break;
		case ID_OB:
			build_object(NULL, (Object *)id);
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

void DepsgraphRelationBuilder::build_collection(
        eDepsNode_CollectionOwner owner_type,
        Object *object,
        Collection *collection)
{
	const bool allow_restrict_flags = (owner_type == DEG_COLLECTION_OWNER_SCENE);
	if (allow_restrict_flags) {
		const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT)
		        ? COLLECTION_RESTRICT_VIEW
		        : COLLECTION_RESTRICT_RENDER;
		if (collection->flag & restrict_flag) {
			return;
		}
	}
	const bool group_done = built_map_.checkIsBuiltAndTag(collection);
	OperationKey object_transform_final_key(object != NULL ? &object->id : NULL,
	                                        DEG_NODE_TYPE_TRANSFORM,
	                                        DEG_OPCODE_TRANSFORM_FINAL);
	if (!group_done) {
		LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
			if (allow_restrict_flags) {
				const int restrict_flag = (graph_->mode == DAG_EVAL_VIEWPORT)
				        ? OB_RESTRICT_VIEW
				        : OB_RESTRICT_RENDER;
				if (cob->ob->restrictflag & restrict_flag) {
					continue;
				}
			}
			build_object(NULL, cob->ob);
		}
		LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
			build_collection(owner_type, NULL, child->collection);
		}
	}
	if (object != NULL) {
		FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN(collection, ob, graph_->mode)
		{
			ComponentKey dupli_transform_key(&ob->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(dupli_transform_key, object_transform_final_key, "Dupligroup");
		}
		FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
	}
}

void DepsgraphRelationBuilder::build_object(Base *base, Object *object)
{
	if (built_map_.checkIsBuiltAndTag(object)) {
		if (base != NULL) {
			build_object_flags(base, object);
		}
		return;
	}
	/* Object Transforms */
	eDepsOperation_Code base_op = (object->parent) ? DEG_OPCODE_TRANSFORM_PARENT
	                                               : DEG_OPCODE_TRANSFORM_LOCAL;
	OperationKey base_op_key(&object->id, DEG_NODE_TYPE_TRANSFORM, base_op);
	OperationKey local_transform_key(&object->id,
	                                 DEG_NODE_TYPE_TRANSFORM,
	                                 DEG_OPCODE_TRANSFORM_LOCAL);
	OperationKey parent_transform_key(&object->id,
	                                  DEG_NODE_TYPE_TRANSFORM,
	                                  DEG_OPCODE_TRANSFORM_PARENT);
	OperationKey final_transform_key(&object->id,
	                                 DEG_NODE_TYPE_TRANSFORM,
	                                 DEG_OPCODE_TRANSFORM_FINAL);
	OperationKey ob_ubereval_key(&object->id,
	                             DEG_NODE_TYPE_TRANSFORM,
	                             DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL);
	/* Various flags, flushing from bases/collections. */
	build_object_flags(base, object);
	/* Parenting. */
	if (object->parent != NULL) {
		/* Make sure parent object's relations are built. */
		build_object(NULL, object->parent);
		/* Parent relationship. */
		build_object_parent(object);
		/* Local -> parent. */
		add_relation(local_transform_key,
		             parent_transform_key,
		             "ObLocal -> ObParent");
	}
	/* Modifiers. */
	if (object->modifiers.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		modifiers_foreachIDLink(object, modifier_walk, &data);
	}
	/* Grease Pencil Modifiers. */
	if (object->greasepencil_modifiers.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		BKE_gpencil_modifiers_foreachIDLink(object, modifier_walk, &data);
	}
	/* Shader FX. */
	if (object->shader_fx.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		BKE_shaderfx_foreachIDLink(object, modifier_walk, &data);
	}
	/* Constraints. */
	if (object->constraints.first != NULL) {
		BuilderWalkUserData data;
		data.builder = this;
		BKE_constraints_id_loop(&object->constraints, constraint_walk, &data);
	}
	/* Object constraints. */
	if (object->constraints.first != NULL) {
		OperationKey constraint_key(&object->id,
		                            DEG_NODE_TYPE_TRANSFORM,
		                            DEG_OPCODE_TRANSFORM_CONSTRAINTS);
		/* Constraint relations. */
		build_constraints(&object->id,
		                  DEG_NODE_TYPE_TRANSFORM,
		                  "",
		                  &object->constraints,
		                  NULL);
		/* operation order */
		add_relation(base_op_key, constraint_key, "ObBase-> Constraint Stack");
		add_relation(constraint_key, final_transform_key, "ObConstraints -> Done");
		// XXX
		add_relation(constraint_key, ob_ubereval_key, "Temp Ubereval");
		add_relation(ob_ubereval_key, final_transform_key, "Temp Ubereval");
	}
	else {
		/* NOTE: Keep an eye here, we skip some relations here to "streamline"
		 * dependencies and avoid transitive relations which causes overhead.
		 * But once we get rid of uber eval node this will need reconsideration.
		 */
		if (object->rigidbody_object == NULL) {
			/* Rigid body will hook up another node inbetween, so skip
			 * relation here to avoid transitive relation.
			 */
			add_relation(base_op_key, ob_ubereval_key, "Temp Ubereval");
		}
		add_relation(ob_ubereval_key, final_transform_key, "Temp Ubereval");
	}
	/* Animation data */
	build_animdata(&object->id);
	/* Object data. */
	build_object_data(object);
	/* Particle systems. */
	if (object->particlesystem.first != NULL) {
		build_particles(object);
	}
	/* Proxy object to copy from. */
	if (object->proxy_from != NULL) {
		build_object(NULL, object->proxy_from);
		ComponentKey ob_transform_key(&object->proxy_from->id, DEG_NODE_TYPE_TRANSFORM);
		ComponentKey proxy_transform_key(&object->id, DEG_NODE_TYPE_TRANSFORM);
		add_relation(ob_transform_key, proxy_transform_key, "Proxy Transform");
	}
	if (object->proxy_group != NULL) {
		build_object(NULL, object->proxy_group);
		OperationKey proxy_group_ubereval_key(&object->proxy_group->id,
		                                      DEG_NODE_TYPE_TRANSFORM,
		                                      DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL);
		add_relation(proxy_group_ubereval_key, final_transform_key, "Proxy Group Transform");
	}
	/* Object dupligroup. */
	if (object->dup_group != NULL) {
		build_collection(DEG_COLLECTION_OWNER_OBJECT, object, object->dup_group);
	}
}

void DepsgraphRelationBuilder::build_object_flags(Base *base, Object *object)
{
	if (base == NULL) {
		return;
	}
	OperationKey view_layer_done_key(&scene_->id,
	                                 DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                                 DEG_OPCODE_VIEW_LAYER_EVAL);
	OperationKey object_flags_key(&object->id,
	                              DEG_NODE_TYPE_OBJECT_FROM_LAYER,
	                              DEG_OPCODE_OBJECT_BASE_FLAGS);
	add_relation(view_layer_done_key, object_flags_key, "Base flags flush");
}

void DepsgraphRelationBuilder::build_object_data(Object *object)
{
	if (object->data == NULL) {
		return;
	}
	ID *obdata_id = (ID *)object->data;
	/* Object data animation. */
	if (!built_map_.checkIsBuilt(obdata_id)) {
		build_animdata(obdata_id);
	}
	/* type-specific data. */
	switch (object->type) {
		case OB_MESH:
		case OB_CURVE:
		case OB_FONT:
		case OB_SURF:
		case OB_MBALL:
		case OB_LATTICE:
		case OB_GPENCIL:
		{
			build_object_data_geometry(object);
			break;
		}
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
	}
	Key *key = BKE_key_from_object(object);
	if (key != NULL) {
		ComponentKey geometry_key((ID *)object->data, DEG_NODE_TYPE_GEOMETRY);
		ComponentKey key_key(&key->id, DEG_NODE_TYPE_GEOMETRY);
		add_relation(key_key, geometry_key, "Shapekeys");
		build_nested_shapekey(&object->id, key);
	}
}

void DepsgraphRelationBuilder::build_object_data_camera(Object *object)
{
	Camera *camera = (Camera *)object->data;
	build_camera(camera);
	ComponentKey object_parameters_key(&object->id, DEG_NODE_TYPE_PARAMETERS);
	ComponentKey camera_parameters_key(&camera->id, DEG_NODE_TYPE_PARAMETERS);
	add_relation(camera_parameters_key, object_parameters_key, "Camera -> Object");
}

void DepsgraphRelationBuilder::build_object_data_lamp(Object *object)
{
	Lamp *lamp = (Lamp *)object->data;
	build_lamp(lamp);
	ComponentKey object_parameters_key(&object->id, DEG_NODE_TYPE_PARAMETERS);
	ComponentKey lamp_parameters_key(&lamp->id, DEG_NODE_TYPE_PARAMETERS);
	add_relation(lamp_parameters_key, object_parameters_key, "Light -> Object");
}

void DepsgraphRelationBuilder::build_object_data_lightprobe(Object *object)
{
	LightProbe *probe = (LightProbe *)object->data;
	build_lightprobe(probe);
	OperationKey probe_key(&probe->id,
	                       DEG_NODE_TYPE_PARAMETERS,
	                       DEG_OPCODE_LIGHT_PROBE_EVAL);
	OperationKey object_key(&object->id,
	                        DEG_NODE_TYPE_PARAMETERS,
	                        DEG_OPCODE_LIGHT_PROBE_EVAL);
	add_relation(probe_key, object_key, "LightProbe Update");
}

void DepsgraphRelationBuilder::build_object_data_speaker(Object *object)
{
	Speaker *speaker = (Speaker *)object->data;
	build_speaker(speaker);
	OperationKey probe_key(&speaker->id,
	                       DEG_NODE_TYPE_PARAMETERS,
	                       DEG_OPCODE_SPEAKER_EVAL);
	OperationKey object_key(&object->id,
	                        DEG_NODE_TYPE_PARAMETERS,
	                        DEG_OPCODE_SPEAKER_EVAL);
	add_relation(probe_key, object_key, "Speaker Update");
}

void DepsgraphRelationBuilder::build_object_parent(Object *object)
{
	/* XXX: for now, need to use the component key (not just direct to the parent op),
	 * or else the matrix doesn't get reset/
	 */
	// XXX: @sergey - it would be good if we got that backwards flushing working
	// when tagging for updates.
	//OperationKey ob_key(&object->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_TRANSFORM_PARENT);
	ComponentKey ob_key(&object->id, DEG_NODE_TYPE_TRANSFORM);

	/* type-specific links */
	switch (object->partype) {
		case PARSKEL:  /* Armature Deform (Virtual Modifier) */
		{
			ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(parent_key, ob_key, "Armature Deform Parent");
			break;
		}

		case PARVERT1: /* Vertex Parent */
		case PARVERT3:
		{
			ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_GEOMETRY);
			add_relation(parent_key, ob_key, "Vertex Parent");

			/* XXX not sure what this is for or how you could be done properly - lukas */
			OperationDepsNode *parent_node = find_operation_node(parent_key);
			if (parent_node != NULL) {
				parent_node->customdata_mask |= CD_MASK_ORIGINDEX;
			}

			ComponentKey transform_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(transform_key, ob_key, "Vertex Parent TFM");
			break;
		}

		case PARBONE: /* Bone Parent */
		{
			ComponentKey parent_bone_key(&object->parent->id,
			                             DEG_NODE_TYPE_BONE,
			                             object->parsubstr);
			OperationKey parent_transform_key(&object->parent->id,
			                                  DEG_NODE_TYPE_TRANSFORM,
			                                  DEG_OPCODE_TRANSFORM_FINAL);
			add_relation(parent_bone_key, ob_key, "Bone Parent");
			add_relation(parent_transform_key, ob_key, "Armature Parent");
			break;
		}

		default:
		{
			if (object->parent->type == OB_LATTICE) {
				/* Lattice Deform Parent - Virtual Modifier */
				// XXX: no virtual modifiers should be left!
				ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
				ComponentKey geom_key(&object->parent->id, DEG_NODE_TYPE_GEOMETRY);

				add_relation(parent_key, ob_key, "Lattice Deform Parent");
				add_relation(geom_key, ob_key, "Lattice Deform Parent Geom");
			}
			else if (object->parent->type == OB_CURVE) {
				Curve *cu = (Curve *)object->parent->data;

				if (cu->flag & CU_PATH) {
					/* Follow Path */
					ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_GEOMETRY);
					add_relation(parent_key, ob_key, "Curve Follow Parent");

					ComponentKey transform_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
					add_relation(transform_key, ob_key, "Curve Follow TFM");
				}
				else {
					/* Standard Parent */
					ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
					add_relation(parent_key, ob_key, "Curve Parent");
				}
			}
			else {
				/* Standard Parent */
				ComponentKey parent_key(&object->parent->id, DEG_NODE_TYPE_TRANSFORM);
				add_relation(parent_key, ob_key, "Parent");
			}
			break;
		}
	}

	/* exception case: parent is duplivert */
	if ((object->type == OB_MBALL) && (object->parent->transflag & OB_DUPLIVERTS)) {
		//dag_add_relation(dag, node2, node, DAG_RL_DATA_DATA | DAG_RL_OB_OB, "Duplivert");
	}
}

void DepsgraphRelationBuilder::build_constraints(ID *id,
                                                 eDepsNode_Type component_type,
                                                 const char *component_subdata,
                                                 ListBase *constraints,
                                                 RootPChanMap *root_map)
{
	OperationKey constraint_op_key(
	        id,
	        component_type,
	        component_subdata,
	        (component_type == DEG_NODE_TYPE_BONE)
	                ? DEG_OPCODE_BONE_CONSTRAINTS
	                : DEG_OPCODE_TRANSFORM_CONSTRAINTS);
	/* Add dependencies for each constraint in turn. */
	for (bConstraint *con = (bConstraint *)constraints->first; con; con = con->next) {
		const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		/* Invalid constraint type. */
		if (cti == NULL) {
			continue;
		}
		/* Special case for camera tracking -- it doesn't use targets to
		 * define relations.
		 */
		/* TODO: we can now represent dependencies in a much richer manner,
		 * so review how this is done.
		 */
		if (ELEM(cti->type,
		         CONSTRAINT_TYPE_FOLLOWTRACK,
		         CONSTRAINT_TYPE_CAMERASOLVER,
		         CONSTRAINT_TYPE_OBJECTSOLVER))
		{
			bool depends_on_camera = false;
			if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
				if (((data->clip) ||
				     (data->flag & FOLLOWTRACK_ACTIVECLIP)) && data->track[0])
				{
					depends_on_camera = true;
				}
				if (data->depth_ob) {
					ComponentKey depth_transform_key(&data->depth_ob->id,
					                                 DEG_NODE_TYPE_TRANSFORM);
					ComponentKey depth_geometry_key(&data->depth_ob->id,
					                                DEG_NODE_TYPE_GEOMETRY);
					add_relation(depth_transform_key, constraint_op_key, cti->name);
					add_relation(depth_geometry_key, constraint_op_key, cti->name);
				}
			}
			else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
				depends_on_camera = true;
			}
			if (depends_on_camera && scene_->camera != NULL) {
				ComponentKey camera_key(&scene_->camera->id, DEG_NODE_TYPE_TRANSFORM);
				add_relation(camera_key, constraint_op_key, cti->name);
			}
			/* TODO(sergey): This is more a TimeSource -> MovieClip ->
			 * Constraint dependency chain.
			 */
			TimeSourceKey time_src_key;
			add_relation(time_src_key, constraint_op_key, "TimeSrc -> Animation");
		}
		else if (cti->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
			/* TODO(kevin): This is more a TimeSource -> CacheFile -> Constraint
			 * dependency chain.
			 */
			TimeSourceKey time_src_key;
			add_relation(time_src_key, constraint_op_key, "TimeSrc -> Animation");
			bTransformCacheConstraint *data = (bTransformCacheConstraint *)con->data;
			if (data->cache_file) {
				ComponentKey cache_key(&data->cache_file->id, DEG_NODE_TYPE_CACHE);
				add_relation(cache_key, constraint_op_key, cti->name);
			}
		}
		else if (cti->get_constraint_targets) {
			ListBase targets = {NULL, NULL};
			cti->get_constraint_targets(con, &targets);
			LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
				if (ct->tar == NULL) {
					continue;
				}
				if (ELEM(con->type,
				         CONSTRAINT_TYPE_KINEMATIC,
				         CONSTRAINT_TYPE_SPLINEIK))
				{
					/* Ignore IK constraints - these are handled separately
					 * (on pose level).
					 */
				}
				else if (ELEM(con->type,
				              CONSTRAINT_TYPE_FOLLOWPATH,
				              CONSTRAINT_TYPE_CLAMPTO))
				{
					/* These constraints require path geometry data. */
					ComponentKey target_key(&ct->tar->id, DEG_NODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, cti->name);
					ComponentKey target_transform_key(&ct->tar->id,
					                                  DEG_NODE_TYPE_TRANSFORM);
					add_relation(target_transform_key, constraint_op_key, cti->name);
				}
				else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
					/* bone */
					if (&ct->tar->id == id) {
						/* same armature  */
						eDepsOperation_Code target_key_opcode;
						/* Using "done" here breaks in-chain deps, while using
						 * "ready" here breaks most production rigs instead.
						 * So, we do a compromise here, and only do this when an
						 * IK chain conflict may occur.
						 */
						if (root_map->has_common_root(component_subdata,
						                              ct->subtarget))
						{
							target_key_opcode = DEG_OPCODE_BONE_READY;
						}
						else {
							target_key_opcode = DEG_OPCODE_BONE_DONE;
						}
						OperationKey target_key(&ct->tar->id,
						                        DEG_NODE_TYPE_BONE,
						                        ct->subtarget,
						                        target_key_opcode);
						add_relation(target_key, constraint_op_key, cti->name);
					}
					else {
						/* Different armature - we can safely use the result
						 * of that.
						 */
						OperationKey target_key(&ct->tar->id,
						                        DEG_NODE_TYPE_BONE,
						                        ct->subtarget,
						                        DEG_OPCODE_BONE_DONE);
						add_relation(target_key, constraint_op_key, cti->name);
					}
				}
				else if (ELEM(ct->tar->type, OB_MESH, OB_LATTICE) &&
				         (ct->subtarget[0]))
				{
					/* Vertex group. */
					/* NOTE: for now, we don't need to represent vertex groups
					 * separately.
					 */
					ComponentKey target_key(&ct->tar->id, DEG_NODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, cti->name);
					if (ct->tar->type == OB_MESH) {
						OperationDepsNode *node2 = find_operation_node(target_key);
						if (node2 != NULL) {
							node2->customdata_mask |= CD_MASK_MDEFORMVERT;
						}
					}
				}
				else if (con->type == CONSTRAINT_TYPE_SHRINKWRAP) {
					/* Constraints which requires the target object surface. */
					ComponentKey target_key(&ct->tar->id, DEG_NODE_TYPE_GEOMETRY);
					add_relation(target_key, constraint_op_key, cti->name);
					/* NOTE: obdata eval now doesn't necessarily depend on the
					 * object's transform.
					 */
					ComponentKey target_transform_key(&ct->tar->id,
					                                  DEG_NODE_TYPE_TRANSFORM);
					add_relation(target_transform_key, constraint_op_key, cti->name);
				}
				else {
					/* Standard object relation. */
					// TODO: loc vs rot vs scale?
					if (&ct->tar->id == id) {
						/* Constraint targetting own object:
						 * - This case is fine IFF we're dealing with a bone
						 *   constraint pointing to its own armature. In that
						 *   case, it's just transform -> bone.
						 * - If however it is a real self targetting case, just
						 *   make it depend on the previous constraint (or the
						 *   pre-constraint state).
						 */
						if ((ct->tar->type == OB_ARMATURE) &&
						    (component_type == DEG_NODE_TYPE_BONE))
						{
							OperationKey target_key(&ct->tar->id,
							                        DEG_NODE_TYPE_TRANSFORM,
							                        DEG_OPCODE_TRANSFORM_FINAL);
							add_relation(target_key, constraint_op_key, cti->name);
						}
						else {
							OperationKey target_key(&ct->tar->id,
							                        DEG_NODE_TYPE_TRANSFORM,
							                        DEG_OPCODE_TRANSFORM_LOCAL);
							add_relation(target_key, constraint_op_key, cti->name);
						}
					}
					else {
						/* Normal object dependency. */
						OperationKey target_key(&ct->tar->id,
						                        DEG_NODE_TYPE_TRANSFORM,
						                        DEG_OPCODE_TRANSFORM_FINAL);
						add_relation(target_key, constraint_op_key, cti->name);
					}
				}
				/* Constraints which needs world's matrix for transform.
				 * TODO(sergey): More constraints here?
				 */
				if (ELEM(con->type,
				         CONSTRAINT_TYPE_ROTLIKE,
				         CONSTRAINT_TYPE_SIZELIKE,
				         CONSTRAINT_TYPE_LOCLIKE,
				         CONSTRAINT_TYPE_TRANSLIKE))
				{
					/* TODO(sergey): Add used space check. */
					ComponentKey target_transform_key(&ct->tar->id,
					                                  DEG_NODE_TYPE_TRANSFORM);
					add_relation(target_transform_key, constraint_op_key, cti->name);
				}
			}
			if (cti->flush_constraint_targets) {
				cti->flush_constraint_targets(con, &targets, 1);
			}
		}
	}
}

void DepsgraphRelationBuilder::build_animdata(ID *id)
{
	/* Animation curves and NLA. */
	build_animdata_curves(id);
	/* Drivers. */
	build_animdata_drivers(id);
}

void DepsgraphRelationBuilder::build_animdata_curves(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	if (adt == NULL) {
		return;
	}
	if (adt->action != NULL) {
		build_action(adt->action);
	}
	if (adt->action == NULL && adt->nla_tracks.first == NULL) {
		return;
	}
	/* Wire up dependency to time source. */
	ComponentKey adt_key(id, DEG_NODE_TYPE_ANIMATION);
	/* Relation from action itself. */
	if (adt->action != NULL) {
		ComponentKey action_key(&adt->action->id, DEG_NODE_TYPE_ANIMATION);
		add_relation(action_key, adt_key, "Action -> Animation");
	}
	/* Get source operations. */
	DepsNode *node_from = get_node(adt_key);
	BLI_assert(node_from != NULL);
	if (node_from == NULL) {
		return;
	}
	OperationDepsNode *operation_from = node_from->get_exit_operation();
	BLI_assert(operation_from != NULL);
	/* Build relations from animation operation to properties it changes. */
	if (adt->action != NULL) {
		build_animdata_curves_targets(id, adt_key,
	                              operation_from,
	                              &adt->action->curves);
	}
	LISTBASE_FOREACH(NlaTrack *, nlt, &adt->nla_tracks) {
		build_animdata_nlastrip_targets(id, adt_key,
		                                operation_from,
		                                &nlt->strips);
	}
}

void DepsgraphRelationBuilder::build_animdata_curves_targets(
        ID *id, ComponentKey &adt_key,
        OperationDepsNode *operation_from,
        ListBase *curves)
{
	/* Iterate over all curves and build relations. */
	PointerRNA id_ptr;
	RNA_id_pointer_create(id, &id_ptr);
	LISTBASE_FOREACH(FCurve *, fcu, curves) {
		PointerRNA ptr;
		PropertyRNA *prop;
		int index;
		if (!RNA_path_resolve_full(&id_ptr, fcu->rna_path,
		                           &ptr, &prop, &index))
		{
			continue;
		}
		DepsNode *node_to = graph_->find_node_from_pointer(&ptr, prop);
		if (node_to == NULL) {
			continue;
		}
		OperationDepsNode *operation_to = node_to->get_entry_operation();
		/* NOTE: Special case for bones, avoid relation from animation to
		 * each of the bones. Bone evaluation could only start from pose
		 * init anyway.
		 */
		if (operation_to->opcode == DEG_OPCODE_BONE_LOCAL) {
			OperationKey pose_init_key(id,
			                           DEG_NODE_TYPE_EVAL_POSE,
			                           DEG_OPCODE_POSE_INIT);
			add_relation(adt_key, pose_init_key, "Animation -> Prop", true);
			continue;
		}
		graph_->add_new_relation(operation_from, operation_to,
		                         "Animation -> Prop",
		                         true);
		/* It is possible that animation is writing to a nested ID datablock,
		 * need to make sure animation is evaluated after target ID is copied.
		 */
		const IDDepsNode *id_node_from = operation_from->owner->owner;
		const IDDepsNode *id_node_to = operation_to->owner->owner;
		if (id_node_from != id_node_to) {
			ComponentKey cow_key(id_node_to->id_orig,
			                     DEG_NODE_TYPE_COPY_ON_WRITE);
			add_relation(cow_key, adt_key, "Target CoW -> Animation", true);
		}
	}
}

void DepsgraphRelationBuilder::build_animdata_nlastrip_targets(
        ID *id, ComponentKey &adt_key,
        OperationDepsNode *operation_from,
        ListBase *strips)
{
	LISTBASE_FOREACH(NlaStrip *, strip, strips) {
		if (strip->act != NULL) {
			build_animdata_curves_targets(id, adt_key,
			                              operation_from,
			                              &strip->act->curves);
		}
		else if (strip->strips.first != NULL) {
			build_animdata_nlastrip_targets(id, adt_key,
			                                operation_from,
			                                &strip->strips);
		}
	}
}

void DepsgraphRelationBuilder::build_animdata_drivers(ID *id)
{
	AnimData *adt = BKE_animdata_from_id(id);
	if (adt == NULL) {
		return;
	}
	ComponentKey adt_key(id, DEG_NODE_TYPE_ANIMATION);
	LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
		OperationKey driver_key(id,
		                        DEG_NODE_TYPE_PARAMETERS,
		                        DEG_OPCODE_DRIVER,
		                        fcu->rna_path ? fcu->rna_path : "",
		                        fcu->array_index);

		/* create the driver's relations to targets */
		build_driver(id, fcu);
		/* Special case for array drivers: we can not multithread them because
		 * of the way how they work internally: animation system will write the
		 * whole array back to RNA even when changing individual array value.
		 *
		 * Some tricky things here:
		 * - array_index is -1 for single channel drivers, meaning we only have
		 *   to do some magic when array_index is not -1.
		 * - We do relation from next array index to a previous one, so we don't
		 *   have to deal with array index 0.
		 *
		 * TODO(sergey): Avoid liner lookup somehow.
		 */
		if (fcu->array_index > 0) {
			FCurve *fcu_prev = NULL;
			LISTBASE_FOREACH (FCurve *, fcu_candidate, &adt->drivers) {
				/* Writing to different RNA paths is  */
				const char *rna_path = fcu->rna_path ? fcu->rna_path : "";
				if (!STREQ(fcu_candidate->rna_path, rna_path)) {
					continue;
				}
				/* We only do relation from previous fcurve to previous one. */
				if (fcu_candidate->array_index >= fcu->array_index) {
					continue;
				}
				/* Choose fcurve with highest possible array index. */
				if (fcu_prev == NULL ||
				    fcu_candidate->array_index > fcu_prev->array_index)
				{
					fcu_prev = fcu_candidate;
				}
			}
			if (fcu_prev != NULL) {
				OperationKey prev_driver_key(id,
				                             DEG_NODE_TYPE_PARAMETERS,
				                             DEG_OPCODE_DRIVER,
				                             fcu_prev->rna_path ? fcu_prev->rna_path : "",
				                             fcu_prev->array_index);
				OperationKey driver_key(id,
				                        DEG_NODE_TYPE_PARAMETERS,
				                        DEG_OPCODE_DRIVER,
				                        fcu->rna_path ? fcu->rna_path : "",
				                        fcu->array_index);
				add_relation(prev_driver_key, driver_key, "Driver Order");
			}
		}

		/* prevent driver from occurring before own animation... */
		if (adt->action || adt->nla_tracks.first) {
			add_relation(adt_key, driver_key, "AnimData Before Drivers");
		}
	}
}

void DepsgraphRelationBuilder::build_action(bAction *action)
{
	if (built_map_.checkIsBuiltAndTag(action)) {
		return;
	}
	TimeSourceKey time_src_key;
	ComponentKey animation_key(&action->id, DEG_NODE_TYPE_ANIMATION);
	add_relation(time_src_key, animation_key, "TimeSrc -> Animation");
}

void DepsgraphRelationBuilder::build_driver(ID *id, FCurve *fcu)
{
	ChannelDriver *driver = fcu->driver;
	OperationKey driver_key(id,
	                        DEG_NODE_TYPE_PARAMETERS,
	                        DEG_OPCODE_DRIVER,
	                        fcu->rna_path ? fcu->rna_path : "",
	                        fcu->array_index);
	/* Driver -> data components (for interleaved evaluation
	 * bones/constraints/modifiers).
	 */
	build_driver_data(id, fcu);
	/* Loop over variables to get the target relationships. */
	build_driver_variables(id, fcu);
	/* It's quite tricky to detect if the driver actually depends on time or
	 * not, so for now we'll be quite conservative here about optimization and
	 * consider all python drivers to be depending on time.
	 */
	if ((driver->type == DRIVER_TYPE_PYTHON) &&
	    python_driver_depends_on_time(driver))
	{
		TimeSourceKey time_src_key;
		add_relation(time_src_key, driver_key, "TimeSrc -> Driver");
	}
}

void DepsgraphRelationBuilder::build_driver_data(ID *id, FCurve *fcu)
{
	OperationKey driver_key(id,
	                        DEG_NODE_TYPE_PARAMETERS,
	                        DEG_OPCODE_DRIVER,
	                        fcu->rna_path ? fcu->rna_path : "",
	                        fcu->array_index);
	const char *rna_path = fcu->rna_path ? fcu->rna_path : "";
	const RNAPathKey self_key(id, rna_path);
	if (GS(id->name) == ID_AR && strstr(rna_path, "bones[")) {
		/* Drivers on armature-level bone settings (i.e. bbone stuff),
		 * which will affect the evaluation of corresponding pose bones.
		 */
		IDDepsNode *arm_node = graph_->find_id_node(id);
		char *bone_name = BLI_str_quoted_substrN(rna_path, "bones[");
		if (arm_node != NULL && bone_name != NULL) {
			/* Find objects which use this, and make their eval callbacks
			 * depend on this.
			 */
			foreach (DepsRelation *rel, arm_node->outlinks) {
				IDDepsNode *to_node = (IDDepsNode *)rel->to;
				/* We only care about objects with pose data which use this. */
				if (GS(to_node->id_orig->name) == ID_OB) {
					Object *object = (Object *)to_node->id_orig;
					// NOTE: object->pose may be NULL
					bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose,
					                                                 bone_name);
					if (pchan != NULL) {
						OperationKey bone_key(&object->id,
						                      DEG_NODE_TYPE_BONE,
						                      pchan->name,
						                      DEG_OPCODE_BONE_LOCAL);
						add_relation(driver_key,
						             bone_key,
						             "Arm Bone -> Driver -> Bone");
					}
				}
			}
			/* Free temp data. */
			MEM_freeN(bone_name);
			bone_name = NULL;
		}
		else {
			fprintf(stderr,
			        "Couldn't find armature bone name for driver path - '%s'\n",
			        rna_path);
		}
	}
	else {
		RNAPathKey target_key(id, rna_path);
		add_relation(driver_key, target_key, "Driver -> Target");
		/* Similar to the case with f-curves, driver might drive a nested
		 * datablock, which means driver execution should wait for that
		 * datablock to be copied.
		 */
		{
			PointerRNA id_ptr;
			PointerRNA ptr;
			RNA_id_pointer_create(id, &id_ptr);
			if (RNA_path_resolve_full(&id_ptr, fcu->rna_path, &ptr, NULL, NULL)) {
				if (id_ptr.id.data != ptr.id.data) {
					ComponentKey cow_key((ID *)ptr.id.data,
					                     DEG_NODE_TYPE_COPY_ON_WRITE);
					add_relation(cow_key,
					             driver_key,
					             "Target CoW -> Driver",
					             true);
				}
			}
		}
		if (RNA_pointer_is_null(&target_key.ptr)) {
			/* TODO(sergey): This would only mean that driver is broken.
			 * so we can't create relation anyway. However, we need to avoid
			 * adding drivers which are known to be buggy to a dependency
			 * graph, in order to save computational power.
			 */
		}
		else {
			if (target_key.prop != NULL &&
			    RNA_property_is_idprop(target_key.prop))
			{
				OperationKey parameters_key(id,
				                            DEG_NODE_TYPE_PARAMETERS,
				                            DEG_OPCODE_PARAMETERS_EVAL);
				add_relation(target_key,
				             parameters_key,
				             "Driver Target -> Properties");
			}
			add_relation(driver_key, target_key, "Driver -> Target");
		}
	}
}

void DepsgraphRelationBuilder::build_driver_variables(ID *id, FCurve *fcu)
{
	ChannelDriver *driver = fcu->driver;
	OperationKey driver_key(id,
	                        DEG_NODE_TYPE_PARAMETERS,
	                        DEG_OPCODE_DRIVER,
	                        fcu->rna_path ? fcu->rna_path : "",
	                        fcu->array_index);
	const char *rna_path = fcu->rna_path ? fcu->rna_path : "";
	const RNAPathKey self_key(id, rna_path);

	LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
		/* Only used targets. */
		DRIVER_TARGETS_USED_LOOPER(dvar)
		{
			if (dtar->id == NULL) {
				continue;
			}
			build_id(dtar->id);
			/* Initialize relations coming to proxy_from. */
			Object *proxy_from = NULL;
			if ((GS(dtar->id->name) == ID_OB) &&
			    (((Object *)dtar->id)->proxy_from != NULL))
			{
				proxy_from = ((Object *)dtar->id)->proxy_from;
				build_id(&proxy_from->id);
			}
			/* Special handling for directly-named bones. */
			if ((dtar->flag & DTAR_FLAG_STRUCT_REF) &&
			    (((Object *)dtar->id)->type == OB_ARMATURE) &&
			    (dtar->pchan_name[0]))
			{
				Object *object = (Object *)dtar->id;
				bPoseChannel *target_pchan =
				        BKE_pose_channel_find_name(object->pose,
				                                   dtar->pchan_name);
				if (target_pchan == NULL) {
					continue;
				}
				OperationKey variable_key(dtar->id,
				                          DEG_NODE_TYPE_BONE,
				                          target_pchan->name,
				                          DEG_OPCODE_BONE_DONE);
				if (is_same_bone_dependency(variable_key, self_key)) {
					continue;
				}
				add_relation(variable_key, driver_key, "Bone Target -> Driver");
			}
			else if (dtar->flag & DTAR_FLAG_STRUCT_REF) {
				/* Get node associated with the object's transforms. */
				if (dtar->id == id) {
					/* Ignore input dependency if we're driving properties of
					 * the same ID, otherwise we'll be ending up in a cyclic
					 * dependency here.
					 */
					continue;
				}
				OperationKey target_key(dtar->id,
				                        DEG_NODE_TYPE_TRANSFORM,
				                        DEG_OPCODE_TRANSFORM_FINAL);
				add_relation(target_key, driver_key, "Target -> Driver");
			}
			else if (dtar->rna_path) {
				RNAPathKey variable_key(dtar->id, dtar->rna_path);
				if (RNA_pointer_is_null(&variable_key.ptr)) {
					continue;
				}
				if (is_same_bone_dependency(variable_key, self_key) ||
				    is_same_nodetree_node_dependency(variable_key, self_key) ||
				    is_same_shapekey_dependency(variable_key, self_key))
				{
					continue;
				}
				add_relation(variable_key, driver_key, "RNA Target -> Driver");
				if (proxy_from != NULL) {
					RNAPathKey proxy_from_variable_key(&proxy_from->id,
					                                   dtar->rna_path);
					add_relation(proxy_from_variable_key,
					             variable_key,
					             "Proxy From -> Variable");
				}
			}
			else {
				if (dtar->id == id) {
					/* Ignore input dependency if we're driving properties of
					 * the same ID, otherwise we'll be ending up in a cyclic
					 * dependency here.
					 */
					continue;
				}
				/* Resolve path to get node. */
				RNAPathKey target_key(dtar->id,
				                      dtar->rna_path ? dtar->rna_path : "");
				add_relation(target_key, driver_key, "RNA Target -> Driver");
			}
		}
		DRIVER_TARGETS_LOOPER_END
	}
}

void DepsgraphRelationBuilder::build_world(World *world)
{
	if (built_map_.checkIsBuiltAndTag(world)) {
		return;
	}
	build_animdata(&world->id);
	/* TODO: other settings? */
	/* world's nodetree */
	if (world->nodetree != NULL) {
		build_nodetree(world->nodetree);
		ComponentKey ntree_key(&world->nodetree->id, DEG_NODE_TYPE_SHADING);
		ComponentKey world_key(&world->id, DEG_NODE_TYPE_SHADING);
		add_relation(ntree_key, world_key, "NTree->World Shading Update");
		build_nested_nodetree(&world->id, world->nodetree);
	}
}

void DepsgraphRelationBuilder::build_rigidbody(Scene *scene)
{
	RigidBodyWorld *rbw = scene->rigidbody_world;

	OperationKey init_key(&scene->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_RIGIDBODY_REBUILD);
	OperationKey sim_key(&scene->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_RIGIDBODY_SIM);

	/* rel between the two sim-nodes */
	add_relation(init_key, sim_key, "Rigidbody [Init -> SimStep]");

	/* set up dependencies between these operations and other builtin nodes --------------- */

	/* time dependency */
	TimeSourceKey time_src_key;
	add_relation(time_src_key, init_key, "TimeSrc -> Rigidbody Reset/Rebuild (Optional)");

	/* objects - simulation participants */
	if (rbw->group) {
		build_collection(DEG_COLLECTION_OWNER_OBJECT, NULL, rbw->group);

		FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->group, object)
		{
			if (object->type != OB_MESH) {
				continue;
			}

			/* hook up evaluation order...
			 * 1) flushing rigidbody results follows base transforms being applied
			 * 2) rigidbody flushing can only be performed after simulation has been run
			 *
			 * 3) simulation needs to know base transforms to figure out what to do
			 *    XXX: there's probably a difference between passive and active
			 *         - passive don't change, so may need to know full transform...
			 */
			OperationKey rbo_key(&object->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY);

			eDepsOperation_Code trans_opcode = object->parent ? DEG_OPCODE_TRANSFORM_PARENT : DEG_OPCODE_TRANSFORM_LOCAL;
			OperationKey trans_op(&object->id, DEG_NODE_TYPE_TRANSFORM, trans_opcode);

			add_relation(sim_key, rbo_key, "Rigidbody Sim Eval -> RBO Sync");

			/* Geometry must be known to create the rigid body. RBO_MESH_BASE uses the non-evaluated
			 * mesh, so then the evaluation is unnecessary. */
			if (object->rigidbody_object->mesh_source != RBO_MESH_BASE) {
				ComponentKey geom_key(&object->id, DEG_NODE_TYPE_GEOMETRY);
				add_relation(geom_key, init_key, "Object Geom Eval -> Rigidbody Rebuild");
			}

			/* if constraints exist, those depend on the result of the rigidbody sim
			 * - This allows constraints to modify the result of the sim (i.e. clamping)
			 *   while still allowing the sim to depend on some changes to the objects.
			 *   Also, since constraints are hooked up to the final nodes, this link
			 *   means that we can also fit in there too...
			 * - Later, it might be good to include a constraint in the stack allowing us
			 *   to control whether rigidbody eval gets interleaved into the constraint stack
			 */
			if (object->constraints.first) {
				OperationKey constraint_key(&object->id,
				                            DEG_NODE_TYPE_TRANSFORM,
				                            DEG_OPCODE_TRANSFORM_CONSTRAINTS);
				add_relation(rbo_key, constraint_key, "RBO Sync -> Ob Constraints");
			}
			else {
				/* Final object transform depends on rigidbody.
				 *
				 * NOTE: Currently we consider final here an ubereval node.
				 * If it is gone we'll need to reconsider relation here.
				 */
				OperationKey uber_key(&object->id,
				                      DEG_NODE_TYPE_TRANSFORM,
				                      DEG_OPCODE_TRANSFORM_OBJECT_UBEREVAL);
				add_relation(rbo_key, uber_key, "RBO Sync -> Uber (Temp)");
			}

			/* Needed to get correct base values. */
			add_relation(trans_op, sim_key, "Base Ob Transform -> Rigidbody Sim Eval");
		}
		FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
	}

	/* constraints */
	if (rbw->constraints) {
		FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(rbw->constraints, object)
		{
			RigidBodyCon *rbc = object->rigidbody_constraint;
			if (rbc == NULL || rbc->ob1 == NULL || rbc->ob2 == NULL) {
				/* When either ob1 or ob2 is NULL, the constraint doesn't work. */
				continue;
			}

			/* final result of the constraint object's transform controls how the
			 * constraint affects the physics sim for these objects
			 */
			ComponentKey trans_key(&object->id, DEG_NODE_TYPE_TRANSFORM);
			OperationKey ob1_key(&rbc->ob1->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY);
			OperationKey ob2_key(&rbc->ob2->id, DEG_NODE_TYPE_TRANSFORM, DEG_OPCODE_RIGIDBODY_TRANSFORM_COPY);

			/* - constrained-objects sync depends on the constraint-holder */
			add_relation(trans_key, ob1_key, "RigidBodyConstraint -> RBC.Object_1");
			add_relation(trans_key, ob2_key, "RigidBodyConstraint -> RBC.Object_2");

			/* - ensure that sim depends on this constraint's transform */
			add_relation(trans_key, sim_key, "RigidBodyConstraint Transform -> RB Simulation");
		}
		FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
	}
}

void DepsgraphRelationBuilder::build_particles(Object *object)
{
	TimeSourceKey time_src_key;
	OperationKey obdata_ubereval_key(&object->id,
	                                 DEG_NODE_TYPE_GEOMETRY,
	                                 DEG_OPCODE_GEOMETRY_UBEREVAL);
	OperationKey eval_init_key(&object->id,
	                           DEG_NODE_TYPE_EVAL_PARTICLES,
	                           DEG_OPCODE_PARTICLE_SYSTEM_EVAL_INIT);

	/* Particle systems. */
	LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
		ParticleSettings *part = psys->part;

		/* Build particle settings relations.
		 *
		 * NOTE: The call itself ensures settings are only build once.
		 */
		build_particle_settings(part);

		/* This particle system. */
		OperationKey psys_key(&object->id,
		                      DEG_NODE_TYPE_EVAL_PARTICLES,
		                      DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
		                      psys->name);

		/* Update particle system when settings changes. */
		OperationKey particle_settings_key(&part->id,
		                                   DEG_NODE_TYPE_PARAMETERS,
		                                   DEG_OPCODE_PARTICLE_SETTINGS_EVAL);
		add_relation(particle_settings_key, eval_init_key, "Particle Settings Change");
		add_relation(eval_init_key, psys_key, "Init -> PSys");
		/* TODO(sergey): Currently particle update is just a placeholder,
		 * hook it to the ubereval node so particle system is getting updated
		 * on playback.
		 */
		add_relation(psys_key, obdata_ubereval_key, "PSys -> UberEval");
		/* Collisions */
		if (part->type != PART_HAIR) {
			add_collision_relations(psys_key,
			                        object,
			                        part->collision_group,
			                        "Particle Collision");
		}
		else if ((psys->flag & PSYS_HAIR_DYNAMICS) &&
		         psys->clmd != NULL &&
		         psys->clmd->coll_parms != NULL)
		{
			add_collision_relations(psys_key,
			                        object,
			                        psys->clmd->coll_parms->group,
			                        "Hair Collision");
		}
		/* Effectors. */
		add_forcefield_relations(psys_key,
		                         object,
		                         psys,
		                         part->effector_weights,
		                         part->type == PART_HAIR,
		                         "Particle Field");
		/* Boids .*/
		if (part->boids) {
			LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
				LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
					Object *ruleob = NULL;
					if (rule->type == eBoidRuleType_Avoid) {
						ruleob = ((BoidRuleGoalAvoid *)rule)->ob;
					}
					else if (rule->type == eBoidRuleType_FollowLeader) {
						ruleob = ((BoidRuleFollowLeader *)rule)->ob;
					}
					if (ruleob) {
						ComponentKey ruleob_key(&ruleob->id,
						                        DEG_NODE_TYPE_TRANSFORM);
						add_relation(ruleob_key, psys_key, "Boid Rule");
					}
				}
			}
		}
		switch (part->ren_as) {
			case PART_DRAW_OB:
				if (part->dup_ob != NULL) {
					/* Make sure object's relations are all built.  */
					build_object(NULL, part->dup_ob);
					/* Build relation for the particle visualization. */
					build_particles_visualization_object(object,
					                                     psys,
					                                     part->dup_ob);
				}
				break;
			case PART_DRAW_GR:
				if (part->dup_group != NULL) {
					build_collection(DEG_COLLECTION_OWNER_OBJECT, NULL, part->dup_group);
					LISTBASE_FOREACH (CollectionObject *, go, &part->dup_group->gobject) {
						build_particles_visualization_object(object,
						                                     psys,
						                                     go->ob);
					}
				}
				break;
		}
	}

	/* Particle depends on the object transform, so that channel is to be ready
	 * first.
	 *
	 * TODO(sergey): This relation should be altered once real granular update
	 * is implemented.
	 */
	ComponentKey transform_key(&object->id, DEG_NODE_TYPE_TRANSFORM);
	add_relation(transform_key, obdata_ubereval_key, "Partcile Eval");

	OperationKey point_cache_reset_key(&object->id,
	                                   DEG_NODE_TYPE_CACHE,
	                                   DEG_OPCODE_POINT_CACHE_RESET);
	add_relation(transform_key, point_cache_reset_key, "Object Transform -> Point Cache Reset");
	add_relation(point_cache_reset_key, obdata_ubereval_key, "Point Cache Reset -> UberEval");
}

void DepsgraphRelationBuilder::build_particle_settings(ParticleSettings *part)
{
	if (built_map_.checkIsBuiltAndTag(part)) {
		return;
	}
	/* Animation data relations. */
	build_animdata(&part->id);
}

void DepsgraphRelationBuilder::build_particles_visualization_object(
        Object *object,
        ParticleSystem *psys,
        Object *draw_object)
{
	OperationKey psys_key(&object->id,
	                      DEG_NODE_TYPE_EVAL_PARTICLES,
	                      DEG_OPCODE_PARTICLE_SYSTEM_EVAL,
	                      psys->name);
	OperationKey obdata_ubereval_key(&object->id,
	                                 DEG_NODE_TYPE_GEOMETRY,
	                                 DEG_OPCODE_GEOMETRY_UBEREVAL);
	ComponentKey dup_ob_key(&draw_object->id, DEG_NODE_TYPE_TRANSFORM);
	add_relation(dup_ob_key, psys_key, "Particle Object Visualization");
	if (draw_object->type == OB_MBALL) {
		ComponentKey dup_geometry_key(&draw_object->id, DEG_NODE_TYPE_GEOMETRY);
		add_relation(obdata_ubereval_key,
		             dup_geometry_key,
		             "Particle MBall Visualization");
	}
}

void DepsgraphRelationBuilder::build_cloth(Object *object,
                                           ModifierData * /*md*/)
{
	OperationKey cache_key(&object->id,
	                       DEG_NODE_TYPE_CACHE,
	                       DEG_OPCODE_GEOMETRY_CLOTH_MODIFIER);
	/* Cache component affects on modifier. */
	OperationKey modifier_key(&object->id,
	                          DEG_NODE_TYPE_GEOMETRY,
	                          DEG_OPCODE_GEOMETRY_UBEREVAL);
	add_relation(cache_key, modifier_key, "Cloth Cache -> Cloth");
}

/* Shapekeys */
void DepsgraphRelationBuilder::build_shapekeys(Key *key)
{
	if (built_map_.checkIsBuiltAndTag(key)) {
		return;
	}
	/* attach animdata to geometry */
	build_animdata(&key->id);
}

/**
 * ObData Geometry Evaluation
 * ==========================
 *
 * The evaluation of geometry on objects is as follows:
 * - The actual evaluated of the derived geometry (e.g. DerivedMesh, DispList)
 *   occurs in the Geometry component of the object which references this.
 *   This includes modifiers, and the temporary "ubereval" for geometry.
 *   Therefore, each user of a piece of shared geometry data ends up evaluating
 *   its own version of the stuff, complete with whatever modifiers it may use.
 *
 * - The datablocks for the geometry data - "obdata" (e.g. ID_ME, ID_CU, ID_LT.)
 *   are used for
 *     1) calculating the bounding boxes of the geometry data,
 *     2) aggregating inward links from other objects (e.g. for text on curve)
 *        and also for the links coming from the shapekey datablocks
 * - Animation/Drivers affecting the parameters of the geometry are made to
 *   trigger updates on the obdata geometry component, which then trigger
 *   downstream re-evaluation of the individual instances of this geometry.
 */
void DepsgraphRelationBuilder::build_object_data_geometry(Object *object)
{
	ID *obdata = (ID *)object->data;
	/* Init operation of object-level geometry evaluation. */
	OperationKey geom_init_key(&object->id,
	                           DEG_NODE_TYPE_GEOMETRY,
	                           DEG_OPCODE_PLACEHOLDER,
	                           "Eval Init");
	/* Get nodes for result of obdata's evaluation, and geometry evaluation
	 * on object.
	 */
	ComponentKey obdata_geom_key(obdata, DEG_NODE_TYPE_GEOMETRY);
	ComponentKey geom_key(&object->id, DEG_NODE_TYPE_GEOMETRY);
	/* Link components to each other. */
	add_relation(obdata_geom_key, geom_key, "Object Geometry Base Data");
	OperationKey obdata_ubereval_key(&object->id,
	                                 DEG_NODE_TYPE_GEOMETRY,
	                                 DEG_OPCODE_GEOMETRY_UBEREVAL);
	/* Special case: modifiers evaluation queries scene for various things like
	 * data mask to be used. We add relation here to ensure object is never
	 * evaluated prior to Scene's CoW is ready.
	 */
	OperationKey scene_key(&scene_->id,
	                       DEG_NODE_TYPE_LAYER_COLLECTIONS,
	                       DEG_OPCODE_VIEW_LAYER_EVAL);
	DepsRelation *rel = add_relation(scene_key, obdata_ubereval_key, "CoW Relation");
	rel->flag |= DEPSREL_FLAG_NO_FLUSH;
	/* Modifiers */
	if (object->modifiers.first != NULL) {
		ModifierUpdateDepsgraphContext ctx = {};
		ctx.scene = scene_;
		ctx.object = object;
		LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
			const ModifierTypeInfo *mti = modifierType_getInfo((ModifierType)md->type);
			if (mti->updateDepsgraph) {
				DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
				ctx.node = reinterpret_cast< ::DepsNodeHandle* >(&handle);
				mti->updateDepsgraph(md, &ctx);
			}
			if (BKE_object_modifier_use_time(object, md)) {
				TimeSourceKey time_src_key;
				add_relation(time_src_key, obdata_ubereval_key, "Time Source");
			}
			if (md->type == eModifierType_Cloth) {
				build_cloth(object, md);
			}
		}
	}
	/* Grease Pencil Modifiers */
	if (object->greasepencil_modifiers.first != NULL) {
		ModifierUpdateDepsgraphContext ctx = {};
		ctx.scene = scene_;
		ctx.object = object;
		LISTBASE_FOREACH(GpencilModifierData *, md, &object->greasepencil_modifiers) {
			const GpencilModifierTypeInfo *mti = BKE_gpencil_modifierType_getInfo((GpencilModifierType)md->type);
			if (mti->updateDepsgraph) {
				DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
				ctx.node = reinterpret_cast< ::DepsNodeHandle* >(&handle);
				mti->updateDepsgraph(md, &ctx);
			}
			if (BKE_object_modifier_gpencil_use_time(object, md)) {
				TimeSourceKey time_src_key;
				add_relation(time_src_key, obdata_ubereval_key, "Time Source");
			}
		}
	}
	/* Shader FX */
	if (object->shader_fx.first != NULL) {
		ModifierUpdateDepsgraphContext ctx = {};
		ctx.scene = scene_;
		ctx.object = object;
		LISTBASE_FOREACH(ShaderFxData *, fx, &object->shader_fx) {
			const ShaderFxTypeInfo *fxi = BKE_shaderfxType_getInfo((ShaderFxType)fx->type);
			if (fxi->updateDepsgraph) {
				DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
				ctx.node = reinterpret_cast< ::DepsNodeHandle* >(&handle);
				fxi->updateDepsgraph(fx, &ctx);
			}
			if (BKE_object_shaderfx_use_time(object, fx)) {
				TimeSourceKey time_src_key;
				add_relation(time_src_key, obdata_ubereval_key, "Time Source");
			}
		}
	}
	/* Materials. */
	if (object->totcol) {
		for (int a = 1; a <= object->totcol; a++) {
			Material *ma = give_current_material(object, a);
			if (ma != NULL) {
				build_material(ma);

				if (object->type == OB_MESH) {
					OperationKey material_key(&ma->id,
					                          DEG_NODE_TYPE_SHADING,
					                          DEG_OPCODE_MATERIAL_UPDATE);
					OperationKey shading_key(&object->id,
					                         DEG_NODE_TYPE_SHADING,
					                         DEG_OPCODE_SHADING);
					add_relation(material_key, shading_key, "Material Update");
				}
			}
		}
	}
	/* Geometry collision. */
	if (ELEM(object->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
		// add geometry collider relations
	}
	/* Make sure uber update is the last in the dependencies.
	 *
	 * TODO(sergey): Get rid of this node.
	 */
	if (object->type != OB_ARMATURE) {
		/* Armatures does no longer require uber node. */
		OperationKey obdata_ubereval_key(&object->id,
		                                 DEG_NODE_TYPE_GEOMETRY,
		                                 DEG_OPCODE_GEOMETRY_UBEREVAL);
		add_relation(geom_init_key,
		             obdata_ubereval_key,
		             "Object Geometry UberEval");
	}
	if (object->type == OB_MBALL) {
		Object *mom = BKE_mball_basis_find(scene_, object);
		ComponentKey mom_geom_key(&mom->id, DEG_NODE_TYPE_GEOMETRY);
		/* motherball - mom depends on children! */
		if (mom == object) {
			ComponentKey mom_transform_key(&mom->id,
			                               DEG_NODE_TYPE_TRANSFORM);
			add_relation(mom_transform_key,
			             mom_geom_key,
			             "Metaball Motherball Transform -> Geometry");
		}
		else {
			ComponentKey transform_key(&object->id, DEG_NODE_TYPE_TRANSFORM);
			add_relation(geom_key, mom_geom_key, "Metaball Motherball");
			add_relation(transform_key, mom_geom_key, "Metaball Motherball");
		}
	}
	/* NOTE: This is compatibility code to support particle systems
	 *
	 * for viewport being properly rendered in final render mode.
	 * This relation is similar to what dag_object_time_update_flags()
	 * was doing for mesh objects with particle system.
	 *
	 * Ideally we need to get rid of this relation.
	 */
	if (object_particles_depends_on_time(object)) {
		TimeSourceKey time_key;
		OperationKey obdata_ubereval_key(&object->id,
		                                 DEG_NODE_TYPE_GEOMETRY,
		                                 DEG_OPCODE_GEOMETRY_UBEREVAL);
		add_relation(time_key, obdata_ubereval_key, "Legacy particle time");
	}
	/* Object data datablock. */
	build_object_data_geometry_datablock((ID *)object->data);
	Key *key = BKE_key_from_object(object);
	if (key != NULL) {
		if (key->adt != NULL) {
			if (key->adt->action || key->adt->nla_tracks.first) {
				ComponentKey obdata_key((ID *)object->data,
				                        DEG_NODE_TYPE_GEOMETRY);
				ComponentKey adt_key(&key->id, DEG_NODE_TYPE_ANIMATION);
				add_relation(adt_key, obdata_key, "Animation");
			}
		}
	}
}

void DepsgraphRelationBuilder::build_object_data_geometry_datablock(ID *obdata)
{
	if (built_map_.checkIsBuiltAndTag(obdata)) {
		return;
	}
	/* Animation. */
	build_animdata(obdata);
	/* ShapeKeys. */
	Key *key = BKE_key_from_id(obdata);
	if (key != NULL) {
		build_shapekeys(key);
	}
	/* Link object data evaluation node to exit operation. */
	OperationKey obdata_geom_eval_key(obdata,
		DEG_NODE_TYPE_GEOMETRY,
		DEG_OPCODE_PLACEHOLDER,
		"Geometry Eval");
	OperationKey obdata_geom_done_key(obdata,
		DEG_NODE_TYPE_GEOMETRY,
		DEG_OPCODE_PLACEHOLDER,
		"Eval Done");
	add_relation(obdata_geom_eval_key,
	             obdata_geom_done_key,
	             "ObData Geom Eval Done");
	/* Type-specific links. */
	const ID_Type id_type = GS(obdata->name);
	switch (id_type) {
		case ID_ME:
			break;
		case ID_MB:
			break;
		case ID_CU:
		{
			Curve *cu = (Curve *)obdata;
			if (cu->bevobj != NULL) {
				ComponentKey bevob_geom_key(&cu->bevobj->id,
					DEG_NODE_TYPE_GEOMETRY);
				add_relation(bevob_geom_key,
					obdata_geom_eval_key,
					"Curve Bevel Geometry");
				ComponentKey bevob_key(&cu->bevobj->id,
					DEG_NODE_TYPE_TRANSFORM);
				add_relation(bevob_key,
					obdata_geom_eval_key,
					"Curve Bevel Transform");
				build_object(NULL, cu->bevobj);
			}
			if (cu->taperobj != NULL) {
				ComponentKey taperob_key(&cu->taperobj->id,
					DEG_NODE_TYPE_GEOMETRY);
				add_relation(taperob_key, obdata_geom_eval_key, "Curve Taper");
				build_object(NULL, cu->taperobj);
			}
			if (cu->textoncurve != NULL) {
				ComponentKey textoncurve_key(&cu->textoncurve->id,
					DEG_NODE_TYPE_GEOMETRY);
				add_relation(textoncurve_key,
					obdata_geom_eval_key,
					"Text on Curve");
				build_object(NULL, cu->textoncurve);
			}
			break;
		}
		case ID_LT:
			break;
		case ID_GD: /* Grease Pencil */
		{
			bGPdata *gpd = (bGPdata *)obdata;

			/* Geometry cache needs to be recalculated on frame change
			* (e.g. to fix crashes after scrubbing the timeline when
			*  onion skinning is enabled, since the ghosts need to be
			*  re-added to the cache once scrubbing ends)
			*/
			TimeSourceKey time_key;
			ComponentKey geometry_key(obdata, DEG_NODE_TYPE_GEOMETRY);
			add_relation(time_key,
			             geometry_key,
			             "GP Frame Change");

			/* Geometry cache also needs to be recalculated when Material
			 * settings change (e.g. when fill.opacity changes on/off,
			 * we need to rebuild the bGPDstroke->triangles caches)
			 */
			for (int i = 0; i < gpd->totcol; i++) {
				Material *ma = gpd->mat[i];
				if ((ma != NULL) && (ma->gp_style != NULL)) {
					OperationKey material_key(&ma->id,
					                          DEG_NODE_TYPE_SHADING,
					                          DEG_OPCODE_MATERIAL_UPDATE);
					add_relation(material_key,
					             geometry_key,
					             "Material -> GP Data");
				}
			}
			break;
		}
		default:
			BLI_assert(!"Should not happen");
			break;
	}
}

void DepsgraphRelationBuilder::build_armature(bArmature *armature)
{
	if (built_map_.checkIsBuiltAndTag(armature)) {
		return;
	}
	build_animdata(&armature->id);
}

void DepsgraphRelationBuilder::build_camera(Camera *camera)
{
	if (built_map_.checkIsBuiltAndTag(camera)) {
		return;
	}
	if (camera->dof_ob != NULL) {
		ComponentKey camera_parameters_key(&camera->id, DEG_NODE_TYPE_PARAMETERS);
		ComponentKey dof_ob_key(&camera->dof_ob->id, DEG_NODE_TYPE_TRANSFORM);
		add_relation(dof_ob_key, camera_parameters_key, "Camera DOF");
	}
}

/* Lamps */
void DepsgraphRelationBuilder::build_lamp(Lamp *lamp)
{
	if (built_map_.checkIsBuiltAndTag(lamp)) {
		return;
	}
	/* lamp's nodetree */
	if (lamp->nodetree != NULL) {
		build_nodetree(lamp->nodetree);
		ComponentKey lamp_parameters_key(&lamp->id, DEG_NODE_TYPE_PARAMETERS);
		ComponentKey nodetree_key(&lamp->nodetree->id, DEG_NODE_TYPE_SHADING);
		add_relation(nodetree_key, lamp_parameters_key, "NTree->Light Parameters");
		build_nested_nodetree(&lamp->id, lamp->nodetree);
	}
}

void DepsgraphRelationBuilder::build_nodetree(bNodeTree *ntree)
{
	if (ntree == NULL) {
		return;
	}
	if (built_map_.checkIsBuiltAndTag(ntree)) {
		return;
	}
	build_animdata(&ntree->id);
	ComponentKey shading_key(&ntree->id, DEG_NODE_TYPE_SHADING);
	/* nodetree's nodes... */
	LISTBASE_FOREACH (bNode *, bnode, &ntree->nodes) {
		ID *id = bnode->id;
		if (id == NULL) {
			continue;
		}
		ID_Type id_type = GS(id->name);
		if (id_type == ID_MA) {
			build_material((Material *)bnode->id);
		}
		else if (id_type == ID_TE) {
			build_texture((Tex *)bnode->id);
		}
		else if (id_type == ID_IM) {
			/* nothing for now. */
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
		else if (id_type == ID_MC) {
			build_movieclip((MovieClip *)id);
		}
		else if (bnode->type == NODE_GROUP) {
			bNodeTree *group_ntree = (bNodeTree *)id;
			build_nodetree(group_ntree);
			ComponentKey group_shading_key(&group_ntree->id,
			                               DEG_NODE_TYPE_SHADING);
			add_relation(group_shading_key, shading_key, "Group Node");
		}
		else {
			BLI_assert(!"Unknown ID type used for node");
		}
	}

	OperationKey shading_update_key(&ntree->id,
	                                DEG_NODE_TYPE_SHADING,
	                                DEG_OPCODE_MATERIAL_UPDATE);
	OperationKey shading_parameters_key(&ntree->id,
	                                    DEG_NODE_TYPE_SHADING_PARAMETERS,
	                                    DEG_OPCODE_MATERIAL_UPDATE);
	add_relation(shading_parameters_key, shading_update_key, "NTree Shading Parameters");

	if (check_id_has_anim_component(&ntree->id)) {
		ComponentKey animation_key(&ntree->id, DEG_NODE_TYPE_ANIMATION);
		add_relation(animation_key, shading_parameters_key, "NTree Shading Parameters");
	}
}

/* Recursively build graph for material */
void DepsgraphRelationBuilder::build_material(Material *material)
{
	if (built_map_.checkIsBuiltAndTag(material)) {
		return;
	}
	/* animation */
	build_animdata(&material->id);
	/* material's nodetree */
	if (material->nodetree != NULL) {
		build_nodetree(material->nodetree);
		OperationKey ntree_key(&material->nodetree->id,
		                       DEG_NODE_TYPE_SHADING,
		                       DEG_OPCODE_MATERIAL_UPDATE);
		OperationKey material_key(&material->id,
		                          DEG_NODE_TYPE_SHADING,
		                          DEG_OPCODE_MATERIAL_UPDATE);
		add_relation(ntree_key, material_key, "Material's NTree");
		build_nested_nodetree(&material->id, material->nodetree);
	}
}

/* Recursively build graph for texture */
void DepsgraphRelationBuilder::build_texture(Tex *texture)
{
	if (built_map_.checkIsBuiltAndTag(texture)) {
		return;
	}
	/* texture itself */
	build_animdata(&texture->id);
	/* texture's nodetree */
	build_nodetree(texture->nodetree);
	build_nested_nodetree(&texture->id, texture->nodetree);
}

void DepsgraphRelationBuilder::build_compositor(Scene *scene)
{
	/* For now, just a plain wrapper? */
	build_nodetree(scene->nodetree);
}

void DepsgraphRelationBuilder::build_gpencil(bGPdata *gpd)
{
	if (built_map_.checkIsBuiltAndTag(gpd)) {
		return;
	}
	/* animation */
	build_animdata(&gpd->id);

	// TODO: parent object (when that feature is implemented)
}

void DepsgraphRelationBuilder::build_cachefile(CacheFile *cache_file)
{
	if (built_map_.checkIsBuiltAndTag(cache_file)) {
		return;
	}
	/* Animation. */
	build_animdata(&cache_file->id);
}

void DepsgraphRelationBuilder::build_mask(Mask *mask)
{
	if (built_map_.checkIsBuiltAndTag(mask)) {
		return;
	}
	ID *mask_id = &mask->id;
	/* F-Curve animation. */
	build_animdata(mask_id);
	/* Own mask animation. */
	OperationKey mask_animation_key(mask_id,
	                                DEG_NODE_TYPE_ANIMATION,
	                                DEG_OPCODE_MASK_ANIMATION);
	TimeSourceKey time_src_key;
	add_relation(time_src_key, mask_animation_key, "TimeSrc -> Mask Animation");
	/* Final mask evaluation. */
	ComponentKey parameters_key(mask_id, DEG_NODE_TYPE_PARAMETERS);
	add_relation(mask_animation_key, parameters_key, "Mask Animation -> Mask Eval");
}

void DepsgraphRelationBuilder::build_movieclip(MovieClip *clip)
{
	if (built_map_.checkIsBuiltAndTag(clip)) {
		return;
	}
	/* Animation. */
	build_animdata(&clip->id);
}

void DepsgraphRelationBuilder::build_lightprobe(LightProbe *probe)
{
	if (built_map_.checkIsBuiltAndTag(probe)) {
		return;
	}
	build_animdata(&probe->id);
}

void DepsgraphRelationBuilder::build_speaker(Speaker *speaker)
{
	if (built_map_.checkIsBuiltAndTag(speaker)) {
		return;
	}
	build_animdata(&speaker->id);
}

void DepsgraphRelationBuilder::build_copy_on_write_relations()
{
	foreach (IDDepsNode *id_node, graph_->id_nodes) {
		build_copy_on_write_relations(id_node);
	}
}

/* Nested datablocks (node trees, shape keys) requires special relation to
 * ensure owner's datablock remapping happens after node tree itself is ready.
 *
 * This is similar to what happens in ntree_hack_remap_pointers().
 */
void DepsgraphRelationBuilder::build_nested_datablock(ID *owner, ID *id)
{
	OperationKey owner_copy_on_write_key(owner,
	                                     DEG_NODE_TYPE_COPY_ON_WRITE,
	                                     DEG_OPCODE_COPY_ON_WRITE);
	OperationKey id_copy_on_write_key(id,
	                                  DEG_NODE_TYPE_COPY_ON_WRITE,
	                                  DEG_OPCODE_COPY_ON_WRITE);
	add_relation(id_copy_on_write_key,
	             owner_copy_on_write_key,
	             "Eval Order");
}

void DepsgraphRelationBuilder::build_nested_nodetree(ID *owner,
                                                     bNodeTree *ntree)
{
	if (ntree == NULL) {
		return;
	}
	build_nested_datablock(owner, &ntree->id);
}

void DepsgraphRelationBuilder::build_nested_shapekey(ID *owner, Key *key)
{
	if (key == NULL) {
		return;
	}
	build_nested_datablock(owner, &key->id);
}

void DepsgraphRelationBuilder::build_copy_on_write_relations(IDDepsNode *id_node)
{
	ID *id_orig = id_node->id_orig;
	const ID_Type id_type = GS(id_orig->name);
	TimeSourceKey time_source_key;
	OperationKey copy_on_write_key(id_orig,
	                               DEG_NODE_TYPE_COPY_ON_WRITE,
	                               DEG_OPCODE_COPY_ON_WRITE);
	/* XXX: This is a quick hack to make Alt-A to work. */
	// add_relation(time_source_key, copy_on_write_key, "Fluxgate capacitor hack");
	/* Resat of code is using rather low level trickery, so need to get some
	 * explicit pointers.
	 */
	DepsNode *node_cow = find_node(copy_on_write_key);
	OperationDepsNode *op_cow = node_cow->get_exit_operation();
	/* Plug any other components to this one. */
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
	{
		if (comp_node->type == DEG_NODE_TYPE_COPY_ON_WRITE) {
			/* Copy-on-write component never depends on itself. */
			continue;
		}
		if (!comp_node->depends_on_cow()) {
			/* Component explicitly requests to not add relation. */
			continue;
		}
		int rel_flag = DEPSREL_FLAG_NO_FLUSH;
		if (id_type == ID_ME && comp_node->type == DEG_NODE_TYPE_GEOMETRY) {
			rel_flag &= ~DEPSREL_FLAG_NO_FLUSH;
		}
		/* materials need update grease pencil objects */
		if (id_type == ID_MA) {
			rel_flag &= ~DEPSREL_FLAG_NO_FLUSH;
		}

		/* Notes on exceptions:
		 * - Parameters component is where drivers are living. Changing any
		 *   of the (custom) properties in the original datablock (even the
		 *   ones which do not imply other component update) need to make
		 *   sure drivers are properly updated.
		 *   This way, for example, changing ID property will properly poke
		 *   all drivers to be updated.
		 *
		 * - View layers have cached array of bases in them, which is not
		 *   copied by copy-on-write, and not preserved. PROBABLY it is better
		 *   to preserve that cache in copy-on-write, but for the time being
		 *   we allow flush to layer collections component which will ensure
		 *   that cached array fo bases exists and is up-to-date.
		 */
		if (comp_node->type == DEG_NODE_TYPE_PARAMETERS ||
		    comp_node->type == DEG_NODE_TYPE_LAYER_COLLECTIONS)
		{
			rel_flag &= ~DEPSREL_FLAG_NO_FLUSH;
		}
		/* All entry operations of each component should wait for a proper
		 * copy of ID.
		 */
		OperationDepsNode *op_entry = comp_node->get_entry_operation();
		if (op_entry != NULL) {
			DepsRelation *rel = graph_->add_new_relation(
			        op_cow, op_entry, "CoW Dependency");
			rel->flag |= rel_flag;
		}
		/* All dangling operations should also be executed after copy-on-write. */
		GHASH_FOREACH_BEGIN(OperationDepsNode *, op_node, comp_node->operations_map)
		{
			if (op_node == op_entry) {
				continue;
			}
			if (op_node->inlinks.size() == 0) {
				DepsRelation *rel = graph_->add_new_relation(
				        op_cow, op_node, "CoW Dependency");
				rel->flag |= rel_flag;
			}
			else {
				bool has_same_comp_dependency = false;
				foreach (DepsRelation *rel_current, op_node->inlinks) {
					if (rel_current->from->type != DEG_NODE_TYPE_OPERATION) {
						continue;
					}
					OperationDepsNode *op_node_from =
					        (OperationDepsNode *)rel_current->from;
					if (op_node_from->owner == op_node->owner) {
						has_same_comp_dependency = true;
						break;
					}
				}
				if (!has_same_comp_dependency) {
					DepsRelation *rel = graph_->add_new_relation(
					        op_cow, op_node, "CoW Dependency");
					rel->flag |= rel_flag;
				}
			}
		}
		GHASH_FOREACH_END();
		/* NOTE: We currently ignore implicit relations to an external
		 * datablocks for copy-on-write operations. This means, for example,
		 * copy-on-write component of Object will not wait for copy-on-write
		 * component of it's Mesh. This is because pointers are all known
		 * already so remapping will happen all correct. And then If some object
		 * evaluation step needs geometry, it will have transitive dependency
		 * to Mesh copy-on-write already.
		 */
	}
	GHASH_FOREACH_END();
	/* TODO(sergey): This solves crash for now, but causes too many
	 * updates potentially.
	 */
	if (GS(id_orig->name) == ID_OB) {
		Object *object = (Object *)id_orig;
		ID *object_data_id = (ID *)object->data;
		if (object_data_id != NULL) {
			OperationKey data_copy_on_write_key(object_data_id,
			                                    DEG_NODE_TYPE_COPY_ON_WRITE,
			                                    DEG_OPCODE_COPY_ON_WRITE);
			add_relation(data_copy_on_write_key, copy_on_write_key, "Eval Order");
		}
		else {
			BLI_assert(object->type == OB_EMPTY);
		}
	}
}

/* **** ID traversal callbacks functions **** */

void DepsgraphRelationBuilder::modifier_walk(void *user_data,
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

void DepsgraphRelationBuilder::constraint_walk(bConstraint * /*con*/,
                                               ID **idpoin,
                                               bool /*is_reference*/,
                                               void *user_data)
{
	BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
	if (*idpoin) {
		ID *id = *idpoin;
		if (GS(id->name) == ID_OB) {
			data->builder->build_object(NULL, (Object *)id);
		}
	}
}

}  // namespace DEG
