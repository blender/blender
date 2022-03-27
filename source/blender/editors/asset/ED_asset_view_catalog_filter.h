/* SPDX-License-Identifier: GPL-2.0-or-later */

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
