/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "AS_asset_library.hh"

namespace blender::asset_system {

class RemoteAssetLibrary : public AssetLibrary {
  std::string remote_url_;

 public:
  RemoteAssetLibrary(StringRef remote_url, StringRef name, StringRef cache_root_path);
  std::optional<AssetLibraryReference> library_reference() const override;
  std::optional<StringRefNull> remote_url() const override;
  void refresh_catalogs() override;
  void load_or_reload_catalogs();
};

}  // namespace blender::asset_system
