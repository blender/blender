/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#include <string>

#include "BLI_function_ref.hh"

struct AssetHandle;
struct AssetLibraryReference;
struct FileDirEntry;
struct bContext;

namespace blender::asset_system {
class AssetLibrary;
}

/**
 * Get the asset library being read into an asset-list and identified using \a library_reference.
 * \note The asset library may be loaded asynchronously, so this may return null until it becomes
 *       available.
 */
blender::asset_system::AssetLibrary *ED_assetlist_library_get(
    const AssetLibraryReference &library_reference);

/* Can return false to stop iterating. */
using AssetListIterFn = blender::FunctionRef<bool(AssetHandle)>;
void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn);
