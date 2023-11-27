/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.hh"
#include "BKE_context.hh"

#include "DNA_userdef_types.h"

#include "UI_resources.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "ED_asset_library.h"

using namespace blender;

int ED_asset_library_reference_to_enum_value(const AssetLibraryReference *library)
{
  /* Simple case: Predefined library, just set the value. */
  if (library->type < ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES) {
    return library->type;
  }

  /* Note that the path isn't checked for validity here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  if (ED_asset_library_find_custom_library_from_reference(library)) {
    return library->type + library->custom_library_index;
  }

  return ASSET_LIBRARY_LOCAL;
}

AssetLibraryReference ED_asset_library_reference_from_enum_value(int value)
{
  AssetLibraryReference library;

  /* Simple case: Predefined repository, just set the value. */
  if (value < ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES) {
    library.type = value;
    library.custom_library_index = -1;
    BLI_assert(ELEM(value, ASSET_LIBRARY_ALL, ASSET_LIBRARY_LOCAL, ASSET_LIBRARY_ESSENTIALS));
    return library;
  }

  const eAssetLibraryType type = (value < ASSET_LIBRARY_CUSTOM_FROM_PROJECT) ?
                                     ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES :
                                     ASSET_LIBRARY_CUSTOM_FROM_PROJECT;

  const CustomAssetLibraryDefinition *custom_library = nullptr;

  library.type = type;
  library.custom_library_index = value - type;

  {
    custom_library = ED_asset_library_find_custom_library_from_reference(&library);

    /* Note that there is no check if the path exists here. If an invalid library path is used, the
     * Asset Browser can give a nice hint on what's wrong. */
    const bool is_valid = custom_library &&
                          (custom_library->name[0] && custom_library->dirpath[0]);
    if (!is_valid) {
      library.custom_library_index = -1;
    }
  }

  return library;
}

static void add_custom_asset_library_enum_items(
    const ListBase & /*CustomAssetLibraryDefinition*/ libraries,
    const eAssetLibraryType library_type,
    EnumPropertyItem **items,
    int *totitem)
{
  int i;
  LISTBASE_FOREACH_INDEX (CustomAssetLibraryDefinition *, custom_library, &libraries, i) {
    /* Note that the path itself isn't checked for validity here. If an invalid library path is
     * used, the Asset Browser can give a nice hint on what's wrong. */
    const bool is_valid = (custom_library->name[0] && custom_library->dirpath[0]);
    if (!is_valid) {
      continue;
    }

    AssetLibraryReference library_reference;
    library_reference.type = library_type;
    library_reference.custom_library_index = i;

    const int enum_value = ED_asset_library_reference_to_enum_value(&library_reference);
    /* Use library path as description, it's a nice hint for users. */
    EnumPropertyItem tmp = {enum_value,
                            custom_library->name,
                            ICON_NONE,
                            custom_library->name,
                            custom_library->dirpath};
    RNA_enum_item_add(items, totitem, &tmp);
  }
}

const EnumPropertyItem *ED_asset_library_reference_to_rna_enum_itemf(const bool include_generated)
{
  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  if (include_generated) {
    /* Add predefined libraries that are generated and not simple directories that can be written
     * to. */
    BLI_assert(rna_enum_asset_library_type_items[0].value == ASSET_LIBRARY_ALL);
    RNA_enum_item_add(&item, &totitem, &rna_enum_asset_library_type_items[0]);
    RNA_enum_item_add_separator(&item, &totitem);

    BLI_assert(rna_enum_asset_library_type_items[1].value == ASSET_LIBRARY_LOCAL);
    RNA_enum_item_add(&item, &totitem, &rna_enum_asset_library_type_items[1]);
    BLI_assert(rna_enum_asset_library_type_items[2].value == ASSET_LIBRARY_ESSENTIALS);
    RNA_enum_item_add(&item, &totitem, &rna_enum_asset_library_type_items[2]);
  }

  bke::BlenderProject *project = CTX_wm_project();
  if (project && !BLI_listbase_is_empty(&project->asset_library_definitions())) {
    RNA_enum_item_add_separator(&item, &totitem);

    add_custom_asset_library_enum_items(
        project->asset_library_definitions(), ASSET_LIBRARY_CUSTOM_FROM_PROJECT, &item, &totitem);
  }

  if (!BLI_listbase_is_empty(&U.asset_libraries)) {
    RNA_enum_item_add_separator(&item, &totitem);

    add_custom_asset_library_enum_items(
        U.asset_libraries, ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES, &item, &totitem);
  }

  RNA_enum_item_end(&item, &totitem);
  return item;
}
