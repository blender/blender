/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include <iostream>

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"

#include "CLG_log.h"

#include "asset_catalog_definition_file.hh"

static CLG_LogRef LOG = {"asset.catalog"};

namespace blender::asset_system {

const int AssetCatalogDefinitionFile::SUPPORTED_VERSION = 1;
const std::string AssetCatalogDefinitionFile::VERSION_MARKER = "VERSION ";

const std::string AssetCatalogDefinitionFile::HEADER =
    "# This is an Asset Catalog Definition file for Blender.\n"
    "#\n"
    "# Empty lines and lines starting with `#` will be ignored.\n"
    "# The first non-ignored line should be the version indicator.\n"
    "# Other lines are of the format \"UUID:catalog/path/for/assets:simple catalog name\"\n";

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

AssetCatalogDefinitionFile::AssetCatalogDefinitionFile(const CatalogFilePath &file_path)
    : file_path(file_path)
{
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
    if (BLI_rename_overwrite(dest_file_path.c_str(), backup_path.c_str())) {
      /* TODO: communicate what went wrong. */
      return false;
    }
  }
  if (BLI_rename_overwrite(writable_path.c_str(), dest_file_path.c_str())) {
    /* TODO: communicate what went wrong. */
    return false;
  }

  return true;
}

bool AssetCatalogDefinitionFile::exists_on_disk() const
{
  return BLI_exists(this->file_path.c_str());
}

bool AssetCatalogDefinitionFile::write_to_disk_unsafe(const CatalogFilePath &dest_file_path) const
{
  char directory[PATH_MAX];
  BLI_path_split_dir_part(dest_file_path.c_str(), directory, sizeof(directory));
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
    const CatalogFilePath &directory_path) const
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

    BLI_assert_msg(false, "A CDF should only reference known catalogs.");
  }

  return copy;
}

}  // namespace blender::asset_system
