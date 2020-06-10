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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include <assert.h>
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
#include "DNA_gpencil_types.h"
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
#include "BKE_bpath.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_rigidbody.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"

#include "atomic_ops.h"

//#define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

static CLG_LogRef LOG = {.identifier = "bke.lib_id"};

/* Empty shell mostly, but needed for read code. */
IDTypeInfo IDType_ID_LINK_PLACEHOLDER = {
    .id_code = ID_LINK_PLACEHOLDER,
    .id_filter = 0,
    .main_listbase_index = INDEX_ID_NULL,
    .struct_size = sizeof(ID),
    .name = "LinkPlaceholder",
    .name_plural = "link_placeholders",
    .translation_context = BLT_I18NCONTEXT_ID_ID,
    .flags = IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_LIBLINKING | IDTYPE_FLAGS_NO_MAKELOCAL,

    .init_data = NULL,
    .copy_data = NULL,
    .free_data = NULL,
    .make_local = NULL,
};

/* GS reads the memory pointed at in a specific ordering.
 * only use this definition, makes little and big endian systems
 * work fine, in conjunction with MAKE_ID */

/* ************* general ************************ */

/**
 * This has to be called from each make_local_* func, we could call from BKE_lib_id_make_local()
 * but then the make local functions would not be self contained.
 * Also note that the id _must_ have a library - campbell */
static void lib_id_library_local_paths(Main *bmain, Library *lib, ID *id)
{
  const char *bpath_user_data[2] = {BKE_main_blendfile_path(bmain), lib->filepath};

  BKE_bpath_traverse_id(bmain,
                        id,
                        BKE_bpath_relocate_visitor,
                        BKE_BPATH_TRAVERSE_SKIP_MULTIFILE,
                        (void *)bpath_user_data);
}

/**
 * Pull an ID out of a library (make it local). Only call this for IDs that
 * don't have other library users.
 */
static void lib_id_clear_library_data_ex(Main *bmain, ID *id)
{
  const bool id_in_mainlist = (id->tag & LIB_TAG_NO_MAIN) == 0 &&
                              (id->flag & LIB_EMBEDDED_DATA) == 0;

  lib_id_library_local_paths(bmain, id->lib, id);

  id_fake_user_clear(id);

  id->lib = NULL;
  id->tag &= ~(LIB_TAG_INDIRECT | LIB_TAG_EXTERN);
  id->flag &= ~LIB_INDIRECT_WEAK_LINK;
  if (id_in_mainlist) {
    if (BKE_id_new_name_validate(which_libbase(bmain, GS(id->name)), id, NULL)) {
      bmain->is_memfile_undo_written = false;
    }
  }

  /* Internal shape key blocks inside data-blocks also stores id->lib,
   * make sure this stays in sync (note that we do not need any explicit handling for real EMBEDDED
   * IDs here, this is down automatically in `lib_id_expand_local_cb()`. */
  Key *key = BKE_key_from_id(id);
  if (key != NULL) {
    lib_id_clear_library_data_ex(bmain, &key->id);
  }
}

void BKE_lib_id_clear_library_data(Main *bmain, ID *id)
{
  lib_id_clear_library_data_ex(bmain, id);
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

/**
 * Ensure we have a real user
 *
 * \note Now that we have flags, we could get rid of the 'fake_user' special case,
 * flags are enough to ensure we always have a real user.
 * However, #ID_REAL_USERS is used in several places outside of core lib.c,
 * so think we can wait later to make this change.
 */
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
                   id->lib ? id->lib->filepath : "[Main]");
        BLI_assert(0);
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

/**
 * Same as \a id_us_plus, but does not handle lib indirect -> extern.
 * Only used by readfile.c so far, but simpler/safer to keep it here nonetheless.
 */
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

/* decrements the user count for *id. */
void id_us_min(ID *id)
{
  if (id) {
    const int limit = ID_FAKE_USERS(id);

    if (id->us <= limit) {
      CLOG_ERROR(&LOG,
                 "ID user decrement error: %s (from '%s'): %d <= %d",
                 id->name,
                 id->lib ? id->lib->filepath : "[Main]",
                 id->us,
                 limit);
      if (GS(id->name) != ID_IP) {
        /* Do not assert on deprecated ID types, we cannot really ensure that their ID refcounting
         * is valid... */
        BLI_assert(0);
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

void BKE_id_clear_newpoin(ID *id)
{
  if (id->newid) {
    id->newid->tag &= ~LIB_TAG_NEW;
  }
  id->newid = NULL;
}

static int lib_id_expand_local_cb(LibraryIDLinkCallbackData *cb_data)
{
  Main *bmain = cb_data->bmain;
  ID *id_self = cb_data->id_self;
  ID **id_pointer = cb_data->id_pointer;
  int const cb_flag = cb_data->cb_flag;

  if (cb_flag & IDWALK_CB_LOOPBACK) {
    /* We should never have anything to do with loopback pointers here. */
    return IDWALK_RET_NOP;
  }

  if (cb_flag & IDWALK_CB_EMBEDDED) {
    /* Embedded data-blocks need to be made fully local as well. */
    if (*id_pointer != NULL) {
      BLI_assert(*id_pointer != id_self);

      lib_id_clear_library_data_ex(bmain, *id_pointer);
    }
    return IDWALK_RET_NOP;
  }

  /* Can happen that we get un-linkable ID here, e.g. with shape-key referring to itself
   * (through drivers)...
   * Just skip it, shape key can only be either indirectly linked, or fully local, period.
   * And let's curse one more time that stupid useless shapekey ID type! */
  if (*id_pointer && *id_pointer != id_self &&
      BKE_idtype_idcode_is_linkable(GS((*id_pointer)->name))) {
    id_lib_extern(*id_pointer);
  }

  return IDWALK_RET_NOP;
}

/**
 * Expand ID usages of given id as 'extern' (and no more indirect) linked data.
 * Used by ID copy/make_local functions.
 */
void BKE_lib_id_expand_local(Main *bmain, ID *id)
{
  BKE_library_foreach_ID_link(bmain, id, lib_id_expand_local_cb, bmain, IDWALK_READONLY);
}

/**
 * Ensure new (copied) ID is fully made local.
 */
static void lib_id_copy_ensure_local(Main *bmain, const ID *old_id, ID *new_id)
{
  if (ID_IS_LINKED(old_id)) {
    BKE_lib_id_expand_local(bmain, new_id);
    lib_id_library_local_paths(bmain, old_id->lib, new_id);
  }
}

/**
 * Generic 'make local' function, works for most of data-block types...
 */
void BKE_lib_id_make_local_generic(Main *bmain, ID *id, const int flags)
{
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;
  bool is_local = false, is_lib = false;

  /* - only lib users: do nothing (unless force_local is set)
   * - only local users: set flag
   * - mixed: make copy
   * In case we make a whole lib's content local,
   * we always want to localize, and we skip remapping (done later).
   */

  if (!ID_IS_LINKED(id)) {
    return;
  }

  BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

  if (lib_local || is_local) {
    if (!is_lib) {
      lib_id_clear_library_data_ex(bmain, id);
      BKE_lib_id_expand_local(bmain, id);
    }
    else {
      ID *id_new;

      /* Should not fail in expected use cases,
       * but a few ID types cannot be copied (LIB, WM, SCR...). */
      if (BKE_id_copy(bmain, id, &id_new)) {
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

        if (!lib_local) {
          BKE_libblock_remap(bmain, id, id_new, ID_REMAP_SKIP_INDIRECT_USAGE);
        }
      }
    }
  }
}

/**
 * Calls the appropriate make_local method for the block, unless test is set.
 *
 * \note Always set ID->newid pointer in case it gets duplicated...
 *
 * \param lib_local: Special flag used when making a whole library's content local,
 * it needs specific handling.
 *
 * \return true if the block can be made local.
 */
bool BKE_lib_id_make_local(Main *bmain, ID *id, const bool test, const int flags)
{
  const bool lib_local = (flags & LIB_ID_MAKELOCAL_FULL_LIBRARY) != 0;

  /* We don't care whether ID is directly or indirectly linked
   * in case we are making a whole lib local... */
  if (!lib_local && (id->tag & LIB_TAG_INDIRECT)) {
    return false;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if ((idtype_info->flags & IDTYPE_FLAGS_NO_MAKELOCAL) == 0) {
      if (!test) {
        if (idtype_info->make_local != NULL) {
          idtype_info->make_local(bmain, id, flags);
        }
        else {
          BKE_lib_id_make_local_generic(bmain, id, flags);
        }
      }
      return true;
    }
    return false;
  }

  BLI_assert(!"IDType Missing IDTypeInfo");
  return false;
}

struct IDCopyLibManagementData {
  const ID *id_src;
  ID *id_dst;
  int flag;
};

/* Increases usercount as required, and remap self ID pointers. */
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
    id_us_plus(id);
  }

  return IDWALK_RET_NOP;
}

bool BKE_id_copy_is_allowed(const ID *id)
{
#define LIB_ID_TYPES_NOCOPY \
  ID_LI, ID_SCR, ID_WM, ID_WS, /* Not supported */ \
      ID_IP                    /* Deprecated */

  return !ELEM(GS(id->name), LIB_ID_TYPES_NOCOPY);

#undef LIB_ID_TYPES_NOCOPY
}

/**
 * Generic entry point for copying a data-block (new API).
 *
 * \note Copy is only affecting given data-block
 * (no ID used by copied one will be affected, besides usercount).
 * There is only one exception, if #LIB_ID_COPY_ACTIONS is defined,
 * actions used by animdata will be duplicated.
 *
 * \note Usercount of new copy is always set to 1.
 *
 * \param bmain: Main database, may be NULL only if LIB_ID_CREATE_NO_MAIN is specified.
 * \param id: Source data-block.
 * \param r_newid: Pointer to new (copied) ID pointer.
 * \param flag: Set of copy options, see DNA_ID.h enum for details
 * (leave to zero for default, full copy).
 * \return False when copying that ID type is not supported, true otherwise.
 */
bool BKE_id_copy_ex(Main *bmain, const ID *id, ID **r_newid, const int flag)
{
  BLI_assert(r_newid != NULL);
  /* Make sure destination pointer is all good. */
  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0) {
    *r_newid = NULL;
  }
  else {
    if (*r_newid != NULL) {
      /* Allow some garbage non-initialized memory to go in, and clean it up here. */
      const size_t size = BKE_libblock_get_alloc_info(GS(id->name), NULL);
      memset(*r_newid, 0, size);
    }
  }

  /* Early output is source is NULL. */
  if (id == NULL) {
    return false;
  }

  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if ((idtype_info->flags & IDTYPE_FLAGS_NO_COPY) != 0) {
      return false;
    }

    BKE_libblock_copy_ex(bmain, id, r_newid, flag);

    if (idtype_info->copy_data != NULL) {
      idtype_info->copy_data(bmain, *r_newid, id, flag);
    }
  }
  else {
    BLI_assert(!"IDType Missing IDTypeInfo");
  }

  /* Update ID refcount, remap pointers to self in new ID. */
  struct IDCopyLibManagementData data = {
      .id_src = id,
      .id_dst = *r_newid,
      .flag = flag,
  };
  BKE_library_foreach_ID_link(bmain, *r_newid, id_copy_libmanagement_cb, &data, IDWALK_NOP);

  /* Do not make new copy local in case we are copying outside of main...
   * XXX TODO: is this behavior OK, or should we need own flag to control that? */
  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    BLI_assert((flag & LIB_ID_COPY_KEEP_LIB) == 0);
    lib_id_copy_ensure_local(bmain, id, *r_newid);
  }
  else {
    (*r_newid)->lib = id->lib;
  }

  return true;
}

/**
 * Invokes the appropriate copy method for the block and returns the result in
 * newid, unless test. Returns true if the block can be copied.
 */
bool BKE_id_copy(Main *bmain, const ID *id, ID **newid)
{
  return BKE_id_copy_ex(bmain, id, newid, LIB_ID_COPY_DEFAULT);
}

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 */
static void id_swap(Main *bmain, ID *id_a, ID *id_b, const bool do_full_id)
{
  BLI_assert(GS(id_a->name) == GS(id_b->name));

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
  }

  if (bmain != NULL) {
    /* Swap will have broken internal references to itself, restore them. */
    BKE_libblock_relink_ex(bmain, id_a, id_b, id_a, ID_REMAP_SKIP_NEVER_NULL_USAGE);
    BKE_libblock_relink_ex(bmain, id_b, id_a, id_b, ID_REMAP_SKIP_NEVER_NULL_USAGE);
  }
}

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 *
 * \param bmain: May be NULL, in which case there will be no remapping of internal pointers to
 * itself.
 */
void BKE_lib_id_swap(Main *bmain, ID *id_a, ID *id_b)
{
  id_swap(bmain, id_a, id_b, false);
}

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note All internal ID data itself is also swapped.
 *
 * \param bmain: May be NULL, in which case there will be no remapping of internal pointers to
 * itself.
 */
void BKE_lib_id_swap_full(Main *bmain, ID *id_a, ID *id_b)
{
  id_swap(bmain, id_a, id_b, true);
}

/** Does *not* set ID->newid pointer. */
bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop)
{
  ID *newid = NULL;
  PointerRNA idptr;

  if (id) {
    /* If property isn't editable,
     * we're going to have an extra block hanging around until we save. */
    if (RNA_property_editable(ptr, prop)) {
      Main *bmain = CTX_data_main(C);
      /* copy animation actions too */
      if (BKE_id_copy_ex(bmain, id, &newid, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS) && newid) {
        /* us is 1 by convention with new IDs, but RNA_property_pointer_set
         * will also increment it, decrement it here. */
        id_us_min(newid);

        /* assign copy */
        RNA_id_pointer_create(newid, &idptr);
        RNA_property_pointer_set(ptr, prop, idptr, NULL);
        RNA_property_update(C, ptr, prop);

        /* tag grease pencil data-block and disable onion */
        if (GS(id->name) == ID_GD) {
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

/** Add a 'NO_MAIN' data-block to given main (also sets usercounts of its IDs if needed). */
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
  BKE_id_new_name_validate(lb, id, NULL);
  /* alphabetic insertion: is in new_id */
  id->tag &= ~(LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT);
  bmain->is_memfile_undo_written = false;
  BKE_main_unlock(bmain);

  BKE_lib_libblock_session_uuid_ensure(id);
}

/** Remove a data-block from given main (set it to 'NO_MAIN' status). */
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

/**
 * Clear or set given tags for all ids in listbase (runtime tags).
 */
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

/**
 * Clear or set given tags for all ids of given type in bmain (runtime tags).
 */
void BKE_main_id_tag_idcode(struct Main *mainvar,
                            const short type,
                            const int tag,
                            const bool value)
{
  ListBase *lb = which_libbase(mainvar, type);

  BKE_main_id_tag_listbase(lb, tag, value);
}

/**
 * Clear or set given tags for all ids in bmain (runtime tags).
 */
void BKE_main_id_tag_all(struct Main *mainvar, const int tag, const bool value)
{
  ListBase *lbarray[MAX_LIBARRAY];
  int a;

  a = set_listbasepointers(mainvar, lbarray);
  while (a--) {
    BKE_main_id_tag_listbase(lbarray[a], tag, value);
  }
}

/**
 * Clear or set given flags for all ids in listbase (persistent flags).
 */
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

/**
 * Clear or set given flags for all ids in bmain (persistent flags).
 */
void BKE_main_id_flag_all(Main *bmain, const int flag, const bool value)
{
  ListBase *lbarray[MAX_LIBARRAY];
  int a;
  a = set_listbasepointers(bmain, lbarray);
  while (a--) {
    BKE_main_id_flag_listbase(lbarray[a], flag, value);
  }
}

void BKE_main_id_repair_duplicate_names_listbase(ListBase *lb)
{
  int lb_len = 0;
  LISTBASE_FOREACH (ID *, id, lb) {
    if (id->lib == NULL) {
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
    if (id->lib == NULL) {
      id_array[i] = id;
      i++;
    }
  }
  for (i = 0; i < lb_len; i++) {
    if (!BLI_gset_add(gset, id_array[i]->name + 2)) {
      BKE_id_new_name_validate(lb, id_array[i], NULL);
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

/**
 * Get allocation size of a given data-block type and optionally allocation name.
 */
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

/**
 * Allocates and returns memory of the right size for the specified block type,
 * initialized to zero.
 */
void *BKE_libblock_alloc_notest(short type)
{
  const char *name;
  size_t size = BKE_libblock_get_alloc_info(type, &name);
  if (size != 0) {
    return MEM_callocN(size, name);
  }
  BLI_assert(!"Request to allocate unknown data type");
  return NULL;
}

/**
 * Allocates and returns a block of the specified type, with the specified name
 * (adjusted as necessary to ensure uniqueness), and appended to the specified list.
 * The user count is set to 1, all other content (apart from name and links) being
 * initialized to zero.
 */
void *BKE_libblock_alloc(Main *bmain, short type, const char *name, const int flag)
{
  BLI_assert((flag & LIB_ID_CREATE_NO_ALLOCATE) == 0);

  ID *id = BKE_libblock_alloc_notest(type);

  if (id) {
    if ((flag & LIB_ID_CREATE_NO_MAIN) != 0) {
      id->tag |= LIB_TAG_NO_MAIN;
    }
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0) {
      id->tag |= LIB_TAG_NO_USER_REFCOUNT;
    }

    id->icon_id = 0;
    *((short *)id->name) = type;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id->us = 1;
    }
    if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
      /* Note that 2.8x versioning has tested not to cause conflicts. */
      BLI_assert(bmain->is_locked_for_linking == false || ELEM(type, ID_WS, ID_GR));
      ListBase *lb = which_libbase(bmain, type);

      BKE_main_lock(bmain);
      BLI_addtail(lb, id);
      BKE_id_new_name_validate(lb, id, name);
      bmain->is_memfile_undo_written = false;
      /* alphabetic insertion: is in new_id */
      BKE_main_unlock(bmain);

      BKE_lib_libblock_session_uuid_ensure(id);

      /* TODO to be removed from here! */
      if ((flag & LIB_ID_CREATE_NO_DEG_TAG) == 0) {
        DEG_id_type_tag(bmain, type);
      }
    }
    else {
      BLI_strncpy(id->name + 2, name, sizeof(id->name) - 2);
    }
  }

  return id;
}

/**
 * Initialize an ID of given type, such that it has valid 'empty' data.
 * ID is assumed to be just calloc'ed.
 */
void BKE_libblock_init_empty(ID *id)
{
  const IDTypeInfo *idtype_info = BKE_idtype_get_info_from_id(id);

  if (idtype_info != NULL) {
    if (idtype_info->init_data != NULL) {
      idtype_info->init_data(id);
    }
    return;
  }

  BLI_assert(!"IDType Missing IDTypeInfo");
}

/* ********** ID session-wise UUID management. ********** */
static uint global_session_uuid = 0;

/**
 * Generate a session-wise uuid for the given \a id.
 *
 * \note "session-wise" here means while editing a given .blend file. Once a new .blend file is
 * loaded or created, undo history is cleared/reset, and so is the uuid counter.
 */
void BKE_lib_libblock_session_uuid_ensure(ID *id)
{
  if (id->session_uuid == MAIN_ID_SESSION_UUID_UNSET) {
    id->session_uuid = atomic_add_and_fetch_uint32(&global_session_uuid, 1);
    /* In case overflow happens, still assign a valid ID. This way opening files many times works
     * correctly. */
    if (UNLIKELY(id->session_uuid == MAIN_ID_SESSION_UUID_UNSET)) {
      id->session_uuid = atomic_add_and_fetch_uint32(&global_session_uuid, 1);
    }
  }
}

/**
 * Re-generate a new session-wise uuid for the given \a id.
 *
 * \warning This has a very specific use-case (to handle UI-related data-blocks that are kept
 * across new file reading, when we do keep existing UI). No other usage is expected currently.
 */
void BKE_lib_libblock_session_uuid_renew(ID *id)
{
  id->session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  BKE_lib_libblock_session_uuid_ensure(id);
}

/**
 * Generic helper to create a new empty data-block of given type in given \a bmain database.
 *
 * \param name: can be NULL, in which case we get default name for this ID type.
 */
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

/**
 * Generic helper to create a new temporary empty data-block of given type,
 * *outside* of any Main database.
 *
 * \param name: can be NULL, in which case we get default name for this ID type. */
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
  if (!is_private_id_data) {
    /* When we are handling private ID data, we might still want to manage usercounts, even
     * though that ID data-block is actually outside of Main... */
    BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) == 0 ||
               (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) != 0);
  }
  /* Never implicitly copy shapekeys when generating temp data outside of Main database. */
  BLI_assert((flag & LIB_ID_CREATE_NO_MAIN) == 0 || (flag & LIB_ID_COPY_SHAPEKEY) == 0);

  /* 'Private ID' data handling. */
  if ((bmain != NULL) && is_private_id_data) {
    flag |= LIB_ID_CREATE_NO_MAIN;
  }

  /* The id->flag bits to copy over. */
  const int copy_idflag_mask = LIB_EMBEDDED_DATA;

  if ((flag & LIB_ID_CREATE_NO_ALLOCATE) != 0) {
    /* r_newid already contains pointer to allocated memory. */
    /* TODO do we want to memset(0) whole mem before filling it? */
    BLI_strncpy(new_id->name, id->name, sizeof(new_id->name));
    new_id->us = 0;
    new_id->tag |= LIB_TAG_NOT_ALLOCATED | LIB_TAG_NO_MAIN | LIB_TAG_NO_USER_REFCOUNT;
    /* TODO Do we want/need to copy more from ID struct itself? */
  }
  else {
    new_id = BKE_libblock_alloc(bmain, GS(id->name), id->name + 2, flag);
  }
  BLI_assert(new_id != NULL);

  const size_t id_len = BKE_libblock_get_alloc_info(GS(new_id->name), NULL);
  const size_t id_offset = sizeof(ID);
  if ((int)id_len - (int)id_offset > 0) { /* signed to allow neg result */ /* XXX ????? */
    const char *cp = (const char *)id;
    char *cpn = (char *)new_id;

    memcpy(cpn + id_offset, cp + id_offset, id_len - id_offset);
  }

  new_id->flag = (new_id->flag & ~copy_idflag_mask) | (id->flag & copy_idflag_mask);

  /* We do not want any handling of usercount in code duplicating the data here, we do that all
   * at once in id_copy_libmanagement_cb() at the end. */
  const int copy_data_flag = orig_flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

  if (id->properties) {
    new_id->properties = IDP_CopyProperty_ex(id->properties, copy_data_flag);
  }

  /* We may need our own flag to control that at some point, but for now 'no main' one should be
   * good enough. */
  if ((orig_flag & LIB_ID_CREATE_NO_MAIN) == 0 && id->override_library != NULL) {
    /* We do not want to copy existing override rules here, as they would break the proper
     * remapping between IDs. Proper overrides rules will be re-generated anyway. */
    BKE_lib_override_library_copy(new_id, id, false);
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

/* used everywhere in blenkernel */
void *BKE_libblock_copy(Main *bmain, const ID *id)
{
  ID *idn;

  BKE_libblock_copy_ex(bmain, id, &idn, 0);

  return idn;
}

/* XXX TODO: get rid of this useless wrapper at some point... */
void *BKE_libblock_copy_for_localize(const ID *id)
{
  ID *idn;
  BKE_libblock_copy_ex(NULL, id, &idn, LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);
  return idn;
}

/* ***************** ID ************************ */
ID *BKE_libblock_find_name(struct Main *bmain, const short type, const char *name)
{
  ListBase *lb = which_libbase(bmain, type);
  BLI_assert(lb != NULL);
  return BLI_findstring(lb, name, offsetof(ID, name) + 2);
}

/**
 * Sort given \a id into given \a lb list, using case-insensitive comparison of the id names.
 *
 * \note All other IDs beside given one are assumed already properly sorted in the list.
 *
 * \param id_sorting_hint: Ignored if NULL. Otherwise, used to check if we can insert \a id
 * immediately before or after that pointer. It must always be into given \a lb list.
 */
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
  if (id_sorting_hint != NULL && id_sorting_hint != id) {
    BLI_assert(BLI_findindex(lb, id_sorting_hint) >= 0);

    ID *id_sorting_hint_next = id_sorting_hint->next;
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) < 0 &&
        (id_sorting_hint_next == NULL ||
         BLI_strcasecmp(id_sorting_hint_next->name, id->name) > 0)) {
      BLI_insertlinkafter(lb, id_sorting_hint, id);
      return;
    }

    ID *id_sorting_hint_prev = id_sorting_hint->prev;
    if (BLI_strcasecmp(id_sorting_hint->name, id->name) > 0 &&
        (id_sorting_hint_prev == NULL ||
         BLI_strcasecmp(id_sorting_hint_prev->name, id->name) < 0)) {
      BLI_insertlinkbefore(lb, id_sorting_hint, id);
      return;
    }
  }

  void *item_array[ID_SORT_STEP_SIZE];
  int item_array_index;

  /* Step one: We go backward over a whole chunk of items at once, until we find a limit item
   * that is lower than, or equal (should never happen!) to the one we want to insert. */
  /* Note: We start from the end, because in typical 'heavy' case (insertion of lots of IDs at
   * once using the same base name), newly inserted items will generally be towards the end
   * (higher extension numbers). */
  for (idtest = lb->last, item_array_index = ID_SORT_STEP_SIZE - 1; idtest != NULL;
       idtest = idtest->prev, item_array_index--) {
    item_array[item_array_index] = idtest;
    if (item_array_index == 0) {
      if ((idtest->lib == NULL && id->lib != NULL) ||
          BLI_strcasecmp(idtest->name, id->name) <= 0) {
        break;
      }
      item_array_index = ID_SORT_STEP_SIZE;
    }
  }

  /* Step two: we go forward in the selected chunk of items and check all of them, as we know
   * that our target is in there. */

  /* If we reached start of the list, current item_array_index is off-by-one.
   * Otherwise, we already know that it points to an item lower-or-equal-than the one we want to
   * insert, no need to redo the check for that one.
   * So we can increment that index in any case. */
  for (item_array_index++; item_array_index < ID_SORT_STEP_SIZE; item_array_index++) {
    idtest = item_array[item_array_index];
    if ((idtest->lib != NULL && id->lib == NULL) || BLI_strcasecmp(idtest->name, id->name) > 0) {
      BLI_insertlinkbefore(lb, idtest, id);
      break;
    }
  }
  if (item_array_index == ID_SORT_STEP_SIZE) {
    if (idtest == NULL) {
      /* If idtest is NULL here, it means that in the first loop, the last comparison was
       * performed exactly on the first item of the list, and that it also failed. In other
       * words, all items in the list are greater than inserted one, so we can put it at the
       * start of the list. */
      /* Note that BLI_insertlinkafter() would have same behavior in that case, but better be
       * explicit here. */
      BLI_addhead(lb, id);
    }
    else {
      BLI_insertlinkafter(lb, idtest, id);
    }
  }

#undef ID_SORT_STEP_SIZE
}

/* Note: this code assumes and ensures that the suffix number can never go beyond 1 billion. */
#define MAX_NUMBER 1000000000
/* We do not want to get "name.000", so minimal number is 1. */
#define MIN_NUMBER 1
/* The maximum value up to which we search for the actual smallest unused number. Beyond that
 * value, we will only use the first biggest unused number, without trying to 'fill the gaps'
 * in-between already used numbers... */
#define MAX_NUMBERS_IN_USE 1024

/**
 * Helper building final ID name from given base_name and number.
 *
 * If everything goes well and we do generate a valid final ID name in given name, we return
 * true. In case the final name would overflow the allowed ID name length, or given number is
 * bigger than maximum allowed value, we truncate further the base_name (and given name, which is
 * assumed to have the same 'base_name' part), and return false.
 */
static bool id_name_final_build(char *name, char *base_name, size_t base_name_len, int number)
{
  char number_str[11]; /* Dot + nine digits + NULL terminator. */
  size_t number_str_len = BLI_snprintf_rlen(number_str, ARRAY_SIZE(number_str), ".%.3d", number);

  /* If the number would lead to an overflow of the maximum ID name length, we need to truncate
   * the base name part and do all the number checks again. */
  if (base_name_len + number_str_len >= MAX_ID_NAME - 2 || number >= MAX_NUMBER) {
    if (base_name_len + number_str_len >= MAX_ID_NAME - 2) {
      base_name_len = MAX_ID_NAME - 2 - number_str_len - 1;
    }
    else {
      base_name_len--;
    }
    base_name[base_name_len] = '\0';

    /* Code above may have generated invalid utf-8 string, due to raw truncation.
     * Ensure we get a valid one now. */
    base_name_len -= (size_t)BLI_utf8_invalid_strip(base_name, base_name_len);

    /* Also truncate orig name, and start the whole check again. */
    name[base_name_len] = '\0';
    return false;
  }

  /* We have our final number, we can put it in name and exit the function. */
  BLI_strncpy(name + base_name_len, number_str, number_str_len + 1);
  return true;
}

/**
 * Check to see if an ID name is already used, and find a new one if so.
 * Return true if a new name was created (returned in name).
 *
 * Normally the ID that's being checked is already in the ListBase, so ID *id points at the new
 * entry. The Python Library module needs to know what the name of a data-block will be before it
 * is appended, in this case ID *id is NULL.
 */
static bool check_for_dupid(ListBase *lb, ID *id, char *name, ID **r_id_sorting_hint)
{
  BLI_assert(strlen(name) < MAX_ID_NAME - 2);

  *r_id_sorting_hint = NULL;

  ID *id_test = lb->first;
  bool is_name_changed = false;

  if (id_test == NULL) {
    return is_name_changed;
  }

  const short id_type = (short)GS(id_test->name);

  /* Static storage of previous handled ID/name info, used to perform a quicker test and optimize
   * creation of huge number of IDs using the same given base name. */
  static char prev_orig_base_name[MAX_ID_NAME - 2] = {0};
  static char prev_final_base_name[MAX_ID_NAME - 2] = {0};
  static short prev_id_type = ID_LINK_PLACEHOLDER; /* Should never exist in actual ID list. */
  static int prev_number = MIN_NUMBER - 1;

  /* Initial test to check whether we can 'shortcut' the more complex loop of the main code
   * below. Note that we do not do that for low numbers, as that would prevent using actual
   * smallest available number in some cases, and benefits of this special case handling mostly
   * show up with high numbers anyway. */
  if (id_type == prev_id_type && prev_number >= MAX_NUMBERS_IN_USE &&
      prev_number < MAX_NUMBER - 1 && name[0] == prev_final_base_name[0]) {

    /* Get the name and number parts ("name.number"). */
    char base_name[MAX_ID_NAME - 2];
    int number = MIN_NUMBER;
    size_t base_name_len = BLI_split_name_num(base_name, &number, name, '.');
    size_t prev_final_base_name_len = strlen(prev_final_base_name);
    size_t prev_orig_base_name_len = strlen(prev_orig_base_name);

    if (base_name_len == prev_orig_base_name_len &&
        STREQLEN(base_name, prev_orig_base_name, prev_orig_base_name_len)) {
      /* Once we have ensured given base_name and original previous one are the same, we can
       * check that previously used number is actually used, and that next one is free. */
      /* Note that from now on, we only used previous final base name, as it might have been
       * truncated from original one due to number suffix length. */
      char final_name[MAX_ID_NAME - 2];
      char prev_final_name[MAX_ID_NAME - 2];
      BLI_strncpy(final_name, prev_final_base_name, prev_final_base_name_len + 1);
      BLI_strncpy(prev_final_name, prev_final_base_name, prev_final_base_name_len + 1);

      if (id_name_final_build(final_name, base_name, prev_final_base_name_len, prev_number + 1) &&
          id_name_final_build(prev_final_name, base_name, prev_final_base_name_len, prev_number)) {
        /* We successfully built valid final names of previous and current iterations,
         * now we have to ensure that previous final name is indeed used in current ID list,
         * and that current one is not. */
        bool is_valid = false;
        for (id_test = lb->first; id_test; id_test = id_test->next) {
          if (id != id_test && !ID_IS_LINKED(id_test)) {
            if (id_test->name[2] == final_name[0] && STREQ(final_name, id_test->name + 2)) {
              /* We expect final_name to not be already used, so this is a failure. */
              is_valid = false;
              break;
            }
            /* Previous final name should only be found once in the list, so if it was found
             * already, no need to do a string comparison again. */
            if (!is_valid && id_test->name[2] == prev_final_name[0] &&
                STREQ(prev_final_name, id_test->name + 2)) {
              is_valid = true;
              *r_id_sorting_hint = id_test;
            }
          }
        }

        if (is_valid) {
          /* Only the number changed, prev_orig_base_name, prev_final_base_name and prev_id_type
           * remain the same. */
          prev_number++;

          strcpy(name, final_name);
          return true;
        }
      }
    }
  }

  /* To speed up finding smallest unused number within [0 .. MAX_NUMBERS_IN_USE - 1].
   * We do not bother beyond that point. */
  ID *ids_in_use[MAX_NUMBERS_IN_USE] = {NULL};

  bool is_first_run = true;
  while (true) {
    /* Get the name and number parts ("name.number"). */
    char base_name[MAX_ID_NAME - 2];
    int number = MIN_NUMBER;
    size_t base_name_len = BLI_split_name_num(base_name, &number, name, '.');

    /* Store previous original given base name now, as we might alter it later in code below. */
    if (is_first_run) {
      strcpy(prev_orig_base_name, base_name);
      is_first_run = false;
    }

    /* In case we get an insane initial number suffix in given name. */
    /* Note: BLI_split_name_num() cannot return negative numbers, so we do not have to check for
     * that here. */
    if (number >= MAX_NUMBER || number < MIN_NUMBER) {
      number = MIN_NUMBER;
    }

    bool is_orig_name_used = false;
    for (id_test = lb->first; id_test; id_test = id_test->next) {
      char base_name_test[MAX_ID_NAME - 2];
      int number_test;
      if ((id != id_test) && !ID_IS_LINKED(id_test) && (name[0] == id_test->name[2]) &&
          (id_test->name[base_name_len + 2] == '.' || id_test->name[base_name_len + 2] == '\0') &&
          STREQLEN(name, id_test->name + 2, base_name_len) &&
          (BLI_split_name_num(base_name_test, &number_test, id_test->name + 2, '.') ==
           base_name_len)) {
        /* If we did not yet encounter exact same name as the given one, check the remaining
         * parts of the strings. */
        if (!is_orig_name_used) {
          is_orig_name_used = STREQ(name + base_name_len, id_test->name + 2 + base_name_len);
        }
        /* Mark number of current id_test name as used, if possible. */
        if (number_test < MAX_NUMBERS_IN_USE) {
          ids_in_use[number_test] = id_test;
        }
        /* Keep track of first largest unused number. */
        if (number <= number_test) {
          *r_id_sorting_hint = id_test;
          number = number_test + 1;
        }
      }
    }

    /* If there is no double, we are done.
     * Note however that name might have been changed (truncated) in a previous iteration
     * already.
     */
    if (!is_orig_name_used) {
      /* Don't bother updating prev_ static variables here, this case is not supposed to happen
       * that often, and is not straight-forward here, so just ignore and reset them to default.
       */
      prev_id_type = ID_LINK_PLACEHOLDER;
      prev_final_base_name[0] = '\0';
      prev_number = MIN_NUMBER - 1;

      /* Value set previously is meaningless in that case. */
      *r_id_sorting_hint = NULL;

      return is_name_changed;
    }

    /* Decide which value of number to use, either the smallest unused one if possible, or
     * default to the first largest unused one we got from previous loop. */
    for (int i = MIN_NUMBER; i < MAX_NUMBERS_IN_USE; i++) {
      if (ids_in_use[i] == NULL) {
        number = i;
        if (i > 0) {
          *r_id_sorting_hint = ids_in_use[i - 1];
        }
        break;
      }
    }
    /* At this point, number is either the lowest unused number within
     * [MIN_NUMBER .. MAX_NUMBERS_IN_USE - 1], or 1 greater than the largest used number if all
     * those low ones are taken.
     * We can't be bothered to look for the lowest unused number beyond
     * (MAX_NUMBERS_IN_USE - 1).
     */
    /* We know for wure that name will be changed. */
    is_name_changed = true;

    /* If id_name_final_build helper returns false, it had to truncate further given name, hence
     * we have to go over the whole check again. */
    if (!id_name_final_build(name, base_name, base_name_len, number)) {
      /* We have to clear our list of small used numbers before we do the whole check again. */
      memset(ids_in_use, 0, sizeof(ids_in_use));

      continue;
    }

    /* Update prev_ static variables, in case next call is for the same type of IDs and with the
     * same initial base name, we can skip a lot of above process. */
    prev_id_type = id_type;
    strcpy(prev_final_base_name, base_name);
    prev_number = number;

    return is_name_changed;
  }

#undef MAX_NUMBERS_IN_USE
}

#undef MIN_NUMBER
#undef MAX_NUMBER

/**
 * Ensures given ID has a unique name in given listbase.
 *
 * Only for local IDs (linked ones already have a unique ID in their library).
 *
 * \return true if a new name had to be created.
 */
bool BKE_id_new_name_validate(ListBase *lb, ID *id, const char *tname)
{
  bool result;
  char name[MAX_ID_NAME - 2];

  /* if library, don't rename */
  if (ID_IS_LINKED(id)) {
    return false;
  }

  /* if no name given, use name of current ID
   * else make a copy (tname args can be const) */
  if (tname == NULL) {
    tname = id->name + 2;
  }

  BLI_strncpy(name, tname, sizeof(name));

  if (name[0] == '\0') {
    /* Disallow empty names. */
    BLI_strncpy(name, DATA_(BKE_idtype_idcode_to_name(GS(id->name))), sizeof(name));
  }
  else {
    /* disallow non utf8 chars,
     * the interface checks for this but new ID's based on file names don't */
    BLI_utf8_invalid_strip(name, strlen(name));
  }

  ID *id_sorting_hint = NULL;
  result = check_for_dupid(lb, id, name, &id_sorting_hint);
  strcpy(id->name + 2, name);

  /* This was in 2.43 and previous releases
   * however all data in blender should be sorted, not just duplicate names
   * sorting should not hurt, but noting just in case it alters the way other
   * functions work, so sort every time. */
#if 0
  if (result) {
    id_sort_by_name(lb, id, id_sorting_hint);
  }
#endif

  id_sort_by_name(lb, id, id_sorting_hint);

  return result;
}

/* next to indirect usage in read/writefile also in editobject.c scene.c */
void BKE_main_id_clear_newpoins(Main *bmain)
{
  ID *id;

  FOREACH_MAIN_ID_BEGIN (bmain, id) {
    id->newid = NULL;
    id->tag &= ~LIB_TAG_NEW;
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

  /* Go over whole Main database to re-generate proper usercounts... */
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

  MainIDRelationsEntry *entry = BLI_ghash_lookup(id_relations->id_used_to_user, id);
  BLI_gset_insert(loop_tags, id);
  for (; entry != NULL; entry = entry->next) {

    /* Used_to_user stores ID pointer, not pointer to ID pointer. */
    ID *par_id = (ID *)entry->id_pointer;

    /* Our oh-so-beloved 'from' pointers... Those should always be ignored here, since the actual
     * relation we want to check is in the other way around. */
    if (entry->usage_flag & IDWALK_CB_LOOPBACK) {
      continue;
    }

    /* Shapekeys are considered 'private' to their owner ID here, and never tagged
     * (since they cannot be linked), so we have to switch effective parent to their owner.
     */
    if (GS(par_id->name) == ID_KE) {
      par_id = ((Key *)par_id)->from;
    }

    if (par_id->lib == NULL) {
      /* Local user, early out to avoid some gset querying... */
      continue;
    }
    if (!BLI_gset_haskey(done_ids, par_id)) {
      if (BLI_gset_haskey(loop_tags, par_id)) {
        /* We are in a 'dependency loop' of IDs, this does not say us anything, skip it.
         * Note that this is the situation that can lead to archipelagoes of linked data-blocks
         * (since all of them have non-local users, they would all be duplicated,
         * leading to a loop of unused linked data-blocks that cannot be freed since they all use
         * each other...). */
        continue;
      }
      /* Else, recursively check that user ID. */
      library_make_local_copying_check(par_id, loop_tags, id_relations, done_ids);
    }

    if (par_id->tag & LIB_TAG_DOIT) {
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

/**
 * Make linked data-blocks local.
 *
 * \param bmain: Almost certainly global main.
 * \param lib: If not NULL, only make local data-blocks from this library.
 * \param untagged_only: If true, only make local data-blocks not tagged with
 * LIB_TAG_PRE_EXISTING.
 * \param set_fake: If true, set fake user on all localized data-blocks
 * (except group and objects ones).
 */
/* Note: Old (2.77) version was simply making (tagging) data-blocks as local,
 * without actually making any check whether they were also indirectly used or not...
 *
 * Current version uses regular id_make_local callback, with advanced pre-processing step to
 * detect all cases of IDs currently indirectly used, but which will be used by local data only
 * once this function is finished.  This allows to avoid any unneeded duplication of IDs, and
 * hence all time lost afterwards to remove orphaned linked data-blocks...
 */
void BKE_library_make_local(Main *bmain,
                            const Library *lib,
                            GHash *old_to_new_ids,
                            const bool untagged_only,
                            const bool set_fake)
{
  ListBase *lbarray[MAX_LIBARRAY];

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

    /* Do not explicitly make local non-linkable IDs (shapekeys, in fact),
     * they are assumed to be handled by real data-blocks responsible of them. */
    const bool do_skip = (id && !BKE_idtype_idcode_is_linkable(GS(id->name)));

    for (; id; id = id->next) {
      ID *ntree = (ID *)ntreeFromID(id);

      id->tag &= ~LIB_TAG_DOIT;
      if (ntree != NULL) {
        ntree->tag &= ~LIB_TAG_DOIT;
      }

      if (id->lib == NULL) {
        id->tag &= ~(LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW);
        id->flag &= ~LIB_INDIRECT_WEAK_LINK;
      }
      /* The check on the fourth line (LIB_TAG_PRE_EXISTING) is done so it's possible to tag data
       * you don't want to be made local, used for appending data,
       * so any libdata already linked wont become local (very nasty
       * to discover all your links are lost after appending).
       * Also, never ever make proxified objects local, would not make any sense. */
      /* Some more notes:
       *   - Shapekeys are never tagged here (since they are not linkable).
       *   - Nodetrees used in materials etc. have to be tagged manually,
       *     since they do not exist in Main (!).
       * This is ok-ish on 'make local' side of things
       * (since those are handled by their 'owner' IDs),
       * but complicates slightly the pre-processing of relations between IDs at step 2... */
      else if (!do_skip && id->tag & (LIB_TAG_EXTERN | LIB_TAG_INDIRECT | LIB_TAG_NEW) &&
               ELEM(lib, NULL, id->lib) &&
               !(GS(id->name) == ID_OB && ((Object *)id)->proxy_from != NULL) &&
               ((untagged_only == false) || !(id->tag & LIB_TAG_PRE_EXISTING))) {
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
      lib_id_clear_library_data_ex(bmain, id);
      BKE_lib_id_expand_local(bmain, id);
      id->tag &= ~LIB_TAG_DOIT;

      if (GS(id->name) == ID_OB) {
        BKE_rigidbody_ensure_local_object(bmain, (Object *)id);
      }
    }
    else {
      /* In this specific case, we do want to make ID local even if it has no local usage yet...
       * Note that for objects, we don't want proxy pointers to be cleared yet. This will happen
       * down the road in this function.
       */
      BKE_lib_id_make_local(bmain,
                            id,
                            false,
                            LIB_ID_MAKELOCAL_FULL_LIBRARY |
                                LIB_ID_MAKELOCAL_OBJECT_NO_PROXY_CLEARING);

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
   * (e.g. if making local a parent object before its child...). See T48907. */
  /* TODO This is now the biggest step by far (in term of processing time).
   * We may be able to gain here by using again main->relations mapping, but...
   * this implies BKE_libblock_remap & co to be able to update main->relations on the fly.
   * Have to think about it a bit more, and see whether new code is OK first, anyway. */
  for (LinkNode *it = copied_ids; it; it = it->next) {
    ID *id = it->link;

    BLI_assert(id->newid != NULL);
    BLI_assert(id->lib != NULL);

    BKE_libblock_remap(bmain, id, id->newid, ID_REMAP_SKIP_INDIRECT_USAGE);
    if (old_to_new_ids) {
      BLI_ghash_insert(old_to_new_ids, id, id->newid);
    }

    /* Special hack for groups... Thing is, since we can't instantiate them here, we need to
     * ensure they remain 'alive' (only instantiation is a real group 'user'... *sigh* See
     * T49722. */
    if (GS(id->name) == ID_GR && (id->tag & LIB_TAG_INDIRECT) != 0) {
      id_us_ensure_real(id->newid);
    }
  }

#ifdef DEBUG_TIME
  printf("Step 4: Remap local usages of old (linked) ID to new (local) ID: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  /* Step 5: proxy 'remapping' hack. */
  for (LinkNode *it = copied_ids; it; it = it->next) {
    ID *id = it->link;

    /* Attempt to re-link copied proxy objects. This allows appending of an entire scene
     * from another blend file into this one, even when that blend file contains proxified
     * armatures that have local references. Since the proxified object needs to be linked
     * (not local), this will only work when the "Localize all" checkbox is disabled.
     * TL;DR: this is a dirty hack on top of an already weak feature (proxies). */
    if (GS(id->name) == ID_OB && ((Object *)id)->proxy != NULL) {
      Object *ob = (Object *)id;
      Object *ob_new = (Object *)id->newid;
      bool is_local = false, is_lib = false;

      /* Proxies only work when the proxified object is linked-in from a library. */
      if (ob->proxy->id.lib == NULL) {
        CLOG_WARN(&LOG,
                  "proxy object %s will loose its link to %s, because the "
                  "proxified object is local.",
                  id->newid->name,
                  ob->proxy->id.name);
        continue;
      }

      BKE_library_ID_test_usages(bmain, id, &is_local, &is_lib);

      /* We can only switch the proxy'ing to a made-local proxy if it is no longer
       * referred to from a library. Not checking for local use; if new local proxy
       * was not used locally would be a nasty bug! */
      if (is_local || is_lib) {
        CLOG_WARN(&LOG,
                  "made-local proxy object %s will loose its link to %s, "
                  "because the linked-in proxy is referenced (is_local=%i, is_lib=%i).",
                  id->newid->name,
                  ob->proxy->id.name,
                  is_local,
                  is_lib);
      }
      else {
        /* we can switch the proxy'ing from the linked-in to the made-local proxy.
         * BKE_object_make_proxy() shouldn't be used here, as it allocates memory that
         * was already allocated by object_make_local() (which called BKE_object_copy). */
        ob_new->proxy = ob->proxy;
        ob_new->proxy_group = ob->proxy_group;
        ob_new->proxy_from = ob->proxy_from;
        ob_new->proxy->proxy_from = ob_new;
        ob->proxy = ob->proxy_from = ob->proxy_group = NULL;
      }
    }
  }

#ifdef DEBUG_TIME
  printf("Step 5: Proxy 'remapping' hack: Done.\n");
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
        ob->pose->flag & POSE_RECALC) {
      BKE_pose_rebuild(bmain, ob, ob->data, true);
    }
  }

#ifdef DEBUG_TIME
  printf("Hack: Forcefully rebuild armature object poses: Done.\n");
  TIMEIT_VALUE_PRINT(make_local);
#endif

  BKE_main_id_clear_newpoins(bmain);
  BLI_memarena_free(linklist_mem);

#ifdef DEBUG_TIME
  printf("Cleanup and finish: Done.\n");
  TIMEIT_END(make_local);
#endif
}

/**
 * Use after setting the ID's name
 * When name exists: call 'new_id'
 */
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
  if (idtest != NULL) {
    /* BKE_id_new_name_validate also takes care of sorting. */
    BKE_id_new_name_validate(lb, idtest, NULL);
    bmain->is_memfile_undo_written = false;
  }
}

/**
 * Sets the name of a block to name, suitably adjusted for uniqueness.
 */
void BKE_libblock_rename(Main *bmain, ID *id, const char *name)
{
  ListBase *lb = which_libbase(bmain, GS(id->name));
  if (BKE_id_new_name_validate(lb, id, name)) {
    bmain->is_memfile_undo_written = false;
  }
}

/**
 * Generate full name of the data-block (without ID code, but with library if any).
 *
 * \note Result is unique to a given ID type in a given Main database.
 *
 * \param name: An allocated string of minimal length #MAX_ID_FULL_NAME,
 *              will be filled with generated string.
 * \param separator_char: Character to use for separating name and library name. Can be 0 to use
 *                        default (' ').
 */
void BKE_id_full_name_get(char name[MAX_ID_FULL_NAME], const ID *id, char separator_char)
{
  strcpy(name, id->name + 2);

  if (id->lib != NULL) {
    const size_t idname_len = strlen(id->name + 2);
    const size_t libname_len = strlen(id->lib->id.name + 2);

    name[idname_len] = separator_char ? separator_char : ' ';
    name[idname_len + 1] = '[';
    strcpy(name + idname_len + 2, id->lib->id.name + 2);
    name[idname_len + 2 + libname_len] = ']';
    name[idname_len + 2 + libname_len + 1] = '\0';
  }
}

/**
 * Generate full name of the data-block (without ID code, but with library if any),
 * with a 3-character prefix prepended indicating whether it comes from a library,
 * is overriding, has a fake or no user, etc.
 *
 * \note Result is unique to a given ID type in a given Main database.
 *
 * \param name: An allocated string of minimal length #MAX_ID_FULL_NAME_UI,
 *              will be filled with generated string.
 * \param separator_char: Character to use for separating name and library name. Can be 0 to use
 *                        default (' ').
 */
void BKE_id_full_name_ui_prefix_get(char name[MAX_ID_FULL_NAME_UI],
                                    const ID *id,
                                    char separator_char)
{
  name[0] = id->lib ? (ID_MISSING(id) ? 'M' : 'L') : ID_IS_OVERRIDE_LIBRARY(id) ? 'O' : ' ';
  name[1] = (id->flag & LIB_FAKEUSER) ? 'F' : ((id->us == 0) ? '0' : ' ');
  name[2] = ' ';

  BKE_id_full_name_get(name + 3, id, separator_char);
}

/**
 * Generate a concatenation of ID name (including two-chars type code) and its lib name, if any.
 *
 * \return A unique allocated string key for any ID in the whole Main database.
 */
char *BKE_id_to_unique_string_key(const struct ID *id)
{
  if (id->lib == NULL) {
    return BLI_strdup(id->name);
  }
  else {
    /* Prefix with an ascii character in the range of 32..96 (visible)
     * this ensures we can't have a library ID pair that collide.
     * Where 'LIfooOBbarOBbaz' could be ('LIfoo, OBbarOBbaz') or ('LIfooOBbar', 'OBbaz'). */
    const char ascii_len = strlen(id->lib->id.name + 2) + 32;
    return BLI_sprintfN("%c%s%s", ascii_len, id->lib->id.name, id->name);
  }
}

void BKE_id_tag_set_atomic(ID *id, int tag)
{
  atomic_fetch_and_or_int32(&id->tag, tag);
}

void BKE_id_tag_clear_atomic(ID *id, int tag)
{
  atomic_fetch_and_and_int32(&id->tag, ~tag);
}

/**
 * Check that given ID pointer actually is in G_MAIN.
 * Main intended use is for debug asserts in places we cannot easily get rid of G_Main...
 */
bool BKE_id_is_in_global_main(ID *id)
{
  /* We do not want to fail when id is NULL here, even though this is a bit strange behavior...
   */
  return (id == NULL || BLI_findindex(which_libbase(G_MAIN, GS(id->name)), id) != -1);
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
    else if (*order_a > *order_b) {
      return 1;
    }
  }

  return strcmp(id_a->name, id_b->name);
}

/**
 * Returns ordered list of data-blocks for display in the UI.
 * Result is list of LinkData of IDs that must be freed.
 */
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

/**
 * Reorder ID in the list, before or after the "relative" ID.
 */
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
