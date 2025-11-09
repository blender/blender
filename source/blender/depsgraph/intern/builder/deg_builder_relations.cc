/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <cstdio>
#include <cstdlib>
#include <cstring> /* required for STREQ later on. */
#include <optional>

#include "BKE_global.hh"
#include "DNA_modifier_types.h"

#include "BLI_listbase.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_curves_types.h"
#include "DNA_effect_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"
#include "DNA_texture_types.h"
#include "DNA_vfont_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_armature.hh"
#include "BKE_collection.hh"
#include "BKE_collision.h"
#include "BKE_constraint.h"
#include "BKE_curve.hh"
#include "BKE_effect.h"
#include "BKE_fcurve_driver.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_grease_pencil.hh"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_query.hh"
#include "BKE_material.hh"
#include "BKE_mball.hh"
#include "BKE_modifier.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_rigidbody.h"
#include "BKE_shader_fx.h"
#include "BKE_shrinkwrap.hh"
#include "BKE_tracking.h"
#include "BKE_world.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "ANIM_action.hh"
#include "SEQ_iterator.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_debug.hh"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_pchanmap.h"
#include "intern/builder/deg_builder_relations_drivers.h"
#include "intern/debug/deg_debug.h"
#include "intern/depsgraph_physics.hh"
#include "intern/depsgraph_tag.hh"
#include "intern/eval/deg_eval_copy_on_write.h"

#include "intern/node/deg_node.hh"
#include "intern/node/deg_node_component.hh"
#include "intern/node/deg_node_id.hh"
#include "intern/node/deg_node_operation.hh"
#include "intern/node/deg_node_time.hh"

#include "intern/depsgraph.hh"
#include "intern/depsgraph_relation.hh"
#include "intern/depsgraph_type.hh"

namespace blender::deg {

/* ***************** */
/* Relations Builder */

namespace {

bool is_time_dependent_scene_driver_target(const DriverTarget *target)
{
  return target->rna_path != nullptr && STREQ(target->rna_path, "frame_current");
}

bool driver_target_depends_on_time(const DriverVar *variable, const DriverTarget *target)
{
  if (variable->type == DVAR_TYPE_CONTEXT_PROP &&
      target->context_property == DTAR_CONTEXT_PROPERTY_ACTIVE_SCENE)
  {
    return is_time_dependent_scene_driver_target(target);
  }

  if (target->idtype == ID_SCE) {
    return is_time_dependent_scene_driver_target(target);
  }

  return false;
}

bool driver_variable_depends_on_time(const DriverVar *variable)
{
  for (int i = 0; i < variable->num_targets; ++i) {
    if (driver_target_depends_on_time(variable, &variable->targets[i])) {
      return true;
    }
  }
  return false;
}

bool driver_variables_depends_on_time(const ListBase *variables)
{
  LISTBASE_FOREACH (const DriverVar *, variable, variables) {
    if (driver_variable_depends_on_time(variable)) {
      return true;
    }
  }
  return false;
}

bool driver_depends_on_time(ChannelDriver *driver)
{
  if (BKE_driver_expression_depends_on_time(driver)) {
    return true;
  }
  if (driver_variables_depends_on_time(&driver->variables)) {
    return true;
  }
  return false;
}

bool particle_system_depends_on_time(ParticleSystem *psys)
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

bool object_particles_depends_on_time(Object *object)
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

bool check_id_has_anim_component(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    return false;
  }
  return (adt->action != nullptr) || !BLI_listbase_is_empty(&adt->nla_tracks);
}

bool check_id_has_driver_component(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    return false;
  }
  return !BLI_listbase_is_empty(&adt->drivers);
}

OperationCode bone_target_opcode(ID *target,
                                 const char *subtarget,
                                 ID *id,
                                 const char *component_subdata,
                                 RootPChanMap *root_map)
{
  /* Same armature. root_map will be nullptr when building object-level constraints, and in that
   * case we don't need to check for the common chains. */
  if (target == id && root_map != nullptr) {
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

bool object_have_geometry_component(const Object *object)
{
  return ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_FONT, OB_SURF, OB_MBALL, OB_LATTICE);
}

}  // namespace

/* **** General purpose functions **** */

DepsgraphRelationBuilder::DepsgraphRelationBuilder(Main *bmain,
                                                   Depsgraph *graph,
                                                   DepsgraphBuilderCache *cache)
    : DepsgraphBuilder(bmain, graph, cache), scene_(nullptr), rna_node_query_(graph, this)
{
}

TimeSourceNode *DepsgraphRelationBuilder::get_node(const TimeSourceKey & /*key*/) const
{
  return graph_->time_source;
}

ComponentNode *DepsgraphRelationBuilder::get_node(const ComponentKey &key) const
{
  IDNode *id_node = graph_->find_id_node(key.id);
  if (!id_node) {
    fprintf(stderr,
            "find_node component: Could not find ID %s\n",
            (key.id != nullptr) ? key.id->name : "<null>");
    return nullptr;
  }

  ComponentNode *node = id_node->find_component(key.type, key.name);
  return node;
}

OperationNode *DepsgraphRelationBuilder::get_node(const OperationKey &key) const
{
  OperationNode *op_node = find_node(key);
  if (op_node == nullptr) {
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

ComponentNode *DepsgraphRelationBuilder::find_node(const ComponentKey &key) const
{
  IDNode *id_node = graph_->find_id_node(key.id);
  if (!id_node) {
    return nullptr;
  }
  return id_node->find_component(key.type, key.name);
}

OperationNode *DepsgraphRelationBuilder::find_node(const OperationKey &key) const
{
  IDNode *id_node = graph_->find_id_node(key.id);
  if (!id_node) {
    return nullptr;
  }
  ComponentNode *comp_node = id_node->find_component(key.component_type, key.component_name);
  if (!comp_node) {
    return nullptr;
  }
  return comp_node->find_operation(key.opcode, key.name, key.name_tag);
}

bool DepsgraphRelationBuilder::has_node(const OperationKey &key) const
{
  return find_node(key) != nullptr;
}

bool DepsgraphRelationBuilder::has_node(const ComponentKey &key) const
{
  return find_node(key) != nullptr;
}

void DepsgraphRelationBuilder::add_depends_on_transform_relation(const DepsNodeHandle *handle,
                                                                 const char *description)
{
  IDNode *id_node = handle->node->owner->owner;
  ID *id = id_node->id_orig;
  const OperationKey geometry_key(
      id, NodeType::GEOMETRY, OperationCode::MODIFIER, handle->node->name.c_str());
  /* Wire up the actual relation. */
  add_depends_on_transform_relation(id, geometry_key, description);
}

void DepsgraphRelationBuilder::add_customdata_mask(Object *object,
                                                   const DEGCustomDataMeshMasks &customdata_masks)
{
  if (customdata_masks != DEGCustomDataMeshMasks() && object != nullptr && object->type == OB_MESH)
  {
    IDNode *id_node = graph_->find_id_node(&object->id);

    if (id_node == nullptr) {
      BLI_assert_msg(0, "ID should always be valid");
    }
    else {
      id_node->customdata_masks |= customdata_masks;
    }
  }
}

void DepsgraphRelationBuilder::add_special_eval_flag(ID *id, uint32_t flag)
{
  IDNode *id_node = graph_->find_id_node(id);
  if (id_node == nullptr) {
    BLI_assert_msg(0, "ID should always be valid");
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

  DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                   BUILD,
                   "add_time_relation(%p = %s, %p = %s, %s) Failed\n",
                   timesrc,
                   (timesrc) ? timesrc->identifier().c_str() : "<None>",
                   node_to,
                   (node_to) ? node_to->identifier().c_str() : "<None>",
                   description);

  return nullptr;
}

void DepsgraphRelationBuilder::add_visibility_relation(ID *id_from, ID *id_to)
{
  ComponentKey from_key(id_from, NodeType::VISIBILITY);
  ComponentKey to_key(id_to, NodeType::VISIBILITY);
  add_relation(from_key, to_key, "visibility");
}

Relation *DepsgraphRelationBuilder::add_operation_relation(OperationNode *node_from,
                                                           OperationNode *node_to,
                                                           const char *description,
                                                           int flags)
{
  if (node_from && node_to) {
    return graph_->add_new_relation(node_from, node_to, description, flags);
  }

  DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                   BUILD,
                   "add_operation_relation(%p = %s, %p = %s, %s) Failed\n",
                   node_from,
                   (node_from) ? node_from->identifier().c_str() : "<None>",
                   node_to,
                   (node_to) ? node_to->identifier().c_str() : "<None>",
                   description);

  return nullptr;
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

  /* Make sure physics effects like wind are properly re-evaluating the modifier stack. */
  if (!BLI_listbase_is_empty(relations)) {
    TimeSourceKey time_src_key;
    ComponentKey geometry_key(&object->id, NodeType::GEOMETRY);
    add_relation(
        time_src_key, geometry_key, "Effector Time -> Particle", RELATION_CHECK_BEFORE_ADD);
  }

  LISTBASE_FOREACH (EffectorRelation *, relation, relations) {
    if (relation->ob != object) {
      /* Relation to forcefield object, optionally including geometry. */
      ComponentKey eff_key(&relation->ob->id, NodeType::TRANSFORM);
      add_relation(eff_key, key, name);

      if (ELEM(relation->pd->shape, PFIELD_SHAPE_SURFACE, PFIELD_SHAPE_POINTS) ||
          relation->pd->forcefield == PFIELD_GUIDE)
      {
        ComponentKey mod_key(&relation->ob->id, NodeType::GEOMETRY);
        add_relation(mod_key, key, name);
      }

      /* Force field Texture. */
      if ((relation->pd != nullptr) && (relation->pd->forcefield == PFIELD_TEXTURE) &&
          (relation->pd->tex != nullptr))
      {
        ComponentKey tex_key(&relation->pd->tex->id, NodeType::GENERIC_DATABLOCK);
        add_relation(tex_key, key, "Force field Texture");
      }

      /* Smoke flow relations. */
      if (relation->pd->forcefield == PFIELD_FLUIDFLOW && relation->pd->f_source) {
        ComponentKey trf_key(&relation->pd->f_source->id, NodeType::TRANSFORM);
        add_relation(trf_key, key, "Smoke Force Domain");
        ComponentKey eff_key(&relation->pd->f_source->id, NodeType::GEOMETRY);
        add_relation(eff_key, key, "Smoke Force Domain");
      }

      /* Absorption forces need collision relation. */
      if (add_absorption && (relation->pd->flag & PFIELD_VISIBILITY)) {
        add_particle_collision_relations(key, object, nullptr, "Force Absorption");
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

void DepsgraphRelationBuilder::begin_build() {}

void DepsgraphRelationBuilder::build_id(ID *id)
{
  if (id == nullptr) {
    return;
  }

  const ID_Type id_type = GS(id->name);
  switch (id_type) {
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
      build_collection(nullptr, (Collection *)id);
      break;
    case ID_OB:
      build_object((Object *)id);
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
    case ID_LS:
      build_freestyle_linestyle((FreestyleLineStyle *)id);
      break;
    case ID_MC:
      build_movieclip((MovieClip *)id);
      break;
    case ID_ME:
    case ID_MB:
    case ID_CU_LEGACY:
    case ID_LT:
    case ID_CV:
    case ID_PT:
    case ID_VO:
    case ID_GD_LEGACY:
    case ID_GP:
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
    case ID_PA:
      build_particle_settings((ParticleSettings *)id);
      break;

    case ID_LI:
    case ID_SCR:
    case ID_VF:
    case ID_BR:
    case ID_WM:
    case ID_PAL:
    case ID_PC:
    case ID_WS:
      BLI_assert(!deg_eval_copy_is_needed(id_type));
      build_generic_id(id);
      break;
  }
}

void DepsgraphRelationBuilder::build_generic_id(ID *id)
{
  if (built_map_.check_is_built_and_tag(id)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(*id);

  build_idproperties(id->properties);
  build_idproperties(id->system_properties);
  build_animdata(id);
  build_parameters(id);
}

void DepsgraphRelationBuilder::build_idproperties(IDProperty *id_property)
{
  IDP_foreach_property(id_property, IDP_TYPE_FILTER_ID, [&](IDProperty *id_property) {
    this->build_id(static_cast<ID *>(id_property->data.pointer));
  });
}

void DepsgraphRelationBuilder::build_collection(LayerCollection *from_layer_collection,
                                                Collection *collection)
{
  if (from_layer_collection != nullptr) {
    /* If we came from layer collection we don't go deeper, view layer builder takes care of going
     * deeper.
     *
     * NOTE: Do early output before tagging build as done, so possible subsequent builds from
     * outside of the layer collection properly recurses into all the nested objects and
     * collections. */

    if (!built_map_.check_is_built_and_tag(collection,
                                           BuilderMap::TAG_COLLECTION_CHILDREN_HIERARCHY))
    {
      const ComponentKey collection_hierarchy_key{&collection->id, NodeType::HIERARCHY};
      OperationNode *collection_hierarchy_exit =
          this->find_node(collection_hierarchy_key)->get_exit_operation();
      LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
        Object *object = cob->ob;
        const ComponentKey object_hierarchy_key{&object->id, NodeType::HIERARCHY};
        /* Check whether the object hierarchy node exists, because the view layer builder can skip
         * bases if they are constantly excluded from the collections. */
        if (Node *object_hierarchy_node = this->find_node(object_hierarchy_key)) {
          this->add_operation_relation(collection_hierarchy_exit,
                                       object_hierarchy_node->get_entry_operation(),
                                       "Collection -> Object hierarchy");
        }
      }
    }

    return;
  }

  if (built_map_.check_is_built_and_tag(collection)) {
    return;
  }

  build_idproperties(collection->id.properties);
  build_idproperties(collection->id.system_properties);
  build_parameters(&collection->id);

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(collection->id);

  const OperationKey collection_geometry_key{
      &collection->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE};

  const ComponentKey collection_hierarchy_key{&collection->id, NodeType::HIERARCHY};
  OperationNode *collection_hierarchy_exit =
      this->find_node(collection_hierarchy_key)->get_exit_operation();

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Object *object = cob->ob;

    build_object(object);

    /* Unfortunately this may add duplicates with the hierarchy relations added below above. This
     * is necessary though, for collections that are built as layer collections and otherwise,
     * where an object may not be built yet in the layer collection case. */
    const ComponentKey object_hierarchy_key{&object->id, NodeType::HIERARCHY};
    Node *object_hierarchy_node = this->find_node(object_hierarchy_key);
    this->add_operation_relation(collection_hierarchy_exit,
                                 object_hierarchy_node->get_entry_operation(),
                                 "Collection -> Object hierarchy");

    const OperationKey object_instance_geometry_key{
        &object->id, NodeType::INSTANCING, OperationCode::INSTANCE_GEOMETRY};
    add_relation(object_instance_geometry_key, collection_geometry_key, "Collection Geometry");

    /* An instance is part of the geometry of the collection. */
    if (object->type == OB_EMPTY) {
      Collection *collection_instance = cob->ob->instance_collection;
      if (collection_instance != nullptr) {
        const OperationKey collection_instance_key{
            &collection_instance->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE};
        add_relation(collection_instance_key, collection_geometry_key, "Collection Geometry");
      }
    }
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    build_collection(nullptr, child->collection);
    const OperationKey child_collection_geometry_key{
        &child->collection->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE};
    add_relation(child_collection_geometry_key, collection_geometry_key, "Collection Geometry");
  }
}

void DepsgraphRelationBuilder::build_object(Object *object)
{
  if (built_map_.check_is_built_and_tag(object)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(object->id);

  /* Object Transforms. */
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
  build_object_layer_component_relations(object);

  /* Parenting. */
  if (object->parent != nullptr) {
    /* Make sure parent object's relations are built. */
    build_object(object->parent);
    /* Parent relationship. */
    build_object_parent(object);
    /* Local -> parent. */
    add_relation(local_transform_key, parent_transform_key, "ObLocal -> ObParent");
  }

  add_relation(OperationKey{&object->id, NodeType::INSTANCING, OperationCode::INSTANCE_GEOMETRY},
               OperationKey{&object->id, NodeType::INSTANCING, OperationCode::INSTANCE},
               "Instance Geometry -> Geometry");

  add_relation(ComponentKey(&object->id, NodeType::TRANSFORM),
               OperationKey{&object->id, NodeType::INSTANCING, OperationCode::INSTANCE_GEOMETRY},
               "Transform -> Instance Geometry");

  /* Modifiers. */
  build_object_modifiers(object);

  /* Grease Pencil Modifiers. */
  if (object->greasepencil_modifiers.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_gpencil_modifiers_foreach_ID_link(object, modifier_walk, &data);
  }

  /* Shader FX. */
  if (object->shader_fx.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_shaderfx_foreach_ID_link(object, modifier_walk, &data);
  }

  /* Constraints. */
  if (object->constraints.first != nullptr) {
    BuilderWalkUserData data;
    data.builder = this;
    BKE_constraints_id_loop(&object->constraints, constraint_walk, IDWALK_NOP, &data);
  }

  /* Object constraints. */
  OperationKey object_transform_simulation_init_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
  if (object->constraints.first != nullptr) {
    OperationKey constraint_key(
        &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_CONSTRAINTS);
    /* Constraint relations. */
    build_constraints(&object->id, NodeType::TRANSFORM, "", &object->constraints, nullptr);
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

  build_idproperties(object->id.properties);
  build_idproperties(object->id.system_properties);

  /* Animation data */
  build_animdata(&object->id);

  /* Object data. */
  build_object_data(object);

  /* Particle systems. */
  if (object->particlesystem.first != nullptr) {
    build_particle_systems(object);
  }

  /* Force field Texture. */
  if ((object->pd != nullptr) && (object->pd->forcefield == PFIELD_TEXTURE) &&
      (object->pd->tex != nullptr))
  {
    build_texture(object->pd->tex);
  }

  build_object_instance_collection(object);
  build_object_pointcache(object);

  build_object_shading(object);
  build_object_light_linking(object);

  /* Synchronization back to original object. */
  OperationKey synchronize_key(
      &object->id, NodeType::SYNCHRONIZATION, OperationCode::SYNCHRONIZE_TO_ORIGINAL);
  add_relation(final_transform_key, synchronize_key, "Synchronize to Original");

  /* Parameters. */
  build_parameters(&object->id);

  /* Visibility.
   * Evaluate visibility node after the object's base_flags has been updated to the current state
   * of collections restrict and object's restrict flags. */
  const ComponentKey object_from_layer_entry_key(&object->id, NodeType::OBJECT_FROM_LAYER);
  const ComponentKey visibility_key(&object->id, NodeType::VISIBILITY);
  add_relation(object_from_layer_entry_key, visibility_key, "Object Visibility");
}

/* NOTE: Implies that the object has base in the current view layer. */
void DepsgraphRelationBuilder::build_object_from_view_layer_base(Object *object)
{
  /* It is possible to have situation when an object is pulled into the dependency graph in a
   * few different ways:
   *
   *  - Indirect driver dependency, which doesn't have a Base (or, Base is unknown).
   *  - Via a base from a view layer (view layer of the graph, or view layer of a set scene).
   *  - Possibly other ways, which are not important for decision making here.
   *
   * There needs to be a relation from view layer which has a base with the object so that the
   * order of flags evaluation is correct (object-level base flags evaluation requires view layer
   * to be evaluated first).
   *
   * This build call handles situation when object comes from a view layer, hence has a base, and
   * needs a relation from the view layer. Do the relation prior to check of whether the object
   * relations are built so that the relation is created from every view layer which has a base
   * with this object. */

  OperationKey view_layer_done_key(
      &scene_->id, NodeType::LAYER_COLLECTIONS, OperationCode::VIEW_LAYER_EVAL);
  OperationKey object_from_layer_entry_key(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_FROM_LAYER_ENTRY);

  add_relation(view_layer_done_key, object_from_layer_entry_key, "View Layer flags to Object");

  /* Regular object building. */
  build_object(object);
}

void DepsgraphRelationBuilder::build_object_layer_component_relations(Object *object)
{
  OperationKey object_from_layer_entry_key(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_FROM_LAYER_ENTRY);
  OperationKey object_from_layer_exit_key(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_FROM_LAYER_EXIT);
  OperationKey object_flags_key(
      &object->id, NodeType::OBJECT_FROM_LAYER, OperationCode::OBJECT_BASE_FLAGS);

  if (!has_node(object_flags_key)) {
    /* Just connect Entry -> Exit if there is no OBJECT_BASE_FLAGS node. */
    add_relation(object_from_layer_entry_key, object_from_layer_exit_key, "Object from Layer");
    return;
  }

  /* Entry -> OBJECT_BASE_FLAGS -> Exit */
  add_relation(object_from_layer_entry_key, object_flags_key, "Base flags flush Entry");
  add_relation(object_flags_key, object_from_layer_exit_key, "Base flags flush Exit");

  /* Synchronization back to original object. */
  OperationKey synchronize_key(
      &object->id, NodeType::SYNCHRONIZATION, OperationCode::SYNCHRONIZE_TO_ORIGINAL);
  add_relation(object_from_layer_exit_key, synchronize_key, "Synchronize to Original");
}

void DepsgraphRelationBuilder::build_object_modifiers(Object *object)
{
  if (BLI_listbase_is_empty(&object->modifiers)) {
    return;
  }

  const OperationKey eval_init_key(
      &object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_INIT);
  const OperationKey eval_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);

  const ComponentKey object_visibility_key(&object->id, NodeType::VISIBILITY);
  const OperationKey modifier_visibility_key(
      &object->id, NodeType::GEOMETRY, OperationCode::VISIBILITY);
  add_relation(modifier_visibility_key,
               object_visibility_key,
               "modifier -> object visibility",
               RELATION_NO_VISIBILITY_CHANGE);

  add_relation(modifier_visibility_key, eval_key, "modifier visibility -> geometry eval");

  ModifierUpdateDepsgraphContext ctx = {};
  ctx.scene = scene_;
  ctx.object = object;

  OperationKey previous_key = eval_init_key;
  LISTBASE_FOREACH (ModifierData *, modifier, &object->modifiers) {
    const OperationKey modifier_key(
        &object->id, NodeType::GEOMETRY, OperationCode::MODIFIER, modifier->name);

    /* Relation for the modifier stack chain. */
    add_relation(previous_key, modifier_key, "Modifier");

    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)modifier->type);
    if (mti->update_depsgraph) {
      const BuilderStack::ScopedEntry stack_entry = stack_.trace(*modifier);

      DepsNodeHandle handle = create_node_handle(modifier_key);
      ctx.node = reinterpret_cast<::DepsNodeHandle *>(&handle);
      mti->update_depsgraph(modifier, &ctx);
    }

    /* Time dependency. */
    if (BKE_modifier_depends_ontime(scene_, modifier)) {
      const TimeSourceKey time_src_key;
      add_relation(time_src_key, modifier_key, "Time Source -> Modifier");
    }

    previous_key = modifier_key;
  }
  add_relation(previous_key, eval_key, "modifier stack order");

  /* Build IDs referenced by the modifiers. */
  BuilderWalkUserData data;
  data.builder = this;
  BKE_modifiers_foreach_ID_link(object, modifier_walk, &data);
}

void DepsgraphRelationBuilder::build_object_data(Object *object)
{
  if (object->data == nullptr) {
    return;
  }
  ID *obdata_id = (ID *)object->data;
  /* Object data animation. */
  if (!built_map_.check_is_built(obdata_id)) {
    build_animdata(obdata_id);
  }
  /* type-specific data. */
  switch (object->type) {
    case OB_MESH:
    case OB_CURVES_LEGACY:
    case OB_FONT:
    case OB_SURF:
    case OB_MBALL:
    case OB_LATTICE:
    case OB_CURVES:
    case OB_POINTCLOUD:
    case OB_VOLUME:
    case OB_GREASE_PENCIL: {
      build_object_data_geometry(object);
      /* TODO(sergey): Only for until we support granular
       * update of curves. */
      if (object->type == OB_FONT) {
        Curve *curve = (Curve *)object->data;
        if (curve->textoncurve) {
          ComponentKey geometry_key((ID *)object->data, NodeType::GEOMETRY);
          ComponentKey transform_key(&object->id, NodeType::TRANSFORM);
          add_relation(transform_key, geometry_key, "Text on Curve own Transform");
          add_special_eval_flag(&curve->textoncurve->id, DAG_EVAL_NEED_CURVE_PATH);
        }
      }
      break;
    }
    case OB_ARMATURE:
      build_rig(object);
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
  if (key != nullptr) {
    ComponentKey geometry_key((ID *)object->data, NodeType::GEOMETRY);
    ComponentKey key_key(&key->id, NodeType::GEOMETRY);
    add_relation(key_key, geometry_key, "Shapekeys");
    build_nested_shapekey(&object->id, key);
  }
  /* Materials. */
  Material ***materials_ptr = BKE_object_material_array_p(object);
  if (materials_ptr != nullptr) {
    short *num_materials_ptr = BKE_object_material_len_p(object);
    ID *obdata = (ID *)object->data;
    build_materials(obdata, *materials_ptr, *num_materials_ptr);
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
  OperationKey object_shading_key(&object->id, NodeType::SHADING, OperationCode::SHADING);
  add_relation(lamp_parameters_key, object_shading_key, "Light -> Object Shading");
}

void DepsgraphRelationBuilder::build_object_data_lightprobe(Object *object)
{
  LightProbe *probe = (LightProbe *)object->data;
  build_lightprobe(probe);
  OperationKey probe_key(&probe->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
  OperationKey object_key(&object->id, NodeType::PARAMETERS, OperationCode::LIGHT_PROBE_EVAL);
  add_relation(probe_key, object_key, "LightProbe Update");
  OperationKey object_shading_key(&object->id, NodeType::SHADING, OperationCode::SHADING);
  add_relation(probe_key, object_shading_key, "LightProbe -> Object Shading");
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
  ComponentKey object_transform_key(&object->id, NodeType::TRANSFORM);
  /* Type-specific links. */
  switch (object->partype) {
    /* Armature Deform (Virtual Modifier) */
    case PARSKEL: {
      ComponentKey parent_transform_key(parent_id, NodeType::TRANSFORM);
      add_relation(parent_transform_key, object_transform_key, "Parent Armature Transform");

      if (parent->type == OB_ARMATURE) {
        ComponentKey object_geometry_key(&object->id, NodeType::GEOMETRY);
        ComponentKey parent_pose_key(parent_id, NodeType::EVAL_POSE);
        add_relation(
            parent_transform_key, object_geometry_key, "Parent Armature Transform -> Geometry");
        add_relation(parent_pose_key, object_geometry_key, "Parent Armature Pose -> Geometry");

        add_depends_on_transform_relation(
            &object->id, object_geometry_key, "Virtual Armature Modifier");
      }

      break;
    }

    /* Vertex Parent */
    case PARVERT1:
    case PARVERT3: {
      ComponentKey parent_key(parent_id, NodeType::GEOMETRY);
      add_relation(parent_key, object_transform_key, "Vertex Parent");
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
      add_relation(transform_key, object_transform_key, "Vertex Parent TFM");
      break;
    }

    /* Bone Parent */
    case PARBONE: {
      if (object->parsubstr[0] != '\0') {
        ComponentKey parent_bone_key(parent_id, NodeType::BONE, object->parsubstr);
        OperationKey parent_transform_key(
            parent_id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
        add_relation(parent_bone_key, object_transform_key, "Bone Parent");
        add_relation(parent_transform_key, object_transform_key, "Armature Parent");
      }
      break;
    }

    default: {
      if (object->parent->type == OB_LATTICE) {
        /* Lattice Deform Parent - Virtual Modifier. */
        ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
        ComponentKey geom_key(parent_id, NodeType::GEOMETRY);
        add_relation(parent_key, object_transform_key, "Lattice Deform Parent");
        add_relation(geom_key, object_transform_key, "Lattice Deform Parent Geom");
      }
      else if (object->parent->type == OB_CURVES_LEGACY) {
        Curve *cu = (Curve *)object->parent->data;

        if (cu->flag & CU_PATH) {
          /* Follow Path. */
          ComponentKey parent_key(parent_id, NodeType::GEOMETRY);
          add_relation(parent_key, object_transform_key, "Curve Follow Parent");
          ComponentKey transform_key(parent_id, NodeType::TRANSFORM);
          add_relation(transform_key, object_transform_key, "Curve Follow TFM");
        }
        else {
          /* Standard Parent. */
          ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
          add_relation(parent_key, object_transform_key, "Curve Parent");
        }
      }
      else {
        /* Standard Parent. */
        ComponentKey parent_key(parent_id, NodeType::TRANSFORM);
        add_relation(parent_key, object_transform_key, "Parent");
      }
      break;
    }
  }
  /* Meta-balls are the odd balls here (no pun intended): they will request
   * instance-list (formerly known as dupli-list) during evaluation. This is
   * their way of interacting with all instanced surfaces, making a nice
   * effect when is used form particle system. */
  if (object->type == OB_MBALL && parent->transflag & OB_DUPLI) {
    ComponentKey parent_geometry_key(parent_id, NodeType::GEOMETRY);
    /* NOTE: Meta-balls are evaluating geometry only after their transform,
     * so we only hook up to transform channel here. */
    add_relation(parent_geometry_key, object_transform_key, "Parent");
  }

  /* Dupliverts uses original vertex index. */
  if (parent->transflag & OB_DUPLIVERTS) {
    add_customdata_mask(parent, DEGCustomDataMeshMasks::MaskVert(CD_MASK_ORIGINDEX));
  }
}

/* Returns the modifier that is last in the modifier stack. */
static const ModifierData *get_latter_modifier(const ModifierData *md1, const ModifierData *md2)
{
  if (md1 == nullptr) {
    return md2;
  }
  if (md2 == nullptr) {
    return md1;
  }

  for (const ModifierData *md = md2->prev; md; md = md->prev) {
    if (md == md1) {
      return md2;
    }
  }
  return md1;
}

void DepsgraphRelationBuilder::build_object_pointcache(Object *object)
{
  std::optional<ComponentKey> point_cache_key;
  bool has_rigid_body_relation = false;
  bool has_geometry_eval_relation = false;
  const ModifierData *last_input_modifier = nullptr;
  BKE_ptcache_foreach_object_cache(
      *object, *scene_, false, [&](PTCacheID &ptcache_id, ModifierData *md) {
        if (!point_cache_key) {
          point_cache_key = ComponentKey(&object->id, NodeType::POINT_CACHE);
        }

        /* Check which components needs the point cache. */
        if (!has_geometry_eval_relation) {
          has_geometry_eval_relation = true;

          OperationKey geometry_key(&object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
          add_relation(*point_cache_key, geometry_key, "Point Cache -> Geometry");
        }
        if (!has_rigid_body_relation && ptcache_id.type == PTCACHE_TYPE_RIGIDBODY) {
          if (object->rigidbody_object->type == RBO_TYPE_PASSIVE) {
            return true;
          }
          has_rigid_body_relation = true;

          OperationKey transform_key(
              &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_SIMULATION_INIT);
          add_relation(*point_cache_key, transform_key, "Point Cache -> Rigid Body");
          /* Manual changes to effectors need to invalidate simulation.
           *
           * Don't add this relation for the render pipeline dependency graph as it does not
           * contain rigid body simulation. Good thing is that there are no user edits in such
           * dependency graph, so the relation is not really needed in it. */
          if (!graph_->is_render_pipeline_depsgraph) {
            OperationKey rigidbody_rebuild_key(
                &scene_->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_REBUILD);
            add_relation(rigidbody_rebuild_key,
                         *point_cache_key,
                         "Rigid Body Rebuild -> Point Cache Reset",
                         RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
          }
        }

        if (md && md->prev) {
          last_input_modifier = get_latter_modifier(last_input_modifier, md->prev);
        }

        return true;
      });

  /* Manual edits to any dependency (or self) should reset the point cache. */
  if (point_cache_key) {
    OperationKey transform_eval_key(
        &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_EVAL);
    add_relation(transform_eval_key,
                 *point_cache_key,
                 "Transform Simulation -> Point Cache",
                 RELATION_FLAG_FLUSH_USER_EDIT_ONLY);

    /* For caches in specific modifiers:
     * Input data changes from previous modifiers require a point cache reset. */
    if (last_input_modifier != nullptr) {
      const OperationKey input_modifier_key(
          &object->id, NodeType::GEOMETRY, OperationCode::MODIFIER, last_input_modifier->name);
      add_relation(input_modifier_key,
                   *point_cache_key,
                   "Previous Modifier -> Point Cache",
                   RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    }
    else {
      OperationKey geometry_init_key(
          &object->id, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_INIT);
      add_relation(geometry_init_key,
                   *point_cache_key,
                   "Geometry Init -> Point Cache",
                   RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    }
  }
}

void DepsgraphRelationBuilder::build_object_instance_collection(Object *object)
{
  if (object->instance_collection == nullptr) {
    return;
  }

  Collection *instance_collection = object->instance_collection;

  build_collection(nullptr, instance_collection);

  const OperationKey object_transform_final_key(
      &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
  const OperationKey instancer_key(&object->id, NodeType::INSTANCING, OperationCode::INSTANCER);

  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN (instance_collection, ob, graph_->mode) {
    const ComponentKey dupli_transform_key(&ob->id, NodeType::TRANSFORM);
    add_relation(dupli_transform_key, object_transform_final_key, "Dupligroup");

    /* Hook to special component, to ensure proper visibility/evaluation optimizations. */
    add_relation(OperationKey(&ob->id, NodeType::INSTANCING, OperationCode::INSTANCE),
                 instancer_key,
                 "Instance -> Instancer");
  }
  FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END;
}

void DepsgraphRelationBuilder::build_object_shading(Object *object)
{
  const OperationKey shading_done_key(&object->id, NodeType::SHADING, OperationCode::SHADING_DONE);

  const OperationKey shading_key(&object->id, NodeType::SHADING, OperationCode::SHADING);
  add_relation(shading_key, shading_done_key, "Shading -> Done");

  /* Hook up shading component to the instance, so that if the object is instanced by a visible
   * object the shading component is ensured to be evaluated.
   * Don't to flushing to avoid re-evaluation of geometry when the object is used as part of a
   * collection used as a boolean modifier operand. */
  add_relation(shading_done_key,
               OperationKey(&object->id, NodeType::INSTANCING, OperationCode::INSTANCE),
               "Light Linking -> Instance",
               RELATION_FLAG_NO_FLUSH);
}

void DepsgraphRelationBuilder::build_object_light_linking(Object *emitter)
{
  const ComponentKey hierarchy_key(&emitter->id, NodeType::HIERARCHY);
  const OperationKey shading_done_key(
      &emitter->id, NodeType::SHADING, OperationCode::SHADING_DONE);
  const OperationKey light_linking_key(
      &emitter->id, NodeType::SHADING, OperationCode::LIGHT_LINKING_UPDATE);

  add_relation(hierarchy_key, light_linking_key, "Light Linking From Layer");
  add_relation(light_linking_key, shading_done_key, "Light Linking -> Shading Done");

  if (emitter->light_linking) {
    LightLinking &light_linking = *emitter->light_linking;

    build_light_linking_collection(emitter, light_linking.receiver_collection);
    build_light_linking_collection(emitter, light_linking.blocker_collection);
  }
}

void DepsgraphRelationBuilder::build_light_linking_collection(Object *emitter,
                                                              Collection *collection)
{
  if (collection == nullptr) {
    return;
  }

  build_collection(nullptr, collection);

  /* TODO(sergey): Avoid duplicate dependencies if multiple emitters are using the same collection.
   */

  const OperationKey emitter_light_linking_key(
      &emitter->id, NodeType::SHADING, OperationCode::LIGHT_LINKING_UPDATE);

  const OperationKey collection_parameters_entry_key(
      &collection->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_ENTRY);
  const OperationKey collection_parameters_exit_key(
      &collection->id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
  const OperationKey collection_hierarchy_key(
      &collection->id, NodeType::HIERARCHY, OperationCode::HIERARCHY);

  const OperationKey collection_light_linking_key(
      &collection->id, NodeType::PARAMETERS, OperationCode::LIGHT_LINKING_UPDATE);

  /* Order of parameters evaluation within the receiver collection. */
  /* TODO(sergey): Can optimize this out by explicitly separating the different built tags. This
   * needs to be done in all places where the collection is built (is not something that can be
   * easily solved from just adding the light linking functionality). */
  add_relation(collection_parameters_entry_key,
               collection_light_linking_key,
               "Entry -> Collection Light Linking",
               RELATION_CHECK_BEFORE_ADD);
  add_relation(collection_light_linking_key,
               collection_parameters_exit_key,
               "Collection Light Linking -> Exit",
               RELATION_CHECK_BEFORE_ADD);

  add_relation(collection_hierarchy_key,
               collection_light_linking_key,
               "Collection Hierarchy -> Light Linking",
               RELATION_CHECK_BEFORE_ADD);

  /* Order to ensure the emitter's light linking is only evaluated after the receiver collection.
   * This is because light linking runtime data is "cached" om the emitter object for the
   * simplicity of access, but the mask is allocated per collection bases (so that if two emitters
   * share the same receiving collection they share the same runtime data). */
  add_relation(collection_light_linking_key,
               emitter_light_linking_key,
               "Collection -> Object Light Linking");
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
  LISTBASE_FOREACH (bConstraint *, con, constraints) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
    ListBase targets = {nullptr, nullptr};
    /* Invalid constraint type. */
    if (cti == nullptr) {
      continue;
    }

    const BuilderStack::ScopedEntry stack_entry = stack_.trace(*con);

    /* Special case for camera tracking -- it doesn't use targets to
     * define relations. */
    /* TODO: we can now represent dependencies in a much richer manner,
     * so review how this is done. */
    if (ELEM(cti->type,
             CONSTRAINT_TYPE_FOLLOWTRACK,
             CONSTRAINT_TYPE_CAMERASOLVER,
             CONSTRAINT_TYPE_OBJECTSOLVER))
    {
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
      if (depends_on_camera && scene_->camera != nullptr) {
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
    else if (BKE_constraint_targets_get(con, &targets)) {
      LISTBASE_FOREACH (bConstraintTarget *, ct, &targets) {
        if (ct->tar == nullptr) {
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
              check_pchan_has_bbone_segments(ct->tar, ct->subtarget))
          {
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
            if (scon->shrinkType == MOD_SHRINKWRAP_TARGET_PROJECT) {
              add_special_eval_flag(&ct->tar->id, DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY);
            }
          }

          /* NOTE: obdata eval now doesn't necessarily depend on the
           * object's transform. */
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          add_relation(target_transform_key, constraint_op_key, cti->name);
        }
        else if (con->type == CONSTRAINT_TYPE_GEOMETRY_ATTRIBUTE) {
          /* Constraints which require the target object geometry attributes. */
          ComponentKey target_key(&ct->tar->id, NodeType::GEOMETRY);
          add_relation(target_key, constraint_op_key, cti->name);

          /* NOTE: The target object's transform is used when the 'Apply target transform' flag
           * is set.*/
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          add_relation(target_transform_key, constraint_op_key, cti->name);
        }
        else {
          /* Standard object relation. */
          /* TODO: loc vs rot vs scale? */
          if (&ct->tar->id == id) {
            /* Constraint targeting its own object:
             * - This case is fine IF we're dealing with a bone
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
                 CONSTRAINT_TYPE_TRANSLIKE))
        {
          /* TODO(sergey): Add used space check. */
          ComponentKey target_transform_key(&ct->tar->id, NodeType::TRANSFORM);
          add_relation(target_transform_key, constraint_op_key, cti->name);
        }
      }
      BKE_constraint_targets_flush(con, &targets, true);
    }
  }
}

void DepsgraphRelationBuilder::build_animdata(ID *id)
{
  /* Images. */
  build_animation_images(id);
  /* Animation curves, NLA, and Animation datablock. */
  build_animdata_curves(id);
  /* Drivers. */
  build_animdata_drivers(id);

  if (check_id_has_anim_component(id)) {
    ComponentKey animation_key(id, NodeType::ANIMATION);
    ComponentKey parameters_key(id, NodeType::PARAMETERS);
    add_relation(animation_key, parameters_key, "Animation -> Parameters");
    build_animdata_force(id);
  }
}

void DepsgraphRelationBuilder::build_animdata_curves(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    return;
  }
  if (adt->action != nullptr) {
    build_action(adt->action);
  }
  if (adt->action == nullptr && BLI_listbase_is_empty(&adt->nla_tracks)) {
    return;
  }
  /* Ensure evaluation order from entry to exit. */
  OperationKey animation_entry_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_ENTRY);
  OperationKey animation_eval_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_EVAL);
  OperationKey animation_exit_key(id, NodeType::ANIMATION, OperationCode::ANIMATION_EXIT);
  add_relation(animation_entry_key, animation_eval_key, "Init -> Eval");
  add_relation(animation_eval_key, animation_exit_key, "Eval -> Exit");
  /* Wire up dependency from Actions. */
  ComponentKey adt_key(id, NodeType::ANIMATION);
  /* Relation from action itself. */
  if (adt->action != nullptr) {
    ComponentKey action_key(&adt->action->id, NodeType::ANIMATION);
    add_relation(action_key, adt_key, "Action -> Animation");
  }
  /* Get source operations. */
  Node *node_from = get_node(adt_key);
  BLI_assert(node_from != nullptr);
  if (node_from == nullptr) {
    return;
  }
  OperationNode *operation_from = node_from->get_exit_operation();
  BLI_assert(operation_from != nullptr);
  /* Build relations from animation operation to properties it changes. */
  if (adt->action != nullptr) {
    build_animdata_action_targets(id, adt->slot_handle, adt_key, operation_from, adt->action);
  }
  LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
    if (!BKE_nlatrack_is_enabled(*adt, *nlt)) {
      continue;
    }
    build_animdata_nlastrip_targets(id, adt_key, operation_from, &nlt->strips);
  }
}

void DepsgraphRelationBuilder::build_animdata_fcurve_target(
    ID *id, PointerRNA id_ptr, ComponentKey &adt_key, OperationNode *operation_from, FCurve *fcu)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  if (!RNA_path_resolve_full(&id_ptr, fcu->rna_path, &ptr, &prop, &index)) {
    return;
  }
  Node *node_to = rna_node_query_.find_node(&ptr, prop, RNAPointerSource::ENTRY);
  if (node_to == nullptr) {
    return;
  }
  OperationNode *operation_to = node_to->get_entry_operation();
  /* NOTE: Special case for bones, avoid relation from animation to
   * each of the bones. Bone evaluation could only start from pose
   * init anyway. */
  if (operation_to->opcode == OperationCode::BONE_LOCAL) {
    OperationKey pose_init_key(id, NodeType::EVAL_POSE, OperationCode::POSE_INIT);
    add_relation(adt_key, pose_init_key, "Animation -> Prop", RELATION_CHECK_BEFORE_ADD);
    return;
  }
  graph_->add_new_relation(
      operation_from, operation_to, "Animation -> Prop", RELATION_CHECK_BEFORE_ADD);
  /* It is possible that animation is writing to a nested ID data-block,
   * need to make sure animation is evaluated after target ID is copied. */
  const IDNode *id_node_from = operation_from->owner->owner;
  const IDNode *id_node_to = operation_to->owner->owner;
  if (id_node_from != id_node_to) {
    ComponentKey cow_key(id_node_to->id_orig, NodeType::COPY_ON_EVAL);
    add_relation(cow_key,
                 adt_key,
                 "Animated Copy-on-Eval -> Animation",
                 RELATION_CHECK_BEFORE_ADD | RELATION_FLAG_NO_FLUSH);
  }
}

void DepsgraphRelationBuilder::build_animdata_curves_targets(ID *id,
                                                             ComponentKey &adt_key,
                                                             OperationNode *operation_from,
                                                             ListBase *curves)
{
  /* Iterate over all curves and build relations. */
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  LISTBASE_FOREACH (FCurve *, fcu, curves) {
    build_animdata_fcurve_target(id, id_ptr, adt_key, operation_from, fcu);
  }
}

void DepsgraphRelationBuilder::build_animdata_action_targets(ID *id,
                                                             const int32_t slot_handle,
                                                             ComponentKey &adt_key,
                                                             OperationNode *operation_from,
                                                             bAction *dna_action)
{
  BLI_assert(id != nullptr);
  BLI_assert(operation_from != nullptr);
  BLI_assert(dna_action != nullptr);
  animrig::Action &action = dna_action->wrap();

  if (action.is_empty()) {
    return;
  }
  if (action.is_action_legacy()) {
    build_animdata_curves_targets(id, adt_key, operation_from, &action.curves);
    return;
  }

  const animrig::Slot *slot = action.slot_for_handle(slot_handle);
  if (slot == nullptr) {
    /* If there's no matching slot, there's no Action dependency. */
    return;
  }

  PointerRNA id_ptr = RNA_id_pointer_create(id);

  for (animrig::Layer *layer : action.layers()) {
    for (animrig::Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case animrig::Strip::Type::Keyframe: {
          animrig::StripKeyframeData &strip_data = strip->data<animrig::StripKeyframeData>(action);
          animrig::Channelbag *channels = strip_data.channelbag_for_slot(*slot);
          if (channels == nullptr) {
            /* Go to next strip. */
            break;
          }
          for (FCurve *fcu : channels->fcurves()) {
            build_animdata_fcurve_target(id, id_ptr, adt_key, operation_from, fcu);
          }
          break;
        }
      }
    }
  }
}

void DepsgraphRelationBuilder::build_animdata_nlastrip_targets(ID *id,
                                                               ComponentKey &adt_key,
                                                               OperationNode *operation_from,
                                                               ListBase *strips)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    if (strip->act != nullptr) {
      build_action(strip->act);

      ComponentKey action_key(&strip->act->id, NodeType::ANIMATION);
      add_relation(action_key, adt_key, "Action -> Animation");

      build_animdata_action_targets(
          id, strip->action_slot_handle, adt_key, operation_from, strip->act);
    }
    else if (strip->strips.first != nullptr) {
      build_animdata_nlastrip_targets(id, adt_key, operation_from, &strip->strips);
    }
  }
}

void DepsgraphRelationBuilder::build_animdata_drivers(ID *id)
{
  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr || BLI_listbase_is_empty(&adt->drivers)) {
    return;
  }
  ComponentKey adt_key(id, NodeType::ANIMATION);
  OperationKey driver_unshare_key(id, NodeType::PARAMETERS, OperationCode::DRIVER_UNSHARE);

  LISTBASE_FOREACH (FCurve *, fcu, &adt->drivers) {
    OperationKey driver_key(id,
                            NodeType::PARAMETERS,
                            OperationCode::DRIVER,
                            fcu->rna_path ? fcu->rna_path : "",
                            fcu->array_index);

    /* create the driver's relations to targets */
    build_driver(id, fcu);

    /* prevent driver from occurring before its own animation... */
    if (adt->action || adt->nla_tracks.first) {
      add_relation(adt_key, driver_key, "AnimData Before Drivers");
    }

    if (data_path_maybe_shared(*id, fcu->rna_path)) {
      add_relation(driver_unshare_key, driver_key, "Un-share shared data before drivers");
    }
  }
}

void DepsgraphRelationBuilder::build_animation_images(ID *id)
{
  /* See #DepsgraphNodeBuilder::build_animation_images. */
  bool has_image_animation = false;
  if (ELEM(GS(id->name), ID_MA, ID_WO)) {
    bNodeTree *ntree = *bke::node_tree_ptr_from_id(id);
    if (ntree != nullptr && ntree->runtime->runtime_flag & NTREE_RUNTIME_FLAG_HAS_IMAGE_ANIMATION)
    {
      has_image_animation = true;
    }
  }

  if (has_image_animation || BKE_image_user_id_has_animation(id)) {
    OperationKey image_animation_key(
        id, NodeType::IMAGE_ANIMATION, OperationCode::IMAGE_ANIMATION);
    TimeSourceKey time_src_key;
    add_relation(time_src_key, image_animation_key, "TimeSrc -> Image Animation");

    /* The image users of these ids may change during evaluation. Make sure that the image
     * animation update happens after evaluation. */
    if (GS(id->name) == ID_MA) {
      OperationKey material_update_key(id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
      add_relation(material_update_key, image_animation_key, "Material Update -> Image Animation");
    }
    else if (GS(id->name) == ID_WO) {
      OperationKey world_update_key(id, NodeType::SHADING, OperationCode::WORLD_UPDATE);
      add_relation(world_update_key, image_animation_key, "World Update -> Image Animation");
    }
    else if (GS(id->name) == ID_NT) {
      OperationKey ntree_output_key(id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
      add_relation(ntree_output_key, image_animation_key, "NTree Output -> Image Animation");
    }
  }
}

void DepsgraphRelationBuilder::build_animdata_force(ID *id)
{
  if (GS(id->name) != ID_OB) {
    return;
  }

  const Object *object = (Object *)id;
  if (object->pd == nullptr || object->pd->forcefield == PFIELD_NULL) {
    return;
  }

  /* Updates to animation data (in the UI, for example by altering FCurve Modifier parameters
   * animating force field strength) may need to rebuild the rigid body world. */
  ComponentKey animation_key(id, NodeType::ANIMATION);
  OperationKey rigidbody_key(&scene_->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_REBUILD);
  add_relation(animation_key, rigidbody_key, "Animation -> Rigid Body");
}

void DepsgraphRelationBuilder::build_action(bAction *dna_action)
{
  if (built_map_.check_is_built_and_tag(dna_action)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(dna_action->id);

  build_parameters(&dna_action->id);
  build_idproperties(dna_action->id.properties);
  build_idproperties(dna_action->id.system_properties);

  blender::animrig::Action &action = dna_action->wrap();
  if (!action.is_empty()) {
    TimeSourceKey time_src_key;
    ComponentKey animation_key(&dna_action->id, NodeType::ANIMATION);
    add_relation(time_src_key, animation_key, "TimeSrc -> Animation");
  }
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
  if (driver_depends_on_time(driver)) {
    TimeSourceKey time_src_key;
    add_relation(time_src_key, driver_key, "TimeSrc -> Driver");
  }
}

void DepsgraphRelationBuilder::build_driver_data(ID *id, FCurve *fcu)
{
  /* Validate the RNA path pointer just in case. */
  const char *rna_path = fcu->rna_path;
  if (rna_path == nullptr || rna_path[0] == '\0') {
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
  ID *id_ptr = property_entry_key.ptr.owner_id;
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
    if (bone == nullptr) {
      fprintf(stderr, "Couldn't find armature bone name for driver path - '%s'\n", rna_path);
      return;
    }

    const char *prop_identifier = RNA_property_identifier(property_entry_key.prop);
    const bool driver_targets_bbone = STRPREFIX(prop_identifier, "bbone_");

    /* Find objects which use this, and make their eval callbacks depend on this. */
    for (IDNode *to_node : graph_->id_nodes) {
      if (GS(to_node->id_orig->name) != ID_OB) {
        continue;
      }

      /* We only care about objects with pose data which use this. */
      Object *object = (Object *)to_node->id_orig;
      if (object->data != id_ptr || object->pose == nullptr) {
        continue;
      }

      bPoseChannel *pchan = BKE_pose_channel_find_name(object->pose, bone->name);
      if (pchan == nullptr) {
        continue;
      }

      OperationCode target_op = OperationCode::BONE_LOCAL;
      if (driver_targets_bbone) {
        target_op = check_pchan_has_bbone_segments(object, pchan) ? OperationCode::BONE_SEGMENTS :
                                                                    OperationCode::BONE_DONE;
      }
      OperationKey bone_key(&object->id, NodeType::BONE, pchan->name, target_op);
      add_relation(driver_key, bone_key, "Arm Bone -> Driver -> Bone");
    }
    /* Make the driver depend on copy-on-eval, similar to the generic case below. */
    if (id_ptr != id) {
      ComponentKey cow_key(id_ptr, NodeType::COPY_ON_EVAL);
      add_relation(
          cow_key, driver_key, "Driven Copy-on-Eval -> Driver", RELATION_CHECK_BEFORE_ADD);
    }
  }
  else {
    /* If it's not a Bone, handle the generic single dependency case. */
    Node *node_to = get_node(property_entry_key);
    if (node_to != nullptr) {
      add_relation(driver_key, property_entry_key, "Driver -> Driven Property");
    }

    /* Similar to the case with f-curves, driver might drive a nested
     * data-block, which means driver execution should wait for that
     * data-block to be copied. */
    {
      PointerRNA id_ptr = RNA_id_pointer_create(id);
      PointerRNA ptr;
      if (RNA_path_resolve_full(&id_ptr, fcu->rna_path, &ptr, nullptr, nullptr)) {
        if (id_ptr.owner_id != ptr.owner_id) {
          ComponentKey cow_key(ptr.owner_id, NodeType::COPY_ON_EVAL);
          add_relation(
              cow_key, driver_key, "Driven Copy-on-Eval -> Driver", RELATION_CHECK_BEFORE_ADD);
        }
      }
    }
    if (rna_prop_affects_parameters_node(&property_entry_key.ptr, property_entry_key.prop)) {
      RNAPathKey property_exit_key(property_entry_key.id,
                                   property_entry_key.ptr,
                                   property_entry_key.prop,
                                   RNAPointerSource::EXIT);
      OperationKey parameters_key(id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EVAL);
      add_relation(property_exit_key, parameters_key, "Driven Property -> Properties");
    }
  }

  /* Assume drivers on a node tree affect the evaluated output of the node tree. In theory we could
   * check if the driven value actually affects the output, i.e. if it drives a node that is linked
   * to the output. */
  if (GS(id_ptr->name) == ID_NT) {
    ComponentKey ntree_output_key(id_ptr, NodeType::NTREE_OUTPUT);
    add_relation(driver_key, ntree_output_key, "Drivers -> NTree Output");
    if (reinterpret_cast<bNodeTree *>(id_ptr)->type == NTREE_GEOMETRY) {
      OperationKey ntree_geo_preprocess_key(
          id, NodeType::NTREE_GEOMETRY_PREPROCESS, OperationCode::NTREE_GEOMETRY_PREPROCESS);
      add_relation(driver_key, ntree_geo_preprocess_key, "Drivers -> NTree Geo Preprocess");
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

  DriverTargetContext driver_target_context;
  driver_target_context.scene = graph_->scene;
  driver_target_context.view_layer = graph_->view_layer;

  LISTBASE_FOREACH (DriverVar *, dvar, &driver->variables) {
    /* Only used targets. */
    DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
      PointerRNA target_prop;
      if (!driver_get_target_property(&driver_target_context, dvar, dtar, &target_prop)) {
        continue;
      }

      /* Property is always expected to be resolved to a non-null RNA property, which is always
       * relative to some ID. */
      BLI_assert(target_prop.owner_id);

      ID *target_id = target_prop.owner_id;

      build_id(target_id);
      build_driver_id_property(target_prop, dtar->rna_path);

      Object *object = nullptr;
      if (GS(target_id->name) == ID_OB) {
        object = (Object *)target_id;
      }
      /* Special handling for directly-named bones. */
      if ((dtar->flag & DTAR_FLAG_STRUCT_REF) && (object && object->type == OB_ARMATURE) &&
          (dtar->pchan_name[0]))
      {
        bPoseChannel *target_pchan = BKE_pose_channel_find_name(object->pose, dtar->pchan_name);
        if (target_pchan == nullptr) {
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
      else if (dtar->rna_path != nullptr && dtar->rna_path[0] != '\0') {
        build_driver_rna_path_variable(
            driver_key, self_key, target_id, target_prop, dtar->rna_path);

        /* Add relations to all other cameras used by the scene timeline if applicable. */
        if (const char *camera_path = get_rna_path_relative_to_scene_camera(
                scene_, target_prop, dtar->rna_path))
        {
          build_driver_scene_camera_variable(driver_key, self_key, scene_, camera_path);
        }

        /* The RNA getter for `object.data` can write to the mesh datablock due
         * to the call to `BKE_mesh_wrapper_ensure_subdivision()`. This relation
         * ensures it is safe to call when the driver is evaluated.
         *
         * For the sake of making the code more generic/defensive, the relation
         * is added for any geometry type.
         *
         * See #96289 for more info. */
        if (object != nullptr && OB_TYPE_IS_GEOMETRY(object->type)) {
          StringRef rna_path(dtar->rna_path);
          if (rna_path == "data" || rna_path.startswith("data.")) {
            ComponentKey ob_key(target_id, NodeType::GEOMETRY);
            add_relation(ob_key, driver_key, "ID -> Driver");
          }
        }
      }
      else {
        /* If rna_path is nullptr, and DTAR_FLAG_STRUCT_REF isn't set, this
         * is an incomplete target reference, so nothing to do here. */
      }
    }
    DRIVER_TARGETS_LOOPER_END;
  }
}

void DepsgraphRelationBuilder::build_driver_scene_camera_variable(const OperationKey &driver_key,
                                                                  const RNAPathKey &self_key,
                                                                  Scene *scene,
                                                                  const char *rna_path)
{
  /* First, add relations to all cameras used in the timeline,
   * excluding scene->camera which was already handled by the caller. */
  bool animated = false;

  LISTBASE_FOREACH (TimeMarker *, marker, &scene->markers) {
    if (!ELEM(marker->camera, nullptr, scene->camera)) {
      PointerRNA camera_ptr = RNA_id_pointer_create(&marker->camera->id);
      build_driver_id_property(camera_ptr, rna_path);
      build_driver_rna_path_variable(driver_key, self_key, &scene->id, camera_ptr, rna_path);
      animated = true;
    }
  }

  /* If timeline indeed switches the camera, this variable also implicitly depends on time. */
  if (animated) {
    TimeSourceKey time_src_key;
    add_relation(time_src_key, driver_key, "TimeSrc -> Driver Camera Ref");
  }
}

void DepsgraphRelationBuilder::build_driver_rna_path_variable(const OperationKey &driver_key,
                                                              const RNAPathKey &self_key,
                                                              ID *target_id,
                                                              const PointerRNA &target_prop,
                                                              const char *rna_path)
{
  RNAPathKey variable_exit_key(target_prop, rna_path, RNAPointerSource::EXIT);
  if (RNA_pointer_is_null(&variable_exit_key.ptr)) {
    return;
  }
  if (is_same_bone_dependency(variable_exit_key, self_key) ||
      is_same_nodetree_node_dependency(variable_exit_key, self_key))
  {
    return;
  }
  add_relation(variable_exit_key, driver_key, "RNA Target -> Driver");

  /* It is possible that RNA path points to a property of a different ID than the target_id:
   * for example, paths like "data" on Object, "camera" on Scene.
   *
   * For the demonstration purposes lets consider a driver variable uses Scene ID as target
   * and "camera.location.x" as its RNA path. If the scene has 2 different cameras at
   * 2 different locations changing the active scene camera is expected to immediately be
   * reflected in the variable value. In order to achieve this behavior we create a relation
   * from the target ID to the driver so that if the ID property of the target ID changes the
   * driver is re-evaluated.
   *
   * The most straightforward (at the moment of writing this comment) way of figuring out
   * such relation is to use copy-on-evaluation operation of the target ID. There are two down
   * sides of this approach which are considered a design limitation as there is a belief
   * that they are not common in practice or are not reliable due to other issues:
   *
   * - IDs which are not covered with the copy-on-evaluation mechanism.
   *
   *   Such IDs are either do not have ID properties, or are not part of the dependency
   *   graph.
   *
   * - Modifications of evaluated IDs from a Python handler.
   *   Such modifications are not fully integrated in the dependency graph evaluation as it
   *   has issues with copy-on-evaluation tagging and the fact that relations are defined by the
   *   original main database status.
   *
   * The original report for this is #98618.
   *
   * The not-so-obvious part is that we don't do such relation for the context properties.
   * They are resolved at the graph build time and do not change at runtime (#107081).
   * Thus scene has to be excluded as a special case; this is OK because changes to
   * scene.camera not caused by animation should actually force a dependency graph rebuild.
   */
  if (target_id != variable_exit_key.ptr.owner_id && GS(target_id->name) != ID_SCE) {
    if (deg_eval_copy_is_needed(GS(target_id->name))) {
      ComponentKey target_id_key(target_id, NodeType::COPY_ON_EVAL);
      add_relation(target_id_key, driver_key, "Target ID -> Driver");
    }
  }
}

void DepsgraphRelationBuilder::build_driver_id_property(const PointerRNA &target_prop,
                                                        const char *rna_path_from_target_prop)
{
  if (rna_path_from_target_prop == nullptr || rna_path_from_target_prop[0] == '\0') {
    return;
  }

  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
  if (!RNA_path_resolve_full(&target_prop, rna_path_from_target_prop, &ptr, &prop, &index)) {
    return;
  }
  if (prop == nullptr) {
    return;
  }
  if (!rna_prop_affects_parameters_node(&ptr, prop)) {
    return;
  }
  if (ptr.owner_id) {
    build_id(ptr.owner_id);
  }
  const char *prop_identifier = RNA_property_identifier(prop);
  /* Custom properties of bones are placed in their components to improve granularity. */
  OperationKey id_property_key;
  if (RNA_struct_is_a(ptr.type, &RNA_PoseBone)) {
    const bPoseChannel *pchan = static_cast<const bPoseChannel *>(ptr.data);
    id_property_key = OperationKey(
        ptr.owner_id, NodeType::BONE, pchan->name, OperationCode::ID_PROPERTY, prop_identifier);
    /* Create relation from the parameters component so that tagging armature for parameters update
     * properly propagates updates to all properties on bones and deeper (if needed). */
    OperationKey parameters_init_key(
        ptr.owner_id, NodeType::PARAMETERS, OperationCode::PARAMETERS_ENTRY);
    add_relation(
        parameters_init_key, id_property_key, "Init -> ID Property", RELATION_CHECK_BEFORE_ADD);
  }
  else {
    id_property_key = OperationKey(
        ptr.owner_id, NodeType::PARAMETERS, OperationCode::ID_PROPERTY, prop_identifier);
  }
  OperationKey parameters_exit_key(
      ptr.owner_id, NodeType::PARAMETERS, OperationCode::PARAMETERS_EXIT);
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

void DepsgraphRelationBuilder::build_dimensions(Object *object)
{
  OperationKey dimensions_key(&object->id, NodeType::PARAMETERS, OperationCode::DIMENSIONS);
  ComponentKey geometry_key(&object->id, NodeType::GEOMETRY);
  ComponentKey transform_key(&object->id, NodeType::TRANSFORM);
  add_relation(geometry_key, dimensions_key, "Geometry -> Dimensions");
  add_relation(transform_key, dimensions_key, "Transform -> Dimensions");
}

void DepsgraphRelationBuilder::build_world(World *world)
{
  if (built_map_.check_is_built_and_tag(world)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(world->id);

  build_idproperties(world->id.properties);
  build_idproperties(world->id.system_properties);
  /* animation */
  build_animdata(&world->id);
  build_parameters(&world->id);

  /* Animated / driven parameters (without nodetree). */
  OperationKey world_key(&world->id, NodeType::SHADING, OperationCode::WORLD_UPDATE);
  ComponentKey parameters_key(&world->id, NodeType::PARAMETERS);
  add_relation(parameters_key, world_key, "World's parameters");

  /* world's nodetree */
  if (world->nodetree != nullptr) {
    build_nodetree(world->nodetree);
    OperationKey ntree_key(
        &world->nodetree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
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
    if (effector_relation->pd != nullptr) {
      const short shape = effector_relation->pd->shape;
      if (ELEM(shape, PFIELD_SHAPE_SURFACE, PFIELD_SHAPE_POINTS)) {
        ComponentKey effector_geometry_key(&effector_relation->ob->id, NodeType::GEOMETRY);
        add_relation(effector_geometry_key, rb_init_key, "RigidBody Field");
      }
      if ((effector_relation->pd->forcefield == PFIELD_TEXTURE) &&
          (effector_relation->pd->tex != nullptr))
      {
        ComponentKey tex_key(&effector_relation->pd->tex->id, NodeType::GENERIC_DATABLOCK);
        add_relation(tex_key, rb_init_key, "Force field Texture");
      }
    }
  }
  /* Objects. */
  if (rbw->group != nullptr) {
    build_collection(nullptr, rbw->group);
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (rbw->group, object) {
      if (object->type != OB_MESH) {
        continue;
      }
      if (object->rigidbody_object == nullptr) {
        continue;
      }

      if (object->parent != nullptr && object->parent->rigidbody_object != nullptr &&
          object->parent->rigidbody_object->shape == RB_SHAPE_COMPOUND)
      {
        /* If we are a child of a compound shape object, the transforms and sim evaluation will be
         * handled by the parent compound shape object. Do not add any evaluation triggers
         * for the child objects.
         */
        continue;
      }

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
      if (rigidbody_object_depends_on_evaluated_geometry(object->rigidbody_object)) {
        /* NOTE: We prefer this relation to be never killed, to avoid
         * access partially evaluated mesh from solver. */
        ComponentKey object_geometry_key(&object->id, NodeType::GEOMETRY);
        add_relation(object_geometry_key,
                     rb_simulate_key,
                     "Object Geom Eval -> Rigidbody Sim Eval",
                     RELATION_FLAG_GODMODE);
      }

      /* Final transform is whatever the solver gave to us. */
      if (object->rigidbody_object->type == RBO_TYPE_ACTIVE) {
        /* We do not have to update the objects final transform after the simulation if it is
         * passive or controlled by the animation system in blender.
         * (Bullet doesn't move the object at all in these cases).
         * But we can't update the depsgraph when the animated property in changed during playback.
         * So always assume that active bodies needs updating. */
        OperationKey rb_transform_copy_key(
            &object->id, NodeType::TRANSFORM, OperationCode::RIGIDBODY_TRANSFORM_COPY);
        /* Rigid body synchronization depends on the actual simulation. */
        add_relation(rb_simulate_key, rb_transform_copy_key, "Rigidbody Sim Eval -> RBO Sync");

        OperationKey object_transform_final_key(
            &object->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);
        add_relation(rb_transform_copy_key,
                     object_transform_final_key,
                     "Rigidbody Sync -> Transform Final");
      }

      /* Relations between colliders and force fields, needed for force field absorption. */
      build_collision_relations(graph_, nullptr, eModifierType_Collision);
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }
}

void DepsgraphRelationBuilder::build_particle_systems(Object *object)
{
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
    else if ((psys->flag & PSYS_HAIR_DYNAMICS) && psys->clmd != nullptr &&
             psys->clmd->coll_parms != nullptr)
    {
      add_particle_collision_relations(
          psys_key, object, psys->clmd->coll_parms->group, "Hair Collision");
    }
    /* Effectors. */
    add_particle_forcefield_relations(
        psys_key, object, psys, part->effector_weights, part->type == PART_HAIR, "Particle Field");
    /* Boids. */
    if (part->boids != nullptr) {
      LISTBASE_FOREACH (BoidState *, state, &part->boids->states) {
        LISTBASE_FOREACH (BoidRule *, rule, &state->rules) {
          Object *ruleob = nullptr;
          if (rule->type == eBoidRuleType_Avoid) {
            ruleob = ((BoidRuleGoalAvoid *)rule)->ob;
          }
          else if (rule->type == eBoidRuleType_FollowLeader) {
            ruleob = ((BoidRuleFollowLeader *)rule)->ob;
          }
          if (ruleob != nullptr) {
            ComponentKey ruleob_key(&ruleob->id, NodeType::TRANSFORM);
            add_relation(ruleob_key, psys_key, "Boid Rule");
          }
        }
      }
    }
    /* Keyed particle targets. */
    if (ELEM(part->phystype, PART_PHYS_KEYED, PART_PHYS_BOIDS)) {
      LISTBASE_FOREACH (ParticleTarget *, particle_target, &psys->targets) {
        if (ELEM(particle_target->ob, nullptr, object)) {
          continue;
        }
        /* Make sure target object is pulled into the graph. */
        build_object(particle_target->ob);
        /* Use geometry component, since that's where particles are
         * actually evaluated. */
        ComponentKey target_key(&particle_target->ob->id, NodeType::GEOMETRY);
        add_relation(target_key, psys_key, "Keyed Target");
      }
    }
    /* Visualization. */
    switch (part->ren_as) {
      case PART_DRAW_OB:
        if (part->instance_object != nullptr) {
          /* Make sure object's relations are all built. */
          build_object(part->instance_object);
          /* Build relation for the particle visualization. */
          build_particle_system_visualization_object(object, psys, part->instance_object);
        }
        break;
      case PART_DRAW_GR:
        if (part->instance_collection != nullptr) {
          build_collection(nullptr, part->instance_collection);
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
  if (built_map_.check_is_built_and_tag(part)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(part->id);

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
  for (MTex *mtex : part->mtex) {
    if (mtex == nullptr || mtex->tex == nullptr) {
      continue;
    }
    build_texture(mtex->tex);
    ComponentKey texture_key(&mtex->tex->id, NodeType::GENERIC_DATABLOCK);
    add_relation(texture_key,
                 particle_settings_reset_key,
                 "Particle Texture -> Particle Reset",
                 RELATION_FLAG_FLUSH_USER_EDIT_ONLY);
    add_relation(texture_key, particle_settings_eval_key, "Particle Texture -> Particle Eval");
    /* TODO(sergey): Consider moving texture space handling to its own
     * function. */
    if (mtex->texco == TEXCO_OBJECT && mtex->object != nullptr) {
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
  if (built_map_.check_is_built_and_tag(key)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(key->id);

  build_idproperties(key->id.properties);
  build_idproperties(key->id.system_properties);
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
 * - The actual evaluated of the derived geometry (e.g. #Mesh, #Curves, etc.)
 *   occurs in the Geometry component of the object which references this.
 *   This includes modifiers, and the temporary "ubereval" for geometry.
 *   Therefore, each user of a piece of shared geometry data ends up evaluating
 *   its own version of the stuff, complete with whatever modifiers it may use.
 *
 * - The data-blocks for the geometry data - "obdata" (e.g. ID_ME, ID_CU_LEGACY, ID_LT.)
 *   are used for
 *     1) calculating the bounding boxes of the geometry data,
 *     2) aggregating inward links from other objects (e.g. for text on curve)
 *        and also for the links coming from the shapekey data-blocks
 * - Animation/Drivers affecting the parameters of the geometry are made to
 *   trigger updates on the obdata geometry component, which then trigger
 *   downstream re-evaluation of the individual instances of this geometry.
 */
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
   * evaluated prior to Scene's evaluated copy is ready. */
  ComponentKey scene_key(&scene_->id, NodeType::SCENE);
  add_relation(scene_key, obdata_ubereval_key, "Copy-on-Eval Relation", RELATION_FLAG_NO_FLUSH);
  /* Relation to the instance, so that instancer can use geometry of this object. */
  add_relation(ComponentKey(&object->id, NodeType::GEOMETRY),
               OperationKey(&object->id, NodeType::INSTANCING, OperationCode::INSTANCE_GEOMETRY),
               "Transform -> Instance Geometry");
  /* Shader FX. */
  if (object->shader_fx.first != nullptr) {
    ModifierUpdateDepsgraphContext ctx = {};
    ctx.scene = scene_;
    ctx.object = object;
    LISTBASE_FOREACH (ShaderFxData *, fx, &object->shader_fx) {
      const ShaderFxTypeInfo *fxi = BKE_shaderfx_get_info((ShaderFxType)fx->type);
      if (fxi->update_depsgraph) {
        DepsNodeHandle handle = create_node_handle(obdata_ubereval_key);
        ctx.node = reinterpret_cast<::DepsNodeHandle *>(&handle);
        fxi->update_depsgraph(fx, &ctx);
      }
      if (BKE_shaderfx_depends_ontime(fx)) {
        TimeSourceKey time_src_key;
        add_relation(time_src_key, obdata_ubereval_key, "Time Source");
      }
    }
  }
  /* Materials. */
  build_materials(&object->id, object->mat, object->totcol);
  /* Geometry collision. */
  if (ELEM(object->type, OB_MESH, OB_CURVES_LEGACY, OB_LATTICE)) {
    // add geometry collider relations
  }
  /* Make sure uber update is the last in the dependencies.
   * Only do it here unless there are modifiers. This avoids transitive relations. */
  if (BLI_listbase_is_empty(&object->modifiers)) {
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
  if (key != nullptr) {
    if (key->adt != nullptr) {
      if (key->adt->action || key->adt->nla_tracks.first) {
        ComponentKey obdata_key((ID *)object->data, NodeType::GEOMETRY);
        ComponentKey adt_key(&key->id, NodeType::ANIMATION);
        add_relation(adt_key, obdata_key, "Animation");
      }
    }
  }
  build_dimensions(object);
  /* Synchronization back to original object. */
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
  /* Shading. */
  ComponentKey geometry_shading_key(obdata, NodeType::SHADING);
  OperationKey object_shading_key(&object->id, NodeType::SHADING, OperationCode::SHADING);
  add_relation(geometry_shading_key, object_shading_key, "Geometry Shading -> Object Shading");
}

void DepsgraphRelationBuilder::build_object_data_geometry_datablock(ID *obdata)
{
  if (built_map_.check_is_built_and_tag(obdata)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(*obdata);

  build_idproperties(obdata->properties);
  build_idproperties(obdata->system_properties);
  /* Animation. */
  build_animdata(obdata);
  build_parameters(obdata);
  /* ShapeKeys. */
  Key *key = BKE_key_from_id(obdata);
  if (key != nullptr) {
    build_shapekeys(key);
  }
  /* Link object data evaluation node to exit operation. */
  OperationKey obdata_geom_eval_key(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL);
  OperationKey obdata_geom_done_key(obdata, NodeType::GEOMETRY, OperationCode::GEOMETRY_EVAL_DONE);
  add_relation(obdata_geom_eval_key, obdata_geom_done_key, "ObData Geom Eval Done");

  /* Link object data evaluation to parameter evaluation. */
  ComponentKey parameters_key(obdata, NodeType::PARAMETERS);
  add_relation(parameters_key, obdata_geom_eval_key, "ObData Geom Params");

  /* Type-specific links. */
  const ID_Type id_type = GS(obdata->name);
  switch (id_type) {
    case ID_ME:
      break;
    case ID_MB:
      break;
    case ID_CU_LEGACY: {
      Curve *cu = (Curve *)obdata;
      if (cu->bevobj != nullptr) {
        ComponentKey bevob_geom_key(&cu->bevobj->id, NodeType::GEOMETRY);
        add_relation(bevob_geom_key, obdata_geom_eval_key, "Curve Bevel Geometry");
        ComponentKey bevob_key(&cu->bevobj->id, NodeType::TRANSFORM);
        add_relation(bevob_key, obdata_geom_eval_key, "Curve Bevel Transform");
        build_object(cu->bevobj);
      }
      if (cu->taperobj != nullptr) {
        ComponentKey taperob_key(&cu->taperobj->id, NodeType::GEOMETRY);
        add_relation(taperob_key, obdata_geom_eval_key, "Curve Taper");
        build_object(cu->taperobj);
      }
      if (cu->textoncurve != nullptr) {
        ComponentKey textoncurve_geom_key(&cu->textoncurve->id, NodeType::GEOMETRY);
        add_relation(textoncurve_geom_key, obdata_geom_eval_key, "Text on Curve Geometry");
        ComponentKey textoncurve_key(&cu->textoncurve->id, NodeType::TRANSFORM);
        add_relation(textoncurve_key, obdata_geom_eval_key, "Text on Curve Transform");
        build_object(cu->textoncurve);
      }
      /* Special relation to ensure active spline index gets properly updated.
       *
       * The active spline index is stored on the Curve data-block, and the curve evaluation might
       * create a new curve data-block for the result, which does not intrinsically sharing the
       * active spline index. Hence a special relation is added to ensure the modifier stack is
       * evaluated when selection changes. */
      {
        const OperationKey object_data_select_key(
            obdata, NodeType::BATCH_CACHE, OperationCode::GEOMETRY_SELECT_UPDATE);
        add_relation(object_data_select_key, obdata_geom_eval_key, "Active Spline Update");
      }
      break;
    }
    case ID_LT:
      break;
    case ID_GD_LEGACY: /* Grease Pencil */
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
        if ((ma != nullptr) && (ma->gp_style != nullptr)) {
          OperationKey material_key(&ma->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
          add_relation(material_key, geometry_key, "Material -> GP Data");
        }
      }

      /* Layer parenting need react to the parent object transformation. */
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        if (gpl->parent != nullptr) {
          ComponentKey gpd_geom_key(&gpd->id, NodeType::GEOMETRY);

          if (gpl->partype == PARBONE) {
            ComponentKey bone_key(&gpl->parent->id, NodeType::BONE, gpl->parsubstr);
            OperationKey armature_key(
                &gpl->parent->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);

            add_relation(bone_key, gpd_geom_key, "Bone Parent");
            add_relation(armature_key, gpd_geom_key, "Armature Parent");
          }
          else {
            ComponentKey transform_key(&gpl->parent->id, NodeType::TRANSFORM);
            add_relation(transform_key, gpd_geom_key, "GPencil Parent Layer");
          }
        }
      }
      break;
    }
    case ID_CV: {
      Curves *curves_id = reinterpret_cast<Curves *>(obdata);
      if (curves_id->surface != nullptr) {
        build_object(curves_id->surface);

        /* The relations between the surface and the curves are handled as part of the modifier
         * stack building. */
      }
      break;
    }
    case ID_PT:
      break;
    case ID_VO: {
      Volume *volume = (Volume *)obdata;
      if (volume->is_sequence) {
        TimeSourceKey time_key;
        ComponentKey geometry_key(obdata, NodeType::GEOMETRY);
        add_relation(time_key, geometry_key, "Volume sequence time");
      }
      break;
    }
    case ID_GP: {
      GreasePencil &grease_pencil = *reinterpret_cast<GreasePencil *>(obdata);

      /* Update geometry when time is changed. */
      TimeSourceKey time_key;
      ComponentKey geometry_key(&grease_pencil.id, NodeType::GEOMETRY);
      add_relation(time_key, geometry_key, "Grease Pencil Frame Change");

      /* Add relations for layer parents. */
      for (const bke::greasepencil::Layer *layer : grease_pencil.layers()) {
        Object *parent = layer->parent;
        if (parent == nullptr) {
          continue;
        }
        if (parent->type == OB_ARMATURE && !layer->parent_bone_name().is_empty()) {
          ComponentKey bone_key(&parent->id, NodeType::BONE, layer->parent_bone_name().c_str());
          OperationKey armature_key(
              &parent->id, NodeType::TRANSFORM, OperationCode::TRANSFORM_FINAL);

          add_relation(bone_key, geometry_key, "Grease Pencil Layer Bone Parent");
          add_relation(armature_key, geometry_key, "Grease Pencil Layer Armature Parent");
        }
        else {
          ComponentKey transform_key(&parent->id, NodeType::TRANSFORM);
          add_relation(transform_key, geometry_key, "Grease Pencil Layer Object Parent");
        }
      }
      break;
    }
    default:
      BLI_assert_msg(0, "Should not happen");
      break;
  }
}

void DepsgraphRelationBuilder::build_armature(bArmature *armature)
{
  if (built_map_.check_is_built_and_tag(armature)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(armature->id);

  build_idproperties(armature->id.properties);
  build_idproperties(armature->id.system_properties);
  build_animdata(&armature->id);
  build_parameters(&armature->id);
  build_armature_bones(&armature->bonebase);
  build_armature_bone_collections(armature->collections_span());
}

void DepsgraphRelationBuilder::build_armature_bones(ListBase *bones)
{
  LISTBASE_FOREACH (Bone *, bone, bones) {
    build_idproperties(bone->prop);
    build_idproperties(bone->system_properties);
    build_armature_bones(&bone->childbase);
  }
}

void DepsgraphRelationBuilder::build_armature_bone_collections(
    blender::Span<BoneCollection *> collections)
{
  for (BoneCollection *bcoll : collections) {
    build_idproperties(bcoll->prop);
    build_idproperties(bcoll->system_properties);
  }
}

void DepsgraphRelationBuilder::build_camera(Camera *camera)
{
  if (built_map_.check_is_built_and_tag(camera)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(camera->id);

  build_idproperties(camera->id.properties);
  build_idproperties(camera->id.system_properties);
  build_animdata(&camera->id);
  build_parameters(&camera->id);
  if (camera->dof.focus_object != nullptr) {
    build_object(camera->dof.focus_object);
    ComponentKey camera_parameters_key(&camera->id, NodeType::PARAMETERS);
    ComponentKey dof_ob_key(&camera->dof.focus_object->id, NodeType::TRANSFORM);
    add_relation(dof_ob_key, camera_parameters_key, "Camera DOF");
    if (camera->dof.focus_subtarget[0]) {
      OperationKey target_key(&camera->dof.focus_object->id,
                              NodeType::BONE,
                              camera->dof.focus_subtarget,
                              OperationCode::BONE_DONE);
      add_relation(target_key, camera_parameters_key, "Camera DOF subtarget");
    }
  }
}

/* Lights */
void DepsgraphRelationBuilder::build_light(Light *lamp)
{
  if (built_map_.check_is_built_and_tag(lamp)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(lamp->id);

  build_idproperties(lamp->id.properties);
  build_idproperties(lamp->id.system_properties);
  build_animdata(&lamp->id);
  build_parameters(&lamp->id);

  ComponentKey lamp_parameters_key(&lamp->id, NodeType::PARAMETERS);

  /* For allowing drivers on lamp properties. */
  ComponentKey shading_key(&lamp->id, NodeType::SHADING);
  add_relation(lamp_parameters_key, shading_key, "Light Shading Parameters");

  /* light's nodetree */
  if (lamp->nodetree != nullptr) {
    build_nodetree(lamp->nodetree);
    OperationKey ntree_key(
        &lamp->nodetree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
    add_relation(ntree_key, shading_key, "NTree->Light Parameters");
    build_nested_nodetree(&lamp->id, lamp->nodetree);
  }
}

void DepsgraphRelationBuilder::build_nodetree_socket(bNodeSocket *socket)
{
  build_idproperties(socket->prop);

  if (socket->type == SOCK_OBJECT) {
    Object *object = ((bNodeSocketValueObject *)socket->default_value)->value;
    if (object != nullptr) {
      build_object(object);
    }
  }
  else if (socket->type == SOCK_IMAGE) {
    Image *image = ((bNodeSocketValueImage *)socket->default_value)->value;
    if (image != nullptr) {
      build_image(image);
    }
  }
  else if (socket->type == SOCK_COLLECTION) {
    Collection *collection = ((bNodeSocketValueCollection *)socket->default_value)->value;
    if (collection != nullptr) {
      build_collection(nullptr, collection);
    }
  }
  else if (socket->type == SOCK_TEXTURE) {
    Tex *texture = ((bNodeSocketValueTexture *)socket->default_value)->value;
    if (texture != nullptr) {
      build_texture(texture);
    }
  }
  else if (socket->type == SOCK_MATERIAL) {
    Material *material = ((bNodeSocketValueMaterial *)socket->default_value)->value;
    if (material != nullptr) {
      build_material(material);
    }
  }
}

void DepsgraphRelationBuilder::build_nodetree(bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }
  if (built_map_.check_is_built_and_tag(ntree)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(ntree->id);

  build_idproperties(ntree->id.properties);
  build_idproperties(ntree->id.system_properties);
  build_animdata(&ntree->id);
  build_parameters(&ntree->id);
  OperationKey ntree_output_key(&ntree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
  OperationKey ntree_geo_preprocess_key(
      &ntree->id, NodeType::NTREE_GEOMETRY_PREPROCESS, OperationCode::NTREE_GEOMETRY_PREPROCESS);
  if (ntree->type == NTREE_GEOMETRY) {
    OperationKey ntree_cow_key(&ntree->id, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
    add_relation(ntree_cow_key, ntree_geo_preprocess_key, "Copy-on-Eval -> Preprocess");
    add_relation(ntree_geo_preprocess_key,
                 ntree_output_key,
                 "Preprocess -> Output",
                 RELATION_FLAG_NO_FLUSH);
  }
  /* nodetree's nodes... */
  for (bNode *bnode : ntree->all_nodes()) {
    build_idproperties(bnode->prop);
    LISTBASE_FOREACH (bNodeSocket *, socket, &bnode->inputs) {
      build_nodetree_socket(socket);
    }
    LISTBASE_FOREACH (bNodeSocket *, socket, &bnode->outputs) {
      build_nodetree_socket(socket);
    }

    if (ntree->type == NTREE_SHADER && bnode->is_type("ShaderNodeAttribute")) {
      NodeShaderAttribute *attr = static_cast<NodeShaderAttribute *>(bnode->storage);
      if (attr->type == SHD_ATTRIBUTE_VIEW_LAYER && STREQ(attr->name, "frame_current")) {
        TimeSourceKey time_src_key;
        add_relation(time_src_key, ntree_output_key, "TimeSrc -> Node");
      }
    }

    ID *id = bnode->id;
    if (id == nullptr) {
      continue;
    }
    ID_Type id_type = GS(id->name);
    if (id_type == ID_MA) {
      build_material((Material *)bnode->id);
      ComponentKey material_key(id, NodeType::SHADING);
      add_relation(material_key, ntree_output_key, "Material -> Node");
    }
    else if (id_type == ID_TE) {
      build_texture((Tex *)bnode->id);
      ComponentKey texture_key(id, NodeType::GENERIC_DATABLOCK);
      add_relation(texture_key, ntree_output_key, "Texture -> Node");
    }
    else if (id_type == ID_IM) {
      build_image((Image *)bnode->id);
      ComponentKey image_key(id, NodeType::GENERIC_DATABLOCK);
      add_relation(image_key, ntree_output_key, "Image -> Node");
    }
    else if (id_type == ID_OB) {
      build_object((Object *)id);
      ComponentKey object_transform_key(id, NodeType::TRANSFORM);
      add_relation(object_transform_key, ntree_output_key, "Object Transform -> Node");
      if (object_have_geometry_component(reinterpret_cast<Object *>(id))) {
        ComponentKey object_geometry_key(id, NodeType::GEOMETRY);
        add_relation(object_geometry_key, ntree_output_key, "Object Geometry -> Node");
      }
    }
    else if (id_type == ID_SCE) {
      Scene *node_scene = (Scene *)id;
      build_scene_parameters(node_scene);
      /* Camera is used by defocus node.
       *
       * On the one hand it's annoying to always pull it in, but on another hand it's also annoying
       * to have hardcoded node-type exception here. */
      if (node_scene->camera != nullptr) {
        build_object(node_scene->camera);
      }
    }
    else if (id_type == ID_TXT) {
      /* Ignore script nodes. */
    }
    else if (id_type == ID_MSK) {
      build_mask((Mask *)id);
      OperationKey mask_key(id, NodeType::PARAMETERS, OperationCode::MASK_EVAL);
      add_relation(mask_key, ntree_output_key, "Mask -> Node");
    }
    else if (id_type == ID_MC) {
      build_movieclip((MovieClip *)id);
      OperationKey clip_key(id, NodeType::PARAMETERS, OperationCode::MOVIECLIP_EVAL);
      add_relation(clip_key, ntree_output_key, "Clip -> Node");
    }
    else if (id_type == ID_VF) {
      build_vfont((VFont *)id);
      ComponentKey vfont_key(id, NodeType::GENERIC_DATABLOCK);
      add_relation(vfont_key, ntree_output_key, "VFont -> Node");
    }
    else if (id_type == ID_GR) {
      /* Build relations in the collection itself, but don't hook it up to the tree.
       * Relations from the collection to the tree are handled by the modifier's update_depsgraph()
       * callback.
       *
       * Other node trees do not currently support references to collections. Once they do this
       * code needs to be reconsidered. */
      build_collection(nullptr, reinterpret_cast<Collection *>(id));
    }
    else if (bnode->is_group()) {
      bNodeTree *group_ntree = (bNodeTree *)id;
      build_nodetree(group_ntree);
      ComponentKey group_output_key(&group_ntree->id, NodeType::NTREE_OUTPUT);
      /* This relation is not necessary in all cases (e.g. when the group node is not connected to
       * the output). Currently, we lack the infrastructure to check for these cases efficiently.
       * That can be added later. */
      add_relation(group_output_key, ntree_output_key, "Group Node");
      if (group_ntree->type == NTREE_GEOMETRY) {
        OperationKey group_preprocess_key(&group_ntree->id,
                                          NodeType::NTREE_GEOMETRY_PREPROCESS,
                                          OperationCode::NTREE_GEOMETRY_PREPROCESS);
        add_relation(group_preprocess_key, ntree_geo_preprocess_key, "Group Node Preprocess");
      }
    }
    else {
      /* Ignore this case. It can happen when the node type is not known currently. Either because
       * it belongs to an add-on or because it comes from a different Blender version that does
       * support the ID type here already. */
    }
  }

  ntree->ensure_interface_cache();
  for (bNodeTreeInterfaceSocket *socket : ntree->interface_inputs()) {
    build_idproperties(socket->properties);
  }
  for (bNodeTreeInterfaceSocket *socket : ntree->interface_outputs()) {
    build_idproperties(socket->properties);
  }

  if (check_id_has_anim_component(&ntree->id)) {
    ComponentKey animation_key(&ntree->id, NodeType::ANIMATION);
    add_relation(animation_key, ntree_output_key, "NTree Shading Parameters");
    if (ntree->type == NTREE_GEOMETRY) {
      add_relation(animation_key, ntree_geo_preprocess_key, "NTree Animation -> Preprocess");
    }
  }
}

/* Recursively build graph for material */
void DepsgraphRelationBuilder::build_material(Material *material, ID *owner)
{
  if (owner) {
    ComponentKey material_key(&material->id, NodeType::SHADING);
    OperationKey owner_shading_key(owner, NodeType::SHADING, OperationCode::SHADING);
    add_relation(material_key, owner_shading_key, "Material -> Owner Shading");
  }

  if (built_map_.check_is_built_and_tag(material)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(material->id);

  build_idproperties(material->id.properties);
  build_idproperties(material->id.system_properties);
  /* animation */
  build_animdata(&material->id);
  build_parameters(&material->id);

  /* Animated / driven parameters (without nodetree). */
  OperationKey material_key(&material->id, NodeType::SHADING, OperationCode::MATERIAL_UPDATE);
  ComponentKey parameters_key(&material->id, NodeType::PARAMETERS);
  add_relation(parameters_key, material_key, "Material's parameters");

  /* material's nodetree */
  if (material->nodetree != nullptr) {
    build_nodetree(material->nodetree);
    OperationKey ntree_key(
        &material->nodetree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
    add_relation(ntree_key, material_key, "Material's NTree");
    build_nested_nodetree(&material->id, material->nodetree);
  }
}

void DepsgraphRelationBuilder::build_materials(ID *owner, Material **materials, int num_materials)
{
  for (int i = 0; i < num_materials; i++) {
    if (materials[i] == nullptr) {
      continue;
    }
    build_material(materials[i], owner);
  }
}

/* Recursively build graph for texture */
void DepsgraphRelationBuilder::build_texture(Tex *texture)
{
  if (built_map_.check_is_built_and_tag(texture)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(texture->id);

  /* texture itself */
  ComponentKey texture_key(&texture->id, NodeType::GENERIC_DATABLOCK);
  build_idproperties(texture->id.properties);
  build_idproperties(texture->id.system_properties);
  build_animdata(&texture->id);
  build_parameters(&texture->id);

  /* texture's nodetree */
  if (texture->nodetree) {
    build_nodetree(texture->nodetree);
    OperationKey ntree_key(
        &texture->nodetree->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
    add_relation(ntree_key, texture_key, "Texture's NTree");
    build_nested_nodetree(&texture->id, texture->nodetree);
  }

  /* Special cases for different IDs which texture uses. */
  if (texture->type == TEX_IMAGE) {
    if (texture->ima != nullptr) {
      build_image(texture->ima);

      ComponentKey image_key(&texture->ima->id, NodeType::GENERIC_DATABLOCK);
      add_relation(image_key, texture_key, "Texture Image");
    }
  }

  if (check_id_has_anim_component(&texture->id)) {
    ComponentKey animation_key(&texture->id, NodeType::ANIMATION);
    add_relation(animation_key, texture_key, "Datablock Animation");
  }

  if (BKE_image_user_id_has_animation(&texture->id)) {
    ComponentKey image_animation_key(&texture->id, NodeType::IMAGE_ANIMATION);
    add_relation(image_animation_key, texture_key, "Datablock Image Animation");
  }
}

void DepsgraphRelationBuilder::build_image(Image *image)
{
  if (built_map_.check_is_built_and_tag(image)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(image->id);

  build_idproperties(image->id.properties);
  build_idproperties(image->id.system_properties);
  build_parameters(&image->id);
}

void DepsgraphRelationBuilder::build_cachefile(CacheFile *cache_file)
{
  if (built_map_.check_is_built_and_tag(cache_file)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(cache_file->id);

  build_idproperties(cache_file->id.properties);
  build_idproperties(cache_file->id.system_properties);
  /* Animation. */
  build_animdata(&cache_file->id);
  build_parameters(&cache_file->id);
  if (check_id_has_anim_component(&cache_file->id)) {
    ComponentKey animation_key(&cache_file->id, NodeType::ANIMATION);
    ComponentKey datablock_key(&cache_file->id, NodeType::CACHE);
    add_relation(animation_key, datablock_key, "Datablock Animation");
  }
  if (check_id_has_driver_component(&cache_file->id)) {
    ComponentKey animation_key(&cache_file->id, NodeType::PARAMETERS);
    ComponentKey datablock_key(&cache_file->id, NodeType::CACHE);
    add_relation(animation_key, datablock_key, "Drivers -> Cache Eval");
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
  if (built_map_.check_is_built_and_tag(mask)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(mask->id);

  ID *mask_id = &mask->id;
  build_idproperties(mask_id->properties);
  build_idproperties(mask_id->system_properties);
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
        if (parent == nullptr || parent->id == nullptr) {
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

void DepsgraphRelationBuilder::build_freestyle_linestyle(FreestyleLineStyle *linestyle)
{
  if (built_map_.check_is_built_and_tag(linestyle)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(linestyle->id);

  ID *linestyle_id = &linestyle->id;
  build_parameters(linestyle_id);
  build_idproperties(linestyle_id->properties);
  build_idproperties(linestyle_id->system_properties);
  build_animdata(linestyle_id);
  build_nodetree(linestyle->nodetree);
}

void DepsgraphRelationBuilder::build_movieclip(MovieClip *clip)
{
  if (built_map_.check_is_built_and_tag(clip)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(clip->id);

  /* Animation. */
  build_idproperties(clip->id.properties);
  build_idproperties(clip->id.system_properties);
  build_animdata(&clip->id);
  build_parameters(&clip->id);
}

void DepsgraphRelationBuilder::build_lightprobe(LightProbe *probe)
{
  if (built_map_.check_is_built_and_tag(probe)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(probe->id);

  build_idproperties(probe->id.properties);
  build_idproperties(probe->id.system_properties);
  build_animdata(&probe->id);
  build_parameters(&probe->id);
}

void DepsgraphRelationBuilder::build_speaker(Speaker *speaker)
{
  if (built_map_.check_is_built_and_tag(speaker)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(speaker->id);

  build_idproperties(speaker->id.properties);
  build_idproperties(speaker->id.system_properties);
  build_animdata(&speaker->id);
  build_parameters(&speaker->id);
  if (speaker->sound != nullptr) {
    build_sound(speaker->sound);
    ComponentKey speaker_key(&speaker->id, NodeType::AUDIO);
    ComponentKey sound_key(&speaker->sound->id, NodeType::AUDIO);
    add_relation(sound_key, speaker_key, "Sound -> Speaker");
  }
}

void DepsgraphRelationBuilder::build_sound(bSound *sound)
{
  if (built_map_.check_is_built_and_tag(sound)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(sound->id);

  build_idproperties(sound->id.properties);
  build_idproperties(sound->id.system_properties);
  build_animdata(&sound->id);
  build_parameters(&sound->id);

  const ComponentKey parameters_key(&sound->id, NodeType::PARAMETERS);
  const ComponentKey audio_key(&sound->id, NodeType::AUDIO);

  add_relation(parameters_key, audio_key, "Parameters -> Audio");
}

struct Seq_build_prop_cb_data {
  DepsgraphRelationBuilder *builder;
  ComponentKey sequencer_key;
  bool has_audio_strips;
};

static bool strip_build_prop_cb(Strip *strip, void *user_data)
{
  Seq_build_prop_cb_data *cd = (Seq_build_prop_cb_data *)user_data;

  cd->builder->build_idproperties(strip->prop);
  cd->builder->build_idproperties(strip->system_properties);
  if (strip->sound != nullptr) {
    cd->builder->build_sound(strip->sound);
    ComponentKey sound_key(&strip->sound->id, NodeType::AUDIO);
    cd->builder->add_relation(sound_key, cd->sequencer_key, "Sound -> Sequencer");
    cd->has_audio_strips = true;
  }
  if (strip->scene != nullptr) {
    cd->builder->build_scene_parameters(strip->scene);
    /* This is to support 3D audio. */
    cd->has_audio_strips = true;
  }
  if (strip->type == STRIP_TYPE_SCENE && strip->scene != nullptr) {
    if (strip->flag & SEQ_SCENE_STRIPS) {
      cd->builder->build_scene_sequencer(strip->scene);
      ComponentKey sequence_scene_audio_key(&strip->scene->id, NodeType::AUDIO);
      cd->builder->add_relation(
          sequence_scene_audio_key, cd->sequencer_key, "Sequence Scene Audio -> Sequencer");
      ComponentKey sequence_scene_key(&strip->scene->id, NodeType::SEQUENCER);
      cd->builder->add_relation(
          sequence_scene_key, cd->sequencer_key, "Sequence Scene -> Sequencer");
    }
    ViewLayer *sequence_view_layer = BKE_view_layer_default_render(strip->scene);
    cd->builder->build_scene_speakers(strip->scene, sequence_view_layer);
  }
  LISTBASE_FOREACH (StripModifierData *, modifier, &strip->modifiers) {
    if (modifier->type != eSeqModifierType_Compositor) {
      continue;
    }

    const SequencerCompositorModifierData *modifier_data =
        reinterpret_cast<SequencerCompositorModifierData *>(modifier);
    if (!modifier_data->node_group) {
      continue;
    }
    cd->builder->build_nodetree(modifier_data->node_group);
    OperationKey node_tree_key(
        &modifier_data->node_group->id, NodeType::NTREE_OUTPUT, OperationCode::NTREE_OUTPUT);
    cd->builder->add_relation(node_tree_key, cd->sequencer_key, "Modifier's Node Group");
  }
  /* TODO(sergey): Movie clip, camera, mask. */
  return true;
}

void DepsgraphRelationBuilder::build_scene_sequencer(Scene *scene)
{
  if (scene->ed == nullptr) {
    return;
  }
  if (built_map_.check_is_built_and_tag(scene, BuilderMap::TAG_SCENE_SEQUENCER)) {
    return;
  }

  /* TODO(sergey): Trace as a scene sequencer. */

  build_scene_audio(scene);
  ComponentKey scene_audio_key(&scene->id, NodeType::AUDIO);
  /* Make sure dependencies from sequences data goes to the sequencer evaluation. */
  ComponentKey sequencer_key(&scene->id, NodeType::SEQUENCER);

  Seq_build_prop_cb_data cb_data = {this, sequencer_key, false};

  seq::foreach_strip(&scene->ed->seqbase, strip_build_prop_cb, &cb_data);
  if (cb_data.has_audio_strips) {
    add_relation(sequencer_key, scene_audio_key, "Sequencer -> Audio");
  }
}

void DepsgraphRelationBuilder::build_scene_audio(Scene *scene)
{
  OperationKey scene_audio_entry_key(&scene->id, NodeType::AUDIO, OperationCode::AUDIO_ENTRY);
  OperationKey scene_audio_volume_key(&scene->id, NodeType::AUDIO, OperationCode::AUDIO_VOLUME);
  OperationKey scene_sound_eval_key(&scene->id, NodeType::AUDIO, OperationCode::SOUND_EVAL);
  add_relation(scene_audio_entry_key, scene_audio_volume_key, "Audio Entry -> Volume");
  add_relation(scene_audio_volume_key, scene_sound_eval_key, "Audio Volume -> Sound");

  if (scene->audio.flag & AUDIO_VOLUME_ANIMATED) {
    ComponentKey scene_anim_key(&scene->id, NodeType::ANIMATION);
    add_relation(scene_anim_key, scene_audio_volume_key, "Animation -> Audio Volume");
  }
}

void DepsgraphRelationBuilder::build_scene_speakers(Scene *scene, ViewLayer *view_layer)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    Object *object = base->object;
    if (object->type != OB_SPEAKER || !need_pull_base_into_graph(base)) {
      continue;
    }
    build_object(base->object);
  }
}

void DepsgraphRelationBuilder::build_vfont(VFont *vfont)
{
  if (built_map_.check_is_built_and_tag(vfont)) {
    return;
  }

  const BuilderStack::ScopedEntry stack_entry = stack_.trace(vfont->id);

  build_parameters(&vfont->id);
  build_idproperties(vfont->id.properties);
  build_idproperties(vfont->id.system_properties);
}

void DepsgraphRelationBuilder::build_copy_on_write_relations()
{
  for (IDNode *id_node : graph_->id_nodes) {
    build_copy_on_write_relations(id_node);
  }
}

/**
 * Nested datablocks (node trees, shape keys) requires special relation to
 * ensure owner's datablock remapping happens after node tree itself is ready.
 *
 * This is similar to what happens in ntree_hack_remap_pointers().
 */
void DepsgraphRelationBuilder::build_nested_datablock(ID *owner, ID *id, bool flush_cow_changes)
{
  int relation_flag = 0;
  if (!flush_cow_changes) {
    relation_flag |= RELATION_FLAG_NO_FLUSH;
  }
  OperationKey owner_copy_on_write_key(owner, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
  OperationKey id_copy_on_write_key(id, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
  add_relation(id_copy_on_write_key, owner_copy_on_write_key, "Eval Order", relation_flag);
}

void DepsgraphRelationBuilder::build_nested_nodetree(ID *owner, bNodeTree *ntree)
{
  if (ntree == nullptr) {
    return;
  }
  /* Don't flush cow changes, because the node tree may change in ways that do not affect the
   * owner data block (e.g. when a node is deleted that is not connected to any output).
   * Data blocks owning node trees should add a relation to the `NTREE_OUTPUT` node instead. */
  build_nested_datablock(owner, &ntree->id, false);
}

void DepsgraphRelationBuilder::build_nested_shapekey(ID *owner, Key *key)
{
  if (key == nullptr) {
    return;
  }
  build_nested_datablock(owner, &key->id, true);
}

void DepsgraphRelationBuilder::build_copy_on_write_relations(IDNode *id_node)
{
  ID *id_orig = id_node->id_orig;

  const ID_Type id_type = GS(id_orig->name);

  if (!deg_eval_copy_is_needed(id_type)) {
    return;
  }

  OperationKey copy_on_write_key(id_orig, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
  /* XXX: This is a quick hack to make Alt-A to work. */
  // add_relation(time_source_key, copy_on_write_key, "Fluxgate capacitor hack");
  /* Resat of code is using rather low level trickery, so need to get some
   * explicit pointers. */
  Node *node_cow = find_node(copy_on_write_key);
  OperationNode *op_cow = node_cow->get_exit_operation();
  /* Plug any other components to this one. */
  for (ComponentNode *comp_node : id_node->components.values()) {
    if (comp_node->type == NodeType::COPY_ON_EVAL) {
      /* Copy-on-eval component never depends on itself. */
      continue;
    }
    if (!comp_node->depends_on_cow()) {
      /* Component explicitly requests to not add relation. */
      continue;
    }
    int rel_flag = (RELATION_FLAG_NO_FLUSH | RELATION_FLAG_GODMODE);
    if ((ELEM(id_type, ID_ME, ID_CV, ID_PT, ID_VO) && comp_node->type == NodeType::GEOMETRY) ||
        (id_type == ID_CF && comp_node->type == NodeType::CACHE))
    {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* TODO(sergey): Needs better solution for this. */
    if (id_type == ID_SO) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* Notes on exceptions:
     * - View layers have cached array of bases in them, which is not
     *   copied by copy-on-evaluation, and not preserved. PROBABLY it is better
     *   to preserve that cache in copy-on-evaluation, but for the time being
     *   we allow flush to layer collections component which will ensure
     *   that cached array of bases exists and is up-to-date. */
    if (ELEM(comp_node->type, NodeType::LAYER_COLLECTIONS)) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* Mask evaluation operation is part of parameters, and it needs to be re-evaluated when the
     * mask is tagged for copy-on-eval.
     *
     * TODO(@sergey): This needs to be moved out of here.
     * In order to do so, moving mask evaluation out of parameters would be helpful and
     * semantically correct. */
    if (comp_node->type == NodeType::PARAMETERS && id_type == ID_MSK) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* Compatibility with the legacy tagging: groups are only tagged for Copy-on-Write when their
     * hierarchy changes, and it needs to be flushed downstream. */
    if (id_type == ID_GR && comp_node->type == NodeType::HIERARCHY) {
      rel_flag &= ~RELATION_FLAG_NO_FLUSH;
    }
    /* All entry operations of each component should wait for a proper
     * copy of ID. */
    OperationNode *op_entry = comp_node->get_entry_operation();
    if (op_entry != nullptr) {
      Relation *rel = graph_->add_new_relation(op_cow, op_entry, "Copy-on-Eval Dependency");
      rel->flag |= rel_flag;
    }
    /* All dangling operations should also be executed after copy-on-evaluation. */
    for (OperationNode *op_node : comp_node->operations_map->values()) {
      if (op_node == op_entry) {
        continue;
      }
      if (op_node->inlinks.is_empty()) {
        Relation *rel = graph_->add_new_relation(op_cow, op_node, "Copy-on-Eval Dependency");
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
          Relation *rel = graph_->add_new_relation(op_cow, op_node, "Copy-on-Eval Dependency");
          rel->flag |= rel_flag;
        }
      }
    }
    /* NOTE: We currently ignore implicit relations to an external
     * data-blocks for copy-on-evaluation operations. This means, for example,
     * copy-on-evaluation component of Object will not wait for copy-on-evaluation
     * component of its Mesh. This is because pointers are all known
     * already so remapping will happen all correct. And then If some object
     * evaluation step needs geometry, it will have transitive dependency
     * to Mesh copy-on-evaluation already. */
  }
  /* TODO(sergey): This solves crash for now, but causes too many
   * updates potentially. */
  if (GS(id_orig->name) == ID_OB) {
    Object *object = (Object *)id_orig;
    ID *object_data_id = (ID *)object->data;
    if (object_data_id != nullptr) {
      if (deg_eval_copy_is_needed(object_data_id)) {
        OperationKey data_copy_on_write_key(
            object_data_id, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
        add_relation(
            data_copy_on_write_key, copy_on_write_key, "Eval Order", RELATION_FLAG_GODMODE);
      }
    }
    else {
      BLI_assert(object->type == OB_EMPTY);
    }
  }

#if 0
  /* NOTE: Relation is disabled since #AnimationBackup() is disabled.
   * See comment in #AnimationBackup:init_from_id(). */

  /* Copy-on-eval of write will iterate over f-curves to store current values corresponding
   * to their RNA path. This means that action must be copied prior to the ID's copy-on-evaluation,
   * otherwise depsgraph might try to access freed data. */
  AnimData *animation_data = BKE_animdata_from_id(id_orig);
  if (animation_data != nullptr) {
    if (animation_data->action != nullptr) {
      OperationKey action_copy_on_write_key(
          &animation_data->action->id, NodeType::COPY_ON_EVAL, OperationCode::COPY_ON_EVAL);
      add_relation(action_copy_on_write_key,
                   copy_on_write_key,
                   "Eval Order",
                   RELATION_FLAG_GODMODE | RELATION_FLAG_NO_FLUSH);
    }
  }
#endif
}

/* **** ID traversal callbacks functions **** */

void DepsgraphRelationBuilder::modifier_walk(void *user_data,
                                             Object * /*object*/,
                                             ID **idpoin,
                                             LibraryForeachIDCallbackFlag /*cb_flag*/)
{
  BuilderWalkUserData *data = (BuilderWalkUserData *)user_data;
  ID *id = *idpoin;
  if (id == nullptr) {
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
  if (id == nullptr) {
    return;
  }
  data->builder->build_id(id);
}

}  // namespace blender::deg
