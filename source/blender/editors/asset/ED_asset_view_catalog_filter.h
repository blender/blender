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

#include "DNA_space_types.h"

struct AssetLibrary;
struct AssetMetaData;
struct bUUID;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AssetViewCatalogFilterSettingsHandle AssetViewCatalogFilterSettingsHandle;

AssetViewCatalogFilterSettingsHandle *asset_view_create_catalog_filter_settings(void);
void asset_view_delete_catalog_filter_settings(
    AssetViewCatalogFilterSettingsHandle **filter_settings_handle);
bool asset_view_set_catalog_filter_settings(
    AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    AssetCatalogFilterMode catalog_visibility,
    bUUID catalog_id);
void asset_view_ensure_updated_catalog_filter_data(
    AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    const AssetLibrary *asset_library);
bool asset_view_is_asset_visible_in_catalog_filter_settings(
    const AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    const AssetMetaData *asset_data);

#ifdef __cplusplus
}
#endif
