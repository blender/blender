/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <fstream>
#include <string>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"
#include "BLI_set.hh"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"

namespace blender::ed::asset::index {

using namespace blender::io::serialize;

/* -------------------------------------------------------------------- */
/** \name #RemoteListingAssetEntry type
 * \{ */

RemoteListingAssetEntry::RemoteListingAssetEntry(RemoteListingAssetEntry &&other)
{
  this->datablock_info = other.datablock_info;
  other.datablock_info = {};
  this->idcode = other.idcode;

  this->archive_url = std::move(other.archive_url);
  this->thumbnail_url = std::move(other.thumbnail_url);
}

RemoteListingAssetEntry &RemoteListingAssetEntry::operator=(RemoteListingAssetEntry &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) RemoteListingAssetEntry(std::move(other));
  return *this;
}

RemoteListingAssetEntry::~RemoteListingAssetEntry()
{
  BLO_datablock_info_free(&datablock_info);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General functions for reading.
 * \{ */

std::unique_ptr<Value> read_contents(StringRefNull filepath)
{
  JsonFormatter formatter;
  std::ifstream is;
  is.open(filepath.c_str());
  BLI_SCOPED_DEFER([&]() { is.close(); });

  return formatter.deserialize(is);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Meta file
 *
 * Containing info like the author and contact information (all of which is ignored here), as well
 * as the API version.
 * \{ */

struct AssetLibraryMeta {
  Set<int> api_versions;

  static std::optional<AssetLibraryMeta> read(StringRefNull root_dirpath);
};

/**
 * \return the supported API versions read from the `_asset-library-meta.json` file.
 */
std::optional<AssetLibraryMeta> AssetLibraryMeta::read(StringRefNull root_dirpath)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), "_asset-library-meta.json");

  if (!BLI_exists(filepath)) {
    /** TODO report error message? */
    return {};
  }

  std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return {};
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return {};
  }

  const ArrayValue *entries = root->lookup_array("api_versions");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  AssetLibraryMeta library_meta;

  int i = 0;
  for (const std::shared_ptr<Value> &element : entries->elements()) {
    const IntValue *version = element->as_int_value();
    if (!version) {
      printf(
          "Error reading asset listing API version at index %i in %s - ignoring\n", i, filepath);
      i++;
      continue;
    }
    library_meta.api_versions.add(std::move(version->value()));
    i++;
  }

  return library_meta;
}

/** \} */

static std::optional<int> choose_api_version(const AssetLibraryMeta &library_meta)
{
  /* API versions this version of Blender can handle, in descending order (most preferred to least
   * preferred order). */
  const Vector readable_versions = {
      1,
  };

  for (int version : readable_versions) {
    if (library_meta.api_versions.contains(version)) {
      return version;
    }
  }

  return {};
}

bool read_remote_listing(StringRefNull root_dirpath, RemoteListingEntryProcessFn process_fn)
{
  /* TODO: Error reporting for all false return branches. */

  const std::optional<AssetLibraryMeta> meta = AssetLibraryMeta::read(root_dirpath);
  if (!meta) {
    printf("Couldn't read meta");
    return false;
  }

  const std::optional<int> api_version = choose_api_version(*meta);
  if (!api_version) {
    printf("Couldn't choose API version");
    return false;
  }

  switch (*api_version) {
    case 1:
      if (!read_remote_listing_v1(root_dirpath, process_fn)) {
        printf("Couldn't read V1 listing");
        return false;
      }
      break;
    default:
      BLI_assert_unreachable();
      return false;
  }

  return true;
}

}  // namespace blender::ed::asset::index
