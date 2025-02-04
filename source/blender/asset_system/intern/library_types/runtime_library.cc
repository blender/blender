/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "runtime_library.hh"

namespace blender::asset_system {

RuntimeAssetLibrary::RuntimeAssetLibrary() : AssetLibrary(ASSET_LIBRARY_LOCAL)
{
  this->on_blend_save_handler_register();
}

std::optional<AssetLibraryReference> RuntimeAssetLibrary::library_reference() const
{
  AssetLibraryReference library_ref{};
  library_ref.type = ASSET_LIBRARY_LOCAL;
  library_ref.custom_library_index = -1;
  return library_ref;
}

}  // namespace blender::asset_system
