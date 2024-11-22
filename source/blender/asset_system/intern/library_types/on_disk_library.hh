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

  void refresh_catalogs() override;
};

}  // namespace blender::asset_system
