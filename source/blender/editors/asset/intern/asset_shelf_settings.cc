/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * Internal and external APIs for #AssetShelfSettings.
 */

#include <type_traits>

#include "AS_asset_catalog_path.hh"

#include "DNA_screen_types.h"

#include "BLO_read_write.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BKE_asset.hh"
#include "BKE_preferences.h"
#include "BKE_screen.hh"

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

  if (active_catalog_path) {
    active_catalog_path = BLI_strdup(other.active_catalog_path);
  }
  BKE_asset_catalog_path_list_free(enabled_catalog_paths);
  enabled_catalog_paths = BKE_asset_catalog_path_list_duplicate(other.enabled_catalog_paths);

  return *this;
}

AssetShelfSettings::~AssetShelfSettings()
{
  BKE_asset_catalog_path_list_free(enabled_catalog_paths);
  MEM_delete(active_catalog_path);
}

namespace blender::ed::asset::shelf {

void settings_blend_write(BlendWriter *writer, const AssetShelfSettings &settings)
{
  BLO_write_struct(writer, AssetShelfSettings, &settings);

  BKE_asset_catalog_path_list_blend_write(writer, settings.enabled_catalog_paths);
  BLO_write_string(writer, settings.active_catalog_path);
}

void settings_blend_read_data(BlendDataReader *reader, AssetShelfSettings &settings)
{
  BKE_asset_catalog_path_list_blend_read_data(reader, settings.enabled_catalog_paths);
  BLO_read_string(reader, &settings.active_catalog_path);
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

static bool use_enabled_catalogs_from_prefs(const AssetShelf &shelf)
{
  return shelf.type && (shelf.type->flag & ASSET_SHELF_TYPE_FLAG_STORE_CATALOGS_IN_PREFS);
}

static const ListBase *get_enabled_catalog_path_list(const AssetShelf &shelf)
{
  if (use_enabled_catalogs_from_prefs(shelf)) {
    bUserAssetShelfSettings *pref_settings = BKE_preferences_asset_shelf_settings_get(
        &U, shelf.idname);
    return pref_settings ? &pref_settings->enabled_catalog_paths : nullptr;
  }
  return &shelf.settings.enabled_catalog_paths;
}

static ListBase *get_enabled_catalog_path_list(AssetShelf &shelf)
{
  return const_cast<ListBase *>(
      get_enabled_catalog_path_list(const_cast<const AssetShelf &>(shelf)));
}

void settings_clear_enabled_catalogs(AssetShelf &shelf)
{
  ListBase *enabled_catalog_paths = get_enabled_catalog_path_list(shelf);
  if (enabled_catalog_paths) {
    BKE_asset_catalog_path_list_free(*enabled_catalog_paths);
    BLI_assert(BLI_listbase_is_empty(enabled_catalog_paths));
  }
}

bool settings_is_catalog_path_enabled(const AssetShelf &shelf,
                                      const asset_system::AssetCatalogPath &path)
{
  const ListBase *enabled_catalog_paths = get_enabled_catalog_path_list(shelf);
  if (!enabled_catalog_paths) {
    return false;
  }

  return BKE_asset_catalog_path_list_has_path(*enabled_catalog_paths, path.c_str());
}

void settings_set_catalog_path_enabled(AssetShelf &shelf,
                                       const asset_system::AssetCatalogPath &path)
{
  if (use_enabled_catalogs_from_prefs(shelf)) {
    if (BKE_preferences_asset_shelf_settings_ensure_catalog_path_enabled(
            &U, shelf.idname, path.c_str()))
    {
      U.runtime.is_dirty = true;
    }
  }
  else {
    if (!BKE_asset_catalog_path_list_has_path(shelf.settings.enabled_catalog_paths, path.c_str()))
    {
      BKE_asset_catalog_path_list_add_path(shelf.settings.enabled_catalog_paths, path.c_str());
    }
  }
}

void settings_foreach_enabled_catalog_path(
    const AssetShelf &shelf,
    FunctionRef<void(const asset_system::AssetCatalogPath &catalog_path)> fn)
{
  const ListBase *enabled_catalog_paths = get_enabled_catalog_path_list(shelf);
  if (!enabled_catalog_paths) {
    return;
  }

  LISTBASE_FOREACH (const AssetCatalogPathLink *, path_link, enabled_catalog_paths) {
    fn(asset_system::AssetCatalogPath(path_link->path));
  }
}

}  // namespace blender::ed::asset::shelf
