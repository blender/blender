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
#include <string>

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"

#include "BLO_readfile.h"

#include "DNA_ID.h"
#include "DNA_asset_types.h"
#include "DNA_space_types.h"

#include "UI_interface_icons.h"

#include "RNA_access.h"

#include "ED_asset.h"

using namespace blender;

bool ED_asset_mark_id(const bContext *C, ID *id)
{
  if (id->asset_data) {
    return false;
  }
  if (!BKE_id_can_be_asset(id)) {
    return false;
  }

  id_fake_user_set(id);

  id->asset_data = BKE_asset_metadata_create();

  UI_icon_render_id(C, nullptr, id, ICON_SIZE_PREVIEW, true);

  /* Important for asset storage to update properly! */
  ED_assetlist_storage_tag_main_data_dirty();

  return true;
}

bool ED_asset_clear_id(ID *id)
{
  if (!id->asset_data) {
    return false;
  }
  BKE_asset_metadata_free(&id->asset_data);
  /* Don't clear fake user here, there's no guarantee that it was actually set by
   * #ED_asset_mark_id(), it might have been something/someone else. */

  /* Important for asset storage to update properly! */
  ED_assetlist_storage_tag_main_data_dirty();

  return true;
}

bool ED_asset_can_make_single_from_context(const bContext *C)
{
  /* Context needs a "id" pointer to be set for #ASSET_OT_mark()/#ASSET_OT_clear() to use. */
  return CTX_data_pointer_get_type_silent(C, "id", &RNA_ID).data != nullptr;
}

/* TODO better place? */
/* TODO What about the setter and the `itemf` callback? */
#include "BKE_preferences.h"
#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"
int ED_asset_library_reference_to_enum_value(const AssetLibraryReference *library)
{
  /* Simple case: Predefined repository, just set the value. */
  if (library->type < ASSET_LIBRARY_CUSTOM) {
    return library->type;
  }

  /* Note that the path isn't checked for validity here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_from_index(
      &U, library->custom_library_index);
  if (user_library) {
    return ASSET_LIBRARY_CUSTOM + library->custom_library_index;
  }

  BLI_assert(0);
  return ASSET_LIBRARY_LOCAL;
}

AssetLibraryReference ED_asset_library_reference_from_enum_value(int value)
{
  AssetLibraryReference library;

  /* Simple case: Predefined repository, just set the value. */
  if (value < ASSET_LIBRARY_CUSTOM) {
    library.type = value;
    library.custom_library_index = -1;
    BLI_assert(ELEM(value, ASSET_LIBRARY_LOCAL));
    return library;
  }

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_from_index(
      &U, value - ASSET_LIBRARY_CUSTOM);

  /* Note that the path isn't checked for validity here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  const bool is_valid = (user_library->name[0] && user_library->path[0]);
  if (!user_library) {
    library.type = ASSET_LIBRARY_LOCAL;
    library.custom_library_index = -1;
  }
  else if (user_library && is_valid) {
    library.custom_library_index = value - ASSET_LIBRARY_CUSTOM;
    library.type = ASSET_LIBRARY_CUSTOM;
  }
  return library;
}

const char *ED_asset_handle_get_name(const AssetHandle *asset)
{
  return asset->file_data->name;
}

void ED_asset_handle_get_full_library_path(const bContext *C,
                                           const AssetLibraryReference *asset_library,
                                           const AssetHandle *asset,
                                           char r_full_lib_path[FILE_MAX_LIBEXTRA])
{
  *r_full_lib_path = '\0';

  std::string asset_path = ED_assetlist_asset_filepath_get(C, *asset_library, *asset);
  if (asset_path.empty()) {
    return;
  }

  BLO_library_path_explode(asset_path.c_str(), r_full_lib_path, nullptr, nullptr);
}
