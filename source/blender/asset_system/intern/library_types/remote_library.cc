/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "remote_library.hh"

namespace blender::asset_system {

RemoteAssetLibrary::RemoteAssetLibrary() : AssetLibrary(ASSET_LIBRARY_CUSTOM)
{
  import_method_ = ASSET_IMPORT_APPEND_REUSE;
  may_override_import_method_ = false;
}

}  // namespace blender::asset_system
