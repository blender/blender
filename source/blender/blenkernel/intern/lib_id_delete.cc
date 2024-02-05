/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's for freeing & deletion.
 */

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_key_types.h"

#include "BLI_utildefines.h"

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_vector.hh"

#include "BKE_anim_data.h"
#include "BKE_asset.hh"
#include "BKE_idprop.h"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"

#include "lib_intern.hh"

#include "DEG_depsgraph.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

static CLG_LogRef LOG = {"bke.lib_id_delete"};

void BKE_libblock_free_data(ID *id, const bool do_id_user)
{
  if (id->properties) {
    IDP_FreePropertyContent_ex(id->properties, do_id_user);
    MEM_freeN(id->properties);
    id->properties = nullptr;
  }

  if (id->override_library) {
    BKE_lib_override_library_free(&id->override_library, do_id_user);
    id->override_library = nullptr;
  }

  if (id->asset_data) {
    BKE_asset_metadata_free(&id->asset_data);
  }

  if (id->library_weak_reference != nullptr) {
    MEM_freeN(id->library_weak_reference);
  }

  BKE_animdata_free(id, do_id_user);
}

void BKE_libblock_free_datablock(ID *id, const int /*flag*/)
{
  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != nullptr) {
    if (idtype_info->free_data != nullptr) {
      idtype_info->free_data(id);
    }
    return;
  }

  BLI_assert_msg(0, "IDType Missing IDTypeInfo");
}

static int id_free(Main *bmain, void *idv, int flag, const bool use_flag_from_idtag)
{
  ID *id = static_cast<ID *>(idv);

  if (use_flag_from_idtag) {
    if ((id->tag & LIB_TAG_NO_MAIN) != 0) {
      flag |= LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_UI_USER | LIB_ID_FREE_NO_DEG_TAG;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_MAIN;
    }

    if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0) {
      flag |= LIB_ID_FREE_NO_USER_REFCOUNT;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_USER_REFCOUNT;
    }

    if ((id->tag & LIB_TAG_NOT_ALLOCATED) != 0) {
      flag |= LIB_ID_FREE_NOT_ALLOCATED;
    }
    else {
      flag &= ~LIB_ID_FREE_NOT_ALLOCATED;
    }
  }

  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || bmain != nullptr);
  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || (flag & LIB_ID_FREE_NOT_ALLOCATED) == 0);
  BLI_assert((flag & LIB_ID_FREE_NO_MAIN) != 0 || (flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0);

  const short type = GS(id->name);

  if (bmain && (flag & LIB_ID_FREE_NO_DEG_TAG) == 0) {
    BLI_assert(bmain->is_locked_for_linking == false);

    DEG_id_type_tag(bmain, type);
  }

  BKE_libblock_free_data_py(id);

  Key *key = ((flag & LIB_ID_FREE_NO_MAIN) == 0) ? BKE_key_from_id(id) : nullptr;

  if ((flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0) {
    BKE_libblock_relink_ex(bmain, id, nullptr, nullptr, ID_REMAP_SKIP_USER_CLEAR);
  }

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0 && key != nullptr) {
    id_free(bmain, &key->id, flag, use_flag_from_idtag);
  }

  BKE_libblock_free_datablock(id, flag);

  /* avoid notifying on removed data */
  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_main_lock(bmain);
  }

  if ((flag & LIB_ID_FREE_NO_UI_USER) == 0) {
    if (free_notifier_reference_cb) {
      free_notifier_reference_cb(id);
    }

    if (remap_editor_id_reference_cb) {
      IDRemapper *remapper = BKE_id_remapper_create();
      BKE_id_remapper_add(remapper, id, nullptr);
      remap_editor_id_reference_cb(remapper);
      BKE_id_remapper_free(remapper);
    }
  }

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    ListBase *lb = which_libbase(bmain, type);
    BLI_remlink(lb, id);
    if ((flag & LIB_ID_FREE_NO_NAMEMAP_REMOVE) == 0) {
      BKE_main_namemap_remove_name(bmain, id, id->name + 2);
    }
  }

  BKE_libblock_free_data(id, (flag & LIB_ID_FREE_NO_USER_REFCOUNT) == 0);

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_main_unlock(bmain);
  }

  if ((flag & LIB_ID_FREE_NOT_ALLOCATED) == 0) {
    MEM_freeN(id);
  }

  return flag;
}

void BKE_id_free_ex(Main *bmain, void *idv, int flag, const bool use_flag_from_idtag)
{
  /* ViewLayer resync needs to be delayed during Scene freeing, since internal relationships
   * between the Scene's master collection and its view_layers become invalid
   * (due to remapping). */
  BKE_layer_collection_resync_forbid();

  flag = id_free(bmain, idv, flag, use_flag_from_idtag);

  BKE_layer_collection_resync_allow();
  if (bmain && (flag & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_main_collection_sync_remap(bmain);
  }
}

void BKE_id_free(Main *bmain, void *idv)
{
  BKE_id_free_ex(bmain, idv, 0, true);
}

void BKE_id_free_us(Main *bmain, void *idv) /* test users */
{
  ID *id = static_cast<ID *>(idv);

  id_us_min(id);

  /* XXX This is a temp (2.77) hack so that we keep same behavior as in 2.76 regarding collections
   *     when deleting an object. Since only 'user_one' usage of objects is collections,
   *     and only 'real user' usage of objects is scenes, removing that 'user_one' tag when there
   *     is no more real (scene) users of an object ensures it gets fully unlinked.
   *     But only for local objects, not linked ones!
   *     Otherwise, there is no real way to get rid of an object anymore -
   *     better handling of this is TODO.
   */
  if ((GS(id->name) == ID_OB) && (id->us == 1) && !ID_IS_LINKED(id)) {
    id_us_clear_real(id);
  }

  if (id->us == 0) {
    BKE_libblock_unlink(bmain, id, false, false);

    BKE_id_free(bmain, id);
  }
}

static size_t id_delete(Main *bmain,
                        const bool do_tagged_deletion,
                        const int extra_remapping_flags)
{
  const int tag = LIB_TAG_DOIT;
  ListBase *lbarray[INDEX_ID_MAX];
  int base_count, i;

  /* Used by batch tagged deletion, when we call BKE_id_free then, id is no more in Main database,
   * and has already properly unlinked its other IDs usages.
   * UI users are always cleared in BKE_libblock_remap_locked() call, so we can always skip it. */
  const int free_flag = LIB_ID_FREE_NO_UI_USER |
                        (do_tagged_deletion ? LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_USER_REFCOUNT :
                                              0);
  const int remapping_flags = (ID_REMAP_FLAG_NEVER_NULL_USAGE | ID_REMAP_FORCE_NEVER_NULL_USAGE |
                               ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS | extra_remapping_flags);
  ListBase tagged_deleted_ids = {nullptr};

  base_count = set_listbasepointers(bmain, lbarray);

  BKE_main_lock(bmain);
  if (do_tagged_deletion) {
    IDRemapper *id_remapper = BKE_id_remapper_create();
    BKE_layer_collection_resync_forbid();

    /* Main idea of batch deletion is to remove all IDs to be deleted from Main database.
     * This means that we won't have to loop over all deleted IDs to remove usages
     * of other deleted IDs.
     * This gives tremendous speed-up when deleting a large amount of IDs from a Main
     * containing thousands of those.
     * This also means that we have to be very careful here, as we by-pass many 'common'
     * processing, hence risking to 'corrupt' at least user counts, if not IDs themselves. */
    bool keep_looping = true;
    while (keep_looping) {
      ID *id, *id_next;
      keep_looping = false;

      /* First tag and remove from Main all datablocks directly from target lib.
       * Note that we go forward here, since we want to check dependencies before users
       * (e.g. meshes before objects). Avoids to have to loop twice. */
      for (i = 0; i < base_count; i++) {
        ListBase *lb = lbarray[i];

        for (id = static_cast<ID *>(lb->first); id; id = id_next) {
          id_next = static_cast<ID *>(id->next);
          /* NOTE: in case we delete a library, we also delete all its datablocks! */
          if ((id->tag & tag) || (ID_IS_LINKED(id) && (id->lib->id.tag & tag))) {
            BLI_remlink(lb, id);
            BKE_main_namemap_remove_name(bmain, id, id->name + 2);
            BLI_addtail(&tagged_deleted_ids, id);
            BKE_id_remapper_add(id_remapper, id, nullptr);
            /* Do not tag as no_main now, we want to unlink it first (lower-level ID management
             * code has some specific handling of 'no main' IDs that would be a problem in that
             * case). */
            id->tag |= tag;

            /* Forcefully also delete shapekeys of the deleted ID if any, 'orphaned' shapekeys are
             * not allowed in Blender and will cause lots of problem in modern code (liboverrides,
             * warning on write & read, etc.). */
            Key *shape_key = BKE_key_from_id(id);
            if (shape_key && (shape_key->id.tag & tag) == 0) {
              BLI_remlink(&bmain->shapekeys, &shape_key->id);
              BKE_main_namemap_remove_name(bmain, &shape_key->id, shape_key->id.name + 2);
              BLI_addtail(&tagged_deleted_ids, &shape_key->id);
              BKE_id_remapper_add(id_remapper, &shape_key->id, nullptr);
              shape_key->id.tag |= tag;
            }

            keep_looping = true;
          }
        }
      }

      /* Will tag 'never nullptr' users of this ID too.
       *
       * NOTE: #BKE_libblock_unlink() cannot be used here, since it would ignore indirect
       * links, this can lead to nasty crashing here in second, actual deleting loop.
       * Also, this will also flag users of deleted data that cannot be unlinked
       * (object using deleted obdata, etc.), so that they also get deleted. */
      BKE_libblock_remap_multiple_locked(bmain, id_remapper, remapping_flags);
      BKE_id_remapper_clear(id_remapper);
    }

    /* Since we removed IDs from Main, their own other IDs usages need to be removed 'manually'. */
    blender::Vector<ID *> cleanup_ids;
    for (ID *id = static_cast<ID *>(tagged_deleted_ids.first); id;
         id = static_cast<ID *>(id->next))
    {
      cleanup_ids.append(id);
    }
    BKE_libblock_relink_multiple(bmain,
                                 cleanup_ids,
                                 ID_REMAP_TYPE_CLEANUP,
                                 id_remapper,
                                 ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS |
                                     ID_REMAP_SKIP_USER_CLEAR);
    cleanup_ids.clear();
    BKE_id_remapper_free(id_remapper);

    BKE_layer_collection_resync_allow();
    BKE_main_collection_sync_remap(bmain);

    /* Now we can safely mark that ID as not being in Main database anymore. */
    /* NOTE: This needs to be done in a separate loop than above, otherwise some user-counts of
     * deleted IDs may not be properly decreased by the remappings (since `NO_MAIN` ID user-counts
     * is never affected). */
    for (ID *id = static_cast<ID *>(tagged_deleted_ids.first); id;
         id = static_cast<ID *>(id->next))
    {
      id->tag |= LIB_TAG_NO_MAIN;
      /* User-count needs to be reset artificially, since some usages may not be cleared in batch
       * deletion (typically, if one deleted ID uses another deleted ID, this may not be cleared by
       * remapping code, depending on order in which these are handled). */
      id->us = ID_FAKE_USERS(id);
    }
  }
  else {
    /* First tag all data-blocks directly from target lib.
     * Note that we go forward here, since we want to check dependencies before users
     * (e.g. meshes before objects).
     * Avoids to have to loop twice. */
    IDRemapper *remapper = BKE_id_remapper_create();
    for (i = 0; i < base_count; i++) {
      ListBase *lb = lbarray[i];
      ID *id, *id_next;
      BKE_id_remapper_clear(remapper);

      for (id = static_cast<ID *>(lb->first); id; id = id_next) {
        id_next = static_cast<ID *>(id->next);
        /* NOTE: in case we delete a library, we also delete all its datablocks! */
        if ((id->tag & tag) || (ID_IS_LINKED(id) && (id->lib->id.tag & tag))) {
          id->tag |= tag;
          BKE_id_remapper_add(remapper, id, nullptr);
        }
      }

      if (BKE_id_remapper_is_empty(remapper)) {
        continue;
      }

      /* Will tag 'never nullptr' users of this ID too.
       *
       * NOTE: #BKE_libblock_unlink() cannot be used here, since it would ignore indirect
       * links, this can lead to nasty crashing here in second, actual deleting loop.
       * Also, this will also flag users of deleted data that cannot be unlinked
       * (object using deleted obdata, etc.), so that they also get deleted. */
      BKE_libblock_remap_multiple_locked(bmain, remapper, remapping_flags);
    }
    BKE_id_remapper_free(remapper);
  }

  BKE_main_unlock(bmain);

  /* ViewLayer resync needs to be delayed during Scene freeing, since internal relationships
   * between the Scene's master collection and its view_layers become invalid
   * (due to remapping). */
  BKE_layer_collection_resync_forbid();

  /* In usual reversed order, such that all usage of a given ID, even 'never nullptr' ones,
   * have been already cleared when we reach it
   * (e.g. Objects being processed before meshes, they'll have already released their 'reference'
   * over meshes when we come to freeing obdata). */
  size_t num_datablocks_deleted = 0;
  for (i = do_tagged_deletion ? 1 : base_count; i--;) {
    ListBase *lb = lbarray[i];
    ID *id, *id_next;

    for (id = static_cast<ID *>(do_tagged_deletion ? tagged_deleted_ids.first : lb->first); id;
         id = id_next)
    {
      id_next = static_cast<ID *>(id->next);
      if (id->tag & tag) {
        if (((id->tag & LIB_TAG_EXTRAUSER_SET) == 0 && ID_REAL_USERS(id) != 0) ||
            ((id->tag & LIB_TAG_EXTRAUSER_SET) != 0 && ID_REAL_USERS(id) != 1))
        {
          CLOG_ERROR(&LOG,
                     "Deleting %s which still has %d users (including %d 'extra' shallow users)\n",
                     id->name,
                     ID_REAL_USERS(id),
                     (id->tag & LIB_TAG_EXTRAUSER_SET) != 0 ? 1 : 0);
        }
        id_free(bmain, id, free_flag, !do_tagged_deletion);
        ++num_datablocks_deleted;
      }
    }
  }

  BKE_layer_collection_resync_allow();
  BKE_main_collection_sync_remap(bmain);

  bmain->is_memfile_undo_written = false;
  return num_datablocks_deleted;
}

void BKE_id_delete_ex(Main *bmain, void *idv, const int extra_remapping_flags)
{
  BLI_assert_msg((((ID *)idv)->tag & LIB_TAG_NO_MAIN) == 0,
                 "Cannot be used with IDs outside of Main");

  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  ((ID *)idv)->tag |= LIB_TAG_DOIT;

  id_delete(bmain, false, extra_remapping_flags);
}

void BKE_id_delete(Main *bmain, void *idv)
{
  BKE_id_delete_ex(bmain, idv, 0);
}

size_t BKE_id_multi_tagged_delete(Main *bmain)
{
  return id_delete(bmain, true, 0);
}

/* -------------------------------------------------------------------- */
/** \name Python Data Handling
 * \{ */

void BKE_libblock_free_data_py(ID *id)
{
#ifdef WITH_PYTHON
#  ifdef WITH_PYTHON_SAFETY
  BPY_id_release(id);
#  endif
  if (id->py_instance) {
    BPY_DECREF_RNA_INVALIDATE(id->py_instance);
  }
#else
  UNUSED_VARS(id);
#endif
}

/** \} */
