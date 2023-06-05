/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "CLG_log.h"

#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_iterator.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_threads.h"
#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_collection.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"

#include "DNA_defaults.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"

#include "BLO_read_write.h"

static CLG_LogRef LOG = {"bke.collection"};

/**
 * Extra asserts that #Collection.gobject_hash is valid which are too slow even for debug mode.
 */
// #define USE_DEBUG_EXTRA_GOBJECT_ASSERT

/* -------------------------------------------------------------------- */
/** \name Prototypes
 * \{ */

static bool collection_child_add(Collection *parent,
                                 Collection *collection,
                                 const int flag,
                                 const bool add_us);
static bool collection_child_remove(Collection *parent, Collection *collection);
static bool collection_object_add(
    Main *bmain, Collection *collection, Object *ob, int flag, const bool add_us);

static void collection_object_remove_no_gobject_hash(Main *bmain,
                                                     Collection *collection,
                                                     CollectionObject *cob,
                                                     const bool free_us);
static bool collection_object_remove(Main *bmain,
                                     Collection *collection,
                                     Object *ob,
                                     const bool free_us);

static CollectionChild *collection_find_child(Collection *parent, Collection *collection);
static CollectionParent *collection_find_parent(Collection *child, Collection *collection);

static bool collection_find_child_recursive(const Collection *parent,
                                            const Collection *collection);

static void collection_object_cache_free(Collection *collection);

static void collection_gobject_hash_ensure(Collection *collection);
static void collection_gobject_hash_update_object(Collection *collection,
                                                  Object *ob_old,
                                                  CollectionObject *cob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Data-Block
 * \{ */

static void collection_init_data(ID *id)
{
  Collection *collection = (Collection *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(collection, id));

  MEMCPY_STRUCT_AFTER(collection, DNA_struct_default_get(Collection), id);
}

/**
 * Only copy internal data of Collection ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void collection_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Collection *collection_dst = (Collection *)id_dst;
  const Collection *collection_src = (const Collection *)id_src;

  BLI_assert(((collection_src->flag & COLLECTION_IS_MASTER) != 0) ==
             ((collection_src->id.flag & LIB_EMBEDDED_DATA) != 0));

  /* Do not copy collection's preview (same behavior as for objects). */
  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0 && false) { /* XXX TODO: temp hack. */
    BKE_previewimg_id_copy(&collection_dst->id, &collection_src->id);
  }
  else {
    collection_dst->preview = NULL;
  }

  collection_dst->flag &= ~(COLLECTION_HAS_OBJECT_CACHE | COLLECTION_HAS_OBJECT_CACHE_INSTANCED);
  BLI_listbase_clear(&collection_dst->runtime.object_cache);
  BLI_listbase_clear(&collection_dst->runtime.object_cache_instanced);

  BLI_listbase_clear(&collection_dst->gobject);
  BLI_listbase_clear(&collection_dst->children);
  BLI_listbase_clear(&collection_dst->runtime.parents);
  collection_dst->runtime.gobject_hash = NULL;

  LISTBASE_FOREACH (CollectionChild *, child, &collection_src->children) {
    collection_child_add(collection_dst, child->collection, flag, false);
  }
  LISTBASE_FOREACH (CollectionObject *, cob, &collection_src->gobject) {
    collection_object_add(bmain, collection_dst, cob->ob, flag, false);
  }
}

static void collection_free_data(ID *id)
{
  Collection *collection = (Collection *)id;

  /* No animation-data here. */
  BKE_previewimg_free(&collection->preview);

  BLI_freelistN(&collection->gobject);
  if (collection->runtime.gobject_hash) {
    BLI_ghash_free(collection->runtime.gobject_hash, NULL, NULL);
    collection->runtime.gobject_hash = NULL;
  }

  BLI_freelistN(&collection->children);
  BLI_freelistN(&collection->runtime.parents);

  collection_object_cache_free(collection);
}

static void collection_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Collection *collection = (Collection *)id;
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);

  BKE_LIB_FOREACHID_PROCESS_ID(
      data, collection->runtime.owner_id, IDWALK_CB_LOOPBACK | IDWALK_CB_NEVER_SELF);

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Object *cob_ob_old = cob->ob;

    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, cob->ob, IDWALK_CB_USER);

    if (collection->runtime.gobject_hash) {
      /* If the remapping does not create inconsistent data (NULL object pointer or duplicate
       * CollectionObjects), keeping the ghash consistent is also possible. Otherwise, this call
       * will take care of tagging the collection objects list as dirty. */
      collection_gobject_hash_update_object(collection, cob_ob_old, cob);
    }
    else if (cob_ob_old != cob->ob || cob->ob == NULL) {
      /* If there is no reference GHash, duplicates cannot be reliably detected, so assume that any
       * NULL pointer or changed pointer may create an invalid collection object list. */
      collection->runtime.tag |= COLLECTION_TAG_COLLECTION_OBJECT_DIRTY;
    }
  }
  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(
        data, child->collection, IDWALK_CB_NEVER_SELF | IDWALK_CB_USER);
  }
  LISTBASE_FOREACH (CollectionParent *, parent, &collection->runtime.parents) {
    /* XXX This is very weak. The whole idea of keeping pointers to private IDs is very bad
     * anyway... */
    const int cb_flag = ((parent->collection != NULL &&
                          (data_flags & IDWALK_NO_ORIG_POINTERS_ACCESS) == 0 &&
                          (parent->collection->id.flag & LIB_EMBEDDED_DATA) != 0) ?
                             IDWALK_CB_EMBEDDED_NOT_OWNING :
                             IDWALK_CB_NOP);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(
        data, parent->collection, IDWALK_CB_NEVER_SELF | IDWALK_CB_LOOPBACK | cb_flag);
  }
}

static ID **collection_owner_pointer_get(ID *id)
{
  if ((id->flag & LIB_EMBEDDED_DATA) == 0) {
    return NULL;
  }
  BLI_assert((id->tag & LIB_TAG_NO_MAIN) == 0);

  Collection *master_collection = (Collection *)id;
  BLI_assert((master_collection->flag & COLLECTION_IS_MASTER) != 0);
  BLI_assert(master_collection->runtime.owner_id != NULL);
  BLI_assert(GS(master_collection->runtime.owner_id->name) == ID_SCE);
  BLI_assert(((Scene *)master_collection->runtime.owner_id)->master_collection ==
             master_collection);

  return &master_collection->runtime.owner_id;
}

void BKE_collection_blend_write_nolib(BlendWriter *writer, Collection *collection)
{
  BKE_id_blend_write(writer, &collection->id);

  /* Shared function for collection data-blocks and scene master collection. */
  BKE_previewimg_blend_write(writer, collection->preview);

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    BLO_write_struct(writer, CollectionObject, cob);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_write_struct(writer, CollectionChild, child);
  }
}

static void collection_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Collection *collection = (Collection *)id;

  memset(&collection->runtime, 0, sizeof(collection->runtime));
  /* Clean up, important in undo case to reduce false detection of changed data-blocks. */
  collection->flag &= ~COLLECTION_FLAG_ALL_RUNTIME;

  /* write LibData */
  BLO_write_id_struct(writer, Collection, id_address, &collection->id);

  BKE_collection_blend_write_nolib(writer, collection);
}

#ifdef USE_COLLECTION_COMPAT_28
void BKE_collection_compat_blend_read_data(BlendDataReader *reader, SceneCollection *sc)
{
  BLO_read_list(reader, &sc->objects);
  BLO_read_list(reader, &sc->scene_collections);

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    BKE_collection_compat_blend_read_data(reader, nsc);
  }
}
#endif

void BKE_collection_blend_read_data(BlendDataReader *reader, Collection *collection, ID *owner_id)
{
  /* Special case for this pointer, do not rely on regular `lib_link` process here. Avoids needs
   * for do_versioning, and ensures coherence of data in any case.
   *
   * NOTE: Old versions are very often 'broken' here, just fix it silently in these cases.
   */
  if (BLO_read_fileversion_get(reader) > 300) {
    BLI_assert((collection->id.flag & LIB_EMBEDDED_DATA) != 0 || owner_id == NULL);
  }
  BLI_assert(owner_id == NULL || owner_id->lib == collection->id.lib);
  if (owner_id != NULL && (collection->id.flag & LIB_EMBEDDED_DATA) == 0) {
    /* This is unfortunate, but currently a lot of existing files (including startup ones) have
     * missing `LIB_EMBEDDED_DATA` flag.
     *
     * NOTE: Using do_version is not a solution here, since this code will be called before any
     * do_version takes place. Keeping it here also ensures future (or unknown existing) similar
     * bugs won't go easily unnoticed. */
    if (BLO_read_fileversion_get(reader) > 300) {
      CLOG_WARN(&LOG,
                "Fixing root node tree '%s' owned by '%s' missing EMBEDDED tag, please consider "
                "re-saving your (startup) file",
                collection->id.name,
                owner_id->name);
    }
    collection->id.flag |= LIB_EMBEDDED_DATA;
  }

  memset(&collection->runtime, 0, sizeof(collection->runtime));
  collection->flag &= ~COLLECTION_FLAG_ALL_RUNTIME;

  collection->runtime.owner_id = owner_id;

  BLO_read_list(reader, &collection->gobject);
  BLO_read_list(reader, &collection->children);

  BLO_read_data_address(reader, &collection->preview);
  BKE_previewimg_blend_read(reader, collection->preview);

#ifdef USE_COLLECTION_COMPAT_28
  /* This runs before the very first doversion. */
  BLO_read_data_address(reader, &collection->collection);
  if (collection->collection != NULL) {
    BKE_collection_compat_blend_read_data(reader, collection->collection);
  }

  BLO_read_data_address(reader, &collection->view_layer);
  if (collection->view_layer != NULL) {
    BKE_view_layer_blend_read_data(reader, collection->view_layer);
  }
#endif
}

static void collection_blend_read_data(BlendDataReader *reader, ID *id)
{
  Collection *collection = (Collection *)id;
  BKE_collection_blend_read_data(reader, collection, NULL);
}

static void lib_link_collection_data(BlendLibReader *reader, ID *self_id, Collection *collection)
{
  BLI_assert(collection->runtime.gobject_hash == NULL);
  LISTBASE_FOREACH_MUTABLE (CollectionObject *, cob, &collection->gobject) {
    BLO_read_id_address(reader, self_id, &cob->ob);

    if (cob->ob == NULL) {
      BLI_freelinkN(&collection->gobject, cob);
    }
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_read_id_address(reader, self_id, &child->collection);
  }
}

#ifdef USE_COLLECTION_COMPAT_28
void BKE_collection_compat_blend_read_lib(BlendLibReader *reader, ID *self_id, SceneCollection *sc)
{
  LISTBASE_FOREACH (LinkData *, link, &sc->objects) {
    BLO_read_id_address(reader, self_id, &link->data);
    BLI_assert(link->data);
  }

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    BKE_collection_compat_blend_read_lib(reader, self_id, nsc);
  }
}
#endif

void BKE_collection_blend_read_lib(BlendLibReader *reader, Collection *collection)
{
#ifdef USE_COLLECTION_COMPAT_28
  if (collection->collection) {
    BKE_collection_compat_blend_read_lib(reader, &collection->id, collection->collection);
  }

  if (collection->view_layer) {
    BKE_view_layer_blend_read_lib(reader, &collection->id, collection->view_layer);
  }
#endif

  lib_link_collection_data(reader, &collection->id, collection);
}

static void collection_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Collection *collection = (Collection *)id;
  BKE_collection_blend_read_lib(reader, collection);
}

#ifdef USE_COLLECTION_COMPAT_28
void BKE_collection_compat_blend_read_expand(BlendExpander *expander, SceneCollection *sc)
{
  LISTBASE_FOREACH (LinkData *, link, &sc->objects) {
    BLO_expand(expander, link->data);
  }

  LISTBASE_FOREACH (SceneCollection *, nsc, &sc->scene_collections) {
    BKE_collection_compat_blend_read_expand(expander, nsc);
  }
}
#endif

void BKE_collection_blend_read_expand(BlendExpander *expander, Collection *collection)
{
  BLI_assert(collection->runtime.gobject_hash == NULL);
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    BLO_expand(expander, cob->ob);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    BLO_expand(expander, child->collection);
  }

#ifdef USE_COLLECTION_COMPAT_28
  if (collection->collection != NULL) {
    BKE_collection_compat_blend_read_expand(expander, collection->collection);
  }
#endif
}

static void collection_blend_read_expand(BlendExpander *expander, ID *id)
{
  Collection *collection = (Collection *)id;
  BKE_collection_blend_read_expand(expander, collection);
}

IDTypeInfo IDType_ID_GR = {
    .id_code = ID_GR,
    .id_filter = FILTER_ID_GR,
    .main_listbase_index = INDEX_ID_GR,
    .struct_size = sizeof(Collection),
    .name = "Collection",
    .name_plural = "collections",
    .translation_context = BLT_I18NCONTEXT_ID_COLLECTION,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = NULL,

    .init_data = collection_init_data,
    .copy_data = collection_copy_data,
    .free_data = collection_free_data,
    .make_local = NULL,
    .foreach_id = collection_foreach_id,
    .foreach_cache = NULL,
    .foreach_path = NULL,
    .owner_pointer_get = collection_owner_pointer_get,

    .blend_write = collection_blend_write,
    .blend_read_data = collection_blend_read_data,
    .blend_read_lib = collection_blend_read_lib,
    .blend_read_expand = collection_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Collection
 * \{ */

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
  Collection *collection = BKE_id_new(bmain, ID_GR, name);

  /* We increase collection user count when linking to Collections. */
  id_us_min(&collection->id);

  /* Optionally add to parent collection. */
  if (collection_parent) {
    collection_child_add(collection_parent, collection, 0, true);
  }

  return collection;
}

Collection *BKE_collection_add(Main *bmain, Collection *collection_parent, const char *name_custom)
{
  Collection *collection = collection_add(bmain, collection_parent, name_custom);
  BKE_main_collection_sync(bmain);
  return collection;
}

void BKE_collection_add_from_object(Main *bmain,
                                    Scene *scene,
                                    const Object *ob_src,
                                    Collection *collection_dst)
{
  bool is_instantiated = false;

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDABLE_LIBRARY(collection) &&
        BKE_collection_has_object(collection, ob_src))
    {
      collection_child_add(collection, collection_dst, 0, true);
      is_instantiated = true;
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  if (!is_instantiated) {
    collection_child_add(scene->master_collection, collection_dst, 0, true);
  }

  BKE_main_collection_sync(bmain);
}

void BKE_collection_add_from_collection(Main *bmain,
                                        Scene *scene,
                                        Collection *collection_src,
                                        Collection *collection_dst)
{
  bool is_instantiated = false;

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY(collection) &&
        collection_find_child(collection, collection_src))
    {
      collection_child_add(collection, collection_dst, 0, true);
      is_instantiated = true;
    }
    else if (!is_instantiated && collection_find_child(collection, collection_dst)) {
      /* If given collection_dst is already instantiated in scene, even if its 'model'
       * collection_src one is not, do not add it to master scene collection. */
      is_instantiated = true;
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  if (!is_instantiated) {
    collection_child_add(scene->master_collection, collection_dst, 0, true);
  }

  BKE_main_collection_sync(bmain);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free and Delete Collection
 * \{ */

void BKE_collection_free_data(Collection *collection)
{
  BKE_libblock_free_data(&collection->id, false);
  collection_free_data(&collection->id);
}

bool BKE_collection_delete(Main *bmain, Collection *collection, bool hierarchy)
{
  /* Master collection is not real datablock, can't be removed. */
  if (collection->flag & COLLECTION_IS_MASTER) {
    BLI_assert_msg(0, "Scene master collection can't be deleted");
    return false;
  }

  /* This is being deleted, no need to handle each item.
   * NOTE: While it might seem an advantage to use the hash instead of the list-lookup
   * it is in fact slower because the items are removed in-order,
   * so the list-lookup succeeds on the first test. */
  if (collection->runtime.gobject_hash) {
    BLI_ghash_free(collection->runtime.gobject_hash, NULL, NULL);
    collection->runtime.gobject_hash = NULL;
  }

  if (hierarchy) {
    /* Remove child objects. */
    CollectionObject *cob = collection->gobject.first;
    while (cob != NULL) {
      collection_object_remove_no_gobject_hash(bmain, collection, cob, true);
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
    LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
      LISTBASE_FOREACH (CollectionParent *, cparent, &collection->runtime.parents) {
        Collection *parent = cparent->collection;
        collection_child_add(parent, child->collection, 0, true);
      }
    }

    CollectionObject *cob = collection->gobject.first;
    while (cob != NULL) {
      /* Link child object into parent collections. */
      LISTBASE_FOREACH (CollectionParent *, cparent, &collection->runtime.parents) {
        Collection *parent = cparent->collection;
        collection_object_add(bmain, parent, cob->ob, 0, true);
      }

      /* Remove child object. */
      collection_object_remove_no_gobject_hash(bmain, collection, cob, true);
      cob = collection->gobject.first;
    }
  }

  BKE_id_delete(bmain, collection);

  BKE_main_collection_sync(bmain);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Copy
 * \{ */

static Collection *collection_duplicate_recursive(Main *bmain,
                                                  Collection *parent,
                                                  Collection *collection_old,
                                                  const eDupli_ID_Flags duplicate_flags,
                                                  const eLibIDDuplicateFlags duplicate_options)
{
  Collection *collection_new;
  bool do_full_process = false;
  const bool is_collection_master = (collection_old->flag & COLLECTION_IS_MASTER) != 0;

  const bool do_objects = (duplicate_flags & USER_DUP_OBJECT) != 0;

  if (is_collection_master) {
    /* We never duplicate master collections here, but we can still deep-copy their objects and
     * collections. */
    BLI_assert(parent == NULL);
    collection_new = collection_old;
    do_full_process = true;
  }
  else if (collection_old->id.newid == NULL) {
    collection_new = (Collection *)BKE_id_copy_for_duplicate(
        bmain, (ID *)collection_old, duplicate_flags, LIB_ID_COPY_DEFAULT);

    if (collection_new == collection_old) {
      return collection_new;
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
  if (!do_full_process) {
    return collection_new;
  }

  if (do_objects) {
    /* We need to first duplicate the objects in a separate loop, to support the master collection
     * case, where both old and new collections are the same.
     * Otherwise, depending on naming scheme and sorting, we may end up duplicating the new objects
     * we just added, in some infinite loop. */
    LISTBASE_FOREACH (CollectionObject *, cob, &collection_old->gobject) {
      Object *ob_old = cob->ob;

      if (ob_old->id.newid == NULL) {
        BKE_object_duplicate(
            bmain, ob_old, duplicate_flags, duplicate_options | LIB_ID_DUPLICATE_IS_SUBPROCESS);
      }
    }

    /* We can loop on collection_old's objects, but have to consider it mutable because with master
     * collections collection_old and collection_new are the same data here. */
    LISTBASE_FOREACH_MUTABLE (CollectionObject *, cob, &collection_old->gobject) {
      Object *ob_old = cob->ob;
      Object *ob_new = (Object *)ob_old->id.newid;

      /* New object can be NULL in master collection case, since new and old objects are in same
       * collection. */
      if (ELEM(ob_new, ob_old, NULL)) {
        continue;
      }

      collection_object_add(bmain, collection_new, ob_new, 0, true);
      collection_object_remove(bmain, collection_new, ob_old, false);
    }
  }

  /* We can loop on collection_old's children,
   * that list is currently identical the collection_new' children, and won't be changed here. */
  LISTBASE_FOREACH_MUTABLE (CollectionChild *, child, &collection_old->children) {
    Collection *child_collection_old = child->collection;

    Collection *child_collection_new = collection_duplicate_recursive(
        bmain, collection_new, child_collection_old, duplicate_flags, duplicate_options);
    if (child_collection_new != child_collection_old) {
      collection_child_remove(collection_new, child_collection_old);
    }
  }

  return collection_new;
}

Collection *BKE_collection_duplicate(Main *bmain,
                                     Collection *parent,
                                     Collection *collection,
                                     /*eDupli_ID_Flags*/ uint duplicate_flags,
                                     /*eLibIDDuplicateFlags*/ uint duplicate_options)
{
  const bool is_subprocess = (duplicate_options & LIB_ID_DUPLICATE_IS_SUBPROCESS) != 0;
  const bool is_root_id = (duplicate_options & LIB_ID_DUPLICATE_IS_ROOT_ID) != 0;

  if (!is_subprocess) {
    BKE_main_id_newptr_and_tag_clear(bmain);
  }
  if (is_root_id) {
    /* In case root duplicated ID is linked, assume we want to get a local copy of it and duplicate
     * all expected linked data. */
    if (ID_IS_LINKED(collection)) {
      duplicate_flags |= USER_DUP_LINKED_ID;
    }
    duplicate_options &= ~LIB_ID_DUPLICATE_IS_ROOT_ID;
  }

  Collection *collection_new = collection_duplicate_recursive(
      bmain, parent, collection, duplicate_flags, duplicate_options);

  if (!is_subprocess) {
    /* `collection_duplicate_recursive` will also tag our 'root' collection, which is not required
     * unless its duplication is a sub-process of another one. */
    collection_new->id.tag &= ~LIB_TAG_NEW;

    /* This code will follow into all ID links using an ID tagged with LIB_TAG_NEW. */
    BKE_libblock_relink_to_newid(bmain, &collection_new->id, 0);

#ifndef NDEBUG
    /* Call to `BKE_libblock_relink_to_newid` above is supposed to have cleared all those flags. */
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (id_iter->tag & LIB_TAG_NEW) {
        BLI_assert((id_iter->tag & LIB_TAG_NEW) == 0);
      }
    }
    FOREACH_MAIN_ID_END;
#endif

    /* Cleanup. */
    BKE_main_id_newptr_and_tag_clear(bmain);

    BKE_main_collection_sync(bmain);
  }

  return collection_new;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Naming
 * \{ */

void BKE_collection_new_name_get(Collection *collection_parent, char *rname)
{
  char *name;

  if (!collection_parent) {
    name = BLI_strdup(DATA_("Collection"));
  }
  else if (collection_parent->flag & COLLECTION_IS_MASTER) {
    name = BLI_sprintfN(DATA_("Collection %d"),
                        BLI_listbase_count(&collection_parent->children) + 1);
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

const char *BKE_collection_ui_name_get(Collection *collection)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return IFACE_("Scene Collection");
  }

  return collection->id.name + 2;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object List Cache
 * \{ */

static void collection_object_cache_fill(ListBase *lb,
                                         Collection *collection,
                                         int parent_restrict,
                                         bool with_instances)
{
  int child_restrict = collection->flag | parent_restrict;

  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Base *base = BLI_findptr(lb, cob->ob, offsetof(Base, object));

    if (base == NULL) {
      base = MEM_callocN(sizeof(Base), "Object Base");
      base->object = cob->ob;
      BLI_addtail(lb, base);
      if (with_instances && cob->ob->instance_collection) {
        collection_object_cache_fill(
            lb, cob->ob->instance_collection, child_restrict, with_instances);
      }
    }

    /* Only collection flags are checked here currently, object restrict flag is checked
     * in FOREACH_COLLECTION_VISIBLE_OBJECT_RECURSIVE_BEGIN since it can be animated
     * without updating the cache. */
    if ((child_restrict & COLLECTION_HIDE_VIEWPORT) == 0) {
      base->flag |= BASE_ENABLED_VIEWPORT;
    }
    if ((child_restrict & COLLECTION_HIDE_RENDER) == 0) {
      base->flag |= BASE_ENABLED_RENDER;
    }
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    collection_object_cache_fill(lb, child->collection, child_restrict, with_instances);
  }
}

ListBase BKE_collection_object_cache_get(Collection *collection)
{
  if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE)) {
    static ThreadMutex cache_lock = BLI_MUTEX_INITIALIZER;

    BLI_mutex_lock(&cache_lock);
    if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE)) {
      collection_object_cache_fill(&collection->runtime.object_cache, collection, 0, false);
      collection->flag |= COLLECTION_HAS_OBJECT_CACHE;
    }
    BLI_mutex_unlock(&cache_lock);
  }

  return collection->runtime.object_cache;
}

ListBase BKE_collection_object_cache_instanced_get(Collection *collection)
{
  if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE_INSTANCED)) {
    static ThreadMutex cache_lock = BLI_MUTEX_INITIALIZER;

    BLI_mutex_lock(&cache_lock);
    if (!(collection->flag & COLLECTION_HAS_OBJECT_CACHE_INSTANCED)) {
      collection_object_cache_fill(
          &collection->runtime.object_cache_instanced, collection, 0, true);
      collection->flag |= COLLECTION_HAS_OBJECT_CACHE_INSTANCED;
    }
    BLI_mutex_unlock(&cache_lock);
  }

  return collection->runtime.object_cache_instanced;
}

static void collection_object_cache_free(Collection *collection)
{
  collection->flag &= ~(COLLECTION_HAS_OBJECT_CACHE | COLLECTION_HAS_OBJECT_CACHE_INSTANCED);
  BLI_freelistN(&collection->runtime.object_cache);
  BLI_freelistN(&collection->runtime.object_cache_instanced);
}

static void collection_object_cache_free_parent_recursive(Collection *collection)
{
  collection_object_cache_free(collection);

  /* Clear cache in all parents recursively, since those are affected by changes as well. */
  LISTBASE_FOREACH (CollectionParent *, parent, &collection->runtime.parents) {
    /* In theory there should be no NULL pointer here. However, this code can be called from
     * non-valid temporary states (e.g. indirectly from #BKE_collections_object_remove_invalids
     * as part of ID remapping process). */
    if (parent->collection == NULL) {
      continue;
    }
    collection_object_cache_free_parent_recursive(parent->collection);
  }
}

void BKE_collection_object_cache_free(Collection *collection)
{
  BLI_assert(collection != NULL);
  collection_object_cache_free_parent_recursive(collection);
}

void BKE_main_collections_object_cache_free(const Main *bmain)
{
  for (Scene *scene = bmain->scenes.first; scene != NULL; scene = scene->id.next) {
    collection_object_cache_free(scene->master_collection);
  }

  for (Collection *collection = bmain->collections.first; collection != NULL;
       collection = collection->id.next)
  {
    collection_object_cache_free(collection);
  }
}

Base *BKE_collection_or_layer_objects(const Scene *scene,
                                      ViewLayer *view_layer,
                                      Collection *collection)
{
  if (collection) {
    return BKE_collection_object_cache_get(collection).first;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  return BKE_view_layer_object_bases_get(view_layer)->first;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Master Collection
 * \{ */

Collection *BKE_collection_master_add(Scene *scene)
{
  BLI_assert(scene != NULL && scene->master_collection == NULL);

  /* Not an actual datablock, but owned by scene. */
  Collection *master_collection = BKE_libblock_alloc(
      NULL, ID_GR, BKE_SCENE_COLLECTION_NAME, LIB_ID_CREATE_NO_MAIN);
  master_collection->id.flag |= LIB_EMBEDDED_DATA;
  master_collection->runtime.owner_id = &scene->id;
  master_collection->flag |= COLLECTION_IS_MASTER;
  master_collection->color_tag = COLLECTION_COLOR_NONE;

  return master_collection;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cyclic Checks
 * \{ */

static bool collection_object_cyclic_check_internal(Object *object, Collection *collection)
{
  if (object->instance_collection) {
    Collection *dup_collection = object->instance_collection;
    if ((dup_collection->id.tag & LIB_TAG_DOIT) == 0) {
      /* Cycle already exists in collections, let's prevent further creepiness. */
      return true;
    }
    /* flag the object to identify cyclic dependencies in further dupli collections */
    dup_collection->id.tag &= ~LIB_TAG_DOIT;

    if (dup_collection == collection) {
      return true;
    }

    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (dup_collection, collection_object) {
      if (collection_object_cyclic_check_internal(collection_object, dup_collection)) {
        return true;
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Object Membership
 * \{ */

bool BKE_collection_has_object(Collection *collection, const Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }
  collection_gobject_hash_ensure(collection);
  return BLI_ghash_lookup(collection->runtime.gobject_hash, ob);
}

bool BKE_collection_has_object_recursive(Collection *collection, Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  const ListBase objects = BKE_collection_object_cache_get(collection);
  return BLI_findptr(&objects, ob, offsetof(Base, object));
}

bool BKE_collection_has_object_recursive_instanced(Collection *collection, Object *ob)
{
  if (ELEM(NULL, collection, ob)) {
    return false;
  }

  const ListBase objects = BKE_collection_object_cache_instanced_get(collection);
  return BLI_findptr(&objects, ob, offsetof(Base, object));
}

static Collection *collection_next_find(Main *bmain, Scene *scene, Collection *collection)
{
  if (scene && collection == scene->master_collection) {
    return bmain->collections.first;
  }

  return collection->id.next;
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
    collection = scene->master_collection;
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

bool BKE_collection_is_empty(const Collection *collection)
{
  return BLI_listbase_is_empty(&collection->gobject) &&
         BLI_listbase_is_empty(&collection->children);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Objects
 * \{ */

static void collection_gobject_assert_internal_consistency(Collection *collection,
                                                           const bool do_extensive_check);

static GHash *collection_gobject_hash_alloc(const Collection *collection)
{
  return BLI_ghash_ptr_new_ex(__func__, (uint)BLI_listbase_count(&collection->gobject));
}

static void collection_gobject_hash_create(Collection *collection)
{
  GHash *gobject_hash = collection_gobject_hash_alloc(collection);
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    if (UNLIKELY(cob->ob == NULL)) {
      BLI_assert(collection->runtime.tag & COLLECTION_TAG_COLLECTION_OBJECT_DIRTY);
      continue;
    }
    CollectionObject **cob_p;
    /* Do not overwrite an already existing entry. */
    if (UNLIKELY(BLI_ghash_ensure_p(gobject_hash, cob->ob, (void ***)&cob_p))) {
      BLI_assert(collection->runtime.tag & COLLECTION_TAG_COLLECTION_OBJECT_DIRTY);
      continue;
    }
    *cob_p = cob;
  }
  collection->runtime.gobject_hash = gobject_hash;
}

static void collection_gobject_hash_ensure(Collection *collection)
{
  if (collection->runtime.gobject_hash) {
#ifdef USE_DEBUG_EXTRA_GOBJECT_ASSERT
    collection_gobject_assert_internal_consistency(collection, true);
#endif
    return;
  }

  collection_gobject_hash_create(collection);

  collection_gobject_assert_internal_consistency(collection, true);
}

/** Similar to #collection_gobject_hash_ensure/#collection_gobject_hash_create, but does fix
 * inconsistencies in the collection objects list. */
static void collection_gobject_hash_ensure_fix(Collection *collection)
{
  bool changed = false;

  if ((collection->runtime.tag & COLLECTION_TAG_COLLECTION_OBJECT_DIRTY) == 0) {
#ifdef USE_DEBUG_EXTRA_GOBJECT_ASSERT
    collection_gobject_assert_internal_consistency(collection, true);
#endif
    return;
  }

  GHash *gobject_hash = collection->runtime.gobject_hash;
  if (gobject_hash) {
    BLI_ghash_clear_ex(gobject_hash, NULL, NULL, BLI_ghash_len(gobject_hash));
  }
  else {
    collection->runtime.gobject_hash = gobject_hash = collection_gobject_hash_alloc(collection);
  }

  LISTBASE_FOREACH_MUTABLE (CollectionObject *, cob, &collection->gobject) {
    if (cob->ob == NULL) {
      BLI_freelinkN(&collection->gobject, cob);
      changed = true;
      continue;
    }
    CollectionObject **cob_p;
    if (BLI_ghash_ensure_p(gobject_hash, cob->ob, (void ***)&cob_p)) {
      BLI_freelinkN(&collection->gobject, cob);
      changed = true;
      continue;
    }
    *cob_p = cob;
  }

  if (changed) {
    BKE_collection_object_cache_free(collection);
  }

  collection->runtime.tag &= ~COLLECTION_TAG_COLLECTION_OBJECT_DIRTY;
  collection_gobject_assert_internal_consistency(collection, true);
}

/**
 * Update the collections object hash, removing `ob_old`, inserting `cob->ob` as the new key.
 *
 * \note This function is called from #IDTypeInfo::foreach_id callback,
 * and a difference of Object pointers is only expected in case ID remapping is happening.
 * This code is the only are in Blender allowed to (temporarily) leave the #CollectionObject list
 * in an inconsistent/invalid state (with NULL object pointers, or duplicates of
 * #CollectionObjects). If such invalid cases are encountered,
 * it will tag the collection objects list as dirty.
 *
 * \param ob_old: The existing key to `cob` in the hash, not removed when NULL.
 * \param cob: The `cob->ob` is to be used as the new key,
 * when NULL it's not added back into the hash.
 */
static void collection_gobject_hash_update_object(Collection *collection,
                                                  Object *ob_old,
                                                  CollectionObject *cob)
{
  if (ob_old == cob->ob) {
    return;
  }

  if (ob_old) {
    CollectionObject *cob_old = BLI_ghash_popkey(collection->runtime.gobject_hash, ob_old, NULL);
    if (cob_old != cob) {
      /* Old object already removed from the #GHash. */
      collection->runtime.tag |= COLLECTION_TAG_COLLECTION_OBJECT_DIRTY;
    }
  }

  if (cob->ob) {
    CollectionObject **cob_p;
    if (!BLI_ghash_ensure_p(collection->runtime.gobject_hash, cob->ob, (void ***)&cob_p)) {
      *cob_p = cob;
    }
    else {
      /* Duplicate #CollectionObject entries. */
      collection->runtime.tag |= COLLECTION_TAG_COLLECTION_OBJECT_DIRTY;
    }
  }
  else {
    /* #CollectionObject with NULL object pointer. */
    collection->runtime.tag |= COLLECTION_TAG_COLLECTION_OBJECT_DIRTY;
  }
}

/**
 * Validate the integrity of the collection's #CollectionObject list, and of its mapping.
 *
 * Simple test is very fast, as it only checks that the 'dirty' tag for collection's objects is not
 * set.
 *
 * The extensive check is expensive. This should not be done from within loops over collections
 * items, or from low-level operations that can be assumed safe (like adding or removing an object
 * from a collection). It ensures that:
 * - There is a `gobject_hash` mapping.
 * - There is no NULL-object #CollectionObject items.
 * - there is no duplicate #CollectionObject items (two or more referencing the same Object).
 */
static void collection_gobject_assert_internal_consistency(Collection *collection,
                                                           const bool do_extensive_check)
{
  BLI_assert((collection->runtime.tag & COLLECTION_TAG_COLLECTION_OBJECT_DIRTY) == 0);
  if (!do_extensive_check) {
    return;
  }

  if (collection->runtime.gobject_hash == NULL) {
    /* NOTE: If the `ghash` does not exist yet, it's creation will assert on errors,
     * so in theory the second loop below could be skipped. */
    collection_gobject_hash_create(collection);
  }
  GHash *gobject_hash = collection->runtime.gobject_hash;
  UNUSED_VARS_NDEBUG(gobject_hash);
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    BLI_assert(cob->ob != NULL);
    /* If there are more than one #CollectionObject for the same object,
     * at most one of them will pass this test. */
    BLI_assert(BLI_ghash_lookup(gobject_hash, cob->ob) == cob);
  }
}

static void collection_tag_update_parent_recursive(Main *bmain,
                                                   Collection *collection,
                                                   const int flag)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return;
  }

  DEG_id_tag_update_ex(bmain, &collection->id, flag);

  LISTBASE_FOREACH (CollectionParent *, collection_parent, &collection->runtime.parents) {
    if (collection_parent->collection->flag & COLLECTION_IS_MASTER) {
      /* We don't care about scene/master collection here. */
      continue;
    }
    collection_tag_update_parent_recursive(bmain, collection_parent->collection, flag);
  }
}

static Collection *collection_parent_editable_find_recursive(const ViewLayer *view_layer,
                                                             Collection *collection)
{
  if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY(collection) &&
      (view_layer == NULL || BKE_view_layer_has_collection(view_layer, collection)))
  {
    return collection;
  }

  if (collection->flag & COLLECTION_IS_MASTER) {
    return NULL;
  }

  LISTBASE_FOREACH (CollectionParent *, collection_parent, &collection->runtime.parents) {
    if (!ID_IS_LINKED(collection_parent->collection) &&
        !ID_IS_OVERRIDE_LIBRARY(collection_parent->collection))
    {
      if (view_layer != NULL &&
          !BKE_view_layer_has_collection(view_layer, collection_parent->collection))
      {
        /* In case this parent collection is not in given view_layer, there is no point in
         * searching in its ancestors either, we can skip that whole parenting branch. */
        continue;
      }
      return collection_parent->collection;
    }
    Collection *editable_collection = collection_parent_editable_find_recursive(
        view_layer, collection_parent->collection);
    if (editable_collection != NULL) {
      return editable_collection;
    }
  }

  return NULL;
}

static bool collection_object_add(
    Main *bmain, Collection *collection, Object *ob, int flag, const bool add_us)
{
  /* Cyclic dependency check. */
  if (ob->instance_collection) {
    if ((ob->instance_collection == collection) ||
        collection_find_child_recursive(ob->instance_collection, collection))
    {
      return false;
    }
  }

  collection_gobject_hash_ensure(collection);
  CollectionObject **cob_p;
  if (BLI_ghash_ensure_p(collection->runtime.gobject_hash, ob, (void ***)&cob_p)) {
    return false;
  }

  CollectionObject *cob = MEM_callocN(sizeof(CollectionObject), __func__);
  cob->ob = ob;
  *cob_p = cob;
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

/**
 * A version of #collection_object_remove that does not handle `collection->runtime.gobject_hash`,
 * Either the caller must have removed the object from the hash or the hash may be NULL.
 */
static void collection_object_remove_no_gobject_hash(Main *bmain,
                                                     Collection *collection,
                                                     CollectionObject *cob,
                                                     const bool free_us)
{
  Object *ob = cob->ob;
  BLI_freelinkN(&collection->gobject, cob);
  BKE_collection_object_cache_free(collection);

  if (free_us) {
    BKE_id_free_us(bmain, ob);
  }
  else {
    id_us_min(&ob->id);
  }

  collection_tag_update_parent_recursive(
      bmain, collection, ID_RECALC_COPY_ON_WRITE | ID_RECALC_GEOMETRY);
}

static bool collection_object_remove(Main *bmain,
                                     Collection *collection,
                                     Object *ob,
                                     const bool free_us)
{
  collection_gobject_hash_ensure(collection);
  CollectionObject *cob = BLI_ghash_popkey(collection->runtime.gobject_hash, ob, NULL);
  if (cob == NULL) {
    return false;
  }
  collection_object_remove_no_gobject_hash(bmain, collection, cob, free_us);
  return true;
}

bool BKE_collection_object_add_notest(Main *bmain, Collection *collection, Object *ob)
{
  if (ob == NULL) {
    return false;
  }

  /* Only case where this pointer can be NULL is when scene itself is linked, this case should
   * never be reached. */
  BLI_assert(collection != NULL);
  if (collection == NULL) {
    return false;
  }

  if (!collection_object_add(bmain, collection, ob, 0, true)) {
    return false;
  }

  if (BKE_collection_is_in_scene(collection)) {
    BKE_main_collection_sync(bmain);
  }

  DEG_id_tag_update(&collection->id, ID_RECALC_GEOMETRY | ID_RECALC_HIERARCHY);

  return true;
}

bool BKE_collection_object_add(Main *bmain, Collection *collection, Object *ob)
{
  return BKE_collection_viewlayer_object_add(bmain, NULL, collection, ob);
}

bool BKE_collection_viewlayer_object_add(Main *bmain,
                                         const ViewLayer *view_layer,
                                         Collection *collection,
                                         Object *ob)
{
  if (collection == NULL) {
    return false;
  }

  collection = collection_parent_editable_find_recursive(view_layer, collection);

  if (collection == NULL) {
    return false;
  }

  return BKE_collection_object_add_notest(bmain, collection, ob);
}

void BKE_collection_object_add_from(Main *bmain, Scene *scene, Object *ob_src, Object *ob_dst)
{
  bool is_instantiated = false;

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (!ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY(collection) &&
        BKE_collection_has_object(collection, ob_src))
    {
      collection_object_add(bmain, collection, ob_dst, 0, true);
      is_instantiated = true;
    }
  }
  FOREACH_SCENE_COLLECTION_END;

  if (!is_instantiated) {
    /* In case we could not find any non-linked collections in which instantiate our ob_dst,
     * fallback to scene's master collection... */
    collection_object_add(bmain, scene->master_collection, ob_dst, 0, true);
  }

  BKE_main_collection_sync(bmain);
}

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

  DEG_id_tag_update(&collection->id, ID_RECALC_GEOMETRY | ID_RECALC_HIERARCHY);

  return true;
}

bool BKE_collection_object_replace(Main *bmain,
                                   Collection *collection,
                                   Object *ob_old,
                                   Object *ob_new)
{
  collection_gobject_hash_ensure(collection);
  CollectionObject *cob;
  cob = BLI_ghash_popkey(collection->runtime.gobject_hash, ob_old, NULL);
  if (cob == NULL) {
    return false;
  }

  if (!BLI_ghash_haskey(collection->runtime.gobject_hash, ob_new)) {
    id_us_min(&cob->ob->id);
    cob->ob = ob_new;
    id_us_plus(&cob->ob->id);

    BLI_ghash_insert(collection->runtime.gobject_hash, cob->ob, cob);
  }
  else {
    collection_object_remove_no_gobject_hash(bmain, collection, cob, false);
  }

  if (BKE_collection_is_in_scene(collection)) {
    BKE_main_collection_sync(bmain);
  }

  return true;
}

/**
 * Remove object from all collections of scene
 * \param collection_skip: Don't remove base from this collection.
 */
static bool scene_collections_object_remove(
    Main *bmain, Scene *scene, Object *ob, const bool free_us, Collection *collection_skip)
{
  bool removed = false;

  /* If given object is removed from all collections in given scene, then it can also be safely
   * removed from rigidbody world for given scene. */
  if (collection_skip == NULL) {
    BKE_scene_remove_rigidbody_object(bmain, scene, ob, free_us);
  }

  FOREACH_SCENE_COLLECTION_BEGIN (scene, collection) {
    if (ID_IS_LINKED(collection) || ID_IS_OVERRIDE_LIBRARY(collection)) {
      continue;
    }
    if (collection == collection_skip) {
      continue;
    }

    removed |= collection_object_remove(bmain, collection, ob, free_us);
  }
  FOREACH_SCENE_COLLECTION_END;

  BKE_main_collection_sync(bmain);

  return removed;
}

bool BKE_scene_collections_object_remove(Main *bmain, Scene *scene, Object *ob, const bool free_us)
{
  return scene_collections_object_remove(bmain, scene, ob, free_us, NULL);
}

void BKE_collections_object_remove_invalids(Main *bmain)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    collection_gobject_hash_ensure_fix(scene->master_collection);
  }

  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    collection_gobject_hash_ensure_fix(collection);
  }
}

static void collection_null_children_remove(Collection *collection)
{
  LISTBASE_FOREACH_MUTABLE (CollectionChild *, child, &collection->children) {
    if (child->collection == NULL) {
      BLI_freelinkN(&collection->children, child);
    }
  }
}

static void collection_missing_parents_remove(Collection *collection)
{
  LISTBASE_FOREACH_MUTABLE (CollectionParent *, parent, &collection->runtime.parents) {
    if ((parent->collection == NULL) || !collection_find_child(parent->collection, collection)) {
      BLI_freelinkN(&collection->runtime.parents, parent);
    }
  }
}

void BKE_collections_child_remove_nulls(Main *bmain,
                                        Collection *parent_collection,
                                        Collection *child_collection)
{
  if (child_collection == NULL) {
    if (parent_collection != NULL) {
      collection_null_children_remove(parent_collection);
    }
    else {
      /* We need to do the checks in two steps when more than one collection may be involved,
       * otherwise we can miss some cases...
       * Also, master collections are not in bmain, so we also need to loop over scenes.
       */
      LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
        collection_null_children_remove(collection);
      }
      LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
        collection_null_children_remove(scene->master_collection);
      }
    }

    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      collection_missing_parents_remove(collection);
    }
    LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
      collection_missing_parents_remove(scene->master_collection);
    }
  }
  else {
    LISTBASE_FOREACH_MUTABLE (CollectionParent *, parent, &child_collection->runtime.parents) {
      collection_null_children_remove(parent->collection);

      if (!collection_find_child(parent->collection, child_collection)) {
        BLI_freelinkN(&child_collection->runtime.parents, parent);
      }
    }
  }
}

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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Scene Membership
 * \{ */

bool BKE_collection_is_in_scene(Collection *collection)
{
  if (collection->flag & COLLECTION_IS_MASTER) {
    return true;
  }

  LISTBASE_FOREACH (CollectionParent *, cparent, &collection->runtime.parents) {
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Children
 * \{ */

static bool collection_instance_find_recursive(Collection *collection,
                                               Collection *instance_collection)
{
  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
    if (collection_object->ob != NULL &&
        /* Object from a given collection should never instantiate that collection either. */
        ELEM(collection_object->ob->instance_collection, instance_collection, collection))
    {
      return true;
    }
  }

  LISTBASE_FOREACH (CollectionChild *, collection_child, &collection->children) {
    if (collection_child->collection != NULL &&
        collection_instance_find_recursive(collection_child->collection, instance_collection))
    {
      return true;
    }
  }

  return false;
}

bool BKE_collection_cycle_find(Collection *new_ancestor, Collection *collection)
{
  if (collection == new_ancestor) {
    return true;
  }

  if (collection == NULL) {
    collection = new_ancestor;
  }

  LISTBASE_FOREACH (CollectionParent *, parent, &new_ancestor->runtime.parents) {
    if (BKE_collection_cycle_find(parent->collection, collection)) {
      return true;
    }
  }

  /* Find possible objects in collection or its children, that would instantiate the given ancestor
   * collection (that would also make a fully invalid cycle of dependencies). */
  return collection_instance_find_recursive(collection, new_ancestor);
}

static bool collection_instance_fix_recursive(Collection *parent_collection,
                                              Collection *collection)
{
  bool cycles_found = false;

  LISTBASE_FOREACH (CollectionObject *, collection_object, &parent_collection->gobject) {
    if (collection_object->ob != NULL && collection_object->ob->instance_collection == collection)
    {
      id_us_min(&collection->id);
      collection_object->ob->instance_collection = NULL;
      cycles_found = true;
    }
  }

  LISTBASE_FOREACH (CollectionChild *, collection_child, &parent_collection->children) {
    if (collection_instance_fix_recursive(collection_child->collection, collection)) {
      cycles_found = true;
    }
  }

  return cycles_found;
}

static bool collection_cycle_fix_recursive(Main *bmain,
                                           Collection *parent_collection,
                                           Collection *collection)
{
  bool cycles_found = false;

  LISTBASE_FOREACH_MUTABLE (CollectionParent *, parent, &parent_collection->runtime.parents) {
    if (BKE_collection_cycle_find(parent->collection, collection)) {
      BKE_collection_child_remove(bmain, parent->collection, parent_collection);
      cycles_found = true;
    }
    else if (collection_cycle_fix_recursive(bmain, parent->collection, collection)) {
      cycles_found = true;
    }
  }

  return cycles_found;
}

bool BKE_collection_cycles_fix(Main *bmain, Collection *collection)
{
  return collection_cycle_fix_recursive(bmain, collection, collection) ||
         collection_instance_fix_recursive(collection, collection);
}

static CollectionChild *collection_find_child(Collection *parent, Collection *collection)
{
  return BLI_findptr(&parent->children, collection, offsetof(CollectionChild, collection));
}

static bool collection_find_child_recursive(const Collection *parent, const Collection *collection)
{
  LISTBASE_FOREACH (const CollectionChild *, child, &parent->children) {
    if (child->collection == collection) {
      return true;
    }

    if (collection_find_child_recursive(child->collection, collection)) {
      return true;
    }
  }

  return false;
}

bool BKE_collection_has_collection(const Collection *parent, const Collection *collection)
{
  return collection_find_child_recursive(parent, collection);
}

static CollectionParent *collection_find_parent(Collection *child, Collection *collection)
{
  return BLI_findptr(&child->runtime.parents, collection, offsetof(CollectionParent, collection));
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
  if (BKE_collection_cycle_find(parent, collection)) {
    return false;
  }

  child = MEM_callocN(sizeof(CollectionChild), "CollectionChild");
  child->collection = collection;
  BLI_addtail(&parent->children, child);

  /* Don't add parent links for depsgraph datablocks, these are not kept in sync. */
  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    CollectionParent *cparent = MEM_callocN(sizeof(CollectionParent), "CollectionParent");
    cparent->collection = parent;
    BLI_addtail(&collection->runtime.parents, cparent);
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
  BLI_freelinkN(&collection->runtime.parents, cparent);
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

bool BKE_collection_child_add_no_sync(Collection *parent, Collection *child)
{
  return collection_child_add(parent, child, 0, true);
}

bool BKE_collection_child_remove(Main *bmain, Collection *parent, Collection *child)
{
  if (!collection_child_remove(parent, child)) {
    return false;
  }

  BKE_main_collection_sync(bmain);
  return true;
}

void BKE_collection_parent_relations_rebuild(Collection *collection)
{
  LISTBASE_FOREACH_MUTABLE (CollectionChild *, child, &collection->children) {
    /* Check for duplicated children (can happen with remapping e.g.). */
    CollectionChild *other_child = collection_find_child(collection, child->collection);
    if (other_child != child) {
      BLI_freelinkN(&collection->children, child);
      continue;
    }

    /* Invalid child, either without a collection, or because it creates a dependency cycle. */
    if (child->collection == NULL || BKE_collection_cycle_find(collection, child->collection)) {
      BLI_freelinkN(&collection->children, child);
      continue;
    }

    /* Can happen when remapping data partially out-of-Main (during advanced ID management
     * operations like lib-override resync e.g.). */
    if ((child->collection->id.tag & (LIB_TAG_NO_MAIN | LIB_TAG_COPIED_ON_WRITE)) != 0) {
      continue;
    }

    BLI_assert(collection_find_parent(child->collection, collection) == NULL);
    CollectionParent *cparent = MEM_callocN(sizeof(CollectionParent), __func__);
    cparent->collection = collection;
    BLI_addtail(&child->collection->runtime.parents, cparent);
  }
}

static void collection_parents_rebuild_recursive(Collection *collection)
{
  /* A same collection may be child of several others, no need to process it more than once. */
  if ((collection->runtime.tag & COLLECTION_TAG_RELATION_REBUILD) == 0) {
    return;
  }

  BKE_collection_parent_relations_rebuild(collection);
  collection->runtime.tag &= ~COLLECTION_TAG_RELATION_REBUILD;

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    /* See comment above in `BKE_collection_parent_relations_rebuild`. */
    if ((child->collection->id.tag & (LIB_TAG_NO_MAIN | LIB_TAG_COPIED_ON_WRITE)) != 0) {
      continue;
    }
    collection_parents_rebuild_recursive(child->collection);
  }
}

void BKE_main_collections_parent_relations_rebuild(Main *bmain)
{
  /* Only collections not in bmain (master ones in scenes) have no parent... */
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    BLI_freelistN(&collection->runtime.parents);

    collection->runtime.tag |= COLLECTION_TAG_RELATION_REBUILD;
  }

  /* Scene's master collections will be 'root' parent of most of our collections, so start with
   * them. */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    /* This function can be called from readfile.c, when this pointer is not guaranteed to be NULL.
     */
    if (scene->master_collection != NULL) {
      BLI_assert(BLI_listbase_is_empty(&scene->master_collection->runtime.parents));
      scene->master_collection->runtime.tag |= COLLECTION_TAG_RELATION_REBUILD;
      collection_parents_rebuild_recursive(scene->master_collection);
    }
  }

  /* We may have parent chains outside of scene's master_collection context? At least, readfile's
   * lib_link_collection_data() seems to assume that, so do the same here. */
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (collection->runtime.tag & COLLECTION_TAG_RELATION_REBUILD) {
      /* NOTE: we do not have easy access to 'which collections is root' info in that case, which
       * means test for cycles in collection relationships may fail here. I don't think that is an
       * issue in practice here, but worth keeping in mind... */
      collection_parents_rebuild_recursive(collection);
    }
  }
}

bool BKE_collection_validate(Collection *collection)
{
  if (!BLI_listbase_validate(&collection->children)) {
    return false;
  }
  if (!BLI_listbase_validate(&collection->runtime.parents)) {
    return false;
  }
  if (BKE_collection_cycle_find(collection, NULL)) {
    return false;
  }

  bool is_ok = true;

  /* Check that children have each collection used/referenced only once. */
  GSet *processed_collections = BLI_gset_ptr_new(__func__);
  for (CollectionChild *child = collection->children.first; child; child = child->next) {
    void **r_key;
    if (BLI_gset_ensure_p_ex(processed_collections, child->collection, &r_key)) {
      is_ok = false;
    }
    else {
      *r_key = child->collection;
    }
  }

  /* Check that parents have each collection used/referenced only once. */
  BLI_gset_clear(processed_collections, NULL);
  for (CollectionParent *parent = collection->runtime.parents.first; parent; parent = parent->next)
  {
    void **r_key;
    if (BLI_gset_ensure_p_ex(processed_collections, parent->collection, &r_key)) {
      is_ok = false;
    }
    else {
      *r_key = parent->collection;
    }
  }

  BLI_gset_free(processed_collections, NULL);
  return is_ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Index
 * \{ */

static Collection *collection_from_index_recursive(Collection *collection,
                                                   const int index,
                                                   int *index_current)
{
  if (index == (*index_current)) {
    return collection;
  }

  (*index_current)++;

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    Collection *nested = collection_from_index_recursive(child->collection, index, index_current);
    if (nested != NULL) {
      return nested;
    }
  }
  return NULL;
}

Collection *BKE_collection_from_index(Scene *scene, const int index)
{
  int index_current = 0;
  Collection *master_collection = scene->master_collection;
  return collection_from_index_recursive(master_collection, index, &index_current);
}

static bool collection_objects_select(const Scene *scene,
                                      ViewLayer *view_layer,
                                      Collection *collection,
                                      bool deselect)
{
  bool changed = false;

  if (collection->flag & COLLECTION_HIDE_SELECT) {
    return false;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
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

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    if (collection_objects_select(scene, view_layer, collection, deselect)) {
      changed = true;
    }
  }

  return changed;
}

bool BKE_collection_objects_select(const Scene *scene,
                                   ViewLayer *view_layer,
                                   Collection *collection,
                                   bool deselect)
{
  LayerCollection *layer_collection = BKE_layer_collection_first_from_scene_collection(view_layer,
                                                                                       collection);

  if (layer_collection != NULL) {
    return BKE_layer_collection_objects_select(scene, view_layer, layer_collection, deselect);
  }

  return collection_objects_select(scene, view_layer, collection, deselect);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection move (outliner drag & drop)
 * \{ */

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
  if (BKE_collection_cycle_find(to_parent, collection)) {
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

  /* Update layer collections. */
  BKE_main_collection_sync(bmain);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Iterators
 * \{ */

/* Scene collection iterator. */

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

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
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

static void scene_collections_array(Scene *scene,
                                    Collection ***r_collections_array,
                                    int *r_collections_array_len)
{
  *r_collections_array = NULL;
  *r_collections_array_len = 0;

  if (scene == NULL) {
    return;
  }

  Collection *collection = scene->master_collection;
  BLI_assert(collection != NULL);
  scene_collection_callback(collection, scene_collections_count, r_collections_array_len);

  BLI_assert(*r_collections_array_len > 0);

  Collection **array = MEM_malloc_arrayN(
      *r_collections_array_len, sizeof(Collection *), "CollectionArray");
  *r_collections_array = array;
  scene_collection_callback(collection, scene_collections_build_array, &array);
}

void BKE_scene_collections_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  Scene *scene = data_in;
  CollectionsIteratorData *data = MEM_callocN(sizeof(CollectionsIteratorData), __func__);

  data->scene = scene;

  BLI_ITERATOR_INIT(iter);
  iter->data = data;

  scene_collections_array(scene, (Collection ***)&data->array, &data->tot);
  BLI_assert(data->tot != 0);

  data->cur = 0;
  iter->current = data->array[data->cur];
}

void BKE_scene_collections_iterator_next(BLI_Iterator *iter)
{
  CollectionsIteratorData *data = iter->data;

  if (++data->cur < data->tot) {
    iter->current = data->array[data->cur];
  }
  else {
    iter->valid = false;
  }
}

void BKE_scene_collections_iterator_end(BLI_Iterator *iter)
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

static void scene_objects_iterator_begin(BLI_Iterator *iter, Scene *scene, GSet *visited_objects)
{
  SceneObjectsIteratorData *data = MEM_callocN(sizeof(SceneObjectsIteratorData), __func__);

  BLI_ITERATOR_INIT(iter);
  iter->data = data;

  /* Lookup list to make sure that each object is only processed once. */
  if (visited_objects != NULL) {
    data->visited = visited_objects;
  }
  else {
    data->visited = BLI_gset_ptr_new(__func__);
  }

  /* We wrap the scenecollection iterator here to go over the scene collections. */
  BKE_scene_collections_iterator_begin(&data->scene_collection_iter, scene);

  Collection *collection = data->scene_collection_iter.current;
  data->cob_next = collection->gobject.first;

  BKE_scene_objects_iterator_next(iter);
}

void BKE_scene_objects_iterator_begin(BLI_Iterator *iter, void *data_in)
{
  Scene *scene = data_in;

  scene_objects_iterator_begin(iter, scene, NULL);
}

static void scene_objects_iterator_skip_invalid_flag(BLI_Iterator *iter)
{
  if (!iter->valid) {
    return;
  }

  /* Unpack the data. */
  SceneObjectsIteratorExData *data = iter->data;
  iter->data = data->iter_data;

  Object *ob = iter->current;
  if (ob && (ob->flag & data->flag) == 0) {
    iter->skip = true;
  }

  /* Pack the data. */
  data->iter_data = iter->data;
  iter->data = data;
}

void BKE_scene_objects_iterator_begin_ex(BLI_Iterator *iter, void *data_in)
{
  SceneObjectsIteratorExData *data = data_in;

  BKE_scene_objects_iterator_begin(iter, data->scene);

  /* Pack the data. */
  data->iter_data = iter->data;
  iter->data = data_in;

  scene_objects_iterator_skip_invalid_flag(iter);
}

void BKE_scene_objects_iterator_next_ex(BLI_Iterator *iter)
{
  /* Unpack the data. */
  SceneObjectsIteratorExData *data = iter->data;
  iter->data = data->iter_data;

  BKE_scene_objects_iterator_next(iter);

  /* Pack the data. */
  data->iter_data = iter->data;
  iter->data = data;

  scene_objects_iterator_skip_invalid_flag(iter);
}

void BKE_scene_objects_iterator_end_ex(BLI_Iterator *iter)
{
  /* Unpack the data. */
  SceneObjectsIteratorExData *data = iter->data;
  iter->data = data->iter_data;

  BKE_scene_objects_iterator_end(iter);

  /* Pack the data. */
  data->iter_data = iter->data;
  iter->data = data;
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
    if (data->visited != NULL) {
      BLI_gset_free(data->visited, NULL);
    }
    MEM_freeN(data);
  }
}

GSet *BKE_scene_objects_as_gset(Scene *scene, GSet *objects_gset)
{
  BLI_Iterator iter;
  scene_objects_iterator_begin(&iter, scene, objects_gset);
  while (iter.valid) {
    BKE_scene_objects_iterator_next(&iter);
  }

  /* `return_gset` is either given `objects_gset` (if non-NULL), or the GSet allocated by the
   * iterator. Either way, we want to get it back, and prevent `BKE_scene_objects_iterator_end`
   * from freeing it. */
  GSet *return_gset = ((SceneObjectsIteratorData *)iter.data)->visited;
  ((SceneObjectsIteratorData *)iter.data)->visited = NULL;
  BKE_scene_objects_iterator_end(&iter);

  return return_gset;
}

/** \} */
