/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

/* all types are needed here, in order to do memory operations */
#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_key_types.h"
#include "DNA_node_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_armature.hh"
#include "BKE_asset.hh"
#include "BKE_bpath.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_namemap.hh"
#include "BKE_node.hh"
#include "BKE_rigidbody.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"

#include "BLO_read_write.hh"

#include "atomic_ops.h"

#include "lib_intern.hh"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "BLI_time_utildefines.h"
#endif

using blender::Vector;

using namespace blender::bke::id;

static CLG_LogRef LOG = {"lib.id"};

IDTypeInfo IDType_ID_LINK_PLACEHOLDER = {
    /*id_code*/ ID_LINK_PLACEHOLDER,
    /*id_filter*/ 0,
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_NULL,
    /*struct_size*/ sizeof(ID),
    /*name*/ "LinkPlaceholder",
    /*name_plural*/ N_("link_placeholders"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_ID,
    /*flags*/ IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ nullptr,
    /*free_data*/ nullptr,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ nullptr,
    /*blend_read_data*/ nullptr,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
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
                                                char *path_dst,
                                                size_t path_dst_maxncpy,
                                                const char *path_src)
{
  const char **data = static_cast<const char **>(bpath_data->user_data);
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
     * because it won't work for paths that start with `//../` */
    BLI_path_normalize(filepath);
    BLI_path_rel(filepath, base_new);
    BLI_strncpy(path_dst, filepath, path_dst_maxncpy);
    return true;
  }

  /* Path was not relative to begin with. */
  return false;
}

/**
 * This has to be called from each make_local_* func, we could call from BKE_lib_id_make_local()
 * but then the make local functions would not be self contained.
 *
 * This function can be used to remap paths in both directions. Typically, an ID comes from a
 * library and is made local (`lib_to` is then `nullptr`). But an ID can also be moved from current
 * Main into a library (`lib_from is then `nullptr`), or between two libraries (both `lib_to` and
 * `lib_from` are provided then).
 *
 * \param lib_to: The library into which the id is moved to
 * (used to get the destination root* path). If `nullptr`, the current #Main::filepath is used.
 *
 * \param lib_from: The library from which the id is coming from
 * (used to get the source root path). If `nullptr`, the current #Main::filepath is used.
 *
 * TODO: This can probably be replaced by an ID-level version of #BKE_bpath_relative_rebase.
 */
static void lib_id_library_local_paths(Main *bmain, Library *lib_to, Library *lib_from, ID *id)
{
  BLI_assert(lib_to || lib_from);
  const char *bpath_user_data[2] = {
      lib_to ? lib_to->runtime->filepath_abs : BKE_main_blendfile_path(bmain),
      lib_from ? lib_from->runtime->filepath_abs : BKE_main_blendfile_path(bmain)};

  BPathForeachPathData path_data{};
  path_data.bmain = bmain;
  path_data.callback_function = lib_id_library_local_paths_callback;
  path_data.flag = BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE;
  path_data.user_data = (void *)bpath_user_data;
  BKE_bpath_foreach_path_id(&path_data, id);
}

static int lib_id_clear_library_data_users_update_cb(LibraryIDLinkCallbackData *cb_data)
{
  ID *id = static_cast<ID *>(cb_data->user_data);
  if (*cb_data->id_pointer == id) {
    /* Even though the ID itself remain the same after being made local, from depsgraph point of
     * view this is a different ID. Hence we need to tag all of its users for a copy-on-eval
     * update. */
    DEG_id_tag_update_ex(
        cb_data->bmain, cb_data->owner_id, ID_RECALC_TAG_FOR_UNDO | ID_RECALC_SYNC_TO_EVAL);
    return IDWALK_RET_STOP_ITER;
  }
  return IDWALK_RET_NOP;
}

void BKE_lib_id_clear_library_data(Main *bmain, ID *id, const int flags)
{
  const bool id_in_mainlist = (id->tag & ID_TAG_NO_MAIN) == 0 &&
                              (id->flag & ID_FLAG_EMBEDDED_DATA) == 0;

  if (id_in_mainlist) {
    BKE_main_namemap_remove_id(*bmain, *id);
  }

  lib_id_library_local_paths(bmain, nullptr, id->lib, id);

  id_fake_user_clear(id);

  id->lib = nullptr;
  id->tag &= ~(ID_TAG_INDIRECT | ID_TAG_EXTERN);
  id->flag &= ~(ID_FLAG_INDIRECT_WEAK_LINK | ID_FLAG_LINKED_AND_PACKED);
  if (id_in_mainlist) {
    IDNewNameResult result = BKE_id_new_name_validate(*bmain,
                                                      *which_libbase(bmain, GS(id->name)),
                                                      *id,
                                                      nullptr,
                                                      IDNewNameMode::RenameExistingNever,
                                                      false);
    if (!ELEM(result.action,
              IDNewNameResult::Action::UNCHANGED,
              IDNewNameResult::Action::UNCHANGED_COLLISION))
    {
      bmain->is_memfile_undo_written = false;
    }
  }

  /* Conceptually, an ID made local is not the same as the linked one anymore. Reflect that by
   * regenerating its session UID. */
  if ((id->tag & ID_TAG_TEMP_MAIN) == 0) {
    BKE_lib_libblock_session_uid_renew(id);
  }

  if (ID_IS_ASSET(id)) {
    if ((flags & LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR) != 0) {
      const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);
      if (idtype_info && idtype_info->asset_type_info &&
          idtype_info->asset_type_info->on_clear_asset_fn)
      {
        idtype_info->asset_type_info->on_clear_asset_fn(id, id->asset_data);
      }
      BKE_asset_metadata_free(&id->asset_data);
    }
    else {
      /* Assets should always have a fake user. Ensure this is the case after "Make Local". */
      id_fake_user_set(id);
    }
  }

  /* Ensure that the deephash is reset when making an ID local (in case it was previously a packed
   * linked ID), as this is by definition not a valid deephash anymore (that ID is now a fully
   * independent copy living in another blendfile). */
  id->deep_hash = {};

  /* We need to tag this IDs and all of its users, conceptually new local ID and original linked
   * ones are two completely different data-blocks that were virtually remapped, even though in
   * reality they remain the same data. For undo this info is critical now. */
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_SYNC_TO_EVAL);
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
  if (key != nullptr) {
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
    if (id->tag & ID_TAG_INDIRECT) {
      id->tag &= ~ID_TAG_INDIRECT;
      id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;
      id->tag |= ID_TAG_EXTERN;
      id->lib->runtime->parent = nullptr;
    }
  }
}

void id_lib_indirect_weak_link(ID *id)
{
  if (id && ID_IS_LINKED(id)) {
    BLI_assert(BKE_idtype_idcode_is_linkable(GS(id->name)));
    if (id->tag & ID_TAG_INDIRECT) {
      id->flag |= ID_FLAG_INDIRECT_WEAK_LINK;
    }
  }
}

void id_us_ensure_real(ID *id)
{
  if (id) {
    const int limit = ID_FAKE_USERS(id);
    id->tag |= ID_TAG_EXTRAUSER;
    if (id->us <= limit) {
      if (id->us < limit || ((id->us == limit) && (id->tag & ID_TAG_EXTRAUSER_SET))) {
        CLOG_ERROR(&LOG,
                   "ID user count error: %s (from '%s')",
                   id->name,
                   id->lib ? id->lib->runtime->filepath_abs : "[Main]");
      }
      id->us = limit + 1;
      id->tag |= ID_TAG_EXTRAUSER_SET;
    }
  }
}

void id_us_clear_real(ID *id)
{
  if (id && (id->tag & ID_TAG_EXTRAUSER)) {
    if (id->tag & ID_TAG_EXTRAUSER_SET) {
      id->us--;
      BLI_assert(id->us >= ID_FAKE_USERS(id));
    }
    id->tag &= ~(ID_TAG_EXTRAUSER | ID_TAG_EXTRAUSER_SET);
  }
}

void id_us_plus_no_lib(ID *id)
{
  if (id) {
    if ((id->tag & ID_TAG_EXTRAUSER) && (id->tag & ID_TAG_EXTRAUSER_SET)) {
      BLI_assert(id->us >= 1);
      /* No need to increase count, just tag extra user as no more set.
       * Avoids annoying & inconsistent +1 in user count. */
      id->tag &= ~ID_TAG_EXTRAUSER_SET;
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
                   id->lib ? id->lib->runtime->filepath_abs : "[Main]",
                   id->us,
                   limit);
      }
      id->us = limit;
    }
    else {
      id->us--;
    }

    if ((id->us == limit) && (id->tag & ID_TAG_EXTRAUSER)) {
      /* We need an extra user here, but never actually incremented user count for it so far,
       * do it now. */
      id_us_ensure_real(id);
    }
  }
}

void id_fake_user_set(ID *id)
{
  if (id && !(id->flag & ID_FLAG_FAKEUSER)) {
    id->flag |= ID_FLAG_FAKEUSER;
    id_us_plus(id);
  }
}

void id_fake_user_clear(ID *id)
{
  if (id && (id->flag & ID_FLAG_FAKEUSER)) {
    id->flag &= ~ID_FLAG_FAKEUSER;
    id_us_min(id);
  }
}

void BKE_id_newptr_and_tag_clear(ID *id)
{
  /* We assume that if this ID has no new ID, its embedded data has not either. */
  if (id->newid == nullptr) {
    return;
  }

  id->newid->tag &= ~ID_TAG_NEW;
  id->newid = nullptr;

  /* Deal with embedded data too. */
  /* NOTE: even though ShapeKeys are not technically embedded data currently, they behave as such
   * in most cases, so for sake of consistency treat them as such here. Also mirrors the behavior
   * in `BKE_lib_id_make_local`. */
  Key *key = BKE_key_from_id(id);
  if (key != nullptr) {
    BKE_id_newptr_and_tag_clear(&key->id);
  }
  bNodeTree *ntree = blender::bke::node_tree_from_id(id);
  if (ntree != nullptr) {
    BKE_id_newptr_and_tag_clear(&ntree->id);
  }
  if (GS(id->name) == ID_SCE) {
    Collection *master_collection = ((Scene *)id)->master_collection;
    if (master_collection != nullptr) {
      BKE_id_newptr_and_tag_clear(&master_collection->id);
    }
  }
}

static int lib_id_expand_local_cb(LibraryIDLinkCallbackData *cb_data)
{
  Main *bmain = cb_data->bmain;
  ID *self_id = cb_data->self_id;
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
    if (*id_pointer != nullptr && ID_IS_LINKED(*id_pointer)) {
      BLI_assert(*id_pointer != self_id);

      BKE_lib_id_clear_library_data(bmain, *id_pointer, flags);
    }
    return IDWALK_RET_NOP;
  }

  /* Can happen that we get un-linkable ID here, e.g. with shape-key referring to itself
   * (through drivers)...
   * Just skip it, shape key can only be either indirectly linked, or fully local, period.
   * And let's curse one more time that stupid useless shape-key ID type! */
  if (*id_pointer && *id_pointer != self_id &&
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

void lib_id_copy_ensure_local(Main *bmain, const ID *old_id, ID *new_id, const int flags)
{
  if (ID_IS_LINKED(old_id)) {
    /* For packed linked data copied into local IDs in Main, assume that they are no more related
     * to their original library source, and clear their deephash.
     *
     * NOTE: In case more control is needed over that behavior in the future, a new flag can be
     * added instead. */
    new_id->deep_hash = {};

    BKE_lib_id_expand_local(bmain, new_id, flags);
    lib_id_library_local_paths(bmain, nullptr, old_id->lib, new_id);
  }
}

void BKE_lib_id_make_local_generic_action_define(
    Main *bmain, ID *id, int flags, bool *r_force_local, bool *r_force_copy)
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
    if ((flags & LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR) != 0) {
      BKE_lib_override_library_make_local(bmain, id);
    }
    BKE_lib_id_expand_local(bmain, id, flags);
  }
  else if (force_copy) {
    const int copy_flags =
        (LIB_ID_COPY_DEFAULT |
         ((flags & LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR) != 0 ? LIB_ID_COPY_NO_LIB_OVERRIDE : 0));
    ID *id_new = BKE_id_copy_ex(bmain, id, nullptr, copy_flags);

    /* Should not fail in expected use cases,
     * but a few ID types cannot be copied (LIB, WM, SCR...). */
    if (id_new != nullptr) {
      id_new->us = 0;

      /* setting newid is mandatory for complex make_lib_local logic... */
      ID_NEW_SET(id, id_new);
      Key *key = BKE_key_from_id(id), *key_new = BKE_key_from_id(id);
      if (key && key_new) {
        ID_NEW_SET(key, key_new);
      }
      bNodeTree *ntree = blender::bke::node_tree_from_id(id),
                *ntree_new = blender::bke::node_tree_from_id(id_new);
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

  /* Skip indirectly linked IDs, unless the whole library is made local, or handling them is
   * explicitly requested. */
  if (!(lib_local || (flags & LIB_ID_MAKELOCAL_INDIRECT) != 0) && (id->tag & ID_TAG_INDIRECT)) {
    return false;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info == nullptr) {
    BLI_assert_msg(0, "IDType Missing IDTypeInfo");
    return false;
  }

  BLI_assert((idtype_info->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0);

  if (idtype_info->make_local != nullptr) {
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
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;
  IDCopyLibManagementData *data = static_cast<IDCopyLibManagementData *>(cb_data->user_data);

  /* Remap self-references to new copied ID. */
  if (id == data->id_src) {
    /* We cannot use self_id here, it is not *always* id_dst (thanks to confounded node-trees!). */
    id = *id_pointer = data->id_dst;
  }

  /* Increase used IDs refcount if needed and required. */
  if ((data->flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0 && (cb_flag & IDWALK_CB_USER)) {
    if ((data->flag & LIB_ID_CREATE_NO_MAIN) != 0) {
      BLI_assert(cb_data->self_id->tag & ID_TAG_NO_MAIN);
      id_us_plus_no_lib(id);
    }
    else if (ID_IS_LINKED(cb_data->owner_id)) {
      /* Do not mark copied ID as directly linked, if its current user is also linked data (which
       * is now fairly common when using 'copy_in_lib' feature). */
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

ID *BKE_id_copy_in_lib(Main *bmain,
                       std::optional<Library *> owner_library,
                       const ID *id,
                       std::optional<const ID *> new_owner_id,
                       ID **new_id_p,
                       const int flag)
{
  ID *newid = (new_id_p != nullptr) ? *new_id_p : nullptr;
  BLI_assert_msg(newid || (flag & LIB_ID_CREATE_NO_ALLOCATE) == 0,
                 "Copying with 'no allocate' behavior should always get a non-null new ID buffer");

  /* Make sure destination pointer is all good. */
  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
    newid = nullptr;
  }
  else {
    if (!newid) {
      /* Invalid case, already caught by the assert above. */
      return nullptr;
    }
  }

  /* Early output if source is nullptr. */
  if (id == nullptr) {
    return nullptr;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != nullptr) {
    if ((idtype_info->flags & IDTYPE_FLAGS_NO_COPY) != 0) {
      return nullptr;
    }

    BKE_libblock_copy_in_lib(bmain, owner_library, id, new_owner_id, &newid, flag);

    if (idtype_info->copy_data != nullptr) {
      idtype_info->copy_data(bmain, owner_library, newid, id, flag);
    }
  }
  else {
    BLI_assert_msg(0, "IDType Missing IDTypeInfo");
  }

  BLI_assert_msg(newid, "Could not get an allocated new ID to copy into");
  if (!newid) {
    return nullptr;
  }

  /* Update ID refcount, remap pointers to self in new ID. */
  IDCopyLibManagementData data{};
  data.id_src = id;
  data.id_dst = newid;
  data.flag = flag;
  /* When copying an embedded ID, typically at this point its owner ID pointer will still point to
   * the owner of the source, this code has no access to its valid (i.e. destination) owner. This
   * can be added at some point if needed, but currently the #id_copy_libmanagement_cb callback
   * does need this information. */
  BKE_library_foreach_ID_link(
      bmain, newid, id_copy_libmanagement_cb, &data, IDWALK_IGNORE_MISSING_OWNER_ID);

  /* FIXME: Check if this code can be moved in #BKE_libblock_copy_in_lib ? Would feel more fitted
   * there, having library handling split between both functions does not look good. */
  /* Do not make new copy local in case we are copying outside of main...
   * XXX TODO: is this behavior OK, or should we need a separate flag to control that? */
  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    BLI_assert(!owner_library || newid->lib == *owner_library);
    /* If the ID was copied into a library, ensure paths are properly remapped, and that it has a
     * 'linked' tag set. */
    if (ID_IS_LINKED(newid)) {
      if (newid->lib != id->lib) {
        lib_id_library_local_paths(bmain, newid->lib, id->lib, newid);
      }
      if ((newid->tag & (ID_TAG_EXTERN | ID_TAG_INDIRECT)) == 0) {
        newid->tag |= ID_TAG_EXTERN;
      }
    }
    /* Expanding local linked ID usages should never be needed with embedded IDs - this will be
     * handled together with their owner ID copying code. */
    else if ((newid->flag & ID_FLAG_EMBEDDED_DATA) == 0) {
      lib_id_copy_ensure_local(bmain, id, newid, 0);
    }
  }

  else {
    /* NOTE: Do not call `ensure_local` for IDs copied outside of Main, even if they do become
     * local.
     *
     * Most of the time, this would not be the desired behavior currently.
     *
     * In the few cases where this is actually needed (e.g. from liboverride resync code, see
     * #lib_override_library_create_from), calling code is responsible for this. */
    newid->lib = owner_library ? *owner_library : id->lib;
  }

  if (new_id_p != nullptr) {
    *new_id_p = newid;
  }

  return newid;
}

ID *BKE_id_copy_ex(Main *bmain, const ID *id, ID **new_id_p, const int flag)
{
  return BKE_id_copy_in_lib(bmain, std::nullopt, id, std::nullopt, new_id_p, flag);
}

ID *BKE_id_copy(Main *bmain, const ID *id)
{
  return BKE_id_copy_in_lib(bmain, std::nullopt, id, std::nullopt, nullptr, LIB_ID_COPY_DEFAULT);
}

ID *BKE_id_copy_for_duplicate(Main *bmain,
                              ID *id,
                              const eDupli_ID_Flags duplicate_flags,
                              const int copy_flags)
{
  if (id == nullptr) {
    return id;
  }
  if (id->newid == nullptr) {
    const bool do_linked_id = (duplicate_flags & USER_DUP_LINKED_ID) != 0;
    if (!(do_linked_id || !ID_IS_LINKED(id))) {
      return id;
    }

    ID *id_new = BKE_id_copy_ex(bmain, id, nullptr, copy_flags);
    /* Copying add one user by default, need to get rid of that one. */
    id_us_min(id_new);
    ID_NEW_SET(id, id_new);

    /* Shape keys are always copied with their owner ID, by default. */
    ID *key_new = (ID *)BKE_key_from_id(id_new);
    ID *key = (ID *)BKE_key_from_id(id);
    if (key != nullptr) {
      ID_NEW_SET(key, key_new);
    }

    /* NOTE: embedded data (root node-trees and master collections) should never be referenced by
     * anything else, so we do not need to set their newid pointer and flag. */

    BKE_animdata_duplicate_id_action(bmain, id_new, duplicate_flags);
    if (key_new != nullptr) {
      BKE_animdata_duplicate_id_action(bmain, key_new, duplicate_flags);
    }
    /* Note that actions of embedded data (root node-trees and master collections) are handled
     * by #BKE_animdata_duplicate_id_action as well. */
  }
  return id->newid;
}

static int foreach_assign_id_to_orig_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;

  if (*id_p) {
    ID *id = *id_p;
    *id_p = DEG_get_original(id);

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

  if (newid == nullptr) {
    return newid;
  }

  /* Assign ID references directly used by the given ID to their original complementary parts.
   *
   * For example, when is called on an evaluated object will assign object->data to its original
   * pointer, the evaluated object->data will be kept unchanged. */
  BKE_library_foreach_ID_link(
      nullptr, newid, foreach_assign_id_to_orig_callback, nullptr, IDWALK_NOP);

  /* Shape keys reference on evaluated ID is preserved to keep driver paths available, but the key
   * data is likely to be invalid now due to modifiers, so clear the shape key reference avoiding
   * any possible shape corruption. */
  if (DEG_is_evaluated(id)) {
    Key **key_p = BKE_key_from_id_p(newid);
    if (key_p) {
      *key_p = nullptr;
    }
  }

  return newid;
}

void BKE_id_move_to_same_lib(Main &bmain, ID &id, const ID &owner_id)
{
  if (owner_id.lib == id.lib) {
    /* `id` is already in the target library, nothing to do. */
    return;
  }
  if (ID_IS_LINKED(&id)) {
    BLI_assert_msg(false, "Only local IDs can be moved into a library");
    /* Protect release builds against errors in calling code, as continuing here can lead to
     * critical Main data-base corruption. */
    return;
  }

  BKE_main_namemap_remove_id(bmain, id);

  id.lib = owner_id.lib;
  id.tag |= ID_TAG_INDIRECT;

  ListBase &lb = *which_libbase(&bmain, GS(id.name));
  BKE_id_new_name_validate(
      bmain, lb, id, BKE_id_name(id), IDNewNameMode::RenameExistingNever, true);
}

static void id_embedded_swap(Main *bmain,
                             ID **embedded_id_a,
                             ID **embedded_id_b,
                             const bool do_full_id,
                             IDRemapper *remapper_id_a,
                             IDRemapper *remapper_id_b);

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 */
static void id_swap(Main *bmain,
                    ID *id_a,
                    ID *id_b,
                    const bool do_full_id,
                    const bool do_self_remap,
                    IDRemapper *input_remapper_id_a,
                    IDRemapper *input_remapper_id_b,
                    const int self_remap_flags)
{
  BLI_assert(GS(id_a->name) == GS(id_b->name));

  IDRemapper *remapper_id_a = input_remapper_id_a;
  IDRemapper *remapper_id_b = input_remapper_id_b;
  if (do_self_remap) {
    if (remapper_id_a == nullptr) {
      remapper_id_a = MEM_new<IDRemapper>(__func__);
    }
    if (remapper_id_b == nullptr) {
      remapper_id_b = MEM_new<IDRemapper>(__func__);
    }
  }

  const IDTypeInfo *id_type = BKE_idtype_get_info_from_id(id_a);
  BLI_assert(id_type != nullptr);
  const size_t id_struct_size = id_type->struct_size;

  const ID id_a_back = *id_a;
  const ID id_b_back = *id_b;

  char *id_swap_buff = static_cast<char *>(alloca(id_struct_size));

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
    id_a->system_properties = id_b_back.system_properties;
    id_b->system_properties = id_a_back.system_properties;
    /* Exception: recalc flags. */
    id_a->recalc = id_b_back.recalc;
    id_b->recalc = id_a_back.recalc;
  }

  id_embedded_swap(bmain,
                   (ID **)blender::bke::node_tree_ptr_from_id(id_a),
                   (ID **)blender::bke::node_tree_ptr_from_id(id_b),
                   do_full_id,
                   remapper_id_a,
                   remapper_id_b);
  if (GS(id_a->name) == ID_SCE) {
    Scene *scene_a = (Scene *)id_a;
    Scene *scene_b = (Scene *)id_b;
    id_embedded_swap(bmain,
                     (ID **)&scene_a->master_collection,
                     (ID **)&scene_b->master_collection,
                     do_full_id,
                     remapper_id_a,
                     remapper_id_b);
  }

  if (remapper_id_a != nullptr) {
    remapper_id_a->add(id_b, id_a);
  }
  if (remapper_id_b != nullptr) {
    remapper_id_b->add(id_a, id_b);
  }

  /* Finalize remapping of internal references to self broken by swapping, if requested. */
  if (do_self_remap) {
    BKE_libblock_relink_multiple(
        bmain, {id_a}, ID_REMAP_TYPE_REMAP, *remapper_id_a, self_remap_flags);
    BKE_libblock_relink_multiple(
        bmain, {id_b}, ID_REMAP_TYPE_REMAP, *remapper_id_b, self_remap_flags);
  }

  if ((id_type->flags & IDTYPE_FLAGS_NO_ANIMDATA) == 0 && bmain) {
    /* Action Slots point to the IDs they animate, and thus now also needs swapping. Instead of
     * doing this here (and requiring knowledge of how that's supposed to be done), just mark these
     * pointers as dirty so that they're rebuilt at first use.
     *
     * There are a few calls to id_swap() where `bmain` is nil. None of these matter here, though:
     *
     * - At blendfile load (`read_libblock_undo_restore_at_old_address()`). Fine
     *   because after loading the action slot user cache is rebuilt anyway.
     * - Swapping window managers (`swap_wm_data_for_blendfile()`). Fine because
     *   WMs cannot be animated.
     * - Palette undo code (`palette_undo_preserve()`). Fine because palettes
     *   cannot be animated. */
    blender::bke::animdata::action_slots_user_cache_invalidate(*bmain);
  }

  if (input_remapper_id_a == nullptr && remapper_id_a != nullptr) {
    MEM_delete(remapper_id_a);
  }
  if (input_remapper_id_b == nullptr && remapper_id_b != nullptr) {
    MEM_delete(remapper_id_b);
  }
}

/* Conceptually, embedded IDs are part of their owner's data. However, some parts of the code
 * (like e.g. the depsgraph) may treat them as independent IDs, so swapping them here and
 * switching their pointers in the owner IDs allows to help not break cached relationships and
 * such (by preserving the pointer values). */
static void id_embedded_swap(Main *bmain,
                             ID **embedded_id_a,
                             ID **embedded_id_b,
                             const bool do_full_id,
                             IDRemapper *remapper_id_a,
                             IDRemapper *remapper_id_b)
{
  if (embedded_id_a != nullptr && *embedded_id_a != nullptr) {
    BLI_assert(embedded_id_b != nullptr);

    if (*embedded_id_b == nullptr) {
      /* Cannot swap anything if one of the embedded IDs is nullptr. */
      return;
    }

    /* Do not remap internal references to itself here, since embedded IDs pointers also need to be
     * potentially remapped in owner ID's data, which will also handle embedded IDs data. */
    id_swap(
        bmain, *embedded_id_a, *embedded_id_b, do_full_id, false, remapper_id_a, remapper_id_b, 0);
    /* Manual 'remap' of owning embedded pointer in owner ID. */
    std::swap(*embedded_id_a, *embedded_id_b);

    /* Restore internal pointers to the swapped embedded IDs in their owners' data. This also
     * includes the potential self-references inside the embedded IDs themselves. */
    if (remapper_id_a != nullptr) {
      remapper_id_a->add(*embedded_id_b, *embedded_id_a);
    }
    if (remapper_id_b != nullptr) {
      remapper_id_b->add(*embedded_id_a, *embedded_id_b);
    }
  }
}

void BKE_lib_id_swap(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags)
{
  id_swap(bmain, id_a, id_b, false, do_self_remap, nullptr, nullptr, self_remap_flags);
}

void BKE_lib_id_swap_full(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags)
{
  id_swap(bmain, id_a, id_b, true, do_self_remap, nullptr, nullptr, self_remap_flags);
}

bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop)
{
  ID *newid = nullptr;

  if (id && (ID_REAL_USERS(id) > 1)) {
    /* If property isn't editable,
     * we're going to have an extra block hanging around until we save. */
    if (RNA_property_editable(ptr, prop)) {
      Main *bmain = CTX_data_main(C);
      /* copy animation actions too */
      newid = BKE_id_copy_ex(bmain, id, nullptr, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
      if (newid != nullptr) {
        /* us is 1 by convention with new IDs, but RNA_property_pointer_set
         * will also increment it, decrement it here. */
        id_us_min(newid);

        /* assign copy */
        PointerRNA idptr = RNA_id_pointer_create(newid);
        RNA_property_pointer_set(ptr, prop, idptr, nullptr);
        RNA_property_update(C, ptr, prop);

        return true;
      }
    }
  }

  return false;
}

static int libblock_management_us_plus(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;
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
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;
  if (cb_flag & IDWALK_CB_USER) {
    id_us_min(*id_pointer);
  }
  /* We can do nothing in IDWALK_CB_USER_ONE case! */

  return IDWALK_RET_NOP;
}

void BKE_libblock_management_main_add(Main *bmain, void *idv)
{
  ID *id = static_cast<ID *>(idv);

  BLI_assert(bmain != nullptr);
  if ((id->tag & ID_TAG_NO_MAIN) == 0) {
    return;
  }

  if ((id->tag & ID_TAG_NOT_ALLOCATED) != 0) {
    /* We cannot add non-allocated ID to Main! */
    return;
  }

  /* We cannot allow non-userrefcounting IDs in Main database! */
  if ((id->tag & ID_TAG_NO_USER_REFCOUNT) != 0) {
    BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, nullptr, IDWALK_NOP);
  }

  ListBase *lb = which_libbase(bmain, GS(id->name));
  BKE_main_lock(bmain);
  BLI_addtail(lb, id);
  /* We need to allow adding extra datablocks into libraries too, e.g. to support generating new
   * overrides for recursive resync. */
  BKE_id_new_name_validate(*bmain, *lb, *id, nullptr, IDNewNameMode::RenameExistingNever, true);
  /* alphabetic insertion: is in new_id */
  id->tag &= ~(ID_TAG_NO_MAIN | ID_TAG_NO_USER_REFCOUNT);
  bmain->is_memfile_undo_written = false;
  BKE_main_unlock(bmain);

  BKE_lib_libblock_session_uid_ensure(id);
}

void BKE_libblock_management_main_remove(Main *bmain, void *idv)
{
  ID *id = static_cast<ID *>(idv);

  BLI_assert(bmain != nullptr);
  if ((id->tag & ID_TAG_NO_MAIN) != 0) {
    return;
  }

  /* For now, allow userrefcounting IDs to get out of Main - can be handy in some cases... */

  ListBase *lb = which_libbase(bmain, GS(id->name));
  BKE_main_lock(bmain);
  BLI_remlink(lb, id);
  BKE_main_namemap_remove_id(*bmain, *id);
  id->tag |= ID_TAG_NO_MAIN;
  bmain->is_memfile_undo_written = false;
  BKE_main_unlock(bmain);
}

void BKE_libblock_management_usercounts_set(Main *bmain, void *idv)
{
  ID *id = static_cast<ID *>(idv);

  if ((id->tag & ID_TAG_NO_USER_REFCOUNT) == 0) {
    return;
  }

  BKE_library_foreach_ID_link(bmain, id, libblock_management_us_plus, nullptr, IDWALK_NOP);
  id->tag &= ~ID_TAG_NO_USER_REFCOUNT;
}

void BKE_libblock_management_usercounts_clear(Main *bmain, void *idv)
{
  ID *id = static_cast<ID *>(idv);

  /* We do not allow IDs in Main database to not be userrefcounting. */
  if ((id->tag & ID_TAG_NO_USER_REFCOUNT) != 0 || (id->tag & ID_TAG_NO_MAIN) != 0) {
    return;
  }

  BKE_library_foreach_ID_link(bmain, id, libblock_management_us_min, nullptr, IDWALK_NOP);
  id->tag |= ID_TAG_NO_USER_REFCOUNT;
}

void BKE_main_id_tag_listbase(ListBase *lb, const int tag, const bool value)
{
  ID *id;
  if (value) {
    for (id = static_cast<ID *>(lb->first); id; id = static_cast<ID *>(id->next)) {
      id->tag |= tag;
    }
  }
  else {
    const int ntag = ~tag;
    for (id = static_cast<ID *>(lb->first); id; id = static_cast<ID *>(id->next)) {
      id->tag &= ntag;
    }
  }
}

void BKE_main_id_tag_idcode(Main *mainvar, const short type, const int tag, const bool value)
{
  ListBase *lb = which_libbase(mainvar, type);

  BKE_main_id_tag_listbase(lb, tag, value);
}

void BKE_main_id_tag_all(Main *mainvar, const int tag, const bool value)
{
  MainListsArray lbarray = BKE_main_lists_get(*mainvar);
  int a = lbarray.size();
  while (a--) {
    BKE_main_id_tag_listbase(lbarray[a], tag, value);
  }
}

void BKE_main_id_flag_listbase(ListBase *lb, const int flag, const bool value)
{
  ID *id;
  if (value) {
    for (id = static_cast<ID *>(lb->first); id; id = static_cast<ID *>(id->next)) {
      id->tag |= flag;
    }
  }
  else {
    const int nflag = ~flag;
    for (id = static_cast<ID *>(lb->first); id; id = static_cast<ID *>(id->next)) {
      id->tag &= nflag;
    }
  }
}

void BKE_main_id_flag_all(Main *bmain, const int flag, const bool value)
{
  MainListsArray lbarray = BKE_main_lists_get(*bmain);
  int a = lbarray.size();
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
  ID **id_array = MEM_malloc_arrayN<ID *>(size_t(lb_len), __func__);
  GSet *gset = BLI_gset_str_new_ex(__func__, lb_len);
  int i = 0;
  LISTBASE_FOREACH (ID *, id, lb) {
    if (!ID_IS_LINKED(id)) {
      id_array[i] = id;
      i++;
    }
  }
  for (i = 0; i < lb_len; i++) {
    if (!BLI_gset_add(gset, BKE_id_name(*id_array[i]))) {
      BKE_id_new_name_validate(
          *bmain, *lb, *id_array[i], nullptr, IDNewNameMode::RenameExistingNever, false);
    }
  }
  BLI_gset_free(gset, nullptr);
  MEM_freeN(id_array);
}

void BKE_main_lib_objects_recalc_all(Main *bmain)
{
  Object *ob;

  /* flag for full recalc */
  for (ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
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

void BKE_libblock_runtime_ensure(ID &id)
{
  if (!id.runtime) {
    id.runtime = MEM_new<blender::bke::id::ID_Runtime>(__func__);
  }
}

size_t BKE_libblock_get_alloc_info(short type, const char **r_name)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(type);

  if (id_type == nullptr) {
    if (r_name != nullptr) {
      *r_name = nullptr;
    }
    return 0;
  }

  if (r_name != nullptr) {
    *r_name = id_type->name;
  }
  return id_type->struct_size;
}

ID *BKE_libblock_alloc_notest(short type)
{
  const char *name;
  size_t size = BKE_libblock_get_alloc_info(type, &name);
  if (size != 0) {
    ID *id = static_cast<ID *>(MEM_callocN(size, name));
    return id;
  }
  BLI_assert_msg(0, "Request to allocate unknown data type");
  return nullptr;
}

void *BKE_libblock_alloc_in_lib(Main *bmain,
                                std::optional<Library *> owner_library,
                                short type,
                                const char *name,
                                const int flag)
{
  BLI_assert((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || bmain != nullptr);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_LOCAL) == 0);

  ID *id = BKE_libblock_alloc_notest(type);
  BKE_libblock_runtime_ensure(*id);

  if (id) {
    if ((flag & LIB_ID_CREATE_NO_MAIN) != 0) {
      id->tag |= ID_TAG_NO_MAIN;
    }
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0) {
      id->tag |= ID_TAG_NO_USER_REFCOUNT;
    }
    if (flag & LIB_ID_CREATE_LOCAL) {
      id->tag |= ID_TAG_LOCALIZED;
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

      /* This is important in "read-file do-version after lib-link" context mainly, but is a good
       * behavior for consistency in general: ID created for a Main should get that main's current
       * library pointer.
       *
       * NOTE: A bit convoluted.
       *   - When Main has a defined `curlib`, it is assumed to be a split main containing only IDs
       *     from that library. In that case, the library can be set later, and it avoids
       *     synchronization issues in the namemap between the one of that temp 'library' Main and
       *     the library ID runtime namemap itself. In a way, the ID can be assumed local to the
       *     current Main, for its assignment to this Main.
       *   - In all other cases, the Main is assumed 'complete', i.e. containing all local and
       *     linked IDs, In that case, it is critical that the ID gets the correct library assigned
       *     now, to ensure that the call to #BKE_id_new_name_validate gives a fully valid result
       *     once it has been assigned to the current Main.
       */
      if (bmain->curlib) {
        id->lib = nullptr;
      }
      else {
        id->lib = owner_library ? *owner_library : nullptr;
      }

      BKE_main_lock(bmain);
      BLI_addtail(lb, id);
      BKE_id_new_name_validate(*bmain, *lb, *id, name, IDNewNameMode::RenameExistingNever, true);
      bmain->is_memfile_undo_written = false;
      /* alphabetic insertion: is in new_id */
      BKE_main_unlock(bmain);

      /* Split Main case, now the ID should get the Main's #curlib. */
      if (bmain->curlib) {
        BLI_assert(!owner_library || *owner_library == bmain->curlib);
        id->lib = bmain->curlib;
      }

      /* This assert avoids having to keep name_map consistency when changing the library of an ID,
       * if this check is not true anymore it will have to be done here too. */
      BLI_assert(bmain->curlib == nullptr || bmain->curlib->runtime->name_map == nullptr);

      /* TODO: to be removed from here! */
      if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0) {
        DEG_id_type_tag(bmain, type);
      }
    }
    else {
      BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
      id->lib = owner_library ? *owner_library : nullptr;
    }

    /* We also need to ensure a valid `session_uid` for some non-main data (like embedded IDs).
     * IDs not allocated however should not need those (this would e.g. avoid generating session
     * UIDs for depsgraph evaluated IDs, if it was using this function). */
    if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
      BKE_lib_libblock_session_uid_ensure(id);
    }
  }

  return id;
}

void *BKE_libblock_alloc(Main *bmain, short type, const char *name, const int flag)
{
  return BKE_libblock_alloc_in_lib(bmain, std::nullopt, type, name, flag);
}

void BKE_libblock_init_empty(ID *id)
{
  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != nullptr) {
    if (idtype_info->init_data != nullptr) {
      idtype_info->init_data(id);
    }
    return;
  }

  BLI_assert_msg(0, "IDType Missing IDTypeInfo");
}

void BKE_libblock_runtime_reset_remapping_status(ID *id)
{
  id->runtime->remap.status = 0;
  id->runtime->remap.skipped_refcounted = 0;
  id->runtime->remap.skipped_direct = 0;
  id->runtime->remap.skipped_indirect = 0;
}

/* ********** ID session-wise UID management. ********** */
static uint global_session_uid = 0;

void BKE_lib_libblock_session_uid_ensure(ID *id)
{
  if (id->session_uid == MAIN_ID_SESSION_UID_UNSET) {
    BLI_assert((id->tag & ID_TAG_TEMP_MAIN) == 0); /* Caller must ensure this. */
    id->session_uid = atomic_add_and_fetch_uint32(&global_session_uid, 1);
    /* In case overflow happens, still assign a valid ID. This way opening files many times works
     * correctly. */
    if (UNLIKELY(id->session_uid == MAIN_ID_SESSION_UID_UNSET)) {
      id->session_uid = atomic_add_and_fetch_uint32(&global_session_uid, 1);
    }
  }
}

void BKE_lib_libblock_session_uid_renew(ID *id)
{
  id->session_uid = MAIN_ID_SESSION_UID_UNSET;
  BKE_lib_libblock_session_uid_ensure(id);
}

void *BKE_id_new_in_lib(Main *bmain,
                        std::optional<Library *> owner_library,
                        const short type,
                        const char *name)

{
  BLI_assert(bmain != nullptr);

  if (name == nullptr) {
    name = DATA_(BKE_idtype_idcode_to_name(type));
  }

  ID *id = static_cast<ID *>(BKE_libblock_alloc_in_lib(bmain, owner_library, type, name, 0));
  BKE_libblock_init_empty(id);

  return id;
}

void *BKE_id_new(Main *bmain, const short type, const char *name)
{
  return BKE_id_new_in_lib(bmain, std::nullopt, type, name);
}

void *BKE_id_new_nomain(const short type, const char *name)
{
  if (name == nullptr) {
    name = DATA_(BKE_idtype_idcode_to_name(type));
  }

  ID *id = static_cast<ID *>(BKE_libblock_alloc(
      nullptr,
      type,
      name,
      LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_DEG_TAG));
  BKE_libblock_init_empty(id);

  return id;
}

void BKE_libblock_copy_in_lib(Main *bmain,
                              std::optional<Library *> owner_library,
                              const ID *id,
                              std::optional<const ID *> new_owner_id,
                              ID **new_id_p,
                              const int orig_flag)
{
  ID *new_id = *new_id_p;
  int flag = orig_flag;

  const bool is_embedded_id = (id->flag & ID_FLAG_EMBEDDED_DATA) != 0;

  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || bmain != nullptr);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) != 0 || (flag & LIB_ID_CREATE_LOCAL) == 0);

  /* Embedded ID handling.
   *
   * NOTE: This makes copying code of embedded IDs non-reentrant (i.e. copying an embedded ID as
   * part of another embedded ID would not work properly). This is not an issue currently, but may
   * need to be addressed in the future. */
  if ((bmain != nullptr) && is_embedded_id) {
    flag |= LIB_ID_CREATE_NO_MAIN;
  }

  /* The id->flag bits to copy over. */
  const int copy_idflag_mask = ID_FLAG_EMBEDDED_DATA;
  /* The id->tag bits to copy over. */
  const int copy_idtag_mask =
      /* Only copy potentially existing 'linked' tags if the new ID is being placed into a library.
       *
       * Further tag and paths remapping is handled in #BKE_id_copy_in_lib.
       */
      ((owner_library && *owner_library) ? (ID_TAG_EXTERN | ID_TAG_INDIRECT) : 0);

  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) != 0) {
    /* `new_id_p` already contains pointer to allocated memory.
     * Clear and initialize it similar to BKE_libblock_alloc_in_lib. */
    const size_t size = BKE_libblock_get_alloc_info(GS(id->name), nullptr);
    memset(new_id, 0, size);
    BKE_libblock_runtime_ensure(*new_id);
    STRNCPY(new_id->name, id->name);
    new_id->us = 0;
    new_id->tag |= ID_TAG_NOT_ALLOCATED | ID_TAG_NO_MAIN | ID_TAG_NO_USER_REFCOUNT;
    new_id->lib = owner_library ? *owner_library : id->lib;
    /* TODO: Is this entirely consistent with BKE_libblock_alloc_in_lib, and can we
     * deduplicate the initialization code? */
  }
  else {
    new_id = static_cast<ID *>(
        BKE_libblock_alloc_in_lib(bmain, owner_library, GS(id->name), BKE_id_name(*id), flag));
  }
  BLI_assert(new_id != nullptr);

  if ((flag & LIB_ID_COPY_SET_COPIED_ON_WRITE) != 0) {
    new_id->tag |= ID_TAG_COPIED_ON_EVAL;
  }
  else {
    new_id->tag &= ~ID_TAG_COPIED_ON_EVAL;
  }

  const size_t id_len = BKE_libblock_get_alloc_info(GS(new_id->name), nullptr);
  const size_t id_offset = sizeof(ID);
  if (int(id_len) - int(id_offset) > 0) { /* signed to allow neg result */ /* XXX ????? */
    const char *cp = (const char *)id;
    char *cpn = (char *)new_id;

    memcpy(cpn + id_offset, cp + id_offset, id_len - id_offset);
  }

  new_id->flag = (new_id->flag & ~copy_idflag_mask) | (id->flag & copy_idflag_mask);
  new_id->tag = (new_id->tag & ~copy_idtag_mask) | (id->tag & copy_idtag_mask);

  /* Embedded ID data handling. */
  if (is_embedded_id && (orig_flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    new_id->tag &= ~ID_TAG_NO_MAIN;
  }
  /* NOTE: This also needs to run for ShapeKeys, which are not (yet) actual embedded IDs.
   * NOTE: for now, keep existing owner ID (i.e. owner of the source embedded ID) if no new one
   * is given. In some cases (e.g. depsgraph), this is important for later remapping to work
   * properly.
   */
  if (new_owner_id.has_value()) {
    const IDTypeInfo *idtype = BKE_idtype_get_info_from_id(new_id);
    BLI_assert(idtype->owner_pointer_get != nullptr);
    ID **owner_id_pointer = idtype->owner_pointer_get(new_id, false);
    if (owner_id_pointer) {
      *owner_id_pointer = const_cast<ID *>(*new_owner_id);
      if (*new_owner_id == nullptr) {
        /* If the new id does not have an owner, it's also not embedded. */
        new_id->flag &= ~ID_FLAG_EMBEDDED_DATA;
      }
    }
  }

  /* We do not want any handling of user-count in code duplicating the data here, we do that all
   * at once in id_copy_libmanagement_cb() at the end. */
  const int copy_data_flag = orig_flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  if (id->properties) {
    new_id->properties = IDP_CopyProperty_ex(id->properties, copy_data_flag);
  }
  if (id->system_properties) {
    new_id->system_properties = IDP_CopyProperty_ex(id->system_properties, copy_data_flag);
  }

  /* This is never duplicated, only one existing ID should have a given weak ref to library/ID. */
  new_id->library_weak_reference = nullptr;

  if ((orig_flag & LIB_ID_COPY_NO_LIB_OVERRIDE) == 0) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(id)) {
      /* We do not want to copy existing override rules here, as they would break the proper
       * remapping between IDs. Proper overrides rules will be re-generated anyway. */
      BKE_lib_override_library_copy(new_id, id, false);
    }
    else if (ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id)) {
      /* Just ensure virtual overrides do get properly tagged, there is not actual override data to
       * copy here. */
      new_id->flag |= ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE;
    }
  }

  if (id_can_have_animdata(new_id)) {
    IdAdtTemplate *iat = (IdAdtTemplate *)new_id;

    /* the duplicate should get a copy of the animdata */
    if ((flag & LIB_ID_COPY_NO_ANIMDATA) == 0) {
      /* Note that even though horrors like root node-trees are not in bmain, the actions they use
       * in their anim data *are* in bmain... super-mega-hooray. */
      BLI_assert((copy_data_flag & LIB_ID_COPY_ACTIONS) == 0 ||
                 (copy_data_flag & LIB_ID_CREATE_NO_MAIN) == 0);
      iat->adt = BKE_animdata_copy_in_lib(bmain, owner_library, iat->adt, copy_data_flag);
    }
    else {
      iat->adt = nullptr;
    }
  }

  if (flag & LIB_ID_COPY_ASSET_METADATA) {
    if (id->asset_data) {
      new_id->asset_data = BKE_asset_metadata_copy(id->asset_data);
    }
  }

  if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0 && (flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    DEG_id_type_tag(bmain, GS(new_id->name));
  }

  if (owner_library && *owner_library && ((*owner_library)->flag & LIBRARY_FLAG_IS_ARCHIVE) != 0) {
    new_id->flag |= ID_FLAG_LINKED_AND_PACKED;
  }

  if (flag & LIB_ID_COPY_ID_NEW_SET) {
    ID_NEW_SET(id, new_id);
  }

  *new_id_p = new_id;
}

void BKE_libblock_copy_ex(Main *bmain, const ID *id, ID **new_id_p, const int orig_flag)
{
  BKE_libblock_copy_in_lib(bmain, std::nullopt, id, std::nullopt, new_id_p, orig_flag);
}

void *BKE_libblock_copy(Main *bmain, const ID *id)
{
  ID *idn = nullptr;

  BKE_libblock_copy_in_lib(bmain, std::nullopt, id, std::nullopt, &idn, 0);

  return idn;
}

/* ***************** ID ************************ */

ID *BKE_libblock_find_name(Main *bmain,
                           const short type,
                           const char *name,
                           const std::optional<Library *> lib)
{
  const ListBase *lb = which_libbase(bmain, type);
  BLI_assert(lb != nullptr);

  ID *id = static_cast<ID *>(BLI_findstring(lb, name, offsetof(ID, name) + 2));
  if (lib) {
    while (id && id->lib != *lib) {
      id = static_cast<ID *>(BLI_listbase_findafter_string(
          reinterpret_cast<Link *>(id), name, offsetof(ID, name) + 2));
    }
  }
  return id;
}

ID *BKE_libblock_find_session_uid(Main *bmain, const short type, const uint32_t session_uid)
{
  const ListBase *lb = which_libbase(bmain, type);
  BLI_assert(lb != nullptr);
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id->session_uid == session_uid) {
      return id;
    }
  }
  return nullptr;
}

ID *BKE_libblock_find_session_uid(Main *bmain, const uint32_t session_uid)
{
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (id_iter->session_uid == session_uid) {
      return id_iter;
    }
  }
  FOREACH_MAIN_ID_END;
  return nullptr;
}

ID *BKE_libblock_find_name_and_library(Main *bmain,
                                       const short type,
                                       const char *name,
                                       const char *lib_name)
{
  const bool is_linked = (lib_name && lib_name[0] != '\0');
  Library *library = is_linked ? reinterpret_cast<Library *>(
                                     BKE_libblock_find_name(bmain, ID_LI, lib_name, nullptr)) :
                                 nullptr;
  if (is_linked && !library) {
    return nullptr;
  }
  return BKE_libblock_find_name(bmain, type, name, library);
}

ID *BKE_libblock_find_name_and_library_filepath(Main *bmain,
                                                short type,
                                                const char *name,
                                                const char *lib_filepath_abs)
{
  const bool is_linked = (lib_filepath_abs && lib_filepath_abs[0] != '\0');
  Library *library = nullptr;
  if (is_linked) {
    const ListBase *lb = which_libbase(bmain, ID_LI);
    LISTBASE_FOREACH (ID *, id_iter, lb) {
      Library *lib_iter = reinterpret_cast<Library *>(id_iter);
      if (STREQ(lib_iter->runtime->filepath_abs, lib_filepath_abs)) {
        library = lib_iter;
        break;
      }
    }
    if (!library) {
      return nullptr;
    }
  }
  return BKE_libblock_find_name(bmain, type, name, library);
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
  if (!ELEM(id_sorting_hint, nullptr, id) && id_sorting_hint->lib == id->lib) {
    BLI_assert(BLI_findindex(lb, id_sorting_hint) >= 0);

    ID *id_sorting_hint_next = static_cast<ID *>(id_sorting_hint->next);
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) < 0 &&
        (id_sorting_hint_next == nullptr || id_sorting_hint_next->lib != id->lib ||
         BLI_strcasecmp(id_sorting_hint_next->name, id->name) > 0))
    {
      BLI_insertlinkafter(lb, id_sorting_hint, id);
      return;
    }

    ID *id_sorting_hint_prev = static_cast<ID *>(id_sorting_hint->prev);
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) > 0 &&
        (id_sorting_hint_prev == nullptr || id_sorting_hint_prev->lib != id->lib ||
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
  for (idtest = static_cast<ID *>(lb->last); idtest != nullptr;
       idtest = static_cast<ID *>(idtest->prev))
  {
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
    idtest = static_cast<ID *>(item_array[item_array_index]);
    if (BLI_strcasecmp(idtest->name, id->name) > 0) {
      BLI_insertlinkbefore(lb, idtest, id);
      break;
    }
  }
  if (item_array_index == ID_SORT_STEP_SIZE) {
    if (idtest == nullptr) {
      /* If idtest is nullptr here, it means that in the first loop, the last comparison was
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

IDNewNameResult BKE_id_new_name_validate(Main &bmain,
                                         ListBase &lb,
                                         ID &id,
                                         const char *newname,
                                         IDNewNameMode mode,
                                         const bool do_linked_data)
{
  char name[MAX_ID_NAME - 2];

  /* If library, don't rename (unless explicitly required), but do ensure proper sorting. */
  if (!do_linked_data && ID_IS_LINKED(&id)) {
    id_sort_by_name(&lb, &id, nullptr);

    return {IDNewNameResult::Action::UNCHANGED, nullptr};
  }

  /* If no name given, use name of current ID. */
  if (newname == nullptr) {
    newname = BKE_id_name(id);
  }
  /* Make a copy of given name (newname args can be const). */
  STRNCPY(name, newname);

  if (name[0] == '\0') {
    /* Disallow empty names. */
    STRNCPY_UTF8(name, DATA_(BKE_idtype_idcode_to_name(GS(id.name))));
  }
  else {
    /* Disallow non UTF8 chars,
     * the interface checks for this but new ID's based on file names don't. */
    BLI_str_utf8_invalid_strip(name, strlen(name));
  }

  /* Store original requested new name, in modes that may solve name conflict by renaming the
   * existing conflicting ID. */
  char orig_name[MAX_ID_NAME - 2];
  if (ELEM(mode, IDNewNameMode::RenameExistingAlways, IDNewNameMode::RenameExistingSameRoot)) {
    STRNCPY(orig_name, name);
  }

  const bool had_name_collision = BKE_main_namemap_get_unique_name(bmain, id, name);

  if (had_name_collision &&
      ELEM(mode, IDNewNameMode::RenameExistingAlways, IDNewNameMode::RenameExistingSameRoot))
  {
    char prev_name[MAX_ID_NAME - 2];
    char prev_name_root[MAX_ID_NAME - 2];
    int prev_number = 0;
    char new_name_root[MAX_ID_NAME - 2];
    int new_number = 0;
    STRNCPY(prev_name, BKE_id_name(id));
    if (mode == IDNewNameMode::RenameExistingSameRoot) {
      BLI_string_split_name_number(BKE_id_name(id), '.', prev_name_root, &prev_number);
      BLI_string_split_name_number(name, '.', new_name_root, &new_number);
    }

    ID *id_other = BKE_libblock_find_name(&bmain, GS(id.name), orig_name, id.lib);
    BLI_assert(id_other);

    /* In case of #RenameExistingSameRoot, the existing ID (`id_other`) is only renamed if it has
     * the same 'root' name as the current name of the renamed `id`. */
    if (mode == IDNewNameMode::RenameExistingAlways ||
        (mode == IDNewNameMode::RenameExistingSameRoot && STREQ(prev_name_root, new_name_root)))
    {
      BLI_strncpy(id_other->name + 2, name, sizeof(id_other->name) - 2);
      id_sort_by_name(&lb, id_other, nullptr);

      const bool is_idname_changed = !STREQ(BKE_id_name(id), orig_name);
      IDNewNameResult result = {IDNewNameResult::Action::UNCHANGED_COLLISION, id_other};
      if (is_idname_changed) {
        BLI_strncpy(id.name + 2, orig_name, sizeof(id.name) - 2);
        result.action = IDNewNameResult::Action::RENAMED_COLLISION_FORCED;
      }
      id_sort_by_name(&lb, &id, nullptr);

      return result;
    }
  }

  /* The requested new name may be available (not collide with any other existing ID name), but
   * still differ from the current name of the renamed ID.
   * Conversely, the requested new name may have been colliding with an existing one, and the
   * generated unique name may end up being the current ID's name. */
  const bool is_idname_changed = !STREQ(BKE_id_name(id), name);

  IDNewNameResult result = {IDNewNameResult::Action::UNCHANGED, nullptr};
  if (is_idname_changed) {
    BLI_strncpy(id.name + 2, name, sizeof(id.name) - 2);
    result.action = had_name_collision ? IDNewNameResult::Action::RENAMED_COLLISION_ADJUSTED :
                                         IDNewNameResult::Action::RENAMED_NO_COLLISION;
  }
  else if (had_name_collision) {
    result.action = IDNewNameResult::Action::UNCHANGED_COLLISION;
  }
  id_sort_by_name(&lb, &id, nullptr);
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
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;
  const bool do_linked_only = bool(POINTER_AS_INT(cb_data->user_data));

  if (*id_pointer == nullptr) {
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

void BKE_main_id_refcount_recompute(Main *bmain, const bool do_linked_only)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    if (!ID_IS_LINKED(id) && do_linked_only) {
      continue;
    }
    id->us = ID_FAKE_USERS(id);
    /* Note that we keep EXTRAUSER tag here, since some UI users may define it too... */
    if (id->tag & ID_TAG_EXTRAUSER) {
      id->tag &= ~(ID_TAG_EXTRAUSER | ID_TAG_EXTRAUSER_SET);
      id_us_ensure_real(id);
    }
    if (ELEM(GS(id->name), ID_SCE, ID_WM, ID_WS)) {
      /* These IDs should always have a 'virtual' user. */
      id_us_ensure_real(id);
    }
  }
  FOREACH_MAIN_ID_END;

  /* Go over whole Main database to re-generate proper user-counts. */
  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    BKE_library_foreach_ID_link(bmain,
                                id,
                                id_refcount_recompute_callback,
                                POINTER_FROM_INT(int(do_linked_only)),
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

  MainIDRelationsEntry *entry = static_cast<MainIDRelationsEntry *>(
      BLI_ghash_lookup(id_relations->relations_from_pointers, id));
  BLI_gset_insert(loop_tags, id);
  for (MainIDRelationsEntryItem *from_id_entry = entry->from_ids; from_id_entry != nullptr;
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

    if (from_id->tag & ID_TAG_DOIT) {
      /* This user will be fully local in future, so far so good,
       * nothing to do here but check next user. */
    }
    else {
      /* This user won't be fully local in future, so current ID won't be either.
       * And we are done checking it. */
      id->tag &= ~ID_TAG_DOIT;
      break;
    }
  }
  BLI_gset_add(done_ids, id);
  BLI_gset_remove(loop_tags, id, nullptr);
}

void BKE_library_make_local(Main *bmain,
                            const Library *lib,
                            GHash *old_to_new_ids,
                            const bool untagged_only,
                            const bool set_fake,
                            const bool clear_asset_data)
{
  /* NOTE: Old (2.77) version was simply making (tagging) data-blocks as local,
   * without actually making any check whether they were also indirectly used or not...
   *
   * Current version uses regular id_make_local callback, with advanced pre-processing step to
   * detect all cases of IDs currently indirectly used, but which will be used by local data only
   * once this function is finished.  This allows to avoid any unneeded duplication of IDs, and
   * hence all time lost afterwards to remove orphaned linked data-blocks. */

  MainListsArray lbarray = BKE_main_lists_get(*bmain);

  LinkNode *todo_ids = nullptr;
  LinkNode *copied_ids = nullptr;
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
  for (int a = lbarray.size(); a--;) {
    ID *id = static_cast<ID *>(lbarray[a]->first);

    /* Do not explicitly make local non-linkable IDs (shape-keys, in fact),
     * they are assumed to be handled by real data-blocks responsible of them. */
    const bool do_skip = (id && !BKE_idtype_idcode_is_linkable(GS(id->name)));

    for (; id; id = static_cast<ID *>(id->next)) {
      ID *ntree = (ID *)blender::bke::node_tree_from_id(id);

      id->tag &= ~ID_TAG_DOIT;
      if (ntree != nullptr) {
        ntree->tag &= ~ID_TAG_DOIT;
      }

      if (!ID_IS_LINKED(id)) {
        id->tag &= ~(ID_TAG_EXTERN | ID_TAG_INDIRECT | ID_TAG_NEW);
        id->flag &= ~ID_FLAG_INDIRECT_WEAK_LINK;
        if (ID_IS_OVERRIDE_LIBRARY_REAL(id) &&
            ELEM(lib, nullptr, id->override_library->reference->lib) &&
            ((untagged_only == false) || !(id->tag & ID_TAG_PRE_EXISTING)))
        {
          /* Validating liboverride hierarchy root pointers will happen later in this function,
           * rather than doing it for each and every localized ID. */
          BKE_lib_override_library_make_local(nullptr, id);
        }
      }
      /* The check on the fourth line (ID_TAG_PRE_EXISTING) is done so it's possible to tag data
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
      else if (!do_skip && id->tag & (ID_TAG_EXTERN | ID_TAG_INDIRECT | ID_TAG_NEW) &&
               ELEM(lib, nullptr, id->lib) &&
               ((untagged_only == false) || !(id->tag & ID_TAG_PRE_EXISTING)))
      {
        BLI_linklist_prepend_arena(&todo_ids, id, linklist_mem);
        id->tag |= ID_TAG_DOIT;

        /* Tag those nasty non-ID node-trees,
         * but do not add them to todo list, making them local is handled by 'owner' ID.
         * This is needed for library_make_local_copying_check() to work OK at step 2. */
        if (ntree != nullptr) {
          ntree->tag |= ID_TAG_DOIT;
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
    library_make_local_copying_check(
        static_cast<ID *>(it->link), loop_tags, bmain->relations, done_ids);
    BLI_assert(BLI_gset_len(loop_tags) == 0);
  }
  BLI_gset_free(loop_tags, nullptr);
  BLI_gset_free(done_ids, nullptr);

  /* Next step will most likely add new IDs, better to get rid of this mapping now. */
  BKE_main_relations_free(bmain);

#ifdef DEBUG_TIME
  printf("Step 2: Check which data-blocks we can directly make local: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  const int make_local_flags = clear_asset_data ? LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR : 0;

  /* Step 3: Make IDs local, either directly (quick and simple), or using generic process,
   * which involves more complex checks and might instead
   * create a local copy of original linked ID. */
  for (LinkNode *it = todo_ids, *it_next; it; it = it_next) {
    it_next = it->next;
    ID *id = static_cast<ID *>(it->link);

    if (id->tag & ID_TAG_DOIT) {
      /* We know all users of this object are local or will be made fully local, even if
       * currently there are some indirect usages. So instead of making a copy that we'll likely
       * get rid of later, directly make that data block local.
       * Saves a tremendous amount of time with complex scenes... */
      BKE_lib_id_clear_library_data(bmain, id, make_local_flags);
      BKE_lib_id_expand_local(bmain, id, 0);
      id->tag &= ~ID_TAG_DOIT;

      if (GS(id->name) == ID_OB) {
        BKE_rigidbody_ensure_local_object(bmain, (Object *)id);
      }
    }
    else {
      /* In this specific case, we do want to make ID local even if it has no local usage yet... */
      BKE_lib_id_make_local(bmain, id, make_local_flags | LIB_ID_MAKELOCAL_FULL_LIBRARY);

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
  todo_ids = nullptr;

  /* Step 4: We have to remap local usages of old (linked) ID to new (local)
   * ID in a separated loop,
   * as lbarray ordering is not enough to ensure us we did catch all dependencies
   * (e.g. if making local a parent object before its child...). See #48907. */
  /* TODO: This is now the biggest step by far (in term of processing time).
   * We may be able to gain here by using again main->relations mapping, but...
   * this implies BKE_libblock_remap & co to be able to update main->relations on the fly.
   * Have to think about it a bit more, and see whether new code is OK first, anyway. */
  for (LinkNode *it = copied_ids; it; it = it->next) {
    ID *id = static_cast<ID *>(it->link);

    BLI_assert(id->newid != nullptr);
    BLI_assert(ID_IS_LINKED(id));

    BKE_libblock_remap(bmain, id, id->newid, ID_REMAP_SKIP_INDIRECT_USAGE);
    if (old_to_new_ids) {
      BLI_ghash_insert(old_to_new_ids, id, id->newid);
    }

    /* Special hack for groups... Thing is, since we can't instantiate them here, we need to
     * ensure they remain 'alive' (only instantiation is a real group 'user'... *sigh* See
     * #49722. */
    if (GS(id->name) == ID_GR && (id->tag & ID_TAG_INDIRECT) != 0) {
      id_us_ensure_real(id->newid);
    }
  }

  /* Making some liboverride local may have had some impact on validity of liboverrides hierarchy
   * roots, these need to be re-validated/re-generated. */
  BKE_lib_override_library_main_hierarchy_root_ensure(bmain);

#ifdef DEBUG_TIME
  printf("Step 4: Remap local usages of old (linked) ID to new (local) ID: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* This is probably more of a hack than something we should do here, but...
   * Issue is, the whole copying + remapping done in complex cases above may leave pose-channels
   * of armatures in complete invalid state (more precisely, the bone pointers of the
   * pose-channels - very crappy cross-data-blocks relationship), so we tag it to be fully
   * recomputed, but this does not seems to be enough in some cases, and evaluation code ends up
   * trying to evaluate a not-yet-updated armature object's deformations.
   * Try "make all local" in 04_01_H.lighting.blend from Agent327 without this, e.g. */
  for (Object *ob = static_cast<Object *>(bmain->objects.first); ob;
       ob = static_cast<Object *>(ob->id.next))
  {
    if (ob->data != nullptr && ob->type == OB_ARMATURE && ob->pose != nullptr &&
        ob->pose->flag & POSE_RECALC)
    {
      BKE_pose_rebuild(bmain, ob, static_cast<bArmature *>(ob->data), true);
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

IDNewNameResult BKE_libblock_rename(Main &bmain,
                                    ID &id,
                                    blender::StringRefNull name,
                                    const IDNewNameMode mode)
{
  BLI_assert(BKE_id_is_in_main(&bmain, &id));

  if (STREQ(BKE_id_name(id), name.c_str())) {
    return {IDNewNameResult::Action::UNCHANGED, nullptr};
  }
  BKE_main_namemap_remove_id(bmain, id);
  ListBase &lb = *which_libbase(&bmain, GS(id.name));
  IDNewNameResult result = BKE_id_new_name_validate(bmain, lb, id, name.c_str(), mode, true);
  if (!ELEM(result.action,
            IDNewNameResult::Action::UNCHANGED,
            IDNewNameResult::Action::UNCHANGED_COLLISION))
  {
    bmain.is_memfile_undo_written = false;
  }
  return result;
}

IDNewNameResult BKE_id_rename(Main &bmain,
                              ID &id,
                              blender::StringRefNull name,
                              const IDNewNameMode mode)
{
  const IDNewNameResult result = BKE_libblock_rename(bmain, id, name, mode);

  auto deg_tag_id = [](ID &id) -> void {
    DEG_id_tag_update(&id, ID_RECALC_SYNC_TO_EVAL);
    switch (GS(id.name)) {
      case ID_OB: {
        Object &ob = reinterpret_cast<Object &>(id);
        if (ob.type == OB_MBALL) {
          DEG_id_tag_update(&ob.id, ID_RECALC_GEOMETRY);
        }
        break;
      }
      default:
        break;
    }
  };

  switch (result.action) {
    case IDNewNameResult::Action::UNCHANGED:
    case IDNewNameResult::Action::UNCHANGED_COLLISION:
      break;
    case IDNewNameResult::Action::RENAMED_NO_COLLISION:
    case IDNewNameResult::Action::RENAMED_COLLISION_ADJUSTED:
      deg_tag_id(id);
      break;
    case IDNewNameResult::Action::RENAMED_COLLISION_FORCED:
      BLI_assert(result.other_id);
      deg_tag_id(*result.other_id);
      deg_tag_id(id);
      break;
  }

  return result;
}

void BKE_id_full_name_get(char name[MAX_ID_FULL_NAME], const ID *id, char separator_char)
{
  BLI_strncpy(name, BKE_id_name(*id), MAX_ID_FULL_NAME);

  if (ID_IS_LINKED(id)) {
    const size_t idname_len = strlen(BKE_id_name(*id));
    const size_t libname_len = strlen(BKE_id_name(id->lib->id));

    name[idname_len] = separator_char ? separator_char : ' ';
    name[idname_len + 1] = '[';
    BLI_strncpy(
        name + idname_len + 2, BKE_id_name(id->lib->id), MAX_ID_FULL_NAME - (idname_len + 2));
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
  name[i++] = (id->flag & ID_FLAG_FAKEUSER) ? 'F' : ((id->us == 0) ? '0' : ' ');
  name[i++] = ' ';

  BKE_id_full_name_get(name + i, id, separator_char);

  if (r_prefix_len) {
    *r_prefix_len = i;
  }
}

char *BKE_id_to_unique_string_key(const ID *id)
{
  if (!ID_IS_LINKED(id)) {
    return BLI_strdup(id->name);
  }

  /* Prefix with an ASCII character in the range of 32..96 (visible)
   * this ensures we can't have a library ID pair that collide.
   * Where 'LIfooOBbarOBbaz' could be ('LIfoo, OBbarOBbaz') or ('LIfooOBbar', 'OBbaz'). */
  const char ascii_len = strlen(BKE_id_name(id->lib->id)) + 32;
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

bool BKE_id_is_in_main(Main *bmain, ID *id)
{
  /* We do not want to fail when id is nullptr here, even though this is a bit strange behavior...
   */
  return (id == nullptr || BLI_findindex(which_libbase(bmain, GS(id->name)), id) != -1);
}

bool BKE_id_is_in_global_main(ID *id)
{
  return BKE_id_is_in_main(G_MAIN, id);
}

bool BKE_id_can_be_asset(const ID *id)
{
  return ID_IS_EDITABLE(id) && !ID_IS_OVERRIDE_LIBRARY(id) &&
         BKE_idtype_idcode_is_linkable(GS(id->name));
}

ID *BKE_id_owner_get(ID *id, const bool debug_relationship_assert)
{
  const IDTypeInfo *idtype = BKE_idtype_get_info_from_id(id);
  if (idtype->owner_pointer_get != nullptr) {
    ID **owner_id_pointer = idtype->owner_pointer_get(id, debug_relationship_assert);
    if (owner_id_pointer != nullptr) {
      return *owner_id_pointer;
    }
  }
  return nullptr;
}

bool BKE_id_is_editable(const Main *bmain, const ID *id)
{
  return ID_IS_EDITABLE(id) && !BKE_lib_override_library_is_system_defined(bmain, id);
}

bool BKE_id_can_use_id(const ID &id_from, const ID &id_to)
{
  /* Can't point from linked to local. */
  if (id_from.lib && !id_to.lib) {
    return false;
  }
  /* Can't point from ID in main database to one outside of it. */
  if (!(id_from.tag & ID_TAG_NO_MAIN) && (id_to.tag & ID_TAG_NO_MAIN)) {
    return false;
  }

  return true;
}

/************************* Datablock order in UI **************************/

static int *id_order_get(ID *id)
{
  /* Only for workspace tabs currently. */
  switch (GS(id->name)) {
    case ID_WS:
      return &((WorkSpace *)id)->order;
    default:
      return nullptr;
  }
}

static bool id_order_compare(ID *a, ID *b)
{
  int *order_a = id_order_get(a);
  int *order_b = id_order_get(b);

  /* In practice either both or neither are set,
   * failing to do this would result in a logically invalid sort function, see #137712. */
  BLI_assert((order_a && order_b) || (!order_a && !order_b));

  if (order_a && order_b) {
    if (*order_a < *order_b) {
      return true;
    }
    if (*order_a > *order_b) {
      return false;
    }
  }

  return strcmp(a->name, b->name) < 0;
}

Vector<ID *> BKE_id_ordered_list(const ListBase *lb)
{
  Vector<ID *> ordered;

  LISTBASE_FOREACH (ID *, id, lb) {
    ordered.append(id);
  }

  std::sort(ordered.begin(), ordered.end(), id_order_compare);

  for (const int i : ordered.index_range()) {
    if (int *order = id_order_get(ordered[i])) {
      *order = i;
    }
  }

  return ordered;
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

  if (id->library_weak_reference != nullptr) {
    BLO_write_struct(writer, LibraryWeakReference, id->library_weak_reference);
  }

  /* ID_WM's id->properties are considered runtime only, and never written in .blend file. */
  if (id->properties && !ELEM(GS(id->name), ID_WM)) {
    IDP_BlendWrite(writer, id->properties);
  }
  /* ID_WM's id->system_properties are considered runtime only, and never written in .blend file.
   */
  if (id->system_properties && !ELEM(GS(id->name), ID_WM)) {
    IDP_BlendWrite(writer, id->system_properties);
  }

  BKE_animdata_blend_write(writer, id);

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

struct SomeTypeWithIDMember {
  int id;
};

static_assert(blender::dna::is_ID_v<ID>);
static_assert(blender::dna::is_ID_v<Object>);
static_assert(!blender::dna::is_ID_v<int>);
static_assert(!blender::dna::is_ID_v<ID *>);
static_assert(!blender::dna::is_ID_v<const ID>);
static_assert(!blender::dna::is_ID_v<ListBase>);
static_assert(!blender::dna::is_ID_v<SomeTypeWithIDMember>);
