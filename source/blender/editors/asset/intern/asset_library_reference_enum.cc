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
 *
 * Helpers to convert asset library references from and to enum values and RNA enums.
 * In some cases it's simply not possible to reference an asset library with
 * #AssetLibraryReferences. This API guarantees a safe translation to indices/enum values for as
 * long as there is no change in the order of registered custom asset libraries.
 */

#include "BLI_listbase.h"

#include "BKE_preferences.h"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "UI_resources.h"

#include "RNA_define.h"

#include "ED_asset_library.h"

/**
 * Return an index that can be used to uniquely identify \a library, assuming
 * that all relevant indices were created with this function.
 */
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

  return ASSET_LIBRARY_LOCAL;
}

/**
 * Return an asset library reference matching the index returned by
 * #ED_asset_library_reference_to_enum_value().
 */
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

  /* Note that there is no check if the path exists here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  if (!user_library) {
    library.type = ASSET_LIBRARY_LOCAL;
    library.custom_library_index = -1;
  }
  else {
    const bool is_valid = (user_library->name[0] && user_library->path[0]);
    if (is_valid) {
      library.custom_library_index = value - ASSET_LIBRARY_CUSTOM;
      library.type = ASSET_LIBRARY_CUSTOM;
    }
  }
  return library;
}

/**
 * Translate all available asset libraries to an RNA enum, whereby the enum values match the result
 * of #ED_asset_library_reference_to_enum_value() for any given library.
 *
 * Since this is meant for UI display, skips non-displayable libraries, that is, libraries with an
 * empty name or path.
 */
const EnumPropertyItem *ED_asset_library_reference_to_rna_enum_itemf()
{
  const EnumPropertyItem predefined_items[] = {
      /* For the future. */
      // {ASSET_REPO_BUNDLED, "BUNDLED", 0, "Bundled", "Show the default user assets"},
      {ASSET_LIBRARY_LOCAL,
       "LOCAL",
       ICON_BLENDER,
       "Current File",
       "Show the assets currently available in this Blender session"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  /* Add predefined items. */
  RNA_enum_items_add(&item, &totitem, predefined_items);

  /* Add separator if needed. */
  if (!BLI_listbase_is_empty(&U.asset_libraries)) {
    RNA_enum_item_add_separator(&item, &totitem);
  }

  int i = 0;
  for (bUserAssetLibrary *user_library = (bUserAssetLibrary *)U.asset_libraries.first;
       user_library;
       user_library = user_library->next, i++) {
    /* Note that the path itself isn't checked for validity here. If an invalid library path is
     * used, the Asset Browser can give a nice hint on what's wrong. */
    const bool is_valid = (user_library->name[0] && user_library->path[0]);
    if (!is_valid) {
      continue;
    }

    AssetLibraryReference library_reference;
    library_reference.type = ASSET_LIBRARY_CUSTOM;
    library_reference.custom_library_index = i;

    const int enum_value = ED_asset_library_reference_to_enum_value(&library_reference);
    /* Use library path as description, it's a nice hint for users. */
    EnumPropertyItem tmp = {
        enum_value, user_library->name, ICON_NONE, user_library->name, user_library->path};
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  return item;
}
