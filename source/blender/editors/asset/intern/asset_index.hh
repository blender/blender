/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include "ED_asset_indexer.hh"

#include <memory>

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

enum class ReadingResult {
  Success,
  Failure,
  Cancelled,
};

/**
 * Reading of API schema version 1. See #read_remote_listing() on \a process_fn.
 * \param version_root_dirpath: Absolute path to the remote listing root directory.
 */
ReadingResult read_remote_listing_v1(StringRefNull listing_root_dirpath,
                                     RemoteListingEntryProcessFn process_fn,
                                     RemoteListingWaitForPagesFn wait_fn = nullptr);

}  // namespace blender::ed::asset::index
