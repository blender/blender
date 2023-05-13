/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_node_types.h"
#include "DNA_workspace_types.h"

#include "BLI_utildefines.h"

#include "BLI_alloca.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_string_utils.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_armature.h"
#include "BKE_asset.h"
#include "BKE_bpath.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_main_namemap.h"
#include "BKE_node.h"
#include "BKE_rigidbody.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "BLO_read_write.h"

#include "atomic_ops.h"

#include "lib_intern.h"

//#define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

static CLG_LogRef LOG = {.identifier = "bke.lib_id"};

IDTypeInfo IDType_ID_LINK_PLACEHOLDER = {
    .id_code = ID_LINK_PLACEHOLDER,
    .id_filter = 0,
    .main_listbase_index = INDEX_ID_NULL,
    .struct_size = sizeof(ID),
    .name = "LinkPlaceholder",
    .name_plural = "link_placeholders",
    .translation_context = BLT_I18NCONTEXT_ID_ID,
    .flags = IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING,
    .asset_type_info = NULL,

    .init_data = NULL,
    .copy_data = NULL,
    .free_data = NULL,
    .make_local = NULL,
    .foreach_id = NULL,
    .foreach_cache = NULL,
    .foreach_path = NULL,
    .owner_pointer_get = NULL,

    .blend_write = NULL,
    .blend_read_data = NULL,
    .blend_read_lib = NULL,
    .blend_read_expand = NULL,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

/* GS reads the memory pointed at in a specific ordering.
 * only use this definition, makes little and big endian systems
 * work fine, in conjunction with MAKE_ID */

/* ************* general ************************ */

/**
 * Rewrites a relative path to be relative to the main file - unless the path is
 * absolute, in which case it is not altered.
 */
static bool lib_id_library_local_paths_callback(BPathForeachPathData *bpath_data,
                                                char *r_path_dst,
                                                const char *path_src)
{
  const char **data = bpath_data->user_data;
  /* be sure there is low chance of the path being too short */
  char filepath[(FILE_MAXDIR * 2) + FILE_MAXFILE];
  const char *base_new = data[0];
  const char *base_old = data[1];

  if (BLI_path_is_rel(base_old)) {
    CLOG_ERROR(&LOG, "old base path '%s' is not absolute.", base_old);
    return false;
  }

  /* Make referenced file absolute. This would be a side-effect of
   * BLI_path_normalize, but we do it explicitly so we know if it changed. */
  BLI_strncpy(filepath, path_src, FILE_MAX);
  if (BLI_path_abs(filepath, base_old)) {
    /* Path was relative and is now absolute. Remap.
     * Important BLI_path_normalize runs before the path is made relative
     * because it won't work for paths that start with "//../" */
    BLI_path_normalize(filepath);
    BLI_path_rel(filepath, base_new);
    BLI_strncpy(r_path_dst, filepath, FILE_MAX);
    return true;
  }

  /* Path was not relative to begin with. */
  return false;
}

/**
 * This has to be called from each make_local_* func, we could call from BKE_lib_id_make_local()
 * but then the make local functions would not be self contained.
 * Also note that the id _must_ have a library - campbell */
/* TODO: This can probably be replaced by an ID-level version of #BKE_bpath_relative_rebase. */
static void lib_id_library_local_paths(Main *bmain, Library *lib, ID *id)
{
  const char *bpath_user_data[2] = {BKE_main_blendfile_path(bmain), lib->filepath_abs};

  BKE_bpath_foreach_path_id(
      &(BPathForeachPathData){.bmain = bmain,
                              .callback_function = lib_id_library_local_paths_callback,
                              .flag = BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE,
                              .user_data = (void *)bpath_user_data},
      id);
}

static int lib_id_clear_library_data_users_update_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *id = cb_data->user_data;
  if (*cb_data->id_pointer == id) {
    /* Even though the ID itself remain the same after being made local, from depsgraph point of
     * view this is a different ID. Hence we need to tag all of its users for COW update. */
    DEG_id_tag_update_ex(
        cb_data->bmain, cb_data->id_owner, ID_RECALC_TAG_FOR_UNDO | ID_RECALC_COPY_ON_WRITE);
    return IDWALK_RET_STOP_ITER;
  }
  return IDWALK_RET_NOP;
}

void BKE_lib_id_clear_library_data(Main *bmain, ID *id, const int flags)
{
  const bool id_in_mainlist = (id->tag & LIB_TAG_NO_MAIN) == 0 &&
                              (id->flag & LIB_EMBEDDED_DATA) == 0;

  if (id_in_mainlist) {
    BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  }

  lib_id_library_local_paths(bmain, id->lib, id);

  id_fake_user_clear(id);

  id->lib = NULL;
  id->tag &= ~(LIB_TAG_INDIRECT | LIB_TAG_EXTERN);
  id->flag &= ~LIB_INDIRECT_WEAK_LINK;
  if (id_in_mainlist) {
    if (BKE_id_new_name_validate(bmain, which_libbase(bmain, GS(id->name)), id, NULL, false)) {
      bmain->is_memfile_undo_written = false;
    }
  }

  /* Conceptually, an ID made local is not the same as the linked one anymore. Reflect that by
   * regenerating its session UUID. */
  if ((id->tag & LIB_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uuid_renew(id);
  }

  if (ID_IS_ASSET(id)) {
    if ((flags & LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR) != 0) {
      BKE_asset_metadata_free(&id->asset_data);
    }
    else {
      /* Assets should always have a fake user. Ensure this is the case after "Make Local". */
      id_fake_user_set(id);
    }
  }

  /* We need to tag this IDs and all of its users, conceptually new local ID and original linked
   * ones are two completely different data-blocks that were virtually remapped, even though in
   * reality they remain the same data. For undo this info is critical now. */
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_COPY_ON_WRITE);
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    BKE_library_foreach_ID_link(
        bmain, id_iter, lib_id_clear_library_data_users_update_cb, id, IDWALK_READONLY);
  }
  FOREACH_MAIN_ID_END;

  /* Internal shape key blocks inside data-blocks also stores id->lib,
   * make sure this stays in sync (note that we do not need any explicit handling for real EMBEDDED
   * IDs here, this is down automatically in `lib_id_expand_local_cb()`. */
  Key *key = BKE_key_from_id(id);
  if (key != NULL) {
    BKE_lib_id_clear_library_data(bmain, &key->id, flags);
  }

  /* Even though the ID itself remain the same after being made local, from depsgraph point of view
   * this is a different ID. Hence we rebuild depsgraph relationships. */
  DEG_relations_tag_update(bmain);
}

void id_lib_extern(ID *id)
{
  if (id && ID_IS_LINKED(id)) {
    BLI_assert(BKE_idtype_idcode_is_linkable(GS(id->name)));
    if (id->tag & LIB_TAG_INDIRECT) {
      id->tag &= ~LIB_TAG_INDIRECT;
      id->flag &= ~LIB_INDIRECT_WEAK_LINK;
      id->tag |= LIB_TAG_EXTERN;
      id->lib->parent = NULL;
    }
  }
}

void id_lib_indirect_weak_link(ID *id)
{
  if (id && ID_IS_LINKED(id)) {
    BLI_assert(BKE_idtype_idcode_is_linkable(GS(id->name)));
    if (id->tag & LIB_TAG_INDIRECT) {
      id->flag |= LIB_INDIRECT_WEAK_LINK;
    }
  }
}

void id_us_ensure_real(ID *id)
{
  if (id) {
    const int limit = ID_FAKE_USERS(id);
    id->tag |= LIB_TAG_EXTRAUSER;
    if (id->us <= limit) {
      if (id->us < limit || ((id->us == limit) && (id->tag & LIB_TAG_EXTRAUSER_SET))) {
        CLOG_ERROR(&LOG,
                   "ID user count error: %s (from '%s')",
                   id->name,
                   id->lib ? id->lib->filepath_abs : "[Main]");
      }
      id->us = limit + 1;
      id->tag |= LIB_TAG_EXTRAUSER_SET;
    }
  }
}

void id_us_clear_real(ID *id)
{
  if (id && (id->tag & LIB_TAG_EXTRAUSER)) {
    if (id->tag & LIB_TAG_EXTRAUSER_SET) {
      id->us--;
      BLI_assert(id->us >= ID_FAKE_USERS(id));
    }
    id->tag &= ~(LIB_TAG_EXTRAUSER | LIB_TAG_EXTRAUSER_SET);
  }
}

void id_us_plus_no_lib(ID *id)
{
  if (id) {
    if ((id->tag & LIB_TAG_EXTRAUSER) && (id->tag & LIB_TAG_EXTRAUSER_SET)) {
      BLI_assert(id->us >= 1);
      /* No need to increase count, just tag extra user as no more set.
       * Avoids annoying & inconsistent +1 in user count. */
      id->tag &= ~LIB_TAG_EXTRAUSER_SET;
    }
    else {
      BLI_assert(id->us >= 0);
      id->us++;
    }
  }
}

void id_us_plus(ID *id)
{
  if (id) {
    id_us_plus_no_lib(id);
    id_lib_extern(id);
  }
}

void id_us_min(ID *id)
{
  if (id) {
    const int limit = ID_FAKE_USERS(id);

    if (id->us <= limit) {
      if (!ID_TYPE_IS_DEPRECATED(GS(id->name))) {
        /* Do not assert on deprecated ID types, we cannot really ensure that their ID
         * reference-counting is valid. */
        CLOG_ERROR(&LOG,
                   "ID user decrement error: %s (from '%s'): %d <= %d",
                   id->name,
                   id->lib ? id->lib->filepath_abs : "[Main]",
                   id->us,
                   limit);
      }
      id->us = limit;
    }
    else {
      id->us--;
    }

    if ((id->us == limit) && (id->tag & LIB_TAG_EXTRAUSER)) {
      /* We need an extra user here, but never actually incremented user count for it so far,
       * do it now. */
      id_us_ensure_real(id);
    }
  }
}

void id_fake_user_set(ID *id)
{
  if (id && !(id->flag & LIB_FAKEUSER)) {
    id->flag |= LIB_FAKEUSER;
    id_us_plus(id);
  }
}

void id_fake_user_clear(ID *id)
{
  if (id && (id->flag & LIB_FAKEUSER)) {
    id->flag &= ~LIB_FAKEUSER;
    id_us_min(id);
  }
}

void BKE_id_newptr_and_tag_clear(ID *id)
{
  /* We assume that if this ID has no new ID, its embedded data has not either. */
  if (id->newid == NULL) {
    return;
  }

  id->newid->tag &= ~LIB_TAG_NEW;
  id->newid = NULL;

  /* Deal with embedded data too. */
  /* NOTE: even though ShapeKeys are not technically embedded data currently, they behave as such
   * in most cases, so for sake of consistency treat them as such here. Also mirrors the behavior
   * in `BKE_lib_id_make_local`. */
  Key *key = BKE_key_from_id(id);
  if (key != NULL) {
    BKE_id_newptr_and_tag_clear(&key->id);
  }
  bNodeTree *ntree = ntreeFromID(id);
  if (ntree != NULL) {
    BKE_id_newptr_and_tag_clear(&ntree->id);
  }
  if (GS(id->name) == ID_SCE) {
    Collection *master_collection = ((Scene *)id)->master_collection;
    if (master_collection != NULL) {
      BKE_id_newptr_and_tag_clear(&master_collection->id);
    }
  }
}

static int lib_id_expand_local_cb(LibraryIDLinkCallbackData *cb_data)
{
  Main *bmain = cb_data->bmain;
  ID *id_self = cb_data->id_self;
  ID **id_pointer = cb_data->id_pointer;
  int const cb_flag = cb_data->cb_flag;
  const int flags = POINTER_AS_INT(cb_data->user_data);

  if (cb_flag & IDWALK_CB_LOOPBACK) {
    /* We should never have anything to do with loop-back pointers here. */
    return IDWALK_RET_NOP;
  }

  if (cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    /* Embedded data-blocks need to be made fully local as well.
     * Note however that in some cases (when owner ID had to be duplicated instead of being made
     * local directly), its embedded IDs should also have already been duplicated, and hence be
     * fully local here already. */
    if (*id_pointer != NULL && ID_IS_LINKED(*id_pointer)) {
      BLI_assert(*id_pointer != id_self);

      BKE_lib_id_clear_library_data(bmain, *id_pointer, flags);
    }
    return IDWALK_RET_NOP;
  }

  /* Can happen that we get un-linkable ID here, e.g. with shape-key referring to itself
   * (through drivers)...
   * Just skip it, shape key can only be either indirectly linked, or fully local, period.
   * And let's curse one more time that stupid useless shape-key ID type! */
  if (*id_pointer && *id_pointer != id_self &&
      BKE_idtype_idcode_is_linkable(GS((*id_pointer)->name)))
  {
    id_lib_extern(*id_pointer);
  }

  return IDWALK_RET_NOP;
}

void BKE_lib_id_expand_local(Main *bmain, ID *id, const int flags)
{
  BKE_library_foreach_ID_link(
      bmain, id, lib_id_expand_local_cb, POINTER_FROM_INT(flags), IDWALK_READONLY);
}

/**
 * Ensure new (copied) ID is fully made local.
 */
void lib_id_copy_ensure_local(Main *bmain, const ID *old_id, ID *new_id, const int flags)
{
  if (ID_IS_LINKED(old_id)) {
    BKE_lib_id_expand_local(bmain, new_id, flags);
    lib_id_library_local_paths(bmain, old_id->lib, new_id);
  }
}

void BKE_lib_id_make_local_generic_action_define(
    struct Main *bmain, struct ID *id, int flags, bool *r_force_local, bool *r_force_copy)
{
  bool force_local = (flags & LIB_ID_MAKELOCAL_FORCE_LOCAL) != 0;
  bool force_copy = (flags & LIB_ID_MAKELOCAL_FORCE_COPY) != 0;
  BLI_assert(force_copy == false || force_copy != force_local);

  if (force_local || force_copy) {
    /* Already set by caller code, nothing to do here. */
    *r_force_local = force_local;
    *r_force_copy = force_copy;
    return;
  }

  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;
  bool is_local = false, is_lib = false;

  /* - no user (neither lib nor local): make local (happens e.g. with UI-used only data).
   * - only lib users: do nothing (unless force_local is set)
   * - only local users: make local
   * - mixed: make copy
   * In case we make a whole lib's content local,
   * we always want to localize, and we skip remapping (done later).
   */

  BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);
  if (!lib_local && !is_local && !is_lib) {
    force_local = true;
  }
  else if (lib_local || is_local) {
    if (!is_lib) {
      force_local = true;
    }
    else {
      force_copy = true;
    }
  }

  *r_force_local = force_local;
  *r_force_copy = force_copy;
}

void BKE_lib_id_make_local_generic(Main *bmain, ID *id, const int flags)
{
  if (!ID_IS_LINKED(id)) {
    return;
  }

  bool force_local, force_copy;
  BKE_lib_id_make_local_generic_action_define(bmain, id, flags, &force_local, &force_copy);

  if (force_local) {
    BKE_lib_id_clear_library_data(bmain, id, flags);
    BKE_lib_id_expand_local(bmain, id, flags);
  }
  else if (force_copy) {
    ID *id_new = BKE_id_copy(bmain, id);

    /* Should not fail in expected use cases,
     * but a few ID types cannot be copied (LIB, WM, SCR...). */
    if (id_new != NULL) {
      id_new->us = 0;

      /* setting newid is mandatory for complex make_lib_local logic... */
      ID_NEW_SET(id, id_new);
      Key *key = BKE_key_from_id(id), *key_new = BKE_key_from_id(id);
      if (key && key_new) {
        ID_NEW_SET(key, key_new);
      }
      bNodeTree *ntree = ntreeFromID(id), *ntree_new = ntreeFromID(id_new);
      if (ntree && ntree_new) {
        ID_NEW_SET(ntree, ntree_new);
      }
      if (GS(id->name) == ID_SCE) {
        Collection *master_collection = ((Scene *)id)->master_collection,
                   *master_collection_new = ((Scene *)id_new)->master_collection;
        if (master_collection && master_collection_new) {
          ID_NEW_SET(master_collection, master_collection_new);
        }
      }

      const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;
      if (!lib_local) {
        BKE_libblock_remap(bmain, id, id_new, ID_REMAP_SKIP_INDIRECT_USAGE);
      }
    }
  }
}

bool BKE_lib_id_make_local(Main *bmain, ID *id, const int flags)
{
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;

  /* We don't care whether ID is directly or indirectly linked
   * in case we are making a whole lib local... */
  if (!lib_local && (id->tag & LIB_TAG_INDIRECT)) {
    return false;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info == NULL) {
    BLI_assert_msg(0, "IDType Missing IDTypeInfo");
    return false;
  }

  BLI_assert((idtype_info->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0);

  if (idtype_info->make_local != NULL) {
    idtype_info->make_local(bmain, id, flags);
  }
  else {
    BKE_lib_id_make_local_generic(bmain, id, flags);
  }

  return true;
}

struct IDCopyLibManagementData {
  const ID *id_src;
  ID *id_dst;
  int flag;
};

/** Increases user-count as required, and remap self ID pointers. */
static int id_copy_libmanagement_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  ID *id = *id_pointer;
  const int cb_flag = cb_data->cb_flag;
  struct IDCopyLibManagementData *data = cb_data->user_data;

  /* Remap self-references to new copied ID. */
  if (id == data->id_src) {
    /* We cannot use id_self here, it is not *always* id_dst (thanks to $£!+@#&/? nodetrees). */
    id = *id_pointer = data->id_dst;
  }

  /* Increase used IDs refcount if needed and required. */
  if ((data->flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0 && (cb_flag & IDWALK_CB_USER)) {
    if ((data->flag & LIB_ID_CREATE_NO_MAIN) != 0) {
      BLI_assert(cb_data->id_self->tag & LIB_TAG_NO_MAIN);
      id_us_plus_no_lib(id);
    }
    else {
      id_us_plus(id);
    }
  }

  return IDWALK_RET_NOP;
}

bool BKE_id_copy_is_allowed(const ID *id)
{
#define LIB_ID_TYPES_NOCOPY ID_LI, ID_SCR, ID_WM, ID_WS /* Not supported */

  return !ID_TYPE_IS_DEPRECATED(GS(id->name)) && !ELEM(GS(id->name), LIB_ID_TYPES_NOCOPY);

#undef LIB_ID_TYPES_NOCOPY
}

ID *BKE_id_copy_ex(Main *bmain, const ID *id, ID **r_newid, const int flag)
{
  ID *newid = (r_newid != NULL) ? *r_newid : NULL;
  /* Make sure destination pointer is all good. */
  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
    newid = NULL;
  }
  else {
    if (newid != NULL) {
      /* Allow some garbage non-initialized memory to go in, and clean it up here. */
      const size_t size = BKE_libblock_get_alloc_info(GS(id->name), NULL);
      memset(newid, 0, size);
    }
  }

  /* Early output if source is NULL. */
  if (id == NULL) {
    return NULL;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if ((idtype_info->flags & IDTYPE_FLAGS_NO_COPY) != 0) {
      return NULL;
    }

    BKE_libblock_copy_ex(bmain, id, &newid, flag);

    if (idtype_info->copy_data != NULL) {
      idtype_info->copy_data(bmain, newid, id, flag);
    }
  }
  else {
    BLI_assert_msg(0, "IDType Missing IDTypeInfo");
  }

  /* Update ID refcount, remap pointers to self in new ID. */
  struct IDCopyLibManagementData data = {
      .id_src = id,
      .id_dst = newid,
      .flag = flag,
  };
  BKE_library_foreach_ID_link(bmain, newid, id_copy_libmanagement_cb, &data, IDWALK_NOP);

  /* Do not make new copy local in case we are copying outside of main...
   * XXX TODO: is this behavior OK, or should we need own flag to control that? */
  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    BLI_assert((flag & LIB_ID_COPY_KEEP_LIB) == 0);
    lib_id_copy_ensure_local(bmain, id, newid, 0);
  }
  else {
    newid->lib = id->lib;
  }

  if (r_newid != NULL) {
    *r_newid = newid;
  }

  return newid;
}

ID *BKE_id_copy(Main *bmain, const ID *id)
{
  return BKE_id_copy_ex(bmain, id, NULL, LIB_ID_COPY_DEFAULT);
}

ID *BKE_id_copy_for_duplicate(Main *bmain,
                              ID *id,
                              const eDupli_ID_Flags duplicate_flags,
                              const int copy_flags)
{
  if (id == NULL) {
    return id;
  }
  if (id->newid == NULL) {
    const bool do_linked_id = (duplicate_flags & USER_DUP_LINKED_ID) != 0;
    if (!(do_linked_id || !ID_IS_LINKED(id))) {
      return id;
    }

    ID *id_new = BKE_id_copy_ex(bmain, id, NULL, copy_flags);
    /* Copying add one user by default, need to get rid of that one. */
    id_us_min(id_new);
    ID_NEW_SET(id, id_new);

    /* Shape keys are always copied with their owner ID, by default. */
    ID *key_new = (ID *)BKE_key_from_id(id_new);
    ID *key = (ID *)BKE_key_from_id(id);
    if (key != NULL) {
      ID_NEW_SET(key, key_new);
    }

    /* NOTE: embedded data (root nodetrees and master collections) should never be referenced by
     * anything else, so we do not need to set their newid pointer and flag. */

    BKE_animdata_duplicate_id_action(bmain, id_new, duplicate_flags);
    if (key_new != NULL) {
      BKE_animdata_duplicate_id_action(bmain, key_new, duplicate_flags);
    }
    /* Note that actions of embedded data (root nodetrees and master collections) are handled
     * by `BKE_animdata_duplicate_id_action` as well. */
  }
  return id->newid;
}

static int foreach_assign_id_to_orig_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;

  if (*id_p) {
    ID *id = *id_p;
    *id_p = DEG_get_original_id(id);

    /* If the ID changes increase the user count.
     *
     * This means that the reference to evaluated ID has been changed with a reference to the
     * original ID which implies that the user count of the original ID is increased.
     *
     * The evaluated IDs do not maintain their user counter, so do not change it to avoid issues
     * with the user counter going negative. */
    if (*id_p != id) {
      if ((cb_data->cb_flag & IDWALK_CB_USER) != 0) {
        id_us_plus(*id_p);
      }
    }
  }

  return IDWALK_RET_NOP;
}

ID *BKE_id_copy_for_use_in_bmain(Main *bmain, const ID *id)
{
  ID *newid = BKE_id_copy(bmain, id);

  if (newid == NULL) {
    return newid;
  }

  /* Assign ID references directly used by the given ID to their original complementary parts.
   *
   * For example, when is called on an evaluated object will assign object->data to its original
   * pointer, the evaluated object->data will be kept unchanged. */
  BKE_library_foreach_ID_link(NULL, newid, foreach_assign_id_to_orig_callback, NULL, IDWALK_NOP);

  /* Shape keys reference on evaluated ID is preserved to keep driver paths available, but the key
   * data is likely to be invalid now due to modifiers, so clear the shape key reference avoiding
   * any possible shape corruption. */
  if (DEG_is_evaluated_id(id)) {
    Key **key_p = BKE_key_from_id_p(newid);
    if (key_p) {
      *key_p = NULL;
    }
  }

  return newid;
}

static void id_embedded_swap(ID **embedded_id_a,
                             ID **embedded_id_b,
                             const bool do_full_id,
                             struct IDRemapper *remapper_id_a,
                             struct IDRemapper *remapper_id_b);

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 */
static void id_swap(Main *bmain,
                    ID *id_a,
                    ID *id_b,
                    const bool do_full_id,
                    const bool do_self_remap,
                    struct IDRemapper *input_remapper_id_a,
                    struct IDRemapper *input_remapper_id_b,
                    const int self_remap_flags)
{
  BLI_assert(GS(id_a->name) == GS(id_b->name));

  struct IDRemapper *remapper_id_a = input_remapper_id_a;
  struct IDRemapper *remapper_id_b = input_remapper_id_b;
  if (do_self_remap) {
    if (remapper_id_a == NULL) {
      remapper_id_a = BKE_id_remapper_create();
    }
    if (remapper_id_b == NULL) {
      remapper_id_b = BKE_id_remapper_create();
    }
  }

  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id_a);
  BLI_assert(id_type != NULL);
  const size_t id_struct_size = id_type->struct_size;

  const ID id_a_back = *id_a;
  const ID id_b_back = *id_b;

  char *id_swap_buff = alloca(id_struct_size);

  memcpy(id_swap_buff, id_a, id_struct_size);
  memcpy(id_a, id_b, id_struct_size);
  memcpy(id_b, id_swap_buff, id_struct_size);

  if (!do_full_id) {
    /* Restore original ID's internal data. */
    *id_a = id_a_back;
    *id_b = id_b_back;

    /* Exception: IDProperties. */
    id_a->properties = id_b_back.properties;
    id_b->properties = id_a_back.properties;
    /* Exception: recalc flags. */
    id_a->recalc = id_b_back.recalc;
    id_b->recalc = id_a_back.recalc;
  }

  id_embedded_swap((ID **)BKE_ntree_ptr_from_id(id_a),
                   (ID **)BKE_ntree_ptr_from_id(id_b),
                   do_full_id,
                   remapper_id_a,
                   remapper_id_b);
  if (GS(id_a->name) == ID_SCE) {
    Scene *scene_a = (Scene *)id_a;
    Scene *scene_b = (Scene *)id_b;
    id_embedded_swap((ID **)&scene_a->master_collection,
                     (ID **)&scene_b->master_collection,
                     do_full_id,
                     remapper_id_a,
                     remapper_id_b);
  }

  if (remapper_id_a != NULL) {
    BKE_id_remapper_add(remapper_id_a, id_b, id_a);
  }
  if (remapper_id_b != NULL) {
    BKE_id_remapper_add(remapper_id_b, id_a, id_b);
  }

  /* Finalize remapping of internal references to self broken by swapping, if requested. */
  if (do_self_remap) {
    LinkNode ids = {.next = NULL, .link = id_a};
    BKE_libblock_relink_multiple(
        bmain, &ids, ID_REMAP_TYPE_REMAP, remapper_id_a, self_remap_flags);
    ids.link = id_b;
    BKE_libblock_relink_multiple(
        bmain, &ids, ID_REMAP_TYPE_REMAP, remapper_id_b, self_remap_flags);
  }

  if (input_remapper_id_a == NULL && remapper_id_a != NULL) {
    BKE_id_remapper_free(remapper_id_a);
  }
  if (input_remapper_id_b == NULL && remapper_id_b != NULL) {
    BKE_id_remapper_free(remapper_id_b);
  }
}

/* Conceptually, embedded IDs are part of their owner's data. However, some parts of the code
 * (like e.g. the depsgraph) may treat them as independent IDs, so swapping them here and
 * switching their pointers in the owner IDs allows to help not break cached relationships and
 * such (by preserving the pointer values). */
static void id_embedded_swap(ID **embedded_id_a,
                             ID **embedded_id_b,
                             const bool do_full_id,
                             struct IDRemapper *remapper_id_a,
                             struct IDRemapper *remapper_id_b)
{
  if (embedded_id_a != NULL && *embedded_id_a != NULL) {
    BLI_assert(embedded_id_b != NULL);

    if (*embedded_id_b == NULL) {
      /* Cannot swap anything if one of the embedded IDs is NULL. */
      return;
    }

    /* Do not remap internal references to itself here, since embedded IDs pointers also need to be
     * potentially remapped in owner ID's data, which will also handle embedded IDs data. */
    id_swap(
        NULL, *embedded_id_a, *embedded_id_b, do_full_id, false, remapper_id_a, remapper_id_b, 0);
    /* Manual 'remap' of owning embedded pointer in owner ID. */
    SWAP(ID *, *embedded_id_a, *embedded_id_b);

    /* Restore internal pointers to the swapped embedded IDs in their owners' data. This also
     * includes the potential self-references inside the embedded IDs themselves. */
    if (remapper_id_a != NULL) {
      BKE_id_remapper_add(remapper_id_a, *embedded_id_b, *embedded_id_a);
    }
    if (remapper_id_b != NULL) {
      BKE_id_remapper_add(remapper_id_b, *embedded_id_a, *embedded_id_b);
    }
  }
}

void BKE_lib_id_swap(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags)
{
  id_swap(bmain, id_a, id_b, false, do_self_remap, NULL, NULL, self_remap_flags);
}

void BKE_lib_id_swap_full(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags)
{
  id_swap(bmain, id_a, id_b, true, do_self_remap, NULL, NULL, self_remap_flags);
}

bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop)
{
  ID *newid = NULL;
  PointerRNA idptr;

  if (id && (ID_REAL_USERS(id) > 1)) {
    /* If property isn't editable,
     * we're going to have an extra block hanging around until we save. */
    if (RNA_property_editable(ptr, prop)) {
      Main *bmain = CTX_data_main(C);
      /* copy animation actions too */
      newid = BKE_id_copy_ex(bmain, id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
      if (newid != NULL) {
        /* us is 1 by convention with new IDs, but RNA_property_pointer_set
         * will also increment it, decrement it here. */
        id_us_min(newid);

        /* assign copy */
        RNA_id_pointer_create(newid, &idptr);
        RNA_property_pointer_set(ptr, prop, idptr, NULL);
        RNA_property_update(C, ptr, prop);

        /* tag grease pencil data-block and disable onion */
        if (GS(id->name) == ID_GD_LEGACY) {
          DEG_id_tag_update(id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
          DEG_id_tag_update(newid, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
          bGPdata *gpd = (bGPdata *)newid;
          gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
        }

        return true;
      }
    }
  }

  return false;
}

static int libblock_management_us_plus(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;
  if (cb_flag & IDWALK_CB_USER) {
    id_us_plus(*id_pointer);
  }
  if (cb_flag & IDWALK_CB_USER_ONE) {
    id_us_ensure_real(*id_pointer);
  }

  return IDWALK_RET_NOP;
}

static int libblock_management_us_min(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;
  if (cb_flag & IDWALK_CB_USER) {
    id_us_min(*id_pointer);
  }
  /* We can do nothing in IDWALK_CB_USER_ONE case! */

  return IDWALK_RET_NOP;
}

void BKE_libblock_management_main_add(Main *bmain, void *idv)
{
  ID *id = idv;

  BLI_assert(bmain != NULL);
  if ((id->tag & LIB_TAG_NO_MAIN) == 0) {
    return;
  }

  if ((id->tag & LIB_TAG_NOT_ALLOCATED) != 0) {
    /* We cannot add non-allocated ID to Main! */
    return;
  }

  /* We cannot allow non-userrefcounting IDs in Main database! */
  if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0) {
    BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, NULL, IDWALK_NOP);
  }

  ListBase *lb = which_libbase(bmain, GS(id->name));
  BKE_main_lock(bmain);
  BLI_addtail(lb, id);
  /* We need to allow adding extra datablocks into libraries too, e.g. to support generating new
   * overrides for recursive resync. */
  BKE_id_new_name_validate(bmain, lb, id, NULL, true);
  /* alphabetic insertion: is in new_id */
  id->tag &= ~(LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT);
  bmain->is_memfile_undo_written = false;
  BKE_main_unlock(bmain);

  BKE_lib_libblock_session_uuid_ensure(id);
}

void BKE_libblock_management_main_remove(Main *bmain, void *idv)
{
  ID *id = idv;

  BLI_assert(bmain != NULL);
  if ((id->tag & LIB_TAG_NO_MAIN) != 0) {
    return;
  }

  /* For now, allow userrefcounting IDs to get out of Main - can be handy in some cases... */

  ListBase *lb = which_libbase(bmain, GS(id->name));
  BKE_main_lock(bmain);
  BLI_remlink(lb, id);
  BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  id->tag |= LIB_TAG_NO_MAIN;
  bmain->is_memfile_undo_written = false;
  BKE_main_unlock(bmain);
}

void BKE_libblock_management_usercounts_set(Main *bmain, void *idv)
{
  ID *id = idv;

  if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) == 0) {
    return;
  }

  BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, NULL, IDWALK_NOP);
  id->tag &= ~LIB_TAG_NO_USER_REFCOUNT;
}

void BKE_libblock_management_usercounts_clear(Main *bmain, void *idv)
{
  ID *id = idv;

  /* We do not allow IDs in Main database to not be userrefcounting. */
  if ((id->tag & LIB_TAG_NO_USER_REFCOUNT) != 0 || (id->tag & LIB_TAG_NO_MAIN) != 0) {
    return;
  }

  BKE_library_foreach_ID_link(bmain, id, libblock_management_us_min, NULL, IDWALK_NOP);
  id->tag |= LIB_TAG_NO_USER_REFCOUNT;
}

void BKE_main_id_tag_listbase(ListBase *lb, const int tag, const bool value)
{
  ID *id;
  if (value) {
    for (id = lb->first; id; id = id->next) {
      id->tag |= tag;
    }
  }
  else {
    const int ntag = ~tag;
    for (id = lb->first; id; id = id->next) {
      id->tag &= ntag;
    }
  }
}

void BKE_main_id_tag_idcode(struct Main *mainvar,
                            const short type,
                            const int tag,
                            const bool value)
{
  ListBase *lb = which_libbase(mainvar, type);

  BKE_main_id_tag_listbase(lb, tag, value);
}

void BKE_main_id_tag_all(struct Main *mainvar, const int tag, const bool value)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int a;

  a = set_listbasepointers(mainvar, lbarray);
  while (a--) {
    BKE_main_id_tag_listbase(lbarray[a], tag, value);
  }
}

void BKE_main_id_flag_listbase(ListBase *lb, const int flag, const bool value)
{
  ID *id;
  if (value) {
    for (id = lb->first; id; id = id->next) {
      id->tag |= flag;
    }
  }
  else {
    const int nflag = ~flag;
    for (id = lb->first; id; id = id->next) {
      id->tag &= nflag;
    }
  }
}

void BKE_main_id_flag_all(Main *bmain, const int flag, const bool value)
{
  ListBase *lbarray[INDEX_ID_MAX];
  int a;
  a = set_listbasepointers(bmain, lbarray);
  while (a--) {
    BKE_main_id_flag_listbase(lbarray[a], flag, value);
  }
}

void BKE_main_id_repair_duplicate_names_listbase(Main *bmain, ListBase *lb)
{
  int lb_len = 0;
  LISTBASE_FOREACH (ID *, id, lb) {
    if (!ID_IS_LINKED(id)) {
      lb_len += 1;
    }
  }
  if (lb_len <= 1) {
    return;
  }

  /* Fill an array because renaming sorts. */
  ID **id_array = MEM_mallocN(sizeof(*id_array) * lb_len, __func__);
  GSet *gset = BLI_gset_str_new_ex(__func__, lb_len);
  int i = 0;
  LISTBASE_FOREACH (ID *, id, lb) {
    if (!ID_IS_LINKED(id)) {
      id_array[i] = id;
      i++;
    }
  }
  for (i = 0; i < lb_len; i++) {
    if (!BLI_gset_add(gset, id_array[i]->name + 2)) {
      BKE_id_new_name_validate(bmain, lb, id_array[i], NULL, false);
    }
  }
  BLI_gset_free(gset, NULL);
  MEM_freeN(id_array);
}

void BKE_main_lib_objects_recalc_all(Main *bmain)
{
  Object *ob;

  /* flag for full recalc */
  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ID_IS_LINKED(ob)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
    }
  }

  DEG_id_type_tag(bmain, ID_OB);
}

/* *********** ALLOC AND FREE *****************
 *
 * BKE_libblock_free(ListBase *lb, ID *id )
 * provide a list-basis and data-block, but only ID is read
 *
 * void *BKE_libblock_alloc(ListBase *lb, type, name)
 * inserts in list and returns a new ID
 *
 * **************************** */

size_t BKE_libblock_get_alloc_info(short type, const char **name)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(type);

  if (id_type == NULL) {
    if (name != NULL) {
      *name = NULL;
    }
    return 0;
  }

  if (name != NULL) {
    *name = id_type->name;
  }
  return id_type->struct_size;
}

void *BKE_libblock_alloc_notest(short type)
{
  const char *name;
  size_t size = BKE_libblock_get_alloc_info(type, &name);
  if (size != 0) {
    return MEM_callocN(size, name);
  }
  BLI_assert_msg(0, "Request to allocate unknown data type");
  return NULL;
}

void *BKE_libblock_alloc(Main *bmain, short type, const char *name, const int flag)
{
  BLI_assert((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || bmain != NULL);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_LOCAL) == 0);

  ID *id = BKE_libblock_alloc_notest(type);

  if (id) {
    if ((flag & LIB_ID_CREATE_NO_MAIN) != 0) {
      id->tag |= LIB_TAG_NO_MAIN;
    }
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0) {
      id->tag |= LIB_TAG_NO_USER_REFCOUNT;
    }
    if (flag & LIB_ID_CREATE_LOCAL) {
      id->tag |= LIB_TAG_LOCALIZED;
    }

    id->icon_id = 0;
    *((short *)id->name) = type;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id->us = 1;
    }
    if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
      /* Note that 2.8x versioning has tested not to cause conflicts. Node trees are
       * skipped in this check to allow adding a geometry node tree for versioning. */
      BLI_assert(bmain->is_locked_for_linking == false || ELEM(type, ID_WS, ID_GR, ID_NT));
      ListBase *lb = which_libbase(bmain, type);

      BKE_main_lock(bmain);
      BLI_addtail(lb, id);
      BKE_id_new_name_validate(bmain, lb, id, name, false);
      bmain->is_memfile_undo_written = false;
      /* alphabetic insertion: is in new_id */
      BKE_main_unlock(bmain);

      /* This assert avoids having to keep name_map consistency when changing the library of an ID,
       * if this check is not true anymore it will have to be done here too. */
      BLI_assert(bmain->curlib == NULL || bmain->curlib->runtime.name_map == NULL);
      /* This is important in 'readfile doversion after liblink' context mainly, but is a good
       * consistency change in general: ID created for a Main should get that main's current
       * library pointer. */
      id->lib = bmain->curlib;

      /* TODO: to be removed from here! */
      if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0) {
        DEG_id_type_tag(bmain, type);
      }
    }
    else {
      BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
    }

    /* We also need to ensure a valid `session_uuid` for some non-main data (like embedded IDs).
     * IDs not allocated however should not need those (this would e.g. avoid generating session
     * uuids for depsgraph CoW IDs, if it was using this function). */
    if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
      BKE_lib_libblock_session_uuid_ensure(id);
    }
  }

  return id;
}

void BKE_libblock_init_empty(ID *id)
{
  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if (idtype_info->init_data != NULL) {
      idtype_info->init_data(id);
    }
    return;
  }

  BLI_assert_msg(0, "IDType Missing IDTypeInfo");
}

void BKE_libblock_runtime_reset_remapping_status(ID *id)
{
  id->runtime.remap.status = 0;
  id->runtime.remap.skipped_refcounted = 0;
  id->runtime.remap.skipped_direct = 0;
  id->runtime.remap.skipped_indirect = 0;
}

/* ********** ID session-wise UUID management. ********** */
static uint global_session_uuid = 0;

void BKE_lib_libblock_session_uuid_ensure(ID *id)
{
  if (id->session_uuid == MAIN_ID_SESSION_UUID_UNSET) {
    BLI_assert((id->tag & LIB_TAG_TEMP_MAIN) == 0); /* Caller must ensure this. */
    id->session_uuid = atomic_add_and_fetch_uint32(&global_session_uuid, 1);
    /* In case overflow happens, still assign a valid ID. This way opening files many times works
     * correctly. */
    if (UNLIKELY(id->session_uuid == MAIN_ID_SESSION_UUID_UNSET)) {
      id->session_uuid = atomic_add_and_fetch_uint32(&global_session_uuid, 1);
    }
  }
}

void BKE_lib_libblock_session_uuid_renew(ID *id)
{
  id->session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  BKE_lib_libblock_session_uuid_ensure(id);
}

void *BKE_id_new(Main *bmain, const short type, const char *name)
{
  BLI_assert(bmain != NULL);

  if (name == NULL) {
    name = DATA_(BKE_idtype_idcode_to_name(type));
  }

  ID *id = BKE_libblock_alloc(bmain, type, name, 0);
  BKE_libblock_init_empty(id);

  return id;
}

void *BKE_id_new_nomain(const short type, const char *name)
{
  if (name == NULL) {
    name = DATA_(BKE_idtype_idcode_to_name(type));
  }

  ID *id = BKE_libblock_alloc(NULL,
                              type,
                              name,
                              LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT |
                                  LIB_ID_CREATE_NO_DEG_TAG);
  BKE_libblock_init_empty(id);

  return id;
}

void BKE_libblock_copy_ex(Main *bmain, const ID *id, ID **r_newid, const int orig_flag)
{
  ID *new_id = *r_newid;
  int flag = orig_flag;

  const bool is_private_id_data = (id->flag & LIB_EMBEDDED_DATA) != 0;

  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || bmain != NULL);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_LOCAL) == 0);

  /* 'Private ID' data handling. */
  if ((bmain != NULL) && is_private_id_data) {
    flag |= LIB_ID_CREATE_NO_MAIN;
  }

  /* The id->flag bits to copy over. */
  const int copy_idflag_mask = LIB_EMBEDDED_DATA;

  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) != 0) {
    /* r_newid already contains pointer to allocated memory. */
    /* TODO: do we want to memset(0) whole mem before filling it? */
    STRNCPY(new_id->name, id->name);
    new_id->us = 0;
    new_id->tag |= LIB_TAG_NOT_ALLOCATED | LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT;
    /* TODO: Do we want/need to copy more from ID struct itself? */
  }
  else {
    new_id = BKE_libblock_alloc(bmain, GS(id->name), id->name + 2, flag);
  }
  BLI_assert(new_id != NULL);

  if ((flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) != 0) {
    new_id->tag |= LIB_TAG_COPIED_ON_WRITE;
  }
  else {
    new_id->tag &= ~LIB_TAG_COPIED_ON_WRITE;
  }

  const size_t id_len = BKE_libblock_get_alloc_info(GS(new_id->name), NULL);
  const size_t id_offset = sizeof(ID);
  if ((int)id_len - (int)id_offset > 0) { /* signed to allow neg result */ /* XXX ????? */
    const char *cp = (const char *)id;
    char *cpn = (char *)new_id;

    memcpy(cpn + id_offset, cp + id_offset, id_len - id_offset);
  }

  new_id->flag = (new_id->flag & ~copy_idflag_mask) | (id->flag & copy_idflag_mask);

  /* We do not want any handling of user-count in code duplicating the data here, we do that all
   * at once in id_copy_libmanagement_cb() at the end. */
  const int copy_data_flag = orig_flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  if (id->properties) {
    new_id->properties = IDP_CopyProperty_ex(id->properties, copy_data_flag);
  }

  /* This is never duplicated, only one existing ID should have a given weak ref to library/ID. */
  new_id->library_weak_reference = NULL;

  if ((orig_flag & LIB_ID_COPY_NO_LIB_OVERRIDE) == 0) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      /* We do not want to copy existing override rules here, as they would break the proper
       * remapping between IDs. Proper overrides rules will be re-generated anyway. */
      BKE_lib_override_library_copy(new_id, id, false);
    }
    else if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
      /* Just ensure virtual overrides do get properly tagged, there is not actual override data to
       * copy here. */
      new_id->flag |= LIB_EMBEDDED_DATA_LIB_OVERRIDE;
    }
  }

  if (id_can_have_animdata(new_id)) {
    IdAdtTemplate *iat = (IdAdtTemplate *)new_id;

    /* the duplicate should get a copy of the animdata */
    if ((flag & LIB_ID_COPY_NO_ANIMDATA) == 0) {
      /* Note that even though horrors like root nodetrees are not in bmain, the actions they use
       * in their anim data *are* in bmain... super-mega-hooray. */
      BLI_assert((copy_data_flag & LIB_ID_COPY_ACTIONS) == 0 ||
                 (copy_data_flag & LIB_ID_CREATE_NO_MAIN) == 0);
      iat->adt = BKE_animdata_copy(bmain, iat->adt, copy_data_flag);
    }
    else {
      iat->adt = NULL;
    }
  }

  if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    DEG_id_type_tag(bmain, GS(new_id->name));
  }

  *r_newid = new_id;
}

void *BKE_libblock_copy(Main *bmain, const ID *id)
{
  ID *idn;

  BKE_libblock_copy_ex(bmain, id, &idn, 0);

  return idn;
}

/* ***************** ID ************************ */

ID *BKE_libblock_find_name(struct Main *bmain, const short type, const char *name)
{
  ListBase *lb = which_libbase(bmain, type);
  BLI_assert(lb != NULL);
  return BLI_findstring(lb, name, offsetof(ID, name) + 2);
}

struct ID *BKE_libblock_find_session_uuid(Main *bmain,
                                          const short type,
                                          const uint32_t session_uuid)
{
  ListBase *lb = which_libbase(bmain, type);
  BLI_assert(lb != NULL);
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id->session_uuid == session_uuid) {
      return id;
    }
  }
  return NULL;
}

void id_sort_by_name(ListBase *lb, ID *id, ID *id_sorting_hint)
{
#define ID_SORT_STEP_SIZE 512

  ID *idtest;

  /* insert alphabetically */
  if (lb->first == lb->last) {
    return;
  }

  BLI_remlink(lb, id);

  /* Check if we can actually insert id before or after id_sorting_hint, if given. */
  if (!ELEM(id_sorting_hint, NULL, id) && id_sorting_hint->lib == id->lib) {
    BLI_assert(BLI_findindex(lb, id_sorting_hint) >= 0);

    ID *id_sorting_hint_next = id_sorting_hint->next;
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) < 0 &&
        (id_sorting_hint_next == NULL || id_sorting_hint_next->lib != id->lib ||
         BLI_strcasecmp(id_sorting_hint_next->name, id->name) > 0))
    {
      BLI_insertlinkafter(lb, id_sorting_hint, id);
      return;
    }

    ID *id_sorting_hint_prev = id_sorting_hint->prev;
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) > 0 &&
        (id_sorting_hint_prev == NULL || id_sorting_hint_prev->lib != id->lib ||
         BLI_strcasecmp(id_sorting_hint_prev->name, id->name) < 0))
    {
      BLI_insertlinkbefore(lb, id_sorting_hint, id);
      return;
    }
  }

  void *item_array[ID_SORT_STEP_SIZE];
  int item_array_index;

  /* Step one: We go backward over a whole chunk of items at once, until we find a limit item
   * that is lower than, or equal (should never happen!) to the one we want to insert. */
  /* NOTE: We start from the end, because in typical 'heavy' case (insertion of lots of IDs at
   * once using the same base name), newly inserted items will generally be towards the end
   * (higher extension numbers). */
  bool is_in_library = false;
  item_array_index = ID_SORT_STEP_SIZE - 1;
  for (idtest = lb->last; idtest != NULL; idtest = idtest->prev) {
    if (is_in_library) {
      if (idtest->lib != id->lib) {
        /* We got out of expected library 'range' in the list, so we are done here and can move on
         * to the next step. */
        break;
      }
    }
    else if (idtest->lib == id->lib) {
      /* We are entering the expected library 'range' of IDs in the list. */
      is_in_library = true;
    }

    if (!is_in_library) {
      continue;
    }

    item_array[item_array_index] = idtest;
    if (item_array_index == 0) {
      if (BLI_strcasecmp(idtest->name, id->name) <= 0) {
        break;
      }
      item_array_index = ID_SORT_STEP_SIZE;
    }
    item_array_index--;
  }

  /* Step two: we go forward in the selected chunk of items and check all of them, as we know
   * that our target is in there. */

  /* If we reached start of the list, current item_array_index is off-by-one.
   * Otherwise, we already know that it points to an item lower-or-equal-than the one we want to
   * insert, no need to redo the check for that one.
   * So we can increment that index in any case. */
  for (item_array_index++; item_array_index < ID_SORT_STEP_SIZE; item_array_index++) {
    idtest = item_array[item_array_index];
    if (BLI_strcasecmp(idtest->name, id->name) > 0) {
      BLI_insertlinkbefore(lb, idtest, id);
      break;
    }
  }
  if (item_array_index == ID_SORT_STEP_SIZE) {
    if (idtest == NULL) {
      /* If idtest is NULL here, it means that in the first loop, the last comparison was
       * performed exactly on the first item of the list, and that it also failed. And that the
       * second loop was not walked at all.
       *
       * In other words, if `id` is local, all the items in the list are greater than the inserted
       * one, so we can put it at the start of the list. Or, if `id` is linked, it is the first one
       * of its library, and we can put it at the very end of the list. */
      if (ID_IS_LINKED(id)) {
        BLI_addtail(lb, id);
      }
      else {
        BLI_addhead(lb, id);
      }
    }
    else {
      BLI_insertlinkafter(lb, idtest, id);
    }
  }

#undef ID_SORT_STEP_SIZE
}

bool BKE_id_new_name_validate(
    struct Main *bmain, ListBase *lb, ID *id, const char *tname, const bool do_linked_data)
{
  bool result = false;
  char name[MAX_ID_NAME - 2];

  /* If library, don't rename (unless explicitly required), but do ensure proper sorting. */
  if (!do_linked_data && ID_IS_LINKED(id)) {
    id_sort_by_name(lb, id, NULL);

    return result;
  }

  /* If no name given, use name of current ID. */
  if (tname == NULL) {
    tname = id->name + 2;
  }
  /* Make a copy of given name (tname args can be const). */
  STRNCPY(name, tname);

  if (name[0] == '\0') {
    /* Disallow empty names. */
    STRNCPY_UTF8(name, DATA_(BKE_idtype_idcode_to_name(GS(id->name))));
  }
  else {
    /* disallow non utf8 chars,
     * the interface checks for this but new ID's based on file names don't */
    BLI_str_utf8_invalid_strip(name, strlen(name));
  }

  result = BKE_main_namemap_get_name(bmain, id, name);

  strcpy(id->name + 2, name);
  id_sort_by_name(lb, id, NULL);
  return result;
}

void BKE_main_id_newptr_and_tag_clear(Main *bmain)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BKE_id_newptr_and_tag_clear(id);
  }
  FOREACH_MAIN_ID_END;
}

static int id_refcount_recompute_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  const int cb_flag = cb_data->cb_flag;
  const bool do_linked_only = (bool)POINTER_AS_INT(cb_data->user_data);

  if (*id_pointer == NULL) {
    return IDWALK_RET_NOP;
  }
  if (do_linked_only && !ID_IS_LINKED(*id_pointer)) {
    return IDWALK_RET_NOP;
  }

  if (cb_flag & IDWALK_CB_USER) {
    /* Do not touch to direct/indirect linked status here... */
    id_us_plus_no_lib(*id_pointer);
  }
  if (cb_flag & IDWALK_CB_USER_ONE) {
    id_us_ensure_real(*id_pointer);
  }

  return IDWALK_RET_NOP;
}

void BKE_main_id_refcount_recompute(struct Main *bmain, const bool do_linked_only)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_LINKED(id) && do_linked_only) {
      continue;
    }
    id->us = ID_FAKE_USERS(id);
    /* Note that we keep EXTRAUSER tag here, since some UI users may define it too... */
    if (id->tag & LIB_TAG_EXTRAUSER) {
      id->tag &= ~(LIB_TAG_EXTRAUSER | LIB_TAG_EXTRAUSER_SET);
      id_us_ensure_real(id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Go over whole Main database to re-generate proper user-counts. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BKE_library_foreach_ID_link(bmain,
                                id,
                                id_refcount_recompute_callback,
                                POINTER_FROM_INT((int)do_linked_only),
                                IDWALK_READONLY | IDWALK_INCLUDE_UI);
  }
  FOREACH_MAIN_ID_END;
}

static void library_make_local_copying_check(ID *id,
                                             GSet *loop_tags,
                                             MainIDRelations *id_relations,
                                             GSet *done_ids)
{
  if (BLI_gset_haskey(done_ids, id)) {
    return; /* Already checked, nothing else to do. */
  }

  MainIDRelationsEntry *entry = BLI_ghash_lookup(id_relations->relations_from_pointers, id);
  BLI_gset_insert(loop_tags, id);
  for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != NULL;
       from_id_entry = from_id_entry->next)
  {
    /* Our oh-so-beloved 'from' pointers... Those should always be ignored here, since the actual
     * relation we want to check is in the other way around. */
    if (from_id_entry->usage_flag & IDWALK_CB_LOOPBACK) {
      continue;
    }

    ID *from_id = from_id_entry->id_pointer.from;

    /* Shape-keys are considered 'private' to their owner ID here, and never tagged
     * (since they cannot be linked), so we have to switch effective parent to their owner.
     */
    if (GS(from_id->name) == ID_KE) {
      from_id = ((Key *)from_id)->from;
    }

    if (!ID_IS_LINKED(from_id)) {
      /* Local user, early out to avoid some gset querying... */
      continue;
    }
    if (!BLI_gset_haskey(done_ids, from_id)) {
      if (BLI_gset_haskey(loop_tags, from_id)) {
        /* We are in a 'dependency loop' of IDs, this does not say us anything, skip it.
         * Note that this is the situation that can lead to archipelagos of linked data-blocks
         * (since all of them have non-local users, they would all be duplicated,
         * leading to a loop of unused linked data-blocks that cannot be freed since they all use
         * each other...). */
        continue;
      }
      /* Else, recursively check that user ID. */
      library_make_local_copying_check(from_id, loop_tags, id_relations, done_ids);
    }

    if (from_id->tag & LIB_TAG_DOIT) {
      /* This user will be fully local in future, so far so good,
       * nothing to do here but check next user. */
    }
    else {
      /* This user won't be fully local in future, so current ID won't be either.
       * And we are done checking it. */
      id->tag &= ~LIB_TAG_DOIT;
      break;
    }
  }
  BLI_gset_add(done_ids, id);
  BLI_gset_remove(loop_tags, id, NULL);
}

/* NOTE: Old (2.77) version was simply making (tagging) data-blocks as local,
 * without actually making any check whether they were also indirectly used or not...
 *
 * Current version uses regular id_make_local callback, with advanced pre-processing step to
 * detect all cases of IDs currently indirectly used, but which will be used by local data only
 * once this function is finished.  This allows to avoid any unneeded duplication of IDs, and
 * hence all time lost afterwards to remove orphaned linked data-blocks. */
void BKE_library_make_local(Main *bmain,
                            const Library *lib,
                            GHash *old_to_new_ids,
                            const bool untagged_only,
                            const bool set_fake)
{

  ListBase *lbarray[INDEX_ID_MAX];

  LinkNode *todo_ids = NULL;
  LinkNode *copied_ids = NULL;
  MemArena *linklist_mem = BLI_memarena_new(512 * sizeof(*todo_ids), __func__);

  GSet *done_ids = BLI_gset_ptr_new(__func__);

#ifdef DEBUG_TIME
  TIMEIT_START(make_local);
#endif

  BKE_main_relations_create(bmain, 0);

#ifdef DEBUG_TIME
  printf("Pre-compute current ID relations: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* Step 1: Detect data-blocks to make local. */
  for (int a = set_listbasepointers(bmain, lbarray); a--;) {
    ID *id = lbarray[a]->first;

    /* Do not explicitly make local non-linkable IDs (shape-keys, in fact),
     * they are assumed to be handled by real data-blocks responsible of them. */
    const bool do_skip = (id && !BKE_idtype_idcode_is_linkable(GS(id->name)));

    for (; id; id = id->next) {
      ID *ntree = (ID *)ntreeFromID(id);

      id->tag &= ~LIB_TAG_DOIT;
      if (ntree != NULL) {
        ntree->tag &= ~LIB_TAG_DOIT;
      }

      if (!ID_IS_LINKED(id)) {
        id->tag &= ~(LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW);
        id->flag &= ~LIB_INDIRECT_WEAK_LINK;
        if (ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
            ELEM(lib, NULL, id->override_library->reference->lib) &&
            ((untagged_only == false) || !(id->tag & LIB_TAG_PRE_EXISTING)))
        {
          BKE_lib_override_library_make_local(id);
        }
      }
      /* The check on the fourth line (LIB_TAG_PRE_EXISTING) is done so it's possible to tag data
       * you don't want to be made local, used for appending data,
       * so any libdata already linked won't become local (very nasty
       * to discover all your links are lost after appending).
       * Also, never ever make proxified objects local, would not make any sense. */
      /* Some more notes:
       *   - Shape-keys are never tagged here (since they are not linkable).
       *   - Node-trees used in materials etc. have to be tagged manually,
       *     since they do not exist in Main (!).
       * This is ok-ish on 'make local' side of things
       * (since those are handled by their 'owner' IDs),
       * but complicates slightly the pre-processing of relations between IDs at step 2... */
      else if (!do_skip && id->tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW) &&
               ELEM(lib, NULL, id->lib) &&
               ((untagged_only == false) || !(id->tag & LIB_TAG_PRE_EXISTING)))
      {
        BLI_linklist_prepend_arena(&todo_ids, id, linklist_mem);
        id->tag |= LIB_TAG_DOIT;

        /* Tag those nasty non-ID nodetrees,
         * but do not add them to todo list, making them local is handled by 'owner' ID.
         * This is needed for library_make_local_copying_check() to work OK at step 2. */
        if (ntree != NULL) {
          ntree->tag |= LIB_TAG_DOIT;
        }
      }
      else {
        /* Linked ID that we won't be making local (needed info for step 2, see below). */
        BLI_gset_add(done_ids, id);
      }
    }
  }

#ifdef DEBUG_TIME
  printf("Step 1: Detect data-blocks to make local: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* Step 2: Check which data-blocks we can directly make local
   * (because they are only used by already, or future, local data),
   * others will need to be duplicated. */
  GSet *loop_tags = BLI_gset_ptr_new(__func__);
  for (LinkNode *it = todo_ids; it; it = it->next) {
    library_make_local_copying_check(it->link, loop_tags, bmain->relations, done_ids);
    BLI_assert(BLI_gset_len(loop_tags) == 0);
  }
  BLI_gset_free(loop_tags, NULL);
  BLI_gset_free(done_ids, NULL);

  /* Next step will most likely add new IDs, better to get rid of this mapping now. */
  BKE_main_relations_free(bmain);

#ifdef DEBUG_TIME
  printf("Step 2: Check which data-blocks we can directly make local: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* Step 3: Make IDs local, either directly (quick and simple), or using generic process,
   * which involves more complex checks and might instead
   * create a local copy of original linked ID. */
  for (LinkNode *it = todo_ids, *it_next; it; it = it_next) {
    it_next = it->next;
    ID *id = it->link;

    if (id->tag & LIB_TAG_DOIT) {
      /* We know all users of this object are local or will be made fully local, even if
       * currently there are some indirect usages. So instead of making a copy that we'll likely
       * get rid of later, directly make that data block local.
       * Saves a tremendous amount of time with complex scenes... */
      BKE_lib_id_clear_library_data(bmain, id, 0);
      BKE_lib_id_expand_local(bmain, id, 0);
      id->tag &= ~LIB_TAG_DOIT;

      if (GS(id->name) == ID_OB) {
        BKE_rigidbody_ensure_local_object(bmain, (Object *)id);
      }
    }
    else {
      /* In this specific case, we do want to make ID local even if it has no local usage yet... */
      BKE_lib_id_make_local(bmain, id, LIB_ID_MAKELOCAL_FULL_LIBRARY);

      if (id->newid) {
        if (GS(id->newid->name) == ID_OB) {
          BKE_rigidbody_ensure_local_object(bmain, (Object *)id->newid);
        }

        /* Reuse already allocated LinkNode (transferring it from todo_ids to copied_ids). */
        BLI_linklist_prepend_nlink(&copied_ids, id, it);
      }
    }

    if (set_fake) {
      if (!ELEM(GS(id->name), ID_OB, ID_GR)) {
        /* do not set fake user on objects, groups (instancing) */
        id_fake_user_set(id);
      }
    }
  }

#ifdef DEBUG_TIME
  printf("Step 3: Make IDs local: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* At this point, we are done with directly made local IDs.
   * Now we have to handle duplicated ones, since their
   * remaining linked original counterpart may not be needed anymore... */
  todo_ids = NULL;

  /* Step 4: We have to remap local usages of old (linked) ID to new (local)
   * ID in a separated loop,
   * as lbarray ordering is not enough to ensure us we did catch all dependencies
   * (e.g. if making local a parent object before its child...). See #48907. */
  /* TODO: This is now the biggest step by far (in term of processing time).
   * We may be able to gain here by using again main->relations mapping, but...
   * this implies BKE_libblock_remap & co to be able to update main->relations on the fly.
   * Have to think about it a bit more, and see whether new code is OK first, anyway. */
  for (LinkNode *it = copied_ids; it; it = it->next) {
    ID *id = it->link;

    BLI_assert(id->newid != NULL);
    BLI_assert(ID_IS_LINKED(id));

    BKE_libblock_remap(bmain, id, id->newid, ID_REMAP_SKIP_INDIRECT_USAGE);
    if (old_to_new_ids) {
      BLI_ghash_insert(old_to_new_ids, id, id->newid);
    }

    /* Special hack for groups... Thing is, since we can't instantiate them here, we need to
     * ensure they remain 'alive' (only instantiation is a real group 'user'... *sigh* See
     * #49722. */
    if (GS(id->name) == ID_GR && (id->tag & LIB_TAG_INDIRECT) != 0) {
      id_us_ensure_real(id->newid);
    }
  }

#ifdef DEBUG_TIME
  printf("Step 4: Remap local usages of old (linked) ID to new (local) ID: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* This is probably more of a hack than something we should do here, but...
   * Issue is, the whole copying + remapping done in complex cases above may leave pose-channels
   * of armatures in complete invalid state (more precisely, the bone pointers of the
   * pose-channels - very crappy cross-data-blocks relationship), se we tag it to be fully
   * recomputed, but this does not seems to be enough in some cases, and evaluation code ends up
   * trying to evaluate a not-yet-updated armature object's deformations.
   * Try "make all local" in 04_01_H.lighting.blend from Agent327 without this, e.g. */
  for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->data != NULL && ob->type == OB_ARMATURE && ob->pose != NULL &&
        ob->pose->flag & POSE_RECALC)
    {
      BKE_pose_rebuild(bmain, ob, ob->data, true);
    }
  }

#ifdef DEBUG_TIME
  printf("Hack: Forcefully rebuild armature object poses: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  BKE_main_id_newptr_and_tag_clear(bmain);
  BLI_memarena_free(linklist_mem);

#ifdef DEBUG_TIME
  printf("Cleanup and finish: Done.\n");
  TIMEIT_END(make_local);
#endif
}

void BLI_libblock_ensure_unique_name(Main *bmain, const char *name)
{
  ListBase *lb;
  ID *idtest;

  lb = which_libbase(bmain, GS(name));
  if (lb == NULL) {
    return;
  }

  /* search for id */
  idtest = BLI_findstring(lb, name + 2, offsetof(ID, name) + 2);
  if (idtest != NULL && !ID_IS_LINKED(idtest)) {
    /* BKE_id_new_name_validate also takes care of sorting. */
    BKE_id_new_name_validate(bmain, lb, idtest, NULL, false);
    bmain->is_memfile_undo_written = false;
  }
}

void BKE_libblock_rename(Main *bmain, ID *id, const char *name)
{
  BLI_assert(!ID_IS_LINKED(id));
  BKE_main_namemap_remove_name(bmain, id, id->name + 2);
  ListBase *lb = which_libbase(bmain, GS(id->name));
  if (BKE_id_new_name_validate(bmain, lb, id, name, false)) {
    bmain->is_memfile_undo_written = false;
  }
}

void BKE_id_full_name_get(char name[MAX_ID_FULL_NAME], const ID *id, char separator_char)
{
  strcpy(name, id->name + 2);

  if (ID_IS_LINKED(id)) {
    const size_t idname_len = strlen(id->name + 2);
    const size_t libname_len = strlen(id->lib->id.name + 2);

    name[idname_len] = separator_char ? separator_char : ' ';
    name[idname_len + 1] = '[';
    strcpy(name + idname_len + 2, id->lib->id.name + 2);
    name[idname_len + 2 + libname_len] = ']';
    name[idname_len + 2 + libname_len + 1] = '\0';
  }
}

void BKE_id_full_name_ui_prefix_get(char name[MAX_ID_FULL_NAME_UI],
                                    const ID *id,
                                    const bool add_lib_hint,
                                    char separator_char,
                                    int *r_prefix_len)
{
  int i = 0;

  if (add_lib_hint) {
    name[i++] = id->lib ? (ID_MISSING(id) ? 'M' : 'L') : ID_IS_OVERRIDE_LIBRARY(id) ? 'O' : ' ';
  }
  name[i++] = (id->flag & LIB_FAKEUSER) ? 'F' : ((id->us == 0) ? '0' : ' ');
  name[i++] = ' ';

  BKE_id_full_name_get(name + i, id, separator_char);

  if (r_prefix_len) {
    *r_prefix_len = i;
  }
}

char *BKE_id_to_unique_string_key(const struct ID *id)
{
  if (!ID_IS_LINKED(id)) {
    return BLI_strdup(id->name);
  }

  /* Prefix with an ascii character in the range of 32..96 (visible)
   * this ensures we can't have a library ID pair that collide.
   * Where 'LIfooOBbarOBbaz' could be ('LIfoo, OBbarOBbaz') or ('LIfooOBbar', 'OBbaz'). */
  const char ascii_len = strlen(id->lib->id.name + 2) + 32;
  return BLI_sprintfN("%c%s%s", ascii_len, id->lib->id.name, id->name);
}

void BKE_id_tag_set_atomic(ID *id, int tag)
{
  atomic_fetch_and_or_int32(&id->tag, tag);
}

void BKE_id_tag_clear_atomic(ID *id, int tag)
{
  atomic_fetch_and_and_int32(&id->tag, ~tag);
}

bool BKE_id_is_in_global_main(ID *id)
{
  /* We do not want to fail when id is NULL here, even though this is a bit strange behavior...
   */
  return (id == NULL || BLI_findindex(which_libbase(G_MAIN, GS(id->name)), id) != -1);
}

bool BKE_id_can_be_asset(const ID *id)
{
  return !ID_IS_LINKED(id) && !ID_IS_OVERRIDE_LIBRARY(id) &&
         BKE_idtype_idcode_is_linkable(GS(id->name));
}

ID *BKE_id_owner_get(ID *id)
{
  const IDTypeInfo *idtype = BKE_idtype_get_info_from_id(id);
  if (idtype->owner_pointer_get != NULL) {
    ID **owner_id_pointer = idtype->owner_pointer_get(id);
    if (owner_id_pointer != NULL) {
      return *owner_id_pointer;
    }
  }
  return NULL;
}

bool BKE_id_is_editable(const Main *bmain, const ID *id)
{
  return !(ID_IS_LINKED(id) || BKE_lib_override_library_is_system_defined(bmain, id));
}

/************************* Datablock order in UI **************************/

static int *id_order_get(ID *id)
{
  /* Only for workspace tabs currently. */
  switch (GS(id->name)) {
    case ID_WS:
      return &((WorkSpace *)id)->order;
    default:
      return NULL;
  }
}

static int id_order_compare(const void *a, const void *b)
{
  ID *id_a = ((LinkData *)a)->data;
  ID *id_b = ((LinkData *)b)->data;

  int *order_a = id_order_get(id_a);
  int *order_b = id_order_get(id_b);

  if (order_a && order_b) {
    if (*order_a < *order_b) {
      return -1;
    }
    if (*order_a > *order_b) {
      return 1;
    }
  }

  return strcmp(id_a->name, id_b->name);
}

void BKE_id_ordered_list(ListBase *ordered_lb, const ListBase *lb)
{
  BLI_listbase_clear(ordered_lb);

  LISTBASE_FOREACH (ID *, id, lb) {
    BLI_addtail(ordered_lb, BLI_genericNodeN(id));
  }

  BLI_listbase_sort(ordered_lb, id_order_compare);

  int num = 0;
  LISTBASE_FOREACH (LinkData *, link, ordered_lb) {
    int *order = id_order_get(link->data);
    if (order) {
      *order = num++;
    }
  }
}

void BKE_id_reorder(const ListBase *lb, ID *id, ID *relative, bool after)
{
  int *id_order = id_order_get(id);
  int relative_order;

  if (relative) {
    relative_order = *id_order_get(relative);
  }
  else {
    relative_order = (after) ? BLI_listbase_count(lb) : 0;
  }

  if (after) {
    /* Insert after. */
    LISTBASE_FOREACH (ID *, other, lb) {
      int *order = id_order_get(other);
      if (*order > relative_order) {
        (*order)++;
      }
    }

    *id_order = relative_order + 1;
  }
  else {
    /* Insert before. */
    LISTBASE_FOREACH (ID *, other, lb) {
      int *order = id_order_get(other);
      if (*order < relative_order) {
        (*order)--;
      }
    }

    *id_order = relative_order - 1;
  }
}

void BKE_id_blend_write(BlendWriter *writer, ID *id)
{
  if (id->asset_data) {
    BKE_asset_metadata_write(writer, id->asset_data);
  }

  if (id->library_weak_reference != NULL) {
    BLO_write_struct(writer, LibraryWeakReference, id->library_weak_reference);
  }

  /* ID_WM's id->properties are considered runtime only, and never written in .blend file. */
  if (id->properties && !ELEM(GS(id->name), ID_WM)) {
    IDP_BlendWrite(writer, id->properties);
  }

  if (id->override_library) {
    BLO_write_struct(writer, IDOverrideLibrary, id->override_library);

    BLO_write_struct_list(writer, IDOverrideLibraryProperty, &id->override_library->properties);
    LISTBASE_FOREACH (IDOverrideLibraryProperty *, op, &id->override_library->properties) {
      BLO_write_string(writer, op->rna_path);

      BLO_write_struct_list(writer, IDOverrideLibraryPropertyOperation, &op->operations);
      LISTBASE_FOREACH (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
        if (opop->subitem_reference_name) {
          BLO_write_string(writer, opop->subitem_reference_name);
        }
        if (opop->subitem_local_name) {
          BLO_write_string(writer, opop->subitem_local_name);
        }
      }
    }
  }
}
