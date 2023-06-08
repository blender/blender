/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

#ifdef __cplusplus
extern "C" {
#endif

struct AssetHandle;

struct AssetRepresentation *ED_asset_handle_get_representation(const struct AssetHandle *asset);
const char *ED_asset_handle_get_name(const struct AssetHandle *asset);
struct AssetMetaData *ED_asset_handle_get_metadata(const struct AssetHandle *asset);
struct ID *ED_asset_handle_get_local_id(const struct AssetHandle *asset);
ID_Type ED_asset_handle_get_id_type(const struct AssetHandle *asset);
int ED_asset_handle_get_preview_icon_id(const struct AssetHandle *asset);
void ED_asset_handle_get_full_library_path(
    const struct AssetHandle *asset,
    /* `1024` for #FILE_MAX,
     * rely on warnings to let us know if this gets out of sync. */
    char r_full_lib_path[1024]);
bool ED_asset_handle_get_use_relative_path(const struct AssetHandle *asset);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#  include <optional>

#  include "BLI_string_ref.hh"

/** The asset library may have an import method (e.g. append vs. link) defined to use. If so, this
 * returns it. Otherwise a reasonable method should be used, usually "Append (Reuse Data)". */
std::optional<eAssetImportMethod> ED_asset_handle_get_import_method(
    const struct AssetHandle *asset);

blender::StringRefNull ED_asset_handle_get_library_relative_identifier(const AssetHandle &asset);

#endif
