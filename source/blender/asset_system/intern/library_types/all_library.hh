/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include <atomic>
#include <mutex>

#include "AS_asset_library.hh"

namespace blender::asset_system {

class AllAssetLibrary : public AssetLibrary {
  std::atomic<bool> catalogs_dirty_ = true;

  /** Serializes #rebuild_catalogs_from_nested so only one thread rebuilds at a time. */
  std::mutex rebuild_mutex_;

 public:
  AllAssetLibrary();

  void force_remote_listing_download() const override;

  std::optional<AssetLibraryReference> library_reference() const override;
  std::optional<eAssetImportMethod> import_method() const override;
  void refresh_catalogs() override;

  /**
   * Update the available catalogs and catalog tree from the nested asset libraries. Completely
   * recreates the catalog service (invalidating pointers to the previous one).
   *
   * \param reload_nested_catalogs: Re-read catalog definitions of nested libraries from disk and
   * merge them into the in-memory representations.
   */
  void rebuild_catalogs_from_nested(bool reload_nested_catalogs);

  void tag_catalogs_dirty();
  bool is_catalogs_dirty() const;
};

}  // namespace blender::asset_system
