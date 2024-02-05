/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#pragma once

#include "AS_asset_library.hh"

namespace blender::asset_system {

class AllAssetLibrary : public AssetLibrary {
 public:
  AllAssetLibrary();

  void refresh_catalogs() override;

  void rebuild(const bool reload_catalogs);
};

}  // namespace blender::asset_system
