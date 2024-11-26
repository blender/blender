/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "BLI_vector.hh"

#include "ED_file_indexer.hh"

struct AssetLibraryReference;

namespace blender {
class StringRefNull;
}

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

struct RemoteIndexAssetEntry {
  BLODataBlockInfo datablock_info = {};
  short idcode = 0;

  std::string archive_url;
  std::string thumbnail_url;

  RemoteIndexAssetEntry() = default;
  RemoteIndexAssetEntry(const RemoteIndexAssetEntry &) = delete;
  RemoteIndexAssetEntry &operator=(const RemoteIndexAssetEntry &) = delete;
  RemoteIndexAssetEntry(RemoteIndexAssetEntry &&);
  RemoteIndexAssetEntry &operator=(RemoteIndexAssetEntry &&);
  ~RemoteIndexAssetEntry();
};

bool read_remote_index(StringRefNull root_dirpath, Vector<RemoteIndexAssetEntry> *r_entries);

}  // namespace blender::ed::asset::index
