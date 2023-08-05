/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "DNA_userdef_types.h"

#include "UI_resources.hh"

#include "RNA_define.h"

#include "ED_asset_library.h"

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

AssetLibraryReference ED_asset_library_reference_from_enum_value(int value)
{
  AssetLibraryReference library;

  /* Simple case: Predefined repository, just set the value. */
  if (value < ASSET_LIBRARY_CUSTOM) {
    library.type = value;
    library.custom_library_index = -1;
    BLI_assert(ELEM(value, ASSET_LIBRARY_ALL, ASSET_LIBRARY_LOCAL, ASSET_LIBRARY_ESSENTIALS));
    return library;
  }

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_from_index(
      &U, value - ASSET_LIBRARY_CUSTOM);

  /* Note that there is no check if the path exists here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  if (!user_library) {
    library.type = ASSET_LIBRARY_ALL;
    library.custom_library_index = -1;
  }
  else {
    const bool is_valid = (user_library->name[0] && user_library->dirpath[0]);
    if (is_valid) {
      library.custom_library_index = value - ASSET_LIBRARY_CUSTOM;
      library.type = ASSET_LIBRARY_CUSTOM;
    }
  }
  return library;
}

const EnumPropertyItem *ED_asset_library_reference_to_rna_enum_itemf(const bool include_generated)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (include_generated) {
    const EnumPropertyItem generated_items[] = {
        {ASSET_LIBRARY_ALL, "ALL", 0, "All", "Show assets from all of the listed asset libraries"},
        RNA_ENUM_ITEM_SEPR,
        {ASSET_LIBRARY_LOCAL,
         "LOCAL",
         ICON_CURRENT_FILE,
         "Current File",
         "Show the assets currently available in this Blender session"},
        {ASSET_LIBRARY_ESSENTIALS,
         "ESSENTIALS",
         0,
         "Essentials",
         "Show the basic building blocks and utilities coming with Blender"},
        {0, nullptr, 0, nullptr, nullptr},
    };

    /* Add predefined libraries that are generated and not simple directories that can be written
     * to. */
    RNA_enum_items_add(&item, &totitem, generated_items);
  }

  /* Add separator if needed. */
  if (!BLI_listbase_is_empty(&U.asset_libraries)) {
    RNA_enum_item_add_separator(&item, &totitem);
  }

  int i;
  LISTBASE_FOREACH_INDEX (bUserAssetLibrary *, user_library, &U.asset_libraries, i) {
    /* Note that the path itself isn't checked for validity here. If an invalid library path is
     * used, the Asset Browser can give a nice hint on what's wrong. */
    const bool is_valid = (user_library->name[0] && user_library->dirpath[0]);
    if (!is_valid) {
      continue;
    }

    AssetLibraryReference library_reference;
    library_reference.type = ASSET_LIBRARY_CUSTOM;
    library_reference.custom_library_index = i;

    const int enum_value = ED_asset_library_reference_to_enum_value(&library_reference);
    /* Use library path as description, it's a nice hint for users. */
    EnumPropertyItem tmp = {
        enum_value, user_library->name, ICON_NONE, user_library->name, user_library->dirpath};
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  RNA_enum_item_end(&item, &totitem);
  return item;
}
