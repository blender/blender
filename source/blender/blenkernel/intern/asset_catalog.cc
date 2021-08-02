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

#include "BKE_asset_catalog.hh"

#include "BLI_string_ref.hh"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace blender::bke {

AssetCatalogService::AssetCatalogService()
{
}

AssetCatalog *AssetCatalogService::find_catalog(const CatalogID &catalog_id)
{
  std::unique_ptr<AssetCatalog> *catalog_uptr_ptr = this->catalogs.lookup_ptr(catalog_id);
  if (catalog_uptr_ptr == nullptr) {
    return nullptr;
  }
  return catalog_uptr_ptr->get();
}

void AssetCatalogService::load_from_disk(const CatalogFilePath &asset_library_root)
{
  fs::file_status status = fs::status(asset_library_root);
  switch (status.type()) {
    case fs::file_type::regular:
      load_single_file(asset_library_root);
      break;
    case fs::file_type::directory:
      load_directory_recursive(asset_library_root);
      break;
    default:
      // TODO(@sybren): throw an appropriate exception.
      return;
  }
}

void AssetCatalogService::load_directory_recursive(const CatalogFilePath & /*directory_path*/)
{
  // TODO(@sybren): implement
}

void AssetCatalogService::load_single_file(const CatalogFilePath &catalog_definition_file_path)
{
  std::unique_ptr<AssetCatalogDefinitionFile> cdf = parse_catalog_file(
      catalog_definition_file_path);

  BLI_assert_msg(!this->catalog_definition_file,
                 "Only loading of a single catalog definition file is supported.");
  this->catalog_definition_file = std::move(cdf);
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogService::parse_catalog_file(
    const CatalogFilePath &catalog_definition_file_path)
{
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>();
  cdf->file_path = catalog_definition_file_path;

  std::fstream infile(catalog_definition_file_path);
  std::string line;
  while (std::getline(infile, line)) {
    const StringRef trimmed_line = StringRef(line).trim().trim(path_separator);
    if (trimmed_line.is_empty() || trimmed_line[0] == '#') {
      continue;
    }

    std::unique_ptr<AssetCatalog> catalog = this->parse_catalog_line(trimmed_line, cdf.get());

    const bool is_catalog_id_reused_in_file = cdf->catalogs.contains(catalog->catalog_id);
    /* The AssetDefinitionFile should include this catalog when writing it back to disk, even if it
     * was a duplicate. */
    cdf->catalogs.add_new(catalog->catalog_id, catalog.get());

    if (is_catalog_id_reused_in_file) {
      std::cerr << catalog_definition_file_path << ": multiple definitions of catalog "
                << catalog->catalog_id << ", using first occurrence." << std::endl;
      /* Don't store 'catalog'; unique_ptr will free its memory. */
      continue;
    }

    if (this->catalogs.contains(catalog->catalog_id)) {
      // TODO(@sybren): apparently another CDF was already loaded. This is not supported yet.
      std::cerr << catalog_definition_file_path << ": multiple definitions of catalog "
                << catalog->catalog_id << " in multiple files, ignoring this one." << std::endl;
      /* Don't store 'catalog'; unique_ptr will free its memory. */
      continue;
    }

    /* The AssetCatalog pointer is owned by the AssetCatalogService. */
    this->catalogs.add_new(catalog->catalog_id, std::move(catalog));
  }

  return cdf;
}

std::unique_ptr<AssetCatalog> AssetCatalogService::parse_catalog_line(
    const StringRef line, const AssetCatalogDefinitionFile *catalog_definition_file)
{
  const int64_t first_space = line.find_first_of(' ');
  if (first_space == StringRef::not_found) {
    std::cerr << "Invalid line in " << catalog_definition_file->file_path << ": " << line
              << std::endl;
    return std::unique_ptr<AssetCatalog>(nullptr);
  }

  const StringRef catalog_id = line.substr(0, first_space);
  const StringRef catalog_path = line.substr(first_space + 1).trim().trim(path_separator);

  return std::make_unique<AssetCatalog>(catalog_id, catalog_path);
}

AssetCatalogDefinitionFile::AssetCatalogDefinitionFile()
{
}

AssetCatalog::AssetCatalog()
{
}

AssetCatalog::AssetCatalog(const CatalogID &catalog_id, const CatalogPath &path)
    : catalog_id(catalog_id), path(path)
{
}

}  // namespace blender::bke
