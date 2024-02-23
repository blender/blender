/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "asset_library_runtime.hh"

namespace blender::asset_system {

RuntimeAssetLibrary::RuntimeAssetLibrary() : AssetLibrary(ASSET_LIBRARY_LOCAL)
{
  this->on_blend_save_handler_register();
}

}  // namespace blender::asset_system
