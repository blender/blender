/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "AS_asset_representation.hh"

#include "BKE_asset.hh"
#include "BKE_idtype.hh"

#include "BLI_listbase.h"

#include "DNA_asset_types.h"

#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"

#include "ED_asset_filter.hh"
#include "ED_asset_handle.hh"
#include "ED_asset_library.hh"
#include "ED_asset_list.hh"

namespace blender::ed::asset {

bool filter_matches_asset(const AssetFilterSettings *filter,
                          const asset_system::AssetRepresentation &asset)
{
  ID_Type asset_type = asset.get_id_type();
  uint64_t asset_id_filter = BKE_idtype_idcode_to_idfilter(asset_type);

  if (filter->id_types && (filter->id_types & asset_id_filter) == 0) {
    return false;
  }
  /* Not very efficient (O(n^2)), could be improved quite a bit. */
  LISTBASE_FOREACH (const AssetTag *, filter_tag, &filter->tags) {
    AssetMetaData &asset_data = asset.get_metadata();

    AssetTag *matched_tag = (AssetTag *)BLI_findstring(
        &asset_data.tags, filter_tag->name, offsetof(AssetTag, name));
    if (matched_tag == nullptr) {
      return false;
    }
  }

  /* Successfully passed through all filters. */
  return true;
}

asset_system::AssetCatalogTree build_filtered_catalog_tree(
    const asset_system::AssetLibrary &library,
    const AssetLibraryReference &library_ref,
    const FunctionRef<bool(const asset_system::AssetRepresentation &)> is_asset_visible_fn)
{
  Set<StringRef> known_paths;

  /* Collect paths containing assets. */
  list::iterate(library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (!is_asset_visible_fn(asset)) {
      return true;
    }

    const AssetMetaData &meta_data = asset.get_metadata();
    if (BLI_uuid_is_nil(meta_data.catalog_id)) {
      return true;
    }

    const asset_system::AssetCatalog *catalog = library.catalog_service->find_catalog(
        meta_data.catalog_id);
    if (catalog == nullptr) {
      return true;
    }
    known_paths.add(catalog->path.str());
    return true;
  });

  /* Build catalog tree. */
  asset_system::AssetCatalogTree filtered_tree;
  asset_system::AssetCatalogTree &full_tree = *library.catalog_service->get_catalog_tree();
  full_tree.foreach_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (!known_paths.contains(item.catalog_path().str())) {
      return;
    }

    asset_system::AssetCatalog *catalog = library.catalog_service->find_catalog(
        item.get_catalog_id());
    if (catalog == nullptr) {
      return;
    }
    filtered_tree.insert_item(*catalog);
  });

  return filtered_tree;
}

AssetItemTree build_filtered_all_catalog_tree(
    const AssetLibraryReference &library_ref,
    const bContext &C,
    const AssetFilterSettings &filter_settings,
    const FunctionRef<bool(const AssetMetaData &)> meta_data_filter)
{
  MultiValueMap<asset_system::AssetCatalogPath, asset_system::AssetRepresentation *>
      assets_per_path;
  Vector<asset_system::AssetRepresentation *> unassigned_assets;

  list::storage_fetch(&library_ref, &C);
  asset_system::AssetLibrary *library = list::library_get_once_available(library_ref);
  if (!library) {
    return {};
  }

  list::iterate(library_ref, [&](asset_system::AssetRepresentation &asset) {
    if (!filter_matches_asset(&filter_settings, asset)) {
      return true;
    }
    const AssetMetaData &meta_data = asset.get_metadata();
    if (meta_data_filter && !meta_data_filter(meta_data)) {
      return true;
    }

    if (BLI_uuid_is_nil(meta_data.catalog_id)) {
      unassigned_assets.append(&asset);
      return true;
    }

    const asset_system::AssetCatalog *catalog = library->catalog_service->find_catalog(
        meta_data.catalog_id);
    if (catalog == nullptr) {
      /* Also include assets with catalogs we're unable to find (e.g. the catalog was deleted) in
       * the "Unassigned" list. */
      unassigned_assets.append(&asset);
      return true;
    }
    assets_per_path.add(catalog->path, &asset);
    return true;
  });

  asset_system::AssetCatalogTree catalogs_with_node_assets;
  asset_system::AssetCatalogTree &catalog_tree = *library->catalog_service->get_catalog_tree();
  catalog_tree.foreach_item([&](asset_system::AssetCatalogTreeItem &item) {
    if (assets_per_path.lookup(item.catalog_path()).is_empty()) {
      return;
    }
    asset_system::AssetCatalog *catalog = library->catalog_service->find_catalog(
        item.get_catalog_id());
    if (catalog == nullptr) {
      return;
    }
    catalogs_with_node_assets.insert_item(*catalog);
  });

  return {std::move(catalogs_with_node_assets),
          std::move(assets_per_path),
          std::move(unassigned_assets),
          false};
}

}  // namespace blender::ed::asset
