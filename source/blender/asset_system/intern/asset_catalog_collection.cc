/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "asset_catalog_definition_file.hh"

#include "asset_catalog_collection.hh"

namespace blender::asset_system {

std::unique_ptr<AssetCatalogCollection> AssetCatalogCollection::deep_copy() const
{
  auto copy = std::make_unique<AssetCatalogCollection>();

  copy->has_unsaved_changes_ = this->has_unsaved_changes_;
  copy->catalogs_ = this->copy_catalog_map(this->catalogs_);
  copy->deleted_catalogs_ = this->copy_catalog_map(this->deleted_catalogs_);

  if (catalog_definition_file_) {
    copy->catalog_definition_file_ = catalog_definition_file_->copy_and_remap(
        copy->catalogs_, copy->deleted_catalogs_);
  }

  return copy;
}

static void copy_catalog_map_into_existing(
    const OwningAssetCatalogMap &source,
    OwningAssetCatalogMap &dest,
    AssetCatalogCollection::OnDuplicateCatalogIdFn on_duplicate_items)
{
  for (const auto &orig_catalog_uptr : source.values()) {
    if (dest.contains(orig_catalog_uptr->catalog_id)) {
      if (on_duplicate_items) {
        on_duplicate_items(*dest.lookup(orig_catalog_uptr->catalog_id), *orig_catalog_uptr);
      }
      continue;
    }

    auto copy_catalog_uptr = std::make_unique<AssetCatalog>(*orig_catalog_uptr);
    dest.add_new(copy_catalog_uptr->catalog_id, std::move(copy_catalog_uptr));
  }
}

void AssetCatalogCollection::add_catalogs_from_existing(
    const AssetCatalogCollection &other,
    AssetCatalogCollection::OnDuplicateCatalogIdFn on_duplicate_items)
{
  copy_catalog_map_into_existing(other.catalogs_, catalogs_, on_duplicate_items);
}

OwningAssetCatalogMap AssetCatalogCollection::copy_catalog_map(const OwningAssetCatalogMap &orig)
{
  OwningAssetCatalogMap copy;
  copy_catalog_map_into_existing(
      orig, copy, /*on_duplicate_items=*/[](const AssetCatalog &, const AssetCatalog &) {
        /* `copy` was empty before. If this happens it means there was a duplicate in the `orig`
         * catalog map which should've been caught already. */
        BLI_assert_unreachable();
      });
  return copy;
}

}  // namespace blender::asset_system
