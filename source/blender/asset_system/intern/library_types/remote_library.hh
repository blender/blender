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

class RemoteAssetLibrary : public AssetLibrary {
  std::string remote_url_;
  /** Helper to get the #bUserAssetLibrary from the preferences (if still valid). */
  UserAssetLibraryWrapper user_library_;

 public:
  RemoteAssetLibrary(const bUserAssetLibrary &custom_library);
  std::optional<AssetLibraryReference> library_reference() const override;
  std::optional<eAssetImportMethod> import_method() const override;
  std::optional<StringRefNull> remote_url() const override;
  void refresh_catalogs() override;
  void load_or_reload_catalogs();
};

}  // namespace blender::asset_system
