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
 */

/** \file
 * \ingroup bke
 */

#pragma once

#ifndef __cplusplus
#  error This is a C++ header. The C interface is yet to be implemented/designed.
#endif

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_uuid.h"
#include "BLI_vector.hh"

#include "BKE_asset_catalog_path.hh"

#include <map>
#include <memory>
#include <set>
#include <string>

namespace blender::bke {

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
 public:
  static const CatalogFilePath DEFAULT_CATALOG_FILENAME;

 public:
  AssetCatalogService();
  explicit AssetCatalogService(const CatalogFilePath &asset_library_root);

  /**
   * Set tag indicating that some catalog modifications are unsaved, which could
   * get lost on exit. This tag is not set by internal catalog code, the catalog
   * service user is responsible for it. It is cleared by #write_to_disk().
   *
   * This "dirty" state is tracked per catalog, so that it's possible to gracefully load changes
   * from disk. Any catalog with unsaved changes will not be overwritten by on-disk changes. */
  void tag_has_unsaved_changes(AssetCatalog *edited_catalog);
  bool has_unsaved_changes() const;

  /** Load asset catalog definitions from the files found in the asset library. */
  void load_from_disk();
  /** Load asset catalog definitions from the given file or directory. */
  void load_from_disk(const CatalogFilePath &file_or_directory_path);

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

  /** Create a catalog with some sensible auto-generated catalog ID.
   * The catalog will be saved to the default catalog file.*/
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

  AssetCatalogTree *get_catalog_tree();

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
  std::unique_ptr<AssetCatalogCollection> catalog_collection_;
  std::unique_ptr<AssetCatalogTree> catalog_tree_ = std::make_unique<AssetCatalogTree>();
  CatalogFilePath asset_library_root_;

  Vector<std::unique_ptr<AssetCatalogCollection>> undo_snapshots_;
  Vector<std::unique_ptr<AssetCatalogCollection>> redo_snapshots_;

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
      const CatalogFilePath &file_path);

  /**
   * Find a suitable path to write a CDF to.
   *
   * This depends on the location of the blend file, and on whether a CDF already exists next to it
   * or whether the blend file is saved inside an asset library.
   */
  static CatalogFilePath find_suitable_cdf_path_for_writing(
      const CatalogFilePath &blend_file_path);

  std::unique_ptr<AssetCatalogTree> read_into_tree();
  void rebuild_tree();

  /**
   * For every catalog, ensure that its parent path also has a known catalog.
   */
  void create_missing_catalogs();

  /* For access by subclasses, as those will not be marked as friend by #AssetCatalogCollection. */
  AssetCatalogDefinitionFile *get_catalog_definition_file();
  OwningAssetCatalogMap &get_catalogs();
  OwningAssetCatalogMap &get_deleted_catalogs();
};

/**
 * All catalogs that are owned by a single asset library, and managed by a single instance of
 * #AssetCatalogService. The undo system for asset catalog edits contains historical copies of this
 * struct.
 */
class AssetCatalogCollection {
  friend AssetCatalogService;

 public:
  AssetCatalogCollection() = default;
  AssetCatalogCollection(const AssetCatalogCollection &other) = delete;
  AssetCatalogCollection(AssetCatalogCollection &&other) noexcept = default;

  std::unique_ptr<AssetCatalogCollection> deep_copy() const;

 protected:
  /** All catalogs known, except the known-but-deleted ones. */
  OwningAssetCatalogMap catalogs_;

  /** Catalogs that have been deleted. They are kept around so that the load-merge-save of catalog
   * definition files can actually delete them if they already existed on disk (instead of the
   * merge operation resurrecting them). */
  OwningAssetCatalogMap deleted_catalogs_;

  /* For now only a single catalog definition file is supported.
   * The aim is to support an arbitrary number of such files per asset library in the future. */
  std::unique_ptr<AssetCatalogDefinitionFile> catalog_definition_file_;

  /** Whether any of the catalogs have unsaved changes. */
  bool has_unsaved_changes_ = false;

  static OwningAssetCatalogMap copy_catalog_map(const OwningAssetCatalogMap &orig);
};

/**
 * Representation of a catalog path in the #AssetCatalogTree.
 */
class AssetCatalogTreeItem {
  friend class AssetCatalogTree;

 public:
  /** Container for child items. Uses a #std::map to keep items ordered by their name (i.e. their
   * last catalog component). */
  using ChildMap = std::map<std::string, AssetCatalogTreeItem>;
  using ItemIterFn = FunctionRef<void(AssetCatalogTreeItem &)>;

  AssetCatalogTreeItem(StringRef name,
                       CatalogID catalog_id,
                       StringRef simple_name,
                       const AssetCatalogTreeItem *parent = nullptr);

  CatalogID get_catalog_id() const;
  StringRefNull get_simple_name() const;
  StringRefNull get_name() const;
  bool has_unsaved_changes() const;
  /** Return the full catalog path, defined as the name of this catalog prefixed by the full
   * catalog path of its parent and a separator. */
  AssetCatalogPath catalog_path() const;
  int count_parents() const;
  bool has_children() const;

  /** Iterate over children calling \a callback for each of them, but do not recurse into their
   * children. */
  void foreach_child(const ItemIterFn callback);

 protected:
  /** Child tree items, ordered by their names. */
  ChildMap children_;
  /** The user visible name of this component. */
  CatalogPathComponent name_;
  CatalogID catalog_id_;
  /** Copy of #AssetCatalog::simple_name. */
  std::string simple_name_;
  /** Copy of #AssetCatalog::flags.has_unsaved_changes. */
  bool has_unsaved_changes_ = false;

  /** Pointer back to the parent item. Used to reconstruct the hierarchy from an item (e.g. to
   * build a path). */
  const AssetCatalogTreeItem *parent_ = nullptr;

 private:
  static void foreach_item_recursive(ChildMap &children_, ItemIterFn callback);
};

/**
 * A representation of the catalog paths as tree structure. Each component of the catalog tree is
 * represented by an #AssetCatalogTreeItem. The last path component of an item is used as its name,
 * which may also be shown to the user.
 * An item can not have multiple children with the same name. That means the name uniquely
 * identifies an item within its parent.
 *
 * There is no single root tree element, the #AssetCatalogTree instance itself represents the root.
 */
class AssetCatalogTree {
  using ChildMap = AssetCatalogTreeItem::ChildMap;
  using ItemIterFn = AssetCatalogTreeItem::ItemIterFn;

 public:
  /** Ensure an item representing \a path is in the tree, adding it if necessary. */
  void insert_item(const AssetCatalog &catalog);

  void foreach_item(const AssetCatalogTreeItem::ItemIterFn callback);
  /** Iterate over root items calling \a callback for each of them, but do not recurse into their
   * children. */
  void foreach_root_item(const ItemIterFn callback);

 protected:
  /** Child tree items, ordered by their names. */
  ChildMap root_items_;
};

/** Keeps track of which catalogs are defined in a certain file on disk.
 * Only contains non-owning pointers to the #AssetCatalog instances, so ensure the lifetime of this
 * class is shorter than that of the #`AssetCatalog`s themselves. */
class AssetCatalogDefinitionFile {
 public:
  /* For now this is the only version of the catalog definition files that is supported.
   * Later versioning code may be added to handle older files. */
  const static int SUPPORTED_VERSION;
  const static std::string VERSION_MARKER;
  const static std::string HEADER;

  CatalogFilePath file_path;

  AssetCatalogDefinitionFile() = default;

  /**
   * Write the catalog definitions to the same file they were read from.
   * Return true when the file was written correctly, false when there was a problem.
   */
  bool write_to_disk() const;
  /**
   * Write the catalog definitions to an arbitrary file path.
   *
   * Any existing file is backed up to "filename~". Any previously existing backup is overwritten.
   *
   * Return true when the file was written correctly, false when there was a problem.
   */
  bool write_to_disk(const CatalogFilePath &dest_file_path) const;

  bool contains(CatalogID catalog_id) const;
  /** Add a catalog, overwriting the one with the same catalog ID. */
  void add_overwrite(AssetCatalog *catalog);
  /** Add a new catalog. Undefined behavior if a catalog with the same ID was already added. */
  void add_new(AssetCatalog *catalog);

  /** Remove the catalog from the collection of catalogs stored in this file. */
  void forget(CatalogID catalog_id);

  using AssetCatalogParsedFn = FunctionRef<bool(std::unique_ptr<AssetCatalog>)>;
  void parse_catalog_file(const CatalogFilePath &catalog_definition_file_path,
                          AssetCatalogParsedFn callback);

  std::unique_ptr<AssetCatalogDefinitionFile> copy_and_remap(
      const OwningAssetCatalogMap &catalogs, const OwningAssetCatalogMap &deleted_catalogs) const;

 protected:
  /* Catalogs stored in this file. They are mapped by ID to make it possible to query whether a
   * catalog is already known, without having to find the corresponding `AssetCatalog*`. */
  Map<CatalogID, AssetCatalog *> catalogs_;

  bool parse_version_line(StringRef line);
  std::unique_ptr<AssetCatalog> parse_catalog_line(StringRef line);

  /**
   * Write the catalog definitions to the given file path.
   * Return true when the file was written correctly, false when there was a problem.
   */
  bool write_to_disk_unsafe(const CatalogFilePath &dest_file_path) const;
  bool ensure_directory_exists(const CatalogFilePath directory_path) const;
};

/** Asset Catalog definition, containing a symbolic ID and a path that points to a node in the
 * catalog hierarchy. */
class AssetCatalog {
 public:
  AssetCatalog() = default;
  AssetCatalog(CatalogID catalog_id, const AssetCatalogPath &path, const std::string &simple_name);

  CatalogID catalog_id;
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
 public:
  bool contains(CatalogID asset_catalog_id) const;

  /* So that all unknown catalogs can be shown under "Unassigned". */
  bool is_known(CatalogID asset_catalog_id) const;

 protected:
  friend AssetCatalogService;
  const Set<CatalogID> matching_catalog_ids;
  const Set<CatalogID> known_catalog_ids;

  explicit AssetCatalogFilter(Set<CatalogID> &&matching_catalog_ids,
                              Set<CatalogID> &&known_catalog_ids);
};

}  // namespace blender::bke
