/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Internal and external APIs for #AssetShelfSettings.
 */

#include <type_traits>

#include "AS_asset_catalog_path.hh"

#include "DNA_screen_types.h"

#include "BLO_read_write.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "asset_shelf.hh"

using namespace blender;
using namespace blender::ed::asset;

AssetShelfSettings::AssetShelfSettings()
{
  memset(this, 0, sizeof(*this));
}

AssetShelfSettings::AssetShelfSettings(const AssetShelfSettings &other)
{
  operator=(other);
}

AssetShelfSettings &AssetShelfSettings::operator=(const AssetShelfSettings &other)
{
  /* Start with a shallow copy. */
  memcpy(this, &other, sizeof(AssetShelfSettings));

  next = prev = nullptr;

  active_catalog_path = BLI_strdup(other.active_catalog_path);
  BLI_listbase_clear(&enabled_catalog_paths);

  LISTBASE_FOREACH (LinkData *, catalog_path_item, &other.enabled_catalog_paths) {
    LinkData *new_path_item = BLI_genericNodeN(BLI_strdup((char *)catalog_path_item->data));
    BLI_addtail(&enabled_catalog_paths, new_path_item);
  }
  return *this;
}

AssetShelfSettings::~AssetShelfSettings()
{
  shelf::settings_clear_enabled_catalogs(*this);
  MEM_delete(active_catalog_path);
}

namespace blender::ed::asset::shelf {

void settings_blend_write(BlendWriter *writer, const AssetShelfSettings &settings)
{
  BLO_write_struct(writer, AssetShelfSettings, &settings);

  LISTBASE_FOREACH (LinkData *, catalog_path_item, &settings.enabled_catalog_paths) {
    BLO_write_struct(writer, LinkData, catalog_path_item);
    BLO_write_string(writer, (const char *)catalog_path_item->data);
  }

  BLO_write_string(writer, settings.active_catalog_path);
}

void settings_blend_read_data(BlendDataReader *reader, AssetShelfSettings &settings)
{
  BLO_read_list(reader, &settings.enabled_catalog_paths);
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &settings.enabled_catalog_paths) {
    BLO_read_data_address(reader, &catalog_path_item->data);
  }
  BLO_read_data_address(reader, &settings.active_catalog_path);
}

void settings_clear_enabled_catalogs(AssetShelfSettings &settings)
{
  LISTBASE_FOREACH_MUTABLE (LinkData *, catalog_path_item, &settings.enabled_catalog_paths) {
    MEM_freeN(catalog_path_item->data);
    BLI_freelinkN(&settings.enabled_catalog_paths, catalog_path_item);
  }
  BLI_assert(BLI_listbase_is_empty(&settings.enabled_catalog_paths));
}

void settings_set_active_catalog(AssetShelfSettings &settings,
                                 const asset_system::AssetCatalogPath &path)
{
  MEM_delete(settings.active_catalog_path);
  settings.active_catalog_path = BLI_strdupn(path.c_str(), path.length());
}

void settings_set_all_catalog_active(AssetShelfSettings &settings)
{
  MEM_delete(settings.active_catalog_path);
  settings.active_catalog_path = nullptr;
}

bool settings_is_active_catalog(const AssetShelfSettings &settings,
                                const asset_system::AssetCatalogPath &path)
{
  return settings.active_catalog_path && settings.active_catalog_path == path.str();
}

bool settings_is_all_catalog_active(const AssetShelfSettings &settings)
{
  return !settings.active_catalog_path || !settings.active_catalog_path[0];
}

bool settings_is_catalog_path_enabled(const AssetShelfSettings &settings,
                                      const asset_system::AssetCatalogPath &path)
{
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &settings.enabled_catalog_paths) {
    if (StringRef((const char *)catalog_path_item->data) == path.str()) {
      return true;
    }
  }
  return false;
}

void settings_set_catalog_path_enabled(AssetShelfSettings &settings,
                                       const asset_system::AssetCatalogPath &path)
{
  char *path_copy = BLI_strdupn(path.c_str(), path.length());
  BLI_addtail(&settings.enabled_catalog_paths, BLI_genericNodeN(path_copy));
}

void settings_foreach_enabled_catalog_path(
    const AssetShelfSettings &settings,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn)
{
  LISTBASE_FOREACH (LinkData *, catalog_path_item, &settings.enabled_catalog_paths) {
    fn(asset_system::AssetCatalogPath((char *)catalog_path_item->data));
  }
}

}  // namespace blender::ed::asset::shelf
