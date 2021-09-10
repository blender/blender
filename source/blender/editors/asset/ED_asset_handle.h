/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup edasset
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
