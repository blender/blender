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
#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage data-blocks inside of Blender's Main data-base, or as independent runtime-only
 * data.
 *
 * \note `BKE_lib_` files are for operations over data-blocks themselves, although they might
 * alter Main as well (when creating/renaming/deleting an ID e.g.).
 *
 * \section Function Names
 *
 * \warning Descriptions below is ideal goal, current status of naming does not yet fully follow it
 * (this is WIP).
 *
 * - `BKE_lib_id_` should be used for rather high-level operations, that involve Main database and
 *   relations with other IDs, and can be considered as 'safe' (as in, in themselves, they leave
 *   affected IDs/Main in a consistent status).
 * - `BKE_lib_libblock_` should be used for lower level operations, that perform some parts of
 *   `BKE_lib_id_` ones, but will generally not ensure caller that affected data is in a consistent
 *   state by their own execution alone.
 * - `BKE_lib_main_` should be used for operations performed over all IDs of a given Main
 *   data-base.
 *
 * \note External code should typically not use `BKE_lib_libblock_` functions, except in some
 * specific cases requiring advanced (and potentially dangerous) handling.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendWriter;
struct GHash;
struct ID;
struct Library;
struct ListBase;
struct Main;
struct PointerRNA;
struct PropertyRNA;
struct bContext;

/**
 * Get allocation size of a given data-block type and optionally allocation name.
 */
size_t BKE_libblock_get_alloc_info(short type, const char **name);
/**
 * Allocates and returns memory of the right size for the specified block type,
 * initialized to zero.
 */
void *BKE_libblock_alloc_notest(short type) ATTR_WARN_UNUSED_RESULT;
/**
 * Allocates and returns a block of the specified type, with the specified name
 * (adjusted as necessary to ensure uniqueness), and appended to the specified list.
 * The user count is set to 1, all other content (apart from name and links) being
 * initialized to zero.
 */
void *BKE_libblock_alloc(struct Main *bmain, short type, const char *name, int flag)
    ATTR_WARN_UNUSED_RESULT;
/**
 * Initialize an ID of given type, such that it has valid 'empty' data.
 * ID is assumed to be just calloc'ed.
 */
void BKE_libblock_init_empty(struct ID *id) ATTR_NONNULL(1);

/* *** ID's session_uuid management. *** */

/**
 * When an ID's uuid is of that value, it is unset/invalid (e.g. for runtime IDs, etc.).
 */
#define MAIN_ID_SESSION_UUID_UNSET 0

/**
 * Generate a session-wise uuid for the given \a id.
 *
 * \note "session-wise" here means while editing a given .blend file. Once a new .blend file is
 * loaded or created, undo history is cleared/reset, and so is the uuid counter.
 */
void BKE_lib_libblock_session_uuid_ensure(struct ID *id);
/**
 * Re-generate a new session-wise uuid for the given \a id.
 *
 * \warning This has a few very specific use-cases, no other usage is expected currently:
 *   - To handle UI-related data-blocks that are kept across new file reading, when we do keep
 * existing UI.
 *   - For IDs that are made local without needing any copying.
 */
void BKE_lib_libblock_session_uuid_renew(struct ID *id);

/**
 * Generic helper to create a new empty data-block of given type in given \a bmain database.
 *
 * \param name: can be NULL, in which case we get default name for this ID type.
 */
void *BKE_id_new(struct Main *bmain, short type, const char *name);
/**
 * Generic helper to create a new temporary empty data-block of given type,
 * *outside* of any Main database.
 *
 * \param name: can be NULL, in which case we get default name for this ID type.
 */
void *BKE_id_new_nomain(short type, const char *name);

/**
 * New ID creation/copying options.
 */
enum {
  /* *** Generic options (should be handled by all ID types copying, ID creation, etc.). *** */
  /** Create datablock outside of any main database -
   * similar to 'localize' functions of materials etc. */
  LIB_ID_CREATE_NO_MAIN = 1 << 0,
  /** Do not affect user refcount of datablocks used by new one
   * (which also gets zero usercount then).
   * Implies LIB_ID_CREATE_NO_MAIN. */
  LIB_ID_CREATE_NO_USER_REFCOUNT = 1 << 1,
  /** Assume given 'newid' already points to allocated memory for whole datablock
   * (ID + data) - USE WITH CAUTION!
   * Implies LIB_ID_CREATE_NO_MAIN. */
  LIB_ID_CREATE_NO_ALLOCATE = 1 << 2,

  /** Do not tag new ID for update in depsgraph. */
  LIB_ID_CREATE_NO_DEG_TAG = 1 << 8,

  /** Very similar to #LIB_ID_CREATE_NO_MAIN, and should never be used with it (typically combined
   * with #LIB_ID_CREATE_LOCALIZE or #LIB_ID_COPY_LOCALIZE in fact).
   * It ensures that IDs created with it will get the #LIB_TAG_LOCALIZED tag, and uses some
   * specific code in some copy cases (mostly for node trees). */
  LIB_ID_CREATE_LOCAL = 1 << 9,

  /** Create for the depsgraph, when set #LIB_TAG_COPIED_ON_WRITE must be set.
   * Internally this is used to share some pointers instead of duplicating them. */
  LIB_ID_COPY_SET_COPIED_ON_WRITE = 1 << 10,

  /* *** Specific options to some ID types or usages. *** */
  /* *** May be ignored by unrelated ID copying functions. *** */
  /** Object only, needed by make_local code. */
  /* LIB_ID_COPY_NO_PROXY_CLEAR = 1 << 16, */ /* UNUSED */
  /** Do not copy preview data, when supported. */
  LIB_ID_COPY_NO_PREVIEW = 1 << 17,
  /** Copy runtime data caches. */
  LIB_ID_COPY_CACHES = 1 << 18,
  /** Don't copy id->adt, used by ID datablock localization routines. */
  LIB_ID_COPY_NO_ANIMDATA = 1 << 19,
  /** Mesh: Reference CD data layers instead of doing real copy - USE WITH CAUTION! */
  LIB_ID_COPY_CD_REFERENCE = 1 << 20,
  /** Do not copy id->override_library, used by ID datablock override routines. */
  LIB_ID_COPY_NO_LIB_OVERRIDE = 1 << 21,
  /** When copying local sub-data (like constraints or modifiers), do not set their "library
   * override local data" flag. */
  LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG = 1 << 22,

  /* *** XXX Hackish/not-so-nice specific behaviors needed for some corner cases. *** */
  /* *** Ideally we should not have those, but we need them for now... *** */
  /** EXCEPTION! Deep-copy actions used by animdata of copied ID. */
  LIB_ID_COPY_ACTIONS = 1 << 24,
  /** Keep the library pointer when copying datablock outside of bmain. */
  LIB_ID_COPY_KEEP_LIB = 1 << 25,
  /** EXCEPTION! Deep-copy shapekeys used by copied obdata ID. */
  LIB_ID_COPY_SHAPEKEY = 1 << 26,
  /** EXCEPTION! Specific deep-copy of node trees used e.g. for rendering purposes. */
  LIB_ID_COPY_NODETREE_LOCALIZE = 1 << 27,
  /**
   * EXCEPTION! Specific handling of RB objects regarding collections differs depending whether we
   * duplicate scene/collections, or objects.
   */
  LIB_ID_COPY_RIGID_BODY_NO_COLLECTION_HANDLING = 1 << 28,

  /* *** Helper 'defines' gathering most common flag sets. *** */
  /** Shapekeys are not real ID's, more like local data to geometry IDs... */
  LIB_ID_COPY_DEFAULT = LIB_ID_COPY_SHAPEKEY,

  /** Create a local, outside of bmain, data-block to work on. */
  LIB_ID_CREATE_LOCALIZE = LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT |
                           LIB_ID_CREATE_NO_DEG_TAG,
  /** Generate a local copy, outside of bmain, to work on (used by COW e.g.). */
  LIB_ID_COPY_LOCALIZE = LIB_ID_CREATE_LOCALIZE | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_CACHES |
                         LIB_ID_COPY_NO_LIB_OVERRIDE,
};

void BKE_libblock_copy_ex(struct Main *bmain,
                          const struct ID *id,
                          struct ID **r_newid,
                          int orig_flag);
/**
 * Used everywhere in blenkernel.
 */
void *BKE_libblock_copy(struct Main *bmain, const struct ID *id) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Sets the name of a block to name, suitably adjusted for uniqueness.
 */
void BKE_libblock_rename(struct Main *bmain, struct ID *id, const char *name) ATTR_NONNULL();
/**
 * Use after setting the ID's name
 * When name exists: call 'new_id'
 */
void BLI_libblock_ensure_unique_name(struct Main *bmain, const char *name) ATTR_NONNULL();

struct ID *BKE_libblock_find_name(struct Main *bmain,
                                  short type,
                                  const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
struct ID *BKE_libblock_find_session_uuid(struct Main *bmain, short type, uint32_t session_uuid);
/**
 * Duplicate (a.k.a. deep copy) common processing options.
 * See also eDupli_ID_Flags for options controlling what kind of IDs to duplicate.
 */
typedef enum eLibIDDuplicateFlags {
  /** This call to a duplicate function is part of another call for some parent ID.
   * Therefore, this sub-process should not clear `newid` pointers, nor handle remapping itself.
   * NOTE: In some cases (like Object one), the duplicate function may be called on the root ID
   * with this flag set, as remapping and/or other similar tasks need to be handled by the caller.
   */
  LIB_ID_DUPLICATE_IS_SUBPROCESS = 1 << 0,
  /** This call is performed on a 'root' ID, and should therefore perform some decisions regarding
   * sub-IDs (dependencies), check for linked vs. locale data, etc. */
  LIB_ID_DUPLICATE_IS_ROOT_ID = 1 << 1,
} eLibIDDuplicateFlags;

ENUM_OPERATORS(eLibIDDuplicateFlags, LIB_ID_DUPLICATE_IS_ROOT_ID)

/* lib_remap.c (keep here since they're general functions) */
/**
 * New freeing logic options.
 */
enum {
  /* *** Generic options (should be handled by all ID types freeing). *** */
  /** Do not try to remove freed ID from given Main (passed Main may be NULL). */
  LIB_ID_FREE_NO_MAIN = 1 << 0,
  /**
   * Do not affect user refcount of datablocks used by freed one.
   * Implies LIB_ID_FREE_NO_MAIN.
   */
  LIB_ID_FREE_NO_USER_REFCOUNT = 1 << 1,
  /**
   * Assume freed ID datablock memory is managed elsewhere, do not free it
   * (still calls relevant ID type's freeing function though) - USE WITH CAUTION!
   * Implies LIB_ID_FREE_NO_MAIN.
   */
  LIB_ID_FREE_NOT_ALLOCATED = 1 << 2,

  /** Do not tag freed ID for update in depsgraph. */
  LIB_ID_FREE_NO_DEG_TAG = 1 << 8,
  /** Do not attempt to remove freed ID from UI data/notifiers/... */
  LIB_ID_FREE_NO_UI_USER = 1 << 9,
};

void BKE_libblock_free_datablock(struct ID *id, int flag) ATTR_NONNULL();
void BKE_libblock_free_data(struct ID *id, bool do_id_user) ATTR_NONNULL();

/**
 * In most cases #BKE_id_free_ex handles this, when lower level functions are called directly
 * this function will need to be called too, if Python has access to the data.
 *
 * ID data-blocks such as #Material.nodetree are not stored in #Main.
 */
void BKE_libblock_free_data_py(struct ID *id);

/**
 * Complete ID freeing, extended version for corner cases.
 * Can override default (and safe!) freeing process, to gain some speed up.
 *
 * At that point, given id is assumed to not be used by any other data-block already
 * (might not be actually true, in case e.g. several inter-related IDs get freed together...).
 * However, they might still be using (referencing) other IDs, this code takes care of it if
 * #LIB_TAG_NO_USER_REFCOUNT is not defined.
 *
 * \param bmain: #Main database containing the freed #ID,
 * can be NULL in case it's a temp ID outside of any #Main.
 * \param idv: Pointer to ID to be freed.
 * \param flag: Set of \a LIB_ID_FREE_... flags controlling/overriding usual freeing process,
 * 0 to get default safe behavior.
 * \param use_flag_from_idtag: Still use freeing info flags from given #ID datablock,
 * even if some overriding ones are passed in \a flag parameter.
 */
void BKE_id_free_ex(struct Main *bmain, void *idv, int flag, bool use_flag_from_idtag);
/**
 * Complete ID freeing, should be usable in most cases (even for out-of-Main IDs).
 *
 * See #BKE_id_free_ex description for full details.
 *
 * \param bmain: Main database containing the freed ID,
 * can be NULL in case it's a temp ID outside of any Main.
 * \param idv: Pointer to ID to be freed.
 */
void BKE_id_free(struct Main *bmain, void *idv);

/**
 * Not really a freeing function by itself,
 * it decrements usercount of given id, and only frees it if it reaches 0.
 */
void BKE_id_free_us(struct Main *bmain, void *idv) ATTR_NONNULL();

/**
 * Properly delete a single ID from given \a bmain database.
 */
void BKE_id_delete(struct Main *bmain, void *idv) ATTR_NONNULL();
/**
 * Properly delete all IDs tagged with \a LIB_TAG_DOIT, in given \a bmain database.
 *
 * This is more efficient than calling #BKE_id_delete repetitively on a large set of IDs
 * (several times faster when deleting most of the IDs at once).
 *
 * \warning Considered experimental for now, seems to be working OK but this is
 * risky code in a complicated area.
 * \return Number of deleted datablocks.
 */
size_t BKE_id_multi_tagged_delete(struct Main *bmain) ATTR_NONNULL();

/**
 * Add a 'NO_MAIN' data-block to given main (also sets usercounts of its IDs if needed).
 */
void BKE_libblock_management_main_add(struct Main *bmain, void *idv);
/** Remove a data-block from given main (set it to 'NO_MAIN' status). */
void BKE_libblock_management_main_remove(struct Main *bmain, void *idv);

void BKE_libblock_management_usercounts_set(struct Main *bmain, void *idv);
void BKE_libblock_management_usercounts_clear(struct Main *bmain, void *idv);

void id_lib_extern(struct ID *id);
void id_lib_indirect_weak_link(struct ID *id);
/**
 * Ensure we have a real user
 *
 * \note Now that we have flags, we could get rid of the 'fake_user' special case,
 * flags are enough to ensure we always have a real user.
 * However, #ID_REAL_USERS is used in several places outside of core lib.c,
 * so think we can wait later to make this change.
 */
void id_us_ensure_real(struct ID *id);
void id_us_clear_real(struct ID *id);
/**
 * Same as \a id_us_plus, but does not handle lib indirect -> extern.
 * Only used by readfile.c so far, but simpler/safer to keep it here nonetheless.
 */
void id_us_plus_no_lib(struct ID *id);
void id_us_plus(struct ID *id);
/* decrements the user count for *id. */
void id_us_min(struct ID *id);
void id_fake_user_set(struct ID *id);
void id_fake_user_clear(struct ID *id);
void BKE_id_newptr_and_tag_clear(struct ID *id);

/** Flags to control make local code behavior. */
enum {
  /** Making that ID local is part of making local a whole library. */
  LIB_ID_MAKELOCAL_FULL_LIBRARY = 1 << 0,

  /** In case caller code already knows this ID should be made local without copying. */
  LIB_ID_MAKELOCAL_FORCE_LOCAL = 1 << 1,
  /** In case caller code already knows this ID should be made local using copying. */
  LIB_ID_MAKELOCAL_FORCE_COPY = 1 << 2,

  /** Clear asset data (in case the ID can actually be made local, in copy case asset data is never
   * copied over). */
  LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR = 1 << 3,

  /* Special type-specific options. */
  /** For Objects, do not clear the proxy pointers while making the data-block local. */
  LIB_ID_MAKELOCAL_OBJECT_NO_PROXY_CLEARING = 1 << 16,
};

/**
 * Generic 'make local' function, works for most of data-block types.
 */
void BKE_lib_id_make_local_generic(struct Main *bmain, struct ID *id, int flags);
/**
 * Calls the appropriate make_local method for the block, unless test is set.
 *
 * \note Always set #ID.newid pointer in case it gets duplicated.
 *
 * \param flags: Special flag used when making a whole library's content local,
 * it needs specific handling.
 * \return true is the ID has successfully been made local.
 */
bool BKE_lib_id_make_local(struct Main *bmain, struct ID *id, int flags);
/**
 * \note Does *not* set #ID.newid pointer.
 */
bool id_single_user(struct bContext *C,
                    struct ID *id,
                    struct PointerRNA *ptr,
                    struct PropertyRNA *prop);
bool BKE_id_copy_is_allowed(const struct ID *id);
/**
 * Invokes the appropriate copy method for the block and returns the result in
 * #ID.newid, unless test. Returns true if the block can be copied.
 */
struct ID *BKE_id_copy(struct Main *bmain, const struct ID *id);
/**
 * Generic entry point for copying a data-block (new API).
 *
 * \note Copy is generally only affecting the given data-block
 * (no ID used by copied one will be affected, besides user-count).
 *
 * There are exceptions though:
 * - Embedded IDs (root node trees and master collections) are always copied with their owner.
 * - If #LIB_ID_COPY_ACTIONS is defined, actions used by animdata will be duplicated.
 * - If #LIB_ID_COPY_SHAPEKEY is defined, shape-keys will be duplicated.
 * - If #LIB_ID_CREATE_LOCAL is defined, root node trees will be deep-duplicated recursively.
 *
 * \note User-count of new copy is always set to 1.
 *
 * \param bmain: Main database, may be NULL only if LIB_ID_CREATE_NO_MAIN is specified.
 * \param id: Source data-block.
 * \param r_newid: Pointer to new (copied) ID pointer, may be NULL.
 * Used to allow copying into already allocated memory.
 * \param flag: Set of copy options, see `DNA_ID.h` enum for details
 * (leave to zero for default, full copy).
 * \return NULL when copying that ID type is not supported, the new copy otherwise.
 */
struct ID *BKE_id_copy_ex(struct Main *bmain, const struct ID *id, struct ID **r_newid, int flag);
/**
 * Invokes the appropriate copy method for the block and returns the result in
 * newid, unless test. Returns true if the block can be copied.
 */
struct ID *BKE_id_copy_for_duplicate(struct Main *bmain,
                                     struct ID *id,
                                     uint duplicate_flags,
                                     int copy_flags);

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 *
 * \param bmain: May be NULL, in which case there will be no remapping of internal pointers to
 * itself.
 */
void BKE_lib_id_swap(struct Main *bmain, struct ID *id_a, struct ID *id_b);
/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note All internal ID data itself is also swapped.
 *
 * \param bmain: May be NULL, in which case there will be no remapping of internal pointers to
 * itself.
 */
void BKE_lib_id_swap_full(struct Main *bmain, struct ID *id_a, struct ID *id_b);

/**
 * Sort given \a id into given \a lb list, using case-insensitive comparison of the id names.
 *
 * \note All other IDs beside given one are assumed already properly sorted in the list.
 *
 * \param id_sorting_hint: Ignored if NULL. Otherwise, used to check if we can insert \a id
 * immediately before or after that pointer. It must always be into given \a lb list.
 */
void id_sort_by_name(struct ListBase *lb, struct ID *id, struct ID *id_sorting_hint);
/**
 * Expand ID usages of given id as 'extern' (and no more indirect) linked data.
 * Used by ID copy/make_local functions.
 */
void BKE_lib_id_expand_local(struct Main *bmain, struct ID *id, int flags);

/**
 * Ensures given ID has a unique name in given listbase.
 *
 * Only for local IDs (linked ones already have a unique ID in their library).
 *
 * \param do_linked_data: if true, also ensure a unique name in case the given \a id is linked
 * (otherwise, just ensure that it is properly sorted).
 *
 * \return true if a new name had to be created.
 */
bool BKE_id_new_name_validate(struct ListBase *lb,
                              struct ID *id,
                              const char *name,
                              bool do_linked_data) ATTR_NONNULL(1, 2);
/**
 * Pull an ID out of a library (make it local). Only call this for IDs that
 * don't have other library users.
 *
 * \param flags: Same set of `LIB_ID_MAKELOCAL_` flags as passed to #BKE_lib_id_make_local.
 */
void BKE_lib_id_clear_library_data(struct Main *bmain, struct ID *id, int flags);

/**
 * Clear or set given tags for all ids of given type in `bmain` (runtime tags).
 *
 * \note Affect whole Main database.
 */
void BKE_main_id_tag_idcode(struct Main *mainvar, short type, int tag, bool value);
/**
 * Clear or set given tags for all ids in listbase (runtime tags).
 */
void BKE_main_id_tag_listbase(struct ListBase *lb, int tag, bool value);
/**
 * Clear or set given tags for all ids in bmain (runtime tags).
 */
void BKE_main_id_tag_all(struct Main *mainvar, int tag, bool value);

/**
 * Clear or set given flags for all ids in listbase (persistent flags).
 */
void BKE_main_id_flag_listbase(struct ListBase *lb, int flag, bool value);
/**
 * Clear or set given flags for all ids in bmain (persistent flags).
 */
void BKE_main_id_flag_all(struct Main *bmain, int flag, bool value);

/**
 * Next to indirect usage in `readfile.c/writefile.c` also in `editobject.c`, `scene.c`.
 */
void BKE_main_id_newptr_and_tag_clear(struct Main *bmain);

void BKE_main_id_refcount_recompute(struct Main *bmain, bool do_linked_only);

void BKE_main_lib_objects_recalc_all(struct Main *bmain);

/**
 * Only for repairing files via versioning, avoid for general use.
 */
void BKE_main_id_repair_duplicate_names_listbase(struct ListBase *lb);

#define MAX_ID_FULL_NAME (64 + 64 + 3 + 1)         /* 64 is MAX_ID_NAME - 2 */
#define MAX_ID_FULL_NAME_UI (MAX_ID_FULL_NAME + 3) /* Adds 'keycode' two letters at beginning. */
/**
 * Generate full name of the data-block (without ID code, but with library if any).
 *
 * \note Result is unique to a given ID type in a given Main database.
 *
 * \param name: An allocated string of minimal length #MAX_ID_FULL_NAME,
 * will be filled with generated string.
 * \param separator_char: Character to use for separating name and library name.
 * Can be 0 to use default (' ').
 */
void BKE_id_full_name_get(char name[MAX_ID_FULL_NAME], const struct ID *id, char separator_char);
/**
 * Generate full name of the data-block (without ID code, but with library if any),
 * with a 2 to 3 character prefix prepended indicating whether it comes from a library,
 * is overriding, has a fake or no user, etc.
 *
 * \note Result is unique to a given ID type in a given Main database.
 *
 * \param name: An allocated string of minimal length #MAX_ID_FULL_NAME_UI,
 * will be filled with generated string.
 * \param separator_char: Character to use for separating name and library name.
 * Can be 0 to use default (' ').
 * \param r_prefix_len: The length of the prefix added.
 */
void BKE_id_full_name_ui_prefix_get(char name[MAX_ID_FULL_NAME_UI],
                                    const struct ID *id,
                                    bool add_lib_hint,
                                    char separator_char,
                                    int *r_prefix_len);

/**
 * Generate a concatenation of ID name (including two-chars type code) and its lib name, if any.
 *
 * \return A unique allocated string key for any ID in the whole Main database.
 */
char *BKE_id_to_unique_string_key(const struct ID *id);

/**
 * Make linked data-blocks local.
 *
 * \param bmain: Almost certainly global main.
 * \param lib: If not NULL, only make local data-blocks from this library.
 * \param untagged_only: If true, only make local data-blocks not tagged with
 * #LIB_TAG_PRE_EXISTING.
 * \param set_fake: If true, set fake user on all localized data-blocks
 * (except group and objects ones).
 */
void BKE_library_make_local(struct Main *bmain,
                            const struct Library *lib,
                            struct GHash *old_to_new_ids,
                            bool untagged_only,
                            bool set_fake);

void BKE_id_tag_set_atomic(struct ID *id, int tag);
void BKE_id_tag_clear_atomic(struct ID *id, int tag);

/**
 * Check that given ID pointer actually is in G_MAIN.
 * Main intended use is for debug asserts in places we cannot easily get rid of #G_Main.
 */
bool BKE_id_is_in_global_main(struct ID *id);

bool BKE_id_can_be_asset(const struct ID *id);

/**
 * Returns ordered list of data-blocks for display in the UI.
 * Result is list of #LinkData of IDs that must be freed.
 */
void BKE_id_ordered_list(struct ListBase *ordered_lb, const struct ListBase *lb);
/**
 * Reorder ID in the list, before or after the "relative" ID.
 */
void BKE_id_reorder(const struct ListBase *lb, struct ID *id, struct ID *relative, bool after);

void BKE_id_blend_write(struct BlendWriter *writer, struct ID *id);

#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->tag & LIB_TAG_DOIT))

/* lib_id_eval.c */

/**
 * Copy relatives parameters, from `id` to `id_cow`.
 * Use handle the #ID_RECALC_PARAMETERS tag.
 * \note Keep in sync with #ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW.
 */
void BKE_id_eval_properties_copy(struct ID *id_cow, struct ID *id);

#ifdef __cplusplus
}
#endif
