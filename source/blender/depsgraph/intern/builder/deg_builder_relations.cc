/*
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
 */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <stdio.h>
#include <stdlib.h>
#include <cstring> /* required for STREQ later on. */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
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
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
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
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_sequencer.h"
#include "BKE_shader_fx.h"
#include "BKE_shrinkwrap.h"
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
#include "intern/debug/deg_debug.h"
#include "intern/depsgraph_tag.h"
#include "intern/depsgraph_physics.h"
#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

#include "intern/depsgraph_type.h"

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
     * handling this. */
    return true;
  }
  /* Possible indirect time relation s should be handled via variable
   * targets. */
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
  return (adt->action != NULL) || (!BLI_listbase_is_empty(&adt->nla_tracks));
}

static OperationCode bone_target_opcode(ID *target,
                                        const char *subtarget,
                                        ID *id,
                                        const char *component_subdata,
                                        RootPChanMap *root_map)
{
  /* Same armature.  */
  if (target == id) {
    /* Using "done" here breaks in-chain deps, while using
     * "ready" here breaks most production rigs instead.
     * So, we do a compromise here, and only do this when an
     * IK chain conflict may occur. */
    if (root_map->has_common_root(component_subdata, subtarget)) {
      return OperationCode::BONE_READY;
    }
  }
  return OperationCode::BONE_DONE;
}

static bool object_have_geometry_component(const Object *object)
{
  return ELEM(object->type, OB_MESH, OB_CURVE, OB_FONT, OB_SURF, OB_MBALL, OB_LATTICE, OB_GPENCIL);
}

/* **** General purpose functions ****  */

DepsgraphRelationBuilder::DepsgraphRelationBuilder(Main *bmain,
                                                   Depsgraph *graph,
                                                   DepsgraphBuilderCache *cache)
    : DepsgraphBuilder(bmain, graph, cache), scene_(NULL), rna_node_query_(graph, this)
{
}

TimeSourceNode *DepsgraphRelationBuilder::get_node(const TimeSourceKey &key) const
{
  if (key.id) {
    /* XXX TODO */
    return NULL;
  }
  else {
    return graph_->time_source;
  }
}

ComponentNode *DepsgraphRelationBuilder::get_node(const ComponentKey &key) const
{
  IDNode *id_node = graph_->find_id_node(key.id);
  if (!id_node) {
    fprintf(stderr,
            "find_node component: Could not find ID %s\n",
            (key.id != NULL) ? key.id->name : "<null>");
    return NULL;
  }

  ComponentNode *node = id_node->find_component(key.type, key.name);
  return node;
}

OperationNode *DepsgraphRelationBuilder::get_node(const OperationKey &key) const
{
  OperationNode *op_node = find_node(key);
  if (op_node == NULL) {
    fprintf(stderr,
            "find_node_operation: Failed for (%s, '%s')\n",
            operationCodeAsString(key.opcode),
            key.name);
  }
  return op_node;
}

Node *DepsgraphRelationBuilder::get_node(const RNAPathKey &key)
{
  return rna_node_query_.find_node(&key.ptr, key.prop, key.source);
}

OperationNode *DepsgraphRelationBuilder::find_node(const OperationKey &key) const
{
  IDNode *id_node = graph_->find_id_node(key.id);
  if (!id_node) {
    return NULL;
  }
  ComponentNode *comp_node = id_node->find_component(key.component_type, key.component_name);
  if (!comp_node) {
    return NULL;
  }
  return comp_node->find_operation(key.opcode, key.name, key.name_tag);
}

bool DepsgraphRelationBuilder::has_node(const OperationKey &key) const
{
  return find_node(key) != NULL;
}

void DepsgraphRelationBuilder::add_modifier_to_transform_relation(const DepsNodeHandle *handle,
                                                                  const char *description)
{
  IDNode *id_node = handle->node->owner->owner;
  ID *id = id_node->id_orig;
  ComponentKey geometry_key(id, NodeType::GEOMETRY);
  /* Wire up the actual relation. */
  add_depends_on_transform_relation(id, geometry_key, description);
}

void DepsgraphRelationBuilder::add_customdata_mask(Object *object,
                                                   const DEGCustomDataMeshMasks &customdata_masks)
{
  if (customdata_masks != DEGCustomDataMeshMasks() && object != NULL && object->type == OB_MESH) {
    DEG::IDNode *id_node = graph_->find_id_node(&object->id);

    if (id_node == NULL) {
      BLI_assert(!"ID should always be valid");
    }
    else {
      id_node->customdata_masks |= customdata_masks;
    }
  }
}

void DepsgraphRelationBuilder::add_special_eval_flag(ID *id, uint32_t flag)
{
  DEG::IDNode *id_node = graph_->find_id_node(id);
  if (id_node == NULL) {
    BLI_assert(!"ID should always be valid");
  }
  else {
    id_node->eval_flags |= flag;
  }
}

Relation *DepsgraphRelationBuilder::add_time_relation(TimeSourceNode *timesrc,
                                                      Node *node_to,
                                                      const char *description,
                                                      int flags)
{
  if (timesrc && node_to) {
    return graph_->add_new_relation(timesrc, node_to, description, flags);
  }
  else {
    DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                     BUILD,
                     "add_time_relation(%p = %s, %p = %s, %s) Failed\n",
                     timesrc,
                     (timesrc) ? timesrc->identifier().c_str() : "<None>",
                     node_to,
                     (node_to) ? node_to->identifier().c_str() : "<None>",
                     description);
  }
  return NULL;
}

Relation *DepsgraphRelationBuilder::add_operation_relation(OperationNode *node_from,
                                                           OperationNode *node_to,
                                                           const char *description,
                                                           int flags)
{
  if (node_from && node_to) {
    return graph_->add_new_relation(node_from, node_to, description, flags);
  }
  else {
    DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                     BUILD,
                     "add_operation_relation(%p = %s, %p = %s, %s) Failed\n",
                     node_from,
                     (node_from) ? node_from->identifier().c_str() : "<None>",
                     node_to,
                     (node_to) ? node_to->identifier().c_str() : "<None>",
                     description);
  }
  return NULL;
}

void DepsgraphRelationBuilder::add_particle_collision_relations(const OperationKey &key,
                                                                Object *object,
                                                                Collection *collection,
                                                                const char *name)
{
  ListBase *relations = build_collision_relations(graph_, collection, eModifierType_Collision);

  LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
    if (relation->ob != object) {
      ComponentKey trf_key(&relation->ob->id, NodeType::TRANSFORM);
      add_relation(trf_key, key, name);

      ComponentKey coll_key(&relation->ob->id, NodeType::GEOMETRY);
      add_relation(coll_key, key, name);
    }
  }
}

void DepsgraphRelationBuilder::add_particle_forcefield_relations(const OperationKey &key,
                                                                 Object *object,
                                                                 ParticleSystem *psys,
                                                                 EffectorWeights *eff,
                                                                 bool add_absorption,
                                                                 const char *name)
{
  ListBase *relations = build_effector_relations(graph_, eff->group);

  LISTBASE_FOREACH (EffectorRelation *, relation, relations) {
    if (relation->ob != object) {
      /* Relation to forcefield object, optionally including geometry. */
      ComponentKey eff_key(&relation->ob->id, NodeType::TRANSFORM);
      add_relation(eff_key, key, name);

      if (ELEM(relation->pd->shape, PFIELD_SHAPE_SURFACE, PFIELD_SHAPE_POINTS) ||
          relation->pd->forcefield == PFIELD_GUIDE) {
        ComponentKey mod_key(&relation->ob->id, NodeType::GEOMETRY);
        add_relation(mod_key, key, name);
      }

      /* Smoke flow relations. */
      if (relation->pd->forcefield == PFIELD_SMOKEFLOW && relation->pd->f_source) {
        ComponentKey trf_key(&relation->pd->f_source->id, NodeType::TRANSFORM);
        add_relation(trf_key, key, "Smoke Force Domain");
        ComponentKey eff_key(&relation->pd->f_source->id, NodeType::GEOMETRY);
        add_relation(eff_key, key, "Smoke Force Domain");
      }

      /* Absorption forces need collision relation. */
      if (add_absorption && (relation->pd->flag & PFIELD_VISIBILITY)) {
        add_particle_collision_relations(key, object, NULL, "Force Absorption");
      }
    }

    if (relation->psys) {
      if (relation->ob != object) {
        ComponentKey eff_key(&relation->ob->id, NodeType::PARTICLE_SYSTEM);
        add_relation(eff_key, key, name);
        /* TODO: remove this when/if EVAL_PARTICLES is sufficient
         * for up to date particles. */
        ComponentKey mod_key(&relation->ob->id, NodeType::GEOMETRY);
        add_relation(mod_key, key, name);
      }
      else if (relation->psys != psys) {
        OperationKey eff_key(&relation->ob->id,
                             NodeType::PARTICLE_SYSTEM,
                             OperationCode::PARTICLE_SYSTEM_EVAL,
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
    case ID_AC:
      build_action((bAction *)id);
      break;
    case ID_AR:
      build_armature((bArmature *)id);
      break;
    case ID_CA:
      build_camera((Camera *)id);
      break;
    case ID_GR:
      build_collection(NULL, NULL, (Collection *)id);
      break;
    case ID_OB:
      build_object(NULL, (Object *)id);
      break;
    case ID_KE:
      build_shapekeys((Key *)id);
      break;
    case ID_LA:
      build_light((Light *)id);
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
    case ID_SO:
      build_sound((bSound *)id);
      break;
    case ID_TXT:
      /* Not a part of dependency graph. */
      break;
    case ID_CF:
      build_cachefile((CacheFile *)id);
      break;
    case ID_SCE:
      build_scene_parameters((Scene *)id);
      break;
    default:
      fprintf(stderr, "Unhandled ID %s\n", id->name);
      BLI_assert(!"Should never happen");
      break;
  }
}

void DepsgraphRelationBuilder::build_collection(LayerCollection *from_layer_collection,
                                                Object *object,
                                                Collection *collection)
{
  if (from_layer_collection != NULL) {
    /* If we came from layer collection we don't go deeper, view layer
     * builder takes care of going deeper.
     *
     * NOTE: Do early output before tagging build as done, so possbile
     * subsequent builds from outside of the layer collection properly
     * recurses into all the nested objects and collections. */
    return;
  }
  const bool group_done = built_map_.checkIsBuiltAndTag(collection);
  OperationKey object_transform_final_key(
      object != NULL ? &object->id : NULL, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
  ComponentKey duplicator_key(object != NULL ? &object->id : NULL, NodeType::DUPLI);
  if (!group_done) {
    LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
      build_object(NULL, cob->ob);
    }
    LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
      build_collection(NULL, NULL, child->collection);
    }
  }
  if (object != NULL) {
    FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (collection, ob, graph_->mode) {
      ComponentKey dupli_transform_key(&ob->id, NodeType::TRANSFORM);
      add_relation(dupli_transform_key, object_transform_final_key, "Dupligroup");
      /* Hook to special component, to ensure proper visibility/evaluation
       * optimizations. */
      add_relation(dupli_transform_key, duplicator_key, "Dupligroup");
      const NodeType dupli_geometry_component_type = geometry_tag_to_component(&ob->id);
      if (dupli_geometry_component_type != NodeType::UNDEFINED) {
        ComponentKey dupli_geometry_component_key(&ob->id, dupli_geometry_component_type);
        add_relation(dupli_geometry_component_key, duplicator_key, "Dupligroup");
      }
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
  OperationCode base_op = (object->parent) ? OperationCode::TRANSFORM_PARENT :
                                             OperationCode::TRANSFORM_LOCAL;
  OperationKey base_op_key(&object->id, NodeType::TRANSFORM, base_op);
  OperationKey init_transform_key(&object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_INIT);
  OperationKey local_transform_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_LOCAL);
  OperationKey parent_transform_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_PARENT);
  OperationKey transform_eval_key(&object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
  OperationKey final_transform_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
  OperationKey ob_eval_key(&object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
  add_relation(init_transform_key, local_transform_key, "Transform Init");
  /* Various flags, flushing from bases/collections. */
  build_object_flags(base, object);
  /* Parenting. */
  if (object->parent != NULL) {
    /* Make sure parent object's relations are built. */
    build_object(NULL, object->parent);
    /* Parent relationship. */
    build_object_parent(object);
    /* Local -> parent. */
    add_relation(local_transform_key, parent_transform_key, "ObLocal -> ObParent");
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
  OperationKey object_transform_simulation_init_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
  if (object->constraints.first != NULL) {
    OperationKey constraint_key(
        &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_CONSTRAINTS);
    /* Constraint relations. */
    build_constraints(&object->id, NodeType::TRANSFORM, "", &object->constraints, NULL);
    /* operation order */
    add_relation(base_op_key, constraint_key, "ObBase-> Constraint Stack");
    add_relation(constraint_key, final_transform_key, "ObConstraints -> Done");
    add_relation(constraint_key, ob_eval_key, "Constraint -> Transform Eval");
    add_relation(
        ob_eval_key, object_transform_simulation_init_key, "Transform Eval -> Simulation Init");
    add_relation(object_transform_simulation_init_key,
                 final_transform_key,
                 "Simulation -> Final Transform");
  }
  else {
    add_relation(base_op_key, ob_eval_key, "Eval");
    add_relation(
        ob_eval_key, object_transform_simulation_init_key, "Transform Eval -> Simulation Init");
    add_relation(object_transform_simulation_init_key,
                 final_transform_key,
                 "Simulation -> Final Transform");
  }
  /* Animation data */
  build_animdata(&object->id);
  /* Object data. */
  build_object_data(object);
  /* Particle systems. */
  if (object->particlesystem.first != NULL) {
    build_particle_systems(object);
  }
  /* Proxy object to copy from. */
  if (object->proxy_from != NULL) {
    /* Object is linked here (comes from the library). */
    build_object(NULL, object->proxy_from);
    ComponentKey ob_transform_key(&object->proxy_from->id, NodeType::TRANSFORM);
    ComponentKey proxy_transform_key(&object->id, NodeType::TRANSFORM);
    add_relation(ob_transform_key, proxy_transform_key, "Proxy Transform");
  }
  if (object->proxy_group != NULL && object->proxy_group != object->proxy) {
    /* Object is local here (local in .blend file, users interacts with it). */
    build_object(NULL, object->proxy_group);
    OperationKey proxy_group_eval_key(
        &object->proxy_group->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
    add_relation(proxy_group_eval_key, transform_eval_key, "Proxy Group Transform");
  }
  /* Object dupligroup. */
  if (object->instance_collection != NULL) {
    build_collection(NULL, object, object->instance_collection);
  }
  /* Point caches. */
  build_object_pointcache(object);
  /* Syncronization back to original object. */
  OperationKey synchronize_key(
      &object->id, NodeType::SYNCHRONIZATION, OperationCode::SYNCHRONIZE_TO_ORIGINAL);
  add_relation(final_transform_key, synchronize_key, "Synchronize to Original");
  /* Parameters. */
  build_parameters(&object->id);
}

void DepsgraphRelationBuilder::build_object_flags(Base *base, Object *object)
{
  if (base == NULL) {
    return;
  }
  OperationKey view_layer_done_key(
      &scene_->id, NodeType::LAYER_COLLECTIONS, OperationCode::VIEW_LAYER_EVAL);
  OperationKey object_flags_key(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_BASE_FLAGS);
  add_relation(view_layer_done_key, object_flags_key, "Base flags flush");
  /* Syncronization back to original object. */
  OperationKey synchronize_key(
      &object->id, NodeType::SYNCHRONIZATION, OperationCode::SYNCHRONIZE_TO_ORIGINAL);
  add_relation(object_flags_key, synchronize_key, "Synchronize to Original");
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
    case OB_GPENCIL: {
      build_object_data_geometry(object);
      /* TODO(sergey): Only for until we support granular
       * update of curves. */
      if (object->type == OB_FONT) {
        Curve *curve = (Curve *)object->data;
        if (curve->textoncurve) {
          add_special_eval_flag(&curve->textoncurve->id, DAG_EVAL_NEED_CURVE_PATH);
        }
      }
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
      build_object_data_light(object);
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
    ComponentKey geometry_key((ID *)object->data, NodeType::GEOMETRY);
    ComponentKey key_key(&key->id, NodeType::GEOMETRY);
    add_relation(key_key, geometry_key, "Shapekeys");
    build_nested_shapekey(&object->id, key);
  }
  /* Materials. */
  Material ***materials_ptr = give_matarar(object);
  if (materials_ptr != NULL) {
    short *num_materials_ptr = give_totcolp(object);
    build_materials(*materials_ptr, *num_materials_ptr);
  }
}

void DepsgraphRelationBuilder::build_object_data_camera(Object *object)
{
  Camera *camera = (Camera *)object->data;
  build_camera(camera);
  ComponentKey object_parameters_key(&object->id, NodeType::PARAMETERS);
  ComponentKey camera_parameters_key(&camera->id, NodeType::PARAMETERS);
  add_relation(camera_parameters_key, object_parameters_key, "Camera -> Object");
}

void DepsgraphRelationBuilder::build_object_data_light(Object *object)
{
  Light *lamp = (Light *)object->data;
  build_light(lamp);
  ComponentKey lamp_parameters_key(&lamp->id, NodeType::PARAMETERS);
  ComponentKey object_parameters_key(&object->id, NodeType::PARAMETERS);
  add_relation(lamp_parameters_key, object_parameters_key, "Light -> Object");
}

void DepsgraphRelationBuilder::build_object_data_lightprobe(Object *object)
{
  LightProbe *probe = (LightProbe *)object->data;
  build_lightprobe(probe);
  OperationKey probe_key(&probe->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
  OperationKey object_key(&object->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
  add_relation(probe_key, object_key, "LightProbe Update");
}

void DepsgraphRelationBuilder::build_object_data_speaker(Object *object)
{
  Speaker *speaker = (Speaker *)object->data;
  build_speaker(speaker);
  ComponentKey speaker_key(&speaker->id, NodeType::AUDIO);
  ComponentKey object_key(&object->id, NodeType::AUDIO);
  add_relation(speaker_key, object_key, "Speaker Update");
}

void DepsgraphRelationBuilder::build_object_parent(Object *object)
{
  Object *parent = object->parent;
  ID *parent_id = &object->parent->id;
  ComponentKey ob_key(&object->id, NodeType::TRANSFORM);
  /* Type-specific links/ */
  switch (object->partype) {
    /* Armature Deform (Virtual Modifier) */
    case PARSKEL: {
      ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
      add_relation(parent_key, ob_key, "Armature Deform Parent");
      break;
    }

    /* Vertex Parent */
    case PARVERT1:
    case PARVERT3: {
      ComponentKey parent_key(parent_id, NodeType::GEOMETRY);
      add_relation(parent_key, ob_key, "Vertex Parent");
      /* Original index is used for optimizations of lookups for subdiv
       * only meshes.
       * TODO(sergey): This optimization got lost at 2.8, so either verify
       * we can get rid of this mask here, or bring the optimization
       * back. */
      add_customdata_mask(object->parent,
                          DEGCustomDataMeshMasks::MaskVert(CD_MASK_ORIGINDEX) |
                              DEGCustomDataMeshMasks::MaskEdge(CD_MASK_ORIGINDEX) |
                              DEGCustomDataMeshMasks::MaskFace(CD_MASK_ORIGINDEX) |
                              DEGCustomDataMeshMasks::MaskPoly(CD_MASK_ORIGINDEX));
      ComponentKey transform_key(parent_id, NodeType::TRANSFORM);
      add_relation(transform_key, ob_key, "Vertex Parent TFM");
      break;
    }

    /* Bone Parent */
    case PARBONE: {
      ComponentKey parent_bone_key(parent_id, NodeType::BONE, object->parsubstr);
      OperationKey parent_transform_key(
          parent_id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
      add_relation(parent_bone_key, ob_key, "Bone Parent");
      add_relation(parent_transform_key, ob_key, "Armature Parent");
      break;
    }

    default: {
      if (object->parent->type == OB_LATTICE) {
        /* Lattice Deform Parent - Virtual Modifier. */
        ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
        ComponentKey geom_key(parent_id, NodeType::GEOMETRY);
        add_relation(parent_key, ob_key, "Lattice Deform Parent");
        add_relation(geom_key, ob_key, "Lattice Deform Parent Geom");
      }
      else if (object->parent->type == OB_CURVE) {
        Curve *cu = (Curve *)object->parent->data;

        if (cu->flag & CU_PATH) {
          /* Follow Path. */
          ComponentKey parent_key(parent_id, NodeType::GEOMETRY);
          add_relation(parent_key, ob_key, "Curve Follow Parent");
          ComponentKey transform_key(parent_id, NodeType::TRANSFORM);
          add_relation(transform_key, ob_key, "Curve Follow TFM");
        }
        else {
          /* Standard Parent. */
          ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
          add_relation(parent_key, ob_key, "Curve Parent");
        }
      }
      else {
        /* Standard Parent. */
        ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
        add_relation(parent_key, ob_key, "Parent");
      }
      break;
    }
  }
  /* Metaballs are the odd balls here (no pun intended): they will request
   * instance-list (formerly known as dupli-list) during evaluation. This is
   * their way of interacting with all instanced surfaces, making a nice
   * effect when is used form particle system. */
  if (object->type == OB_MBALL && parent->transflag & OB_DUPLI) {
    ComponentKey parent_geometry_key(parent_id, NodeType::GEOMETRY);
    /* NOTE: Metaballs are evaluating geometry only after their transform,
     * so we only hook up to transform channel here. */
    add_relation(parent_geometry_key, ob_key, "Parent");
  }

  /* Dupliverts uses original vertex index. */
  if (parent->transflag & OB_DUPLIVERTS) {
    add_customdata_mask(parent, DEGCustomDataMeshMasks::MaskVert(CD_MASK_ORIGINDEX));
  }
}

void DepsgraphRelationBuilder::build_object_pointcache(Object *object)
{
  ComponentKey point_cache_key(&object->id, NodeType::POINT_CACHE);
  /* Different point caches are affecting different aspects of life of the
   * object. We keep track of those aspects and avoid duplicate relations. */
  enum {
    FLAG_TRANSFORM = (1 << 0),
    FLAG_GEOMETRY = (1 << 1),
    FLAG_ALL = (FLAG_TRANSFORM | FLAG_GEOMETRY),
  };
  ListBase ptcache_id_list;
  BKE_ptcache_ids_from_object(&ptcache_id_list, object, scene_, 0);
  int handled_components = 0;
  LISTBASE_FOREACH (PTCacheID *, ptcache_id, &ptcache_id_list) {
    /* Check which components needs the point cache. */
    int flag = -1;
    if (ptcache_id->type == PTCACHE_TYPE_RIGIDBODY) {
      flag = FLAG_TRANSFORM;
      OperationKey transform_key(
          &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
      add_relation(point_cache_key, transform_key, "Point Cache -> Rigid Body");
      /* Manual changes to effectors need to invalidate simulation. */
      OperationKey rigidbody_rebuild_key(
          &scene_->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_REBUILD);
      add_relation(rigidbody_rebuild_key,
                   point_cache_key,
                   "Rigid Body Rebuild -> Point Cache Reset",
                   RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    }
    else {
      flag = FLAG_GEOMETRY;
      OperationKey geometry_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
      add_relation(point_cache_key, geometry_key, "Point Cache -> Geometry");
    }
    BLI_assert(flag != -1);
    /* Tag that we did handle that component. */
    handled_components |= flag;
    if (handled_components == FLAG_ALL) {
      break;
    }
  }
  /* Manual edits to any dependency (or self) should reset the point cache. */
  if (!BLI_listbase_is_empty(&ptcache_id_list)) {
    OperationKey transform_eval_key(
        &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
    OperationKey geometry_init_key(
        &object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_INIT);
    add_relation(transform_eval_key,
                 point_cache_key,
                 "Transform Simulation -> Point Cache",
                 RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    add_relation(geometry_init_key,
                 point_cache_key,
                 "Geometry Init -> Point Cache",
                 RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
  }
  BLI_freelistN(&ptcache_id_list);
}

void DepsgraphRelationBuilder::build_constraints(ID *id,
                                                 NodeType component_type,
                                                 const char *component_subdata,
                                                 ListBase *constraints,
                                                 RootPChanMap *root_map)
{
  OperationKey constraint_op_key(id,
                                 component_type,
                                 component_subdata,
                                 (component_type == NodeType::BONE) ?
                                     OperationCode::BONE_CONSTRAINTS :
                                     OperationCode::TRANSFORM_CONSTRAINTS);
  /* Add dependencies for each constraint in turn. */
  for (bConstraint *con = (bConstraint *)constraints->first; con; con = con->next) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    /* Invalid constraint type. */
    if (cti == NULL) {
      continue;
    }
    /* Special case for camera tracking -- it doesn't use targets to
     * define relations. */
    /* TODO: we can now represent dependencies in a much richer manner,
     * so review how this is done.  */
    if (ELEM(cti->type,
             CONSTRAINT_TYPE_FOLLOWTRACK,
             CONSTRAINT_TYPE_CAMERASOLVER,
             CONSTRAINT_TYPE_OBJECTSOLVER)) {
      bool depends_on_camera = false;
      if (cti->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
        bFollowTrackConstraint *data = (bFollowTrackConstraint *)con->data;
        if (((data->clip) || (data->flag & FOLLOWTRACK_ACTIVECLIP)) && data->track[0]) {
          depends_on_camera = true;
        }
        if (data->depth_ob) {
          ComponentKey depth_transform_key(&data->depth_ob->id, NodeType::TRANSFORM);
          ComponentKey depth_geometry_key(&data->depth_ob->id, NodeType::GEOMETRY);
          add_relation(depth_transform_key, constraint_op_key, cti->name);
          add_relation(depth_geometry_key, constraint_op_key, cti->name);
        }
      }
      else if (cti->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
        depends_on_camera = true;
      }
      if (depends_on_camera && scene_->camera != NULL) {
        ComponentKey camera_key(&scene_->camera->id, NodeType::TRANSFORM);
        add_relation(camera_key, constraint_op_key, cti->name);
      }
      /* TODO(sergey): This is more a TimeSource -> MovieClip ->
       * Constraint dependency chain. */
      TimeSourceKey time_src_key;
      add_relation(time_src_key, constraint_op_key, "TimeSrc -> Animation");
    }
    else if (cti->type == CONSTRAINT_TYPE_TRANSFORM_CACHE) {
      /* TODO(kevin): This is more a TimeSource -> CacheFile -> Constraint
       * dependency chain. */
      TimeSourceKey time_src_key;
      add_relation(time_src_key, constraint_op_key, "TimeSrc -> Animation");
      bTransformCacheConstraint *data = (bTransformCacheConstraint *)con->data;
      if (data->cache_file) {
        ComponentKey cache_key(&data->cache_file->id, NodeType::CACHE);
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
        if (ELEM(con->type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
          /* Ignore IK constraints - these are handled separately
           * (on pose level). */
        }
        else if (ELEM(con->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO)) {
          /* These constraints require path geometry data. */
          ComponentKey target_key(&ct->tar->id, NodeType::GEOMETRY);
          add_relation(target_key, constraint_op_key, cti->name);
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          add_relation(target_transform_key, constraint_op_key, cti->name);
        }
        else if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) {
          OperationCode opcode;
          /* relation to bone */
          opcode = bone_target_opcode(
              &ct->tar->id, ct->subtarget, id, component_subdata, root_map);
          /* Armature constraint always wants the final position and chan_mat. */
          if (ELEM(con->type, CONSTRAINT_TYPE_ARMATURE)) {
            opcode = OperationCode::BONE_DONE;
          }
          /* if needs bbone shape, reference the segment computation */
          if (BKE_constraint_target_uses_bbone(con, ct) &&
              check_pchan_has_bbone_segments(ct->tar, ct->subtarget)) {
            opcode = OperationCode::BONE_SEGMENTS;
          }
          OperationKey target_key(&ct->tar->id, NodeType::BONE, ct->subtarget, opcode);
          add_relation(target_key, constraint_op_key, cti->name);
        }
        else if (ELEM(ct->tar->type, OB_MESH, OB_LATTICE) && (ct->subtarget[0])) {
          /* Vertex group. */
          /* NOTE: Vertex group is likely to be used to get vertices
           * in a world space. This requires to know both geometry
           * and transformation of the target object. */
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          ComponentKey target_geometry_key(&ct->tar->id, NodeType::GEOMETRY);
          add_relation(target_transform_key, constraint_op_key, cti->name);
          add_relation(target_geometry_key, constraint_op_key, cti->name);
          add_customdata_mask(ct->tar, DEGCustomDataMeshMasks::MaskVert(CD_MASK_MDEFORMVERT));
        }
        else if (con->type == CONSTRAINT_TYPE_SHRINKWRAP) {
          bShrinkwrapConstraint *scon = (bShrinkwrapConstraint *)con->data;

          /* Constraints which requires the target object surface. */
          ComponentKey target_key(&ct->tar->id, NodeType::GEOMETRY);
          add_relation(target_key, constraint_op_key, cti->name);

          /* Add dependency on normal layers if necessary. */
          if (ct->tar->type == OB_MESH && scon->shrinkType != MOD_SHRINKWRAP_NEAREST_VERTEX) {
            bool track = (scon->flag & CON_SHRINKWRAP_TRACK_NORMAL) != 0;
            if (track || BKE_shrinkwrap_needs_normals(scon->shrinkType, scon->shrinkMode)) {
              add_customdata_mask(ct->tar,
                                  DEGCustomDataMeshMasks::MaskVert(CD_MASK_NORMAL) |
                                      DEGCustomDataMeshMasks::MaskLoop(CD_MASK_CUSTOMLOOPNORMAL));
            }
            if (scon->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
              add_special_eval_flag(&ct->tar->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
            }
          }

          /* NOTE: obdata eval now doesn't necessarily depend on the
           * object's transform. */
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          add_relation(target_transform_key, constraint_op_key, cti->name);
        }
        else {
          /* Standard object relation. */
          // TODO: loc vs rot vs scale?
          if (&ct->tar->id == id) {
            /* Constraint targeting own object:
             * - This case is fine IFF we're dealing with a bone
             *   constraint pointing to its own armature. In that
             *   case, it's just transform -> bone.
             * - If however it is a real self targeting case, just
             *   make it depend on the previous constraint (or the
             *   pre-constraint state). */
            if ((ct->tar->type == OB_ARMATURE) && (component_type == NodeType::BONE)) {
              OperationKey target_key(
                  &ct->tar->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
              add_relation(target_key, constraint_op_key, cti->name);
            }
            else {
              OperationKey target_key(
                  &ct->tar->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_LOCAL);
              add_relation(target_key, constraint_op_key, cti->name);
            }
          }
          else {
            /* Normal object dependency. */
            OperationKey target_key(
                &ct->tar->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
            add_relation(target_key, constraint_op_key, cti->name);
          }
        }
        /* Constraints which needs world's matrix for transform.
         * TODO(sergey): More constraints here? */
        if (ELEM(con->type,
                 CONSTRAINT_TYPE_ROTLIKE,
                 CONSTRAINT_TYPE_SIZELIKE,
                 CONSTRAINT_TYPE_LOCLIKE,
                 CONSTRAINT_TYPE_TRANSLIKE)) {
          /* TODO(sergey): Add used space check. */
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
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
  /* Images. */
  build_animation_images(id);
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
  if (adt->action == NULL && BLI_listbase_is_empty(&adt->nla_tracks)) {
    return;
  }
  /* Ensure evaluation order from entry to exit. */
  OperationKey animation_entry_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_ENTRY);
  OperationKey animation_eval_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_EVAL);
  OperationKey animation_exit_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_EXIT);
  add_relation(animation_entry_key, animation_eval_key, "Init -> Eval");
  add_relation(animation_eval_key, animation_exit_key, "Eval -> Exit");
  /* Wire up dependency from action. */
  ComponentKey adt_key(id, NodeType::ANIMATION);
  /* Relation from action itself. */
  if (adt->action != NULL) {
    ComponentKey action_key(&adt->action->id, NodeType::ANIMATION);
    add_relation(action_key, adt_key, "Action -> Animation");
  }
  /* Get source operations. */
  Node *node_from = get_node(adt_key);
  BLI_assert(node_from != NULL);
  if (node_from == NULL) {
    return;
  }
  OperationNode *operation_from = node_from->get_exit_operation();
  BLI_assert(operation_from != NULL);
  /* Build relations from animation operation to properties it changes. */
  if (adt->action != NULL) {
    build_animdata_curves_targets(id, adt_key, operation_from, &adt->action->curves);
  }
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    build_animdata_nlastrip_targets(id, adt_key, operation_from, &nlt->strips);
  }
}

void DepsgraphRelationBuilder::build_animdata_curves_targets(ID *id,
                                                             ComponentKey &adt_key,
                                                             OperationNode *operation_from,
                                                             ListBase *curves)
{
  /* Iterate over all curves and build relations. */
  PointerRNA id_ptr;
  RNA_id_pointer_create(id, &id_ptr);
  LISTBASE_FOREACH (FCurve *, fcu, curves) {
    PointerRNA ptr;
    PropertyRNA *prop;
    int index;
    if (!RNA_path_resolve_full(&id_ptr, fcu->rna_path, &ptr, &prop, &index)) {
      continue;
    }
    Node *node_to = rna_node_query_.find_node(&ptr, prop, RNAPointerSource::ENTRY);
    if (node_to == NULL) {
      continue;
    }
    OperationNode *operation_to = node_to->get_entry_operation();
    /* NOTE: Special case for bones, avoid relation from animation to
     * each of the bones. Bone evaluation could only start from pose
     * init anyway. */
    if (operation_to->opcode == OperationCode::BONE_LOCAL) {
      OperationKey pose_init_key(id, NodeType::EVAL_POSE, OperationCode::POSE_INIT);
      add_relation(adt_key, pose_init_key, "Animation -> Prop", RELATION_CHECK_BEFORE_ADD);
      continue;
    }
    graph_->add_new_relation(
        operation_from, operation_to, "Animation -> Prop", RELATION_CHECK_BEFORE_ADD);
    /* It is possible that animation is writing to a nested ID data-block,
     * need to make sure animation is evaluated after target ID is copied. */
    const IDNode *id_node_from = operation_from->owner->owner;
    const IDNode *id_node_to = operation_to->owner->owner;
    if (id_node_from != id_node_to) {
      ComponentKey cow_key(id_node_to->id_orig, NodeType::COPY_ON_WRITE);
      add_relation(cow_key,
                   adt_key,
                   "Animated CoW -> Animation",
                   RELATION_CHECK_BEFORE_ADD | RELATION_FLAG_NO_FLUSH);
    }
  }
}

void DepsgraphRelationBuilder::build_animdata_nlastrip_targets(ID *id,
                                                               ComponentKey &adt_key,
                                                               OperationNode *operation_from,
                                                               ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    if (strip->act != NULL) {
      build_action(strip->act);

      ComponentKey action_key(&strip->act->id, NodeType::ANIMATION);
      add_relation(action_key, adt_key, "Action -> Animation");

      build_animdata_curves_targets(id, adt_key, operation_from, &strip->act->curves);
    }
    else if (strip->strips.first != NULL) {
      build_animdata_nlastrip_targets(id, adt_key, operation_from, &strip->strips);
    }
  }
}

void DepsgraphRelationBuilder::build_animdata_drivers(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == NULL) {
    return;
  }
  ComponentKey adt_key(id, NodeType::ANIMATION);
  LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
    OperationKey driver_key(id,
                            NodeType::PARAMETERS,
                            OperationCode::DRIVER,
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
     * TODO(sergey): Avoid liner lookup somehow. */
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
        if (fcu_prev == NULL || fcu_candidate->array_index > fcu_prev->array_index) {
          fcu_prev = fcu_candidate;
        }
      }
      if (fcu_prev != NULL) {
        OperationKey prev_driver_key(id,
                                     NodeType::PARAMETERS,
                                     OperationCode::DRIVER,
                                     fcu_prev->rna_path ? fcu_prev->rna_path : "",
                                     fcu_prev->array_index);
        OperationKey driver_key(id,
                                NodeType::PARAMETERS,
                                OperationCode::DRIVER,
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

void DepsgraphRelationBuilder::build_animation_images(ID *id)
{
  /* TODO: can we check for existance of node for performance? */
  if (BKE_image_user_id_has_animation(id)) {
    OperationKey image_animation_key(id, NodeType::ANIMATION, OperationCode::IMAGE_ANIMATION);
    TimeSourceKey time_src_key;
    add_relation(time_src_key, image_animation_key, "TimeSrc -> Image Animation");
  }
}

void DepsgraphRelationBuilder::build_action(bAction *action)
{
  if (built_map_.checkIsBuiltAndTag(action)) {
    return;
  }
  TimeSourceKey time_src_key;
  ComponentKey animation_key(&action->id, NodeType::ANIMATION);
  add_relation(time_src_key, animation_key, "TimeSrc -> Animation");
}

void DepsgraphRelationBuilder::build_driver(ID *id, FCurve *fcu)
{
  ChannelDriver *driver = fcu->driver;
  OperationKey driver_key(id,
                          NodeType::PARAMETERS,
                          OperationCode::DRIVER,
                          fcu->rna_path ? fcu->rna_path : "",
                          fcu->array_index);
  /* Driver -> data components (for interleaved evaluation
   * bones/constraints/modifiers). */
  build_driver_data(id, fcu);
  /* Loop over variables to get the target relationships. */
  build_driver_variables(id, fcu);
  /* It's quite tricky to detect if the driver actually depends on time or
   * not, so for now we'll be quite conservative here about optimization and
   * consider all python drivers to be depending on time. */
  if ((driver->type == DRIVER_TYPE_PYTHON) && python_driver_depends_on_time(driver)) {
    TimeSourceKey time_src_key;
    add_relation(time_src_key, driver_key, "TimeSrc -> Driver");
  }
}

void DepsgraphRelationBuilder::build_driver_data(ID *id, FCurve *fcu)
{
  /* Validate the RNA path pointer just in case. */
  const char *rna_path = fcu->rna_path;
  if (rna_path == NULL || rna_path[0] == '\0') {
    return;
  }
  /* Parse the RNA path to find the target property pointer. */
  RNAPathKey property_entry_key(id, rna_path, RNAPointerSource::ENTRY);
  if (RNA_pointer_is_null(&property_entry_key.ptr)) {
    /* TODO(sergey): This would only mean that driver is broken.
     * so we can't create relation anyway. However, we need to avoid
     * adding drivers which are known to be buggy to a dependency
     * graph, in order to save computational power. */
    return;
  }
  OperationKey driver_key(
      id, NodeType::PARAMETERS, OperationCode::DRIVER, rna_path, fcu->array_index);
  /* If the target of the driver is a Bone property, find the Armature data,
   * and then link the driver to all pose bone evaluation components that use
   * it. This is necessary to provide more granular dependencies specifically for
   * Bone objects, because the armature data doesn't have per-bone components,
   * and generic add_relation can only add one link. */
  ID *id_ptr = (ID *)property_entry_key.ptr.id.data;
  bool is_bone = id_ptr && property_entry_key.ptr.type == &RNA_Bone;
  /* If the Bone property is referenced via obj.pose.bones[].bone,
   * the RNA pointer refers to the Object ID, so skip to data. */
  if (is_bone && GS(id_ptr->name) == ID_OB) {
    id_ptr = (ID *)((Object *)id_ptr)->data;
  }
  if (is_bone && GS(id_ptr->name) == ID_AR) {
    /* Drivers on armature-level bone settings (i.e. bbone stuff),
     * which will affect the evaluation of corresponding pose bones. */
    Bone *bone = (Bone *)property_entry_key.ptr.data;
    if (bone != NULL) {
      /* Find objects which use this, and make their eval callbacks
       * depend on this. */
      for (IDNode *to_node : graph_->id_nodes) {
        if (GS(to_node->id_orig->name) == ID_OB) {
          Object *object = (Object *)to_node->id_orig;
          /* We only care about objects with pose data which use this. */
          if (object->data == id_ptr && object->pose != NULL) {
            bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone->name);
            if (pchan != NULL) {
              OperationKey bone_key(
                  &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
              add_relation(driver_key, bone_key, "Arm Bone -> Driver -> Bone");
            }
          }
        }
      }
      /* Make the driver depend on COW, similar to the generic case below. */
      if (id_ptr != id) {
        ComponentKey cow_key(id_ptr, NodeType::COPY_ON_WRITE);
        add_relation(cow_key, driver_key, "Driven CoW -> Driver", RELATION_CHECK_BEFORE_ADD);
      }
    }
    else {
      fprintf(stderr, "Couldn't find armature bone name for driver path - '%s'\n", rna_path);
    }
  }
  else {
    /* If it's not a Bone, handle the generic single dependency case. */
    add_relation(driver_key, property_entry_key, "Driver -> Driven Property");
    /* Similar to the case with f-curves, driver might drive a nested
     * data-block, which means driver execution should wait for that
     * data-block to be copied. */
    {
      PointerRNA id_ptr;
      PointerRNA ptr;
      RNA_id_pointer_create(id, &id_ptr);
      if (RNA_path_resolve_full(&id_ptr, fcu->rna_path, &ptr, NULL, NULL)) {
        if (id_ptr.id.data != ptr.id.data) {
          ComponentKey cow_key((ID *)ptr.id.data, NodeType::COPY_ON_WRITE);
          add_relation(cow_key, driver_key, "Driven CoW -> Driver", RELATION_CHECK_BEFORE_ADD);
        }
      }
    }
    if (property_entry_key.prop != NULL && RNA_property_is_idprop(property_entry_key.prop)) {
      RNAPathKey property_exit_key(id, rna_path, RNAPointerSource::EXIT);
      OperationKey parameters_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL);
      add_relation(property_exit_key, parameters_key, "Driven Property -> Properties");
    }
  }
}

void DepsgraphRelationBuilder::build_driver_variables(ID *id, FCurve *fcu)
{
  ChannelDriver *driver = fcu->driver;
  OperationKey driver_key(id,
                          NodeType::PARAMETERS,
                          OperationCode::DRIVER,
                          fcu->rna_path ? fcu->rna_path : "",
                          fcu->array_index);
  const char *rna_path = fcu->rna_path ? fcu->rna_path : "";
  const RNAPathKey self_key(id, rna_path, RNAPointerSource::ENTRY);
  LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
    /* Only used targets. */
    DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
      ID *target_id = dtar->id;
      if (target_id == NULL) {
        continue;
      }
      build_id(target_id);
      build_driver_id_property(target_id, dtar->rna_path);
      /* Look up the proxy - matches dtar_id_ensure_proxy_from during evaluation. */
      Object *object = NULL;
      if (GS(target_id->name) == ID_OB) {
        object = (Object *)target_id;
        if (object->proxy_from != NULL) {
          /* Redirect the target to the proxy, like in evaluation. */
          object = object->proxy_from;
          target_id = &object->id;
          /* Prepare the redirected target. */
          build_id(target_id);
          build_driver_id_property(target_id, dtar->rna_path);
        }
      }
      /* Special handling for directly-named bones. */
      if ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (object && object->type == OB_ARMATURE) &&
          (dtar->pchan_name[0])) {
        bPoseChannel *target_pchan = BKE_pose_channel_find_name(object->pose, dtar->pchan_name);
        if (target_pchan == NULL) {
          continue;
        }
        OperationKey variable_key(
            target_id, NodeType::BONE, target_pchan->name, OperationCode::BONE_DONE);
        if (is_same_bone_dependency(variable_key, self_key)) {
          continue;
        }
        add_relation(variable_key, driver_key, "Bone Target -> Driver");
      }
      else if (dtar->flag & DTAR_FLAG_STRUCT_REF) {
        /* Get node associated with the object's transforms. */
        if (target_id == id) {
          /* Ignore input dependency if we're driving properties of
           * the same ID, otherwise we'll be ending up in a cyclic
           * dependency here. */
          continue;
        }
        OperationKey target_key(target_id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
        add_relation(target_key, driver_key, "Target -> Driver");
      }
      else if (dtar->rna_path != NULL && dtar->rna_path[0] != '\0') {
        RNAPathKey variable_exit_key(target_id, dtar->rna_path, RNAPointerSource::EXIT);
        if (RNA_pointer_is_null(&variable_exit_key.ptr)) {
          continue;
        }
        if (is_same_bone_dependency(variable_exit_key, self_key) ||
            is_same_nodetree_node_dependency(variable_exit_key, self_key)) {
          continue;
        }
        add_relation(variable_exit_key, driver_key, "RNA Target -> Driver");
      }
      else {
        /* If rna_path is NULL, and DTAR_FLAG_STRUCT_REF isn't set, this
         * is an incomplete target reference, so nothing to do here. */
      }
    }
    DRIVER_TARGETS_LOOPER_END;
  }
}

void DepsgraphRelationBuilder::build_driver_id_property(ID *id, const char *rna_path)
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
  OperationKey id_property_key(
      id, NodeType::PARAMETERS, OperationCode::ID_PROPERTY, prop_identifier);
  OperationKey parameters_exit_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
  add_relation(
      id_property_key, parameters_exit_key, "ID Property -> Done", RELATION_CHECK_BEFORE_ADD);
}

void DepsgraphRelationBuilder::build_parameters(ID *id)
{
  OperationKey parameters_entry_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_ENTRY);
  OperationKey parameters_eval_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL);
  OperationKey parameters_exit_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
  add_relation(parameters_entry_key, parameters_eval_key, "Entry -> Eval");
  add_relation(parameters_eval_key, parameters_exit_key, "Entry -> Exit");
}

void DepsgraphRelationBuilder::build_world(World *world)
{
  if (built_map_.checkIsBuiltAndTag(world)) {
    return;
  }
  /* animation */
  build_animdata(&world->id);
  build_parameters(&world->id);
  /* world's nodetree */
  if (world->nodetree != NULL) {
    build_nodetree(world->nodetree);
    OperationKey ntree_key(
        &world->nodetree->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
    OperationKey world_key(&world->id, NodeType::SHADING, OperationCode::WORLD_UPDATE);
    add_relation(ntree_key, world_key, "World's NTree");
    build_nested_nodetree(&world->id, world->nodetree);
  }
}

void DepsgraphRelationBuilder::build_rigidbody(Scene *scene)
{
  RigidBodyWorld *rbw = scene->rigidbody_world;
  OperationKey rb_init_key(&scene->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_REBUILD);
  OperationKey rb_simulate_key(&scene->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_SIM);
  /* Simulation depends on time. */
  TimeSourceKey time_src_key;
  add_relation(time_src_key, rb_init_key, "TimeSrc -> Rigidbody Init");
  /* Simulation should always be run after initialization. */
  /* NOTE: It is possible in theory to have dependency cycle which involves
   * this relation. We never want it to be killed. */
  add_relation(rb_init_key, rb_simulate_key, "Rigidbody [Init -> SimStep]", RELATION_FLAG_GODMODE);
  /* Effectors should be evaluated at the time simulation is being
   * initialized.
   * TODO(sergey): Verify that it indeed goes to initialization and not to a
   * simulation. */
  ListBase *effector_relations = build_effector_relations(graph_, rbw->effector_weights->group);
  LISTBASE_FOREACH (EffectorRelation *, effector_relation, effector_relations) {
    ComponentKey effector_transform_key(&effector_relation->ob->id, NodeType::TRANSFORM);
    add_relation(effector_transform_key, rb_init_key, "RigidBody Field");
    if (effector_relation->pd != NULL) {
      const short shape = effector_relation->pd->shape;
      if (ELEM(shape, PFIELD_SHAPE_SURFACE, PFIELD_SHAPE_POINTS)) {
        ComponentKey effector_geometry_key(&effector_relation->ob->id, NodeType::GEOMETRY);
        add_relation(effector_geometry_key, rb_init_key, "RigidBody Field");
      }
    }
  }
  /* Objects. */
  if (rbw->group != NULL) {
    build_collection(NULL, NULL, rbw->group);
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
      if (object->type != OB_MESH) {
        continue;
      }
      OperationKey rb_transform_copy_key(
          &object->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_TRANSFORM_COPY);
      /* Rigid body synchronization depends on the actual simulation. */
      add_relation(rb_simulate_key, rb_transform_copy_key, "Rigidbody Sim Eval -> RBO Sync");
      /* Simulation uses object transformation after parenting and solving constraints. */
      OperationKey object_transform_simulation_init_key(
          &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
      OperationKey object_transform_eval_key(
          &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
      add_relation(object_transform_simulation_init_key,
                   rb_simulate_key,
                   "Object Transform -> Rigidbody Sim Eval");
      /* Geometry must be known to create the rigid body. RBO_MESH_BASE
       * uses the non-evaluated mesh, so then the evaluation is
       * unnecessary. */
      if (object->rigidbody_object != NULL &&
          object->rigidbody_object->mesh_source != RBO_MESH_BASE) {
        /* NOTE: We prefer this relation to be never killed, to avoid
         * access partially evaluated mesh from solver. */
        ComponentKey object_geometry_key(&object->id, NodeType::GEOMETRY);
        add_relation(object_geometry_key,
                     rb_simulate_key,
                     "Object Geom Eval -> Rigidbody Rebuild",
                     RELATION_FLAG_GODMODE);
      }
      /* Final transform is whetever solver gave to us. */
      OperationKey object_transform_final_key(
          &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
      add_relation(
          rb_transform_copy_key, object_transform_final_key, "Rigidbody Sync -> Transform Final");
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
  /* Constraints. */
  if (rbw->constraints != NULL) {
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->constraints, object) {
      RigidBodyCon *rbc = object->rigidbody_constraint;
      if (rbc == NULL || rbc->ob1 == NULL || rbc->ob2 == NULL) {
        /* When either ob1 or ob2 is NULL, the constraint doesn't
         * work. */
        continue;
      }
      /* Make sure indirectly linked objects are fully built. */
      build_object(NULL, object);
      build_object(NULL, rbc->ob1);
      build_object(NULL, rbc->ob2);
      /* final result of the constraint object's transform controls how
       * the constraint affects the physics sim for these objects. */
      ComponentKey trans_key(&object->id, NodeType::TRANSFORM);
      OperationKey ob1_key(
          &rbc->ob1->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_TRANSFORM_COPY);
      OperationKey ob2_key(
          &rbc->ob2->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_TRANSFORM_COPY);
      /* Constrained-objects sync depends on the constraint-holder. */
      add_relation(trans_key, ob1_key, "RigidBodyConstraint -> RBC.Object_1");
      add_relation(trans_key, ob2_key, "RigidBodyConstraint -> RBC.Object_2");
      /* Ensure that sim depends on this constraint's transform. */
      add_relation(trans_key, rb_simulate_key, "RigidBodyConstraint Transform -> RB Simulation");
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
}

void DepsgraphRelationBuilder::build_particle_systems(Object *object)
{
  TimeSourceKey time_src_key;
  OperationKey obdata_ubereval_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
  OperationKey eval_init_key(
      &object->id, NodeType::PARTICLE_SYSTEM, OperationCode::PARTICLE_SYSTEM_INIT);
  OperationKey eval_done_key(
      &object->id, NodeType::PARTICLE_SYSTEM, OperationCode::PARTICLE_SYSTEM_DONE);
  ComponentKey eval_key(&object->id, NodeType::PARTICLE_SYSTEM);
  if (BKE_ptcache_object_has(scene_, object, 0)) {
    ComponentKey point_cache_key(&object->id, NodeType::POINT_CACHE);
    add_relation(
        eval_key, point_cache_key, "Particle Point Cache", RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
  }
  /* Particle systems. */
  LISTBASE_FOREACH (ParticleSystem *, psys, &object->particlesystem) {
    ParticleSettings *part = psys->part;
    /* Build particle settings relations.
     * NOTE: The call itself ensures settings are only build once. */
    build_particle_settings(part);
    /* This particle system. */
    OperationKey psys_key(
        &object->id, NodeType::PARTICLE_SYSTEM, OperationCode::PARTICLE_SYSTEM_EVAL, psys->name);
    /* Update particle system when settings changes. */
    OperationKey particle_settings_key(
        &part->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_EVAL);
    add_relation(particle_settings_key, eval_init_key, "Particle Settings Change");
    add_relation(eval_init_key, psys_key, "Init -> PSys");
    add_relation(psys_key, eval_done_key, "PSys -> Done");
    /* TODO(sergey): Currently particle update is just a placeholder,
     * hook it to the ubereval node so particle system is getting updated
     * on playback. */
    add_relation(psys_key, obdata_ubereval_key, "PSys -> UberEval");
    /* Collisions. */
    if (part->type != PART_HAIR) {
      add_particle_collision_relations(
          psys_key, object, part->collision_group, "Particle Collision");
    }
    else if ((psys->flag & PSYS_HAIR_DYNAMICS) && psys->clmd != NULL &&
             psys->clmd->coll_parms != NULL) {
      add_particle_collision_relations(
          psys_key, object, psys->clmd->coll_parms->group, "Hair Collision");
    }
    /* Effectors. */
    add_particle_forcefield_relations(
        psys_key, object, psys, part->effector_weights, part->type == PART_HAIR, "Particle Field");
    /* Boids .*/
    if (part->boids != NULL) {
      LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
        LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
          Object *ruleob = NULL;
          if (rule->type == eBoidRuleType_Avoid) {
            ruleob = ((BoidRuleGoalAvoid *)rule)->ob;
          }
          else if (rule->type == eBoidRuleType_FollowLeader) {
            ruleob = ((BoidRuleFollowLeader *)rule)->ob;
          }
          if (ruleob != NULL) {
            ComponentKey ruleob_key(&ruleob->id, NodeType::TRANSFORM);
            add_relation(ruleob_key, psys_key, "Boid Rule");
          }
        }
      }
    }
    /* Keyed particle targets. */
    if (part->phystype == PART_PHYS_KEYED) {
      LISTBASE_FOREACH (ParticleTarget *, particle_target, &psys->targets) {
        if (particle_target->ob == NULL || particle_target->ob == object) {
          continue;
        }
        /* Make sure target object is pulled into the graph. */
        build_object(NULL, particle_target->ob);
        /* Use geometry component, since that's where particles are
         * actually evaluated. */
        ComponentKey target_key(&particle_target->ob->id, NodeType::GEOMETRY);
        add_relation(target_key, psys_key, "Keyed Target");
      }
    }
    /* Visualization. */
    switch (part->ren_as) {
      case PART_DRAW_OB:
        if (part->instance_object != NULL) {
          /* Make sure object's relations are all built.  */
          build_object(NULL, part->instance_object);
          /* Build relation for the particle visualization. */
          build_particle_system_visualization_object(object, psys, part->instance_object);
        }
        break;
      case PART_DRAW_GR:
        if (part->instance_collection != NULL) {
          build_collection(NULL, NULL, part->instance_collection);
          LISTBASE_FOREACH (CollectionObject *, go, &part->instance_collection->gobject) {
            build_particle_system_visualization_object(object, psys, go->ob);
          }
        }
        break;
    }
  }
  /* Particle depends on the object transform, so that channel is to be ready
   * first. */
  add_depends_on_transform_relation(&object->id, obdata_ubereval_key, "Particle Eval");
}

void DepsgraphRelationBuilder::build_particle_settings(ParticleSettings *part)
{
  if (built_map_.checkIsBuiltAndTag(part)) {
    return;
  }
  /* Animation data relations. */
  build_animdata(&part->id);
  build_parameters(&part->id);
  OperationKey particle_settings_init_key(
      &part->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_INIT);
  OperationKey particle_settings_eval_key(
      &part->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_EVAL);
  OperationKey particle_settings_reset_key(
      &part->id, NodeType::PARTICLE_SETTINGS, OperationCode::PARTICLE_SETTINGS_RESET);
  add_relation(
      particle_settings_init_key, particle_settings_eval_key, "Particle Settings Init Order");
  add_relation(particle_settings_reset_key, particle_settings_eval_key, "Particle Settings Reset");
  /* Texture slots. */
  for (int mtex_index = 0; mtex_index < MAX_MTEX; ++mtex_index) {
    MTex *mtex = part->mtex[mtex_index];
    if (mtex == NULL || mtex->tex == NULL) {
      continue;
    }
    build_texture(mtex->tex);
    ComponentKey texture_key(&mtex->tex->id, NodeType::GENERIC_DATABLOCK);
    add_relation(texture_key,
                 particle_settings_reset_key,
                 "Particle Texture",
                 RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    /* TODO(sergey): Consider moving texture space handling to an own
     * function. */
    if (mtex->texco == TEXCO_OBJECT && mtex->object != NULL) {
      ComponentKey object_key(&mtex->object->id, NodeType::TRANSFORM);
      add_relation(object_key, particle_settings_eval_key, "Particle Texture Space");
    }
  }
  if (check_id_has_anim_component(&part->id)) {
    ComponentKey animation_key(&part->id, NodeType::ANIMATION);
    add_relation(animation_key, particle_settings_eval_key, "Particle Settings Animation");
  }
}

void DepsgraphRelationBuilder::build_particle_system_visualization_object(Object *object,
                                                                          ParticleSystem *psys,
                                                                          Object *draw_object)
{
  OperationKey psys_key(
      &object->id, NodeType::PARTICLE_SYSTEM, OperationCode::PARTICLE_SYSTEM_EVAL, psys->name);
  OperationKey obdata_ubereval_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
  ComponentKey dup_ob_key(&draw_object->id, NodeType::TRANSFORM);
  add_relation(dup_ob_key, psys_key, "Particle Object Visualization");
  if (draw_object->type == OB_MBALL) {
    ComponentKey dup_geometry_key(&draw_object->id, NodeType::GEOMETRY);
    add_relation(obdata_ubereval_key, dup_geometry_key, "Particle MBall Visualization");
  }
}

/* Shapekeys */
void DepsgraphRelationBuilder::build_shapekeys(Key *key)
{
  if (built_map_.checkIsBuiltAndTag(key)) {
    return;
  }
  /* Attach animdata to geometry. */
  build_animdata(&key->id);
  build_parameters(&key->id);
  /* Connect all blocks properties to the final result evaluation. */
  ComponentKey geometry_key(&key->id, NodeType::GEOMETRY);
  OperationKey parameters_eval_key(&key->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL);
  LISTBASE_FOREACH (KeyBlock *, key_block, &key->block) {
    OperationKey key_block_key(
        &key->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL, key_block->name);
    add_relation(key_block_key, geometry_key, "Key Block Properties");
    add_relation(key_block_key, parameters_eval_key, "Key Block Properties");
  }
}

/**
 * ObData Geometry Evaluation
 * ==========================
 *
 * The evaluation of geometry on objects is as follows:
 * - The actual evaluated of the derived geometry (e.g. Mesh, DispList)
 *   occurs in the Geometry component of the object which references this.
 *   This includes modifiers, and the temporary "ubereval" for geometry.
 *   Therefore, each user of a piece of shared geometry data ends up evaluating
 *   its own version of the stuff, complete with whatever modifiers it may use.
 *
 * - The data-blocks for the geometry data - "obdata" (e.g. ID_ME, ID_CU, ID_LT.)
 *   are used for
 *     1) calculating the bounding boxes of the geometry data,
 *     2) aggregating inward links from other objects (e.g. for text on curve)
 *        and also for the links coming from the shapekey data-blocks
 * - Animation/Drivers affecting the parameters of the geometry are made to
 *   trigger updates on the obdata geometry component, which then trigger
 *   downstream re-evaluation of the individual instances of this geometry. */
void DepsgraphRelationBuilder::build_object_data_geometry(Object *object)
{
  ID *obdata = (ID *)object->data;
  /* Init operation of object-level geometry evaluation. */
  OperationKey geom_init_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_INIT);
  /* Get nodes for result of obdata's evaluation, and geometry evaluation
   * on object. */
  ComponentKey obdata_geom_key(obdata, NodeType::GEOMETRY);
  ComponentKey geom_key(&object->id, NodeType::GEOMETRY);
  /* Link components to each other. */
  add_relation(obdata_geom_key, geom_key, "Object Geometry Base Data");
  OperationKey obdata_ubereval_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
  /* Special case: modifiers evaluation queries scene for various things like
   * data mask to be used. We add relation here to ensure object is never
   * evaluated prior to Scene's CoW is ready. */
  OperationKey scene_key(&scene_->id, NodeType::PARAMETERS, OperationCode::SCENE_EVAL);
  Relation *rel = add_relation(scene_key, obdata_ubereval_key, "CoW Relation");
  rel->flag |= RELATION_FLAG_NO_FLUSH;
  /* Modifiers */
  if (object->modifiers.first != NULL) {
    ModifierUpdateDepsgraphContext ctx = {};
    ctx.scene = scene_;
    ctx.object = object;
    LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
      const ModifierTypeInfo *mti = modifierType_getInfo((ModifierType)md->type);
      if (mti->updateDepsgraph) {
        DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
        ctx.node = reinterpret_cast<::DepsNodeHandle *>(&handle);
        mti->updateDepsgraph(md, &ctx);
      }
      if (BKE_object_modifier_use_time(object, md)) {
        TimeSourceKey time_src_key;
        add_relation(time_src_key, obdata_ubereval_key, "Time Source");
      }
    }
  }
  /* Grease Pencil Modifiers. */
  if (object->greasepencil_modifiers.first != NULL) {
    ModifierUpdateDepsgraphContext ctx = {};
    ctx.scene = scene_;
    ctx.object = object;
    LISTBASE_FOREACH (GpencilModifierData *, md, &object->greasepencil_modifiers) {
      const GpencilModifierTypeInfo *mti = BKE_gpencil_modifierType_getInfo(
          (GpencilModifierType)md->type);
      if (mti->updateDepsgraph) {
        DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
        ctx.node = reinterpret_cast<::DepsNodeHandle *>(&handle);
        mti->updateDepsgraph(md, &ctx);
      }
      if (BKE_object_modifier_gpencil_use_time(object, md)) {
        TimeSourceKey time_src_key;
        add_relation(time_src_key, obdata_ubereval_key, "Time Source");
      }
    }
  }
  /* Shader FX. */
  if (object->shader_fx.first != NULL) {
    ModifierUpdateDepsgraphContext ctx = {};
    ctx.scene = scene_;
    ctx.object = object;
    LISTBASE_FOREACH (ShaderFxData *, fx, &object->shader_fx) {
      const ShaderFxTypeInfo *fxi = BKE_shaderfxType_getInfo((ShaderFxType)fx->type);
      if (fxi->updateDepsgraph) {
        DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
        ctx.node = reinterpret_cast<::DepsNodeHandle *>(&handle);
        fxi->updateDepsgraph(fx, &ctx);
      }
      if (BKE_object_shaderfx_use_time(object, fx)) {
        TimeSourceKey time_src_key;
        add_relation(time_src_key, obdata_ubereval_key, "Time Source");
      }
    }
  }
  /* Materials. */
  build_materials(object->mat, object->totcol);
  /* Geometry collision. */
  if (ELEM(object->type, OB_MESH, OB_CURVE, OB_LATTICE)) {
    // add geometry collider relations
  }
  /* Make sure uber update is the last in the dependencies. */
  if (object->type != OB_ARMATURE) {
    /* Armatures does no longer require uber node. */
    OperationKey obdata_ubereval_key(
        &object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
    add_relation(geom_init_key, obdata_ubereval_key, "Object Geometry UberEval");
  }
  if (object->type == OB_MBALL) {
    Object *mom = BKE_mball_basis_find(scene_, object);
    ComponentKey mom_geom_key(&mom->id, NodeType::GEOMETRY);
    /* motherball - mom depends on children! */
    if (mom == object) {
      ComponentKey mom_transform_key(&mom->id, NodeType::TRANSFORM);
      add_relation(mom_transform_key, mom_geom_key, "Metaball Motherball Transform -> Geometry");
    }
    else {
      ComponentKey transform_key(&object->id, NodeType::TRANSFORM);
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
   * Ideally we need to get rid of this relation. */
  if (object_particles_depends_on_time(object)) {
    TimeSourceKey time_key;
    OperationKey obdata_ubereval_key(
        &object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
    add_relation(time_key, obdata_ubereval_key, "Legacy particle time");
  }
  /* Object data data-block. */
  build_object_data_geometry_datablock((ID *)object->data);
  Key *key = BKE_key_from_object(object);
  if (key != NULL) {
    if (key->adt != NULL) {
      if (key->adt->action || key->adt->nla_tracks.first) {
        ComponentKey obdata_key((ID *)object->data, NodeType::GEOMETRY);
        ComponentKey adt_key(&key->id, NodeType::ANIMATION);
        add_relation(adt_key, obdata_key, "Animation");
      }
    }
  }
  /* Syncronization back to original object. */
  ComponentKey final_geometry_key(&object->id, NodeType::GEOMETRY);
  OperationKey synchronize_key(
      &object->id, NodeType::SYNCHRONIZATION, OperationCode::SYNCHRONIZE_TO_ORIGINAL);
  add_relation(final_geometry_key, synchronize_key, "Synchronize to Original");
  /* Batch cache. */
  OperationKey object_data_select_key(
      obdata, NodeType::BATCH_CACHE, OperationCode::GEOMETRY_SELECT_UPDATE);
  OperationKey object_select_key(
      &object->id, NodeType::BATCH_CACHE, OperationCode::GEOMETRY_SELECT_UPDATE);
  add_relation(object_data_select_key, object_select_key, "Data Selection -> Object Selection");
  add_relation(
      geom_key, object_select_key, "Object Geometry -> Select Update", RELATION_FLAG_NO_FLUSH);
}

void DepsgraphRelationBuilder::build_object_data_geometry_datablock(ID *obdata)
{
  if (built_map_.checkIsBuiltAndTag(obdata)) {
    return;
  }
  /* Animation. */
  build_animdata(obdata);
  build_parameters(obdata);
  /* ShapeKeys. */
  Key *key = BKE_key_from_id(obdata);
  if (key != NULL) {
    build_shapekeys(key);
  }
  /* Link object data evaluation node to exit operation. */
  OperationKey obdata_geom_eval_key(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
  OperationKey obdata_geom_done_key(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE);
  add_relation(obdata_geom_eval_key, obdata_geom_done_key, "ObData Geom Eval Done");
  /* Type-specific links. */
  const ID_Type id_type = GS(obdata->name);
  switch (id_type) {
    case ID_ME:
      break;
    case ID_MB:
      break;
    case ID_CU: {
      Curve *cu = (Curve *)obdata;
      if (cu->bevobj != NULL) {
        ComponentKey bevob_geom_key(&cu->bevobj->id, NodeType::GEOMETRY);
        add_relation(bevob_geom_key, obdata_geom_eval_key, "Curve Bevel Geometry");
        ComponentKey bevob_key(&cu->bevobj->id, NodeType::TRANSFORM);
        add_relation(bevob_key, obdata_geom_eval_key, "Curve Bevel Transform");
        build_object(NULL, cu->bevobj);
      }
      if (cu->taperobj != NULL) {
        ComponentKey taperob_key(&cu->taperobj->id, NodeType::GEOMETRY);
        add_relation(taperob_key, obdata_geom_eval_key, "Curve Taper");
        build_object(NULL, cu->taperobj);
      }
      if (cu->textoncurve != NULL) {
        ComponentKey textoncurve_key(&cu->textoncurve->id, NodeType::GEOMETRY);
        add_relation(textoncurve_key, obdata_geom_eval_key, "Text on Curve");
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
       * onion skinning is enabled, since the ghosts need to be
       * re-added to the cache once scrubbing ends). */
      TimeSourceKey time_key;
      ComponentKey geometry_key(obdata, NodeType::GEOMETRY);
      add_relation(time_key, geometry_key, "GP Frame Change");

      /* Geometry cache also needs to be recalculated when Material
       * settings change (e.g. when fill.opacity changes on/off,
       * we need to rebuild the bGPDstroke->triangles caches). */
      for (int i = 0; i < gpd->totcol; i++) {
        Material *ma = gpd->mat[i];
        if ((ma != NULL) && (ma->gp_style != NULL)) {
          OperationKey material_key(&ma->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
          add_relation(material_key, geometry_key, "Material -> GP Data");
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
  build_parameters(&armature->id);
}

void DepsgraphRelationBuilder::build_camera(Camera *camera)
{
  if (built_map_.checkIsBuiltAndTag(camera)) {
    return;
  }
  build_animdata(&camera->id);
  build_parameters(&camera->id);
  if (camera->dof.focus_object != NULL) {
    build_object(NULL, camera->dof.focus_object);
    ComponentKey camera_parameters_key(&camera->id, NodeType::PARAMETERS);
    ComponentKey dof_ob_key(&camera->dof.focus_object->id, NodeType::TRANSFORM);
    add_relation(dof_ob_key, camera_parameters_key, "Camera DOF");
  }
}

/* Lights */
void DepsgraphRelationBuilder::build_light(Light *lamp)
{
  if (built_map_.checkIsBuiltAndTag(lamp)) {
    return;
  }
  build_animdata(&lamp->id);
  build_parameters(&lamp->id);
  /* light's nodetree */
  if (lamp->nodetree != NULL) {
    build_nodetree(lamp->nodetree);
    ComponentKey lamp_parameters_key(&lamp->id, NodeType::PARAMETERS);
    ComponentKey nodetree_key(&lamp->nodetree->id, NodeType::SHADING);
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
  build_parameters(&ntree->id);
  ComponentKey shading_key(&ntree->id, NodeType::SHADING);
  /* nodetree's nodes... */
  LISTBASE_FOREACH (bNode *, bnode, &ntree->nodes) {
    ID *id = bnode->id;
    if (id == NULL) {
      continue;
    }
    ID_Type id_type = GS(id->name);
    if (id_type == ID_MA) {
      build_material((Material *)bnode->id);
      ComponentKey material_key(id, NodeType::SHADING);
      add_relation(material_key, shading_key, "Material -> Node");
    }
    else if (id_type == ID_TE) {
      build_texture((Tex *)bnode->id);
      ComponentKey texture_key(id, NodeType::GENERIC_DATABLOCK);
      add_relation(texture_key, shading_key, "Texture -> Node");
    }
    else if (id_type == ID_IM) {
      build_image((Image *)bnode->id);
      ComponentKey image_key(id, NodeType::GENERIC_DATABLOCK);
      add_relation(image_key, shading_key, "Image -> Node");
    }
    else if (id_type == ID_OB) {
      build_object(NULL, (Object *)id);
      ComponentKey object_transform_key(id, NodeType::TRANSFORM);
      add_relation(object_transform_key, shading_key, "Object Transform -> Node");
      if (object_have_geometry_component(reinterpret_cast<Object *>(id))) {
        ComponentKey object_geometry_key(id, NodeType::GEOMETRY);
        add_relation(object_geometry_key, shading_key, "Object Geometry -> Node");
      }
    }
    else if (id_type == ID_SCE) {
      Scene *node_scene = (Scene *)id;
      build_scene_parameters(node_scene);
      /* Camera is used by defocus node.
       *
       * On the one hand it's annoying to always pull it in, but on another hand it's also annoying
       * to have hardcoded node-type exception here. */
      if (node_scene->camera != NULL) {
        build_object(NULL, node_scene->camera);
      }
    }
    else if (id_type == ID_TXT) {
      /* Ignore script nodes. */
    }
    else if (id_type == ID_MSK) {
      build_mask((Mask *)id);
      OperationKey mask_key(id, NodeType::PARAMETERS, OperationCode::MASK_EVAL);
      add_relation(mask_key, shading_key, "Mask -> Node");
    }
    else if (id_type == ID_MC) {
      build_movieclip((MovieClip *)id);
      OperationKey clip_key(id, NodeType::PARAMETERS, OperationCode::MOVIECLIP_EVAL);
      add_relation(clip_key, shading_key, "Clip -> Node");
    }
    else if (ELEM(bnode->type, NODE_GROUP, NODE_CUSTOM_GROUP)) {
      bNodeTree *group_ntree = (bNodeTree *)id;
      build_nodetree(group_ntree);
      ComponentKey group_shading_key(&group_ntree->id, NodeType::SHADING);
      add_relation(group_shading_key, shading_key, "Group Node");
    }
    else {
      BLI_assert(!"Unknown ID type used for node");
    }
  }

  OperationKey shading_update_key(&ntree->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
  OperationKey shading_parameters_key(
      &ntree->id, NodeType::SHADING_PARAMETERS, OperationCode::MATERIAL_UPDATE);
  add_relation(shading_parameters_key, shading_update_key, "NTree Shading Parameters");

  if (check_id_has_anim_component(&ntree->id)) {
    ComponentKey animation_key(&ntree->id, NodeType::ANIMATION);
    add_relation(animation_key, shading_parameters_key, "NTree Shading Parameters");
  }
  ComponentKey parameters_key(&ntree->id, NodeType::PARAMETERS);
  add_relation(parameters_key, shading_parameters_key, "NTree Shading Parameters");
}

/* Recursively build graph for material */
void DepsgraphRelationBuilder::build_material(Material *material)
{
  if (built_map_.checkIsBuiltAndTag(material)) {
    return;
  }
  /* animation */
  build_animdata(&material->id);
  build_parameters(&material->id);
  /* material's nodetree */
  if (material->nodetree != NULL) {
    build_nodetree(material->nodetree);
    OperationKey ntree_key(
        &material->nodetree->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
    OperationKey material_key(&material->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
    add_relation(ntree_key, material_key, "Material's NTree");
    build_nested_nodetree(&material->id, material->nodetree);
  }
}

void DepsgraphRelationBuilder::build_materials(Material **materials, int num_materials)
{
  for (int i = 0; i < num_materials; ++i) {
    if (materials[i] == NULL) {
      continue;
    }
    build_material(materials[i]);
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
  build_parameters(&texture->id);
  /* texture's nodetree */
  build_nodetree(texture->nodetree);
  /* Special cases for different IDs which texture uses. */
  if (texture->type == TEX_IMAGE) {
    if (texture->ima != NULL) {
      build_image(texture->ima);
    }
  }
  build_nested_nodetree(&texture->id, texture->nodetree);
  if (check_id_has_anim_component(&texture->id)) {
    ComponentKey animation_key(&texture->id, NodeType::ANIMATION);
    ComponentKey datablock_key(&texture->id, NodeType::GENERIC_DATABLOCK);
    add_relation(animation_key, datablock_key, "Datablock Animation");
  }
}

void DepsgraphRelationBuilder::build_image(Image *image)
{
  if (built_map_.checkIsBuiltAndTag(image)) {
    return;
  }
  build_parameters(&image->id);
}

void DepsgraphRelationBuilder::build_gpencil(bGPdata *gpd)
{
  if (built_map_.checkIsBuiltAndTag(gpd)) {
    return;
  }
  /* animation */
  build_animdata(&gpd->id);
  build_parameters(&gpd->id);

  // TODO: parent object (when that feature is implemented)
}

void DepsgraphRelationBuilder::build_cachefile(CacheFile *cache_file)
{
  if (built_map_.checkIsBuiltAndTag(cache_file)) {
    return;
  }
  /* Animation. */
  build_animdata(&cache_file->id);
  build_parameters(&cache_file->id);
  if (check_id_has_anim_component(&cache_file->id)) {
    ComponentKey animation_key(&cache_file->id, NodeType::ANIMATION);
    ComponentKey datablock_key(&cache_file->id, NodeType::CACHE);
    add_relation(animation_key, datablock_key, "Datablock Animation");
  }

  /* Cache file updates */
  if (cache_file->is_sequence) {
    OperationKey cache_update_key(
        &cache_file->id, NodeType::CACHE, OperationCode::FILE_CACHE_UPDATE);
    TimeSourceKey time_src_key;
    add_relation(time_src_key, cache_update_key, "TimeSrc -> Cache File Eval");
  }
}

void DepsgraphRelationBuilder::build_mask(Mask *mask)
{
  if (built_map_.checkIsBuiltAndTag(mask)) {
    return;
  }
  ID *mask_id = &mask->id;
  /* F-Curve animation. */
  build_animdata(mask_id);
  build_parameters(mask_id);
  /* Own mask animation. */
  OperationKey mask_animation_key(mask_id, NodeType::ANIMATION, OperationCode::MASK_ANIMATION);
  TimeSourceKey time_src_key;
  add_relation(time_src_key, mask_animation_key, "TimeSrc -> Mask Animation");
  /* Final mask evaluation. */
  OperationKey mask_eval_key(mask_id, NodeType::PARAMETERS, OperationCode::MASK_EVAL);
  add_relation(mask_animation_key, mask_eval_key, "Mask Animation -> Mask Eval");
  /* Build parents. */
  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      for (int i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];
        MaskParent *parent = &point->parent;
        if (parent == NULL || parent->id == NULL) {
          continue;
        }
        build_id(parent->id);
        if (parent->id_type == ID_MC) {
          OperationKey movieclip_eval_key(
              parent->id, NodeType::PARAMETERS, OperationCode::MOVIECLIP_EVAL);
          add_relation(movieclip_eval_key, mask_eval_key, "Movie Clip -> Mask Eval");
        }
      }
    }
  }
}

void DepsgraphRelationBuilder::build_movieclip(MovieClip *clip)
{
  if (built_map_.checkIsBuiltAndTag(clip)) {
    return;
  }
  /* Animation. */
  build_animdata(&clip->id);
  build_parameters(&clip->id);
}

void DepsgraphRelationBuilder::build_lightprobe(LightProbe *probe)
{
  if (built_map_.checkIsBuiltAndTag(probe)) {
    return;
  }
  build_animdata(&probe->id);
  build_parameters(&probe->id);
}

void DepsgraphRelationBuilder::build_speaker(Speaker *speaker)
{
  if (built_map_.checkIsBuiltAndTag(speaker)) {
    return;
  }
  build_animdata(&speaker->id);
  build_parameters(&speaker->id);
  if (speaker->sound != NULL) {
    build_sound(speaker->sound);
    ComponentKey speaker_key(&speaker->id, NodeType::AUDIO);
    ComponentKey sound_key(&speaker->sound->id, NodeType::AUDIO);
    add_relation(sound_key, speaker_key, "Sound -> Speaker");
  }
}

void DepsgraphRelationBuilder::build_sound(bSound *sound)
{
  if (built_map_.checkIsBuiltAndTag(sound)) {
    return;
  }
  build_animdata(&sound->id);
  build_parameters(&sound->id);
}

void DepsgraphRelationBuilder::build_scene_sequencer(Scene *scene)
{
  if (scene->ed == NULL) {
    return;
  }
  build_scene_audio(scene);
  ComponentKey scene_audio_key(&scene->id, NodeType::AUDIO);
  /* Make sure dependencies from sequences data goes to the sequencer evaluation. */
  ComponentKey sequencer_key(&scene->id, NodeType::SEQUENCER);
  Sequence *seq;
  bool has_audio_strips = false;
  SEQ_BEGIN (scene->ed, seq) {
    if (seq->sound != NULL) {
      build_sound(seq->sound);
      ComponentKey sound_key(&seq->sound->id, NodeType::AUDIO);
      add_relation(sound_key, sequencer_key, "Sound -> Sequencer");
      has_audio_strips = true;
    }
    if (seq->scene != NULL) {
      build_scene_parameters(seq->scene);
      /* This is to support 3D audio. */
      has_audio_strips = true;
    }
    if (seq->type == SEQ_TYPE_SCENE && seq->scene != NULL) {
      if (seq->flag & SEQ_SCENE_STRIPS) {
        build_scene_sequencer(seq->scene);
        ComponentKey sequence_scene_audio_key(&seq->scene->id, NodeType::AUDIO);
        add_relation(sequence_scene_audio_key, sequencer_key, "Sequence Scene Audio -> Sequencer");
        ComponentKey sequence_scene_key(&seq->scene->id, NodeType::SEQUENCER);
        add_relation(sequence_scene_key, sequencer_key, "Sequence Scene -> Sequencer");
      }
      ViewLayer *sequence_view_layer = BKE_view_layer_default_render(seq->scene);
      build_scene_speakers(seq->scene, sequence_view_layer);
    }
    /* TODO(sergey): Movie clip, camera, mask. */
  }
  SEQ_END;
  if (has_audio_strips) {
    add_relation(sequencer_key, scene_audio_key, "Sequencer -> Audio");
  }
}

void DepsgraphRelationBuilder::build_scene_audio(Scene * /*scene*/)
{
}

void DepsgraphRelationBuilder::build_scene_speakers(Scene * /*scene*/, ViewLayer *view_layer)
{
  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    Object *object = base->object;
    if (object->type != OB_SPEAKER || !need_pull_base_into_graph(base)) {
      continue;
    }
    build_object(NULL, base->object);
  }
}

void DepsgraphRelationBuilder::build_copy_on_write_relations()
{
  for (IDNode *id_node : graph_->id_nodes) {
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
  OperationKey owner_copy_on_write_key(
      owner, NodeType::COPY_ON_WRITE, OperationCode::COPY_ON_WRITE);
  OperationKey id_copy_on_write_key(id, NodeType::COPY_ON_WRITE, OperationCode::COPY_ON_WRITE);
  add_relation(id_copy_on_write_key, owner_copy_on_write_key, "Eval Order");
}

void DepsgraphRelationBuilder::build_nested_nodetree(ID *owner, bNodeTree *ntree)
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

void DepsgraphRelationBuilder::build_copy_on_write_relations(IDNode *id_node)
{
  ID *id_orig = id_node->id_orig;
  const ID_Type id_type = GS(id_orig->name);
  TimeSourceKey time_source_key;
  OperationKey copy_on_write_key(id_orig, NodeType::COPY_ON_WRITE, OperationCode::COPY_ON_WRITE);
  /* XXX: This is a quick hack to make Alt-A to work. */
  // add_relation(time_source_key, copy_on_write_key, "Fluxgate capacitor hack");
  /* Resat of code is using rather low level trickery, so need to get some
   * explicit pointers. */
  Node *node_cow = find_node(copy_on_write_key);
  OperationNode *op_cow = node_cow->get_exit_operation();
  /* Plug any other components to this one. */
  GHASH_FOREACH_BEGIN (ComponentNode *, comp_node, id_node->components) {
    if (comp_node->type == NodeType::COPY_ON_WRITE) {
      /* Copy-on-write component never depends on itself. */
      continue;
    }
    if (!comp_node->depends_on_cow()) {
      /* Component explicitly requests to not add relation. */
      continue;
    }
    int rel_flag = (RELATION_FLAG_NO_FLUSH | RELATION_FLAG_GODMODE);
    if ((id_type == ID_ME && comp_node->type == NodeType::GEOMETRY) ||
        (id_type == ID_CF && comp_node->type == NodeType::CACHE)) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* TODO(sergey): Needs better solution for this. */
    if (id_type == ID_SO) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
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
     *   that cached array fo bases exists and is up-to-date. */
    if (comp_node->type == NodeType::PARAMETERS ||
        comp_node->type == NodeType::LAYER_COLLECTIONS) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* All entry operations of each component should wait for a proper
     * copy of ID. */
    OperationNode *op_entry = comp_node->get_entry_operation();
    if (op_entry != NULL) {
      Relation *rel = graph_->add_new_relation(op_cow, op_entry, "CoW Dependency");
      rel->flag |= rel_flag;
    }
    /* All dangling operations should also be executed after copy-on-write. */
    GHASH_FOREACH_BEGIN (OperationNode *, op_node, comp_node->operations_map) {
      if (op_node == op_entry) {
        continue;
      }
      if (op_node->inlinks.size() == 0) {
        Relation *rel = graph_->add_new_relation(op_cow, op_node, "CoW Dependency");
        rel->flag |= rel_flag;
      }
      else {
        bool has_same_comp_dependency = false;
        for (Relation *rel_current : op_node->inlinks) {
          if (rel_current->from->type != NodeType::OPERATION) {
            continue;
          }
          OperationNode *op_node_from = (OperationNode *)rel_current->from;
          if (op_node_from->owner == op_node->owner) {
            has_same_comp_dependency = true;
            break;
          }
        }
        if (!has_same_comp_dependency) {
          Relation *rel = graph_->add_new_relation(op_cow, op_node, "CoW Dependency");
          rel->flag |= rel_flag;
        }
      }
    }
    GHASH_FOREACH_END();
    /* NOTE: We currently ignore implicit relations to an external
     * data-blocks for copy-on-write operations. This means, for example,
     * copy-on-write component of Object will not wait for copy-on-write
     * component of it's Mesh. This is because pointers are all known
     * already so remapping will happen all correct. And then If some object
     * evaluation step needs geometry, it will have transitive dependency
     * to Mesh copy-on-write already. */
  }
  GHASH_FOREACH_END();
  /* TODO(sergey): This solves crash for now, but causes too many
   * updates potentially. */
  if (GS(id_orig->name) == ID_OB) {
    Object *object = (Object *)id_orig;
    ID *object_data_id = (ID *)object->data;
    if (object_data_id != NULL) {
      if (deg_copy_on_write_is_needed(object_data_id)) {
        OperationKey data_copy_on_write_key(
            object_data_id, NodeType::COPY_ON_WRITE, OperationCode::COPY_ON_WRITE);
        add_relation(
            data_copy_on_write_key, copy_on_write_key, "Eval Order", RELATION_FLAG_GODMODE);
      }
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
  data->builder->build_id(id);
}

void DepsgraphRelationBuilder::constraint_walk(bConstraint * /*con*/,
                                               ID **idpoin,
                                               bool /*is_reference*/,
                                               void *user_data)
{
  BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
  ID *id = *idpoin;
  if (id == NULL) {
    return;
  }
  data->builder->build_id(id);
}

}  // namespace DEG
