/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "AS_asset_catalog.hh"

namespace blender::asset_system {

/**
 * All catalogs that are owned by a single asset library, and managed by a single instance of
 * #AssetCatalogService. The undo system for asset catalog edits contains historical copies of this
 * struct.
 */
class AssetCatalogCollection {
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

  friend AssetCatalogService;

 public:
  AssetCatalogCollection() = default;
  AssetCatalogCollection(const AssetCatalogCollection &other) = delete;
  AssetCatalogCollection(AssetCatalogCollection &&other) noexcept = default;

  std::unique_ptr<AssetCatalogCollection> deep_copy() const;
  using OnDuplicateCatalogIdFn =
      FunctionRef<void(const AssetCatalog &existing, const AssetCatalog &to_be_ignored)>;
  /**
   * Copy the catalogs from \a other and append them to this collection. Copies no other data
   * otherwise.
   *
   * \note If a catalog from \a other already exists in this collection (identified by catalog ID),
   * it will be skipped and \a on_duplicate_items will be called.
   */
  void add_catalogs_from_existing(const AssetCatalogCollection &other,
                                  OnDuplicateCatalogIdFn on_duplicate_items);

 protected:
  static OwningAssetCatalogMap copy_catalog_map(const OwningAssetCatalogMap &orig);
};

}
