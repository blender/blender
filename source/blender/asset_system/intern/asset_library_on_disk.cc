/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "asset_library_on_disk.hh"

namespace blender::asset_system {

OnDiskAssetLibrary::OnDiskAssetLibrary(eAssetLibraryType library_type,
                                       StringRef name,
                                       StringRef root_path)
    : AssetLibrary(library_type, name, root_path)
{
  this->on_blend_save_handler_register();
}

void OnDiskAssetLibrary::refresh_catalogs()
{
  catalog_service->reload_catalogs();
}

}  // namespace blender::asset_system
