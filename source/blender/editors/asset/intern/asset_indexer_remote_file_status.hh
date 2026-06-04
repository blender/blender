/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

#include "AS_disk_file_hash_service.hh"
#include "AS_remote_library.hh"

#include "ED_asset_indexer.hh"

namespace blender {
struct bContext;
}

namespace blender::ed::asset::index {

/**
 * Check files on disk against their expected hash / size in bytes.
 *
 * Instances of this class manage their own Disk File Hash Service for efficiently computing file
 * hashes.
 */
class FileStatusChecker {
 private:
  /** Absolute path to the cache directory for the remote asset library. */
  std::string library_root_path_;
  std::unique_ptr<asset_system::DiskFileHashService> dfhs_;

 public:
  explicit FileStatusChecker(StringRefNull library_root_path);
  ~FileStatusChecker() = default;

  /**
   * Determine the status of the file on disk.
   *
   * Once a file has been checked, its RemoteListingFileEntry is updated to reflect its status.
   * Subsequent checks just return that status, so this is efficient to call for each asset that
   * uses the file.
   */
  asset_system::RemoteAssetFileStatus remote_file_status(RemoteListingFileEntry &file_to_check);

 private:
  asset_system::RemoteAssetFileStatus remember(RemoteListingFileEntry &file_to_check,
                                               asset_system::RemoteAssetFileStatus status);
};

}  // namespace blender::ed::asset::index
