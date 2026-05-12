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
                     StringRef name,
                     StringRef root_path,
                     bool is_read_only);

  std::optional<AssetLibraryReference> library_reference() const override;
  std::optional<eAssetImportMethod> import_method() const override;
  void refresh_catalogs() override;

  virtual bool is_enabled() const;
};

}  // namespace blender::asset_system
