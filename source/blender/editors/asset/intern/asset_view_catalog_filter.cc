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

#include <memory>

#include "DNA_space_types.h"

#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"

#include "ED_asset_view_catalog_filter.h"

namespace bke = blender::bke;

struct AssetViewCatalogFilter {
  AssetCatalogFilterSettings filter_settings;
  std::unique_ptr<bke::AssetCatalogFilter> catalog_filter;
};

AssetViewCatalogFilterSettingsHandle *asset_view_create_catalog_filter_settings()
{
  AssetViewCatalogFilter *filter_settings = MEM_new<AssetViewCatalogFilter>(__func__);
  return reinterpret_cast<AssetViewCatalogFilterSettingsHandle *>(filter_settings);
}

void asset_view_delete_catalog_filter_settings(
    AssetViewCatalogFilterSettingsHandle **filter_settings_handle)
{
  AssetViewCatalogFilter **filter_settings = reinterpret_cast<AssetViewCatalogFilter **>(
      filter_settings_handle);
  MEM_delete(*filter_settings);
  *filter_settings = nullptr;
}

bool asset_view_set_catalog_filter_settings(
    AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    AssetCatalogFilterMode catalog_visibility,
    ::bUUID catalog_id)
{
  AssetViewCatalogFilter *filter_settings = reinterpret_cast<AssetViewCatalogFilter *>(
      filter_settings_handle);
  bool needs_update = false;

  if (filter_settings->filter_settings.filter_mode != catalog_visibility) {
    filter_settings->filter_settings.filter_mode = catalog_visibility;
    needs_update = true;
  }

  if (filter_settings->filter_settings.filter_mode == ASSET_CATALOG_SHOW_ASSETS_FROM_CATALOG &&
      !BLI_uuid_equal(filter_settings->filter_settings.active_catalog_id, catalog_id)) {
    filter_settings->filter_settings.active_catalog_id = catalog_id;
    needs_update = true;
  }

  return needs_update;
}

void asset_view_ensure_updated_catalog_filter_data(
    AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    const ::AssetLibrary *asset_library)
{
  AssetViewCatalogFilter *filter_settings = reinterpret_cast<AssetViewCatalogFilter *>(
      filter_settings_handle);
  const bke::AssetCatalogService *catalog_service = BKE_asset_library_get_catalog_service(
      asset_library);

  if (filter_settings->filter_settings.filter_mode != ASSET_CATALOG_SHOW_ALL_ASSETS) {
    filter_settings->catalog_filter = std::make_unique<bke::AssetCatalogFilter>(
        catalog_service->create_catalog_filter(
            filter_settings->filter_settings.active_catalog_id));
  }
}

bool asset_view_is_asset_visible_in_catalog_filter_settings(
    const AssetViewCatalogFilterSettingsHandle *filter_settings_handle,
    const AssetMetaData *asset_data)
{
  const AssetViewCatalogFilter *filter_settings = reinterpret_cast<const AssetViewCatalogFilter *>(
      filter_settings_handle);

  switch (filter_settings->filter_settings.filter_mode) {
    case ASSET_CATALOG_SHOW_ASSETS_WITHOUT_CATALOG:
      return !filter_settings->catalog_filter->is_known(asset_data->catalog_id);
    case ASSET_CATALOG_SHOW_ASSETS_FROM_CATALOG:
      return filter_settings->catalog_filter->contains(asset_data->catalog_id);
    case ASSET_CATALOG_SHOW_ALL_ASSETS:
      /* All asset files should be visible. */
      return true;
  }

  BLI_assert_unreachable();
  return false;
}
