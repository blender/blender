/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "common.hh"

#include "on_disk_library.hh"

namespace blender::asset_system {

class PreferencesOnDiskAssetLibrary : public OnDiskAssetLibrary {
  /** Helper to get the #bUserAssetLibrary from the preferences (if still valid). */
  UserAssetLibraryWrapper user_library_;

 public:
  explicit PreferencesOnDiskAssetLibrary(const bUserAssetLibrary &user_asset_library);

  std::optional<AssetLibraryReference> library_reference() const override;
  std::optional<eAssetImportMethod> import_method() const override;
  bool use_relative_paths() const override;
  bool is_enabled() const override;
};

}  // namespace blender::asset_system
