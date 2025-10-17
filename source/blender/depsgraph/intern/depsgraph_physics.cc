/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#include "intern/depsgraph_physics.hh"

#include "BLI_enum_flags.hh"
#include "BLI_listbase.h"

#include "BKE_collision.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_modifier.hh"
#include "BKE_object.hh"

#include "DNA_collection_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"

#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_physics.hh"
#include "DEG_depsgraph_query.hh"

#include "depsgraph.hh"

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
  ID *collection_orig = DEG_get_original(object_id_safe(collection));
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
  ID *collection_orig = DEG_get_original(object_id_safe(collection));
  return hash->lookup_default(collection_orig, nullptr);
}

/********************** Depsgraph Building API ************************/

/**
 * Flags to store point-cache relations which have been calculated.
 * This avoid adding relations multiple times.
 *
 * \note This could be replaced by bit-shifting #eDepsObjectComponentType values,
 * although this would limit them to integer size.
 */
enum class CollisionComponentFlag : uint8_t {
  None = 0,
  /** #DEG_OB_COMP_TRANSFORM is set. */
  Transform = 1 << 0,
  /** #DEG_OB_COMP_GEOMETRY is set. */
  Geometry = 1 << 1,
  /** #DEG_OB_COMP_EVAL_POSE is set. */
  EvalPose = 1 << 2,
};
ENUM_OPERATORS(CollisionComponentFlag);

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

  /* Expand tag objects, matching: #BKE_object_modifier_update_subframe behavior. */

  /* NOTE: #eModifierType_Fluid should be included,
   * leave out for the purpose of validating the fix for dynamic paint only. */
  const bool use_recursive_parents = (modifier_type == eModifierType_DynamicPaint);

  blender::Map<Object *, CollisionComponentFlag> *object_component_map = nullptr;
  if (use_recursive_parents) {
    object_component_map = MEM_new<blender::Map<Object *, CollisionComponentFlag>>(__func__);
  }

  LISTBASE_FOREACH (CollisionRelation *, relation, relations) {
    Object *ob1 = relation->ob;
    if (ob1 == object) {
      continue;
    }
    if (filter_function &&
        !filter_function(ob1, BKE_modifiers_findby_type(ob1, (ModifierType)modifier_type)))
    {
      continue;
    }

    if (use_recursive_parents) {
      /* Add relations for `ob1` and other objects it references,
       * using `object_component_map` to avoid redundant calls.
       *
       * When #BKE_object_modifier_update_subframe is used by a modifier,
       * it's important the depsgraph tags objects this modifier uses.
       *
       * Without this, access to objects is not thread-safe, see: #142137.
       *
       * NOTE(@ideasman42): #BKE_object_modifier_update_subframe calls
       * #BKE_animsys_evaluate_animdata, depending on the object type.
       * Equivalent relations could be added here.
       * This was not done and there are no bug reports relating to this,
       * so leave as-is unless the current code is failing in a real world scenario. */

      BKE_object_modifier_update_subframe_only_callback(
          ob1,
          true,
          OBJECT_MODIFIER_UPDATE_SUBFRAME_RECURSION_DEFAULT,
          modifier_type,
          [&handle, &name, &object_component_map](Object *ob, const bool update_mesh) {
            CollisionComponentFlag &update_flag = object_component_map->lookup_or_add_default(ob);
            {
              constexpr CollisionComponentFlag test_flag = CollisionComponentFlag::Transform;
              if (!flag_is_set(update_flag, test_flag)) {
                update_flag |= test_flag;
                DEG_add_object_pointcache_relation(handle, ob, DEG_OB_COMP_TRANSFORM, name);
              }
            }
            if (update_mesh) {
              constexpr CollisionComponentFlag test_flag = CollisionComponentFlag::Geometry;
              if (!flag_is_set(update_flag, test_flag)) {
                update_flag |= test_flag;
                DEG_add_object_pointcache_relation(handle, ob, DEG_OB_COMP_GEOMETRY, name);
              }
            }
            if (ob->type == OB_ARMATURE) {
              constexpr CollisionComponentFlag test_flag = CollisionComponentFlag::EvalPose;
              if (!flag_is_set(update_flag, test_flag)) {
                update_flag |= test_flag;
                DEG_add_object_pointcache_relation(handle, ob, DEG_OB_COMP_EVAL_POSE, name);
              }
            }
          });

      continue;
    }

    DEG_add_object_pointcache_relation(handle, ob1, DEG_OB_COMP_TRANSFORM, name);
    DEG_add_object_pointcache_relation(handle, ob1, DEG_OB_COMP_GEOMETRY, name);
  }

  if (use_recursive_parents) {
    MEM_delete(object_component_map);
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
