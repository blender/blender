/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <iostream>
#include <set>

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "asset_catalog_collection.hh"
#include "asset_catalog_definition_file.hh"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"

/* For S_ISREG() and S_ISDIR() on Windows. */
#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "asset_library_service.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"asset.catalog"};

namespace blender::asset_system {

const CatalogFilePath AssetCatalogService::DEFAULT_CATALOG_FILENAME = "blender_assets.cats.txt";

AssetCatalogService::AssetCatalogService(const CatalogFilePath &asset_library_root)
    : catalog_collection_(std::make_unique<AssetCatalogCollection>()),
      asset_library_root_(asset_library_root)
{
}

AssetCatalogService::AssetCatalogService(read_only_tag /*unused*/) : AssetCatalogService()
{
  const_cast<bool &>(is_read_only_) = true;
}

void AssetCatalogService::tag_has_unsaved_changes(AssetCatalog *edited_catalog)
{
  BLI_assert(!is_read_only_);

  if (edited_catalog) {
    edited_catalog->flags.has_unsaved_changes = true;
  }
  BLI_assert(catalog_collection_);
  catalog_collection_->has_unsaved_changes_ = true;
}

void AssetCatalogService::untag_has_unsaved_changes()
{
  BLI_assert(catalog_collection_);
  catalog_collection_->has_unsaved_changes_ = false;

  /* TODO(Sybren): refactor; this is more like "post-write cleanup" than "remove a tag" code. */

  /* Forget about any deleted catalogs. */
  if (catalog_collection_->catalog_definition_file_) {
    for (CatalogID catalog_id : catalog_collection_->deleted_catalogs_.keys()) {
      catalog_collection_->catalog_definition_file_->forget(catalog_id);
    }
  }
  catalog_collection_->deleted_catalogs_.clear();

  /* Mark all remaining catalogs as "without unsaved changes". */
  for (auto &catalog_uptr : catalog_collection_->catalogs_.values()) {
    catalog_uptr->flags.has_unsaved_changes = false;
  }
}

bool AssetCatalogService::has_unsaved_changes() const
{
  BLI_assert(catalog_collection_);
  return catalog_collection_->has_unsaved_changes_;
}

bool AssetCatalogService::is_read_only() const
{
  return is_read_only_;
}

void AssetCatalogService::tag_all_catalogs_as_unsaved_changes()
{
  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    catalog->flags.has_unsaved_changes = true;
  }
  catalog_collection_->has_unsaved_changes_ = true;
}

bool AssetCatalogService::is_empty() const
{
  BLI_assert(catalog_collection_);
  return catalog_collection_->catalogs_.is_empty();
}

const OwningAssetCatalogMap &AssetCatalogService::get_catalogs() const
{
  return catalog_collection_->catalogs_;
}
const OwningAssetCatalogMap &AssetCatalogService::get_deleted_catalogs() const
{
  return catalog_collection_->deleted_catalogs_;
}

const AssetCatalogDefinitionFile *AssetCatalogService::get_catalog_definition_file() const
{
  return catalog_collection_->catalog_definition_file_.get();
}

AssetCatalog *AssetCatalogService::find_catalog(CatalogID catalog_id) const
{
  const std::unique_ptr<AssetCatalog> *catalog_uptr_ptr =
      catalog_collection_->catalogs_.lookup_ptr(catalog_id);
  if (catalog_uptr_ptr == nullptr) {
    return nullptr;
  }
  return catalog_uptr_ptr->get();
}

AssetCatalog *AssetCatalogService::find_catalog_by_path(const AssetCatalogPath &path) const
{
  /* Use an AssetCatalogOrderedSet to find the 'best' catalog for this path. This will be the first
   * one loaded from disk, or if that does not exist the one with the lowest UUID. This ensures
   * stable, predictable results. */
  MutableAssetCatalogOrderedSet ordered_catalogs;

  for (const auto &catalog : catalog_collection_->catalogs_.values()) {
    if (catalog->path == path) {
      ordered_catalogs.insert(catalog.get());
    }
  }

  if (ordered_catalogs.empty()) {
    return nullptr;
  }

  MutableAssetCatalogOrderedSet::iterator best_choice_it = ordered_catalogs.begin();
  return *best_choice_it;
}

bool AssetCatalogService::is_catalog_known(CatalogID catalog_id) const
{
  BLI_assert(catalog_collection_);
  return catalog_collection_->catalogs_.contains(catalog_id);
}

AssetCatalogFilter AssetCatalogService::create_catalog_filter(
    const CatalogID active_catalog_id) const
{
  Set<CatalogID> matching_catalog_ids;
  Set<CatalogID> known_catalog_ids;
  matching_catalog_ids.add(active_catalog_id);

  const AssetCatalog *active_catalog = this->find_catalog(active_catalog_id);

  /* This cannot just iterate over tree items to get all the required data, because tree items only
   * represent single UUIDs. It could be used to get the main UUIDs of the children, though, and
   * then only do an exact match on the path (instead of the more complex `is_contained_in()`
   * call). Without an extra indexed-by-path acceleration structure, this is still going to require
   * a linear search, though. */
  for (const auto &catalog_uptr : catalog_collection_->catalogs_.values()) {
    if (active_catalog && catalog_uptr->path.is_contained_in(active_catalog->path)) {
      matching_catalog_ids.add(catalog_uptr->catalog_id);
    }
    known_catalog_ids.add(catalog_uptr->catalog_id);
  }

  return AssetCatalogFilter(std::move(matching_catalog_ids), std::move(known_catalog_ids));
}

void AssetCatalogService::delete_catalog_by_id_soft(const CatalogID catalog_id)
{
  std::unique_ptr<AssetCatalog> *catalog_uptr_ptr = catalog_collection_->catalogs_.lookup_ptr(
      catalog_id);
  if (catalog_uptr_ptr == nullptr) {
    /* Catalog cannot be found, which is fine. */
    return;
  }

  /* Mark the catalog as deleted. */
  AssetCatalog *catalog = catalog_uptr_ptr->get();
  catalog->flags.is_deleted = true;

  /* Move ownership from catalog_collection_->catalogs_ to catalog_collection_->deleted_catalogs_.
   */
  catalog_collection_->deleted_catalogs_.add(catalog_id, std::move(*catalog_uptr_ptr));

  /* The catalog can now be removed from the map without freeing the actual AssetCatalog. */
  catalog_collection_->catalogs_.remove(catalog_id);
}

void AssetCatalogService::delete_catalog_by_id_hard(CatalogID catalog_id)
{
  catalog_collection_->catalogs_.remove(catalog_id);
  catalog_collection_->deleted_catalogs_.remove(catalog_id);

  /* TODO(@sybren): adjust this when supporting multiple CDFs. */
  catalog_collection_->catalog_definition_file_->forget(catalog_id);
}

void AssetCatalogService::prune_catalogs_by_path(const AssetCatalogPath &path)
{
  /* Build a collection of catalog IDs to delete. */
  Set<CatalogID> catalogs_to_delete;
  for (const auto &catalog_uptr : catalog_collection_->catalogs_.values()) {
    const AssetCatalog *cat = catalog_uptr.get();
    if (cat->path.is_contained_in(path)) {
      catalogs_to_delete.add(cat->catalog_id);
    }
  }

  /* Delete the catalogs. */
  for (const CatalogID cat_id : catalogs_to_delete) {
    this->delete_catalog_by_id_soft(cat_id);
  }

  this->invalidate_catalog_tree();
  AssetLibraryService::get()->tag_all_library_catalogs_dirty();
}

void AssetCatalogService::prune_catalogs_by_id(const CatalogID catalog_id)
{
  const AssetCatalog *catalog = find_catalog(catalog_id);
  BLI_assert_msg(catalog, "trying to prune asset catalogs by the path of a non-existent catalog");
  if (!catalog) {
    return;
  }
  this->prune_catalogs_by_path(catalog->path);
}

void AssetCatalogService::update_catalog_path(const CatalogID catalog_id,
                                              const AssetCatalogPath &new_catalog_path)
{
  AssetCatalog *renamed_cat = this->find_catalog(catalog_id);
  const AssetCatalogPath old_cat_path = renamed_cat->path;

  for (auto &catalog_uptr : catalog_collection_->catalogs_.values()) {
    AssetCatalog *cat = catalog_uptr.get();

    const AssetCatalogPath new_path = cat->path.rebase(old_cat_path, new_catalog_path);
    if (!new_path) {
      continue;
    }
    cat->path = new_path;
    cat->simple_name_refresh();
    this->tag_has_unsaved_changes(cat);

    /* TODO(Sybren): go over all assets that are assigned to this catalog, defined in the current
     * blend file, and update the catalog simple name stored there. */
  }

  this->create_missing_catalogs();
  this->invalidate_catalog_tree();
  AssetLibraryService::get()->tag_all_library_catalogs_dirty();
}

AssetCatalog *AssetCatalogService::create_catalog(const AssetCatalogPath &catalog_path)
{
  std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path(catalog_path);
  catalog->flags.has_unsaved_changes = true;

  /* So we can std::move(catalog) and still use the non-owning pointer: */
  AssetCatalog *const catalog_ptr = catalog.get();

  /* TODO(@sybren): move the `AssetCatalog::from_path()` function to another place, that can reuse
   * catalogs when a catalog with the given path is already known, and avoid duplicate catalog IDs.
   */
  BLI_assert_msg(!catalog_collection_->catalogs_.contains(catalog->catalog_id),
                 "duplicate catalog ID not supported");
  catalog_collection_->catalogs_.add_new(catalog->catalog_id, std::move(catalog));

  if (catalog_collection_->catalog_definition_file_) {
    /* Ensure the new catalog gets written to disk at some point. If there is no CDF in memory yet,
     * it's enough to have the catalog known to the service as it'll be saved to a new file. */
    catalog_collection_->catalog_definition_file_->add_new(catalog_ptr);
  }

  this->invalidate_catalog_tree();
  AssetLibraryService::get()->tag_all_library_catalogs_dirty();

  return catalog_ptr;
}

static std::string asset_definition_default_file_path_from_dir(StringRef asset_library_root)
{
  char file_path[PATH_MAX];
  BLI_path_join(file_path,
                sizeof(file_path),
                asset_library_root.data(),
                AssetCatalogService::DEFAULT_CATALOG_FILENAME.data());
  return file_path;
}

void AssetCatalogService::load_from_disk()
{
  this->load_from_disk(asset_library_root_);
}

void AssetCatalogService::load_from_disk(const CatalogFilePath &file_or_directory_path)
{
  BLI_stat_t status;
  if (BLI_stat(file_or_directory_path.data(), &status) == -1) {
    /* TODO(@sybren): throw an appropriate exception. */
    CLOG_WARN(&LOG, "path not found: %s", file_or_directory_path.data());
    return;
  }

  if (S_ISREG(status.st_mode)) {
    this->load_single_file(file_or_directory_path);
  }
  else if (S_ISDIR(status.st_mode)) {
    this->load_directory_recursive(file_or_directory_path);
  }
  else {
    /* TODO(@sybren): throw an appropriate exception. */
  }

  /* TODO: Should there be a sanitize step? E.g. to remove catalogs with identical paths? */

  this->create_missing_catalogs();
  this->invalidate_catalog_tree();
}

void AssetCatalogService::add_from_existing(
    const AssetCatalogService &other_service,
    AssetCatalogCollection::OnDuplicateCatalogIdFn on_duplicate_items)
{
  catalog_collection_->add_catalogs_from_existing(*other_service.catalog_collection_,
                                                  on_duplicate_items);
}

void AssetCatalogService::load_directory_recursive(const CatalogFilePath &directory_path)
{
  /* TODO(@sybren): implement proper multi-file support. For now, just load
   * the default file if it is there. */
  CatalogFilePath file_path = asset_definition_default_file_path_from_dir(directory_path);

  if (!BLI_exists(file_path.data())) {
    /* No file to be loaded is perfectly fine. */
    CLOG_DEBUG(&LOG, "path not found: %s", file_path.data());
    return;
  }

  this->load_single_file(file_path);
}

void AssetCatalogService::load_single_file(const CatalogFilePath &catalog_definition_file_path)
{
  /* TODO(@sybren): check that #catalog_definition_file_path is contained in #asset_library_root_,
   * otherwise some assumptions may fail. */
  std::unique_ptr<AssetCatalogDefinitionFile> cdf = parse_catalog_file(
      catalog_definition_file_path);

  BLI_assert_msg(!catalog_collection_->catalog_definition_file_,
                 "Only loading of a single catalog definition file is supported.");
  catalog_collection_->catalog_definition_file_ = std::move(cdf);
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogService::parse_catalog_file(
    const CatalogFilePath &catalog_definition_file_path)
{
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>(catalog_definition_file_path);

  /* TODO(Sybren): this might have to move to a higher level when supporting multiple CDFs. */
  Set<AssetCatalogPath> seen_paths;

  auto catalog_parsed_callback = [this, catalog_definition_file_path, &seen_paths](
                                     std::unique_ptr<AssetCatalog> catalog) {
    if (catalog_collection_->catalogs_.contains(catalog->catalog_id)) {
      /* TODO(@sybren): apparently another CDF was already loaded. This is not supported yet. */
      std::cerr << catalog_definition_file_path << ": multiple definitions of catalog "
                << catalog->catalog_id << " in multiple files, ignoring this one." << std::endl;
      /* Don't store 'catalog'; unique_ptr will free its memory. */
      return false;
    }

    catalog->flags.is_first_loaded = seen_paths.add(catalog->path);

    /* The AssetCatalog pointer is now owned by the AssetCatalogService. */
    catalog_collection_->catalogs_.add_new(catalog->catalog_id, std::move(catalog));
    return true;
  };

  cdf->parse_catalog_file(cdf->file_path, catalog_parsed_callback);

  return cdf;
}

void AssetCatalogService::reload_catalogs()
{
  /* TODO(Sybren): expand to support multiple CDFs. */
  AssetCatalogDefinitionFile *const cdf = catalog_collection_->catalog_definition_file_.get();
  if (!cdf || cdf->file_path.empty() || !BLI_is_file(cdf->file_path.c_str())) {
    return;
  }

  /* Keeps track of the catalog IDs that are seen in the CDF, so that we also know what was deleted
   * from the file on disk. */
  Set<CatalogID> cats_in_file;

  auto catalog_parsed_callback = [this, &cats_in_file](std::unique_ptr<AssetCatalog> catalog) {
    const CatalogID catalog_id = catalog->catalog_id;
    cats_in_file.add(catalog_id);

    const bool should_skip = this->is_catalog_known_with_unsaved_changes(catalog_id);
    if (should_skip) {
      /* Do not overwrite unsaved local changes. */
      return false;
    }

    /* This is either a new catalog, or we can just replace the in-memory one with the newly loaded
     * one. */
    catalog_collection_->catalogs_.add_overwrite(catalog_id, std::move(catalog));
    return true;
  };

  cdf->parse_catalog_file(cdf->file_path, catalog_parsed_callback);
  this->purge_catalogs_not_listed(cats_in_file);
  this->create_missing_catalogs();
  this->invalidate_catalog_tree();
}

void AssetCatalogService::purge_catalogs_not_listed(const Set<CatalogID> &catalogs_to_keep)
{
  Set<CatalogID> cats_to_remove;
  for (CatalogID cat_id : this->catalog_collection_->catalogs_.keys()) {
    if (catalogs_to_keep.contains(cat_id)) {
      continue;
    }
    if (this->is_catalog_known_with_unsaved_changes(cat_id)) {
      continue;
    }
    /* This catalog is not on disk, but also not modified, so get rid of it. */
    cats_to_remove.add(cat_id);
  }

  for (CatalogID cat_id : cats_to_remove) {
    this->delete_catalog_by_id_hard(cat_id);
  }
}

bool AssetCatalogService::is_catalog_known_with_unsaved_changes(const CatalogID catalog_id) const
{
  if (catalog_collection_->deleted_catalogs_.contains(catalog_id)) {
    /* Deleted catalogs are always considered modified, by definition. */
    return true;
  }

  const std::unique_ptr<AssetCatalog> *catalog_uptr_ptr =
      catalog_collection_->catalogs_.lookup_ptr(catalog_id);
  if (!catalog_uptr_ptr) {
    /* Catalog is unknown. */
    return false;
  }

  const bool has_unsaved_changes = (*catalog_uptr_ptr)->flags.has_unsaved_changes;
  return has_unsaved_changes;
}

bool AssetCatalogService::write_to_disk(const CatalogFilePath &blend_file_path)
{
  BLI_assert(!is_read_only_);

  if (!this->write_to_disk_ex(blend_file_path)) {
    return false;
  }

  this->untag_has_unsaved_changes();
  this->invalidate_catalog_tree();
  return true;
}

bool AssetCatalogService::write_to_disk_ex(const CatalogFilePath &blend_file_path)
{
  /* TODO(Sybren): expand to support multiple CDFs. */

  /* - Already loaded a CDF from disk? -> Only write to that file when there were actual changes.
   * This prevents touching the file, which can cause issues when multiple Blender instances are
   * accessing the same file (like on shared storage, Syncthing, etc.). See #111576.
   */
  if (catalog_collection_->catalog_definition_file_) {
    /* Always sync with what's on disk. */
    this->reload_catalogs();

    if (!this->has_unsaved_changes() &&
        catalog_collection_->catalog_definition_file_->exists_on_disk())
    {
      return true;
    }
    return catalog_collection_->catalog_definition_file_->write_to_disk();
  }

  if (catalog_collection_->is_empty()) {
    /* Avoid saving anything, when there is nothing to save. */
    return true; /* Writing nothing when there is nothing to write is still a success. */
  }

  const CatalogFilePath cdf_path_to_write = find_suitable_cdf_path_for_writing(blend_file_path);
  catalog_collection_->catalog_definition_file_ = construct_cdf_in_memory(cdf_path_to_write);
  this->reload_catalogs();
  return catalog_collection_->catalog_definition_file_->write_to_disk();
}

void AssetCatalogService::prepare_to_merge_on_write()
{
  /* TODO(Sybren): expand to support multiple CDFs. */

  if (!catalog_collection_->catalog_definition_file_) {
    /* There is no CDF connected, so it's a no-op. */
    return;
  }

  /* Remove any association with the CDF, so that a new location will be chosen
   * when the blend file is saved. */
  catalog_collection_->catalog_definition_file_.reset();

  /* Mark all in-memory catalogs as "dirty", to force them to be kept around on
   * the next "load-merge-write" cycle. */
  this->tag_all_catalogs_as_unsaved_changes();
}

CatalogFilePath AssetCatalogService::find_suitable_cdf_path_for_writing(
    const CatalogFilePath &blend_file_path)
{
  BLI_assert_msg(!blend_file_path.empty(),
                 "A non-empty .blend file path is required to be able to determine where the "
                 "catalog definition file should be put");

  /* Ask the asset library API for an appropriate location. */
  const std::string suitable_root_path = AS_asset_library_find_suitable_root_path_from_path(
      blend_file_path);
  if (!suitable_root_path.empty()) {
    char asset_lib_cdf_path[PATH_MAX];
    BLI_path_join(asset_lib_cdf_path,
                  sizeof(asset_lib_cdf_path),
                  suitable_root_path.c_str(),
                  DEFAULT_CATALOG_FILENAME.c_str());
    return asset_lib_cdf_path;
  }

  /* Determine the default CDF path in the same directory of the blend file. */
  char blend_dir_path[PATH_MAX];
  BLI_path_split_dir_part(blend_file_path.c_str(), blend_dir_path, sizeof(blend_dir_path));
  const CatalogFilePath cdf_path_next_to_blend = asset_definition_default_file_path_from_dir(
      blend_dir_path);
  return cdf_path_next_to_blend;
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogService::construct_cdf_in_memory(
    const CatalogFilePath &file_path) const
{
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>(file_path);

  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    cdf->add_new(catalog.get());
  }

  return cdf;
}

std::unique_ptr<AssetCatalogTree> AssetCatalogService::read_into_tree() const
{
  auto tree = std::make_unique<AssetCatalogTree>();

  /* Go through the catalogs, insert each path component into the tree where needed. */
  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    tree->insert_item(*catalog);
  }

  return tree;
}

void AssetCatalogService::invalidate_catalog_tree()
{
  std::lock_guard lock{catalog_tree_mutex_};
  this->catalog_tree_ = nullptr;
}

const AssetCatalogTree &AssetCatalogService::catalog_tree()
{
  std::lock_guard lock{catalog_tree_mutex_};
  if (!catalog_tree_) {
    /* Ensure all catalog paths lead to valid catalogs. This is important for the catalog tree to
     * be usable, e.g. it makes sure every item in the tree maps to an actual catalog. */
    this->create_missing_catalogs();

    catalog_tree_ = read_into_tree();
  }
  return *catalog_tree_;
}

void AssetCatalogService::create_missing_catalogs()
{
  /* Construct an ordered set of paths to check, so that parents are ordered before children. */
  std::set<AssetCatalogPath> paths_to_check;
  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    paths_to_check.insert(catalog->path);
  }

  std::set<AssetCatalogPath> seen_paths;
  /* The empty parent should never be created, so always be considered "seen". */
  seen_paths.insert(AssetCatalogPath(""));

  /* Find and create missing direct parents (so ignoring parents-of-parents). */
  while (!paths_to_check.empty()) {
    /* Pop the first path of the queue. */
    const AssetCatalogPath path = *paths_to_check.begin();
    paths_to_check.erase(paths_to_check.begin());

    if (seen_paths.find(path) != seen_paths.end()) {
      /* This path has been seen already, so it can be ignored. */
      continue;
    }
    seen_paths.insert(path);

    const AssetCatalogPath parent_path = path.parent();
    if (seen_paths.find(parent_path) != seen_paths.end()) {
      /* The parent exists, continue to the next path. */
      continue;
    }

    /* The parent doesn't exist, so create it and queue it up for checking its parent. */
    AssetCatalog *parent_catalog = this->create_catalog(parent_path);
    parent_catalog->flags.has_unsaved_changes = true;

    paths_to_check.insert(parent_path);
  }

  /* TODO(Sybren): bind the newly created catalogs to a CDF, if we know about it. */
}

bool AssetCatalogService::is_undo_possbile() const
{
  return !undo_snapshots_.is_empty();
}

bool AssetCatalogService::is_redo_possbile() const
{
  return !redo_snapshots_.is_empty();
}

void AssetCatalogService::undo()
{
  BLI_assert_msg(is_undo_possbile(), "Undo stack is empty");

  redo_snapshots_.append(std::move(catalog_collection_));
  catalog_collection_ = undo_snapshots_.pop_last();
  this->create_missing_catalogs();
  this->invalidate_catalog_tree();
  AssetLibraryService::get()->tag_all_library_catalogs_dirty();
}

void AssetCatalogService::redo()
{
  BLI_assert(!is_read_only_);
  BLI_assert_msg(is_redo_possbile(), "Redo stack is empty");

  undo_snapshots_.append(std::move(catalog_collection_));
  catalog_collection_ = redo_snapshots_.pop_last();
  this->create_missing_catalogs();
  this->invalidate_catalog_tree();
  AssetLibraryService::get()->tag_all_library_catalogs_dirty();
}

void AssetCatalogService::undo_push()
{
  BLI_assert(!is_read_only_);
  std::unique_ptr<AssetCatalogCollection> snapshot = catalog_collection_->deep_copy();
  undo_snapshots_.append(std::move(snapshot));
  redo_snapshots_.clear();
}

/* ---------------------------------------------------------------------- */

AssetCatalog::AssetCatalog(const CatalogID catalog_id,
                           const AssetCatalogPath &path,
                           const std::string &simple_name)
    : catalog_id(catalog_id), path(path), simple_name(simple_name)
{
}

std::unique_ptr<AssetCatalog> AssetCatalog::from_path(const AssetCatalogPath &path)
{
  const AssetCatalogPath clean_path = path.cleanup();
  const CatalogID cat_id = BLI_uuid_generate_random();
  const std::string simple_name = sensible_simple_name_for_path(clean_path);
  auto catalog = std::make_unique<AssetCatalog>(cat_id, clean_path, simple_name);
  return catalog;
}

void AssetCatalog::simple_name_refresh()
{
  this->simple_name = sensible_simple_name_for_path(this->path);
}

std::string AssetCatalog::sensible_simple_name_for_path(const AssetCatalogPath &path)
{
  std::string name = path.str();
  std::replace(name.begin(), name.end(), AssetCatalogPath::SEPARATOR, '-');
  if (name.length() < MAX_NAME - 1) {
    return name;
  }

  /* Trim off the start of the path, as that's the most generic part and thus contains the least
   * information. */
  return "..." + name.substr(name.length() - 60);
}

/* ---------------------------------------------------------------------- */

AssetCatalogFilter::AssetCatalogFilter(Set<CatalogID> &&matching_catalog_ids,
                                       Set<CatalogID> &&known_catalog_ids)
    : matching_catalog_ids_(std::move(matching_catalog_ids)),
      known_catalog_ids_(std::move(known_catalog_ids))
{
}

bool AssetCatalogFilter::contains(const CatalogID asset_catalog_id) const
{
  return matching_catalog_ids_.contains(asset_catalog_id);
}

bool AssetCatalogFilter::is_known(const CatalogID asset_catalog_id) const
{
  if (BLI_uuid_is_nil(asset_catalog_id)) {
    return false;
  }
  return known_catalog_ids_.contains(asset_catalog_id);
}

}  // namespace blender::asset_system
