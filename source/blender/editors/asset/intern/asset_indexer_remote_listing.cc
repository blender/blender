/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"

#include "CLG_log.h"

#include "ED_asset_indexer.hh"
#include "asset_index.hh"
#include "asset_indexer_remote_listing.hh"

static CLG_LogRef LOG = {"asset.remote_listing"};

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

  this->file_path = std::move(other.file_path);
  this->download_url = std::move(other.download_url);
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
/** \name #RemoteListingFileEntry type
 * \{ */

RemoteListingFileEntry::RemoteListingFileEntry(RemoteListingFileEntry &&other)
{
  this->local_path = std::move(other.local_path);
  this->download_url = std::move(other.download_url);
}

RemoteListingFileEntry &RemoteListingFileEntry::operator=(RemoteListingFileEntry &&other)
{
  if (this == &other) {
    return *this;
  }
  std::destroy_at(this);
  new (this) RemoteListingFileEntry(std::move(other));
  return *this;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General functions for reading.
 * \{ */

std::unique_ptr<Value> read_contents(const StringRefNull filepath)
{
  JsonFormatter formatter;
  std::ifstream is;
  is.open(filepath.c_str());
  BLI_SCOPED_DEFER([&]() { is.close(); });

  return formatter.deserialize(is);
}

std::optional<asset_system::URLWithHash> parse_url_with_hash_dict(
    const DictionaryValue *url_with_hash_dict)
{
  if (!url_with_hash_dict) {
    return {};
  }

  const std::optional<StringRefNull> url = url_with_hash_dict->lookup_str("url");
  const std::optional<StringRefNull> hash = url_with_hash_dict->lookup_str("hash");

  /* A URL without hash is not up to spec, but we can work with it. But without
   * a URL it's hopeless. */
  if (!url) {
    return {};
  }

  return asset_system::URLWithHash{*url, hash.value_or("")};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Meta file
 *
 * Containing info like the author and contact information (all of which is ignored here), as well
 * as the API version.
 * \{ */

struct AssetLibraryMeta {
  /** Map of API version string ("v1", "v2", ...) to path relative to root directory. */
  Map<std::string, asset_system::URLWithHash> api_versions;

  static std::optional<AssetLibraryMeta> read(const StringRefNull root_dirpath,
                                              std::optional<Timestamp> ignore_before_timestamp);
};

/**
 * Note that this uses `std::filesystem::file_time_type` to get
 * \return true if the file is older than the timestamp, or no value if the file was not found.
 */
std::optional<bool> file_older_than_timestamp(const char *filepath, Timestamp timestamp)
{
  std::error_code error;
  Timestamp file_timestamp = std::filesystem::last_write_time(filepath, error);
  /** TODO better report error message? */
  if (error) {
    printf("Can't find file at path %s: %s\n", filepath, error.message().c_str());
    return {};
  }

  return file_timestamp < timestamp;
}

/**
 * \return the supported API versions read from the `_asset-library-meta.json` file.
 */
std::optional<AssetLibraryMeta> AssetLibraryMeta::read(
    const StringRefNull root_dirpath, const std::optional<Timestamp> ignore_before_timestamp)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), "_asset-library-meta.json");

  if (!BLI_exists(filepath)) {
    /** TODO report error message? */
    return {};
  }

  if (ignore_before_timestamp) {
    std::optional<bool> is_older = file_older_than_timestamp(filepath, *ignore_before_timestamp);
    if (!is_older) {
      CLOG_ERROR(&LOG, "Couldn't find meta file %s\n", filepath);
      return {};
    }
    if (*is_older) {
      CLOG_ERROR(&LOG, "Meta file too old %s\n", filepath);
      return {};
    }
  }

  const std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    /** TODO report error message? */
    return {};
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    /** TODO report error message? */
    return {};
  }

  const DictionaryValue *entries = root->lookup_dict("api_versions");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return {};
  }

  AssetLibraryMeta library_meta;

  for (const DictionaryValue::Item &version : entries->elements()) {
    /* Relative path to the listing meta-file (e.g. `_v1/asset-index.json`). */
    const DictionaryValue *index_path_info = version.second->as_dictionary_value();
    if (!index_path_info) {
      printf("Error reading asset listing API version '%s' in %s - ignoring\n",
             version.first.c_str(),
             filepath);
      continue;
    }

    std::optional<asset_system::URLWithHash> url_with_hash = parse_url_with_hash_dict(
        index_path_info);
    if (!url_with_hash) {
      printf("Error reading asset listing API version '%s' in %s, no URL found - ignoring\n",
             version.first.c_str(),
             filepath);
    }
    library_meta.api_versions.add(version.first, std::move(*url_with_hash));
  }

  return library_meta;
}

/** \} */

struct ApiVersionInfo {
  uint version_nr;
  /** Relative path to the listing meta-file (e.g. `_v1/asset-index.json`). */
  std::string listing_relpath;
  /** Hash of the file, like `SHA256:112233`. */
  std::string listing_hash;
};

static std::optional<ApiVersionInfo> choose_api_version(const AssetLibraryMeta &library_meta)
{
  /* API versions this version of Blender can handle, in descending order (most preferred to least
   * preferred order). */
  const Vector<std::pair<uint, StringRefNull>> readable_versions = {
      {1, "v1"},
  };

  for (const auto &[version_nr, version_str] : readable_versions) {
    if (const asset_system::URLWithHash *url_with_hash = library_meta.api_versions.lookup_ptr(
            version_str))
    {
      return ApiVersionInfo{version_nr, url_with_hash->url, url_with_hash->hash};
    }
  }

  return {};
}

bool read_remote_listing(const StringRefNull root_dirpath,
                         const RemoteListingEntryProcessFn process_fn,
                         const RemoteListingWaitForPagesFn wait_fn,
                         const std::optional<Timestamp> ignore_before_timestamp)
{
  /* TODO: Error reporting for all false return branches. */

  const std::optional<AssetLibraryMeta> meta = AssetLibraryMeta::read(root_dirpath,
                                                                      ignore_before_timestamp);
  if (!meta) {
    printf("Couldn't read meta\n");
    return false;
  }

  const std::optional<ApiVersionInfo> api_version_info = choose_api_version(*meta);
  if (!api_version_info) {
    printf("Couldn't choose API version\n");
    return false;
  }

  /* Path to the listing meta-file is version-dependent. */
  switch (api_version_info->version_nr) {
    case 1: {
      const ReadingResult result = read_remote_listing_v1(
          root_dirpath, process_fn, wait_fn, ignore_before_timestamp);
      if (result == ReadingResult::Failure) {
        return false;
      }
      if (result == ReadingResult::Cancelled) {
        return false;
      }
      break;
    }
    default:
      BLI_assert_unreachable();
      return false;
  }

  return true;
}

}  // namespace blender::ed::asset::index
