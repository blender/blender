/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's for freeing & deletion.
 */

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_key_types.h"

#include "BLI_utildefines.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "BKE_anim_data.hh"
#include "BKE_asset.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"

#include "BLO_readfile.hh"

#include "lib_intern.hh"

#include "DEG_depsgraph.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

using namespace blender::bke::id;

void BKE_libblock_free_data(ID *id, const bool do_id_user)
{
  if (id->properties) {
    IDP_FreePropertyContent_ex(id->properties, do_id_user);
    MEM_freeN(id->properties);
    id->properties = nullptr;
  }
  if (id->system_properties) {
    IDP_FreePropertyContent_ex(id->system_properties, do_id_user);
    MEM_freeN(id->system_properties);
    id->system_properties = nullptr;
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

  BKE_libblock_free_runtime_data(id);
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

void BKE_libblock_free_runtime_data(ID *id)
{
  if (id->runtime) {
    /* During "normal" file loading this data is released when versioning ends. Some versioning
     * code also deletes IDs, though. For example, in the startup blend file, brushes that were
     * replaced by assets are deleted. This means that the regular "delete this ID" flow (aka this
     * code here) also needs to free this data. */
    BLO_readfile_id_runtime_data_free(*id);

    MEM_SAFE_DELETE(id->runtime);
  }
}

static int id_free(Main *bmain, void *idv, int flag, const bool use_flag_from_idtag)
{
  ID *id = static_cast<ID *>(idv);

  if (use_flag_from_idtag) {
    if ((id->tag & ID_TAG_NO_MAIN) != 0) {
      flag |= LIB_ID_FREE_NO_MAIN | LIB_ID_FREE_NO_UI_USER | LIB_ID_FREE_NO_DEG_TAG;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_MAIN;
    }

    if ((id->tag & ID_TAG_NO_USER_REFCOUNT) != 0) {
      flag |= LIB_ID_FREE_NO_USER_REFCOUNT;
    }
    else {
      flag &= ~LIB_ID_FREE_NO_USER_REFCOUNT;
    }

    if ((id->tag & ID_TAG_NOT_ALLOCATED) != 0) {
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
      IDRemapper remapper;
      remapper.add(id, nullptr);
      remap_editor_id_reference_cb(remapper);
    }
  }

  if ((flag & LIB_ID_FREE_NO_MAIN) == 0) {
    ListBase *lb = which_libbase(bmain, type);
    BLI_remlink(lb, id);
    if ((flag & LIB_ID_FREE_NO_NAMEMAP_REMOVE) == 0) {
      BKE_main_namemap_remove_id(*bmain, *id);
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

void BKE_id_free_ex(Main *bmain, void *idv, const int flag_orig, const bool use_flag_from_idtag)
{
  /* ViewLayer resync needs to be delayed during Scene freeing, since internal relationships
   * between the Scene's master collection and its view_layers become invalid
   * (due to remapping). */
  if (bmain && (flag_orig & LIB_ID_FREE_NO_MAIN) == 0) {
    BKE_layer_collection_resync_forbid();
  }

  const ID_Type id_type = GS(static_cast<ID *>(idv)->name);

  int flag_final = id_free(bmain, idv, flag_orig, use_flag_from_idtag);

  if (bmain) {
    if ((flag_orig & LIB_ID_FREE_NO_MAIN) == 0) {
      BKE_layer_collection_resync_allow();
    }

    if ((flag_final & LIB_ID_FREE_NO_MAIN) == 0) {
      if (ELEM(id_type, ID_SCE, ID_GR, ID_OB)) {
        BKE_main_collection_sync_remap(bmain);
      }
    }
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
    const bool is_lib = GS(id->name) == ID_LI;

    BKE_libblock_unlink(bmain, id, false);

    BKE_id_free(bmain, id);

    if (is_lib) {
      BKE_library_main_rebuild_hierarchy(bmain);
    }
  }
}

static size_t id_delete(Main *bmain,
                        blender::Set<ID *> &ids_to_delete,
                        const int extra_remapping_flags)
{
  bool has_deleted_library = false;

  /* Used by batch tagged deletion, when we call BKE_id_free then, id is no more in Main database,
   * and has already properly unlinked its other IDs usages.
   * UI users are always cleared in BKE_libblock_remap_locked() call, so we can always skip it. */
  const int free_flag = LIB_ID_FREE_NO_UI_USER | LIB_ID_FREE_NO_MAIN |
                        LIB_ID_FREE_NO_USER_REFCOUNT;
  const int remapping_flags = (ID_REMAP_STORE_NEVER_NULL_USAGE | ID_REMAP_FORCE_NEVER_NULL_USAGE |
                               ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS | extra_remapping_flags);

  MainListsArray lbarray = BKE_main_lists_get(*bmain);
  const int base_count = lbarray.size();

  BKE_main_lock(bmain);
  BKE_layer_collection_resync_forbid();
  IDRemapper id_remapper;

  /* Main idea of batch deletion is to remove all IDs to be deleted from Main database.
   * This means that we won't have to loop over all deleted IDs to remove usages
   * of other deleted IDs.
   * This gives tremendous speed-up when deleting a large amount of IDs from a Main
   * containing thousands of these.
   * This also means that we have to be very careful here, as we bypass many 'common'
   * processing, hence risking to 'corrupt' at least user counts, if not IDs themselves. */
  bool keep_looping = true;
  while (keep_looping) {
    keep_looping = false;

    /* First tag and remove from Main all datablocks directly from target lib.
     * Note that we go forward here, since we want to check dependencies before users
     * (e.g. meshes before objects). Reduces the chances to have to loop many times in the
     * `while (keep_looking)` outer loop. */
    for (int i = 0; i < base_count; i++) {
      ListBase *lb = lbarray[i];
      ID *id_iter;

      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id_iter) {
        /* NOTE: in case we delete a library, we also delete all its datablocks! */
        if (ids_to_delete.contains(id_iter) ||
            (ID_IS_LINKED(id_iter) && ids_to_delete.contains(&id_iter->lib->id)))
        {
          BLI_remlink(lb, id_iter);
          BKE_main_namemap_remove_id(*bmain, *id_iter);
          ids_to_delete.add(id_iter);
          id_remapper.add(id_iter, nullptr);
          /* Do not tag as no_main now, we want to unlink it first (lower-level ID management
           * code has some specific handling of 'no main' IDs that would be a problem in that
           * case). */

          /* Forcefully also delete shapekeys of the deleted ID if any, 'orphaned' shapekeys are
           * not allowed in Blender and will cause lots of problem in modern code (liboverrides,
           * warning on write & read, etc.). */
          Key *shape_key = BKE_key_from_id(id_iter);
          if (shape_key && !ids_to_delete.contains(&shape_key->id)) {
            BLI_remlink(&bmain->shapekeys, &shape_key->id);
            BKE_main_namemap_remove_id(*bmain, shape_key->id);
            ids_to_delete.add(&shape_key->id);
            id_remapper.add(&shape_key->id, nullptr);
          }

          keep_looping = true;
        }
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }

    /* Will tag 'never nullptr' users of this ID too.
     *
     * NOTE: #BKE_libblock_unlink() cannot be used here, since it would ignore indirect
     * links, this can lead to nasty crashing here in second, actual deleting loop.
     * Also, this will also flag users of deleted data that cannot be unlinked
     * (object using deleted obdata, etc.), so that they also get deleted. */
    BKE_libblock_remap_multiple_locked(bmain, id_remapper, remapping_flags);
    for (ID *id_never_null_iter : id_remapper.never_null_users()) {
      ids_to_delete.add(id_never_null_iter);
    }
    id_remapper.clear();
  }

  /* Remapping above may have left some Library::runtime::archived_libraries items to nullptr,
   * clean this up and shrink the vector accordingly. */
  blender::bke::library::main_cleanup_parent_archives(*bmain);

  /* Since we removed IDs from Main, their own other IDs usages need to be removed 'manually'. */
  blender::Vector<ID *> cleanup_ids{ids_to_delete.begin(), ids_to_delete.end()};
  BKE_libblock_relink_multiple(
      bmain,
      cleanup_ids,
      ID_REMAP_TYPE_CLEANUP,
      id_remapper,
      (ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS | ID_REMAP_SKIP_USER_CLEAR));
  cleanup_ids.clear();

  /* Now we can safely mark that ID as not being in Main database anymore. */
  /* NOTE: This needs to be done in a separate loop than above, otherwise some user-counts of
   * deleted IDs may not be properly decreased by the remappings (since `NO_MAIN` ID user-counts
   * is never affected). */
  for (ID *id : ids_to_delete) {
    id->tag |= ID_TAG_NO_MAIN;
    /* User-count needs to be reset artificially, since some usages may not be cleared in batch
     * deletion (typically, if one deleted ID uses another deleted ID, this may not be cleared by
     * remapping code, depending on order in which these are handled). */
    id->us = ID_FAKE_USERS(id);

    if (!has_deleted_library && GS(id->name) == ID_LI) {
      has_deleted_library = true;
    }

    id_free(bmain, id, free_flag, false);
  }

  BKE_main_unlock(bmain);
  BKE_layer_collection_resync_allow();
  BKE_main_collection_sync_remap(bmain);

  if (has_deleted_library) {
    BKE_library_main_rebuild_hierarchy(bmain);
  }

  bmain->is_memfile_undo_written = false;
  return size_t(ids_to_delete.size());
}

void BKE_id_delete_ex(Main *bmain, void *idv, const int extra_remapping_flags)
{
  ID *id = static_cast<ID *>(idv);
  BLI_assert_msg((id->tag & ID_TAG_NO_MAIN) == 0, "Cannot be used with IDs outside of Main");

  blender::Set<ID *> ids_to_delete = {id};
  id_delete(bmain, ids_to_delete, extra_remapping_flags);
}

void BKE_id_delete(Main *bmain, void *idv)
{
  BKE_id_delete_ex(bmain, idv, 0);
}

size_t BKE_id_multi_tagged_delete(Main *bmain)
{
  blender::Set<ID *> ids_to_delete;
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (id_iter->tag & ID_TAG_DOIT) {
      ids_to_delete.add(id_iter);
    }
  }
  FOREACH_MAIN_ID_END;
  return id_delete(bmain, ids_to_delete, 0);
}

size_t BKE_id_multi_delete(Main *bmain, blender::Set<ID *> &ids_to_delete)
{
  return id_delete(bmain, ids_to_delete, 0);
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
