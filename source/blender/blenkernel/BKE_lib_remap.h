/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to perform remapping from one data-block pointer to another.
 *
 * \note `BKE_lib_` files are for operations over data-blocks themselves, although they might
 * alter Main as well (when creating/renaming/deleting an ID e.g.).
 *
 * \section Function Names
 *
 * \warning Descriptions below is ideal goal, current status of naming does not yet fully follow it
 * (this is WIP).
 *
 * - `BKE_lib_remap_libblock_` should be used for functions performing remapping.
 * - `BKE_lib_remap_callback_` should be used for functions managing remapping callbacks.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct IDRemapper;
struct LinkNode;

/* BKE_libblock_free, delete are declared in BKE_lib_id.h for convenience. */

/* Also IDRemap->flag. */
enum {
  /** Do not remap indirect usages of IDs (that is, when user is some linked data). */
  ID_REMAP_SKIP_INDIRECT_USAGE = 1 << 0,
  /**
   * This flag should always be set, *except for 'unlink' scenarios*
   * (only relevant when new_id == NULL).
   * Basically, when unset, NEVER_NULL ID usages will keep pointing to old_id, but (if needed)
   * old_id user count will still be decremented.
   * This is mandatory for 'delete ID' case,
   * but in all other situation this would lead to invalid user counts!
   */
  ID_REMAP_SKIP_NEVER_NULL_USAGE = 1 << 1,
  /**
   * This tells the callback func to flag with #LIB_DOIT all IDs
   * using target one with a 'never NULL' pointer (like e.g. #Object.data).
   */
  ID_REMAP_FLAG_NEVER_NULL_USAGE = 1 << 2,
  /**
   * This tells the callback func to force setting IDs
   * using target one with a 'never NULL' pointer to NULL.
   * \warning Use with extreme care, this will leave database in broken state
   * and can cause crashes very easily!
   */
  ID_REMAP_FORCE_NEVER_NULL_USAGE = 1 << 3,
  /** Do not remap library override pointers. */
  ID_REMAP_SKIP_OVERRIDE_LIBRARY = 1 << 4,
  /**
   * Force internal ID runtime pointers (like `ID.newid`, `ID.orig_id` etc.) to also be processed.
   * This should only be needed in some very specific cases, typically only BKE ID management code
   * should need it (e.g. required from `id_delete` to ensure no runtime pointer remains using
   * freed ones).
   */
  ID_REMAP_FORCE_INTERNAL_RUNTIME_POINTERS = 1 << 5,
  /** Force remapping of 'UI-like' ID usages (ID pointers stored in editors data etc.). */
  ID_REMAP_FORCE_UI_POINTERS = 1 << 6,
  /**
   * Force obdata pointers to also be processed, even when object (`id_owner`) is in Edit mode.
   * This is required by some tools creating/deleting IDs while operating in Edit mode, like e.g.
   * the 'separate' mesh operator.
   */
  ID_REMAP_FORCE_OBDATA_IN_EDITMODE = 1 << 7,

  /**
   * Don't touch the special user counts (use when the 'old' remapped ID remains in use):
   * - Do not transfer 'fake user' status from old to new ID.
   * - Do not clear 'extra user' from old ID.
   */
  ID_REMAP_SKIP_USER_CLEAR = 1 << 16,
  /**
   * Force handling user count even for IDs that are outside of Main (used in some cases when
   * dealing with IDs temporarily out of Main, but which will be put in it ultimately).
   */
  ID_REMAP_FORCE_USER_REFCOUNT = 1 << 17,
  /**
   * Do NOT handle user count for IDs (used in some cases when dealing with IDs from different
   * BMains, if usercount will be recomputed anyway afterwards, like e.g. in memfile reading during
   * undo step decoding).
   */
  ID_REMAP_SKIP_USER_REFCOUNT = 1 << 18,
  /**
   * Do NOT tag IDs which had some of their ID pointers updated for update in the depsgraph, or ID
   * type specific updates, like e.g. with node trees.
   */
  ID_REMAP_SKIP_UPDATE_TAGGING = 1 << 19,
  /**
   * Do not attempt to access original ID pointers (triggers usages of
   * `IDWALK_NO_ORIG_POINTERS_ACCESS` too).
   *
   * Use when original ID pointers values are (probably) not valid, e.g. during read-file process.
   */
  ID_REMAP_NO_ORIG_POINTERS_ACCESS = 1 << 20,
};

typedef enum eIDRemapType {
  /** Remap an ID reference to a new reference. The new reference can also be null. */
  ID_REMAP_TYPE_REMAP = 0,

  /** Cleanup all IDs used by a specific one. */
  ID_REMAP_TYPE_CLEANUP = 1,
} eIDRemapType;

/**
 * Replace all references in given Main using the given \a mappings
 *
 * \note Is preferred over BKE_libblock_remap_locked due to performance.
 */
void BKE_libblock_remap_multiple_locked(struct Main *bmain,
                                        struct IDRemapper *mappings,
                                        const int remap_flags);

void BKE_libblock_remap_multiple(struct Main *bmain,
                                 struct IDRemapper *mappings,
                                 const int remap_flags);

/**
 * Bare raw remapping of IDs, with no other processing than actually updating the ID pointers. No
 * usercount, direct vs indirect linked status update, depsgraph tagging, etc.
 *
 * This is way more efficient than regular remapping from #BKE_libblock_remap_multiple & co, but it
 * implies that calling code handles all the other aspects described above. This is typically the
 * case e.g. in readfile process.
 *
 * WARNING: This call will likely leave the given BMain in invalid state in many aspects. */
void BKE_libblock_remap_multiple_raw(struct Main *bmain,
                                     struct IDRemapper *mappings,
                                     const int remap_flags);
/**
 * Replace all references in given Main to \a old_id by \a new_id
 * (if \a new_id is NULL, it unlinks \a old_id).
 *
 * \note Requiring new_id to be non-null, this *may* not be the case ultimately,
 * but makes things simpler for now.
 */
void BKE_libblock_remap_locked(struct Main *bmain, void *old_idv, void *new_idv, int remap_flags)
    ATTR_NONNULL(1, 2);
void BKE_libblock_remap(struct Main *bmain, void *old_idv, void *new_idv, int remap_flags)
    ATTR_NONNULL(1, 2);

/**
 * Unlink given \a id from given \a bmain
 * (does not touch to indirect, i.e. library, usages of the ID).
 *
 * \param do_flag_never_null: If true, all IDs using \a idv in a 'non-NULL' way are flagged by
 * #LIB_TAG_DOIT flag (quite obviously, 'non-NULL' usages can never be unlinked by this function).
 */
void BKE_libblock_unlink(struct Main *bmain,
                         void *idv,
                         bool do_flag_never_null,
                         bool do_skip_indirect) ATTR_NONNULL();

/**
 * Similar to libblock_remap, but only affects IDs used by given \a idv ID.
 *
 * \param old_idv: Unlike BKE_libblock_remap, can be NULL,
 * in which case all ID usages by given \a idv will be cleared.
 *
 * \param bmain: May be NULL, in which case there won't be depsgraph updates nor post-processing on
 * some ID types (like collections or objects) to ensure their runtime data is valid.
 */
void BKE_libblock_relink_ex(
    struct Main *bmain, void *idv, void *old_idv, void *new_idv, int remap_flags) ATTR_NONNULL(2);
/**
 * Same as #BKE_libblock_relink_ex, but applies all rules defined in \a id_remapper to \a ids (or
 * does cleanup if `ID_REMAP_TYPE_CLEANUP` is specified as \a remap_type).
 */
void BKE_libblock_relink_multiple(struct Main *bmain,
                                  struct LinkNode *ids,
                                  eIDRemapType remap_type,
                                  struct IDRemapper *id_remapper,
                                  int remap_flags);

/**
 * Remaps ID usages of given ID to their `id->newid` pointer if not None, and proceeds recursively
 * in the dependency tree of IDs for all data-blocks tagged with `LIB_TAG_NEW`.
 *
 * \note `LIB_TAG_NEW` is cleared.
 *
 * Very specific usage, not sure we'll keep it on the long run,
 * currently only used in Object/Collection duplication code.
 */
void BKE_libblock_relink_to_newid(struct Main *bmain, struct ID *id, int remap_flag)
    ATTR_NONNULL();

typedef void (*BKE_library_free_notifier_reference_cb)(const void *);
typedef void (*BKE_library_remap_editor_id_reference_cb)(const struct IDRemapper *mappings);

void BKE_library_callback_free_notifier_reference_set(BKE_library_free_notifier_reference_cb func);
void BKE_library_callback_remap_editor_id_reference_set(
    BKE_library_remap_editor_id_reference_cb func);

/* IDRemapper */
struct IDRemapper;
typedef enum IDRemapperApplyResult {
  /** No remapping rules available for the source. */
  ID_REMAP_RESULT_SOURCE_UNAVAILABLE,
  /** Source isn't mappable (e.g. NULL). */
  ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE,
  /** Source has been remapped to a new pointer. */
  ID_REMAP_RESULT_SOURCE_REMAPPED,
  /** Source has been set to NULL. */
  ID_REMAP_RESULT_SOURCE_UNASSIGNED,
} IDRemapperApplyResult;

typedef enum IDRemapperApplyOptions {
  /**
   * Update the user count of the old and new ID data-block.
   *
   * For remapping the old ID users will be decremented and the new ID users will be
   * incremented. When un-assigning the old ID users will be decremented.
   *
   * NOTE: Currently unused by main remapping code, since user-count is handled by
   * `foreach_libblock_remap_callback_apply` there, depending on whether the remapped pointer does
   * use it or not. Need for rare cases in UI handling though (see e.g. `image_id_remap` in
   * `space_image.cc`).
   */
  ID_REMAP_APPLY_UPDATE_REFCOUNT = (1 << 0),

  /**
   * Make sure that the new ID data-block will have a 'real' user.
   *
   * NOTE: See Note for #ID_REMAP_APPLY_UPDATE_REFCOUNT above.
   */
  ID_REMAP_APPLY_ENSURE_REAL = (1 << 1),

  /**
   * Unassign in stead of remap when the new ID data-block would become id_self.
   *
   * To use this option 'BKE_id_remapper_apply_ex' must be used with a not-null id_self parameter.
   */
  ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF = (1 << 2),

  ID_REMAP_APPLY_DEFAULT = 0,
} IDRemapperApplyOptions;
ENUM_OPERATORS(IDRemapperApplyOptions, ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF)

typedef void (*IDRemapperIterFunction)(struct ID *old_id, struct ID *new_id, void *user_data);

/**
 * Create a new ID Remapper.
 *
 * An ID remapper stores multiple remapping rules.
 */
struct IDRemapper *BKE_id_remapper_create(void);

void BKE_id_remapper_clear(struct IDRemapper *id_remapper);
bool BKE_id_remapper_is_empty(const struct IDRemapper *id_remapper);
/** Free the given ID Remapper. */
void BKE_id_remapper_free(struct IDRemapper *id_remapper);
/** Add a new remapping. Does not replace an existing mapping for `old_id`, if any. */
void BKE_id_remapper_add(struct IDRemapper *id_remapper, struct ID *old_id, struct ID *new_id);
/** Add a new remapping, replacing a potential already existing mapping of `old_id`. */
void BKE_id_remapper_add_overwrite(struct IDRemapper *id_remapper,
                                   struct ID *old_id,
                                   struct ID *new_id);

/**
 * Apply a remapping.
 *
 * Update the id pointer stored in the given r_id_ptr if a remapping rule exists.
 */
IDRemapperApplyResult BKE_id_remapper_apply(const struct IDRemapper *id_remapper,
                                            struct ID **r_id_ptr,
                                            IDRemapperApplyOptions options);
/**
 * Apply a remapping.
 *
 * Use this function when `ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF`. In this case
 * the #id_self parameter is required. Otherwise the #BKE_id_remapper_apply can be used.
 *
 * \param id_self: required for ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF.
 *     When remapping to id_self it will then be remapped to NULL.
 */
IDRemapperApplyResult BKE_id_remapper_apply_ex(const struct IDRemapper *id_remapper,
                                               struct ID **r_id_ptr,
                                               IDRemapperApplyOptions options,
                                               struct ID *id_self);
bool BKE_id_remapper_has_mapping_for(const struct IDRemapper *id_remapper, uint64_t type_filter);

/**
 * Determine the mapping result, without applying the mapping.
 */
IDRemapperApplyResult BKE_id_remapper_get_mapping_result(const struct IDRemapper *id_remapper,
                                                         struct ID *id,
                                                         IDRemapperApplyOptions options,
                                                         const struct ID *id_self);
void BKE_id_remapper_iter(const struct IDRemapper *id_remapper,
                          IDRemapperIterFunction func,
                          void *user_data);

/** Returns a readable string for the given result. Can be used for debugging purposes. */
const char *BKE_id_remapper_result_string(const IDRemapperApplyResult result);
/** Prints out the rules inside the given id_remapper. Can be used for debugging purposes. */
void BKE_id_remapper_print(const struct IDRemapper *id_remapper);

#ifdef __cplusplus
}
#endif
