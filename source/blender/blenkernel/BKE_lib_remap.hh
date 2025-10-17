/* SPDX-FileCopyrightText: 2023 Blender Authors
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
#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"

struct ID;
struct Main;

namespace blender::bke::id {
class IDRemapper;
}

/* BKE_libblock_free, delete are declared in BKE_lib_id.hh for convenience. */

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
   * Store in the #IDRemapper all IDs using target one with a 'never NULL' pointer (like e.g.
   * #Object.data), when such ID usage has (or should have) been remapped to `nullptr`. See also
   * #ID_REMAP_FORCE_NEVER_NULL_USAGE and #ID_REMAP_SKIP_NEVER_NULL_USAGE.
   */
  ID_REMAP_STORE_NEVER_NULL_USAGE = 1 << 2,
  /**
   * This tells the callback function to force setting IDs
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
   * Do remapping of `lib` Library pointers of IDs (by default these are completely ignored).
   *
   * WARNING: Use with caution. This is currently a 'raw' remapping, with no further processing. In
   * particular, DO NOT use this to make IDs local (i.e. remap a library pointer to NULL), unless
   * the calling code takes care of the rest of the required changes
   * (ID tags & flags updates, etc.).
   */
  ID_REMAP_DO_LIBRARY_POINTERS = 1 << 8,

  /**
   * Allow remapping of an ID pointer of a certain to another one of a different type.
   *
   * WARNING: Use with caution. Should only be needed in a very small amount of cases, e.g. when
   * converting an ID type to another. */
  ID_REMAP_ALLOW_IDTYPE_MISMATCH = 1 << 9,

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
   * BMains, if user-count will be recomputed anyway afterwards, like e.g.
   * in memfile reading during undo step decoding).
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

enum eIDRemapType {
  /** Remap an ID reference to a new reference. The new reference can also be null. */
  ID_REMAP_TYPE_REMAP = 0,

  /** Cleanup all IDs used by a specific one. */
  ID_REMAP_TYPE_CLEANUP = 1,
};

/**
 * Replace all references in given Main using the given \a mappings
 *
 * \note Is preferred over BKE_libblock_remap_locked due to performance.
 */
void BKE_libblock_remap_multiple_locked(Main *bmain,
                                        blender::bke::id::IDRemapper &mappings,
                                        const int remap_flags);

void BKE_libblock_remap_multiple(Main *bmain,
                                 blender::bke::id::IDRemapper &mappings,
                                 const int remap_flags);

/**
 * Bare raw remapping of IDs, with no other processing than actually updating the ID pointers.
 * No user-count, direct vs indirect linked status update, depsgraph tagging, etc.
 *
 * This is way more efficient than regular remapping from #BKE_libblock_remap_multiple & co, but it
 * implies that calling code handles all the other aspects described above. This is typically the
 * case e.g. in read-file process.
 *
 * WARNING: This call will likely leave the given BMain in invalid state in many aspects. */
void BKE_libblock_remap_multiple_raw(Main *bmain,
                                     blender::bke::id::IDRemapper &mappings,
                                     const int remap_flags);
/**
 * Replace all references in given Main to \a old_id by \a new_id
 * (if \a new_id is NULL, it unlinks \a old_id).
 *
 * \note Requiring new_id to be non-null, this *may* not be the case ultimately,
 * but makes things simpler for now.
 */
void BKE_libblock_remap_locked(Main *bmain, void *old_idv, void *new_idv, int remap_flags)
    ATTR_NONNULL(1, 2);
void BKE_libblock_remap(Main *bmain, void *old_idv, void *new_idv, int remap_flags)
    ATTR_NONNULL(1, 2);

/**
 * Unlink given \a id from given \a bmain
 * (does not touch to indirect, i.e. library, usages of the ID).
 */
void BKE_libblock_unlink(Main *bmain, void *idv, bool do_skip_indirect) ATTR_NONNULL();

/**
 * Similar to libblock_remap, but only affects IDs used by given \a idv ID.
 *
 * \param old_idv: Unlike BKE_libblock_remap, can be NULL,
 * in which case all ID usages by given \a idv will be cleared.
 *
 * \param bmain: May be NULL, in which case there won't be depsgraph updates nor post-processing on
 * some ID types (like collections or objects) to ensure their runtime data is valid.
 */
void BKE_libblock_relink_ex(Main *bmain, void *idv, void *old_idv, void *new_idv, int remap_flags)
    ATTR_NONNULL(2);
/**
 * Same as #BKE_libblock_relink_ex, but applies all rules defined in \a id_remapper to \a ids (or
 * does cleanup if `ID_REMAP_TYPE_CLEANUP` is specified as \a remap_type).
 */
void BKE_libblock_relink_multiple(Main *bmain,
                                  const blender::Span<ID *> ids,
                                  eIDRemapType remap_type,
                                  blender::bke::id::IDRemapper &id_remapper,
                                  int remap_flags);

/**
 * Remaps ID usages of given ID to their `id->newid` pointer if not None, and proceeds recursively
 * in the dependency tree of IDs for all data-blocks tagged with `ID_TAG_NEW`.
 *
 * \note `ID_TAG_NEW` is cleared.
 *
 * Very specific usage, not sure we'll keep it on the long run,
 * currently only used in Object/Collection duplication code.
 */
void BKE_libblock_relink_to_newid(Main *bmain, ID *id, int remap_flag) ATTR_NONNULL();

using BKE_library_free_notifier_reference_cb = void (*)(const void *);
using BKE_library_remap_editor_id_reference_cb =
    void (*)(const blender::bke::id::IDRemapper &mappings);

void BKE_library_callback_free_notifier_reference_set(BKE_library_free_notifier_reference_cb func);
void BKE_library_callback_remap_editor_id_reference_set(
    BKE_library_remap_editor_id_reference_cb func);

/* IDRemapper */
enum IDRemapperApplyResult {
  /** No remapping rules available for the source. */
  ID_REMAP_RESULT_SOURCE_UNAVAILABLE,
  /** Source isn't mappable (e.g. NULL). */
  ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE,
  /** Source has been remapped to a new pointer. */
  ID_REMAP_RESULT_SOURCE_REMAPPED,
  /** Source has been set to NULL. */
  ID_REMAP_RESULT_SOURCE_UNASSIGNED,
};

enum IDRemapperApplyOptions {
  /**
   * Update the user count of the old and new ID data-block.
   *
   * For remapping the old ID users will be decremented and the new ID users will be
   * incremented. When un-assigning the old ID users will be decremented.
   *
   * NOTE: Currently unused by main remapping code, since user-count is handled by
   * `foreach_libblock_remap_callback_apply` there, depending on whether the remapped pointer does
   * use it or not. Needed for rare cases in UI handling though (see e.g. `image_id_remap` in
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
   * Unassign instead of remap when the new ID pointer would point to itself.
   *
   * To use this option #IDRemapper::apply must be used with a non-null id_self parameter.
   */
  ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF = (1 << 2),

  ID_REMAP_APPLY_DEFAULT = 0,
};
ENUM_OPERATORS(IDRemapperApplyOptions)

using IDRemapperIterFunction = void (*)(ID *old_id, ID *new_id, void *user_data);
using IDTypeFilter = uint64_t;

namespace blender::bke::id {

class IDRemapper {
  Map<ID *, ID *> mappings_;
  IDTypeFilter source_types_ = 0;

  /**
   * Store all IDs using another ID with the 'NEVER_NULL' flag, which have (or
   * should have been) remapped to `nullptr`.
   */
  Set<ID *> never_null_users_;

 public:
  /**
   * In almost all cases, the original pointer and its new replacement should be of the same type.
   * however, there are some rare exceptions, e.g. when converting from one ID type to another.
   */
  bool allow_idtype_mismatch = false;

  void clear()
  {
    mappings_.clear();
    never_null_users_.clear();
    source_types_ = 0;
  }

  bool is_empty() const
  {
    return mappings_.is_empty();
  }

  bool contains_mappings_for_any(IDTypeFilter filter) const
  {
    return (source_types_ & filter) != 0;
  }

  /** Add a new remapping. Does not replace an existing mapping for `old_id`, if any. */
  void add(ID *old_id, ID *new_id);
  /** Add a new remapping, replacing a potential already existing mapping of `old_id`. */
  void add_overwrite(ID *old_id, ID *new_id);

  /** Determine the mapping result, without applying the mapping. */
  IDRemapperApplyResult get_mapping_result(ID *id,
                                           IDRemapperApplyOptions options,
                                           const ID *id_self) const;

  /**
   * Apply a remapping.
   *
   * Update the id pointer stored in the given r_id_ptr if a remapping rule exists.
   *
   * \param id_self: Only for ID_REMAP_APPLY_UNMAP_WHEN_REMAPPING_TO_SELF.
   *     When remapping to `id_self` it will then be remapped to `nullptr` instead.
   */
  IDRemapperApplyResult apply(ID **r_id_ptr,
                              IDRemapperApplyOptions options,
                              ID *id_self = nullptr) const;

  void never_null_users_add(ID *id)
  {
    never_null_users_.add(id);
  }

  const Set<ID *> &never_null_users() const
  {
    return never_null_users_;
  }

  /** Iterate over all remapping pairs in the remapper, and call the callback function on them. */
  void iter(FunctionRef<void(ID *old_id, ID *new_id)> func) const
  {
    for (auto item : mappings_.items()) {
      func(item.key, item.value);
    }
  }

  /** Return a readable string for the given result. Can be used for debugging purposes. */
  static StringRefNull result_to_string(const IDRemapperApplyResult result);

  /** Print out the rules inside the given id_remapper. Can be used for debugging purposes. */
  void print() const;
};

}  // namespace blender::bke::id
