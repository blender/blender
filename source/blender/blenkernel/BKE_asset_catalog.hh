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

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include <filesystem>
#include <memory>
#include <string>

namespace blender::bke {

using CatalogID = std::string;
using CatalogPath = std::string;
using CatalogPathReference = StringRef;
using CatalogPathComponent = std::string;
using CatalogFilePath = std::filesystem::path;

class AssetCatalog;
class AssetCatalogDefinitionFile;
class AssetCatalogTreeNode;

/* Manages the asset catalogs of a single asset library (i.e. of catalogs defined in a single
 * directory hierarchy). */
class AssetCatalogService {
  const char path_separator = '/';

  /* TODO(@sybren): determine which properties should be private / get accessors. */

  // These pointers are owned by this AssetCatalogService.
  Map<CatalogID, std::unique_ptr<AssetCatalog>> catalogs;
  std::unique_ptr<AssetCatalogDefinitionFile> catalog_definition_file;

 public:
  AssetCatalogService();

  // Return nullptr if not found.
  AssetCatalog *find_catalog(const CatalogID &catalog_id);

  void load_from_disk(const CatalogFilePath &asset_library_root);

 protected:
  void load_directory_recursive(const CatalogFilePath &directory_path);
  void load_single_file(const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalogDefinitionFile> parse_catalog_file(
      const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalog> parse_catalog_line(
      StringRef line, const AssetCatalogDefinitionFile *catalog_definition_file);
};

class AssetCatalogDefinitionFile {
  /* TODO(@sybren): determine which properties should be private / get accessors. */
 public:
  CatalogFilePath file_path;
  Map<CatalogID, AssetCatalog *> catalogs;

  AssetCatalogDefinitionFile();
};

class AssetCatalog {
  /* TODO(@sybren): determine which properties should be private / get accessors. */
 public:
  AssetCatalog();
  AssetCatalog(const CatalogID &catalog_id, const CatalogPath &name);

  CatalogID catalog_id;
  CatalogPath path;
};

}  // namespace blender::bke
