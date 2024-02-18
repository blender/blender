/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup asset_system
 */

#include "asset_library_from_preferences.hh"

namespace blender::asset_system {

PreferencesOnDiskAssetLibrary::PreferencesOnDiskAssetLibrary(StringRef name, StringRef root_path)
    : OnDiskAssetLibrary(ASSET_LIBRARY_CUSTOM, name, root_path)
{
}

}  // namespace blender::asset_system
