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

std::string ED_assetlist_asset_filepath_get(const bContext *C,
                                            const AssetLibraryReference &library_reference,
                                            const AssetHandle &asset_handle);

/* Can return false to stop iterating. */
using AssetListIterFn = blender::FunctionRef<bool(AssetHandle)>;
/**
 * Iterate the currently loaded assets for the referenced asset library, calling \a fn for each
 * asset. This may be executed while the asset list is loading asynchronously. Assets will then be
 * included as they get done loading.
 */
void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn);
