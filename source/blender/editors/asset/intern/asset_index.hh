/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "ED_asset_indexer.hh"

struct AssetMetaData;
namespace blender {
class StringRefNull;
}  // namespace blender
namespace blender::io::serialize {
class DictionaryValue;
class Value;
}  // namespace blender::io::serialize

namespace blender::ed::asset::index {

struct RemoteListingAssetEntry;

std::unique_ptr<io::serialize::Value> read_contents(StringRefNull filepath);

AssetMetaData *asset_metadata_from_dictionary(const io::serialize::DictionaryValue &entry);

bool read_remote_listing_v1(StringRefNull root_dirpath,
                            Vector<RemoteListingAssetEntry> &io_entries);

}  // namespace blender::ed::asset::index
