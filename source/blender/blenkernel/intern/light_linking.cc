/* SPDX-FileCopyrightText: 2001-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_light_linking.h"

#include <string>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_collection.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

void BKE_light_linking_ensure(Object *object)
{
  if (object->light_linking == nullptr) {
    object->light_linking = MEM_callocN<LightLinking>(__func__);
  }
}

void BKE_light_linking_copy(Object *object_dst, const Object *object_src, const int copy_flags)
{
  BLI_assert(ELEM(object_dst->light_linking, nullptr, object_src->light_linking));
  if (object_src->light_linking) {
    object_dst->light_linking = MEM_dupallocN<LightLinking>(__func__,
                                                            *(object_src->light_linking));
    if ((copy_flags & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus(blender::id_cast<ID *>(object_dst->light_linking->receiver_collection));
      id_us_plus(blender::id_cast<ID *>(object_dst->light_linking->blocker_collection));
    }
  }
}

void BKE_light_linking_delete(Object *object, const int delete_flags)
{
  if (object->light_linking) {
    if ((delete_flags & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_min(blender::id_cast<ID *>(object->light_linking->receiver_collection));
      id_us_min(blender::id_cast<ID *>(object->light_linking->blocker_collection));
    }
    MEM_SAFE_FREE(object->light_linking);
  }
}

void BKE_light_linking_free_if_empty(Object *object)
{
  if (object->light_linking->receiver_collection == nullptr &&
      object->light_linking->blocker_collection == nullptr)
  {
    BKE_light_linking_delete(object, LIB_ID_CREATE_NO_USER_REFCOUNT);
  }
}

Collection *BKE_light_linking_collection_get(const Object *object,
                                             const LightLinkingType link_type)
{
  if (!object->light_linking) {
    return nullptr;
  }

  switch (link_type) {
    case LIGHT_LINKING_RECEIVER:
      return object->light_linking->receiver_collection;
    case LIGHT_LINKING_BLOCKER:
      return object->light_linking->blocker_collection;
  }

  return nullptr;
}

static std::string get_default_collection_name(const Object *object,
                                               const LightLinkingType link_type)
{
  const char *format;

  switch (link_type) {
    case LIGHT_LINKING_RECEIVER:
      format = DATA_("Light Linking for %s");
      break;
    case LIGHT_LINKING_BLOCKER:
      format = DATA_("Shadow Linking for %s");
      break;
  }

  char name[MAX_ID_NAME - 2];
  SNPRINTF_UTF8(name, format, object->id.name + 2);

  return name;
}

Collection *BKE_light_linking_collection_new(Main *bmain,
                                             Object *object,
                                             const LightLinkingType link_type)
{
  const std::string collection_name = get_default_collection_name(object, link_type);

  Collection *new_collection = BKE_collection_add(bmain, nullptr, collection_name.c_str());

  BKE_light_linking_collection_assign(bmain, object, new_collection, link_type);

  return new_collection;
}

void BKE_light_linking_collection_assign_only(Object *object,
                                              Collection *new_collection,
                                              const LightLinkingType link_type)
{
  /* Remove user from old collection. */
  Collection *old_collection = BKE_light_linking_collection_get(object, link_type);
  if (old_collection) {
    id_us_min(&old_collection->id);
  }

  /* Allocate light linking on demand. */
  if (new_collection) {
    BKE_light_linking_ensure(object);
  }

  if (object->light_linking) {
    /* Assign and increment user of new collection. */
    switch (link_type) {
      case LIGHT_LINKING_RECEIVER:
        object->light_linking->receiver_collection = new_collection;
        break;
      case LIGHT_LINKING_BLOCKER:
        object->light_linking->blocker_collection = new_collection;
        break;
      default:
        BLI_assert_unreachable();
        break;
    }

    if (new_collection) {
      id_us_plus(&new_collection->id);
    }

    BKE_light_linking_free_if_empty(object);
  }
}

void BKE_light_linking_collection_assign(Main *bmain,
                                         Object *object,
                                         Collection *new_collection,
                                         const LightLinkingType link_type)
{
  BKE_light_linking_collection_assign_only(object, new_collection, link_type);

  DEG_id_tag_update(&object->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SHADING);
  DEG_relations_tag_update(bmain);
}

static CollectionObject *find_collection_object(const Collection *collection, const Object *object)
{
  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
    if (collection_object->ob == object) {
      return collection_object;
    }
  }

  return nullptr;
}

static CollectionChild *find_collection_child(const Collection *collection,
                                              const Collection *child)
{
  LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
    if (collection_child->collection == child) {
      return collection_child;
    }
  }

  return nullptr;
}

/* Add object to the light linking collection and return corresponding CollectionLightLinking
 * settings.
 *
 * If the object is already in the collection then the content of the collection is not modified,
 * and the existing light linking settings are returned. */
static CollectionLightLinking *light_linking_collection_add_object(Main *bmain,
                                                                   Collection *collection,
                                                                   Object *object)
{
  BKE_collection_object_add(bmain, collection, object);

  CollectionObject *collection_object = find_collection_object(collection, object);

  if (!collection_object) {
    BLI_assert_msg(0, "Object was not found after added to the light linking collection");
    return nullptr;
  }

  return &collection_object->light_linking;
}

/* Add child collection to the light linking collection and return corresponding
 * CollectionLightLinking settings.
 *
 * If the child collection is already in the collection then the content of the collection is
 * not modified, and the existing light linking settings are returned. */
static CollectionLightLinking *light_linking_collection_add_collection(Main *bmain,
                                                                       Collection *collection,
                                                                       Collection *child)
{
  BKE_collection_child_add(bmain, collection, child);

  CollectionChild *collection_child = find_collection_child(collection, child);

  if (!collection_child) {
    BLI_assert_msg(0, "Collection was not found after added to the light linking collection");
    return nullptr;
  }

  return &collection_child->light_linking;
}

void BKE_light_linking_add_receiver_to_collection(Main *bmain,
                                                  Collection *collection,
                                                  ID *receiver,
                                                  const eCollectionLightLinkingState link_state)
{
  const ID_Type id_type = GS(receiver->name);

  CollectionLightLinking *collection_light_linking = nullptr;

  if (id_type == ID_OB) {
    Object *object = reinterpret_cast<Object *>(receiver);

    if (object->type == OB_EMPTY && object->instance_collection) {
      if (!BKE_collection_contains_geometry_recursive(object->instance_collection)) {
        return;
      }
    }
    else if (!OB_TYPE_IS_GEOMETRY(object->type)) {
      return;
    }
    collection_light_linking = light_linking_collection_add_object(bmain, collection, object);
  }
  else if (id_type == ID_GR) {
    collection_light_linking = light_linking_collection_add_collection(
        bmain, collection, reinterpret_cast<Collection *>(receiver));
  }
  else {
    return;
  }

  if (!collection_light_linking) {
    return;
  }

  collection_light_linking->link_state = link_state;

  DEG_id_tag_update(&collection->id, ID_RECALC_HIERARCHY);
  DEG_id_tag_update(receiver, ID_RECALC_SHADING);

  DEG_relations_tag_update(bmain);
}

static void order_collection_receiver_before(Collection *collection,
                                             Collection *receiver,
                                             const ID *before)
{
  CollectionChild *receiver_collection_child = find_collection_child(collection, receiver);
  if (!receiver_collection_child) {
    BLI_assert_msg(0, "Receiver child was not found after adding collection to light linking");
    return;
  }

  const ID_Type before_id_type = GS(before->name);

  if (before_id_type != ID_GR) {
    /* Adding before object: move the collection to the very bottom.
     * This is as far to the bottom as the receiver can be in the flattened list of the collection.
     */
    BLI_remlink(&collection->children, receiver_collection_child);
    BLI_addtail(&collection->children, receiver_collection_child);
    return;
  }

  CollectionChild *before_collection_child = find_collection_child(
      collection, reinterpret_cast<const Collection *>(before));
  if (!before_collection_child) {
    BLI_assert_msg(0, "Before child was not found");
    return;
  }

  BLI_remlink(&collection->children, receiver_collection_child);
  BLI_insertlinkbefore(&collection->children, before_collection_child, receiver_collection_child);
}

static void order_collection_receiver_after(Collection *collection,
                                            Collection *receiver,
                                            const ID *after)
{
  CollectionChild *receiver_collection_child = find_collection_child(collection, receiver);
  if (!receiver_collection_child) {
    BLI_assert_msg(0, "Receiver child was not found after adding collection to light linking");
    return;
  }

  const ID_Type after_id_type = GS(after->name);

  if (after_id_type != ID_GR) {
    /* Adding before object: move the collection to the very bottom.
     * This is as far to the bottom as the receiver can be in the flattened list of the collection.
     */
    BLI_remlink(&collection->children, receiver_collection_child);
    BLI_addtail(&collection->children, receiver_collection_child);
    return;
  }

  CollectionChild *after_collection_child = find_collection_child(
      collection, reinterpret_cast<const Collection *>(after));
  if (!after_collection_child) {
    BLI_assert_msg(0, "After child was not found");
    return;
  }

  BLI_remlink(&collection->children, receiver_collection_child);
  BLI_insertlinkafter(&collection->children, after_collection_child, receiver_collection_child);
}

static void order_object_receiver_before(Collection *collection,
                                         Object *receiver,
                                         const ID *before)
{
  CollectionObject *receiver_collection_object = find_collection_object(collection, receiver);
  if (!receiver_collection_object) {
    BLI_assert_msg(
        0, "Receiver collection object was not found after adding collection to light linking");
    return;
  }

  const ID_Type before_id_type = GS(before->name);

  if (before_id_type != ID_OB) {
    /* Adding before collection: move the receiver to the very beginning of the child objects list.
     * This is as close to the top of the flattened list of the collection content the object can
     * possibly be. */
    BLI_remlink(&collection->gobject, receiver_collection_object);
    BLI_addhead(&collection->gobject, receiver_collection_object);
    return;
  }

  CollectionObject *before_collection_object = find_collection_object(
      collection, reinterpret_cast<const Object *>(before));
  if (!before_collection_object) {
    BLI_assert_msg(0, "Before collection object was not found");
    return;
  }

  BLI_remlink(&collection->gobject, receiver_collection_object);
  BLI_insertlinkbefore(&collection->gobject, before_collection_object, receiver_collection_object);
}

static void order_object_receiver_after(Collection *collection, Object *receiver, const ID *after)
{
  CollectionObject *receiver_collection_object = find_collection_object(collection, receiver);
  if (!receiver_collection_object) {
    BLI_assert_msg(
        0, "Receiver collection object was not found after adding collection to light linking");
    return;
  }

  const ID_Type after_id_type = GS(after->name);

  if (after_id_type != ID_OB) {
    /* Adding after collection: move the receiver to the very beginning of the child objects list.
     * This is as close to the top of the flattened list of the collection content the object can
     * possibly be. */
    BLI_remlink(&collection->gobject, receiver_collection_object);
    BLI_addhead(&collection->gobject, receiver_collection_object);
    return;
  }

  CollectionObject *after_collection_object = find_collection_object(
      collection, reinterpret_cast<const Object *>(after));
  if (!after_collection_object) {
    BLI_assert_msg(0, "After collection object was not found");
    return;
  }

  BLI_remlink(&collection->gobject, receiver_collection_object);
  BLI_insertlinkafter(&collection->gobject, after_collection_object, receiver_collection_object);
}

void BKE_light_linking_add_receiver_to_collection_before(
    Main *bmain,
    Collection *collection,
    ID *receiver,
    const ID *before,
    const eCollectionLightLinkingState link_state)
{
  BLI_assert(before);

  BKE_light_linking_add_receiver_to_collection(bmain, collection, receiver, link_state);

  if (!before) {
    return;
  }

  const ID_Type id_type = GS(receiver->name);
  if (id_type == ID_OB) {
    order_object_receiver_before(collection, reinterpret_cast<Object *>(receiver), before);
  }
  else if (id_type == ID_GR) {
    order_collection_receiver_before(collection, reinterpret_cast<Collection *>(receiver), before);
  }
}

void BKE_light_linking_add_receiver_to_collection_after(
    Main *bmain,
    Collection *collection,
    ID *receiver,
    const ID *after,
    const eCollectionLightLinkingState link_state)
{
  BLI_assert(after);

  BKE_light_linking_add_receiver_to_collection(bmain, collection, receiver, link_state);

  if (!after) {
    return;
  }

  const ID_Type id_type = GS(receiver->name);
  if (id_type == ID_OB) {
    order_object_receiver_after(collection, reinterpret_cast<Object *>(receiver), after);
  }
  else if (id_type == ID_GR) {
    order_collection_receiver_after(collection, reinterpret_cast<Collection *>(receiver), after);
  }
}

bool BKE_light_linking_unlink_id_from_collection(Main *bmain,
                                                 Collection *collection,
                                                 ID *id,
                                                 ReportList *reports)
{
  const ID_Type id_type = GS(id->name);

  if (id_type == ID_OB) {
    BKE_collection_object_remove(bmain, collection, reinterpret_cast<Object *>(id), false);
  }
  else if (id_type == ID_GR) {
    BKE_collection_child_remove(bmain, collection, reinterpret_cast<Collection *>(id));
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot unlink unsupported '%s' from light linking collection '%s'",
                id->name + 2,
                collection->id.name + 2);
    return false;
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_HIERARCHY);
  if (id_type == ID_OB) {
    DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  }

  DEG_relations_tag_update(bmain);

  return true;
}

void BKE_light_linking_link_receiver_to_emitter(Main *bmain,
                                                Object *emitter,
                                                Object *receiver,
                                                const LightLinkingType link_type,
                                                const eCollectionLightLinkingState link_state)
{
  if (receiver->type == OB_EMPTY && receiver->instance_collection) {
    if (!BKE_collection_contains_geometry_recursive(receiver->instance_collection)) {
      return;
    }
  }
  else if (!OB_TYPE_IS_GEOMETRY(receiver->type)) {
    return;
  }

  Collection *collection = BKE_light_linking_collection_get(emitter, link_type);

  if (!collection) {
    collection = BKE_light_linking_collection_new(bmain, emitter, link_type);
  }

  BKE_light_linking_add_receiver_to_collection(bmain, collection, &receiver->id, link_state);
}

void BKE_light_linking_select_receivers_of_emitter(Scene *scene,
                                                   ViewLayer *view_layer,
                                                   Object *emitter,
                                                   const LightLinkingType link_type)
{
  Collection *collection = BKE_light_linking_collection_get(emitter, link_type);
  if (!collection) {
    return;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);

  /* Deselect all currently selected objects in the view layer, but keep the emitter selected.
   * This is because the operation is called from the emitter being active, and it will be
   * confusing to deselect it but keep active. */
  LISTBASE_FOREACH (Base *, base, BKE_view_layer_object_bases_get(view_layer)) {
    if (base->object == emitter) {
      continue;
    }
    base->flag &= ~BASE_SELECTED;
  }

  /* Select objects which are reachable via the receiver collection hierarchy. */
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Base *base = BKE_view_layer_base_find(view_layer, cob->ob);
    if (!base) {
      continue;
    }

    /* TODO(sergey): Check whether the object is configured to receive light. */

    base->flag |= BASE_SELECTED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
}
