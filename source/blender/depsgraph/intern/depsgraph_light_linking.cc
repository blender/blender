/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Light linking utilities. */

#include "intern/depsgraph_light_linking.hh"

#include "MEM_guardedalloc.h"

#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_light_linking.hh"
#include "DEG_depsgraph_query.hh"

#include "intern/depsgraph.hh"

namespace deg = blender::deg;

/* -------------------------------------------------------------------- */
/** \name Public C++ API
 * \{ */

namespace blender::deg::light_linking {

void eval_runtime_data(const ::Depsgraph *depsgraph, Object &object_eval)
{
  const deg::Depsgraph *deg_graph = reinterpret_cast<const deg::Depsgraph *>(depsgraph);
  deg_graph->light_linking_cache.eval_runtime_data(object_eval);
}

}  // namespace blender::deg::light_linking

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal builder API
 * \{ */

namespace {

/* Check whether the ID is suitable to be an input of the dependency graph. */
/* TODO(sergey): Move the function and check to a more generic place. */
#ifndef NDEBUG
bool is_valid_input_id(const ID &id)
{
  return (id.tag & ID_TAG_LOCALIZED) || DEG_is_original(&id);
}
#endif

}  // namespace

namespace blender::deg::light_linking {

using LightSet = internal::LightSet;
using EmitterData = internal::EmitterData;
using EmitterDataMap = internal::EmitterDataMap;
using EmitterSetMembership = internal::EmitterSetMembership;
using LinkingData = internal::LinkingData;

namespace internal {

namespace {

/* Helper class which takes care of allocating an unique light set IDs, performing checks for
 * overflows. */
class LightSetIDManager {
  using LightSet = internal::LightSet;

 public:
  explicit LightSetIDManager(const Scene &scene) : scene_(scene) {}

  bool get(const LightSet &light_set, uint64_t &id)
  {
    /* Performance note.
     *
     * Always ensure the light set data exists in the map, even when an overflow happens. This has
     * a downside of potentially higher memory usage and when there are many emitters with light
     * linking, but it avoids distinct lookup + add fore the normal cases. */

    const uint64_t light_set_id = light_set_id_map_.lookup_or_add_cb(light_set, [&]() {
      const uint64_t new_light_set_id = next_light_set_id_++;

      if (new_light_set_id == LightSet::MAX_ID + 1) {
        printf("Maximum number of light linking sets (%d) exceeded scene \"%s\".\n",
               LightSet::MAX_ID + 1,
               scene_.id.name + 2);
      }

      return new_light_set_id;
    });

    id = light_set_id;

    return id <= LightSet::MAX_ID;
  }

 private:
  const Scene &scene_;

  /* Next unique ID of a light set. */
  uint64_t next_light_set_id_ = LightSet::DEFAULT_ID + 1;

  /* Map from a link set to its original id. */
  Map<internal::LightSet, uint64_t> light_set_id_map_;
};

}  // namespace

/* LightSet */

bool LightSet::operator==(const LightSet &other) const
{
  return include_collection_mask == other.include_collection_mask &&
         exclude_collection_mask == other.exclude_collection_mask;
}

uint64_t LightSet::hash() const
{
  return get_default_hash(get_default_hash(include_collection_mask),
                          get_default_hash(exclude_collection_mask));
}

/* EmitterSetMembership */

uint64_t EmitterSetMembership::get_mask() const
{
  const uint64_t effective_included_mask = included_sets_mask ? included_sets_mask :
                                                                SET_MEMBERSHIP_ALL;
  return effective_included_mask & ~excluded_sets_mask;
}

/* EmitterDataMap. */

void EmitterDataMap::clear()
{
  emitter_data_map_.clear();
  next_collection_id_ = 0;
}

EmitterData *EmitterDataMap::ensure_data_if_possible(const Scene &scene, const Object &emitter)
{
  BLI_assert(is_valid_input_id(emitter.id));

  const Collection *collection = get_collection(emitter);
  BLI_assert(collection);

  /* Performance note.
   *
   * Always ensure the emitter data exists in the map, even when an overflow happens. This has a
   * downside of potentially higher memory usage when there are many emitters with light linking,
   * but it avoids distinct lookup + add fore the normal cases.
   *
   * On the API level the function always returns nullptr on overflow, so it is more of an internal
   * behavior. */

  EmitterData &emitter_data = emitter_data_map_.lookup_or_add_cb(collection, [&]() {
    const uint64_t collection_id = next_collection_id_++;

    EmitterData new_emitter_data;

    if (collection_id > EmitterData::MAX_COLLECTION_ID) {
      if (collection_id == EmitterData::MAX_COLLECTION_ID + 1) {
        printf("Maximum number of light linking collections (%d) exceeded in scene \"%s\".\n",
               EmitterData::MAX_COLLECTION_ID + 1,
               scene.id.name + 2);
      }
      new_emitter_data.collection_mask = 0;
    }
    else {
      new_emitter_data.collection_mask = uint64_t(1) << collection_id;
    }

    return new_emitter_data;
  });

  if (!emitter_data.collection_mask) {
    return nullptr;
  }

  return &emitter_data;
}

const EmitterData *EmitterDataMap::get_data(const Object &emitter) const
{
  const Collection *collection_eval = get_collection(emitter);

  if (!collection_eval) {
    return nullptr;
  }

  const Collection *collection_orig = DEG_get_original(collection_eval);

  return emitter_data_map_.lookup_ptr(collection_orig);
}

bool EmitterDataMap::can_skip_emitter(const Object &emitter) const
{
  BLI_assert(is_valid_input_id(emitter.id));

  const Collection *collection = get_collection(emitter);

  if (!collection) {
    return true;
  }

  return emitter_data_map_.contains(collection);
}

/* LinkingData */

void LinkingData::clear()
{
  light_linked_sets_.clear();
  object_light_sets_.clear();
}

void LinkingData::link_object(const EmitterData &emitter_data,
                              const eCollectionLightLinkingState link_state,
                              const Object &object)
{
  LightSet &light_set = ensure_light_set_for(object);

  switch (link_state) {
    case COLLECTION_LIGHT_LINKING_STATE_INCLUDE:
      light_set.include_collection_mask |= emitter_data.collection_mask;
      light_set.exclude_collection_mask &= ~emitter_data.collection_mask;
      break;

    case COLLECTION_LIGHT_LINKING_STATE_EXCLUDE:
      light_set.exclude_collection_mask |= emitter_data.collection_mask;
      light_set.include_collection_mask &= ~emitter_data.collection_mask;
      break;
  }
}

LightSet &LinkingData::ensure_light_set_for(const Object &object)
{
  BLI_assert(is_valid_input_id(object.id));

  return light_linked_sets_.lookup_or_add_as(&object);
}

void LinkingData::clear_after_build()
{
  light_linked_sets_.clear();
}

void LinkingData::end_build(const Scene &scene, EmitterDataMap &emitter_data_map)
{
  LightSetIDManager light_set_id_manager(scene);

  for (const auto it : light_linked_sets_.items()) {
    const Object *receiver = it.key;
    LightSet &light_set = it.value;

    uint64_t light_set_id;
    if (!light_set_id_manager.get(light_set, light_set_id)) {
      continue;
    }

    const uint64_t light_set_mask = uint64_t(1) << light_set_id;

    object_light_sets_.add(receiver, light_set_id);

    update_emitters_membership(emitter_data_map, light_set, light_set_mask);
  }

  clear_after_build();
}

void LinkingData::update_emitters_membership(EmitterDataMap &emitter_data_map,
                                             const LightSet &light_set,
                                             const uint64_t light_set_mask)
{
  for (EmitterData &emitter_data : emitter_data_map.values()) {
    EmitterSetMembership &set_membership = get_emitter_set_membership(emitter_data);

    if (emitter_data.collection_mask & light_set.include_collection_mask) {
      set_membership.included_sets_mask |= light_set_mask;
    }
    if (emitter_data.collection_mask & light_set.exclude_collection_mask) {
      set_membership.excluded_sets_mask |= light_set_mask;
    }
  }
}

uint64_t LinkingData::get_light_set_for(const Object &object) const
{
  const Object *object_orig = DEG_get_original(&object);
  return object_light_sets_.lookup_default(object_orig, LightSet::DEFAULT_ID);
}

}  // namespace internal

namespace {

/* Iterate over all objects of the collection and invoke the given callback with two arguments:
 * the given collection light linking settings, and the object (passed as reference).
 *
 * Note that if an object is reachable from multiple children collection the callback is invoked
 * for all of them. */
template<class Proc>
void foreach_light_collection_object_inner(const CollectionLightLinking &collection_light_linking,
                                           const Collection &collection,
                                           Proc &&callback)
{
  LISTBASE_FOREACH (const CollectionChild *, collection_child, &collection.children) {
    foreach_light_collection_object_inner(
        collection_light_linking, *collection_child->collection, callback);
  }

  LISTBASE_FOREACH (const CollectionObject *, collection_object, &collection.gobject) {
    callback(collection_light_linking, *collection_object->ob);
  }
}

/* Iterate over all objects of the collection and invoke the given callback with two arguments:
 * CollectionLightLinking and the actual Object (passed as reference).
 *
 * The CollectionLightLinking denotes the effective light linking settings of the object. It comes
 * from the first level of hierarchy from the given collection.
 *
 * Note that if an object is reachable from multiple children collection the callback is invoked
 * for all of them. */
template<class Proc>
void foreach_light_collection_object(const Collection &collection, Proc &&callback)
{
  LISTBASE_FOREACH (const CollectionChild *, collection_child, &collection.children) {
    foreach_light_collection_object_inner(
        collection_child->light_linking, *collection_child->collection, callback);
  }

  LISTBASE_FOREACH (const CollectionObject *, collection_object, &collection.gobject) {
    callback(collection_object->light_linking, *collection_object->ob);
  }
}

}  // namespace

void Cache::clear()
{
  light_emitter_data_map_.clear();
  shadow_emitter_data_map_.clear();

  light_linking_.clear();
  shadow_linking_.clear();
}

void Cache::add_emitter(const Scene &scene, const Object &emitter)
{
  BLI_assert(is_valid_input_id(emitter.id));

  add_light_linking_emitter(scene, emitter);
  add_shadow_linking_emitter(scene, emitter);
}

void Cache::add_light_linking_emitter(const Scene &scene, const Object &emitter)
{
  BLI_assert(is_valid_input_id(emitter.id));

  if (light_emitter_data_map_.can_skip_emitter(emitter)) {
    return;
  }

  const EmitterData *light_emitter_data = light_emitter_data_map_.ensure_data_if_possible(scene,
                                                                                          emitter);
  if (light_emitter_data) {
    foreach_light_collection_object(
        *emitter.light_linking->receiver_collection,
        [&](const CollectionLightLinking &collection_light_linking, const Object &receiver) {
          add_receiver_object(*light_emitter_data, collection_light_linking, receiver);
        });
  }
}

void Cache::add_shadow_linking_emitter(const Scene &scene, const Object &emitter)
{
  BLI_assert(is_valid_input_id(emitter.id));

  if (shadow_emitter_data_map_.can_skip_emitter(emitter)) {
    return;
  }

  const EmitterData *shadow_emitter_data = shadow_emitter_data_map_.ensure_data_if_possible(
      scene, emitter);
  if (shadow_emitter_data) {
    foreach_light_collection_object(
        *emitter.light_linking->blocker_collection,
        [&](const CollectionLightLinking &collection_light_linking, const Object &receiver) {
          add_blocker_object(*shadow_emitter_data, collection_light_linking, receiver);
        });
  }
}

void Cache::add_receiver_object(const EmitterData &emitter_data,
                                const CollectionLightLinking &collection_light_linking,
                                const Object &receiver)
{
  BLI_assert(is_valid_input_id(receiver.id));

  light_linking_.link_object(
      emitter_data, eCollectionLightLinkingState(collection_light_linking.link_state), receiver);
}

void Cache::add_blocker_object(const EmitterData &emitter_data,
                               const CollectionLightLinking &collection_light_linking,
                               const Object &blocker)
{
  BLI_assert(is_valid_input_id(blocker.id));

  shadow_linking_.link_object(
      emitter_data, eCollectionLightLinkingState(collection_light_linking.link_state), blocker);
}

void Cache::end_build(const Scene &scene)
{
  if (!has_light_linking()) {
    return;
  }

  light_linking_.end_build(scene, light_emitter_data_map_);
  shadow_linking_.end_build(scene, shadow_emitter_data_map_);
}

void Cache::eval_runtime_data(Object &object_eval) const
{
  static const LightLinkingRuntime runtime_no_links = {
      EmitterSetMembership::SET_MEMBERSHIP_ALL, EmitterSetMembership::SET_MEMBERSHIP_ALL, 0, 0};

  if (!has_light_linking()) {
    /* No light linking used in the scene, still reset to default on objects that have
     * allocated light linking data structures since we can't free them here. */
    if (object_eval.light_linking) {
      object_eval.light_linking->runtime = runtime_no_links;
    }

    return;
  }

  /* Receiver and blocker configuration. */
  LightLinkingRuntime runtime = {};
  runtime.receiver_light_set = light_linking_.get_light_set_for(object_eval);
  runtime.blocker_shadow_set = shadow_linking_.get_light_set_for(object_eval);

  /* Emitter configuration. */
  const EmitterData *light_emitter_data = light_emitter_data_map_.get_data(object_eval);
  if (light_emitter_data) {
    runtime.light_set_membership = light_emitter_data->light_membership.get_mask();
  }
  else {
    runtime.light_set_membership = EmitterSetMembership::SET_MEMBERSHIP_ALL;
  }

  const EmitterData *shadow_emitter_data = shadow_emitter_data_map_.get_data(object_eval);
  if (shadow_emitter_data) {
    runtime.shadow_set_membership = shadow_emitter_data->shadow_membership.get_mask();
  }
  else {
    runtime.shadow_set_membership = EmitterSetMembership::SET_MEMBERSHIP_ALL;
  }

  const bool need_runtime = (memcmp(&runtime, &runtime_no_links, sizeof(runtime)) != 0);

  /* Assign, allocating light linking on demand if needed. */
  if (object_eval.light_linking) {
    object_eval.light_linking->runtime = runtime;
    if (!need_runtime) {
      /* Note that this will only remove lazily allocated light_linking on the evaluated object,
       * as an empty light_linking is not allowed on the original object. */
      BKE_light_linking_free_if_empty(&object_eval);
    }
  }
  else if (need_runtime) {
    BKE_light_linking_ensure(&object_eval);
    object_eval.light_linking->runtime = runtime;
  }
}

}  // namespace blender::deg::light_linking

/** \} */
