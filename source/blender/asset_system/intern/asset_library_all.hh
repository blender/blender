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
  bool catalogs_dirty_ = true;

 public:
  AllAssetLibrary();

  void refresh_catalogs() override;

  /**
   * Update the available catalogs and catalog tree from the nested asset libraries. Completely
   * recreates the catalog service (invalidating pointers to the previous one).
   *
   * \param reload_nested_catalogs: Re-read catalog definitions of nested libraries from disk and
   * merge them into the in-memory representations.
   */
  void rebuild_catalogs_from_nested(bool reload_nested_catalogs);

  void tag_catalogs_dirty();
  bool is_catalogs_dirty();
};

}  // namespace blender::asset_system
