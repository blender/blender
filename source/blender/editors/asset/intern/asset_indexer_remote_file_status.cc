/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "AS_remote_library.hh"

#include "CLG_log.h"

#include "asset_indexer_remote_file_status.hh"

static CLG_LogRef LOG = {"asset.remote_listing"};

using namespace blender::asset_system;

namespace blender::ed::asset::index {

/**
 * Filename prefix for the Disk File Hash Service used by FileStatusChecker.
 *
 * The Disk File Hash Service itself will complete the filename depending on the back-end used. At
 * the moment of writing that's SQLite, which will append `_v{schema version}.sqlite`.
 *
 * NOTE: if this changes, also update the RemoteAssetListingLocator class in listing_downloader.py.
 */
constexpr const char *hash_service_filename_prefix = "_file_hashes";

FileStatusChecker::FileStatusChecker(const StringRefNull library_root_path)
    : library_root_path_(library_root_path)
{
  char dfhs_path[PATH_MAX];
  BLI_path_join(
      dfhs_path, sizeof(dfhs_path), library_root_path.c_str(), hash_service_filename_prefix);
  this->dfhs_ = disk_file_hash_service_get(dfhs_path);
}

RemoteAssetFileStatus FileStatusChecker::remote_file_status(RemoteListingFileEntry &file_to_check)
{
  const StringRefNull relative_file_path = file_to_check.local_path;

  /* Check against our own cache to see if we checked this file before. */
  if (file_to_check.file_status.has_value()) {
    return *file_to_check.file_status;
  }

  /* Construct the absolute path, so we can check its hash on disk. */
  char file_abspath[PATH_MAX];
  BLI_path_join(
      file_abspath, sizeof(file_abspath), library_root_path_.c_str(), relative_file_path.c_str());

  if (!BLI_exists(file_abspath)) {
    return this->remember(file_to_check, RemoteAssetFileStatus::NOT_ON_DISK);
  }

  /* Split METHOD:HASH into two StringRefs. */
  const StringRefNull hash_with_method = file_to_check.download_url.hash;
  const int64_t colon_index = hash_with_method.find_first_of(':');
  if (colon_index == StringRef::not_found) {
    CLOG_WARN(&LOG, "Asset file hash not in METHOD:HASH format: %s", hash_with_method.c_str());
    return this->remember(file_to_check, RemoteAssetFileStatus::NO_MATCH);
  }
  std::string hash_algorithm = hash_with_method.substr(0, colon_index);
  const StringRef hexhash = hash_with_method.substr(colon_index + 1);

  BLI_str_tolower_ascii(hash_algorithm.data(), hash_algorithm.length());

  /* Check with the Disk File Hash Service. */
  const bool is_match = dfhs_->file_matches(
      file_abspath, hash_algorithm, hexhash, file_to_check.size_in_bytes);

  return this->remember(file_to_check,
                        is_match ? RemoteAssetFileStatus::MATCH : RemoteAssetFileStatus::NO_MATCH);
}

asset_system::RemoteAssetFileStatus FileStatusChecker::remember(
    RemoteListingFileEntry &file_to_check, const asset_system::RemoteAssetFileStatus status)
{
  file_to_check.file_status = status;
  return status;
}

}  // namespace blender::ed::asset::index
