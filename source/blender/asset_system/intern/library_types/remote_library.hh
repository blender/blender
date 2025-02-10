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
  RemoteAssetLibrary(StringRef remote_url);
  std::optional<AssetLibraryReference> library_reference() const override;
};

}  // namespace blender::asset_system
