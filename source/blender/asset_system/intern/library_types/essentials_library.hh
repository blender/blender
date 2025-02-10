/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "on_disk_library.hh"

namespace blender::asset_system {

class EssentialsAssetLibrary : public OnDiskAssetLibrary {
 public:
  EssentialsAssetLibrary();

  std::optional<AssetLibraryReference> library_reference() const override;
};

}  // namespace blender::asset_system
