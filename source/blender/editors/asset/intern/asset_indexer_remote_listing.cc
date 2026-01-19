/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <optional>
#include <string>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_serialize.hh"

#include "BKE_report.hh"

#include "BLT_translation.hh"

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

  this->online_info = std::move(other.online_info);
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

  static ReadingResult<AssetLibraryMeta> read(const StringRefNull root_dirpath,
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
ReadingResult<AssetLibraryMeta> AssetLibraryMeta::read(
    const StringRefNull root_dirpath, const std::optional<Timestamp> ignore_before_timestamp)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), root_dirpath.c_str(), "_asset-library-meta.json");

  if (!BLI_exists(filepath)) {
    return ReadingResult<AssetLibraryMeta>::Failure(
        fmt::format(N_("file does not exist: {:s}"), filepath));
  }

  if (ignore_before_timestamp) {
    std::optional<bool> is_older = file_older_than_timestamp(filepath, *ignore_before_timestamp);
    if (!is_older) {
      return ReadingResult<AssetLibraryMeta>::Failure(
          fmt::format(N_("file does not exist: {:s}"), filepath));
    }
    if (*is_older) {
      return ReadingResult<AssetLibraryMeta>::Failure(
          fmt::format(N_("file is too old: {:s}"), filepath));
    }
  }

  const std::unique_ptr<Value> contents = read_contents(filepath);
  if (!contents) {
    return ReadingResult<AssetLibraryMeta>::Failure(
        fmt::format(N_("file does not contain JSON: {:s}"), filepath));
  }

  const DictionaryValue *root = contents->as_dictionary_value();
  if (!root) {
    return ReadingResult<AssetLibraryMeta>::Failure(
        fmt::format(N_("file is not a JSON dictionary: {:s}"), filepath));
  }

  const DictionaryValue *entries = root->lookup_dict("api_versions");
  BLI_assert(entries != nullptr);
  if (entries == nullptr) {
    return ReadingResult<AssetLibraryMeta>::Failure(
        fmt::format(N_("no API versions defined: {:s}"), filepath));
  }

  AssetLibraryMeta library_meta;

  for (const DictionaryValue::Item &version : entries->elements()) {
    /* Relative path to the listing meta-file (e.g. `_v1/asset-index.json`). */
    const DictionaryValue *index_path_info = version.second->as_dictionary_value();
    if (!index_path_info) {
      CLOG_WARN(&LOG,
                "Error reading asset listing API version '%s' in %s - ignoring",
                version.first.c_str(),
                filepath);
      continue;
    }

    std::optional<asset_system::URLWithHash> url_with_hash = parse_url_with_hash_dict(
        index_path_info);
    if (!url_with_hash) {
      CLOG_WARN(&LOG,
                "Error reading asset listing API version '%s' in %s, no URL+hash found - ignoring",
                version.first.c_str(),
                filepath);
      continue;
    }
    library_meta.api_versions.add(version.first, std::move(*url_with_hash));
  }

  return ReadingResult<AssetLibraryMeta>::Success(std::move(library_meta));
}

/** \} */

struct ApiVersionInfo {
  uint version_nr;
  /** Relative path to the listing meta-file (e.g. `_v1/asset-index.json`). */
  std::string listing_relpath;
  /** Hash of the file, like `SHA256:112233`. */
  std::string listing_hash;
};

static ReadingResult<ApiVersionInfo> choose_api_version(const AssetLibraryMeta &library_meta)
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
      ApiVersionInfo version_info{version_nr, url_with_hash->url, url_with_hash->hash};
      return ReadingResult<ApiVersionInfo>::Success(std::move(version_info));
    }
  }

  return ReadingResult<ApiVersionInfo>::Failure(
      N_("remote does not offer an API version supported by this version of Blender"));
}

bool read_remote_listing(const StringRefNull root_dirpath,
                         const StringRefNull asset_library_name,
                         ReportList &reports,
                         const RemoteListingEntryProcessFn process_fn,
                         const RemoteListingWaitForPagesFn wait_fn,
                         const std::optional<Timestamp> ignore_before_timestamp)
{
  /* This actually does the work, and returns a ReadingResult. It's implemented as a lambda
   * function, to be able to use early returns on error. */
  auto get_result = [&]() {
    const ReadingResult<AssetLibraryMeta> meta = AssetLibraryMeta::read(root_dirpath,
                                                                        ignore_before_timestamp);
    if (!meta.is_success()) {
      return meta.without_success_value();
    }

    const ReadingResult<ApiVersionInfo> api_version_info = choose_api_version(*meta);
    if (!api_version_info.is_success()) {
      return api_version_info.without_success_value();
    }

    /* Path to the listing meta-file is version-dependent. */
    switch (api_version_info->version_nr) {
      case 1: {
        return read_remote_listing_v1(root_dirpath, process_fn, wait_fn, ignore_before_timestamp);
      }
      default:
        /* choose_api_version() should not have chosen this version. */
        BLI_assert_unreachable();
        return ReadingResult<>::Failure(N_("internal error, please report a bug"));
    }
  };

  const ReadingResult<> result = get_result();

  /* Get these messages up-stream. The last call to BKE_report(f) will be the one shown in the
   * status bar. The rest are just printed to the terminal and gathered at the Info editor. */
  if (result.is_failure()) {
    BKE_reportf(&reports,
                RPT_ERROR,
                "Asset Library '%s': %s",
                asset_library_name.c_str(),
                RPT_(result.failure_reason.c_str()));
    BKE_reportf(&reports,
                RPT_ERROR,
                "Could not read asset listing '%s', see Info Editor for details",
                asset_library_name.c_str());
    return false;
  }
  if (result.is_cancelled()) {
    return false;
  }
  if (result.has_warnings()) {
    for (const std::string &warning : result.warnings) {
      BKE_reportf(&reports,
                  RPT_WARNING,
                  "Asset Library '%s': %s",
                  asset_library_name.c_str(),
                  RPT_(warning.c_str()));
    }
    BKE_reportf(&reports,
                RPT_WARNING,
                "Could not read asset listing for '%s', see Info Editor for details",
                asset_library_name.c_str());
  }
  return true;
}

}  // namespace blender::ed::asset::index
