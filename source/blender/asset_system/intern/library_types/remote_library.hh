/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "common.hh"

#include "AS_asset_library.hh"

namespace blender::asset_system {

/**
 * Abstract class for remote libraries. #PreferencesRemoteAssetLibrary and #OnlineEssentialsLibrary
 * derive from this.
 */
class RemoteAssetLibrary : public AssetLibrary {
  std::string remote_url_;

 public:
  RemoteAssetLibrary(eAssetLibraryType library_type,
                     bool is_read_only,
                     StringRef remote_url,
                     StringRef name,
                     StringRef root_path);
  void force_remote_listing_download() const override;

  std::optional<eAssetImportMethod> import_method() const override;
  std::optional<StringRefNull> remote_url() const override;
  void refresh_catalogs() override;
};

class PreferencesRemoteAssetLibrary : public RemoteAssetLibrary {
  /** Helper to get the #bUserAssetLibrary from the preferences (if still valid). */
  UserAssetLibraryWrapper user_library_;

 public:
  PreferencesRemoteAssetLibrary(const bUserAssetLibrary &custom_library);
  std::optional<AssetLibraryReference> library_reference() const override;
  bool is_enabled() const;
};

}  // namespace blender::asset_system
