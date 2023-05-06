/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2018 Blender Foundation */

/** \file
 * \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#include "intern/depsgraph_physics.h"

#include "MEM_guardedalloc.h"

#include "BLI_compiler_compat.h"
#include "BLI_listbase.h"

#include "BKE_collision.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"

#include "DNA_collection_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "depsgraph.h"

namespace deg = blender::deg;

/*************************** Evaluation Query API *****************************/

static ePhysicsRelationType modifier_to_relation_type(uint modifier_type)
{
  switch (modifier_type) {
    case eModifierType_Collision:
      return DEG_PHYSICS_COLLISION;
    case eModifierType_Fluid:
      return DEG_PHYSICS_SMOKE_COLLISION;
    case eModifierType_DynamicPaint:
      return DEG_PHYSICS_DYNAMIC_BRUSH;
  }

  BLI_assert_msg(0, "Unknown collision modifier type");
  return DEG_PHYSICS_RELATIONS_NUM;
}
/* Get ID from an ID type object, in a safe manner. This means that object can be nullptr,
 * in which case the function returns nullptr.
 */
template<class T> static ID *object_id_safe(T *object)
{
  if (object == nullptr) {
    return nullptr;
  }
  return &object->id;
}

ListBase *DEG_get_effector_relations(const Depsgraph *graph, Collection *collection)
{
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(graph);
  blender::Map<const ID *, ListBase *> *hash = deg_graph->physics_relations[DEG_PHYSICS_EFFECTOR];
  if (hash == nullptr) {
    return nullptr;
  }
  /* NOTE: nullptr is a valid lookup key here as it means that the relation is not bound to a
   * specific collection. */
  ID *collection_orig = DEG_get_original_id(object_id_safe(collection));
  return hash->lookup_default(collection_orig, nullptr);
}

ListBase *DEG_get_collision_relations(const Depsgraph *graph,
                                      Collection *collection,
                                      uint modifier_type)
{
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(graph);
  const ePhysicsRelationType type = modifier_to_relation_type(modifier_type);
  blender::Map<const ID *, ListBase *> *hash = deg_graph->physics_relations[type];
  if (hash == nullptr) {
    return nullptr;
  }
  /* NOTE: nullptr is a valid lookup key here as it means that the relation is not bound to a
   * specific collection. */
  ID *collection_orig = DEG_get_original_id(object_id_safe(collection));
  return hash->lookup_default(collection_orig, nullptr);
}

/********************** Depsgraph Building API ************************/

void DEG_add_collision_relations(DepsNodeHandle *handle,
                                 Object *object,
                                 Collection *collection,
                                 uint modifier_type,
                                 DEG_CollobjFilterFunction filter_function,
                                 const char *name)
{
  Depsgraph *depsgraph = DEG_get_graph_from_handle(handle);
  deg::Depsgraph *deg_graph = (deg::Depsgraph *)depsgraph;
  ListBase *relations = build_collision_relations(deg_graph, collection, modifier_type);
  LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
    Object *ob1 = relation->ob;
    if (ob1 == object) {
      continue;
    }
    if (filter_function == nullptr ||
        filter_function(ob1, BKE_modifiers_findby_type(ob1, (ModifierType)modifier_type)))
    {
      DEG_add_object_pointcache_relation(handle, ob1, DEG_OB_COMP_TRANSFORM, name);
      DEG_add_object_pointcache_relation(handle, ob1, DEG_OB_COMP_GEOMETRY, name);
    }
  }
}

void DEG_add_forcefield_relations(DepsNodeHandle *handle,
                                  Object *object,
                                  EffectorWeights *effector_weights,
                                  bool add_absorption,
                                  int skip_forcefield,
                                  const char *name)
{
  Depsgraph *depsgraph = DEG_get_graph_from_handle(handle);
  deg::Depsgraph *deg_graph = (deg::Depsgraph *)depsgraph;
  ListBase *relations = build_effector_relations(deg_graph, effector_weights->group);
  LISTBASE_FOREACH (EffectorRelation *, relation, relations) {
    if (relation->ob == object) {
      continue;
    }
    if (relation->pd->forcefield == skip_forcefield) {
      continue;
    }

    /* Relation to forcefield object, optionally including geometry.
     * Use special point cache relations for automatic cache clearing. */
    DEG_add_object_pointcache_relation(handle, relation->ob, DEG_OB_COMP_TRANSFORM, name);

    if (relation->psys || ELEM(relation->pd->shape, PFIELD_SHAPE_SURFACE, PFIELD_SHAPE_POINTS) ||
        relation->pd->forcefield == PFIELD_GUIDE)
    {
      /* TODO(sergey): Consider going more granular with more dedicated
       * particle system operation. */
      DEG_add_object_pointcache_relation(handle, relation->ob, DEG_OB_COMP_GEOMETRY, name);
    }

    /* Smoke flow relations. */
    if (relation->pd->forcefield == PFIELD_FLUIDFLOW && relation->pd->f_source != nullptr) {
      DEG_add_object_pointcache_relation(
          handle, relation->pd->f_source, DEG_OB_COMP_TRANSFORM, "Fluid Force Domain");
      DEG_add_object_pointcache_relation(
          handle, relation->pd->f_source, DEG_OB_COMP_GEOMETRY, "Fluid Force Domain");
    }

    /* Absorption forces need collision relation. */
    if (add_absorption && (relation->pd->flag & PFIELD_VISIBILITY)) {
      DEG_add_collision_relations(
          handle, object, nullptr, eModifierType_Collision, nullptr, "Force Absorption");
    }
  }
}

/******************************** Internal API ********************************/

namespace blender::deg {

ListBase *build_effector_relations(Depsgraph *graph, Collection *collection)
{
  Map<const ID *, ListBase *> *hash = graph->physics_relations[DEG_PHYSICS_EFFECTOR];
  if (hash == nullptr) {
    graph->physics_relations[DEG_PHYSICS_EFFECTOR] = new Map<const ID *, ListBase *>();
    hash = graph->physics_relations[DEG_PHYSICS_EFFECTOR];
  }
  /* If collection is nullptr still use it as a key.
   * In this case the BKE_effector_relations_create() will create relates for all bases in the
   * view layer.
   */
  ID *collection_id = object_id_safe(collection);
  return hash->lookup_or_add_cb(collection_id, [&]() {
    ::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph *>(graph);
    return BKE_effector_relations_create(depsgraph, graph->scene, graph->view_layer, collection);
  });
}

ListBase *build_collision_relations(Depsgraph *graph, Collection *collection, uint modifier_type)
{
  const ePhysicsRelationType type = modifier_to_relation_type(modifier_type);
  Map<const ID *, ListBase *> *hash = graph->physics_relations[type];
  if (hash == nullptr) {
    graph->physics_relations[type] = new Map<const ID *, ListBase *>();
    hash = graph->physics_relations[type];
  }
  /* If collection is nullptr still use it as a key.
   * In this case the BKE_collision_relations_create() will create relates for all bases in the
   * view layer.
   */
  ID *collection_id = object_id_safe(collection);
  return hash->lookup_or_add_cb(collection_id, [&]() {
    ::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph *>(graph);
    return BKE_collision_relations_create(depsgraph, collection, modifier_type);
  });
}

void clear_physics_relations(Depsgraph *graph)
{
  for (int i = 0; i < DEG_PHYSICS_RELATIONS_NUM; i++) {
    Map<const ID *, ListBase *> *hash = graph->physics_relations[i];
    if (hash) {
      const ePhysicsRelationType type = (ePhysicsRelationType)i;

      switch (type) {
        case DEG_PHYSICS_EFFECTOR:
          for (ListBase *list : hash->values()) {
            BKE_effector_relations_free(list);
          }
          break;
        case DEG_PHYSICS_COLLISION:
        case DEG_PHYSICS_SMOKE_COLLISION:
        case DEG_PHYSICS_DYNAMIC_BRUSH:
          for (ListBase *list : hash->values()) {
            BKE_collision_relations_free(list);
          }
          break;
        case DEG_PHYSICS_RELATIONS_NUM:
          break;
      }
      delete hash;
      graph->physics_relations[i] = nullptr;
    }
  }
}

}  // namespace blender::deg
