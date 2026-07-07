/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 *
 * Classes internal to the asset system for asset catalog management.
 */

#pragma once

#include "AS_asset_catalog.hh"

#include "BLI_string_ref.hh"

namespace blender::asset_system {

/**
 * Keeps track of which catalogs are defined in a certain file on disk.
 * Only contains non-owning pointers to the #AssetCatalog instances, so ensure the lifetime of this
 * class is shorter than that of the #`AssetCatalog`s themselves.
 */
class AssetCatalogDefinitionFile {
 protected:
  /* Catalogs stored in this file. They are mapped by ID to make it possible to query whether a
   * catalog is already known, without having to find the corresponding `AssetCatalog*`. */
  Map<CatalogID, AssetCatalog *> catalogs_;

 public:
  /* For now this is the only version of the catalog definition files that is supported.
   * Later versioning code may be added to handle older files. */
  const static int SUPPORTED_VERSION;
  /* String that's matched in the catalog definition file to know that the line is the version
   * declaration. It has to start with a space to ensure it won't match any hypothetical future
   * field that starts with "VERSION". */
  const static std::string VERSION_MARKER;
  const static std::string HEADER;

  const CatalogFilePath file_path;

  AssetCatalogDefinitionFile(const CatalogFilePath &file_path);

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

  /**
   * Returns whether this file exists on disk.
   */
  bool exists_on_disk() const;

  bool contains(CatalogID catalog_id) const;
  /** Add a catalog, overwriting the one with the same catalog ID. */
  void add_overwrite(AssetCatalog *catalog);
  /** Add a new catalog. Undefined behavior if a catalog with the same ID was already added. */
  void add_new(AssetCatalog *catalog);

  /** Remove the catalog from the collection of catalogs stored in this file. */
  void forget(CatalogID catalog_id);

  using AssetCatalogParsedFn = FunctionRef<bool(std::unique_ptr<AssetCatalog>)>;
  void parse_catalog_file(const CatalogFilePath &catalog_definition_file_path,
                          AssetCatalogParsedFn catalog_loaded_callback);

  std::unique_ptr<AssetCatalogDefinitionFile> copy_and_remap(
      const OwningAssetCatalogMap &catalogs, const OwningAssetCatalogMap &deleted_catalogs) const;

 protected:
  bool parse_version_line(StringRef line);
  std::unique_ptr<AssetCatalog> parse_catalog_line(StringRef line);

  /**
   * Write the catalog definitions to the given file path.
   * Return true when the file was written correctly, false when there was a problem.
   */
  bool write_to_disk_unsafe(const CatalogFilePath &dest_file_path) const;
  bool ensure_directory_exists(const CatalogFilePath &directory_path) const;
};

}  // namespace blender::asset_system
