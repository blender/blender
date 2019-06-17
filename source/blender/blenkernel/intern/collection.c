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
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_iterator.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_threads.h"
#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

/******************************** Prototypes ********************************/

static bool collection_child_add(Collection *parent,
                                 Collection *collection,
                                 const int flag,
                                 const bool add_us);
static bool collection_child_remove(Collection *parent, Collection *collection);
static bool collection_object_add(
    Main *bmain, Collection *collection, Object *ob, int flag, const bool add_us);
static bool collection_object_remove(Main *bmain,
                                     Collection *collection,
                                     Object *ob,
                                     const bool free_us);

static CollectionChild *collection_find_child(Collection *parent, Collection *collection);
static CollectionParent *collection_find_parent(Collection *child, Collection *collection);

static bool collection_find_child_recursive(Collection *parent, Collection *collection);

/***************************** Add Collection *******************************/

/* Add new collection, without view layer syncing. */
static Collection *collection_add(Main *bmain,
                                  Collection *collection_parent,
                                  const char *name_custom)
{
  /* Determine new collection name. */
  char name[MAX_NAME];

  if (name_custom) {
    STRNCPY(name, name_custom);
  }
  else {
    BKE_collection_new_name_get(collection_parent, name);
  }

  /* Create new collection. */
  Collection *collection = BKE_libblock_alloc(bmain, ID_GR, name, 0);

  /* We increase collection user count when linking to Collections. */
  id_us_min(&collection->id);

  /* Optionally add to parent collection. */
  if (collection_parent) {
    collection_child_add(collection_parent, collection, 0, true);
  }

  return collection;
}

/**
 * Add a collection to a collection ListBase and synchronize all render layers
 * The ListBase is NULL when the collection is to be added to the master collection
 */
Collection *BKE_collection_add(Main *bmain, Collection *collection_parent, const char *name_custom)
{
  Collection *collection = collection_add(bmain, collection_parent, name_custom);
  BKE_main_collection_sync(bmain);
  return collection;
}

/*********************** Free and Delete Collection ****************************/

/** Free (or release) any data used by this collection (does not free the collection itself). */
void BKE_collection_free(Collection *collection)
{
  /* No animdata here. */
  BKE_previewimg_free(&collection->preview);

  BLI_freelistN(&collection->gobject);
  BLI_freelistN(&collection->children);
  BLI_freelistN(&collection->parents);

  BKE_collection_object_cache_free(collection);
}

/**
 * Remove a collection, optionally removing its child objects or moving
 * them to parent collections.
 */
bool BKE_collection_delete(Main *bmain, Collection *collection, bool hierarchy)
{
  /* Master collection is not real datablock, can't be removed. */
  if (collection->flag & COLLECTION_IS_MASTER) {
    BLI_assert(!"Scene master collection can't be deleted");
    return false;
  }

  if (hierarchy) {
    /* Remove child objects. */
    CollectionObject *cob = collection->gobject.first;
    while (cob != NULL) {
      collection_object_remove(bmain, collection, cob->ob, true);
      cob = collection->gobject.first;
    }

    /* Delete all child collections recursively. */
    CollectionChild *child = collection->children.first;
    while (child != NULL) {
      BKE_collection_delete(bmain, child->collection, hierarchy);
      child = collection->children.first;
    }
  }
  else {
    /* Link child collections into parent collection. */
    for (CollectionChild *child = collection->children.first; child; child = child->next) {
      for (CollectionParent *cparent = collection->parents.first; cparent;
           cparent = cparent->next) {
        Collection *parent = cparent->collection;
        collection_child_add(parent, child->collection, 0, true);
      }
    }

    CollectionObject *cob = collection->gobject.first;
    while (cob != NULL) {
      /* Link child object into parent collections. */
      for (CollectionParent *cparent = collection->parents.first; cparent;
           cparent = cparent->next) {
        Collection *parent = cparent->collection;
        collection_object_add(bmain, parent, cob->ob, 0, true);
      }

      /* Remove child object. */
      collection_object_remove(bmain, collection, cob->ob, true);
      cob = collection->gobject.first;
    }
  }

  BKE_id_delete(bmain, collection);

  BKE_main_collection_sync(bmain);

  return true;
}

/***************************** Collection Copy *******************************/

/**
 * Only copy internal data of Collection ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_collection_copy_data(Main *bmain,
                              Collection *collection_dst,
                              const Collection *collection_src,
                              const int flag)
{
  /* Do not copy collection's preview (same behavior as for objects). */
  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0 && false) { /* XXX TODO temp hack */
    BKE_previewimg_id_copy(&collection_dst->id, &collection_src->id);
  }
  else {
    collection_dst->preview = NULL;
  }

  collection_dst->flag &= ~COLLECTION_HAS_OBJECT_CACHE;
  BLI_listbase_clear(&collection_dst->object_cache);

  BLI_listbase_clear(&collection_dst->gobject);
  BLI_listbase_clear(&collection_dst->children);
  BLI_listbase_clear(&collection_dst->parents);

  for (CollectionChild *child = collection_src->children.first; child; child = child->next) {
    collection_child_add(collection_dst, child->collection, flag, false);
  }
  for (CollectionObject *cob = collection_src->gobject.first; cob; cob = cob->next) {
    collection_object_add(bmain, collection_dst, cob->ob, flag, false);
  }
}

static Collection *collection_duplicate_recursive(Main *bmain,
                                                  Collection *parent,
                                                  Collection *collection_old,
                                                  const bool do_hierarchy,
                                                  const bool do_objects,
                                                  const bool do_obdata)
{
  Collection *collection_new;
  bool do_full_process = false;
  const int object_dupflag = (do_obdata) ? U.dupflag : 0;

  if (!do_hierarchy || collection_old->id.newid == NULL) {
    BKE_id_copy(bmain, &collection_old->id, (ID **)&collection_new);
    id_us_min(
        &collection_new->id); /* Copying add one user by default, need to get rid of that one. */

    if (do_hierarchy) {
      ID_NEW_SET(collection_old, collection_new);
    }
    do_full_process = true;
  }
  else {
    collection_new = (Collection *)collection_old->id.newid;
  }

  /* Optionally add to parent (we always want to do that,
   * even if collection_old had already been duplicated). */
  if (parent != NULL) {
    if (collection_child_add(parent, collection_new, 0, true)) {
      /* Put collection right after existing one. */
      CollectionChild *child = collection_find_child(parent, collection_old);
      CollectionChild *child_new = collection_find_child(parent, collection_new);

      if (child && child_new) {
        BLI_remlink(&parent->children, child_new);
        BLI_insertlinkafter(&parent->children, child, child_new);
      }
    }
  }

  /* If we are not doing any kind of deep-copy, we can return immediately.
   * False do_full_process means collection_old had already been duplicated,
   * no need to redo some deep-copy on it. */
  if (!do_hierarchy || !do_full_process) {
    return collection_new;
  }

  if (do_objects) {
    /* We can loop on collection_old's objects, that list is currently identical the collection_new
     * objects, and won't be changed here. */
    for (CollectionObject *cob = collection_old->gobject.first; cob; cob = cob->next) {
      Object *ob_old = cob->ob;
      Object *ob_new = (Object *)ob_old->id.newid;

      if (ob_new == NULL) {
        ob_new = BKE_object_duplicate(bmain, ob_old, object_dupflag);
        ID_NEW_SET(ob_old, ob_new);
      }

      collection_object_add(bmain, collection_new, ob_new, 0, true);
      collection_object_remove(bmain, collection_new, ob_old, false);

      if (ob_new->rigidbody_object != NULL) {
        BLI_assert(ob_old->rigidbody_object != NULL);
        for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
          if (scene->rigidbody_world != NULL &&
              BKE_collection_has_object(scene->rigidbody_world->group, ob_old)) {
            collection_object_add(bmain, scene->rigidbody_world->group, ob_new, 0, true);
          }
        }
      }
    }
  }

  /* We can loop on collection_old's children,
   * that list is currently identical the collection_new' children, and won't be changed here. */
  for (CollectionChild *child = collection_old->children.first; child; child = child->next) {
    Collection *child_collection_old = child->collection;

    collection_duplicate_recursive(
        bmain, collection_new, child_collection_old, do_hierarchy, do_objects, do_obdata);
    collection_child_remove(collection_new, child_collection_old);
  }

  return collection_new;
}

/**
 * Makes a standard (aka shallow) ID copy of a Collection.
 *
 * Add a new collection in the same level as the old one, link any nested collections
 * and finally link the objects to the new collection (as opposed to copying them).
 */
Collection *BKE_collection_copy(Main *bmain, Collection *parent, Collection *collection)
{
  return BKE_collection_duplicate(bmain, parent, collection, false, false, false);
}

/**
 * Make either a shallow copy, or deeper duplicate of given collection.
 *
 * If \a do_hierarchy and \a do_deep_copy are false, this is a regular (shallow) ID copy.
 *
 * \warning If any 'deep copy' behavior is enabled,
 * this functions will clear all \a bmain id.idnew pointers.
 *
 * \param do_hierarchy: If true, it will recursively make shallow copies of children collections.
 * \param do_objects: If true, it will also make duplicates of objects.
 * This one does nothing if \a do_hierarchy is not set.
 * \param do_obdata: If true, it will also make deep duplicates of objects,
 * using behavior defined in user settings (#U.dupflag).
 * This one does nothing if \a do_hierarchy and \a do_objects are not set.
 */
Collection *BKE_collection_duplicate(Main *bmain,
                                     Collection *parent,
                                     Collection *collection,
                                     const bool do_hierarchy,
                                     const bool do_objects,
                                     const bool do_obdata)
{
  /* It's not allowed to copy the master collection. */
  if (collection->flag & COLLECTION_IS_MASTER) {
    BLI_assert("!Master collection can't be duplicated");
    return NULL;
  }

  if (do_hierarchy) {
    BKE_main_id_tag_all(bmain, LIB_TAG_NEW, false);
    BKE_main_id_clear_newpoins(bmain);
  }

  Collection *collection_new = collection_duplicate_recursive(
      bmain, parent, collection, do_hierarchy, do_objects, do_obdata);

  /* This code will follows into all ID links using an ID tagged with LIB_TAG_NEW.*/
  BKE_libblock_relink_to_newid(&collection_new->id);

  if (do_hierarchy) {
    /* Cleanup. */
    BKE_main_id_tag_all(bmain, LIB_TAG_NEW, false);
    BKE_main_id_clear_newpoins(bmain);
  }

  BKE_main_collection_sync(bmain);

  return collection_new;
}

Collection *BKE_collection_copy_master(Main *bmain, Collection *collection, const int flag)
{
  BLI_assert(collection->flag & COLLECTION_IS_MASTER);

  Collection *collection_dst = MEM_dupallocN(collection);
  BKE_collection_copy_data(bmain, collection_dst, collection, flag);
  return collection_dst;
}

void BKE_collection_make_local(Main *bmain, Collection *collection, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &collection->id, true, lib_local);
}

/********************************* Naming *******************************/

/**
 * The automatic/fallback name of a new collection.
 */
void BKE_collection_new_name_get(Collection *collection_parent, char *rname)
{
  char *name;

  if (!collection_parent) {
    name = BLI_strdup("Collection");
  }
  else if (collection_parent->flag & COLLECTION_IS_MASTER) {
    name = BLI_sprintfN("Collection %d", BLI_listbase_count(&collection_parent->children) + 1);
  }
  else {
    const int number = BLI_listbase_count(&collection_parent->children) + 1;
    const int digits = integer_digits_i(number);
    const int max_len = sizeof(collection_parent->id.name) - 1 /* NULL terminator */ -
                        (1 + digits) /* " %d" */ - 2 /* ID */;
    name = BLI_sprintfN("%.*s %d", max_len, collection_parent->id.name + 2, number);
  }

  BLI_strncpy(rname, name, MAX_NAME);
  MEM_freeN(name);
}

/**
 * The name to show in the interface.
 */
const char *BKE_collection_ui_name_get(struct Collection *collection)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return IFACE_("Scene Collection");
  }
  else {
    return collection->id.name + 2;
  }
}

/* **************** Object List Cache *******************/

static void collection_object_cache_fill(ListBase *lb, Collection *collection, int parent_restrict)
{
  int child_restrict = collection->flag | parent_restrict;

  for (CollectionObject *cob = collection->gobject.first; cob; cob = cob->next) {
    Base *base = BLI_findptr(lb, cob->ob, offsetof(Base, object));

    if (base == NULL) {
      base = MEM_callocN(sizeof(Base), "Object Base");
      base->object = cob->ob;
      BLI_addtail(lb, base);
    }

    int object_restrict = base->object->restrictflag;

    if (((child_restrict & COLLECTION_RESTRICT_VIEWPORT) == 0) &&
        ((object_restrict & OB_RESTRICT_VIEWPORT) == 0)) {
      base->flag |= BASE_ENABLED_VIEWPORT;
    }

    if (((child_restrict & COLLECTION_RESTRICT_RENDER) == 0) &&
        ((object_restrict & OB_RESTRICT_RENDER) == 0)) {
      base->flag |= BASE_ENABLED_RENDER;
    }
  }

  for (CollectionChild *child = collection->children.first; child; child = child->next) {
    collection_object_cache_fill(lb, child->collection, child_restrict);
  }
}

ListBase BKE_collection_object_cache_get(Collection *collection)
{
  if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE)) {
    static ThreadMutex cache_lock = BLI_MUTEX_INITIALIZER;

    BLI_mutex_lock(&cache_lock);
    if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE)) {
      collection_object_cache_fill(&collection->object_cache, collection, 0);
      collection->flag |= COLLECTION_HAS_OBJECT_CACHE;
    }
    BLI_mutex_unlock(&cache_lock);
  }

  return collection->object_cache;
}

static void collection_object_cache_free(Collection *collection)
{
  /* Clear own cache an for all parents, since those are affected by changes as well. */
  collection->flag &= ~COLLECTION_HAS_OBJECT_CACHE;
  BLI_freelistN(&collection->object_cache);

  for (CollectionParent *parent = collection->parents.first; parent; parent = parent->next) {
    collection_object_cache_free(parent->collection);
  }
}

void BKE_collection_object_cache_free(Collection *collection)
{
  collection_object_cache_free(collection);
}

Base *BKE_collection_or_layer_objects(const ViewLayer *view_layer, Collection *collection)
{
  if (collection) {
    return BKE_collection_object_cache_get(collection).first;
  }
  else {
    return FIRSTBASE(view_layer);
  }
}

/*********************** Scene Master Collection ***************/

Collection *BKE_collection_master_add()
{
  /* Not an actual datablock, but owned by scene. */
  Collection *master_collection = MEM_callocN(sizeof(Collection), "Master Collection");
  STRNCPY(master_collection->id.name, "GRMaster Collection");
  master_collection->flag |= COLLECTION_IS_MASTER;
  return master_collection;
}

Collection *BKE_collection_master(const Scene *scene)
{
  return scene->master_collection;
}

Scene *BKE_collection_master_scene_search(const Main *bmain, const Collection *master_collection)
{
  BLI_assert((master_collection->flag & COLLECTION_IS_MASTER) != 0);

  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    if (scene->master_collection == master_collection) {
      return scene;
    }
  }

  return NULL;
}

/*********************** Cyclic Checks ************************/

static bool collection_object_cyclic_check_internal(Object *object, Collection *collection)
{
  if (object->instance_collection) {
    Collection *dup_collection = object->instance_collection;
    if ((dup_collection->id.tag & LIB_TAG_DOIT) == 0) {
      /* Cycle already exists in collections, let's prevent further crappyness */
      return true;
    }
    /* flag the object to identify cyclic dependencies in further dupli collections */
    dup_collection->id.tag &= ~LIB_TAG_DOIT;

    if (dup_collection == collection) {
      return true;
    }
    else {
      FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (dup_collection, collection_object) {
        if (collection_object_cyclic_check_internal(collection_object, dup_collection)) {
          return true;
        }
      }
      FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
    }

    /* un-flag the object, it's allowed to have the same collection multiple times in parallel */
    dup_collection->id.tag |= LIB_TAG_DOIT;
  }

  return false;
}

bool BKE_collection_object_cyclic_check(Main *bmain, Object *object, Collection *collection)
{
  /* first flag all collections */
  BKE_main_id_tag_listbase(&bmain->collections, LIB_TAG_DOIT, true);

  return collection_object_cyclic_check_internal(object, collection);
}

/******************* Collection Object Membership *******************/

bool BKE_collection_has_object(Collection *collection, Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  return (BLI_findptr(&collection->gobject, ob, offsetof(CollectionObject, ob)));
}

bool BKE_collection_has_object_recursive(Collection *collection, Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  const ListBase objects = BKE_collection_object_cache_get(collection);
  return (BLI_findptr(&objects, ob, offsetof(Base, object)));
}

static Collection *collection_next_find(Main *bmain, Scene *scene, Collection *collection)
{
  if (scene && collection == BKE_collection_master(scene)) {
    return bmain->collections.first;
  }
  else {
    return collection->id.next;
  }
}

Collection *BKE_collection_object_find(Main *bmain,
                                       Scene *scene,
                                       Collection *collection,
                                       Object *ob)
{
  if (collection) {
    collection = collection_next_find(bmain, scene, collection);
  }
  else if (scene) {
    collection = BKE_collection_master(scene);
  }
  else {
    collection = bmain->collections.first;
  }

  while (collection) {
    if (BKE_collection_has_object(collection, ob)) {
      return collection;
    }
    collection = collection_next_find(bmain, scene, collection);
  }
  return NULL;
}

bool BKE_collection_is_empty(Collection *collection)
{
  return BLI_listbase_is_empty(&collection->gobject) &&
         BLI_listbase_is_empty(&collection->children);
}

/********************** Collection Objects *********************/

static void collection_tag_update_parent_recursive(Main *bmain,
                                                   Collection *collection,
                                                   const int flag)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  DEG_id_tag_update_ex(bmain, &collection->id, flag);

  for (CollectionParent *collection_parent = collection->parents.first; collection_parent;
       collection_parent = collection_parent->next) {
    if (collection_parent->collection->flag & COLLECTION_IS_MASTER) {
      /* We don't care about scene/master collection here. */
      continue;
    }
    collection_tag_update_parent_recursive(bmain, collection_parent->collection, flag);
  }
}

static bool collection_object_add(
    Main *bmain, Collection *collection, Object *ob, int flag, const bool add_us)
{
  if (ob->instance_collection) {
    /* Cyclic dependency check. */
    if (collection_find_child_recursive(ob->instance_collection, collection)) {
      return false;
    }
  }

  CollectionObject *cob = BLI_findptr(&collection->gobject, ob, offsetof(CollectionObject, ob));
  if (cob) {
    return false;
  }

  cob = MEM_callocN(sizeof(CollectionObject), __func__);
  cob->ob = ob;
  BLI_addtail(&collection->gobject, cob);
  BKE_collection_object_cache_free(collection);

  if (add_us && (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus(&ob->id);
  }

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    collection_tag_update_parent_recursive(bmain, collection, ID_RECALC_COPY_ON_WRITE);
  }

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    BKE_rigidbody_main_collection_object_add(bmain, collection, ob);
  }

  return true;
}

static bool collection_object_remove(Main *bmain,
                                     Collection *collection,
                                     Object *ob,
                                     const bool free_us)
{
  CollectionObject *cob = BLI_findptr(&collection->gobject, ob, offsetof(CollectionObject, ob));
  if (cob == NULL) {
    return false;
  }

  BLI_freelinkN(&collection->gobject, cob);
  BKE_collection_object_cache_free(collection);

  if (free_us) {
    BKE_id_free_us(bmain, ob);
  }
  else {
    id_us_min(&ob->id);
  }

  collection_tag_update_parent_recursive(bmain, collection, ID_RECALC_COPY_ON_WRITE);

  return true;
}

/**
 * Add object to collection
 */
bool BKE_collection_object_add(Main *bmain, Collection *collection, Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  if (!collection_object_add(bmain, collection, ob, 0, true)) {
    return false;
  }

  if (BKE_collection_is_in_scene(collection)) {
    BKE_main_collection_sync(bmain);
  }

  return true;
}

/**
 * Add object to all scene collections that reference object is in
 * (used to copy objects).
 */
void BKE_collection_object_add_from(Main *bmain, Scene *scene, Object *ob_src, Object *ob_dst)
{
  bool is_instantiated = false;

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (!ID_IS_LINKED(collection) && BKE_collection_has_object(collection, ob_src)) {
      collection_object_add(bmain, collection, ob_dst, 0, true);
      is_instantiated = true;
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  if (!is_instantiated) {
    /* In case we could not find any non-linked collections in which instantiate our ob_dst,
     * fallback to scene's master collection... */
    collection_object_add(bmain, BKE_collection_master(scene), ob_dst, 0, true);
  }

  BKE_main_collection_sync(bmain);
}

/**
 * Remove object from collection.
 */
bool BKE_collection_object_remove(Main *bmain,
                                  Collection *collection,
                                  Object *ob,
                                  const bool free_us)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  if (!collection_object_remove(bmain, collection, ob, free_us)) {
    return false;
  }

  if (BKE_collection_is_in_scene(collection)) {
    BKE_main_collection_sync(bmain);
  }

  return true;
}

/**
 * Remove object from all collections of scene
 * \param scene_collection_skip: Don't remove base from this collection.
 */
static bool scene_collections_object_remove(
    Main *bmain, Scene *scene, Object *ob, const bool free_us, Collection *collection_skip)
{
  bool removed = false;

  if (collection_skip == NULL) {
    BKE_scene_remove_rigidbody_object(bmain, scene, ob);
  }

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (collection != collection_skip) {
      removed |= collection_object_remove(bmain, collection, ob, free_us);
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  BKE_main_collection_sync(bmain);

  return removed;
}

/**
 * Remove object from all collections of scene
 */
bool BKE_scene_collections_object_remove(Main *bmain, Scene *scene, Object *ob, const bool free_us)
{
  return scene_collections_object_remove(bmain, scene, ob, free_us, NULL);
}

/*
 * Remove all NULL objects from collections.
 * This is used for library remapping, where these pointers have been set to NULL.
 * Otherwise this should never happen.
 */
static void collection_object_remove_nulls(Collection *collection)
{
  bool changed = false;

  for (CollectionObject *cob = collection->gobject.first, *cob_next = NULL; cob; cob = cob_next) {
    cob_next = cob->next;

    if (cob->ob == NULL) {
      BLI_freelinkN(&collection->gobject, cob);
      changed = true;
    }
  }

  if (changed) {
    BKE_collection_object_cache_free(collection);
  }
}

void BKE_collections_object_remove_nulls(Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    collection_object_remove_nulls(scene->master_collection);
  }

  for (Collection *collection = bmain->collections.first; collection;
       collection = collection->id.next) {
    collection_object_remove_nulls(collection);
  }
}

static void collection_null_children_remove(Collection *collection)
{
  for (CollectionChild *child = collection->children.first, *child_next = NULL; child;
       child = child_next) {
    child_next = child->next;

    if (child->collection == NULL) {
      BLI_freelinkN(&collection->children, child);
    }
  }
}

static void collection_missing_parents_remove(Collection *collection)
{
  for (CollectionParent *parent = collection->parents.first, *parent_next; parent != NULL;
       parent = parent_next) {
    parent_next = parent->next;
    if ((parent->collection == NULL) || !collection_find_child(parent->collection, collection)) {
      BLI_freelinkN(&collection->parents, parent);
    }
  }
}

/**
 * Remove all NULL children from parent collections of changed \a collection.
 * This is used for library remapping, where these pointers have been set to NULL.
 * Otherwise this should never happen.
 *
 * \note caller must ensure #BKE_main_collection_sync_remap() is called afterwards!
 *
 * \param collection: may be \a NULL,
 * in which case whole \a bmain database of collections is checked.
 */
void BKE_collections_child_remove_nulls(Main *bmain, Collection *collection)
{
  if (collection == NULL) {
    /* We need to do the checks in two steps when more than one collection may be involved,
     * otherwise we can miss some cases...
     * Also, master collections are not in bmain, so we also need to loop over scenes.
     */
    for (collection = bmain->collections.first; collection != NULL;
         collection = collection->id.next) {
      collection_null_children_remove(collection);
    }
    for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
      collection_null_children_remove(BKE_collection_master(scene));
    }

    for (collection = bmain->collections.first; collection != NULL;
         collection = collection->id.next) {
      collection_missing_parents_remove(collection);
    }
    for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
      collection_missing_parents_remove(BKE_collection_master(scene));
    }
  }
  else {
    for (CollectionParent *parent = collection->parents.first, *parent_next; parent;
         parent = parent_next) {
      parent_next = parent->next;

      collection_null_children_remove(parent->collection);

      if (!collection_find_child(parent->collection, collection)) {
        BLI_freelinkN(&collection->parents, parent);
      }
    }
  }
}

/**
 * Move object from a collection into another
 *
 * If source collection is NULL move it from all the existing collections.
 */
void BKE_collection_object_move(
    Main *bmain, Scene *scene, Collection *collection_dst, Collection *collection_src, Object *ob)
{
  /* In both cases we first add the object, then remove it from the other collections.
   * Otherwise we lose the original base and whether it was active and selected. */
  if (collection_src != NULL) {
    if (BKE_collection_object_add(bmain, collection_dst, ob)) {
      BKE_collection_object_remove(bmain, collection_src, ob, false);
    }
  }
  else {
    /* Adding will fail if object is already in collection.
     * However we still need to remove it from the other collections. */
    BKE_collection_object_add(bmain, collection_dst, ob);
    scene_collections_object_remove(bmain, scene, ob, false, collection_dst);
  }
}

/***************** Collection Scene Membership ****************/

bool BKE_collection_is_in_scene(Collection *collection)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return true;
  }

  for (CollectionParent *cparent = collection->parents.first; cparent; cparent = cparent->next) {
    if (BKE_collection_is_in_scene(cparent->collection)) {
      return true;
    }
  }

  return false;
}

void BKE_collections_after_lib_link(Main *bmain)
{
  /* Need to update layer collections because objects might have changed
   * in linked files, and because undo push does not include updated base
   * flags since those are refreshed after the operator completes. */
  BKE_main_collection_sync(bmain);
}

/********************** Collection Children *******************/

bool BKE_collection_find_cycle(Collection *new_ancestor, Collection *collection)
{
  if (collection == new_ancestor) {
    return true;
  }

  for (CollectionParent *parent = new_ancestor->parents.first; parent; parent = parent->next) {
    if (BKE_collection_find_cycle(parent->collection, collection)) {
      return true;
    }
  }

  return false;
}

static CollectionChild *collection_find_child(Collection *parent, Collection *collection)
{
  return BLI_findptr(&parent->children, collection, offsetof(CollectionChild, collection));
}

static bool collection_find_child_recursive(Collection *parent, Collection *collection)
{
  for (CollectionChild *child = parent->children.first; child; child = child->next) {
    if (child->collection == collection) {
      return true;
    }

    if (collection_find_child_recursive(child->collection, collection)) {
      return true;
    }
  }

  return false;
}

bool BKE_collection_has_collection(Collection *parent, Collection *collection)
{
  return collection_find_child_recursive(parent, collection);
}

static CollectionParent *collection_find_parent(Collection *child, Collection *collection)
{
  return BLI_findptr(&child->parents, collection, offsetof(CollectionParent, collection));
}

static bool collection_child_add(Collection *parent,
                                 Collection *collection,
                                 const int flag,
                                 const bool add_us)
{
  CollectionChild *child = collection_find_child(parent, collection);
  if (child) {
    return false;
  }
  if (BKE_collection_find_cycle(parent, collection)) {
    return false;
  }

  child = MEM_callocN(sizeof(CollectionChild), "CollectionChild");
  child->collection = collection;
  BLI_addtail(&parent->children, child);

  /* Don't add parent links for depsgraph datablocks, these are not kept in sync. */
  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    CollectionParent *cparent = MEM_callocN(sizeof(CollectionParent), "CollectionParent");
    cparent->collection = parent;
    BLI_addtail(&collection->parents, cparent);
  }

  if (add_us) {
    id_us_plus(&collection->id);
  }

  BKE_collection_object_cache_free(parent);

  return true;
}

static bool collection_child_remove(Collection *parent, Collection *collection)
{
  CollectionChild *child = collection_find_child(parent, collection);
  if (child == NULL) {
    return false;
  }

  CollectionParent *cparent = collection_find_parent(collection, parent);
  BLI_freelinkN(&collection->parents, cparent);
  BLI_freelinkN(&parent->children, child);

  id_us_min(&collection->id);

  BKE_collection_object_cache_free(parent);

  return true;
}

bool BKE_collection_child_add(Main *bmain, Collection *parent, Collection *child)
{
  if (!collection_child_add(parent, child, 0, true)) {
    return false;
  }

  BKE_main_collection_sync(bmain);
  return true;
}

bool BKE_collection_child_remove(Main *bmain, Collection *parent, Collection *child)
{
  if (!collection_child_remove(parent, child)) {
    return false;
  }

  BKE_main_collection_sync(bmain);
  return true;
}

/**
 * Rebuild parent relationships from child ones, for all children of given \a collection.
 *
 * \note Given collection is assumed to already have valid parents.
 */
void BKE_collection_parent_relations_rebuild(Collection *collection)
{
  for (CollectionChild *child = collection->children.first, *child_next = NULL; child;
       child = child_next) {
    child_next = child->next;

    if (child->collection == NULL || BKE_collection_find_cycle(collection, child->collection)) {
      BLI_freelinkN(&collection->children, child);
    }
    else {
      CollectionParent *cparent = MEM_callocN(sizeof(CollectionParent), __func__);
      cparent->collection = collection;
      BLI_addtail(&child->collection->parents, cparent);
    }
  }
}

static void collection_parents_rebuild_recursive(Collection *collection)
{
  BKE_collection_parent_relations_rebuild(collection);
  collection->id.tag &= ~LIB_TAG_DOIT;

  for (CollectionChild *child = collection->children.first; child != NULL; child = child->next) {
    collection_parents_rebuild_recursive(child->collection);
  }
}

/**
 * Rebuild parent relationships from child ones, for all collections in given \a bmain.
 *
 * \note Uses LIB_TAG_DOIT internally...
 */
void BKE_main_collections_parent_relations_rebuild(Main *bmain)
{
  /* Only collections not in bmain (master ones in scenes) have no parent... */
  for (Collection *collection = bmain->collections.first; collection != NULL;
       collection = collection->id.next) {
    BLI_freelistN(&collection->parents);

    collection->id.tag |= LIB_TAG_DOIT;
  }

  /* Scene's master collections will be 'root' parent of most of our collections, so start with
   * them. */
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    collection_parents_rebuild_recursive(scene->master_collection);
  }

  /* We may have parent chains outside of scene's master_collection context? At least, readfile's
   * lib_link_collection_data() seems to assume that, so do the same here. */
  for (Collection *collection = bmain->collections.first; collection != NULL;
       collection = collection->id.next) {
    if (collection->id.tag & LIB_TAG_DOIT) {
      /* Note: we do not have easy access to 'which collections is root' info in that case, which
       * means test for cycles in collection relationships may fail here. I don't think that is an
       * issue in practice here, but worth keeping in mind... */
      collection_parents_rebuild_recursive(collection);
    }
  }
}

/********************** Collection index *********************/

static Collection *collection_from_index_recursive(Collection *collection,
                                                   const int index,
                                                   int *index_current)
{
  if (index == (*index_current)) {
    return collection;
  }

  (*index_current)++;

  for (CollectionChild *child = collection->children.first; child; child = child->next) {
    Collection *nested = collection_from_index_recursive(child->collection, index, index_current);
    if (nested != NULL) {
      return nested;
    }
  }
  return NULL;
}

/**
 * Return Scene Collection for a given index.
 *
 * The index is calculated from top to bottom counting the children before the siblings.
 */
Collection *BKE_collection_from_index(Scene *scene, const int index)
{
  int index_current = 0;
  Collection *master_collection = BKE_collection_master(scene);
  return collection_from_index_recursive(master_collection, index, &index_current);
}

static bool collection_objects_select(ViewLayer *view_layer, Collection *collection, bool deselect)
{
  bool changed = false;

  if (collection->flag & COLLECTION_RESTRICT_SELECT) {
    return false;
  }

  for (CollectionObject *cob = collection->gobject.first; cob; cob = cob->next) {
    Base *base = BKE_view_layer_base_find(view_layer, cob->ob);

    if (base) {
      if (deselect) {
        if (base->flag & BASE_SELECTED) {
          base->flag &= ~BASE_SELECTED;
          changed = true;
        }
      }
      else {
        if ((base->flag & BASE_SELECTABLE) && !(base->flag & BASE_SELECTED)) {
          base->flag |= BASE_SELECTED;
          changed = true;
        }
      }
    }
  }

  for (CollectionChild *child = collection->children.first; child; child = child->next) {
    if (collection_objects_select(view_layer, collection, deselect)) {
      changed = true;
    }
  }

  return changed;
}

/**
 * Select all the objects in this Collection (and its nested collections) for this ViewLayer.
 * Return true if any object was selected.
 */
bool BKE_collection_objects_select(ViewLayer *view_layer, Collection *collection, bool deselect)
{
  LayerCollection *layer_collection = BKE_layer_collection_first_from_scene_collection(view_layer,
                                                                                       collection);

  if (layer_collection != NULL) {
    return BKE_layer_collection_objects_select(view_layer, layer_collection, deselect);
  }
  else {
    return collection_objects_select(view_layer, collection, deselect);
  }
}

/***************** Collection move (outliner drag & drop) *********************/

bool BKE_collection_move(Main *bmain,
                         Collection *to_parent,
                         Collection *from_parent,
                         Collection *relative,
                         bool relative_after,
                         Collection *collection)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return false;
  }
  if (BKE_collection_find_cycle(to_parent, collection)) {
    return false;
  }

  /* Move to new parent collection */
  if (from_parent) {
    collection_child_remove(from_parent, collection);
  }

  collection_child_add(to_parent, collection, 0, true);

  /* Move to specified location under parent. */
  if (relative) {
    CollectionChild *child = collection_find_child(to_parent, collection);
    CollectionChild *relative_child = collection_find_child(to_parent, relative);

    if (relative_child) {
      BLI_remlink(&to_parent->children, child);

      if (relative_after) {
        BLI_insertlinkafter(&to_parent->children, relative_child, child);
      }
      else {
        BLI_insertlinkbefore(&to_parent->children, relative_child, child);
      }

      BKE_collection_object_cache_free(to_parent);
    }
  }

  /* Make sure we store the flag of the layer collections before we remove and re-create them.
   * Otherwise they will get lost and everything will be copied from the new parent collection. */
  GHash *view_layer_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

  for (Scene *scene = bmain->scenes.first; scene; scene = scene->id.next) {
    for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
         view_layer = view_layer->next) {

      LayerCollection *layer_collection = BKE_layer_collection_first_from_scene_collection(
          view_layer, collection);

      if (layer_collection == NULL) {
        continue;
      }

      BLI_ghash_insert(view_layer_hash, view_layer, POINTER_FROM_INT(layer_collection->flag));
    }
  }

  /* Create and remove layer collections. */
  BKE_main_collection_sync(bmain);

  /* Restore back the original layer collection flags. */
  GHashIterator gh_iter;
  GHASH_ITER (gh_iter, view_layer_hash) {
    ViewLayer *view_layer = BLI_ghashIterator_getKey(&gh_iter);

    LayerCollection *layer_collection = BKE_layer_collection_first_from_scene_collection(
        view_layer, collection);

    if (layer_collection) {
      /* We treat exclude as a special case.
       *
       * If in a different view layer the parent collection was disabled (e.g., background)
       * and now we moved a new collection to be part of the background this collection should
       * probably be disabled.
       *
       * Note: If we were to also keep the exclude flag we would need to re-sync the collections.
       */
      layer_collection->flag = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter)) |
                               (layer_collection->flag & LAYER_COLLECTION_EXCLUDE);
    }
  }

  BLI_ghash_free(view_layer_hash, NULL, NULL);

  return true;
}

/**************************** Iterators ******************************/

/* scene collection iteractor */

typedef struct CollectionsIteratorData {
  Scene *scene;
  void **array;
  int tot, cur;
} CollectionsIteratorData;

static void scene_collection_callback(Collection *collection,
                                      BKE_scene_collections_Cb callback,
                                      void *data)
{
  callback(collection, data);

  for (CollectionChild *child = collection->children.first; child; child = child->next) {
    scene_collection_callback(child->collection, callback, data);
  }
}

static void scene_collections_count(Collection *UNUSED(collection), void *data)
{
  int *tot = data;
  (*tot)++;
}

static void scene_collections_build_array(Collection *collection, void *data)
{
  Collection ***array = data;
  **array = collection;
  (*array)++;
}

static void scene_collections_array(Scene *scene, Collection ***collections_array, int *tot)
{
  Collection *collection;
  Collection **array;

  *collections_array = NULL;
  *tot = 0;

  if (scene == NULL) {
    return;
  }

  collection = BKE_collection_master(scene);
  BLI_assert(collection != NULL);
  scene_collection_callback(collection, scene_collections_count, tot);

  if (*tot == 0) {
    return;
  }

  *collections_array = array = MEM_mallocN(sizeof(Collection *) * (*tot), "CollectionArray");
  scene_collection_callback(collection, scene_collections_build_array, &array);
}

/**
 * Only use this in non-performance critical situations
 * (it iterates over all scene collections twice)
 */
void BKE_scene_collections_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  Scene *scene = data_in;
  CollectionsIteratorData *data = MEM_callocN(sizeof(CollectionsIteratorData), __func__);

  data->scene = scene;
  iter->data = data;
  iter->valid = true;

  scene_collections_array(scene, (Collection ***)&data->array, &data->tot);
  BLI_assert(data->tot != 0);

  data->cur = 0;
  iter->current = data->array[data->cur];
}

void BKE_scene_collections_iterator_next(struct BLI_Iterator *iter)
{
  CollectionsIteratorData *data = iter->data;

  if (++data->cur < data->tot) {
    iter->current = data->array[data->cur];
  }
  else {
    iter->valid = false;
  }
}

void BKE_scene_collections_iterator_end(struct BLI_Iterator *iter)
{
  CollectionsIteratorData *data = iter->data;

  if (data) {
    if (data->array) {
      MEM_freeN(data->array);
    }
    MEM_freeN(data);
  }
  iter->valid = false;
}

/* scene objects iterator */

typedef struct SceneObjectsIteratorData {
  GSet *visited;
  CollectionObject *cob_next;
  BLI_Iterator scene_collection_iter;
} SceneObjectsIteratorData;

void BKE_scene_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  Scene *scene = data_in;
  SceneObjectsIteratorData *data = MEM_callocN(sizeof(SceneObjectsIteratorData), __func__);
  iter->data = data;

  /* lookup list ot make sure each object is object called once */
  data->visited = BLI_gset_ptr_new(__func__);

  /* we wrap the scenecollection iterator here to go over the scene collections */
  BKE_scene_collections_iterator_begin(&data->scene_collection_iter, scene);

  Collection *collection = data->scene_collection_iter.current;
  data->cob_next = collection->gobject.first;

  BKE_scene_objects_iterator_next(iter);
}

/**
 * Ensures we only get each object once, even when included in several collections.
 */
static CollectionObject *object_base_unique(GSet *gs, CollectionObject *cob)
{
  for (; cob != NULL; cob = cob->next) {
    Object *ob = cob->ob;
    void **ob_key_p;
    if (!BLI_gset_ensure_p_ex(gs, ob, &ob_key_p)) {
      *ob_key_p = ob;
      return cob;
    }
  }
  return NULL;
}

void BKE_scene_objects_iterator_next(BLI_Iterator *iter)
{
  SceneObjectsIteratorData *data = iter->data;
  CollectionObject *cob = data->cob_next ? object_base_unique(data->visited, data->cob_next) :
                                           NULL;

  if (cob) {
    data->cob_next = cob->next;
    iter->current = cob->ob;
  }
  else {
    /* if this is the last object of this ListBase look at the next Collection */
    Collection *collection;
    BKE_scene_collections_iterator_next(&data->scene_collection_iter);
    do {
      collection = data->scene_collection_iter.current;
      /* get the first unique object of this collection */
      CollectionObject *new_cob = object_base_unique(data->visited, collection->gobject.first);
      if (new_cob) {
        data->cob_next = new_cob->next;
        iter->current = new_cob->ob;
        return;
      }
      BKE_scene_collections_iterator_next(&data->scene_collection_iter);
    } while (data->scene_collection_iter.valid);

    if (!data->scene_collection_iter.valid) {
      iter->valid = false;
    }
  }
}

void BKE_scene_objects_iterator_end(BLI_Iterator *iter)
{
  SceneObjectsIteratorData *data = iter->data;
  if (data) {
    BKE_scene_collections_iterator_end(&data->scene_collection_iter);
    BLI_gset_free(data->visited, NULL);
    MEM_freeN(data);
  }
}
