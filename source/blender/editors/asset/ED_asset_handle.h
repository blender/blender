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

#ifdef __cplusplus
namespace blender::asset_system {
class AssetRepresentation;
}
using AssetRepresentationHandle = blender::asset_system::AssetRepresentation;
#else
typedef struct AssetRepresentationHandle AssetRepresentationHandle;
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct AssetHandle;

AssetRepresentationHandle *ED_asset_handle_get_representation(const struct AssetHandle *asset);
const char *ED_asset_handle_get_name(const AssetHandle *asset_handle);
const char *ED_asset_handle_get_identifier(const struct AssetHandle *asset);
ID_Type ED_asset_handle_get_id_type(const struct AssetHandle *asset);
int ED_asset_handle_get_preview_icon_id(const struct AssetHandle *asset);
int ED_asset_handle_get_preview_or_type_icon_id(const struct AssetHandle *asset);
void ED_asset_handle_get_full_library_path(
    const struct AssetHandle *asset,
    /* `1024` for #FILE_MAX,
     * rely on warnings to let us know if this gets out of sync. */
    char r_full_lib_path[1024]);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace blender::ed::asset {

PointerRNA create_asset_rna_ptr(const asset_system::AssetRepresentation *asset);

}

#endif
