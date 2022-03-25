/* SPDX-License-Identifier: GPL-2.0-or-later */

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

#ifdef __cplusplus
extern "C" {
#endif

struct AssetHandle;
struct AssetLibraryReference;
struct bContext;

const char *ED_asset_handle_get_name(const struct AssetHandle *asset);
struct AssetMetaData *ED_asset_handle_get_metadata(const struct AssetHandle *asset);
struct ID *ED_asset_handle_get_local_id(const struct AssetHandle *asset);
ID_Type ED_asset_handle_get_id_type(const struct AssetHandle *asset);
int ED_asset_handle_get_preview_icon_id(const struct AssetHandle *asset);
void ED_asset_handle_get_full_library_path(const struct bContext *C,
                                           const struct AssetLibraryReference *asset_library_ref,
                                           const struct AssetHandle *asset,
                                           char r_full_lib_path[]);

#ifdef __cplusplus
}
#endif
