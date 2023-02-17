/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "BKE_appdir.h"

#include "BLT_translation.h"

#include "DNA_asset_types.h"
#include "DNA_userdef_types.h"

#include "BKE_asset_library_custom.h"

using namespace blender;

/* -------------------------------------------------------------------- */
/** \name Asset Libraries
 * \{ */

CustomAssetLibraryDefinition *BKE_asset_library_custom_add(ListBase *custom_libraries,
                                                           const char *name,
                                                           const char *path)
{
  CustomAssetLibraryDefinition *library = MEM_cnew<CustomAssetLibraryDefinition>(
      "CustomAssetLibraryDefinition");

  BLI_addtail(custom_libraries, library);

  if (name) {
    BKE_asset_library_custom_name_set(custom_libraries, library, name);
  }
  if (path) {
    BLI_strncpy(library->path, path, sizeof(library->path));
  }
  library->import_method = ASSET_IMPORT_APPEND_REUSE;

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
  BLI_strncpy_utf8(library->name, name, sizeof(library->name));
  BLI_uniquename(custom_libraries,
                 library,
                 name,
                 '.',
                 offsetof(CustomAssetLibraryDefinition, name),
                 sizeof(library->name));
}

void BKE_asset_library_custom_path_set(CustomAssetLibraryDefinition *library, const char *path)
{
  BLI_strncpy(library->path, path, sizeof(library->path));
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
    const ListBase *custom_libraries, const char *path)
{
  LISTBASE_FOREACH (CustomAssetLibraryDefinition *, asset_lib_pref, custom_libraries) {
    if (BLI_path_contains(asset_lib_pref->path, path)) {
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
