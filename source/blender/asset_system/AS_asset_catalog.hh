/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_uuid.h"
#include "BLI_vector.hh"

#include "AS_asset_catalog_path.hh"

namespace blender::asset_system {

class AssetCatalog;
class AssetCatalogCollection;
class AssetCatalogDefinitionFile;
class AssetCatalogFilter;
class AssetCatalogTree;

using CatalogID = bUUID;
using CatalogPathComponent = std::string;
/* Would be nice to be able to use `std::filesystem::path` for this, but it's currently not
 * available on the minimum macOS target version. */
using CatalogFilePath = std::string;
using OwningAssetCatalogMap = Map<CatalogID, std::unique_ptr<AssetCatalog>>;

/* Manages the asset catalogs of a single asset library (i.e. of catalogs defined in a single
 * directory hierarchy). */
class AssetCatalogService {
  std::unique_ptr<AssetCatalogCollection> catalog_collection_;

  /**
   * Cached catalog tree storage. Lazy-created by #AssetCatalogService::catalog_tree().
   */
  std::unique_ptr<AssetCatalogTree> catalog_tree_;
  std::recursive_mutex catalog_tree_mutex_;

  Vector<std::unique_ptr<AssetCatalogCollection>> undo_snapshots_;
  Vector<std::unique_ptr<AssetCatalogCollection>> redo_snapshots_;

  CatalogFilePath asset_library_root_;
  bool is_read_only_ = false;

  friend class AssetLibraryService;
  friend class AssetLibrary;

 public:
  static const CatalogFilePath DEFAULT_CATALOG_FILENAME;

  struct read_only_tag {};

  explicit AssetCatalogService(const CatalogFilePath &asset_library_root = {});
  explicit AssetCatalogService(read_only_tag);

  /**
   * Set tag indicating that some catalog modifications are unsaved, which could
   * get lost on exit. This tag is not set by internal catalog code, the catalog
   * service user is responsible for it. It is cleared by #write_to_disk().
   *
   * This "dirty" state is tracked per catalog, so that it's possible to gracefully load changes
   * from disk. Any catalog with unsaved changes will not be overwritten by on-disk changes. */
  void tag_has_unsaved_changes(AssetCatalog *edited_catalog = nullptr);
  bool has_unsaved_changes() const;

  /**
   * Check if this is a read-only service meaning the user shouldn't be able to do edits. This is
   * not enforced by internal catalog code, the catalog service user is responsible for it. For
   * example the UI should disallow edits.
   */
  bool is_read_only() const;

  /** Load asset catalog definitions from the files found in the asset library. */
  void load_from_disk();
  /** Load asset catalog definitions from the given file or directory. */
  void load_from_disk(const CatalogFilePath &file_or_directory_path);

  /**
   * Duplicate the catalogs from \a other_service into this one. Does not rebuild the tree, this
   * needs to be done by the caller (call #rebuild_tree()!).
   *
   * \note If a catalog from \a other already exists in this collection (identified by catalog ID),
   * it will be skipped and \a on_duplicate_items will be called.
   */
  void add_from_existing(const AssetCatalogService &other_service,
                         FunctionRef<void(const AssetCatalog &existing,
                                          const AssetCatalog &to_be_ignored)> on_duplicate_items);

  /**
   * Write the catalog definitions to disk.
   *
   * The location where the catalogs are saved is variable, and depends on the location of the
   * blend file. The first matching rule wins:
   *
   * - Already loaded a CDF from disk?
   *    -> Always write to that file.
   * - The directory containing the blend file has a blender_assets.cats.txt file?
   *    -> Merge with & write to that file.
   * - The directory containing the blend file is part of an asset library, as per
   *   the user's preferences?
   *    -> Merge with & write to ${ASSET_LIBRARY_ROOT}/blender_assets.cats.txt
   * - Create a new file blender_assets.cats.txt next to the blend file.
   *
   * Return true on success, which either means there were no in-memory categories to save,
   * or the save was successful. */
  bool write_to_disk(const CatalogFilePath &blend_file_path);

  /**
   * Ensure that the next call to #on_blend_save_post() will choose a new location for the CDF
   * suitable for the location of the blend file (regardless of where the current catalogs come
   * from), and that catalogs will be merged with already-existing ones in that location.
   *
   * Use this for a "Save as..." that has to write the catalogs to the new blend file location,
   * instead of updating the previously read CDF. */
  void prepare_to_merge_on_write();

  /**
   * Merge on-disk changes into the in-memory asset catalogs.
   * This should be called before writing the asset catalogs to disk.
   *
   * - New on-disk catalogs are loaded into memory.
   * - Already-known on-disk catalogs are ignored (so will be overwritten with our in-memory
   *   data). This includes in-memory marked-as-deleted catalogs.
   */
  void reload_catalogs();

  /** Return catalog with the given ID. Return nullptr if not found. */
  AssetCatalog *find_catalog(CatalogID catalog_id) const;

  /**
   * Return first catalog with the given path. Return nullptr if not found. This is not an
   * efficient call as it's just a linear search over the catalogs.
   *
   * If there are multiple catalogs with the same path, return the first-loaded one. If there is
   * none marked as "first loaded", return the one with the lowest UUID. */
  AssetCatalog *find_catalog_by_path(const AssetCatalogPath &path) const;

  /**
   * Return true only if this catalog is known.
   * This treats deleted catalogs as "unknown". */
  bool is_catalog_known(CatalogID catalog_id) const;

  /**
   * Create a filter object that can be used to determine whether an asset belongs to the given
   * catalog, or any of the catalogs in the sub-tree rooted at the given catalog.
   *
   * \see #AssetCatalogFilter
   */
  AssetCatalogFilter create_catalog_filter(CatalogID active_catalog_id) const;

  /**
   * Create a catalog with some sensible auto-generated catalog ID.
   * The catalog will be saved to the default catalog file.
   *
   * NOTE: this does NOT mark the catalog service itself as 'has changes'. The caller is
   * responsible for that.
   *
   * \see #tag_has_unsaved_changes()
   */
  AssetCatalog *create_catalog(const AssetCatalogPath &catalog_path);

  /**
   * Delete all catalogs with the given path, and their children.
   */
  void prune_catalogs_by_path(const AssetCatalogPath &path);

  /**
   * Delete all catalogs with the same path as the identified catalog, and their children.
   * This call is the same as calling `prune_catalogs_by_path(find_catalog(catalog_id)->path)`.
   */
  void prune_catalogs_by_id(CatalogID catalog_id);

  /**
   * Update the catalog path, also updating the catalog path of all sub-catalogs.
   */
  void update_catalog_path(CatalogID catalog_id, const AssetCatalogPath &new_catalog_path);

  /**
   * May be called from multiple threads.
   */
  const AssetCatalogTree &catalog_tree();

  /** Return true only if there are no catalogs known. */
  bool is_empty() const;

  /**
   * Store the current catalogs in the undo stack.
   * This snapshots everything in the #AssetCatalogCollection. */
  void undo_push();
  /**
   * Restore the last-saved undo snapshot, pushing the current state onto the redo stack.
   * The caller is responsible for first checking that undoing is possible.
   */
  void undo();
  bool is_undo_possbile() const;
  /**
   * Restore the last-saved redo snapshot, pushing the current state onto the undo stack.
   * The caller is responsible for first checking that undoing is possible. */
  void redo();
  bool is_redo_possbile() const;

 protected:
  void load_directory_recursive(const CatalogFilePath &directory_path);
  void load_single_file(const CatalogFilePath &catalog_definition_file_path);

  /** Implementation of #write_to_disk() that doesn't clear the "has unsaved changes" tag. */
  bool write_to_disk_ex(const CatalogFilePath &blend_file_path);
  void untag_has_unsaved_changes();
  bool is_catalog_known_with_unsaved_changes(CatalogID catalog_id) const;

  /**
   * Delete catalogs, only keeping them when they are either listed in
   * \a catalogs_to_keep or have unsaved changes.
   *
   * \note Deleted catalogs are hard-deleted, i.e. they just vanish instead of
   * remembering them as "deleted".
   */
  void purge_catalogs_not_listed(const Set<CatalogID> &catalogs_to_keep);

  /**
   * Delete a catalog, without deleting any of its children and without rebuilding the catalog
   * tree. The deletion in "Soft", in the sense that the catalog pointer is moved from `catalogs_`
   * to `deleted_catalogs_`; the AssetCatalog instance itself is kept in memory. As a result, it
   * will be removed from a CDF when saved to disk.
   *
   * This is a lower-level function than #prune_catalogs_by_path.
   *
   * NOTE: this does NOT mark the catalog service itself as 'has changes'. The caller is
   * responsible for that.
   *
   * \see #tag_has_unsaved_changes()
   */
  void delete_catalog_by_id_soft(CatalogID catalog_id);

  /**
   * Hard delete a catalog. This simply removes the catalog from existence. The deletion will not
   * be remembered, and reloading the CDF will bring it back. */
  void delete_catalog_by_id_hard(CatalogID catalog_id);

  std::unique_ptr<AssetCatalogDefinitionFile> parse_catalog_file(
      const CatalogFilePath &catalog_definition_file_path);

  /**
   * Construct an in-memory catalog definition file (CDF) from the currently known catalogs.
   * This object can then be processed further before saving to disk. */
  std::unique_ptr<AssetCatalogDefinitionFile> construct_cdf_in_memory(
      const CatalogFilePath &file_path) const;

  /**
   * Find a suitable path to write a CDF to.
   *
   * This depends on the location of the blend file, and on whether a CDF already exists next to it
   * or whether the blend file is saved inside an asset library.
   */
  static CatalogFilePath find_suitable_cdf_path_for_writing(
      const CatalogFilePath &blend_file_path);

  std::unique_ptr<AssetCatalogTree> read_into_tree() const;
  /**
   * Ensure a #catalog_tree() will update the tree. Must be called whenever the contained user
   * visible catalogs change.
   * May be called from multiple threads.
   */
  void invalidate_catalog_tree();

  /**
   * For every catalog, ensure that its parent path also has a known catalog.
   */
  void create_missing_catalogs();

  /**
   * For every catalog, mark it as "dirty".
   */
  void tag_all_catalogs_as_unsaved_changes();

  /* For access by subclasses, as those will not be marked as friend by #AssetCatalogCollection. */
  const AssetCatalogDefinitionFile *get_catalog_definition_file() const;
  const OwningAssetCatalogMap &get_catalogs() const;
  const OwningAssetCatalogMap &get_deleted_catalogs() const;
};

/**
 * Asset Catalog definition, containing a symbolic ID and a path that points to a node in the
 * catalog hierarchy.
 *
 * \warning The asset system may reload catalogs, invalidating pointers. Thus it's not recommended
 *          to store pointers to asset catalogs. Store the #CatalogID instead and do a lookup when
 *          needed.
 */
class AssetCatalog {
 public:
  const CatalogID catalog_id;
  AssetCatalogPath path;
  /**
   * Simple, human-readable name for the asset catalog. This is stored on assets alongside the
   * catalog ID; the catalog ID is a UUID that is not human-readable,
   * so to avoid complete data-loss when the catalog definition file gets lost,
   * we also store a human-readable simple name for the catalog.
   *
   * It should fit in sizeof(AssetMetaData::catalog_simple_name) bytes. */
  std::string simple_name;

  struct Flags {
    /* Treat this catalog as deleted. Keeping deleted catalogs around is necessary to support
     * merging of on-disk changes with in-memory changes. */
    bool is_deleted = false;

    /* Sort this catalog first when there are multiple catalogs with the same catalog path. This
     * ensures that in a situation where missing catalogs were auto-created, and then
     * load-and-merged with a file that also has these catalogs, the first one in that file is
     * always sorted first, regardless of the sort order of its UUID. */
    bool is_first_loaded = false;

    /* Merging on-disk changes into memory will not overwrite this catalog.
     * For example, when a catalog was renamed (i.e. changed path) in this Blender session,
     * reloading the catalog definition file should not overwrite that change.
     *
     * Note that this flag is ignored when is_deleted=true; deleted catalogs that are still in
     * memory are considered "unsaved" by definition. */
    bool has_unsaved_changes = false;
  } flags;

  AssetCatalog() = delete;
  AssetCatalog(CatalogID catalog_id, const AssetCatalogPath &path, const std::string &simple_name);

  /**
   * Create a new Catalog with the given path, auto-generating a sensible catalog simple-name.
   *
   * NOTE: the given path will be cleaned up (trailing spaces removed, etc.), so the returned
   * `AssetCatalog`'s path differ from the given one.
   */
  static std::unique_ptr<AssetCatalog> from_path(const AssetCatalogPath &path);

  /** Make a new simple name for the catalog, based on its path. */
  void simple_name_refresh();

 protected:
  /** Generate a sensible catalog ID for the given path. */
  static std::string sensible_simple_name_for_path(const AssetCatalogPath &path);
};

/** Comparator for asset catalogs, ordering by (path, first_seen, UUID). */
struct AssetCatalogLessThan {
  bool operator()(const AssetCatalog *lhs, const AssetCatalog *rhs) const
  {
    if (lhs->path != rhs->path) {
      return lhs->path < rhs->path;
    }

    if (lhs->flags.is_first_loaded != rhs->flags.is_first_loaded) {
      return lhs->flags.is_first_loaded;
    }

    return lhs->catalog_id < rhs->catalog_id;
  }
};

/**
 * Set that stores catalogs ordered by (path, UUID).
 * Being a set, duplicates are removed. The catalog's simple name is ignored in this. */
using AssetCatalogOrderedSet = std::set<const AssetCatalog *, AssetCatalogLessThan>;
using MutableAssetCatalogOrderedSet = std::set<AssetCatalog *, AssetCatalogLessThan>;

/**
 * Filter that can determine whether an asset should be visible or not, based on its catalog ID.
 *
 * \see AssetCatalogService::create_catalog_filter()
 */
class AssetCatalogFilter {
  const Set<CatalogID> matching_catalog_ids_;
  const Set<CatalogID> known_catalog_ids_;

  friend AssetCatalogService;

 public:
  bool contains(CatalogID asset_catalog_id) const;

  /* So that all unknown catalogs can be shown under "Unassigned". */
  bool is_known(CatalogID asset_catalog_id) const;

 protected:
  explicit AssetCatalogFilter(Set<CatalogID> &&matching_catalog_ids,
                              Set<CatalogID> &&known_catalog_ids);
};

}  // namespace blender::asset_system
