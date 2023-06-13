/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "DNA_asset_types.h"
#include "DNA_defaults.h"

#include "BKE_asset_library_custom.h"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Asset Libraries
 * \{ */

CustomAssetLibraryDefinition *BKE_asset_library_custom_add(ListBase *custom_libraries,
                                                           const char *name,
                                                           const char *dirpath)
{
  CustomAssetLibraryDefinition *library = MEM_cnew<CustomAssetLibraryDefinition>(
      "CustomAssetLibraryDefinition");
  memcpy(library, DNA_struct_default_get(CustomAssetLibraryDefinition), sizeof(*library));

  BLI_addtail(custom_libraries, library);

  if (name) {
    BKE_asset_library_custom_name_set(custom_libraries, library, name);
  }
  if (dirpath) {
    STRNCPY(library->dirpath, dirpath);
  }

  return library;
}

void BKE_asset_library_custom_remove(ListBase *custom_libraries,
                                     CustomAssetLibraryDefinition *library)
{
  BLI_freelinkN(custom_libraries, library);
}

void BKE_asset_library_custom_name_set(ListBase *custom_libraries,
                                       CustomAssetLibraryDefinition *library,
                                       const char *name)
{
  STRNCPY_UTF8(library->name, name);
  BLI_uniquename(custom_libraries,
                 library,
                 name,
                 '.',
                 offsetof(CustomAssetLibraryDefinition, name),
                 sizeof(library->name));
}

void BKE_asset_library_custom_path_set(CustomAssetLibraryDefinition *library, const char *dirpath)
{
  STRNCPY(library->dirpath, dirpath);
}

CustomAssetLibraryDefinition *BKE_asset_library_custom_find_from_index(
    const ListBase *custom_libraries, int index)
{
  return static_cast<CustomAssetLibraryDefinition *>(BLI_findlink(custom_libraries, index));
}

CustomAssetLibraryDefinition *BKE_asset_library_custom_find_from_name(
    const ListBase *custom_libraries, const char *name)
{
  return static_cast<CustomAssetLibraryDefinition *>(
      BLI_findstring(custom_libraries, name, offsetof(CustomAssetLibraryDefinition, name)));
}

CustomAssetLibraryDefinition *BKE_asset_library_custom_containing_path(
    const ListBase *custom_libraries, const char *dirpath)
{
  LISTBASE_FOREACH (CustomAssetLibraryDefinition *, asset_lib_pref, custom_libraries) {
    if (BLI_path_contains(asset_lib_pref->dirpath, dirpath)) {
      return asset_lib_pref;
    }
  }
  return NULL;
}

int BKE_asset_library_custom_get_index(const ListBase *custom_libraries,
                                       const CustomAssetLibraryDefinition *library)
{
  return BLI_findindex(custom_libraries, library);
}

/** \} */
