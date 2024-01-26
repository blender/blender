/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Asset-handle is a temporary design, not part of the core asset system design.
 *
 * Currently asset-list items are just file directory items (#FileDirEntry). So an asset-handle
 * just wraps a pointer to this. We try to abstract away the fact that it's just a file entry,
 * although that doesn't always work (see #rna_def_asset_handle()).
 */

#pragma once

#include "DNA_ID_enums.h"
#include "DNA_asset_types.h"

#include "RNA_types.hh"

struct AssetHandle;
namespace blender::asset_system {
class AssetRepresentation;
}

namespace blender::ed::asset {

asset_system::AssetRepresentation *handle_get_representation(const AssetHandle *asset);
int handle_get_preview_icon_id(const AssetHandle *asset);
int handle_get_preview_or_type_icon_id(const AssetHandle *asset);

}  // namespace blender::ed::asset
