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

/* Can return false to stop iterating. */
using AssetListIterFn = blender::FunctionRef<bool(AssetHandle)>;
void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn);
