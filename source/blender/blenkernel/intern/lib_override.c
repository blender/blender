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
 * The Original Code is Copyright (C) 2016 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "BKE_armature.h"
#include "BKE_collection.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "BLO_readfile.h"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_memarena.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "atomic_ops.h"

#define OVERRIDE_AUTO_CHECK_DELAY 0.2 /* 200ms between auto-override checks. */
//#define DEBUG_OVERRIDE_TIMEIT

#ifdef DEBUG_OVERRIDE_TIMEIT
#  include "PIL_time_utildefines.h"
#endif

static CLG_LogRef LOG = {"bke.liboverride"};

static void lib_override_library_property_copy(IDOverrideLibraryProperty *op_dst,
                                               IDOverrideLibraryProperty *op_src);
static void lib_override_library_property_operation_copy(
    IDOverrideLibraryPropertyOperation *opop_dst, IDOverrideLibraryPropertyOperation *opop_src);

static void lib_override_library_property_clear(IDOverrideLibraryProperty *op);
static void lib_override_library_property_operation_clear(
    IDOverrideLibraryPropertyOperation *opop);

/** Get override data for a given ID. Needed because of our beloved shape keys snowflake. */
BLI_INLINE IDOverrideLibrary *lib_override_get(Main *bmain, ID *id)
{
  if (id->flag & LIB_EMBEDDED_DATA_LIB_OVERRIDE) {
    const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id);
    if (id_type->owner_get != NULL) {
      return id_type->owner_get(bmain, id)->override_library;
    }
    BLI_assert_msg(0, "IDTypeInfo of liboverride-embedded ID with no owner getter");
  }
  return id->override_library;
}

IDOverrideLibrary *BKE_lib_override_library_init(ID *local_id, ID *reference_id)
{
  /* If reference_id is NULL, we are creating an override template for purely local data.
   * Else, reference *must* be linked data. */
  BLI_assert(reference_id == NULL || ID_IS_LINKED(reference_id));
  BLI_assert(local_id->override_library == NULL);

  ID *ancestor_id;
  for (ancestor_id = reference_id; ancestor_id != NULL && ancestor_id->override_library != NULL &&
                                   ancestor_id->override_library->reference != NULL;
       ancestor_id = ancestor_id->override_library->reference) {
    /* pass */
  }

  if (ancestor_id != NULL && ancestor_id->override_library != NULL) {
    /* Original ID has a template, use it! */
    BKE_lib_override_library_copy(local_id, ancestor_id, true);
    if (local_id->override_library->reference != reference_id) {
      id_us_min(local_id->override_library->reference);
      local_id->override_library->reference = reference_id;
      id_us_plus(local_id->override_library->reference);
    }
    return local_id->override_library;
  }

  /* Else, generate new empty override. */
  local_id->override_library = MEM_callocN(sizeof(*local_id->override_library), __func__);
  local_id->override_library->reference = reference_id;
  id_us_plus(local_id->override_library->reference);
  local_id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
  /* TODO: do we want to add tag or flag to referee to mark it as such? */
  return local_id->override_library;
}

void BKE_lib_override_library_copy(ID *dst_id, const ID *src_id, const bool do_full_copy)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY(src_id) || ID_IS_OVERRIDE_LIBRARY_TEMPLATE(src_id));

  if (dst_id->override_library != NULL) {
    if (src_id->override_library == NULL) {
      BKE_lib_override_library_free(&dst_id->override_library, true);
      return;
    }

    BKE_lib_override_library_clear(dst_id->override_library, true);
  }
  else if (src_id->override_library == NULL) {
    /* Virtual overrides of embedded data does not require any extra work. */
    return;
  }
  else {
    BKE_lib_override_library_init(dst_id, NULL);
  }

  /* If source is already overriding data, we copy it but reuse its reference for dest ID.
   * Otherwise, source is only an override template, it then becomes reference of dest ID. */
  dst_id->override_library->reference = src_id->override_library->reference ?
                                            src_id->override_library->reference :
                                            (ID *)src_id;
  id_us_plus(dst_id->override_library->reference);

  if (do_full_copy) {
    BLI_duplicatelist(&dst_id->override_library->properties,
                      &src_id->override_library->properties);
    for (IDOverrideLibraryProperty *op_dst = dst_id->override_library->properties.first,
                                   *op_src = src_id->override_library->properties.first;
         op_dst;
         op_dst = op_dst->next, op_src = op_src->next) {
      lib_override_library_property_copy(op_dst, op_src);
    }
  }

  dst_id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
}

void BKE_lib_override_library_clear(IDOverrideLibrary *override, const bool do_id_user)
{
  BLI_assert(override != NULL);

  if (!ELEM(NULL, override->runtime, override->runtime->rna_path_to_override_properties)) {
    BLI_ghash_clear(override->runtime->rna_path_to_override_properties, NULL, NULL);
  }

  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &override->properties) {
    lib_override_library_property_clear(op);
  }
  BLI_freelistN(&override->properties);

  if (do_id_user) {
    id_us_min(override->reference);
    /* override->storage should never be refcounted... */
  }
}

void BKE_lib_override_library_free(struct IDOverrideLibrary **override, const bool do_id_user)
{
  BLI_assert(*override != NULL);

  if ((*override)->runtime != NULL) {
    if ((*override)->runtime->rna_path_to_override_properties != NULL) {
      BLI_ghash_free((*override)->runtime->rna_path_to_override_properties, NULL, NULL);
    }
    MEM_SAFE_FREE((*override)->runtime);
  }

  BKE_lib_override_library_clear(*override, do_id_user);
  MEM_freeN(*override);
  *override = NULL;
}

static ID *lib_override_library_create_from(Main *bmain,
                                            ID *reference_id,
                                            const int lib_id_copy_flags)
{
  /* NOTE: We do not want to copy possible override data from reference here (whether it is an
   * override template, or already an override of some other ref data). */
  ID *local_id = BKE_id_copy_ex(bmain,
                                reference_id,
                                NULL,
                                LIB_ID_COPY_DEFAULT | LIB_ID_COPY_NO_LIB_OVERRIDE |
                                    lib_id_copy_flags);

  if (local_id == NULL) {
    return NULL;
  }
  id_us_min(local_id);

  BKE_lib_override_library_init(local_id, reference_id);

  /* NOTE: From liboverride perspective (and RNA one), shape keys are considered as local embedded
   * data-blocks, just like root node trees or master collections. Therefore, we never need to
   * create overrides for them. We need a way to mark them as overrides though. */
  Key *reference_key;
  if ((reference_key = BKE_key_from_id(reference_id)) != NULL) {
    Key *local_key = BKE_key_from_id(local_id);
    BLI_assert(local_key != NULL);
    local_key->id.flag |= LIB_EMBEDDED_DATA_LIB_OVERRIDE;
  }

  return local_id;
}

/* TODO: This could be simplified by storing a flag in #IDOverrideLibrary
 * during the diffing process? */
bool BKE_lib_override_library_is_user_edited(struct ID *id)
{

  if (!ID_IS_OVERRIDE_LIBRARY(id)) {
    return false;
  }

  /* A bit weird, but those embedded IDs are handled by their owner ID anyway, so we can just
   * assume they are never user-edited, actual proper detection will happen from their owner check.
   */
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
    return false;
  }

  LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &id->override_library->properties) {
    LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
      if ((opop->flag & IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) != 0) {
        continue;
      }
      if (opop->operation == IDOVERRIDE_LIBRARY_OP_NOOP) {
        continue;
      }
      /* If an operation does not match the filters above, it is considered as a user-editing one,
       * therefore this override is user-edited. */
      return true;
    }
  }
  return false;
}

ID *BKE_lib_override_library_create_from_id(Main *bmain,
                                            ID *reference_id,
                                            const bool do_tagged_remap)
{
  BLI_assert(reference_id != NULL);
  BLI_assert(ID_IS_LINKED(reference_id));

  ID *local_id = lib_override_library_create_from(bmain, reference_id, 0);
  /* We cannot allow automatic hierarchy resync on this ID, it is highly likely to generate a giant
   * mess in case there are a lot of hidden, non-instantiated, non-properly organized dependencies.
   * Ref T94650. */
  local_id->override_library->flag |= IDOVERRIDE_LIBRARY_FLAG_NO_HIERARCHY;

  if (do_tagged_remap) {
    Key *reference_key, *local_key = NULL;
    if ((reference_key = BKE_key_from_id(reference_id)) != NULL) {
      local_key = BKE_key_from_id(local_id);
      BLI_assert(local_key != NULL);
    }

    ID *other_id;
    FOREACH_MAIN_ID_BEGIN (bmain, other_id) {
      if ((other_id->tag & LIB_TAG_DOIT) != 0 && !ID_IS_LINKED(other_id)) {
        /* Note that using ID_REMAP_SKIP_INDIRECT_USAGE below is superfluous, as we only remap
         * local IDs usages anyway. */
        BKE_libblock_relink_ex(bmain,
                               other_id,
                               reference_id,
                               local_id,
                               ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);
        if (reference_key != NULL) {
          BKE_libblock_relink_ex(bmain,
                                 other_id,
                                 &reference_key->id,
                                 &local_key->id,
                                 ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_OVERRIDE_LIBRARY);
        }
      }
    }
    FOREACH_MAIN_ID_END;
  }

  return local_id;
}

bool BKE_lib_override_library_create_from_tag(Main *bmain,
                                              const Library *reference_library,
                                              const bool do_no_main)
{
  ID *reference_id;
  bool success = true;

  ListBase todo_ids = {NULL};
  LinkData *todo_id_iter;

  /* Get all IDs we want to override. */
  FOREACH_MAIN_ID_BEGIN (bmain, reference_id) {
    if ((reference_id->tag & LIB_TAG_DOIT) != 0 && reference_id->lib == reference_library &&
        BKE_idtype_idcode_is_linkable(GS(reference_id->name))) {
      todo_id_iter = MEM_callocN(sizeof(*todo_id_iter), __func__);
      todo_id_iter->data = reference_id;
      BLI_addtail(&todo_ids, todo_id_iter);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Override the IDs. */
  for (todo_id_iter = todo_ids.first; todo_id_iter != NULL; todo_id_iter = todo_id_iter->next) {
    reference_id = todo_id_iter->data;

    /* If `newid` is already set, assume it has been handled by calling code.
     * Only current use case: re-using proxy ID when converting to liboverride. */
    if (reference_id->newid == NULL) {
      /* NOTE: `no main` case is used during resync procedure, to support recursive resync.
       * This requires extra care further down the resync process,
       * see: #BKE_lib_override_library_resync. */
      reference_id->newid = lib_override_library_create_from(
          bmain, reference_id, do_no_main ? LIB_ID_CREATE_NO_MAIN : 0);
      if (reference_id->newid == NULL) {
        success = false;
        break;
      }
    }
    /* We also tag the new IDs so that in next step we can remap their pointers too. */
    reference_id->newid->tag |= LIB_TAG_DOIT;

    Key *reference_key;
    if ((reference_key = BKE_key_from_id(reference_id)) != NULL) {
      reference_key->id.tag |= LIB_TAG_DOIT;

      Key *local_key = BKE_key_from_id(reference_id->newid);
      BLI_assert(local_key != NULL);
      reference_key->id.newid = &local_key->id;
      /* We also tag the new IDs so that in next step we can remap their pointers too. */
      local_key->id.tag |= LIB_TAG_DOIT;
    }
  }

  /* Only remap new local ID's pointers, we don't want to force our new overrides onto our whole
   * existing linked IDs usages. */
  if (success) {
    for (todo_id_iter = todo_ids.first; todo_id_iter != NULL; todo_id_iter = todo_id_iter->next) {
      reference_id = todo_id_iter->data;
      ID *local_id = reference_id->newid;

      if (local_id == NULL) {
        continue;
      }

      Key *reference_key, *local_key = NULL;
      if ((reference_key = BKE_key_from_id(reference_id)) != NULL) {
        local_key = BKE_key_from_id(reference_id->newid);
        BLI_assert(local_key != NULL);
      }

      /* Still checking the whole Main, that way we can tag other local IDs as needing to be
       * remapped to use newly created overriding IDs, if needed. */
      ID *id;
      FOREACH_MAIN_ID_BEGIN (bmain, id) {
        ID *other_id;
        /* In case we created new overrides as 'no main', they are not accessible directly in this
         * loop, but we can get to them through their reference's `newid` pointer. */
        if (do_no_main && id->lib == reference_id->lib && id->newid != NULL) {
          other_id = id->newid;
          /* Otherwise we cannot properly distinguish between IDs that are actually from the
           * linked library (and should not be remapped), and IDs that are overrides re-generated
           * from the reference from the linked library, and must therefore be remapped.
           *
           * This is reset afterwards at the end of this loop. */
          other_id->lib = NULL;
        }
        else {
          other_id = id;
        }

        /* If other ID is a linked one, but not from the same library as our reference, then we
         * consider we should also remap it, as part of recursive resync. */
        if ((other_id->tag & LIB_TAG_DOIT) != 0 && other_id->lib != reference_id->lib &&
            other_id != local_id) {
          BKE_libblock_relink_ex(bmain,
                                 other_id,
                                 reference_id,
                                 local_id,
                                 ID_REMAP_SKIP_OVERRIDE_LIBRARY | ID_REMAP_FORCE_USER_REFCOUNT);
          if (reference_key != NULL) {
            BKE_libblock_relink_ex(bmain,
                                   other_id,
                                   &reference_key->id,
                                   &local_key->id,
                                   ID_REMAP_SKIP_OVERRIDE_LIBRARY | ID_REMAP_FORCE_USER_REFCOUNT);
          }
        }
        if (other_id != id) {
          other_id->lib = reference_id->lib;
        }
      }
      FOREACH_MAIN_ID_END;
    }
  }
  else {
    /* We need to cleanup potentially already created data. */
    for (todo_id_iter = todo_ids.first; todo_id_iter != NULL; todo_id_iter = todo_id_iter->next) {
      reference_id = todo_id_iter->data;
      BKE_id_delete(bmain, reference_id->newid);
      reference_id->newid = NULL;
    }
  }

  BLI_freelistN(&todo_ids);

  return success;
}

typedef struct LibOverrideGroupTagData {
  Main *bmain;
  Scene *scene;
  ID *id_root;
  uint tag;
  uint missing_tag;
  /* Whether we are looping on override data, or their references (linked) one. */
  bool is_override;
  /* Whether we are creating new override, or resyncing existing one. */
  bool is_resync;

  /* Mapping linked objects to all their instantiating collections (as a linked list).
   * Avoids calling #BKE_collection_object_find over and over, this function is very expansive. */
  GHash *linked_object_to_instantiating_collections;
  MemArena *mem_arena;
} LibOverrideGroupTagData;

static void lib_override_group_tag_data_object_to_collection_init_collection_process(
    LibOverrideGroupTagData *data, Collection *collection)
{
  LISTBASE_FOREACH (CollectionObject *, collection_object, &collection->gobject) {
    Object *ob = collection_object->ob;
    if (!ID_IS_LINKED(ob)) {
      continue;
    }

    LinkNodePair **collections_linkedlist_p;
    if (!BLI_ghash_ensure_p(data->linked_object_to_instantiating_collections,
                            ob,
                            (void ***)&collections_linkedlist_p)) {
      *collections_linkedlist_p = BLI_memarena_calloc(data->mem_arena,
                                                      sizeof(**collections_linkedlist_p));
    }
    BLI_linklist_append_arena(*collections_linkedlist_p, collection, data->mem_arena);
  }
}

/* Initialize complex data, `data` is expected to be already initialized with basic pointers and
 * other simple data.
 *
 * NOTE: Currently creates a mapping from linked object to all of their instantiating collections
 * (as returned by #BKE_collection_object_find). */
static void lib_override_group_tag_data_object_to_collection_init(LibOverrideGroupTagData *data)
{
  data->mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  data->linked_object_to_instantiating_collections = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  if (data->scene != NULL) {
    lib_override_group_tag_data_object_to_collection_init_collection_process(
        data, data->scene->master_collection);
  }
  LISTBASE_FOREACH (Collection *, collection, &data->bmain->collections) {
    lib_override_group_tag_data_object_to_collection_init_collection_process(data, collection);
  }
}

static void lib_override_group_tag_data_clear(LibOverrideGroupTagData *data)
{
  BLI_ghash_free(data->linked_object_to_instantiating_collections, NULL, NULL);
  BLI_memarena_free(data->mem_arena);
  memset(data, 0, sizeof(*data));
}

/* Tag all IDs in dependency relationships within an override hierarchy/group.
 *
 * Requires existing `Main.relations`.
 *
 * NOTE: This is typically called to complete `lib_override_linked_group_tag()`.
 */
static bool lib_override_hierarchy_dependencies_recursive_tag(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id = data->id_root;
  const bool is_override = data->is_override;

  MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers, id);
  BLI_assert(entry != NULL);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return (*(uint *)&id->tag & data->tag) != 0;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != NULL;
       to_id_entry = to_id_entry->next) {
    if ((to_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) != 0) {
      /* Never consider non-overridable relationships ('from', 'parents', 'owner' etc. pointers) as
       * actual dependencies. */
      continue;
    }
    /* We only consider IDs from the same library. */
    ID *to_id = *to_id_entry->id_pointer.to;
    if (to_id == NULL || to_id->lib != id->lib ||
        (is_override && !ID_IS_OVERRIDE_LIBRARY(to_id))) {
      /* IDs from different libraries, or non-override IDs in case we are processing overrides, are
       * both barriers of dependency. */
      continue;
    }
    LibOverrideGroupTagData sub_data = *data;
    sub_data.id_root = to_id;
    if (lib_override_hierarchy_dependencies_recursive_tag(&sub_data)) {
      id->tag |= data->tag;
    }
  }

  return (*(uint *)&id->tag & data->tag) != 0;
}

static void lib_override_linked_group_tag_recursive(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_owner = data->id_root;
  BLI_assert(ID_IS_LINKED(id_owner));
  BLI_assert(!data->is_override);
  const uint tag = data->tag;
  const uint missing_tag = data->missing_tag;

  MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers,
                                                 id_owner);
  BLI_assert(entry != NULL);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != NULL;
       to_id_entry = to_id_entry->next) {
    if ((to_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) != 0) {
      /* Never consider non-overridable relationships as actual dependencies. */
      continue;
    }

    ID *to_id = *to_id_entry->id_pointer.to;
    if (ELEM(to_id, NULL, id_owner)) {
      continue;
    }
    /* We only consider IDs from the same library. */
    if (to_id->lib != id_owner->lib) {
      continue;
    }
    BLI_assert(ID_IS_LINKED(to_id));

    /* We tag all collections and objects for override. And we also tag all other data-blocks which
     * would use one of those.
     * NOTE: missing IDs (aka placeholders) are never overridden. */
    if (ELEM(GS(to_id->name), ID_OB, ID_GR)) {
      if (to_id->tag & LIB_TAG_MISSING) {
        to_id->tag |= missing_tag;
      }
      else {
        to_id->tag |= tag;
      }
    }

    /* Recursively process the dependencies. */
    LibOverrideGroupTagData sub_data = *data;
    sub_data.id_root = to_id;
    lib_override_linked_group_tag_recursive(&sub_data);
  }
}

static void lib_override_linked_group_tag_clear_boneshapes_objects(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;

  /* Remove (untag) bone shape objects, they shall never need to be to directly/explicitly
   * overridden. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type == OB_ARMATURE && ob->pose != NULL && (ob->id.tag & data->tag)) {
      for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan != NULL; pchan = pchan->next) {
        if (pchan->custom != NULL) {
          pchan->custom->id.tag &= ~data->tag;
        }
      }
    }
  }

  /* Remove (untag) collections if they do not own any tagged object (either themselves, or in
   * their children collections). */
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if ((collection->id.tag & data->tag) == 0) {
      continue;
    }
    bool keep_tagged = false;
    const ListBase object_bases = BKE_collection_object_cache_get(collection);
    LISTBASE_FOREACH (Base *, base, &object_bases) {
      if ((base->object->id.tag & data->tag) != 0) {
        keep_tagged = true;
        break;
      }
    }
    if (!keep_tagged) {
      collection->id.tag &= ~data->tag;
    }
  }
}

/* This will tag at least all 'boundary' linked IDs for a potential override group.
 *
 * Requires existing `Main.relations`.
 *
 * Note that you will then need to call #lib_override_hierarchy_dependencies_recursive_tag to
 * complete tagging of all dependencies within the override group.
 *
 * We currently only consider Collections and Objects (that are not used as bone shapes) as valid
 * boundary IDs to define an override group.
 */
static void lib_override_linked_group_tag(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_root = data->id_root;
  const bool is_resync = data->is_resync;
  BLI_assert(!data->is_override);

  if (id_root->tag & LIB_TAG_MISSING) {
    id_root->tag |= data->missing_tag;
  }
  else {
    id_root->tag |= data->tag;
  }

  /* Only objects and groups are currently considered as 'keys' in override hierarchies. */
  if (!ELEM(GS(id_root->name), ID_OB, ID_GR)) {
    return;
  }

  /* Tag all collections and objects recursively. */
  lib_override_linked_group_tag_recursive(data);

  /* Do not override objects used as bone shapes, nor their collections if possible. */
  lib_override_linked_group_tag_clear_boneshapes_objects(data);

  /* For each object tagged for override, ensure we get at least one local or liboverride
   * collection to host it. Avoids getting a bunch of random object in the scene's master
   * collection when all objects' dependencies are not properly 'packed' into a single root
   * collection. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ID_IS_LINKED(ob) && (ob->id.tag & data->tag) != 0) {
      Collection *instantiating_collection = NULL;
      Collection *instantiating_collection_override_candidate = NULL;
      /* Loop over all collections instantiating the object, if we already have a 'locale' one we
       * have nothing to do, otherwise try to find a 'linked' one that we can override too. */
      LinkNodePair *instantiating_collection_linklist = BLI_ghash_lookup(
          data->linked_object_to_instantiating_collections, ob);
      if (instantiating_collection_linklist != NULL) {
        for (LinkNode *instantiating_collection_linknode = instantiating_collection_linklist->list;
             instantiating_collection_linknode != NULL;
             instantiating_collection_linknode = instantiating_collection_linknode->next) {
          instantiating_collection = instantiating_collection_linknode->link;
          /* In (recursive) resync case, if a collection of a 'parent' lib instantiates the
           * linked object, it is also fine. */
          if (!ID_IS_LINKED(instantiating_collection) ||
              (is_resync && ID_IS_LINKED(id_root) &&
               instantiating_collection->id.lib->temp_index < id_root->lib->temp_index)) {
            break;
          }
          if (ID_IS_LINKED(instantiating_collection) &&
              (!is_resync || instantiating_collection->id.lib == id_root->lib)) {
            instantiating_collection_override_candidate = instantiating_collection;
          }
          instantiating_collection = NULL;
        }
      }

      if (instantiating_collection == NULL &&
          instantiating_collection_override_candidate != NULL) {
        if (instantiating_collection_override_candidate->id.tag & LIB_TAG_MISSING) {
          instantiating_collection_override_candidate->id.tag |= data->missing_tag;
        }
        else {
          instantiating_collection_override_candidate->id.tag |= data->tag;
        }
      }
    }
  }
}

static void lib_override_overrides_group_tag_recursive(LibOverrideGroupTagData *data)
{
  Main *bmain = data->bmain;
  ID *id_owner = data->id_root;
  BLI_assert(ID_IS_OVERRIDE_LIBRARY(id_owner));
  BLI_assert(data->is_override);

  if (ID_IS_OVERRIDE_LIBRARY_REAL(id_owner) &&
      (id_owner->override_library->flag & IDOVERRIDE_LIBRARY_FLAG_NO_HIERARCHY) != 0) {
    return;
  }

  const uint tag = data->tag;
  const uint missing_tag = data->missing_tag;

  MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers,
                                                 id_owner);
  BLI_assert(entry != NULL);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != NULL;
       to_id_entry = to_id_entry->next) {
    if ((to_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) != 0) {
      /* Never consider non-overridable relationships as actual dependencies. */
      continue;
    }

    ID *to_id = *to_id_entry->id_pointer.to;
    if (ELEM(to_id, NULL, id_owner)) {
      continue;
    }
    if (!ID_IS_OVERRIDE_LIBRARY(to_id) || (to_id->lib != id_owner->lib)) {
      continue;
    }

    Library *reference_lib = lib_override_get(bmain, id_owner)->reference->lib;
    ID *to_id_reference = lib_override_get(bmain, to_id)->reference;
    if (to_id_reference->lib != reference_lib) {
      /* We do not override data-blocks from other libraries, nor do we process them. */
      continue;
    }

    if (to_id_reference->tag & LIB_TAG_MISSING) {
      to_id->tag |= missing_tag;
    }
    else {
      to_id->tag |= tag;
    }

    /* Recursively process the dependencies. */
    LibOverrideGroupTagData sub_data = *data;
    sub_data.id_root = to_id;
    lib_override_overrides_group_tag_recursive(&sub_data);
  }
}

/* This will tag all override IDs of an override group defined by the given `id_root`. */
static void lib_override_overrides_group_tag(LibOverrideGroupTagData *data)
{
  ID *id_root = data->id_root;
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));
  BLI_assert(data->is_override);

  if (id_root->override_library->reference->tag & LIB_TAG_MISSING) {
    id_root->tag |= data->missing_tag;
  }
  else {
    id_root->tag |= data->tag;
  }

  /* Tag all local overrides in id_root's group. */
  lib_override_overrides_group_tag_recursive(data);
}

static bool lib_override_library_create_do(Main *bmain, Scene *scene, ID *id_root)
{
  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data = {.bmain = bmain,
                                  .scene = scene,
                                  .id_root = id_root,
                                  .tag = LIB_TAG_DOIT,
                                  .missing_tag = LIB_TAG_MISSING,
                                  .is_override = false,
                                  .is_resync = false};
  lib_override_group_tag_data_object_to_collection_init(&data);
  lib_override_linked_group_tag(&data);

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  lib_override_hierarchy_dependencies_recursive_tag(&data);

  BKE_main_relations_free(bmain);
  lib_override_group_tag_data_clear(&data);

  return BKE_lib_override_library_create_from_tag(bmain, id_root->lib, false);
}

static void lib_override_library_create_post_process(Main *bmain,
                                                     Scene *scene,
                                                     ViewLayer *view_layer,
                                                     ID *id_root,
                                                     ID *id_reference,
                                                     Collection *residual_storage,
                                                     const bool is_resync)
{
  /* NOTE: We only care about local IDs here, if a linked object is not instantiated in any way we
   * do not do anything about it. */

  /* We need to use the `_remap` version here as we prevented any LayerCollection resync during the
   * whole liboverride resyncing, which involves a lot of ID remapping.
   *
   * Otherwise, cached Base GHash e.g. can contain invalid stale data. */
  BKE_main_collection_sync_remap(bmain);

  /* We create a set of all objects referenced into the scene by its hierarchy of collections.
   * NOTE: This is different that the list of bases, since objects in excluded collections etc.
   * won't have a base, but are still considered as instanced from our point of view. */
  GSet *all_objects_in_scene = BKE_scene_objects_as_gset(scene, NULL);

  /* Instantiating the root collection or object should never be needed in resync case, since the
   * old override would be remapped to the new one. */
  if (!is_resync && id_root != NULL && id_root->newid != NULL && !ID_IS_LINKED(id_root->newid)) {
    switch (GS(id_root->name)) {
      case ID_GR: {
        Object *ob_reference = id_reference != NULL && GS(id_reference->name) == ID_OB ?
                                   (Object *)id_reference :
                                   NULL;
        Collection *collection_new = ((Collection *)id_root->newid);
        if (is_resync && BKE_collection_is_in_scene(collection_new)) {
          break;
        }
        if (ob_reference != NULL) {
          BKE_collection_add_from_object(bmain, scene, ob_reference, collection_new);
        }
        else if (id_reference != NULL) {
          BLI_assert(GS(id_reference->name) == ID_GR);
          BKE_collection_add_from_collection(
              bmain, scene, ((Collection *)id_reference), collection_new);
        }
        else {
          BKE_collection_add_from_collection(
              bmain, scene, ((Collection *)id_root), collection_new);
        }

        BLI_assert(BKE_collection_is_in_scene(collection_new));

        all_objects_in_scene = BKE_scene_objects_as_gset(scene, all_objects_in_scene);
        break;
      }
      case ID_OB: {
        Object *ob_new = (Object *)id_root->newid;
        if (BLI_gset_lookup(all_objects_in_scene, ob_new) == NULL) {
          BKE_collection_object_add_from(bmain, scene, (Object *)id_root, ob_new);
          all_objects_in_scene = BKE_scene_objects_as_gset(scene, all_objects_in_scene);
        }
        break;
      }
      default:
        break;
    }
  }

  /* We need to ensure all new overrides of objects are properly instantiated. */
  Collection *default_instantiating_collection = residual_storage;
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    Object *ob_new = (Object *)ob->id.newid;
    if (ob_new == NULL || ID_IS_LINKED(ob_new)) {
      continue;
    }

    BLI_assert(ob_new->id.override_library != NULL &&
               ob_new->id.override_library->reference == &ob->id);

    if (BLI_gset_lookup(all_objects_in_scene, ob_new) == NULL) {
      if (id_root != NULL && default_instantiating_collection == NULL) {
        ID *id_ref = id_root->newid != NULL ? id_root->newid : id_root;
        switch (GS(id_ref->name)) {
          case ID_GR: {
            /* Adding the object to a specific collection outside of the root overridden one is a
             * fairly bad idea (it breaks the override hierarchy concept). But there is no other
             * way to do this currently (we cannot add new collections to overridden root one,
             * this is not currently supported).
             * Since that will be fairly annoying and noisy, only do that in case the override
             * object is not part of any existing collection (i.e. its user count is 0). In
             * practice this should never happen I think. */
            if (ID_REAL_USERS(ob_new) != 0) {
              continue;
            }
            default_instantiating_collection = BKE_collection_add(
                bmain, (Collection *)id_root, "OVERRIDE_HIDDEN");
            /* Hide the collection from viewport and render. */
            default_instantiating_collection->flag |= COLLECTION_HIDE_VIEWPORT |
                                                      COLLECTION_HIDE_RENDER;
            break;
          }
          case ID_OB: {
            /* Add the other objects to one of the collections instantiating the
             * root object, or scene's master collection if none found. */
            Object *ob_ref = (Object *)id_ref;
            LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
              if (BKE_collection_has_object(collection, ob_ref) &&
                  (view_layer != NULL ?
                       BKE_view_layer_has_collection(view_layer, collection) :
                       BKE_collection_has_collection(scene->master_collection, collection)) &&
                  !ID_IS_LINKED(collection) && !ID_IS_OVERRIDE_LIBRARY(collection)) {
                default_instantiating_collection = collection;
              }
            }
            break;
          }
          default:
            BLI_assert(0);
        }
      }
      if (default_instantiating_collection == NULL) {
        default_instantiating_collection = scene->master_collection;
      }

      BKE_collection_object_add(bmain, default_instantiating_collection, ob_new);
      DEG_id_tag_update_ex(bmain, &ob_new->id, ID_RECALC_TRANSFORM | ID_RECALC_BASE_FLAGS);
    }
  }

  BLI_gset_free(all_objects_in_scene, NULL);
}

bool BKE_lib_override_library_create(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     ID *id_root,
                                     ID *id_reference,
                                     ID **r_id_root_override)
{
  if (r_id_root_override != NULL) {
    *r_id_root_override = NULL;
  }

  const bool success = lib_override_library_create_do(bmain, scene, id_root);

  if (!success) {
    return success;
  }

  if (r_id_root_override != NULL) {
    *r_id_root_override = id_root->newid;
  }

  lib_override_library_create_post_process(
      bmain, scene, view_layer, id_root, id_reference, NULL, false);

  /* Cleanup. */
  BKE_main_id_newptr_and_tag_clear(bmain);
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

  /* We need to rebuild some of the deleted override rules (for UI feedback purpose). */
  BKE_lib_override_library_main_operations_create(bmain, true);

  return success;
}

bool BKE_lib_override_library_template_create(struct ID *id)
{
  if (ID_IS_LINKED(id)) {
    return false;
  }
  if (ID_IS_OVERRIDE_LIBRARY(id)) {
    return false;
  }

  BKE_lib_override_library_init(id, NULL);
  return true;
}

bool BKE_lib_override_library_proxy_convert(Main *bmain,
                                            Scene *scene,
                                            ViewLayer *view_layer,
                                            Object *ob_proxy)
{
  /* `proxy_group`, if defined, is the empty instantiating the collection from which the proxy is
   * coming. */
  Object *ob_proxy_group = ob_proxy->proxy_group;
  const bool is_override_instancing_object = ob_proxy_group != NULL;
  ID *id_root = is_override_instancing_object ? &ob_proxy_group->instance_collection->id :
                                                &ob_proxy->proxy->id;
  ID *id_reference = is_override_instancing_object ? &ob_proxy_group->id : &ob_proxy->id;

  /* In some cases the instance collection of a proxy object may be local (see e.g. T83875). Not
   * sure this is a valid state, but for now just abort the overriding process. */
  if (!ID_IS_OVERRIDABLE_LIBRARY(id_root)) {
    return false;
  }

  /* We manually convert the proxy object into a library override, further override handling will
   * then be handled by `BKE_lib_override_library_create()` just as for a regular override
   * creation.
   */
  ob_proxy->proxy->id.tag |= LIB_TAG_DOIT;
  ob_proxy->proxy->id.newid = &ob_proxy->id;
  BKE_lib_override_library_init(&ob_proxy->id, &ob_proxy->proxy->id);

  ob_proxy->proxy->proxy_from = NULL;
  ob_proxy->proxy = ob_proxy->proxy_group = NULL;

  DEG_id_tag_update(&ob_proxy->id, ID_RECALC_COPY_ON_WRITE);

  /* In case of proxy conversion, remap all local ID usages to linked IDs to their newly created
   * overrides.
   * While this might not be 100% the desired behavior, it is likely to be the case most of the
   * time. Ref: T91711. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (!ID_IS_LINKED(id_iter)) {
      id_iter->tag |= LIB_TAG_DOIT;
    }
  }
  FOREACH_MAIN_ID_END;

  return BKE_lib_override_library_create(bmain, scene, view_layer, id_root, id_reference, NULL);
}

static void lib_override_library_proxy_convert_do(Main *bmain,
                                                  Scene *scene,
                                                  Object *ob_proxy,
                                                  BlendFileReadReport *reports)
{
  Object *ob_proxy_group = ob_proxy->proxy_group;
  const bool is_override_instancing_object = ob_proxy_group != NULL;

  const bool success = BKE_lib_override_library_proxy_convert(bmain, scene, NULL, ob_proxy);

  if (success) {
    CLOG_INFO(&LOG,
              4,
              "Proxy object '%s' successfully converted to library overrides",
              ob_proxy->id.name);
    /* Remove the instance empty from this scene, the items now have an overridden collection
     * instead. */
    if (is_override_instancing_object) {
      BKE_scene_collections_object_remove(bmain, scene, ob_proxy_group, true);
    }
    reports->count.proxies_to_lib_overrides_success++;
  }
}

void BKE_lib_override_library_main_proxy_convert(Main *bmain, BlendFileReadReport *reports)
{
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    FOREACH_SCENE_OBJECT_BEGIN (scene, object) {
      if (object->proxy_group == NULL) {
        continue;
      }

      lib_override_library_proxy_convert_do(bmain, scene, object, reports);
    }
    FOREACH_SCENE_OBJECT_END;

    FOREACH_SCENE_OBJECT_BEGIN (scene, object) {
      if (object->proxy == NULL) {
        continue;
      }

      lib_override_library_proxy_convert_do(bmain, scene, object, reports);
    }
    FOREACH_SCENE_OBJECT_END;
  }

  LISTBASE_FOREACH (Object *, object, &bmain->objects) {
    if (ID_IS_LINKED(object)) {
      if (object->proxy != NULL) {
        CLOG_WARN(&LOG, "Did not try to convert linked proxy object '%s'", object->id.name);
        reports->count.linked_proxies++;
      }
      continue;
    }

    if (object->proxy_group != NULL || object->proxy != NULL) {
      CLOG_WARN(
          &LOG, "Proxy object '%s' failed to be converted to library override", object->id.name);
      reports->count.proxies_to_lib_overrides_failures++;
    }
  }
}

static void lib_override_library_remap(Main *bmain,
                                       const ID *id_root_reference,
                                       GHash *linkedref_to_old_override)
{
  ID *id;
  struct IDRemapper *remapper = BKE_id_remapper_create();
  FOREACH_MAIN_ID_BEGIN (bmain, id) {

    if (id->tag & LIB_TAG_DOIT && id->newid != NULL && id->lib == id_root_reference->lib) {
      ID *id_override_new = id->newid;
      ID *id_override_old = BLI_ghash_lookup(linkedref_to_old_override, id);
      if (id_override_old == NULL) {
        continue;
      }

      BKE_id_remapper_add(remapper, id_override_old, id_override_new);
      /* Remap no-main override IDs we just created too. */
      GHashIterator linkedref_to_old_override_iter;
      GHASH_ITER (linkedref_to_old_override_iter, linkedref_to_old_override) {
        ID *id_override_old_iter = BLI_ghashIterator_getValue(&linkedref_to_old_override_iter);
        if ((id_override_old_iter->tag & LIB_TAG_NO_MAIN) == 0) {
          continue;
        }

        BKE_libblock_relink_ex(bmain,
                               id_override_old_iter,
                               id_override_old,
                               id_override_new,
                               ID_REMAP_FORCE_USER_REFCOUNT | ID_REMAP_FORCE_NEVER_NULL_USAGE);
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Remap all IDs to use the new override. */
  BKE_libblock_remap_multiple(bmain, remapper, 0);
  BKE_id_remapper_free(remapper);
}

static bool lib_override_library_resync(Main *bmain,
                                        Scene *scene,
                                        ViewLayer *view_layer,
                                        ID *id_root,
                                        Collection *override_resync_residual_storage,
                                        const bool do_hierarchy_enforce,
                                        const bool do_post_process,
                                        BlendFileReadReport *reports)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));

  ID *id_root_reference = id_root->override_library->reference;

  if (id_root_reference->tag & LIB_TAG_MISSING) {
    BKE_reportf(reports != NULL ? reports->reports : NULL,
                RPT_ERROR,
                "Impossible to resync data-block %s and its dependencies, as its linked reference "
                "is missing",
                id_root->name + 2);
    return false;
  }

  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data = {.bmain = bmain,
                                  .scene = scene,
                                  .id_root = id_root,
                                  .tag = LIB_TAG_DOIT,
                                  .missing_tag = LIB_TAG_MISSING,
                                  .is_override = true,
                                  .is_resync = true};
  lib_override_group_tag_data_object_to_collection_init(&data);
  lib_override_overrides_group_tag(&data);

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  data.id_root = id_root_reference;
  data.is_override = false;
  lib_override_linked_group_tag(&data);

  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  lib_override_hierarchy_dependencies_recursive_tag(&data);

  /* Make a mapping 'linked reference IDs' -> 'Local override IDs' of existing overrides. */
  GHash *linkedref_to_old_override = BLI_ghash_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    /* IDs that get fully removed from linked data remain as local overrides (using place-holder
     * linked IDs as reference), but they are often not reachable from any current valid local
     * override hierarchy anymore. This will ensure they get properly deleted at the end of this
     * function. */
    if (!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
        (id->override_library->reference->tag & LIB_TAG_MISSING) != 0 &&
        /* Unfortunately deleting obdata means deleting their objects too. Since there is no
         * guarantee that a valid override object using an obsolete override obdata gets properly
         * updated, we ignore those here for now. In practice this should not be a big issue. */
        !OB_DATA_SUPPORT_ID(GS(id->name))) {
      id->tag |= LIB_TAG_MISSING;
    }

    if (id->tag & LIB_TAG_DOIT && (id->lib == id_root->lib) && ID_IS_OVERRIDE_LIBRARY(id)) {
      /* While this should not happen in typical cases (and won't be properly supported here), user
       * is free to do all kind of very bad things, including having different local overrides of a
       * same linked ID in a same hierarchy. */
      IDOverrideLibrary *id_override_library = lib_override_get(bmain, id);
      ID *reference_id = id_override_library->reference;
      if (GS(reference_id->name) != GS(id->name)) {
        switch (GS(id->name)) {
          case ID_KE:
            reference_id = (ID *)BKE_key_from_id(reference_id);
            break;
          case ID_GR:
            BLI_assert(GS(reference_id->name) == ID_SCE);
            reference_id = (ID *)((Scene *)reference_id)->master_collection;
            break;
          case ID_NT:
            reference_id = (ID *)ntreeFromID(id);
            break;
          default:
            break;
        }
      }
      BLI_assert(GS(reference_id->name) == GS(id->name));

      if (!BLI_ghash_haskey(linkedref_to_old_override, reference_id)) {
        BLI_ghash_insert(linkedref_to_old_override, reference_id, id);
        if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
          continue;
        }
        if ((id->override_library->reference->tag & LIB_TAG_DOIT) == 0) {
          /* We have an override, but now it does not seem to be necessary to override that ID
           * anymore. Check if there are some actual overrides from the user, otherwise assume
           * that we can get rid of this local override. */
          LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &id->override_library->properties) {
            if (!ELEM(op->rna_prop_type, PROP_POINTER, PROP_COLLECTION)) {
              id->override_library->reference->tag |= LIB_TAG_DOIT;
              break;
            }

            bool do_break = false;
            LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
              if ((opop->flag & IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) == 0) {
                id->override_library->reference->tag |= LIB_TAG_DOIT;
                do_break = true;
                break;
              }
            }
            if (do_break) {
              break;
            }
          }
        }
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Code above may have added some tags, we need to update this too. */
  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  lib_override_hierarchy_dependencies_recursive_tag(&data);

  BKE_main_relations_free(bmain);
  lib_override_group_tag_data_clear(&data);

  /* Make new override from linked data. */
  /* Note that this call also remaps all pointers of tagged IDs from old override IDs to new
   * override IDs (including within the old overrides themselves, since those are tagged too
   * above). */
  const bool success = BKE_lib_override_library_create_from_tag(
      bmain, id_root_reference->lib, true);

  if (!success) {
    return success;
  }

  ListBase *lb;
  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
      if (id->tag & LIB_TAG_DOIT && id->newid != NULL && id->lib == id_root_reference->lib) {
        ID *id_override_new = id->newid;
        ID *id_override_old = BLI_ghash_lookup(linkedref_to_old_override, id);

        BLI_assert((id_override_new->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC) == 0);

        /* We need to 'move back' newly created override into its proper library (since it was
         * duplicated from the reference ID with 'no main' option, it should currently be the same
         * as the reference ID one). */
        BLI_assert(/*!ID_IS_LINKED(id_override_new) || */ id_override_new->lib == id->lib);
        BLI_assert(id_override_old == NULL || id_override_old->lib == id_root->lib);
        id_override_new->lib = id_root->lib;
        /* Remap step below will tag directly linked ones properly as needed. */
        if (ID_IS_LINKED(id_override_new)) {
          id_override_new->tag |= LIB_TAG_INDIRECT;
        }

        if (id_override_old != NULL) {
          /* Swap the names between old override ID and new one. */
          char id_name_buf[MAX_ID_NAME];
          memcpy(id_name_buf, id_override_old->name, sizeof(id_name_buf));
          memcpy(id_override_old->name, id_override_new->name, sizeof(id_override_old->name));
          memcpy(id_override_new->name, id_name_buf, sizeof(id_override_new->name));

          BLI_insertlinkreplace(lb, id_override_old, id_override_new);
          id_override_old->tag |= LIB_TAG_NO_MAIN;
          id_override_new->tag &= ~LIB_TAG_NO_MAIN;

          if (ID_IS_OVERRIDE_LIBRARY_REAL(id_override_new)) {
            BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_override_old));

            /* Copy over overrides rules from old override ID to new one. */
            BLI_duplicatelist(&id_override_new->override_library->properties,
                              &id_override_old->override_library->properties);
            IDOverrideLibraryProperty *op_new =
                id_override_new->override_library->properties.first;
            IDOverrideLibraryProperty *op_old =
                id_override_old->override_library->properties.first;
            for (; op_new; op_new = op_new->next, op_old = op_old->next) {
              lib_override_library_property_copy(op_new, op_old);
            }
          }
        }
        else {
          /* Add to proper main list, ensure unique name for local ID, sort, and clear relevant
           * tags. */
          BKE_libblock_management_main_add(bmain, id_override_new);
        }
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;

  /* We remap old to new override usages in a separate loop, after all new overrides have
   * been added to Main. */
  lib_override_library_remap(bmain, id_root_reference, linkedref_to_old_override);

  BKE_main_collection_sync(bmain);

  /* We need to apply override rules in a separate loop, after all ID pointers have been properly
   * remapped, and all new local override IDs have gotten their proper original names, otherwise
   * override operations based on those ID names would fail. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->tag & LIB_TAG_DOIT && id->newid != NULL && id->lib == id_root_reference->lib) {
      ID *id_override_new = id->newid;
      if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_override_new)) {
        continue;
      }
      ID *id_override_old = BLI_ghash_lookup(linkedref_to_old_override, id);

      if (id_override_old == NULL) {
        continue;
      }
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id_override_old)) {
        /* Apply rules on new override ID using old one as 'source' data. */
        /* Note that since we already remapped ID pointers in old override IDs to new ones, we
         * can also apply ID pointer override rules safely here. */
        PointerRNA rnaptr_src, rnaptr_dst;
        RNA_id_pointer_create(id_override_old, &rnaptr_src);
        RNA_id_pointer_create(id_override_new, &rnaptr_dst);

        /* We remove any operation tagged with `IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE`,
         * that way the potentially new pointer will be properly kept, when old one is still valid
         * too (typical case: assigning new ID to some usage, while old one remains used elsewhere
         * in the override hierarchy). */
        LISTBASE_FOREACH_MUTABLE (
            IDOverrideLibraryProperty *, op, &id_override_new->override_library->properties) {
          LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
            if (opop->flag & IDOVERRIDE_LIBRARY_FLAG_IDPOINTER_MATCH_REFERENCE) {
              lib_override_library_property_operation_clear(opop);
              BLI_freelinkN(&op->operations, opop);
            }
          }
          if (BLI_listbase_is_empty(&op->operations)) {
            BKE_lib_override_library_property_delete(id_override_new->override_library, op);
          }
        }

        RNA_struct_override_apply(bmain,
                                  &rnaptr_dst,
                                  &rnaptr_src,
                                  NULL,
                                  id_override_new->override_library,
                                  do_hierarchy_enforce ?
                                      RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS :
                                      RNA_OVERRIDE_APPLY_FLAG_NOP);
      }

      /* Once overrides have been properly 'transferred' from old to new ID, we can clear ID usages
       * of the old one.
       * This is necessary in case said old ID is not in Main anymore. */
      BKE_libblock_relink_ex(bmain,
                             id_override_old,
                             NULL,
                             NULL,
                             ID_REMAP_FORCE_USER_REFCOUNT | ID_REMAP_FORCE_NEVER_NULL_USAGE);
      id_override_old->tag |= LIB_TAG_NO_USER_REFCOUNT;
    }
  }
  FOREACH_MAIN_ID_END;

  /* Delete old override IDs.
   * Note that we have to use tagged group deletion here, since ID deletion also uses
   * LIB_TAG_DOIT. This improves performances anyway, so everything is fine. */
  int user_edited_overrides_deletion_count = 0;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->tag & LIB_TAG_DOIT) {
      /* Note that this works because linked IDs are always after local ones (including
       * overrides), so we will only ever tag an old override ID after we have already checked it
       * in this loop, hence we cannot untag it later. */
      if (id->newid != NULL && id->lib == id_root_reference->lib) {
        ID *id_override_old = BLI_ghash_lookup(linkedref_to_old_override, id);

        if (id_override_old != NULL) {
          id->newid->tag &= ~LIB_TAG_DOIT;
          id_override_old->tag |= LIB_TAG_DOIT;
          if (id_override_old->tag & LIB_TAG_NO_MAIN) {
            BKE_id_free(bmain, id_override_old);
          }
        }
      }
      id->tag &= ~LIB_TAG_DOIT;
    }
    /* Also deal with old overrides that went missing in new linked data - only for real local
     * overrides for now, not those who are linked. */
    else if (id->tag & LIB_TAG_MISSING && !ID_IS_LINKED(id)) {
      BLI_assert(ID_IS_OVERRIDE_LIBRARY(id));
      if (!BKE_lib_override_library_is_user_edited(id)) {
        /* If user never edited them, we can delete them. */
        id->tag |= LIB_TAG_DOIT;
        id->tag &= ~LIB_TAG_MISSING;
        CLOG_INFO(&LOG, 2, "Old override %s is being deleted", id->name);
      }
#if 0
      else {
        /* Otherwise, keep them, user needs to decide whether what to do with them. */
        BLI_assert((id->tag & LIB_TAG_DOIT) == 0);
        id_fake_user_set(id);
        id->flag |= LIB_LIB_OVERRIDE_RESYNC_LEFTOVER;
        CLOG_INFO(&LOG, 2, "Old override %s is being kept around as it was user-edited", id->name);
      }
#else
      else {
        /* Delete them nevertheless, with fat warning, user needs to decide whether they want to
         * save that version of the file (and accept the loss), or not. */
        id->tag |= LIB_TAG_DOIT;
        id->tag &= ~LIB_TAG_MISSING;
        CLOG_WARN(
            &LOG, "Old override %s is being deleted even though it was user-edited", id->name);
        user_edited_overrides_deletion_count++;
      }
#endif
    }
  }
  FOREACH_MAIN_ID_END;

  /* Cleanup, many pointers in this GHash are already invalid now. */
  BLI_ghash_free(linkedref_to_old_override, NULL, NULL);

  BKE_id_multi_tagged_delete(bmain);

  /* At this point, `id_root` has very likely been deleted, we need to update it to its new
   * version.
   */
  id_root = id_root_reference->newid;

  if (user_edited_overrides_deletion_count > 0) {
    BKE_reportf(reports != NULL ? reports->reports : NULL,
                RPT_WARNING,
                "During resync of data-block %s, %d obsolete overrides were deleted, that had "
                "local changes defined by user",
                id_root->name + 2,
                user_edited_overrides_deletion_count);
  }

  if (do_post_process) {
    /* Essentially ensures that potentially new overrides of new objects will be instantiated. */
    /* NOTE: Here 'reference' collection and 'newly added' collection are the same, which is fine
     * since we already relinked old root override collection to new resync'ed one above. So this
     * call is not expected to instantiate this new resync'ed collection anywhere, just to ensure
     * that we do not have any stray objects. */
    lib_override_library_create_post_process(bmain,
                                             scene,
                                             view_layer,
                                             id_root_reference,
                                             id_root,
                                             override_resync_residual_storage,
                                             true);
  }

  /* Cleanup. */
  BKE_main_id_newptr_and_tag_clear(bmain);
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false); /* That one should not be needed in fact. */

  return success;
}

bool BKE_lib_override_library_resync(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     ID *id_root,
                                     Collection *override_resync_residual_storage,
                                     const bool do_hierarchy_enforce,
                                     BlendFileReadReport *reports)
{
  const bool success = lib_override_library_resync(bmain,
                                                   scene,
                                                   view_layer,
                                                   id_root,
                                                   override_resync_residual_storage,
                                                   do_hierarchy_enforce,
                                                   true,
                                                   reports);

  return success;
}

/* Also tag ancestors overrides for resync.
 *
 * WARNING: Expects `bmain` to have valid relation data.
 *
 * NOTE: Related to `lib_override_library_main_resync_find_root_recurse` below.
 *
 * TODO: This is a sub-optimal, simple solution. At some point, we should rather find a way to
 * resync a set of 'sub-roots' overrides, instead of having to 'go back' to the real root and
 * resync the whole hierarchy.
 */
static void lib_override_resync_tagging_finalize_recurse(Main *bmain,
                                                         ID *id,
                                                         const int library_indirect_level)
{
  if (id->lib != NULL && id->lib->temp_index > library_indirect_level) {
    CLOG_ERROR(
        &LOG,
        "While processing indirect level %d, ID %s from lib %s of indirect level %d detected "
        "as needing resync, skipping.",
        library_indirect_level,
        id->name,
        id->lib->filepath,
        id->lib->temp_index);
    id->tag &= ~LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
    return;
  }

  MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers, id);
  BLI_assert(entry != NULL);

  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }
  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *entry_item = entry->from_ids; entry_item != NULL;
       entry_item = entry_item->next) {
    if (entry_item->usage_flag &
        (IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE | IDWALK_CB_LOOPBACK)) {
      continue;
    }
    ID *id_from = entry_item->id_pointer.from;

    /* Case where this ID pointer was to a linked ID, that now needs to be overridden. */
    if (id_from != id && ID_IS_OVERRIDE_LIBRARY_REAL(id_from) && id_from->lib == id->lib) {
      id_from->tag |= LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
      CLOG_INFO(&LOG,
                4,
                "ID %s (%p) now tagged as needing resync because they use %s (%p) that needs to "
                "be overridden",
                id_from->name,
                id_from->lib,
                id->name,
                id->lib);
      lib_override_resync_tagging_finalize_recurse(bmain, id_from, library_indirect_level);
    }
  }
}

/* Ensures parent collection (or objects) in the same override group are also tagged for resync.
 *
 * This is needed since otherwise, some (new) ID added in one sub-collection might be used in
 * another unrelated sub-collection, if 'root' collection is not resynced separated resync of those
 * sub-collections would be unaware that this is the same ID, and would re-generate several
 * overrides for it.
 *
 * NOTE: Related to `lib_override_resync_tagging_finalize` above.
 */
static ID *lib_override_library_main_resync_find_root_recurse(ID *id, int *level)
{
  (*level)++;
  ID *return_id = id;

  switch (GS(id->name)) {
    case ID_GR: {
      /* Find the highest valid collection in the parenting hierarchy.
       * Note that in practice, in any decent common case there is only one well defined root
       * collection anyway. */
      int max_level = *level;
      Collection *collection = (Collection *)id;
      LISTBASE_FOREACH (CollectionParent *, collection_parent_iter, &collection->parents) {
        Collection *collection_parent = collection_parent_iter->collection;
        if (ID_IS_OVERRIDE_LIBRARY_REAL(collection_parent) &&
            collection_parent->id.lib == id->lib) {
          int tmp_level = *level;
          ID *tmp_id = lib_override_library_main_resync_find_root_recurse(&collection_parent->id,
                                                                          &tmp_level);
          if (tmp_level > max_level) {
            max_level = tmp_level;
            return_id = tmp_id;
          }
        }
      }
      break;
    }
    case ID_OB: {
      Object *object = (Object *)id;
      if (object->parent != NULL && ID_IS_OVERRIDE_LIBRARY_REAL(object->parent) &&
          object->parent->id.lib == id->lib) {
        return_id = lib_override_library_main_resync_find_root_recurse(&object->parent->id, level);
      }
      break;
    }
    default:
      break;
  }

  return return_id;
}

/* Ensure resync of all overrides at one level of indirect usage.
 *
 * We need to handle each level independently, since an override at level n may be affected by
 * other overrides from level n + 1 etc. (i.e. from linked overrides it may use).
 */
static void lib_override_library_main_resync_on_library_indirect_level(
    Main *bmain,
    Scene *scene,
    ViewLayer *view_layer,
    Collection *override_resync_residual_storage,
    const int library_indirect_level,
    BlendFileReadReport *reports)
{
  const bool do_reports_recursive_resync_timing = (library_indirect_level != 0);
  const double init_time = do_reports_recursive_resync_timing ? PIL_check_seconds_timer() : 0.0;

  BKE_main_relations_create(bmain, 0);
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

  /* NOTE: in code below, the order in which `FOREACH_MAIN_ID_BEGIN` processes ID types ensures
   * that we always process 'higher-level' overrides first (i.e. scenes, then collections, then
   * objects, then other types). */

  /* Detect all linked data that would need to be overridden if we had to create an override from
   * those used by current existing overrides. */
  LibOverrideGroupTagData data = {.bmain = bmain,
                                  .scene = scene,
                                  .id_root = NULL,
                                  .tag = LIB_TAG_DOIT,
                                  .missing_tag = LIB_TAG_MISSING,
                                  .is_override = false,
                                  .is_resync = true};
  lib_override_group_tag_data_object_to_collection_init(&data);
  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      continue;
    }
    if (id->tag & (LIB_TAG_DOIT | LIB_TAG_MISSING)) {
      /* We already processed that ID as part of another ID's hierarchy. */
      continue;
    }

    if (id->override_library->flag & IDOVERRIDE_LIBRARY_FLAG_NO_HIERARCHY) {
      /* This ID is not part of an override hierarchy. */
      continue;
    }

    data.id_root = id->override_library->reference;
    lib_override_linked_group_tag(&data);
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
    lib_override_hierarchy_dependencies_recursive_tag(&data);
    BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  }
  FOREACH_MAIN_ID_END;
  lib_override_group_tag_data_clear(&data);

  /* Now check existing overrides, those needing resync will be the one either already tagged as
   * such, or the one using linked data that is now tagged as needing override. */
  BKE_main_relations_tag_set(bmain, MAINIDRELATIONS_ENTRY_TAGS_PROCESSED, false);
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      continue;
    }

    if (id->override_library->flag & IDOVERRIDE_LIBRARY_FLAG_NO_HIERARCHY) {
      /* This ID is not part of an override hierarchy. */
      BLI_assert((id->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC) == 0);
      continue;
    }

    if (id->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC) {
      CLOG_INFO(&LOG, 4, "ID %s (%p) was already tagged as needing resync", id->name, id->lib);
      lib_override_resync_tagging_finalize_recurse(bmain, id, library_indirect_level);
      continue;
    }

    MainIDRelationsEntry *entry = BLI_ghash_lookup(bmain->relations->relations_from_pointers, id);
    BLI_assert(entry != NULL);

    for (MainIDRelationsEntryItem *entry_item = entry->to_ids; entry_item != NULL;
         entry_item = entry_item->next) {
      if (entry_item->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) {
        continue;
      }
      ID *id_to = *entry_item->id_pointer.to;

      /* Case where this ID pointer was to a linked ID, that now needs to be overridden. */
      if (ID_IS_LINKED(id_to) && (id_to->lib != id->lib) && (id_to->tag & LIB_TAG_DOIT) != 0) {
        id->tag |= LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
        CLOG_INFO(&LOG,
                  3,
                  "ID %s (%p) now tagged as needing resync because they use linked %s (%p) that "
                  "now needs to be overridden",
                  id->name,
                  id->lib,
                  id_to->name,
                  id_to->lib);
        lib_override_resync_tagging_finalize_recurse(bmain, id, library_indirect_level);
        break;
      }
    }
  }
  FOREACH_MAIN_ID_END;

  BKE_main_relations_free(bmain);
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

  /* And do the actual resync for all IDs detected as needing it.
   * NOTE: Since this changes `bmain` (adding **and** removing IDs), we cannot use
   * `FOREACH_MAIN_ID_BEGIN/END` here, and need special multi-loop processing. */
  bool do_continue = true;
  while (do_continue) {
    do_continue = false;
    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
        if ((id->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC) == 0 ||
            (ID_IS_LINKED(id) && id->lib->temp_index < library_indirect_level) ||
            (!ID_IS_LINKED(id) && library_indirect_level != 0)) {
          continue;
        }

        if (ID_IS_LINKED(id)) {
          id->lib->tag |= LIBRARY_TAG_RESYNC_REQUIRED;
        }

        /* We cannot resync a scene that is currently active. */
        if (id == &scene->id) {
          id->tag &= ~LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
          BKE_reportf(reports->reports,
                      RPT_WARNING,
                      "Scene '%s' was not resynced as it is the currently active one",
                      scene->id.name + 2);
          continue;
        }

        Library *library = id->lib;

        int level = 0;
        /* In complex non-supported cases, with several different override hierarchies sharing
         * relations between each-other, we may end up not actually updating/replacing the given
         * root id (see e.g. pro/shots/110_rextoria/110_0150_A/110_0150_A.anim.blend of sprites
         * project repository, r2687).
         * This can lead to infinite loop here, at least avoid this. */
        id->tag &= ~LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
        id = lib_override_library_main_resync_find_root_recurse(id, &level);
        id->tag &= ~LIB_TAG_LIB_OVERRIDE_NEED_RESYNC;
        BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id));
        BLI_assert(id->lib == library);
        do_continue = true;

        CLOG_INFO(&LOG, 2, "Resyncing %s (%p)...", id->name, library);
        const bool success = lib_override_library_resync(
            bmain, scene, view_layer, id, override_resync_residual_storage, false, false, reports);
        CLOG_INFO(&LOG, 2, "\tSuccess: %d", success);
        if (success) {
          reports->count.resynced_lib_overrides++;
          if (library_indirect_level > 0 && reports->do_resynced_lib_overrides_libraries_list &&
              BLI_linklist_index(reports->resynced_lib_overrides_libraries, library) < 0) {
            BLI_linklist_prepend(&reports->resynced_lib_overrides_libraries, library);
            reports->resynced_lib_overrides_libraries_count++;
          }
        }
        break;
      }
      FOREACH_MAIN_LISTBASE_ID_END;
      if (do_continue) {
        break;
      }
    }
    FOREACH_MAIN_LISTBASE_END;
  }

  if (do_reports_recursive_resync_timing) {
    reports->duration.lib_overrides_recursive_resync += PIL_check_seconds_timer() - init_time;
  }
}

static int lib_override_sort_libraries_func(LibraryIDLinkCallbackData *cb_data)
{
  if (cb_data->cb_flag & IDWALK_CB_LOOPBACK) {
    return IDWALK_RET_NOP;
  }
  ID *id_owner = cb_data->id_owner;
  ID *id = *cb_data->id_pointer;
  if (id != NULL && ID_IS_LINKED(id) && id->lib != id_owner->lib) {
    const int owner_library_indirect_level = id_owner->lib != NULL ? id_owner->lib->temp_index : 0;
    if (owner_library_indirect_level > 10000) {
      CLOG_ERROR(
          &LOG,
          "Levels of indirect usages of libraries is way too high, skipping further building "
          "loops (Involves at least '%s' and '%s')",
          id_owner->lib->filepath,
          id->lib->filepath);
      BLI_assert(0);
      return IDWALK_RET_NOP;
    }

    if (owner_library_indirect_level >= id->lib->temp_index) {
      id->lib->temp_index = owner_library_indirect_level + 1;
      *(bool *)cb_data->user_data = true;
    }
  }
  return IDWALK_RET_NOP;
}

/** Define the `temp_index` of libraries from their highest level of indirect usage.
 *
 * E.g. if lib_a uses lib_b, lib_c and lib_d, and lib_b also uses lib_d, then lib_a has an index of
 * 1, lib_b and lib_c an index of 2, and lib_d an index of 3. */
static int lib_override_libraries_index_define(Main *bmain)
{
  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    /* index 0 is reserved for local data. */
    library->temp_index = 1;
  }
  bool do_continue = true;
  while (do_continue) {
    do_continue = false;
    ID *id;
    FOREACH_MAIN_ID_BEGIN (bmain, id) {
      BKE_library_foreach_ID_link(
          bmain, id, lib_override_sort_libraries_func, &do_continue, IDWALK_READONLY);
    }
    FOREACH_MAIN_ID_END;
  }

  int library_indirect_level_max = 0;
  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    if (library->temp_index > library_indirect_level_max) {
      library_indirect_level_max = library->temp_index;
    }
  }
  return library_indirect_level_max;
}

void BKE_lib_override_library_main_resync(Main *bmain,
                                          Scene *scene,
                                          ViewLayer *view_layer,
                                          BlendFileReadReport *reports)
{
  /* We use a specific collection to gather/store all 'orphaned' override collections and objects
   * generated by re-sync-process. This avoids putting them in scene's master collection. */
#define OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME "OVERRIDE_RESYNC_LEFTOVERS"
  Collection *override_resync_residual_storage = BLI_findstring(
      &bmain->collections, OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME, offsetof(ID, name) + 2);
  if (override_resync_residual_storage != NULL && ID_IS_LINKED(override_resync_residual_storage)) {
    override_resync_residual_storage = NULL;
  }
  if (override_resync_residual_storage == NULL) {
    override_resync_residual_storage = BKE_collection_add(
        bmain, scene->master_collection, OVERRIDE_RESYNC_RESIDUAL_STORAGE_NAME);
    /* Hide the collection from viewport and render. */
    override_resync_residual_storage->flag |= COLLECTION_HIDE_VIEWPORT | COLLECTION_HIDE_RENDER;
  }

  /* Necessary to improve performances, and prevent layers matching override sub-collections to be
   * lost when re-syncing the parent override collection.
   * Ref. T73411. */
  BKE_layer_collection_resync_forbid();

  int library_indirect_level = lib_override_libraries_index_define(bmain);
  while (library_indirect_level >= 0) {
    /* Update overrides from each indirect level separately. */
    lib_override_library_main_resync_on_library_indirect_level(bmain,
                                                               scene,
                                                               view_layer,
                                                               override_resync_residual_storage,
                                                               library_indirect_level,
                                                               reports);
    library_indirect_level--;
  }

  BKE_layer_collection_resync_allow();

  /* Essentially ensures that potentially new overrides of new objects will be instantiated. */
  lib_override_library_create_post_process(
      bmain, scene, view_layer, NULL, NULL, override_resync_residual_storage, true);

  if (BKE_collection_is_empty(override_resync_residual_storage)) {
    BKE_collection_delete(bmain, override_resync_residual_storage, true);
  }

  LISTBASE_FOREACH (Library *, library, &bmain->libraries) {
    if (library->tag & LIBRARY_TAG_RESYNC_REQUIRED) {
      CLOG_INFO(&LOG,
                2,
                "library '%s' contains some linked overrides that required recursive resync, "
                "consider updating it",
                library->filepath);
    }
  }
}

void BKE_lib_override_library_delete(Main *bmain, ID *id_root)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(id_root));

  /* Tag all library overrides in the chains of dependencies from the given root one. */
  BKE_main_relations_create(bmain, 0);
  LibOverrideGroupTagData data = {.bmain = bmain,
                                  .scene = NULL,
                                  .id_root = id_root,
                                  .tag = LIB_TAG_DOIT,
                                  .missing_tag = LIB_TAG_MISSING,
                                  .is_override = true,
                                  .is_resync = false};
  lib_override_group_tag_data_object_to_collection_init(&data);
  lib_override_overrides_group_tag(&data);

  BKE_main_relations_free(bmain);
  lib_override_group_tag_data_clear(&data);

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->tag & LIB_TAG_DOIT) {
      if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
        ID *id_override_reference = id->override_library->reference;

        /* Remap the whole local IDs to use the linked data. */
        BKE_libblock_remap(bmain, id, id_override_reference, ID_REMAP_SKIP_INDIRECT_USAGE);
      }
    }
  }
  FOREACH_MAIN_ID_END;

  /* Delete the override IDs. */
  BKE_id_multi_tagged_delete(bmain);

  /* Should not actually be needed here. */
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
}

void BKE_lib_override_library_make_local(ID *id)
{
  if (!ID_IS_OVERRIDE_LIBRARY(id)) {
    return;
  }
  if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
    /* We should never directly 'make local' virtual overrides (aka shape keys). */
    BLI_assert_unreachable();
    id->flag &= ~LIB_EMBEDDED_DATA_LIB_OVERRIDE;
    return;
  }

  BKE_lib_override_library_free(&id->override_library, true);

  Key *shape_key = BKE_key_from_id(id);
  if (shape_key != NULL) {
    shape_key->id.flag &= ~LIB_EMBEDDED_DATA_LIB_OVERRIDE;
  }

  if (GS(id->name) == ID_SCE) {
    Collection *master_collection = ((Scene *)id)->master_collection;
    if (master_collection != NULL) {
      master_collection->id.flag &= ~LIB_EMBEDDED_DATA_LIB_OVERRIDE;
    }
  }

  bNodeTree *node_tree = ntreeFromID(id);
  if (node_tree != NULL) {
    node_tree->id.flag &= ~LIB_EMBEDDED_DATA_LIB_OVERRIDE;
  }
}

BLI_INLINE IDOverrideLibraryRuntime *override_library_rna_path_runtime_ensure(
    IDOverrideLibrary *override)
{
  if (override->runtime == NULL) {
    override->runtime = MEM_callocN(sizeof(*override->runtime), __func__);
  }
  return override->runtime;
}

/* We only build override GHash on request. */
BLI_INLINE GHash *override_library_rna_path_mapping_ensure(IDOverrideLibrary *override)
{
  IDOverrideLibraryRuntime *override_runtime = override_library_rna_path_runtime_ensure(override);
  if (override_runtime->rna_path_to_override_properties == NULL) {
    override_runtime->rna_path_to_override_properties = BLI_ghash_new(
        BLI_ghashutil_strhash_p_murmur, BLI_ghashutil_strcmp, __func__);
    for (IDOverrideLibraryProperty *op = override->properties.first; op != NULL; op = op->next) {
      BLI_ghash_insert(override_runtime->rna_path_to_override_properties, op->rna_path, op);
    }
  }

  return override_runtime->rna_path_to_override_properties;
}

IDOverrideLibraryProperty *BKE_lib_override_library_property_find(IDOverrideLibrary *override,
                                                                  const char *rna_path)
{
  GHash *override_runtime = override_library_rna_path_mapping_ensure(override);
  return BLI_ghash_lookup(override_runtime, rna_path);
}

IDOverrideLibraryProperty *BKE_lib_override_library_property_get(IDOverrideLibrary *override,
                                                                 const char *rna_path,
                                                                 bool *r_created)
{
  IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(override, rna_path);

  if (op == NULL) {
    op = MEM_callocN(sizeof(IDOverrideLibraryProperty), __func__);
    op->rna_path = BLI_strdup(rna_path);
    BLI_addtail(&override->properties, op);

    GHash *override_runtime = override_library_rna_path_mapping_ensure(override);
    BLI_ghash_insert(override_runtime, op->rna_path, op);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return op;
}

bool BKE_lib_override_rna_property_find(PointerRNA *idpoin,
                                        const IDOverrideLibraryProperty *library_prop,
                                        PointerRNA *r_override_poin,
                                        PropertyRNA **r_override_prop)
{
  BLI_assert(RNA_struct_is_ID(idpoin->type) && ID_IS_OVERRIDE_LIBRARY(idpoin->data));
  return RNA_path_resolve_property(
      idpoin, library_prop->rna_path, r_override_poin, r_override_prop);
}

void lib_override_library_property_copy(IDOverrideLibraryProperty *op_dst,
                                        IDOverrideLibraryProperty *op_src)
{
  op_dst->rna_path = BLI_strdup(op_src->rna_path);
  BLI_duplicatelist(&op_dst->operations, &op_src->operations);

  for (IDOverrideLibraryPropertyOperation *opop_dst = op_dst->operations.first,
                                          *opop_src = op_src->operations.first;
       opop_dst;
       opop_dst = opop_dst->next, opop_src = opop_src->next) {
    lib_override_library_property_operation_copy(opop_dst, opop_src);
  }
}

void lib_override_library_property_clear(IDOverrideLibraryProperty *op)
{
  BLI_assert(op->rna_path != NULL);

  MEM_freeN(op->rna_path);

  LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
    lib_override_library_property_operation_clear(opop);
  }
  BLI_freelistN(&op->operations);
}

void BKE_lib_override_library_property_delete(IDOverrideLibrary *override,
                                              IDOverrideLibraryProperty *override_property)
{
  if (!ELEM(NULL, override->runtime, override->runtime->rna_path_to_override_properties)) {
    BLI_ghash_remove(override->runtime->rna_path_to_override_properties,
                     override_property->rna_path,
                     NULL,
                     NULL);
  }
  lib_override_library_property_clear(override_property);
  BLI_freelinkN(&override->properties, override_property);
}

IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_find(
    IDOverrideLibraryProperty *override_property,
    const char *subitem_refname,
    const char *subitem_locname,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict)
{
  IDOverrideLibraryPropertyOperation *opop;
  const int subitem_defindex = -1;

  if (r_strict) {
    *r_strict = true;
  }

  if (subitem_locname != NULL) {
    opop = BLI_findstring_ptr(&override_property->operations,
                              subitem_locname,
                              offsetof(IDOverrideLibraryPropertyOperation, subitem_local_name));

    if (opop == NULL) {
      return NULL;
    }

    if (subitem_refname == NULL || opop->subitem_reference_name == NULL) {
      return subitem_refname == opop->subitem_reference_name ? opop : NULL;
    }
    return (subitem_refname != NULL && opop->subitem_reference_name != NULL &&
            STREQ(subitem_refname, opop->subitem_reference_name)) ?
               opop :
               NULL;
  }

  if (subitem_refname != NULL) {
    opop = BLI_findstring_ptr(
        &override_property->operations,
        subitem_refname,
        offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_name));

    if (opop == NULL) {
      return NULL;
    }

    if (subitem_locname == NULL || opop->subitem_local_name == NULL) {
      return subitem_locname == opop->subitem_local_name ? opop : NULL;
    }
    return (subitem_locname != NULL && opop->subitem_local_name != NULL &&
            STREQ(subitem_locname, opop->subitem_local_name)) ?
               opop :
               NULL;
  }

  if ((opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_locindex,
           sizeof(subitem_locindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)))) {
    return ELEM(subitem_refindex, -1, opop->subitem_reference_index) ? opop : NULL;
  }

  if ((opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_refindex,
           sizeof(subitem_refindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_reference_index)))) {
    return ELEM(subitem_locindex, -1, opop->subitem_local_index) ? opop : NULL;
  }

  /* `index == -1` means all indices, that is a valid fallback in case we requested specific index.
   */
  if (!strict && (subitem_locindex != subitem_defindex) &&
      (opop = BLI_listbase_bytes_find(
           &override_property->operations,
           &subitem_defindex,
           sizeof(subitem_defindex),
           offsetof(IDOverrideLibraryPropertyOperation, subitem_local_index)))) {
    if (r_strict) {
      *r_strict = false;
    }
    return opop;
  }

  return NULL;
}

IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_get(
    IDOverrideLibraryProperty *override_property,
    const short operation,
    const char *subitem_refname,
    const char *subitem_locname,
    const int subitem_refindex,
    const int subitem_locindex,
    const bool strict,
    bool *r_strict,
    bool *r_created)
{
  IDOverrideLibraryPropertyOperation *opop = BKE_lib_override_library_property_operation_find(
      override_property,
      subitem_refname,
      subitem_locname,
      subitem_refindex,
      subitem_locindex,
      strict,
      r_strict);

  if (opop == NULL) {
    opop = MEM_callocN(sizeof(IDOverrideLibraryPropertyOperation), __func__);
    opop->operation = operation;
    if (subitem_locname) {
      opop->subitem_local_name = BLI_strdup(subitem_locname);
    }
    if (subitem_refname) {
      opop->subitem_reference_name = BLI_strdup(subitem_refname);
    }
    opop->subitem_local_index = subitem_locindex;
    opop->subitem_reference_index = subitem_refindex;

    BLI_addtail(&override_property->operations, opop);

    if (r_created) {
      *r_created = true;
    }
  }
  else if (r_created) {
    *r_created = false;
  }

  return opop;
}

void lib_override_library_property_operation_copy(IDOverrideLibraryPropertyOperation *opop_dst,
                                                  IDOverrideLibraryPropertyOperation *opop_src)
{
  if (opop_src->subitem_reference_name) {
    opop_dst->subitem_reference_name = BLI_strdup(opop_src->subitem_reference_name);
  }
  if (opop_src->subitem_local_name) {
    opop_dst->subitem_local_name = BLI_strdup(opop_src->subitem_local_name);
  }
}

void lib_override_library_property_operation_clear(IDOverrideLibraryPropertyOperation *opop)
{
  if (opop->subitem_reference_name) {
    MEM_freeN(opop->subitem_reference_name);
  }
  if (opop->subitem_local_name) {
    MEM_freeN(opop->subitem_local_name);
  }
}

void BKE_lib_override_library_property_operation_delete(
    IDOverrideLibraryProperty *override_property,
    IDOverrideLibraryPropertyOperation *override_property_operation)
{
  lib_override_library_property_operation_clear(override_property_operation);
  BLI_freelinkN(&override_property->operations, override_property_operation);
}

bool BKE_lib_override_library_property_operation_operands_validate(
    struct IDOverrideLibraryPropertyOperation *override_property_operation,
    struct PointerRNA *ptr_dst,
    struct PointerRNA *ptr_src,
    struct PointerRNA *ptr_storage,
    struct PropertyRNA *prop_dst,
    struct PropertyRNA *prop_src,
    struct PropertyRNA *prop_storage)
{
  switch (override_property_operation->operation) {
    case IDOVERRIDE_LIBRARY_OP_NOOP:
      return true;
    case IDOVERRIDE_LIBRARY_OP_ADD:
      ATTR_FALLTHROUGH;
    case IDOVERRIDE_LIBRARY_OP_SUBTRACT:
      ATTR_FALLTHROUGH;
    case IDOVERRIDE_LIBRARY_OP_MULTIPLY:
      if (ptr_storage == NULL || ptr_storage->data == NULL || prop_storage == NULL) {
        BLI_assert_msg(0, "Missing data to apply differential override operation.");
        return false;
      }
      ATTR_FALLTHROUGH;
    case IDOVERRIDE_LIBRARY_OP_INSERT_AFTER:
      ATTR_FALLTHROUGH;
    case IDOVERRIDE_LIBRARY_OP_INSERT_BEFORE:
      ATTR_FALLTHROUGH;
    case IDOVERRIDE_LIBRARY_OP_REPLACE:
      if ((ptr_dst == NULL || ptr_dst->data == NULL || prop_dst == NULL) ||
          (ptr_src == NULL || ptr_src->data == NULL || prop_src == NULL)) {
        BLI_assert_msg(0, "Missing data to apply override operation.");
        return false;
      }
  }

  return true;
}

void BKE_lib_override_library_validate(Main *UNUSED(bmain), ID *id, ReportList *reports)
{
  if (id->override_library == NULL) {
    return;
  }
  if (id->override_library->reference == NULL) {
    /* This is a template ID, could be linked or local, not an override. */
    return;
  }
  if (id->override_library->reference == id) {
    /* Very serious data corruption, cannot do much about it besides removing the reference
     * (therefore making the id a local override template one only). */
    BKE_reportf(reports,
                RPT_ERROR,
                "Data corruption: data-block '%s' is using itself as library override reference",
                id->name);
    id->override_library->reference = NULL;
    return;
  }
  if (!ID_IS_LINKED(id->override_library->reference)) {
    /* Very serious data corruption, cannot do much about it besides removing the reference
     * (therefore making the id a local override template one only). */
    BKE_reportf(reports,
                RPT_ERROR,
                "Data corruption: data-block '%s' is using another local data-block ('%s') as "
                "library override reference",
                id->name,
                id->override_library->reference->name);
    id->override_library->reference = NULL;
    return;
  }
}

void BKE_lib_override_library_main_validate(Main *bmain, ReportList *reports)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->override_library != NULL) {
      BKE_lib_override_library_validate(bmain, id, reports);
    }
  }
  FOREACH_MAIN_ID_END;
}

bool BKE_lib_override_library_status_check_local(Main *bmain, ID *local)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  ID *reference = local->override_library->reference;

  if (reference == NULL) {
    /* This is an override template, local status is always OK! */
    return true;
  }

  BLI_assert(GS(local->name) == GS(reference->name));

  if (GS(local->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves. */
    Object *ob_local = (Object *)local;
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = (Object *)local->override_library->reference;
      BLI_assert(ob_local->data != NULL);
      BLI_assert(ob_reference->data != NULL);
      BKE_pose_ensure(bmain, ob_local, ob_local->data, true);
      BKE_pose_ensure(bmain, ob_reference, ob_reference->data, true);
    }
  }

  /* Note that reference is assumed always valid, caller has to ensure that itself. */

  PointerRNA rnaptr_local, rnaptr_reference;
  RNA_id_pointer_create(local, &rnaptr_local);
  RNA_id_pointer_create(reference, &rnaptr_reference);

  if (!RNA_struct_override_matches(bmain,
                                   &rnaptr_local,
                                   &rnaptr_reference,
                                   NULL,
                                   0,
                                   local->override_library,
                                   RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE |
                                       RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN,
                                   NULL)) {
    local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
    return false;
  }

  return true;
}

bool BKE_lib_override_library_status_check_reference(Main *bmain, ID *local)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  ID *reference = local->override_library->reference;

  if (reference == NULL) {
    /* This is an override template, reference is virtual, so its status is always OK! */
    return true;
  }

  BLI_assert(GS(local->name) == GS(reference->name));

  if (reference->override_library && (reference->tag & LIB_TAG_OVERRIDE_LIBRARY_REFOK) == 0) {
    if (!BKE_lib_override_library_status_check_reference(bmain, reference)) {
      /* If reference is also an override of another data-block, and its status is not OK,
       * then this override is not OK either.
       * Note that this should only happen when reloading libraries. */
      local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
      return false;
    }
  }

  if (GS(local->name) == ID_OB) {
    /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
     * ensure this is valid, but in some situations (like hidden collections etc.) this won't
     * be the case, so we need to take care of this ourselves. */
    Object *ob_local = (Object *)local;
    if (ob_local->type == OB_ARMATURE) {
      Object *ob_reference = (Object *)local->override_library->reference;
      BLI_assert(ob_local->data != NULL);
      BLI_assert(ob_reference->data != NULL);
      BKE_pose_ensure(bmain, ob_local, ob_local->data, true);
      BKE_pose_ensure(bmain, ob_reference, ob_reference->data, true);
    }
  }

  PointerRNA rnaptr_local, rnaptr_reference;
  RNA_id_pointer_create(local, &rnaptr_local);
  RNA_id_pointer_create(reference, &rnaptr_reference);

  if (!RNA_struct_override_matches(bmain,
                                   &rnaptr_local,
                                   &rnaptr_reference,
                                   NULL,
                                   0,
                                   local->override_library,
                                   RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN,
                                   NULL)) {
    local->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_REFOK;
    return false;
  }

  return true;
}

bool BKE_lib_override_library_operations_create(Main *bmain, ID *local)
{
  BLI_assert(local->override_library != NULL);
  const bool is_template = (local->override_library->reference == NULL);
  bool created = false;

  if (!is_template) {
    /* Do not attempt to generate overriding rules from an empty place-holder generated by link
     * code when it cannot find the actual library/ID. Much better to keep the local data-block as
     * is in the file in that case, until broken lib is fixed. */
    if (ID_MISSING(local->override_library->reference)) {
      return created;
    }

    if (GS(local->name) == ID_OB) {
      /* Our beloved pose's bone cross-data pointers. Usually, depsgraph evaluation would
       * ensure this is valid, but in some situations (like hidden collections etc.) this won't
       * be the case, so we need to take care of this ourselves. */
      Object *ob_local = (Object *)local;
      if (ob_local->type == OB_ARMATURE) {
        Object *ob_reference = (Object *)local->override_library->reference;
        BLI_assert(ob_local->data != NULL);
        BLI_assert(ob_reference->data != NULL);
        BKE_pose_ensure(bmain, ob_local, ob_local->data, true);
        BKE_pose_ensure(bmain, ob_reference, ob_reference->data, true);
      }
    }

    PointerRNA rnaptr_local, rnaptr_reference;
    RNA_id_pointer_create(local, &rnaptr_local);
    RNA_id_pointer_create(local->override_library->reference, &rnaptr_reference);

    eRNAOverrideMatchResult report_flags = 0;
    RNA_struct_override_matches(bmain,
                                &rnaptr_local,
                                &rnaptr_reference,
                                NULL,
                                0,
                                local->override_library,
                                RNA_OVERRIDE_COMPARE_CREATE | RNA_OVERRIDE_COMPARE_RESTORE,
                                &report_flags);

    if (report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) {
      created = true;
    }

    if (report_flags & RNA_OVERRIDE_MATCH_RESULT_RESTORED) {
      CLOG_INFO(&LOG, 2, "We did restore some properties of %s from its reference", local->name);
    }
    if (report_flags & RNA_OVERRIDE_MATCH_RESULT_CREATED) {
      CLOG_INFO(&LOG, 2, "We did generate library override rules for %s", local->name);
    }
    else {
      CLOG_INFO(&LOG, 2, "No new library override rules for %s", local->name);
    }
  }
  return created;
}

struct LibOverrideOpCreateData {
  Main *bmain;
  bool changed;
};

static void lib_override_library_operations_create_cb(TaskPool *__restrict pool, void *taskdata)
{
  struct LibOverrideOpCreateData *create_data = BLI_task_pool_user_data(pool);
  ID *id = taskdata;

  if (BKE_lib_override_library_operations_create(create_data->bmain, id)) {
    /* Technically no need for atomic, all jobs write the same value and we only care if one did
     * it. But play safe and avoid implicit assumptions. */
    atomic_fetch_and_or_uint8((uint8_t *)&create_data->changed, true);
  }
}

bool BKE_lib_override_library_main_operations_create(Main *bmain, const bool force_auto)
{
  ID *id;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(BKE_lib_override_library_main_operations_create);
#endif

  /* When force-auto is set, we also remove all unused existing override properties & operations.
   */
  if (force_auto) {
    BKE_lib_override_library_main_tag(bmain, IDOVERRIDE_LIBRARY_TAG_UNUSED, true);
  }

  /* Usual pose bones issue, need to be done outside of the threaded process or we may run into
   * concurrency issues here.
   * Note that calling #BKE_pose_ensure again in thread in
   * #BKE_lib_override_library_operations_create is not a problem then. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type == OB_ARMATURE) {
      BLI_assert(ob->data != NULL);
      BKE_pose_ensure(bmain, ob, ob->data, true);
    }
  }

  struct LibOverrideOpCreateData create_pool_data = {.bmain = bmain, .changed = false};
  TaskPool *task_pool = BLI_task_pool_create(&create_pool_data, TASK_PRIORITY_HIGH);

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_LINKED(id) && ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
        (force_auto || (id->tag & LIB_TAG_OVERRIDE_LIBRARY_AUTOREFRESH))) {
      /* Usual issue with pose, it's quiet rare but sometimes they may not be up to date when this
       * function is called. */
      if (GS(id->name) == ID_OB) {
        Object *ob = (Object *)id;
        if (ob->type == OB_ARMATURE) {
          BLI_assert(ob->data != NULL);
          BKE_pose_ensure(bmain, ob, ob->data, true);
        }
      }
      /* Only check overrides if we do have the real reference data available, and not some empty
       * 'placeholder' for missing data (broken links). */
      if ((id->override_library->reference->tag & LIB_TAG_MISSING) == 0) {
        BLI_task_pool_push(task_pool, lib_override_library_operations_create_cb, id, false, NULL);
      }
      else {
        BKE_lib_override_library_properties_tag(
            id->override_library, IDOVERRIDE_LIBRARY_TAG_UNUSED, false);
      }
    }
    id->tag &= ~LIB_TAG_OVERRIDE_LIBRARY_AUTOREFRESH;
  }
  FOREACH_MAIN_ID_END;

  BLI_task_pool_work_and_wait(task_pool);

  BLI_task_pool_free(task_pool);

  if (force_auto) {
    BKE_lib_override_library_main_unused_cleanup(bmain);
  }

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(BKE_lib_override_library_main_operations_create);
#endif

  return create_pool_data.changed;
}

static bool lib_override_library_id_reset_do(Main *bmain, ID *id_root)
{
  bool was_op_deleted = false;

  LISTBASE_FOREACH_MUTABLE (
      IDOverrideLibraryProperty *, op, &id_root->override_library->properties) {
    bool do_op_delete = true;
    const bool is_collection = op->rna_prop_type == PROP_COLLECTION;
    if (is_collection || op->rna_prop_type == PROP_POINTER) {
      PointerRNA ptr_root, ptr_root_lib, ptr, ptr_lib;
      PropertyRNA *prop, *prop_lib;

      RNA_pointer_create(id_root, &RNA_ID, id_root, &ptr_root);
      RNA_pointer_create(id_root->override_library->reference,
                         &RNA_ID,
                         id_root->override_library->reference,
                         &ptr_root_lib);

      bool prop_exists = RNA_path_resolve_property(&ptr_root, op->rna_path, &ptr, &prop);
      if (prop_exists) {
        prop_exists = RNA_path_resolve_property(&ptr_root_lib, op->rna_path, &ptr_lib, &prop_lib);

        if (prop_exists) {
          BLI_assert(ELEM(RNA_property_type(prop), PROP_POINTER, PROP_COLLECTION));
          BLI_assert(RNA_property_type(prop) == RNA_property_type(prop_lib));
          if (is_collection) {
            ptr.type = RNA_property_pointer_type(&ptr, prop);
            ptr_lib.type = RNA_property_pointer_type(&ptr_lib, prop_lib);
          }
          else {
            ptr = RNA_property_pointer_get(&ptr, prop);
            ptr_lib = RNA_property_pointer_get(&ptr_lib, prop_lib);
          }
          if (ptr.owner_id != NULL && ptr_lib.owner_id != NULL) {
            BLI_assert(ptr.type == ptr_lib.type);
            do_op_delete = !(RNA_struct_is_ID(ptr.type) &&
                             ptr.owner_id->override_library != NULL &&
                             ptr.owner_id->override_library->reference == ptr_lib.owner_id);
          }
        }
      }
    }

    if (do_op_delete) {
      BKE_lib_override_library_property_delete(id_root->override_library, op);
      was_op_deleted = true;
    }
  }

  if (was_op_deleted) {
    DEG_id_tag_update_ex(bmain, id_root, ID_RECALC_COPY_ON_WRITE);
    IDOverrideLibraryRuntime *override_runtime = override_library_rna_path_runtime_ensure(
        id_root->override_library);
    override_runtime->tag |= IDOVERRIDE_LIBRARY_RUNTIME_TAG_NEEDS_RELOAD;
  }

  return was_op_deleted;
}

void BKE_lib_override_library_id_reset(Main *bmain, ID *id_root)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root)) {
    return;
  }

  if (lib_override_library_id_reset_do(bmain, id_root)) {
    if (id_root->override_library->runtime != NULL &&
        (id_root->override_library->runtime->tag & IDOVERRIDE_LIBRARY_RUNTIME_TAG_NEEDS_RELOAD) !=
            0) {
      BKE_lib_override_library_update(bmain, id_root);
      id_root->override_library->runtime->tag &= ~IDOVERRIDE_LIBRARY_RUNTIME_TAG_NEEDS_RELOAD;
    }
  }
}

static void lib_override_library_id_hierarchy_recursive_reset(Main *bmain, ID *id_root)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(id_root)) {
    return;
  }

  void **entry_vp = BLI_ghash_lookup_p(bmain->relations->relations_from_pointers, id_root);
  if (entry_vp == NULL) {
    /* This ID is not used by nor using any other ID. */
    lib_override_library_id_reset_do(bmain, id_root);
    return;
  }

  MainIDRelationsEntry *entry = *entry_vp;
  if (entry->tags & MAINIDRELATIONS_ENTRY_TAGS_PROCESSED) {
    /* This ID has already been processed. */
    return;
  }

  lib_override_library_id_reset_do(bmain, id_root);

  /* This way we won't process again that ID, should we encounter it again through another
   * relationship hierarchy. */
  entry->tags |= MAINIDRELATIONS_ENTRY_TAGS_PROCESSED;

  for (MainIDRelationsEntryItem *to_id_entry = entry->to_ids; to_id_entry != NULL;
       to_id_entry = to_id_entry->next) {
    if ((to_id_entry->usage_flag & IDWALK_CB_OVERRIDE_LIBRARY_NOT_OVERRIDABLE) != 0) {
      /* Never consider non-overridable relationships ('from', 'parents', 'owner' etc. pointers) as
       * actual dependencies. */
      continue;
    }
    /* We only consider IDs from the same library. */
    if (*to_id_entry->id_pointer.to != NULL) {
      ID *to_id = *to_id_entry->id_pointer.to;
      if (to_id->override_library != NULL) {
        lib_override_library_id_hierarchy_recursive_reset(bmain, to_id);
      }
    }
  }
}

void BKE_lib_override_library_id_hierarchy_reset(Main *bmain, ID *id_root)
{
  BKE_main_relations_create(bmain, 0);

  lib_override_library_id_hierarchy_recursive_reset(bmain, id_root);

  BKE_main_relations_free(bmain);

  ID *id;
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_OVERRIDE_LIBRARY_REAL(id) || id->override_library->runtime == NULL ||
        (id->override_library->runtime->tag & IDOVERRIDE_LIBRARY_RUNTIME_TAG_NEEDS_RELOAD) == 0) {
      continue;
    }
    BKE_lib_override_library_update(bmain, id);
    id->override_library->runtime->tag &= ~IDOVERRIDE_LIBRARY_RUNTIME_TAG_NEEDS_RELOAD;
  }
  FOREACH_MAIN_ID_END;
}

void BKE_lib_override_library_operations_tag(struct IDOverrideLibraryProperty *override_property,
                                             const short tag,
                                             const bool do_set)
{
  if (override_property != NULL) {
    if (do_set) {
      override_property->tag |= tag;
    }
    else {
      override_property->tag &= ~tag;
    }

    LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &override_property->operations) {
      if (do_set) {
        opop->tag |= tag;
      }
      else {
        opop->tag &= ~tag;
      }
    }
  }
}

void BKE_lib_override_library_properties_tag(struct IDOverrideLibrary *override,
                                             const short tag,
                                             const bool do_set)
{
  if (override != NULL) {
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &override->properties) {
      BKE_lib_override_library_operations_tag(op, tag, do_set);
    }
  }
}

void BKE_lib_override_library_main_tag(struct Main *bmain, const short tag, const bool do_set)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_OVERRIDE_LIBRARY(id)) {
      BKE_lib_override_library_properties_tag(id->override_library, tag, do_set);
    }
  }
  FOREACH_MAIN_ID_END;
}

void BKE_lib_override_library_id_unused_cleanup(struct ID *local)
{
  if (ID_IS_OVERRIDE_LIBRARY_REAL(local)) {
    LISTBASE_FOREACH_MUTABLE (
        IDOverrideLibraryProperty *, op, &local->override_library->properties) {
      if (op->tag & IDOVERRIDE_LIBRARY_TAG_UNUSED) {
        BKE_lib_override_library_property_delete(local->override_library, op);
      }
      else {
        LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
          if (opop->tag & IDOVERRIDE_LIBRARY_TAG_UNUSED) {
            BKE_lib_override_library_property_operation_delete(op, opop);
          }
        }
      }
    }
  }
}

void BKE_lib_override_library_main_unused_cleanup(struct Main *bmain)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (ID_IS_OVERRIDE_LIBRARY(id)) {
      BKE_lib_override_library_id_unused_cleanup(id);
    }
  }
  FOREACH_MAIN_ID_END;
}

static void lib_override_id_swap(Main *bmain, ID *id_local, ID *id_temp)
{
  BKE_lib_id_swap(bmain, id_local, id_temp);
  /* We need to keep these tags from temp ID into orig one.
   * ID swap does not swap most of ID data itself. */
  id_local->tag |= (id_temp->tag & LIB_TAG_LIB_OVERRIDE_NEED_RESYNC);
}

void BKE_lib_override_library_update(Main *bmain, ID *local)
{
  if (!ID_IS_OVERRIDE_LIBRARY_REAL(local)) {
    return;
  }

  /* Do not attempt to apply overriding rules over an empty place-holder generated by link code
   * when it cannot find the actual library/ID. Much better to keep the local data-block as loaded
   * from the file in that case, until broken lib is fixed. */
  if (ID_MISSING(local->override_library->reference)) {
    return;
  }

  /* Recursively do 'ancestor' overrides first, if any. */
  if (local->override_library->reference->override_library &&
      (local->override_library->reference->tag & LIB_TAG_OVERRIDE_LIBRARY_REFOK) == 0) {
    BKE_lib_override_library_update(bmain, local->override_library->reference);
  }

  /* We want to avoid having to remap here, however creating up-to-date override is much simpler
   * if based on reference than on current override.
   * So we work on temp copy of reference, and 'swap' its content with local. */

  /* XXX We need a way to get off-Main copies of IDs (similar to localized mats/texts/ etc.)!
   *     However, this is whole bunch of code work in itself, so for now plain stupid ID copy
   *     will do, as inefficient as it is. :/
   *     Actually, maybe not! Since we are swapping with original ID's local content, we want to
   *     keep user-count in correct state when freeing tmp_id
   *     (and that user-counts of IDs used by 'new' local data also remain correct). */
  /* This would imply change in handling of user-count all over RNA
   * (and possibly all over Blender code).
   * Not impossible to do, but would rather see first if extra useless usual user handling
   * is actually a (performances) issue here. */

  ID *tmp_id = BKE_id_copy_ex(bmain,
                              local->override_library->reference,
                              NULL,
                              LIB_ID_COPY_DEFAULT | LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG);

  if (tmp_id == NULL) {
    return;
  }

  /* This ID name is problematic, since it is an 'rna name property' it should not be editable or
   * different from reference linked ID. But local ID names need to be unique in a given type
   * list of Main, so we cannot always keep it identical, which is why we need this special
   * manual handling here. */
  BLI_strncpy(tmp_id->name, local->name, sizeof(tmp_id->name));

  /* Those ugly loop-back pointers again. Luckily we only need to deal with the shape keys here,
   * collections' parents are fully runtime and reconstructed later. */
  Key *local_key = BKE_key_from_id(local);
  Key *tmp_key = BKE_key_from_id(tmp_id);
  if (local_key != NULL && tmp_key != NULL) {
    tmp_key->id.flag |= (local_key->id.flag & LIB_EMBEDDED_DATA_LIB_OVERRIDE);
  }

  PointerRNA rnaptr_src, rnaptr_dst, rnaptr_storage_stack, *rnaptr_storage = NULL;
  RNA_id_pointer_create(local, &rnaptr_src);
  RNA_id_pointer_create(tmp_id, &rnaptr_dst);
  if (local->override_library->storage) {
    rnaptr_storage = &rnaptr_storage_stack;
    RNA_id_pointer_create(local->override_library->storage, rnaptr_storage);
  }

  RNA_struct_override_apply(bmain,
                            &rnaptr_dst,
                            &rnaptr_src,
                            rnaptr_storage,
                            local->override_library,
                            RNA_OVERRIDE_APPLY_FLAG_NOP);

  /* This also transfers all pointers (memory) owned by local to tmp_id, and vice-versa.
   * So when we'll free tmp_id, we'll actually free old, outdated data from local. */
  lib_override_id_swap(bmain, local, tmp_id);

  if (local_key != NULL && tmp_key != NULL) {
    /* This is some kind of hard-coded 'always enforced override'. */
    lib_override_id_swap(bmain, &local_key->id, &tmp_key->id);
    tmp_key->id.flag |= (local_key->id.flag & LIB_EMBEDDED_DATA_LIB_OVERRIDE);
    /* The swap of local and tmp_id inverted those pointers, we need to redefine proper
     * relationships. */
    *BKE_key_from_id_p(local) = local_key;
    *BKE_key_from_id_p(tmp_id) = tmp_key;
    local_key->from = local;
    tmp_key->from = tmp_id;
  }

  /* Again, horribly inefficient in our case, we need something off-Main
   * (aka more generic nolib copy/free stuff)! */
  BKE_id_free_ex(bmain, tmp_id, LIB_ID_FREE_NO_UI_USER, true);

  if (GS(local->name) == ID_AR) {
    /* Fun times again, thanks to bone pointers in pose data of objects. We keep same ID addresses,
     * but internal data has changed for sure, so we need to invalidate pose-bones caches. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->pose != NULL && ob->data == local) {
        BLI_assert(ob->type == OB_ARMATURE);
        ob->pose->flag |= POSE_RECALC;
        /* We need to clear pose bone pointers immediately, some code may access those before pose
         * is actually recomputed, which can lead to segfault. */
        BKE_pose_clear_pointers(ob->pose);
      }
    }
  }

  if (local->override_library->storage) {
    /* We know this data-block is not used anywhere besides local->override->storage. */
    /* XXX For until we get fully shadow copies, we still need to ensure storage releases
     *     its usage of any ID pointers it may have. */
    BKE_id_free_ex(bmain, local->override_library->storage, LIB_ID_FREE_NO_UI_USER, true);
    local->override_library->storage = NULL;
  }

  local->tag |= LIB_TAG_OVERRIDE_LIBRARY_REFOK;

  /* NOTE: Since we reload full content from linked ID here, potentially from edited local
   * override, we do not really have a way to know *what* is changed, so we need to rely on the
   * massive destruction weapon of `ID_RECALC_ALL` here. */
  DEG_id_tag_update_ex(bmain, local, ID_RECALC_ALL);
  /* For same reason as above, also assume that the relationships between IDs changed. */
  DEG_relations_tag_update(bmain);
}

void BKE_lib_override_library_main_update(Main *bmain)
{
  ID *id;

  /* This temporary swap of G_MAIN is rather ugly,
   * but necessary to avoid asserts checks in some RNA assignment functions,
   * since those always use on G_MAIN when they need access to a Main database. */
  Main *orig_gmain = G_MAIN;
  G_MAIN = bmain;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (id->override_library != NULL) {
      BKE_lib_override_library_update(bmain, id);
    }
  }
  FOREACH_MAIN_ID_END;

  G_MAIN = orig_gmain;
}

bool BKE_lib_override_library_id_is_user_deletable(struct Main *bmain, struct ID *id)
{
  if (!(ID_IS_LINKED(id) || ID_IS_OVERRIDE_LIBRARY(id))) {
    return true;
  }

  /* The only strong known case currently are objects used by override collections. */
  /* TODO: There are most likely other cases... This may need to be addressed in a better way at
   * some point. */
  if (GS(id->name) != ID_OB) {
    return true;
  }
  Object *ob = (Object *)id;
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (!ID_IS_OVERRIDE_LIBRARY(collection)) {
      continue;
    }
    if (BKE_collection_has_object(collection, ob)) {
      return false;
    }
  }
  return true;
}

/**
 * Storage (how to store overriding data into `.blend` files).
 *
 * Basically:
 * 1) Only 'differential' overrides needs special handling here. All others (replacing values or
 *    inserting/removing items from a collection) can be handled with simply storing current
 *    content of local data-block.
 * 2) We store the differential value into a second 'ghost' data-block, which is an empty ID of
 *    same type as the local one, where we only define values that need differential data.
 *
 * This avoids us having to modify 'real' data-block at write time (and restoring it afterwards),
 * which is inefficient, and potentially dangerous (in case of concurrent access...), while not
 * using much extra memory in typical cases.  It also ensures stored data-block always contains
 * exact same data as "desired" ones (kind of "baked" data-blocks).
 */

OverrideLibraryStorage *BKE_lib_override_library_operations_store_init(void)
{
  return BKE_main_new();
}

ID *BKE_lib_override_library_operations_store_start(Main *bmain,
                                                    OverrideLibraryStorage *override_storage,
                                                    ID *local)
{
  if (ID_IS_OVERRIDE_LIBRARY_TEMPLATE(local) || ID_IS_OVERRIDE_LIBRARY_VIRTUAL(local)) {
    /* This is actually purely local data with an override template, or one of those embedded IDs
     * (root node trees, master collections or shapekeys) that cannot have their own override.
     * Nothing to do here! */
    return NULL;
  }

  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));
  BLI_assert(override_storage != NULL);
  UNUSED_VARS_NDEBUG(override_storage);

  /* Forcefully ensure we know about all needed override operations. */
  BKE_lib_override_library_operations_create(bmain, local);

  ID *storage_id;
#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_START_AVERAGED(BKE_lib_override_library_operations_store_start);
#endif

  /* This is fully disabled for now, as it generated very hard to solve issues with Collections and
   * how they reference each-other in their parents/children relations.
   * Core of the issue is creating and storing those copies in a separate Main, while collection
   * copy code re-assign blindly parents/children, even if they do not belong to the same Main.
   * One solution could be to implement special flag as discussed below, and prevent any
   * other-ID-reference creation/update in that case (since no differential operation is expected
   * to involve those anyway). */
#if 0
  /* XXX TODO: We may also want a specialized handling of things here too, to avoid copying heavy
   * never-overridable data (like Mesh geometry etc.)? And also maybe avoid lib
   * reference-counting completely (shallow copy). */
  /* This would imply change in handling of user-count all over RNA
   * (and possibly all over Blender code).
   * Not impossible to do, but would rather see first is extra useless usual user handling is
   * actually a (performances) issue here, before doing it. */
  storage_id = BKE_id_copy((Main *)override_storage, local);

  if (storage_id != NULL) {
    PointerRNA rnaptr_reference, rnaptr_final, rnaptr_storage;
    RNA_id_pointer_create(local->override_library->reference, &rnaptr_reference);
    RNA_id_pointer_create(local, &rnaptr_final);
    RNA_id_pointer_create(storage_id, &rnaptr_storage);

    if (!RNA_struct_override_store(
            bmain, &rnaptr_final, &rnaptr_reference, &rnaptr_storage, local->override_library)) {
      BKE_id_free_ex(override_storage, storage_id, LIB_ID_FREE_NO_UI_USER, true);
      storage_id = NULL;
    }
  }
#else
  storage_id = NULL;
#endif

  local->override_library->storage = storage_id;

#ifdef DEBUG_OVERRIDE_TIMEIT
  TIMEIT_END_AVERAGED(BKE_lib_override_library_operations_store_start);
#endif
  return storage_id;
}

void BKE_lib_override_library_operations_store_end(
    OverrideLibraryStorage *UNUSED(override_storage), ID *local)
{
  BLI_assert(ID_IS_OVERRIDE_LIBRARY_REAL(local));

  /* Nothing else to do here really, we need to keep all temp override storage data-blocks in
   * memory until whole file is written anyway (otherwise we'd get mem pointers overlap). */
  local->override_library->storage = NULL;
}

void BKE_lib_override_library_operations_store_finalize(OverrideLibraryStorage *override_storage)
{
  /* We cannot just call BKE_main_free(override_storage), not until we have option to make
   * 'ghost' copies of IDs without increasing usercount of used data-blocks. */
  ID *id;

  FOREACH_MAIN_ID_BEGIN (override_storage, id) {
    BKE_id_free_ex(override_storage, id, LIB_ID_FREE_NO_UI_USER, true);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_free(override_storage);
}
