/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

#include <optional>

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "DNA_ID.h"
#include "DNA_userdef_enums.h"

struct BlendWriter;
struct Depsgraph;
struct GHash;
struct ID;
struct ID_Readfile_Data;
struct Library;
struct ListBase;
struct Main;
struct PointerRNA;
struct PropertyRNA;
struct bContext;

namespace blender::bke::id {

/** Status used and counters created during id-remapping. */
struct ID_Runtime_Remap {
  /** Status during ID remapping. */
  int status = 0;
  /** During ID remapping the number of skipped use cases that refcount the data-block. */
  int skipped_refcounted = 0;
  /**
   * During ID remapping the number of direct use cases that could be remapped
   * (e.g. obdata when in edit mode).
   */
  int skipped_direct = 0;
  /** During ID remapping, the number of indirect use cases that could not be remapped. */
  int skipped_indirect = 0;
};

struct ID_Runtime {
  /**
   * The last modifification time of the source .blend file where this ID was loaded from.
   */
  int64_t src_blend_modifification_time;

  ID_Runtime_Remap remap = {};
  /**
   * The depsgraph that owns this data block. This is only set on data-blocks which are
   * copied-on-eval by the depsgraph. Additional data-blocks created during depsgraph evaluation
   * are not owned by any specific depsgraph and thus this pointer is null for those.
   */
  Depsgraph *depsgraph = nullptr;

  /**
   * This data is only allocated & used during the readfile process. After that, the memory is
   * freed and the pointer set to `nullptr`.
   */
  ID_Readfile_Data *readfile_data = nullptr;
};

}  // namespace blender::bke::id

/**
 * Get allocation size of a given data-block type and optionally allocation `r_name`.
 */
size_t BKE_libblock_get_alloc_info(short type, const char **r_name);
/**
 * Allocates and returns memory of the right size for the specified block type,
 * initialized to zero.
 *
 * \note: Typically, caller also needs to immediately call #BKE_libblock_runtime_ensure on the
 * allocated ID data.
 */
ID *BKE_libblock_alloc_notest(short type) ATTR_WARN_UNUSED_RESULT;
/**
 * Allocates and returns an ID block of the specified type, with the specified name
 * (adjusted as necessary to ensure uniqueness), and appended to the specified list.
 * The user count is set to 1, all other content (apart from name and links) being
 * initialized to zero.
 *
 * \note By default, IDs allocated in a Main database will get the current library of the Main,
 * i.e. usually (besides in readfile case), they will have a `nullptr` `lib` pointer and be local
 * data. IDs allocated outside of a Main database will always get a `nullptr` `lib` pointer.
 */
void *BKE_libblock_alloc(Main *bmain, short type, const char *name, int flag)
    ATTR_WARN_UNUSED_RESULT;
/**
 * Same as for #BKE_libblock_alloc, but allows creating a data-block for a given owner library.
 *
 * \param owner_library: the Library to 'assign' the newly created ID to. Use `nullptr` to make ID
 * not use any library (i.e. become a local ID). Use `std::nullopt` for default behavior (i.e.
 * behavior of the #BKE_libblock_alloc function).
 */
void *BKE_libblock_alloc_in_lib(Main *bmain,
                                std::optional<Library *> owner_library,
                                short type,
                                const char *name,
                                int flag) ATTR_WARN_UNUSED_RESULT;
/**
 * Initialize an ID of given type, such that it has valid 'empty' data.
 * ID is assumed to be just calloc'ed.
 */
void BKE_libblock_init_empty(ID *id) ATTR_NONNULL(1);

/**
 * Ensure that the given ID does have a valid runtime data.
 *
 * Low-level API, should not be needed in typical ID usages, where ID::runtime can always be
 * assumed valid.
 */
void BKE_libblock_runtime_ensure(ID &id);

/**
 * Reset the runtime counters used by ID remapping.
 */
void BKE_libblock_runtime_reset_remapping_status(ID *id) ATTR_NONNULL(1);

/* *** ID's session_uid management. *** */

/**
 * When an ID's UID is of that value, it is unset/invalid (e.g. for runtime IDs, etc.).
 */
#define MAIN_ID_SESSION_UID_UNSET 0

/**
 * Generate a session-wise UID for the given \a id.
 *
 * \note "session-wise" here means while editing a given .blend file. Once a new .blend file is
 * loaded or created, undo history is cleared/reset, and so is the UID counter.
 */
void BKE_lib_libblock_session_uid_ensure(ID *id);
/**
 * Re-generate a new session-wise UID for the given \a id.
 *
 * \warning This has a few very specific use-cases, no other usage is expected currently:
 *   - To handle UI-related data-blocks that are kept across new file reading, when we do keep
 * existing UI.
 *   - For IDs that are made local without needing any copying.
 */
void BKE_lib_libblock_session_uid_renew(ID *id);

/**
 * Generic helper to create a new empty data-block of given type in given \a bmain database.
 *
 * \note By default, IDs created in a Main database will get the current library of the Main,
 * i.e. usually (besides in readfile case), they will have a `nullptr` `lib` pointer and be local
 * data. IDs created outside of a Main database will always get a `nullptr` `lib` pointer.
 *
 * \param name: can be NULL, in which case we get default name for this ID type.
 */
void *BKE_id_new(Main *bmain, short type, const char *name);

template<typename T> inline T *BKE_id_new(Main *bmain, const char *name)
{
  const ID_Type id_type = T::id_type;
  return static_cast<T *>(BKE_id_new(bmain, id_type, name));
}

/**
 * Same as for #BKE_id_new, but allows creating a data-block for (within) a given owner library.
 *
 * \param owner_library: the Library to 'assign' the newly created ID to. Use `nullptr` to make ID
 * not use any library (i.e. become a local ID). Use `std::nullopt` for default behavior (i.e.
 * behavior of the #BKE_id_new function).
 */
void *BKE_id_new_in_lib(Main *bmain,
                        std::optional<Library *> owner_library,
                        short type,
                        const char *name);
/**
 * Generic helper to create a new temporary empty data-block of given type,
 * *outside* of any Main database.
 *
 * \param name: can be NULL, in which case we get default name for this ID type.
 */
void *BKE_id_new_nomain(short type, const char *name);

template<typename T> inline T *BKE_id_new_nomain(const char *name)
{
  const ID_Type id_type = T::id_type;
  return static_cast<T *>(BKE_id_new_nomain(id_type, name));
}

/**
 * New ID creation/copying options.
 */
enum {
  /* *** Generic options (should be handled by all ID types copying, ID creation, etc.). *** */
  /**
   * Create data-block outside of any main database -
   * similar to 'localize' functions of materials etc.
   */
  LIB_ID_CREATE_NO_MAIN = 1 << 0,
  /**
   * Do not affect user reference-count of data-blocks used by new one
   * (which also gets zero user-count then).
   * Implies LIB_ID_CREATE_NO_MAIN.
   */
  LIB_ID_CREATE_NO_USER_REFCOUNT = 1 << 1,
  /**
   * Assume given `newid` already points to allocated memory for whole data-block
   * (ID + data) - USE WITH CAUTION!
   * Implies LIB_ID_CREATE_NO_MAIN.
   */
  LIB_ID_CREATE_NO_ALLOCATE = 1 << 2,

  /** Do not tag new ID for update in depsgraph. */
  LIB_ID_CREATE_NO_DEG_TAG = 1 << 8,

  /**
   * Very similar to #LIB_ID_CREATE_NO_MAIN, and should never be used with it (typically combined
   * with #LIB_ID_CREATE_LOCALIZE or #LIB_ID_COPY_LOCALIZE in fact).
   * It ensures that IDs created with it will get the #ID_TAG_LOCALIZED tag, and uses some
   * specific code in some copy cases (mostly for node trees).
   */
  LIB_ID_CREATE_LOCAL = 1 << 9,

  /**
   * Create for the depsgraph, when set #ID_TAG_COPIED_ON_EVAL must be set.
   * Internally this is used to share some pointers instead of duplicating them.
   */
  LIB_ID_COPY_SET_COPIED_ON_WRITE = 1 << 10,

  /**
   * Set #ID.newid pointer of the given source ID with the address of its new copy.
   */
  LIB_ID_COPY_ID_NEW_SET = 1 << 11,

  /* *** Specific options to some ID types or usages. *** */
  /* *** May be ignored by unrelated ID copying functions. *** */
  /** Object only, needed by make_local code. */
  /* LIB_ID_COPY_NO_PROXY_CLEAR = 1 << 16, */ /* UNUSED */
  /** Do not copy preview data, when supported. */
  LIB_ID_COPY_NO_PREVIEW = 1 << 17,
  /** Copy runtime data caches. */
  LIB_ID_COPY_CACHES = 1 << 18,
  /** Don't copy `id->adt`, used by ID data-block localization routines. */
  LIB_ID_COPY_NO_ANIMDATA = 1 << 19,
  /** Do not copy id->override_library, used by ID data-block override routines. */
  LIB_ID_COPY_NO_LIB_OVERRIDE = 1 << 21,
  /**
   * When copying local sub-data (like constraints or modifiers), do not set their "library
   * override local data" flag.
   */
  LIB_ID_COPY_NO_LIB_OVERRIDE_LOCAL_DATA_FLAG = 1 << 22,

  /* *** XXX Hackish/not-so-nice specific behaviors needed for some corner cases. *** */
  /* *** Ideally we should not have those, but we need them for now... *** */
  /** EXCEPTION! Deep-copy actions used by animation-data of copied ID. */
  LIB_ID_COPY_ACTIONS = 1 << 24,
  /** EXCEPTION! Deep-copy shape-keys used by copied obdata ID. */
  LIB_ID_COPY_SHAPEKEY = 1 << 26,
  /** EXCEPTION! Specific deep-copy of node trees used e.g. for rendering purposes. */
  LIB_ID_COPY_NODETREE_LOCALIZE = 1 << 27,
  /**
   * EXCEPTION! Specific handling of RB objects regarding collections differs depending whether we
   * duplicate scene/collections, or objects.
   */
  LIB_ID_COPY_RIGID_BODY_NO_COLLECTION_HANDLING = 1 << 28,
  /* Copy asset metadata. */
  LIB_ID_COPY_ASSET_METADATA = 1 << 29,

  /* *** Helper 'defines' gathering most common flag sets. *** */
  /** Shape-keys are not real ID's, more like local data to geometry IDs. */
  LIB_ID_COPY_DEFAULT = LIB_ID_COPY_SHAPEKEY,

  /** Create a local, outside of bmain, data-block to work on. */
  LIB_ID_CREATE_LOCALIZE = LIB_ID_CREATE_NO_MAIN | LIB_ID_CREATE_NO_USER_REFCOUNT |
                           LIB_ID_CREATE_NO_DEG_TAG,
  /** Generate a local copy, outside of bmain, to work on (used by copy-on-eval e.g.). */
  LIB_ID_COPY_LOCALIZE = LIB_ID_CREATE_LOCALIZE | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_CACHES |
                         LIB_ID_COPY_NO_LIB_OVERRIDE,
};

void BKE_libblock_copy_ex(Main *bmain, const ID *id, ID **new_id_p, int orig_flag);
/**
 * Same as #BKE_libblock_copy_ex, but allows copying data into a library, and not as local data
 * only.
 *
 * \param owner_library: the Library to 'assign' the newly created ID to. Use `nullptr` to make ID
 * not use any library (i.e. become a local ID). Use #std::nullopt for default behavior (i.e.
 * behavior of the #BKE_libblock_copy_ex function).
 * \param new_owner_id: When copying an embedded ID, the owner ID of the new copy. Should be
 * `nullopt` for regular ID copying, or in case the owner ID is not (yet) known.
 */
void BKE_libblock_copy_in_lib(Main *bmain,
                              std::optional<Library *> owner_library,
                              const ID *id,
                              std::optional<const ID *> new_owner_id,
                              ID **new_id_p,
                              int orig_flag);

/**
 * Used everywhere in blenkernel.
 *
 * \note Typically, the newly copied ID will be a local data (its `lib` pointer will be `nullptr`).
 * In practice, ID copying follows the same behavior as ID creation (see #BKE_libblock_alloc
 * documentation), with one special case: when the special flag #LIB_ID_CREATE_NO_ALLOCATE is
 * specified, the copied ID will have the same library as the source ID.
 *
 */
void *BKE_libblock_copy(Main *bmain, const ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * For newly created IDs, move it into same library as owner ID.
 * This assumes the ID is local.
 */
void BKE_id_move_to_same_lib(Main &bmain, ID &id, const ID &owner_id);

/** How to handle ID rename in case requested name is already used by another ID. */
enum class IDNewNameMode {
  /**
   * Never rename another existing ID if the target name is already in use. The renamed ID will get
   * a name modified with a numerical suffix instead.
   */
  RenameExistingNever = 0,
  /**
   * Always rename another existing ID if the target name is already in use. The renamed ID will
   * get the requested unmodified name.
   */
  RenameExistingAlways = 1,
  /**
   * Only rename another existing ID if the target name is already in use, when the current name of
   * the renamed ID has the same root as the other ID name (i.e. they have the same name, besides
   * the numeric suffix).
   * E.g. Assuming there is already an existing ID named `Object`:
   *   - Renaming `Object.001` to `Object`: rename to `Object`, the existing `Object` ID is renamed
   *     to e.g. `Object.001`
   *   - Renaming `Cube` to `Object`: rename to `Object.001`,  the existing `Object` ID is not
   *     renamed.
   */
  RenameExistingSameRoot = 2,
};

/** Information about how an ID rename went on. */
struct IDNewNameResult {
  /** How the renaming went on. */
  enum class Action {
    /** ID was not renamed, because requested new name was already the ID's name. */
    UNCHANGED = 0,
    /**
     * ID was not renamed, because requested new name would collide with another existing ID's
     * name, and the first available unique name is already current ID's name.
     */
    UNCHANGED_COLLISION = 1,
    /** Successfully renamed, without any collision with another ID's name. */
    RENAMED_NO_COLLISION = 2,
    /**
     * Successfully renamed, requested new name was adjusted to avoid collision with another ID.
     */
    RENAMED_COLLISION_ADJUSTED = 3,
    /**
     * Successfully renamed, requested new name was enforced onto given ID, and another ID had to
     * be renamed to avoid name collision.
     */
    RENAMED_COLLISION_FORCED = 4,
  } action = Action::UNCHANGED;

  /**
   * The colliding ID, if any.
   *
   * \warning Currently will be `nullptr` in #RENAMED_COLLISION_ADJUSTED case, for performance
   * reasons (avoid an ID lookup by name) when doing 'standard' #RenameExistingNever renames.
   */
  ID *other_id = nullptr;
};

/**
 * Sets the name of a block to name, suitably adjusted for uniqueness.
 *
 * \return true if the name of the ID was actually modified.
 */
IDNewNameResult BKE_libblock_rename(Main &bmain,
                                    ID &id,
                                    blender::StringRefNull name,
                                    const IDNewNameMode mode = IDNewNameMode::RenameExistingNever);

/**
 * Like #BKE_libblock_rename, but also performs additional higher-level updates like depsgraph
 * tagging when renaming Meta-ball objects, etc.
 *
 * \return true if the name of the ID was actually modified.
 */
IDNewNameResult BKE_id_rename(Main &bmain,
                              ID &id,
                              blender::StringRefNull name,
                              const IDNewNameMode mode = IDNewNameMode::RenameExistingNever);

/**
 * Find an ID in `bmain` by its type, name, and library ID.
 *
 * If `lib` is unset, the first ID matching the looked-up name is returned (local or linked). If
 * `lib` is null, the ID is searched into local ones only.
 */
ID *BKE_libblock_find_name(Main *bmain,
                           short type,
                           const char *name,
                           const std::optional<Library *> lib = std::nullopt)
    ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
ID *BKE_libblock_find_session_uid(Main *bmain, short type, uint32_t session_uid);
ID *BKE_libblock_find_session_uid(Main *bmain, uint32_t session_uid);
/**
 * Find an ID in `bmain` by its type, name, and library ID name.
 *
 * If `lib_name` is null or empty, the ID is searched into local ones only.
 */
ID *BKE_libblock_find_name_and_library(Main *bmain,
                                       short type,
                                       const char *name,
                                       const char *lib_name);
/**
 * Find an ID in `bmain` by its type, name, and absolute library file path.
 *
 * If `lib_filepath_abs` is null or empty, the ID is searched into local ones only.
 */
ID *BKE_libblock_find_name_and_library_filepath(Main *bmain,
                                                short type,
                                                const char *name,
                                                const char *lib_filepath_abs);

/**
 * Duplicate (a.k.a. deep copy) common processing options.
 * See also eDupli_ID_Flags for options controlling what kind of IDs to duplicate.
 */
enum eLibIDDuplicateFlags {
  /**
   * This call to a duplicate function is part of another call for some parent ID.
   * Therefore, this sub-process should not clear `newid` pointers, nor handle remapping itself.
   * NOTE: In some cases (like Object one), the duplicate function may be called on the root ID
   * with this flag set, as remapping and/or other similar tasks need to be handled by the caller.
   */
  LIB_ID_DUPLICATE_IS_SUBPROCESS = 1 << 0,
  /**
   * This call is performed on a 'root' ID, and should therefore perform some decisions regarding
   * sub-IDs (dependencies), check for linked vs. locale data, etc.
   */
  LIB_ID_DUPLICATE_IS_ROOT_ID = 1 << 1,
};

ENUM_OPERATORS(eLibIDDuplicateFlags)

/* `lib_remap.cc` (keep here since they're general functions) */
/**
 * New freeing logic options.
 */
enum {
  /* *** Generic options (should be handled by all ID types freeing). *** */
  /** Do not try to remove freed ID from given Main (passed Main may be NULL). */
  LIB_ID_FREE_NO_MAIN = 1 << 0,
  /**
   * Do not affect user reference-count of data-blocks used by freed one.
   * Implies LIB_ID_FREE_NO_MAIN.
   */
  LIB_ID_FREE_NO_USER_REFCOUNT = 1 << 1,
  /**
   * Assume freed ID data-block memory is managed elsewhere, do not free it
   * (still calls relevant ID type's freeing function though) - USE WITH CAUTION!
   * Implies LIB_ID_FREE_NO_MAIN.
   */
  LIB_ID_FREE_NOT_ALLOCATED = 1 << 2,

  /** Do not tag freed ID for update in depsgraph. */
  LIB_ID_FREE_NO_DEG_TAG = 1 << 8,
  /** Do not attempt to remove freed ID from UI data/notifiers/... */
  LIB_ID_FREE_NO_UI_USER = 1 << 9,
  /** Do not remove freed ID's name from a potential runtime name-map. */
  LIB_ID_FREE_NO_NAMEMAP_REMOVE = 1 << 10,
};

/**
 * Low-level ID freeing functions.
 *
 * \note These `BKE_libblock_free_` functions do NOT cover embedded IDs. Those are managed by the
 * owning ID, and are typically allocated/freed from the IDType callbacks.
 */

/**
 * Only free generic Python instance data (ID::py_instance).
 *
 * In most cases #BKE_id_free_ex handles this, when lower level functions are called directly
 * this function will need to be called too, if Python has access to the data.
 *
 * ID data-blocks such as #Material.nodetree are not stored in #Main.
 */
void BKE_libblock_free_data_py(ID *id);
/**
 * Only free generic runtime data (ID::runtime).
 *
 * In most cases #BKE_libblock_free_data handles this, but in rare cases (currently in readfile,
 * when freeing linked ID placeholders), it is necessary.
 */
void BKE_libblock_free_runtime_data(ID *id);

/** Free generic ID data, including the runtime and animation data, but not the python data. */
void BKE_libblock_free_data(ID *id, bool do_id_user) ATTR_NONNULL();

/**
 * Free IDtype-specific data (does _not_ free generic ID data, use
 * #BKE_libblock_free_data for that).
 */
void BKE_libblock_free_datablock(ID *id, int flag) ATTR_NONNULL();

/**
 * Complete ID freeing, extended version for corner cases.
 * Can override default (and safe!) freeing process, to gain some speed up.
 *
 * At that point, given id is assumed to not be used by any other data-block already
 * (might not be actually true, in case e.g. several inter-related IDs get freed together...).
 * However, they might still be using (referencing) other IDs, this code takes care of it if
 * #ID_TAG_NO_USER_REFCOUNT is not defined.
 *
 * \param bmain: #Main database containing the freed #ID,
 * can be NULL in case it's a temp ID outside of any #Main.
 * \param idv: Pointer to ID to be freed.
 * \param flag_orig: Set of \a LIB_ID_FREE_... flags controlling/overriding usual freeing process,
 * 0 to get default safe behavior.
 * \param use_flag_from_idtag: Still use freeing info flags from given #ID data-block,
 * even if some overriding ones are passed in \a flag parameter.
 */
void BKE_id_free_ex(Main *bmain, void *idv, int flag_orig, bool use_flag_from_idtag);
/**
 * Complete ID freeing, should be usable in most cases (even for out-of-Main IDs).
 *
 * See #BKE_id_free_ex description for full details.
 *
 * \param bmain: Main database containing the freed ID,
 * can be NULL in case it's a temp ID outside of any Main.
 * \param idv: Pointer to ID to be freed.
 */
void BKE_id_free(Main *bmain, void *idv);

/**
 * Not really a freeing function by itself,
 * it decrements user-count of given id, and only frees it if it reaches 0.
 */
void BKE_id_free_us(Main *bmain, void *idv) ATTR_NONNULL();

/**
 * Properly delete a single ID from given \a bmain database.
 */
void BKE_id_delete(Main *bmain, void *idv) ATTR_NONNULL();
/**
 * Like BKE_id_delete, but with extra corner-case options.
 *
 * \param extra_remapping_flags: Additional `ID_REMAP_` flags to pass to remapping code when
 * ensuring that deleted IDs are not used by any other ID in given `bmain`. Typical example would
 * be e.g. `ID_REMAP_FORCE_UI_POINTERS`, required when default UI-handling callbacks of remapping
 * code won't be working (e.g. from readfile code).
 */
void BKE_id_delete_ex(Main *bmain, void *idv, const int extra_remapping_flags) ATTR_NONNULL(1, 2);
/**
 * Properly delete all IDs tagged with \a ID_TAG_DOIT, in given \a bmain database.
 *
 * This is more efficient than calling #BKE_id_delete repetitively on a large set of IDs
 * (several times faster when deleting most of the IDs at once).
 *
 * \return Number of deleted data-blocks.
 */
size_t BKE_id_multi_tagged_delete(Main *bmain) ATTR_NONNULL();
/**
 * Properly delete all IDs from \a ids_to_delete, from given \a bmain database.
 *
 * This is more efficient than calling #BKE_id_delete repetitively on a large set of IDs
 * (several times faster when deleting most of the IDs at once).
 *
 * \note The ID pointers are not removed from the Set (which may contain more pointers than
 * originally given, when extra users or dependencies also had to be deleted with the original set
 * of IDs). They are all freed though, so these pointers are all invalid after calling this
 * function.
 *
 * \return Number of deleted data-blocks.
 */
size_t BKE_id_multi_delete(Main *bmain, blender::Set<ID *> &ids_to_delete);

/**
 * Add a 'NO_MAIN' data-block to given main (also sets user-counts of its IDs if needed).
 */
void BKE_libblock_management_main_add(Main *bmain, void *idv);
/** Remove a data-block from given main (set it to 'NO_MAIN' status). */
void BKE_libblock_management_main_remove(Main *bmain, void *idv);

void BKE_libblock_management_usercounts_set(Main *bmain, void *idv);
void BKE_libblock_management_usercounts_clear(Main *bmain, void *idv);

/**
 * Flag this linked ID as directly used by another ID in the current blend file.
 *
 * If the ID was marked as indirectly/weakly linked, those flags are cleared.
 *
 * This is a no-op when `id` is `nullptr` or not linked.
 *
 * This status is rechecked for the whole Main data-base as a step of pre-blendfile writing (see
 * #write_id_direct_linked_data_process_cb() and its usage in #write_file_handle). This ensures
 * that no reference to indirectly used IDs are kept in the written blendfile.
 */
void id_lib_extern(ID *id);
void id_lib_indirect_weak_link(ID *id);
/**
 * Ensure we have a real user
 *
 * \note Now that we have flags, we could get rid of the 'fake_user' special case,
 * flags are enough to ensure we always have a real user.
 * However, #ID_REAL_USERS is used in several places outside of core lib.c,
 * so think we can wait later to make this change.
 */
void id_us_ensure_real(ID *id);
void id_us_clear_real(ID *id);
/**
 * Same as \a id_us_plus, but does not handle lib indirect -> extern.
 * Only used by `readfile.cc` so far, but simpler/safer to keep it here nonetheless.
 */
void id_us_plus_no_lib(ID *id);
void id_us_plus(ID *id);
/* decrements the user count for *id. */
void id_us_min(ID *id);
void id_fake_user_set(ID *id);
void id_fake_user_clear(ID *id);
void BKE_id_newptr_and_tag_clear(ID *id);

/** Flags to control make local code behavior. */
enum {
  /**
   * Making that ID local is part of making local a whole library. Implies
   * #LIB_ID_MAKELOCAL_INDIRECT.
   */
  LIB_ID_MAKELOCAL_FULL_LIBRARY = 1 << 0,
  /** Also make local indirectly linked IDs. Implied by #LIB_ID_MAKELOCAL_FULL_LIBRARY. */
  LIB_ID_MAKELOCAL_INDIRECT = 1 << 1,

  /** In case caller code already knows this ID should be made local without copying. */
  LIB_ID_MAKELOCAL_FORCE_LOCAL = 1 << 8,
  /** In case caller code already knows this ID should be made local using copying. */
  LIB_ID_MAKELOCAL_FORCE_COPY = 1 << 9,

  /**
   * Clear asset data (in case the ID can actually be made local, in copy case asset data is never
   * copied over).
   */
  LIB_ID_MAKELOCAL_ASSET_DATA_CLEAR = 1 << 16,

  /** Clear any liboverride data as part of making this linked data local. */
  LIB_ID_MAKELOCAL_LIBOVERRIDE_CLEAR = 1 << 17,
};

/**
 * Helper to decide whether given `id` can be directly made local, or needs to be copied.
 * `r_force_local` and `r_force_copy` cannot be true together. But both can be false, in case no
 * action should be performed.
 *
 * \note low-level helper to de-duplicate logic between `BKE_lib_id_make_local_generic` and the
 * specific corner-cases implementations needed for objects and brushes.
 */
void BKE_lib_id_make_local_generic_action_define(
    Main *bmain, ID *id, int flags, bool *r_force_local, bool *r_force_copy);
/**
 * Generic 'make local' function, works for most of data-block types.
 */
void BKE_lib_id_make_local_generic(Main *bmain, ID *id, int flags);
/**
 * Calls the appropriate make_local method for the block, unless test is set.
 *
 * \note Always set #ID.newid pointer in case it gets duplicated.
 *
 * \param flags: Special flag used when making a whole library's content local,
 * it needs specific handling.
 * \return true is the ID has successfully been made local.
 */
bool BKE_lib_id_make_local(Main *bmain, ID *id, int flags);
/**
 * \note Does *not* set #ID.newid pointer.
 */
bool id_single_user(bContext *C, ID *id, PointerRNA *ptr, PropertyRNA *prop);

/** Test whether given `id` can be copied or not. */
bool BKE_id_copy_is_allowed(const ID *id);
/**
 * Generic entry point for copying a data-block (new API).
 *
 * \note Copy is generally only affecting the given data-block
 * (no ID used by copied one will be affected, besides user-count).
 *
 * There are exceptions though:
 * - Embedded IDs (root node trees and master collections) are always copied with their owner.
 * - If #LIB_ID_COPY_ACTIONS is defined, actions used by anim-data will be duplicated.
 * - If #LIB_ID_COPY_SHAPEKEY is defined, shape-keys will be duplicated.
 * - If #LIB_ID_CREATE_LOCAL is defined, root node trees will be deep-duplicated recursively.
 *
 * \note User-count of new copy is always set to 1.
 *
 * \note Typically, the newly copied ID will be a local data (its `lib` pointer will be `nullptr`).
 * In practice, ID copying follows the same behavior as ID creation (see #BKE_libblock_alloc
 * documentation), with one special case: when the special flag #LIB_ID_CREATE_NO_ALLOCATE is
 * specified, the copied ID will have the same library as the source ID.
 *
 * \param bmain: Main database, may be NULL only if LIB_ID_CREATE_NO_MAIN is specified.
 * \param id: Source data-block.
 * \param new_id_p: Pointer to new (copied) ID pointer, may be NULL.
 * Used to allow copying into already allocated memory.
 * \param flag: Set of copy options, see `DNA_ID.h` enum for details
 * (leave to zero for default, full copy).
 * \return NULL when copying that ID type is not supported, the new copy otherwise.
 */
ID *BKE_id_copy_ex(Main *bmain, const ID *id, ID **new_id_p, int flag);
/**
 * Enable copying non-local data into libraries.
 *
 * See #BKE_id_copy_ex for details.
 *
 * \param owner_library: the Library to 'assign' the newly created ID to. Use `nullptr` to make ID
 * not use any library (i.e. become a local ID). Use #std::nullopt for default behavior (i.e.
 * behavior of the #BKE_id_copy_ex function).
 * \param new_owner_id: When copying an embedded ID, the owner ID of the new copy. Should be
 * `nullopt` for regular ID copying, or in case the owner ID is not (yet) known.
 */
struct ID *BKE_id_copy_in_lib(Main *bmain,
                              std::optional<Library *> owner_library,
                              const ID *id,
                              std::optional<const ID *> new_owner_id,
                              ID **new_id_p,
                              int flag);
/**
 * Invoke the appropriate copy method for the block and return the new id as result.
 *
 * See #BKE_id_copy_ex for details.
 */
ID *BKE_id_copy(Main *bmain, const ID *id);

/**
 * Invoke the appropriate copy method for the block and return the new id as result.
 *
 * Unlike #BKE_id_copy, it does set the #ID.newid pointer of the given `id` to the copied one.
 *
 * It is designed as a basic common helper for the higher-level 'duplicate' operations (aka 'deep
 * copy' of data-blocks and some of their dependency ones), see e.g. #BKE_object_duplicate.
 *
 * Currently, it only handles the given ID, and their shape keys and actions if any, according to
 * the given `duplicate_flags`.
 *
 * \param duplicate_flags: is of type #eDupli_ID_Flags, see #UserDef.dupflag. Currently only
 * `USER_DUP_LINKED_ID` and `USER_DUP_ACT` have an effect here.
 * \param copy_flags: flags passed to #BKE_id_copy_ex.
 */
ID *BKE_id_copy_for_duplicate(Main *bmain,
                              ID *id,
                              eDupli_ID_Flags duplicate_flags,
                              int copy_flags);

/**
 * Special version of #BKE_id_copy which is safe from using evaluated id as source with a copy
 * result appearing in the main database.
 * Takes care of the referenced data-blocks consistency.
 */
ID *BKE_id_copy_for_use_in_bmain(Main *bmain, const ID *id);

/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note Most internal ID data itself is not swapped (only IDProperties are).
 *
 * \param bmain: May be NULL, in which case there is no guarantee that internal remapping of ID
 * pointers to themselves will be complete (regarding depsgraph and/or runtime data updates).
 * \param do_self_remap: Whether to remap internal pointers to itself or not.
 * \param self_remap_flags: Flags controlling self remapping, see BKE_lib_remap.hh.
 */
void BKE_lib_id_swap(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags);
/**
 * Does a mere memory swap over the whole IDs data (including type-specific memory).
 * \note All internal ID data itself is also swapped.
 *
 * For parameters description, see #BKE_lib_id_swap above.
 */
void BKE_lib_id_swap_full(
    Main *bmain, ID *id_a, ID *id_b, const bool do_self_remap, const int self_remap_flags);

/**
 * Sort given \a id into given \a lb list, using case-insensitive comparison of the id names.
 *
 * \note All other IDs beside given one are assumed already properly sorted in the list.
 *
 * \param id_sorting_hint: Ignored if NULL. Otherwise, used to check if we can insert \a id
 * immediately before or after that pointer. It must always be into given \a lb list.
 */
void id_sort_by_name(ListBase *lb, ID *id, ID *id_sorting_hint);
/**
 * Expand ID usages of given id as 'extern' (and no more indirect) linked data.
 * Used by ID copy/make_local functions.
 */
void BKE_lib_id_expand_local(Main *bmain, ID *id, int flags);

/**
 * Optionally set the given ID's name from given parameter, and ensure that the ID has a unique
 * name in given listbase.
 *
 * Uniqueness is only ensured within the ID's library (nullptr for local ones), libraries act as
 * some kind of namespace for IDs.
 *
 * \param newname: The new name of the given ID, if `nullptr` the current given ID name is used
 * instead. If the given ID has no name (or the given name is an empty string), the default
 * matching data name is used as a fallback.
 * \param do_linked_data: if true, also ensure a unique name in case the given ID is linked
 * (otherwise, just ensure that it is properly sorted).
 *
 * \return How renaming went on, see #IDNewNameResult for details.
 */
IDNewNameResult BKE_id_new_name_validate(Main &bmain,
                                         ListBase &lb,
                                         ID &id,
                                         const char *newname,
                                         IDNewNameMode mode,
                                         bool do_linked_data);

/**
 * Pull an ID out of a library (make it local). Only call this for IDs that
 * don't have other library users.
 *
 * \param flags: Same set of `LIB_ID_MAKELOCAL_` flags as passed to #BKE_lib_id_make_local.
 */
void BKE_lib_id_clear_library_data(Main *bmain, ID *id, int flags);

/**
 * Clear or set given tags for all ids of given type in `bmain` (runtime tags).
 *
 * \note Affect whole Main database.
 */
void BKE_main_id_tag_idcode(Main *mainvar, short type, int tag, bool value);
/**
 * Clear or set given tags for all ids in listbase (runtime tags).
 */
void BKE_main_id_tag_listbase(ListBase *lb, int tag, bool value);
/**
 * Clear or set given tags for all ids in bmain (runtime tags).
 */
void BKE_main_id_tag_all(Main *mainvar, int tag, bool value);

/**
 * Clear or set given flags for all ids in listbase (persistent flags).
 */
void BKE_main_id_flag_listbase(ListBase *lb, int flag, bool value);
/**
 * Clear or set given flags for all ids in bmain (persistent flags).
 */
void BKE_main_id_flag_all(Main *bmain, int flag, bool value);

/**
 * Next to indirect usage in `readfile.cc` / `writefile.cc` also in `editobject.c`, `scene.cc`.
 */
void BKE_main_id_newptr_and_tag_clear(Main *bmain);

void BKE_main_id_refcount_recompute(Main *bmain, bool do_linked_only);

void BKE_main_lib_objects_recalc_all(Main *bmain);

/**
 * Only for repairing files via versioning, avoid for general use.
 */
void BKE_main_id_repair_duplicate_names_listbase(Main *bmain, ListBase *lb);

/** 256 is MAX_ID_NAME - 2 */
#define MAX_ID_FULL_NAME (256 + 256 + 3 + 1)
/** Adds 'key-code' two letters at beginning. */
#define MAX_ID_FULL_NAME_UI (MAX_ID_FULL_NAME + 3)
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
void BKE_id_full_name_get(char name[MAX_ID_FULL_NAME], const ID *id, char separator_char);
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
                                    const ID *id,
                                    bool add_lib_hint,
                                    char separator_char,
                                    int *r_prefix_len);

/**
 * Get the name (without type prefix) of the ID.
 */
inline const char *BKE_id_name(const ID &id)
{
  return id.name + 2;
}
inline char *BKE_id_name(ID &id)
{
  return id.name + 2;
}

/**
 * Generate a concatenation of ID name (including two-chars type code) and its lib name, if any.
 *
 * \return A unique allocated string key for any ID in the whole Main database.
 */
char *BKE_id_to_unique_string_key(const ID *id);

/**
 * Make linked data-blocks local.
 *
 * \param bmain: Almost certainly global main.
 * \param lib: If not NULL, only make local data-blocks from this library.
 * \param untagged_only: If true, only make local data-blocks not tagged with
 * #ID_TAG_PRE_EXISTING.
 * \param set_fake: If true, set fake user on all localized data-blocks
 * (except group and objects ones).
 * \param clear_asset_data: If true, clear the asset metadata on all localized data-blocks, making
 * them normal non-asset data-blocks.
 */
void BKE_library_make_local(Main *bmain,
                            const Library *lib,
                            GHash *old_to_new_ids,
                            bool untagged_only,
                            bool set_fake,
                            bool clear_asset_data);

void BKE_id_tag_set_atomic(ID *id, int tag);
void BKE_id_tag_clear_atomic(ID *id, int tag);

/** Check that given ID pointer actually is in given `bmain`. */
bool BKE_id_is_in_main(Main *bmain, ID *id);
/**
 * Check that given ID pointer actually is in G_MAIN.
 * Main intended use is for debug asserts in places we cannot easily get rid of #G_MAIN.
 */
bool BKE_id_is_in_global_main(ID *id);

bool BKE_id_can_be_asset(const ID *id);

/**
 * Return the owner ID of the given `id`, if any.
 *
 * \note This will only return non-NULL for embedded IDs (master collections etc.), and shape-keys.
 *
 * \param debug_relationship_assert: True by default, whether to perform debug checks on validity
 * of the pointers between owner and embedded IDs. In some cases, these relations are not yet
 * (fully) valid, e.g. during ID copying.
 */
ID *BKE_id_owner_get(ID *id, const bool debug_relationship_assert = true);

/**
 * Check if that ID can be considered as editable from a high-level (editor) perspective.
 *
 * \note Unlike the #ID_IS_EDITABLE macro, this also take into account higher-level aspects, e.g.
 * it checks if the given ID is a system overrides (which should not be editable from the UI).
 */
bool BKE_id_is_editable(const Main *bmain, const ID *id);

/**
 * Check that a pointer from one ID to another is possible.
 *
 * Taking into account lib linking and main database membership.
 */
bool BKE_id_can_use_id(const ID &id_from, const ID &id_to);

/**
 * Returns ordered list of data-blocks for display in the UI.
 */
blender::Vector<ID *> BKE_id_ordered_list(const ListBase *lb);
/**
 * Reorder ID in the list, before or after the "relative" ID.
 */
void BKE_id_reorder(const ListBase *lb, ID *id, ID *relative, bool after);

void BKE_id_blend_write(BlendWriter *writer, ID *id);

#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->tag & ID_TAG_DOIT))

/* `lib_id_eval.cc` */

/**
 * Copy relatives parameters, from `id` to `id_cow`.
 * Use handle the #ID_RECALC_PARAMETERS tag.
 * \note Keep in sync with #ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW.
 */
void BKE_id_eval_properties_copy(ID *id_cow, ID *id);
