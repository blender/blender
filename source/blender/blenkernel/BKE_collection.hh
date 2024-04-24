/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "BLI_sys_types.h"

#include "DNA_listBase.h"

/* Structs */

struct BLI_Iterator;
struct Base;
struct BlendDataReader;
struct BlendWriter;
struct Collection;
struct ID;
struct CollectionExport;
struct Main;
struct Object;
struct Scene;
struct ViewLayer;

struct CollectionParent {
  struct CollectionParent *next, *prev;
  struct Collection *collection;
};

/* Collections */

/**
 * Add a collection to a collection ListBase and synchronize all render layers
 * The ListBase is NULL when the collection is to be added to the master collection
 */
Collection *BKE_collection_add(Main *bmain, Collection *parent, const char *name);
/**
 * Add \a collection_dst to all scene collections that reference object \a ob_src is in.
 * Used to replace an instance object with a collection (library override operator).
 *
 * Logic is very similar to #BKE_collection_object_add_from().
 */
void BKE_collection_add_from_object(Main *bmain,
                                    Scene *scene,
                                    const Object *ob_src,
                                    Collection *collection_dst);
/**
 * Add \a collection_dst to all scene collections that reference collection \a collection_src is
 * in.
 *
 * Logic is very similar to #BKE_collection_object_add_from().
 */
void BKE_collection_add_from_collection(Main *bmain,
                                        Scene *scene,
                                        Collection *collection_src,
                                        Collection *collection_dst);
/**
 * Free (or release) any data used by this collection (does not free the collection itself).
 */
void BKE_collection_free_data(Collection *collection);

/**
 * Free any data used by the IO handler (does not free the IO handler itself).
 */
void BKE_collection_exporter_free_data(CollectionExport *data);

/**
 * Remove a collection, optionally removing its child objects or moving
 * them to parent collections.
 */
bool BKE_collection_delete(Main *bmain, Collection *collection, bool hierarchy);

/**
 * Make a deep copy (aka duplicate) of the given collection and all of its children, recursively.
 *
 * \warning This functions will clear all \a bmain #ID.idnew pointers, unless \a
 * #LIB_ID_DUPLICATE_IS_SUBPROCESS duplicate option is passed on, in which case caller is
 * responsible to reconstruct collection dependencies information's
 * (i.e. call #BKE_main_collection_sync).
 */
Collection *BKE_collection_duplicate(Main *bmain,
                                     Collection *parent,
                                     Collection *collection,
                                     uint duplicate_flags,
                                     uint duplicate_options);

/* Master Collection for Scene */

#define BKE_SCENE_COLLECTION_NAME "Scene Collection"
Collection *BKE_collection_master_add(Scene *scene);

/* Collection Objects */

bool BKE_collection_has_object(Collection *collection, const Object *ob);
bool BKE_collection_has_object_recursive(Collection *collection, Object *ob);
bool BKE_collection_has_object_recursive_instanced(Collection *collection, Object *ob);
/**
 * Find whether an evaluated object's original ID is contained or instanced by any object in this
 * collection. The collection is expected to be an evaluated data-block too.
 */
bool BKE_collection_has_object_recursive_instanced_orig_id(Collection *collection_eval,
                                                           Object *object_eval);
Collection *BKE_collection_object_find(Main *bmain,
                                       Scene *scene,
                                       Collection *collection,
                                       Object *ob);
bool BKE_collection_is_empty(const Collection *collection);

/**
 * Add object to given collection, ensuring this collection is 'editable' (i.e. local and not a
 * liboverride), and finding a suitable parent one otherwise.
 */
bool BKE_collection_object_add(Main *bmain, Collection *collection, Object *ob);

/**
 * Add object to given collection, similar to #BKE_collection_object_add.
 *
 * However, it additionally ensures that the selected collection is also part of the given
 * `view_layer`, if non-NULL. Otherwise, the object is not added to any collection.
 */
bool BKE_collection_viewlayer_object_add(Main *bmain,
                                         const ViewLayer *view_layer,
                                         Collection *collection,
                                         Object *ob);

/**
 * Same as #BKE_collection_object_add, but unconditionally adds the object to the given collection.
 *
 * NOTE: required in certain cases, like do-versioning or complex ID management tasks.
 */
bool BKE_collection_object_add_notest(Main *bmain, Collection *collection, Object *ob);
/**
 * Add \a ob_dst to all scene collections that reference object \a ob_src is in.
 * Used for copying objects.
 *
 * Logic is very similar to #BKE_collection_add_from_object()
 */
void BKE_collection_object_add_from(Main *bmain, Scene *scene, Object *ob_src, Object *ob_dst);
/**
 * Remove object from collection.
 */
bool BKE_collection_object_remove(Main *bmain,
                                  Collection *collection,
                                  Object *object,
                                  bool free_us);
/**
 * Replace one object with another in a collection (managing user counts).
 */
bool BKE_collection_object_replace(Main *bmain,
                                   Collection *collection,
                                   Object *ob_old,
                                   Object *ob_new);

/**
 * Move object from a collection into another
 *
 * If source collection is NULL move it from all the existing collections.
 */
void BKE_collection_object_move(
    Main *bmain, Scene *scene, Collection *collection_dst, Collection *collection_src, Object *ob);

/**
 * Remove object from all collections of scene
 */
bool BKE_scene_collections_object_remove(Main *bmain, Scene *scene, Object *object, bool free_us);

/**
 * Check all collections in \a bmain (including embedded ones in scenes) for invalid
 * CollectionObject (either with NULL object pointer, or duplicates), and remove them.
 *
 * \note In case of duplicates, the first CollectionObject in the list is kept, all others are
 * removed.
 */
void BKE_collections_object_remove_invalids(Main *bmain);

/**
 * Remove all NULL children from parent collections of changed \a collection.
 * This is used for library remapping, where these pointers have been set to NULL.
 * Otherwise this should never happen.
 *
 * \note caller must ensure #BKE_main_collection_sync_remap() is called afterwards!
 *
 * \param parent_collection: The collection owning the pointers that were remapped. May be \a NULL,
 * in which case whole \a bmain database of collections is checked.
 * \param child_collection: The collection that was remapped to another pointer. May be \a NULL,
 * in which case whole \a bmain database of collections is checked.
 */
void BKE_collections_child_remove_nulls(Main *bmain,
                                        Collection *parent_collection,
                                        Collection *child_collection);

/* Dependencies. */

bool BKE_collection_is_in_scene(Collection *collection);
void BKE_collections_after_lib_link(Main *bmain);
bool BKE_collection_object_cyclic_check(Main *bmain, Object *object, Collection *collection);

/* Object list cache. */

ListBase BKE_collection_object_cache_get(Collection *collection);
ListBase BKE_collection_object_cache_instanced_get(Collection *collection);
/** Free the object cache of given `collection` and all of its ancestors (recursively).
 *
 * \param bmain: The Main database owning the collection. May be `nullptr`, only used if doing
 * depsgraph tagging.
 * \param id_create_flag: Flags controlling ID creation, used here to enable or
 * not depsgraph tagging of affected IDs (e.g. #LIB_ID_CREATE_NO_DEG_TAG would prevent depsgraph
 * tagging). */
void BKE_collection_object_cache_free(const Main *bmain,
                                      Collection *collection,
                                      const int id_create_flag);
/**
 * Free the object cache of all collections in given `bmain`, including master collections of
 * scenes.
 */
void BKE_main_collections_object_cache_free(const Main *bmain);

Base *BKE_collection_or_layer_objects(const Scene *scene,
                                      ViewLayer *view_layer,
                                      Collection *collection);

/* Editing. */

/**
 * Return Scene Collection for a given index.
 *
 * The index is calculated from top to bottom counting the children before the siblings.
 */
Collection *BKE_collection_from_index(Scene *scene, int index);
/**
 * The automatic/fallback name of a new collection.
 */
void BKE_collection_new_name_get(Collection *collection_parent, char *rname);
/**
 * The name to show in the interface.
 */
const char *BKE_collection_ui_name_get(Collection *collection);
/**
 * Select all the objects in this Collection (and its nested collections) for this ViewLayer.
 * Return true if any object was selected.
 */
bool BKE_collection_objects_select(const Scene *scene,
                                   ViewLayer *view_layer,
                                   Collection *collection,
                                   bool deselect);

/* Collection children */

bool BKE_collection_child_add(Main *bmain, Collection *parent, Collection *child);

bool BKE_collection_child_add_no_sync(Main *bmain, Collection *parent, Collection *child);

bool BKE_collection_child_remove(Main *bmain, Collection *parent, Collection *child);

bool BKE_collection_move(Main *bmain,
                         Collection *to_parent,
                         Collection *from_parent,
                         Collection *relative,
                         bool relative_after,
                         Collection *collection);

/**
 * Find potential cycles in collections.
 *
 * \param new_ancestor: the potential new owner of given \a collection,
 * or the collection to check if the later is NULL.
 * \param collection: the collection we want to add to \a new_ancestor,
 * may be NULL if we just want to ensure \a new_ancestor does not already have cycles.
 * \return true if a cycle is found.
 */
bool BKE_collection_cycle_find(Collection *new_ancestor, Collection *collection);
/**
 * Find and fix potential cycles in collections.
 *
 * \param collection: The collection to check for existing cycles.
 * \return true if cycles are found and fixed.
 */
bool BKE_collection_cycles_fix(Main *bmain, Collection *collection);

bool BKE_collection_has_collection(const Collection *parent, const Collection *collection);

/**
 * Return parent collection which is not linked.
 */
Collection *BKE_collection_parent_editable_find_recursive(const ViewLayer *view_layer,
                                                          Collection *collection);
/**
 * Rebuild parent relationships from child ones, for all children of given \a collection.
 *
 * \note Given collection is assumed to already have valid parents.
 */
void BKE_collection_parent_relations_rebuild(Collection *collection);
/**
 * Rebuild parent relationships from child ones, for all collections in given \a bmain.
 */
void BKE_main_collections_parent_relations_rebuild(Main *bmain);

/**
 * Perform some validation on integrity of the data of this collection.
 *
 * \return `true` if everything is OK, false if some errors are detected. */
bool BKE_collection_validate(Collection *collection);

/* .blend file I/O */

/**
 * Perform some pre-writing cleanup on the COllection data itself (_not_ in any sub-data
 * referenced by pointers). To be called before writing the Collection struct itself.
 */
void BKE_collection_blend_write_prepare_nolib(BlendWriter *writer, Collection *collection);
void BKE_collection_blend_write_nolib(BlendWriter *writer, Collection *collection);
void BKE_collection_blend_read_data(BlendDataReader *reader, Collection *collection, ID *owner_id);

/* Iteration callbacks. */

using BKE_scene_objects_Cb = void (*)(Object *ob, void *data);
using BKE_scene_collections_Cb = void (*)(Collection *ob, void *data);

/* Iteration over objects in collection. */

#define FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN(_collection, _object, _mode) \
  { \
    int _base_flag = (_mode == DAG_EVAL_VIEWPORT) ? BASE_ENABLED_VIEWPORT : BASE_ENABLED_RENDER; \
    int _object_visibility_flag = (_mode == DAG_EVAL_VIEWPORT) ? OB_HIDE_VIEWPORT : \
                                                                 OB_HIDE_RENDER; \
    int _base_id = 0; \
    for (Base *_base = static_cast<Base *>(BKE_collection_object_cache_get(_collection).first); \
         _base; \
         _base = _base->next, _base_id++) \
    { \
      Object *_object = _base->object; \
      if ((_base->flag & _base_flag) && \
          (_object->visibility_flag & _object_visibility_flag) == 0) {

#define FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_END \
  } \
  } \
  } \
  ((void)0)

#define FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN(_collection, _object) \
  for (Base *_base = static_cast<Base *>(BKE_collection_object_cache_get(_collection).first); \
       _base; \
       _base = _base->next) \
  { \
    Object *_object = _base->object; \
    BLI_assert(_object != NULL);

#define FOREACH_COLLECTION_OBJECT_RECURSIVE_END \
  } \
  ((void)0)

/* Iteration over collections in scene. */

/**
 * Only use this in non-performance critical situations
 * (it iterates over all scene collections twice)
 */
void BKE_scene_collections_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_scene_collections_iterator_next(BLI_Iterator *iter);
void BKE_scene_collections_iterator_end(BLI_Iterator *iter);

void BKE_scene_objects_iterator_begin(BLI_Iterator *iter, void *data_in);
void BKE_scene_objects_iterator_next(BLI_Iterator *iter);
void BKE_scene_objects_iterator_end(BLI_Iterator *iter);

/**
 * Iterate over objects in the scene based on a flag.
 *
 * \note The object->flag is tested against flag.
 */
struct SceneObjectsIteratorExData {
  Scene *scene;
  int flag;
  void *iter_data;
};

void BKE_scene_objects_iterator_begin_ex(BLI_Iterator *iter, void *data_in);
void BKE_scene_objects_iterator_next_ex(BLI_Iterator *iter);
void BKE_scene_objects_iterator_end_ex(BLI_Iterator *iter);

/**
 * Generate a new #GSet (or extend given `objects_gset` if not NULL) with all objects referenced by
 * all collections of given `scene`.
 *
 * \note This will include objects without a base currently
 * (because they would belong to excluded collections only e.g.).
 */
GSet *BKE_scene_objects_as_gset(Scene *scene, GSet *objects_gset);

#define FOREACH_SCENE_COLLECTION_BEGIN(scene, _instance) \
  ITER_BEGIN (BKE_scene_collections_iterator_begin, \
              BKE_scene_collections_iterator_next, \
              BKE_scene_collections_iterator_end, \
              scene, \
              Collection *, \
              _instance)

#define FOREACH_SCENE_COLLECTION_END ITER_END

#define FOREACH_COLLECTION_BEGIN(_bmain, _scene, Type, _instance) \
  { \
    Type _instance; \
    Collection *_instance_next; \
    bool is_scene_collection = (_scene) != NULL; \
\
    if (_scene) { \
      _instance_next = _scene->master_collection; \
    } \
    else { \
      _instance_next = static_cast<Collection *>((_bmain)->collections.first); \
    } \
\
    while ((_instance = _instance_next)) { \
      if (is_scene_collection) { \
        _instance_next = static_cast<Collection *>((_bmain)->collections.first); \
        is_scene_collection = false; \
      } \
      else { \
        _instance_next = static_cast<Collection *>(_instance->id.next); \
      }

#define FOREACH_COLLECTION_END \
  } \
  } \
  ((void)0)

#define FOREACH_SCENE_OBJECT_BEGIN(scene, _instance) \
  ITER_BEGIN (BKE_scene_objects_iterator_begin, \
              BKE_scene_objects_iterator_next, \
              BKE_scene_objects_iterator_end, \
              scene, \
              Object *, \
              _instance)

#define FOREACH_SCENE_OBJECT_END ITER_END
