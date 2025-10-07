/* SPDX-FileCopyrightText: 2001-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage light linking.
 */

#include "DNA_collection_types.h" /* eCollectionLightLinkingState */

struct ID;
struct Main;
struct Object;
struct Collection;
struct ReportList;
struct Scene;
struct ViewLayer;

typedef enum LightLinkingType {
  LIGHT_LINKING_RECEIVER,
  LIGHT_LINKING_BLOCKER,
} LightLinkingType;

/**
 * Add an empty LightLinking data to an Object.
 */
void BKE_light_linking_ensure(struct Object *object);

/**
 * Copy the LightLinking data from `object_src` to `object_dst`.
 *
 * \param copy_flags: Flags controlling the copy process, see e.g. #LIB_ID_CREATE_NO_USER_REFCOUNT
 * and related flags in the same enum.
 */
void BKE_light_linking_copy(struct Object *object_dst,
                            const struct Object *object_src,
                            const int copy_flags);

/**
 * Free the LightLinking data from the object.
 *
 * \param copy_flags: Flags controlling the copy process, see e.g. #LIB_ID_CREATE_NO_USER_REFCOUNT
 * and related flags in the same enum.
 */
void BKE_light_linking_delete(struct Object *object, const int delete_flags);

/**
 * Free object's light_linking if it is not needed to hold any of collections.
 */
void BKE_light_linking_free_if_empty(struct Object *object);

/**
 * Get a collection of the given light linking type of the given object.
 */
struct Collection *BKE_light_linking_collection_get(const struct Object *object,
                                                    LightLinkingType link_type);

/**
 * Create new collection and assign it as a light or shadow linking collection (denoted by the
 * link_type) of the given object.
 *
 * The collection is created outside of the view layer collections.
 * If the object already has light linking collection set up it is unreferenced from the object.
 *
 * \return the newly created collection.
 */
struct Collection *BKE_light_linking_collection_new(struct Main *bmain,
                                                    struct Object *object,
                                                    LightLinkingType link_type);

/**
 * Assign given light or shadow linking collection (denoted by the link_type) to the given object.
 * Maintains user counters of the collection: old collection is decreased the user counter, the new
 * one is increased after this call.
 * The new_collection is allowed to be null pointer.
 *
 * The assign_only variant takes care of (re)assigning the collection and maintaining the user
 * counter, but not the dependency graph tagging for update.
 */
void BKE_light_linking_collection_assign_only(struct Object *object,
                                              struct Collection *new_collection,
                                              LightLinkingType link_type);
void BKE_light_linking_collection_assign(struct Main *bmain,
                                         struct Object *object,
                                         struct Collection *new_collection,
                                         LightLinkingType link_type);

/**
 * Add receiver to the given light linking collection.
 * The ID is expected to either be collection or an object.
 * Passing other types of IDs has no effect.
 */
void BKE_light_linking_add_receiver_to_collection(struct Main *bmain,
                                                  struct Collection *collection,
                                                  struct ID *receiver,
                                                  const eCollectionLightLinkingState link_state);
void BKE_light_linking_add_receiver_to_collection_before(
    struct Main *bmain,
    struct Collection *collection,
    struct ID *receiver,
    const struct ID *before,
    const eCollectionLightLinkingState link_state);
void BKE_light_linking_add_receiver_to_collection_after(
    struct Main *bmain,
    struct Collection *collection,
    struct ID *receiver,
    const struct ID *after,
    const eCollectionLightLinkingState link_state);

/**
 * Remove the given ID from the light or shadow linking collection of the given object.
 *
 * The collection is expected to be either receiver_collection or blocker_collection from an
 * emitter object.
 *
 * The ID is expected to either be collection or an object. If other ID type is passed to the
 * function an error is reported and false is returned.
 *
 * \return true if the ID was unlinked from the receiver collection, false otherwise. The unlinking
 * will be unsuccessful if, for example, the receiver collection is a linked data-block.
 *
 * The optional reports argument is used to provide human-readable details about why unlinking was
 * not successful. If it is nullptr then the report is printed to the console.
 */
bool BKE_light_linking_unlink_id_from_collection(struct Main *bmain,
                                                 struct Collection *collection,
                                                 struct ID *id,
                                                 struct ReportList *reports);

/**
 * Link receiver object to the given emitter.
 *
 * If the emitter already has light linking collection specified the object is added to that
 * collection. Otherwise, first a new collection is created and assigned, and the receiver is added
 * to it.
 */
void BKE_light_linking_link_receiver_to_emitter(struct Main *bmain,
                                                struct Object *emitter,
                                                struct Object *receiver,
                                                LightLinkingType link_type,
                                                eCollectionLightLinkingState link_state);

/**
 * Select all objects which are linked to the given emitter via the given light link type.
 */
void BKE_light_linking_select_receivers_of_emitter(struct Scene *scene,
                                                   struct ViewLayer *view_layer,
                                                   struct Object *emitter,
                                                   LightLinkingType link_type);
