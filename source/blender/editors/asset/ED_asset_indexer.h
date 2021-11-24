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
 * \ingroup edasset
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ED_file_indexer.h"

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

#ifdef __cplusplus
}
#endif
