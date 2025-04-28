/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "AS_asset_library.hh"

namespace blender::asset_system {

class OnDiskAssetLibrary : public AssetLibrary {
 public:
  OnDiskAssetLibrary(eAssetLibraryType library_type,
                     StringRef name = "",
                     StringRef root_path = "");

  std::optional<AssetLibraryReference> library_reference() const override;
  void refresh_catalogs() override;
  void load_or_reload_catalogs();
};

}  // namespace blender::asset_system
