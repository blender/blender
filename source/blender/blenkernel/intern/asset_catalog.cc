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

#include <fstream>
#include <set>

#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.h"

#include "BLI_fileops.hh"
#include "BLI_path_util.h"

/* For S_ISREG() and S_ISDIR() on Windows. */
#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.asset_service"};

namespace blender::bke {

const CatalogFilePath AssetCatalogService::DEFAULT_CATALOG_FILENAME = "blender_assets.cats.txt";

const int AssetCatalogDefinitionFile::SUPPORTED_VERSION = 1;
const std::string AssetCatalogDefinitionFile::VERSION_MARKER = "VERSION ";

const std::string AssetCatalogDefinitionFile::HEADER =
    "# This is an Asset Catalog Definition file for Blender.\n"
    "#\n"
    "# Empty lines and lines starting with `#` will be ignored.\n"
    "# The first non-ignored line should be the version indicator.\n"
    "# Other lines are of the format \"UUID:catalog/path/for/assets:simple catalog name\"\n";

AssetCatalogService::AssetCatalogService()
    : catalog_collection_(std::make_unique<AssetCatalogCollection>())
{
}

AssetCatalogService::AssetCatalogService(const CatalogFilePath &asset_library_root)
    : catalog_collection_(std::make_unique<AssetCatalogCollection>()),
      asset_library_root_(asset_library_root)
{
}

void AssetCatalogService::tag_has_unsaved_changes(AssetCatalog *edited_catalog)
{
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

OwningAssetCatalogMap &AssetCatalogService::get_catalogs()
{
  return catalog_collection_->catalogs_;
}
OwningAssetCatalogMap &AssetCatalogService::get_deleted_catalogs()
{
  return catalog_collection_->deleted_catalogs_;
}

AssetCatalogDefinitionFile *AssetCatalogService::get_catalog_definition_file()
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

  const AssetCatalog *active_catalog = find_catalog(active_catalog_id);

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

  this->rebuild_tree();
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

    /* TODO(Sybren): go over all assets that are assigned to this catalog, defined in the current
     * blend file, and update the catalog simple name stored there. */
  }

  this->rebuild_tree();
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

  BLI_assert_msg(catalog_tree_, "An Asset Catalog tree should always exist.");
  catalog_tree_->insert_item(*catalog_ptr);

  return catalog_ptr;
}

static std::string asset_definition_default_file_path_from_dir(StringRef asset_library_root)
{
  char file_path[PATH_MAX];
  BLI_join_dirfile(file_path,
                   sizeof(file_path),
                   asset_library_root.data(),
                   AssetCatalogService::DEFAULT_CATALOG_FILENAME.data());
  return file_path;
}

void AssetCatalogService::load_from_disk()
{
  load_from_disk(asset_library_root_);
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
    load_single_file(file_or_directory_path);
  }
  else if (S_ISDIR(status.st_mode)) {
    load_directory_recursive(file_or_directory_path);
  }
  else {
    /* TODO(@sybren): throw an appropriate exception. */
  }

  /* TODO: Should there be a sanitize step? E.g. to remove catalogs with identical paths? */

  rebuild_tree();
}

void AssetCatalogService::load_directory_recursive(const CatalogFilePath &directory_path)
{
  /* TODO(@sybren): implement proper multi-file support. For now, just load
   * the default file if it is there. */
  CatalogFilePath file_path = asset_definition_default_file_path_from_dir(directory_path);

  if (!BLI_exists(file_path.data())) {
    /* No file to be loaded is perfectly fine. */
    CLOG_INFO(&LOG, 2, "path not found: %s", file_path.data());
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
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>();
  cdf->file_path = catalog_definition_file_path;

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

    const bool should_skip = is_catalog_known_with_unsaved_changes(catalog_id);
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
  this->rebuild_tree();
}

void AssetCatalogService::purge_catalogs_not_listed(const Set<CatalogID> &catalogs_to_keep)
{
  Set<CatalogID> cats_to_remove;
  for (CatalogID cat_id : this->catalog_collection_->catalogs_.keys()) {
    if (catalogs_to_keep.contains(cat_id)) {
      continue;
    }
    if (is_catalog_known_with_unsaved_changes(cat_id)) {
      continue;
    }
    /* This catalog is not on disk, but also not modified, so get rid of it. */
    cats_to_remove.add(cat_id);
  }

  for (CatalogID cat_id : cats_to_remove) {
    delete_catalog_by_id_hard(cat_id);
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
  if (!write_to_disk_ex(blend_file_path)) {
    return false;
  }

  untag_has_unsaved_changes();
  rebuild_tree();
  return true;
}

bool AssetCatalogService::write_to_disk_ex(const CatalogFilePath &blend_file_path)
{
  /* TODO(Sybren): expand to support multiple CDFs. */

  /* - Already loaded a CDF from disk? -> Always write to that file. */
  if (catalog_collection_->catalog_definition_file_) {
    reload_catalogs();
    return catalog_collection_->catalog_definition_file_->write_to_disk();
  }

  if (catalog_collection_->catalogs_.is_empty() &&
      catalog_collection_->deleted_catalogs_.is_empty()) {
    /* Avoid saving anything, when there is nothing to save. */
    return true; /* Writing nothing when there is nothing to write is still a success. */
  }

  const CatalogFilePath cdf_path_to_write = find_suitable_cdf_path_for_writing(blend_file_path);
  catalog_collection_->catalog_definition_file_ = construct_cdf_in_memory(cdf_path_to_write);
  reload_catalogs();
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
  tag_all_catalogs_as_unsaved_changes();
}

CatalogFilePath AssetCatalogService::find_suitable_cdf_path_for_writing(
    const CatalogFilePath &blend_file_path)
{
  BLI_assert_msg(!blend_file_path.empty(),
                 "A non-empty .blend file path is required to be able to determine where the "
                 "catalog definition file should be put");

  /* Ask the asset library API for an appropriate location. */
  char suitable_root_path[PATH_MAX];
  const bool asset_lib_root_found = BKE_asset_library_find_suitable_root_path_from_path(
      blend_file_path.c_str(), suitable_root_path);
  if (asset_lib_root_found) {
    char asset_lib_cdf_path[PATH_MAX];
    BLI_path_join(asset_lib_cdf_path,
                  sizeof(asset_lib_cdf_path),
                  suitable_root_path,
                  DEFAULT_CATALOG_FILENAME.c_str(),
                  NULL);
    return asset_lib_cdf_path;
  }

  /* Determine the default CDF path in the same directory of the blend file. */
  char blend_dir_path[PATH_MAX];
  BLI_split_dir_part(blend_file_path.c_str(), blend_dir_path, sizeof(blend_dir_path));
  const CatalogFilePath cdf_path_next_to_blend = asset_definition_default_file_path_from_dir(
      blend_dir_path);
  return cdf_path_next_to_blend;
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogService::construct_cdf_in_memory(
    const CatalogFilePath &file_path)
{
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>();
  cdf->file_path = file_path;

  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    cdf->add_new(catalog.get());
  }

  return cdf;
}

AssetCatalogTree *AssetCatalogService::get_catalog_tree()
{
  return catalog_tree_.get();
}

std::unique_ptr<AssetCatalogTree> AssetCatalogService::read_into_tree()
{
  auto tree = std::make_unique<AssetCatalogTree>();

  /* Go through the catalogs, insert each path component into the tree where needed. */
  for (auto &catalog : catalog_collection_->catalogs_.values()) {
    tree->insert_item(*catalog);
  }

  return tree;
}

void AssetCatalogService::rebuild_tree()
{
  create_missing_catalogs();
  this->catalog_tree_ = read_into_tree();
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
    AssetCatalog *parent_catalog = create_catalog(parent_path);
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
  rebuild_tree();
}

void AssetCatalogService::redo()
{
  BLI_assert_msg(is_redo_possbile(), "Redo stack is empty");

  undo_snapshots_.append(std::move(catalog_collection_));
  catalog_collection_ = redo_snapshots_.pop_last();
  rebuild_tree();
}

void AssetCatalogService::undo_push()
{
  std::unique_ptr<AssetCatalogCollection> snapshot = catalog_collection_->deep_copy();
  undo_snapshots_.append(std::move(snapshot));
  redo_snapshots_.clear();
}

/* ---------------------------------------------------------------------- */

std::unique_ptr<AssetCatalogCollection> AssetCatalogCollection::deep_copy() const
{
  auto copy = std::make_unique<AssetCatalogCollection>();

  copy->has_unsaved_changes_ = this->has_unsaved_changes_;
  copy->catalogs_ = copy_catalog_map(this->catalogs_);
  copy->deleted_catalogs_ = copy_catalog_map(this->deleted_catalogs_);

  if (catalog_definition_file_) {
    copy->catalog_definition_file_ = catalog_definition_file_->copy_and_remap(
        copy->catalogs_, copy->deleted_catalogs_);
  }

  return copy;
}

OwningAssetCatalogMap AssetCatalogCollection::copy_catalog_map(const OwningAssetCatalogMap &orig)
{
  OwningAssetCatalogMap copy;

  for (const auto &orig_catalog_uptr : orig.values()) {
    auto copy_catalog_uptr = std::make_unique<AssetCatalog>(*orig_catalog_uptr);
    copy.add_new(copy_catalog_uptr->catalog_id, std::move(copy_catalog_uptr));
  }

  return copy;
}

/* ---------------------------------------------------------------------- */

AssetCatalogTreeItem::AssetCatalogTreeItem(StringRef name,
                                           CatalogID catalog_id,
                                           StringRef simple_name,
                                           const AssetCatalogTreeItem *parent)
    : name_(name), catalog_id_(catalog_id), simple_name_(simple_name), parent_(parent)
{
}

CatalogID AssetCatalogTreeItem::get_catalog_id() const
{
  return catalog_id_;
}

StringRefNull AssetCatalogTreeItem::get_name() const
{
  return name_;
}

StringRefNull AssetCatalogTreeItem::get_simple_name() const
{
  return simple_name_;
}
bool AssetCatalogTreeItem::has_unsaved_changes() const
{
  return has_unsaved_changes_;
}

AssetCatalogPath AssetCatalogTreeItem::catalog_path() const
{
  AssetCatalogPath current_path = name_;
  for (const AssetCatalogTreeItem *parent = parent_; parent; parent = parent->parent_) {
    current_path = AssetCatalogPath(parent->name_) / current_path;
  }
  return current_path;
}

int AssetCatalogTreeItem::count_parents() const
{
  int i = 0;
  for (const AssetCatalogTreeItem *parent = parent_; parent; parent = parent->parent_) {
    i++;
  }
  return i;
}

bool AssetCatalogTreeItem::has_children() const
{
  return !children_.empty();
}

void AssetCatalogTreeItem::foreach_item_recursive(AssetCatalogTreeItem::ChildMap &children,
                                                  const ItemIterFn callback)
{
  for (auto &[key, item] : children) {
    callback(item);
    foreach_item_recursive(item.children_, callback);
  }
}

void AssetCatalogTreeItem::foreach_child(const ItemIterFn callback)
{
  for (auto &[key, item] : children_) {
    callback(item);
  }
}

/* ---------------------------------------------------------------------- */

void AssetCatalogTree::insert_item(const AssetCatalog &catalog)
{
  const AssetCatalogTreeItem *parent = nullptr;
  /* The children for the currently iterated component, where the following component should be
   * added to (if not there yet). */
  AssetCatalogTreeItem::ChildMap *current_item_children = &root_items_;

  BLI_assert_msg(!ELEM(catalog.path.str()[0], '/', '\\'),
                 "Malformed catalog path; should not start with a separator");

  const CatalogID nil_id{};

  catalog.path.iterate_components([&](StringRef component_name, const bool is_last_component) {
    /* Insert new tree element - if no matching one is there yet! */
    auto [key_and_item, was_inserted] = current_item_children->emplace(
        component_name,
        AssetCatalogTreeItem(component_name,
                             is_last_component ? catalog.catalog_id : nil_id,
                             is_last_component ? catalog.simple_name : "",
                             parent));
    AssetCatalogTreeItem &item = key_and_item->second;

    /* If full path of this catalog already exists as parent path of a previously read catalog,
     * we can ensure this tree item's UUID is set here. */
    if (is_last_component) {
      if (BLI_uuid_is_nil(item.catalog_id_) || catalog.flags.is_first_loaded) {
        item.catalog_id_ = catalog.catalog_id;
      }
      item.has_unsaved_changes_ = catalog.flags.has_unsaved_changes;
    }

    /* Walk further into the path (no matter if a new item was created or not). */
    parent = &item;
    current_item_children = &item.children_;
  });
}

void AssetCatalogTree::foreach_item(AssetCatalogTreeItem::ItemIterFn callback)
{
  AssetCatalogTreeItem::foreach_item_recursive(root_items_, callback);
}

void AssetCatalogTree::foreach_root_item(const ItemIterFn callback)
{
  for (auto &[key, item] : root_items_) {
    callback(item);
  }
}

/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */

bool AssetCatalogDefinitionFile::contains(const CatalogID catalog_id) const
{
  return catalogs_.contains(catalog_id);
}

void AssetCatalogDefinitionFile::add_new(AssetCatalog *catalog)
{
  catalogs_.add_new(catalog->catalog_id, catalog);
}

void AssetCatalogDefinitionFile::add_overwrite(AssetCatalog *catalog)
{
  catalogs_.add_overwrite(catalog->catalog_id, catalog);
}

void AssetCatalogDefinitionFile::forget(CatalogID catalog_id)
{
  catalogs_.remove(catalog_id);
}

void AssetCatalogDefinitionFile::parse_catalog_file(
    const CatalogFilePath &catalog_definition_file_path,
    AssetCatalogParsedFn catalog_loaded_callback)
{
  fstream infile(catalog_definition_file_path, std::ios::in);

  if (!infile.is_open()) {
    CLOG_ERROR(&LOG, "%s: unable to open file", catalog_definition_file_path.c_str());
    return;
  }
  bool seen_version_number = false;
  std::string line;
  while (std::getline(infile, line)) {
    const StringRef trimmed_line = StringRef(line).trim();
    if (trimmed_line.is_empty() || trimmed_line[0] == '#') {
      continue;
    }

    if (!seen_version_number) {
      /* The very first non-ignored line should be the version declaration. */
      const bool is_valid_version = this->parse_version_line(trimmed_line);
      if (!is_valid_version) {
        std::cerr << catalog_definition_file_path
                  << ": first line should be version declaration; ignoring file." << std::endl;
        break;
      }
      seen_version_number = true;
      continue;
    }

    std::unique_ptr<AssetCatalog> catalog = this->parse_catalog_line(trimmed_line);
    if (!catalog) {
      continue;
    }

    AssetCatalog *non_owning_ptr = catalog.get();
    const bool keep_catalog = catalog_loaded_callback(std::move(catalog));
    if (!keep_catalog) {
      continue;
    }

    /* The AssetDefinitionFile should include this catalog when writing it back to disk. */
    this->add_overwrite(non_owning_ptr);
  }
}

bool AssetCatalogDefinitionFile::parse_version_line(const StringRef line)
{
  if (!line.startswith(VERSION_MARKER)) {
    return false;
  }

  const std::string version_string = line.substr(VERSION_MARKER.length());
  const int file_version = std::atoi(version_string.c_str());

  /* No versioning, just a blunt check whether it's the right one. */
  return file_version == SUPPORTED_VERSION;
}

std::unique_ptr<AssetCatalog> AssetCatalogDefinitionFile::parse_catalog_line(const StringRef line)
{
  const char delim = ':';
  const int64_t first_delim = line.find_first_of(delim);
  if (first_delim == StringRef::not_found) {
    std::cerr << "Invalid catalog line in " << this->file_path << ": " << line << std::endl;
    return std::unique_ptr<AssetCatalog>(nullptr);
  }

  /* Parse the catalog ID. */
  const std::string id_as_string = line.substr(0, first_delim).trim();
  bUUID catalog_id;
  const bool uuid_parsed_ok = BLI_uuid_parse_string(&catalog_id, id_as_string.c_str());
  if (!uuid_parsed_ok) {
    std::cerr << "Invalid UUID in " << this->file_path << ": " << line << std::endl;
    return std::unique_ptr<AssetCatalog>(nullptr);
  }

  /* Parse the path and simple name. */
  const StringRef path_and_simple_name = line.substr(first_delim + 1);
  const int64_t second_delim = path_and_simple_name.find_first_of(delim);

  std::string path_in_file;
  std::string simple_name;
  if (second_delim == 0) {
    /* Delimiter as first character means there is no path. These lines are to be ignored. */
    return std::unique_ptr<AssetCatalog>(nullptr);
  }

  if (second_delim == StringRef::not_found) {
    /* No delimiter means no simple name, just treat it as all "path". */
    path_in_file = path_and_simple_name;
    simple_name = "";
  }
  else {
    path_in_file = path_and_simple_name.substr(0, second_delim);
    simple_name = path_and_simple_name.substr(second_delim + 1).trim();
  }

  AssetCatalogPath catalog_path = path_in_file;
  return std::make_unique<AssetCatalog>(catalog_id, catalog_path.cleanup(), simple_name);
}

bool AssetCatalogDefinitionFile::write_to_disk() const
{
  BLI_assert_msg(!this->file_path.empty(), "Writing to CDF requires its file path to be known");
  return this->write_to_disk(this->file_path);
}

bool AssetCatalogDefinitionFile::write_to_disk(const CatalogFilePath &dest_file_path) const
{
  const CatalogFilePath writable_path = dest_file_path + ".writing";
  const CatalogFilePath backup_path = dest_file_path + "~";

  if (!this->write_to_disk_unsafe(writable_path)) {
    /* TODO: communicate what went wrong. */
    return false;
  }
  if (BLI_exists(dest_file_path.c_str())) {
    if (BLI_rename(dest_file_path.c_str(), backup_path.c_str())) {
      /* TODO: communicate what went wrong. */
      return false;
    }
  }
  if (BLI_rename(writable_path.c_str(), dest_file_path.c_str())) {
    /* TODO: communicate what went wrong. */
    return false;
  }

  return true;
}

bool AssetCatalogDefinitionFile::write_to_disk_unsafe(const CatalogFilePath &dest_file_path) const
{
  char directory[PATH_MAX];
  BLI_split_dir_part(dest_file_path.c_str(), directory, sizeof(directory));
  if (!ensure_directory_exists(directory)) {
    /* TODO(Sybren): pass errors to the UI somehow. */
    return false;
  }

  fstream output(dest_file_path, std::ios::out);

  /* TODO(@sybren): remember the line ending style that was originally read, then use that to write
   * the file again. */

  /* Write the header. */
  output << HEADER;
  output << "" << std::endl;
  output << VERSION_MARKER << SUPPORTED_VERSION << std::endl;
  output << "" << std::endl;

  /* Write the catalogs, ordered by path (primary) and UUID (secondary). */
  AssetCatalogOrderedSet catalogs_by_path;
  for (const AssetCatalog *catalog : catalogs_.values()) {
    if (catalog->flags.is_deleted) {
      continue;
    }
    catalogs_by_path.insert(catalog);
  }

  for (const AssetCatalog *catalog : catalogs_by_path) {
    output << catalog->catalog_id << ":" << catalog->path << ":" << catalog->simple_name
           << std::endl;
  }
  output.close();
  return !output.bad();
}

bool AssetCatalogDefinitionFile::ensure_directory_exists(
    const CatalogFilePath directory_path) const
{
  /* TODO(@sybren): design a way to get such errors presented to users (or ensure that they never
   * occur). */
  if (directory_path.empty()) {
    std::cerr
        << "AssetCatalogService: no asset library root configured, unable to ensure it exists."
        << std::endl;
    return false;
  }

  if (BLI_exists(directory_path.data())) {
    if (!BLI_is_dir(directory_path.data())) {
      std::cerr << "AssetCatalogService: " << directory_path
                << " exists but is not a directory, this is not a supported situation."
                << std::endl;
      return false;
    }

    /* Root directory exists, work is done. */
    return true;
  }

  /* Ensure the root directory exists. */
  std::error_code err_code;
  if (!BLI_dir_create_recursive(directory_path.data())) {
    std::cerr << "AssetCatalogService: error creating directory " << directory_path << ": "
              << err_code << std::endl;
    return false;
  }

  /* Root directory has been created, work is done. */
  return true;
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogDefinitionFile::copy_and_remap(
    const OwningAssetCatalogMap &catalogs, const OwningAssetCatalogMap &deleted_catalogs) const
{
  auto copy = std::make_unique<AssetCatalogDefinitionFile>(*this);
  copy->catalogs_.clear();

  /* Remap pointers of the copy from the original AssetCatalogCollection to the given one. */
  for (CatalogID catalog_id : catalogs_.keys()) {
    /* The catalog can be in the regular or the deleted map. */
    const std::unique_ptr<AssetCatalog> *remapped_catalog_uptr_ptr = catalogs.lookup_ptr(
        catalog_id);
    if (remapped_catalog_uptr_ptr) {
      copy->catalogs_.add_new(catalog_id, remapped_catalog_uptr_ptr->get());
      continue;
    }

    remapped_catalog_uptr_ptr = deleted_catalogs.lookup_ptr(catalog_id);
    if (remapped_catalog_uptr_ptr) {
      copy->catalogs_.add_new(catalog_id, remapped_catalog_uptr_ptr->get());
      continue;
    }

    BLI_assert(!"A CDF should only reference known catalogs.");
  }

  return copy;
}

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

AssetCatalogFilter::AssetCatalogFilter(Set<CatalogID> &&matching_catalog_ids,
                                       Set<CatalogID> &&known_catalog_ids)
    : matching_catalog_ids(std::move(matching_catalog_ids)),
      known_catalog_ids(std::move(known_catalog_ids))
{
}

bool AssetCatalogFilter::contains(const CatalogID asset_catalog_id) const
{
  return matching_catalog_ids.contains(asset_catalog_id);
}

bool AssetCatalogFilter::is_known(const CatalogID asset_catalog_id) const
{
  if (BLI_uuid_is_nil(asset_catalog_id)) {
    return false;
  }
  return known_catalog_ids.contains(asset_catalog_id);
}

}  // namespace blender::bke
