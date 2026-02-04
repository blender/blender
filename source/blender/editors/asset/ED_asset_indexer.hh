/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include <filesystem>
#include <optional>

#include "BLI_function_ref.hh"
#include "BLI_utility_mixins.hh"

#include "AS_remote_library.hh"

#include "ED_file_indexer.hh"

namespace blender {
class StringRefNull;
}  // namespace blender

namespace blender::ed::asset::index {

/**
 * File Indexer Service for indexing asset files.
 *
 * Opening and parsing a large collection of asset files inside a library can take a lot of time.
 * To reduce the time it takes the files are indexed.
 *
 * - Index files are created for each blend file in the asset library, even when the blend file
 *   doesn't contain any assets.
 * - Indexes are stored in an persistent cache folder (`BKE_appdir_folder_caches` +
 *   `asset_library_indexes/{asset_library_dir}/{asset_index_file.json}`).
 * - The content of the indexes are used when:
 *   - Index exists and can be opened
 *   - Last modification date is earlier than the file it represents.
 *   - The index file version is the latest.
 * - Blend files without any assets can be determined by the size of the index file for some
 *   additional performance.
 */
extern const FileIndexerType file_indexer_asset;

struct RemoteListingAssetEntry : NonCopyable {
  BLODataBlockInfo datablock_info = {};
  short idcode = 0;

  asset_system::OnlineAssetInfo online_info;

  RemoteListingAssetEntry() = default;
  RemoteListingAssetEntry(RemoteListingAssetEntry &&);
  RemoteListingAssetEntry &operator=(RemoteListingAssetEntry &&);
  ~RemoteListingAssetEntry();
};

/**
 * Representation of the FileV1 type in the OpenAPI definition.
 * See blender_asset_library_openapi.yaml.
 *
 * Not all fields are included here, just the ones that are used by Blender.
 */
struct RemoteListingFileEntry : NonCopyable {
  std::string local_path;
  asset_system::URLWithHash download_url;

  RemoteListingFileEntry() = default;
  RemoteListingFileEntry(RemoteListingFileEntry &&);
  RemoteListingFileEntry &operator=(RemoteListingFileEntry &&);
  ~RemoteListingFileEntry() = default;
};

using RemoteListingEntryProcessFn = FunctionRef<bool(RemoteListingAssetEntry &)>;
using RemoteListingWaitForPagesFn = FunctionRef<bool()>;
/* Uses #std::filesystem::file_time_type because it needs to be compared against file time-stamps,
 * which may have low precision (often just 1 sec). */
using Timestamp = std::filesystem::file_time_type;
/**
 * \param process_fn: Called for each asset entry read from the listing. It's fine to move out the
 *   passed #RemoteListingAssetEntry. Returning false will cancel the whole reading process and not
 *   read any further entries.
 * \param wait_fn: If this is set, reading will keep retrying to load unavailable pages, and call
 *   this wait function for each try. The wait function can block until it thinks new pages might
 *   be available. If this returns false the whole reading process will be cancelled.
 */
bool read_remote_listing(StringRefNull root_dirpath,
                         StringRefNull asset_library_name,
                         ReportList &reports,
                         RemoteListingEntryProcessFn process_fn,
                         RemoteListingWaitForPagesFn wait_fn = nullptr,
                         std::optional<Timestamp> ignore_before_timestamp = std::nullopt);

}  // namespace blender::ed::asset::index
