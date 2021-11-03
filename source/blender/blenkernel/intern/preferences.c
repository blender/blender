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
 * \ingroup bke
 *
 * User defined asset library API.
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"

#include "BKE_appdir.h"
#include "BKE_preferences.h"

#include "BLT_translation.h"

#include "DNA_userdef_types.h"

#define U BLI_STATIC_ASSERT(false, "Global 'U' not allowed, only use arguments passed in!")

/* -------------------------------------------------------------------- */
/** \name Asset Libraries
 * \{ */

bUserAssetLibrary *BKE_preferences_asset_library_add(UserDef *userdef,
                                                     const char *name,
                                                     const char *path)
{
  bUserAssetLibrary *library = MEM_callocN(sizeof(*library), "bUserAssetLibrary");

  BLI_addtail(&userdef->asset_libraries, library);

  if (name) {
    BKE_preferences_asset_library_name_set(userdef, library, name);
  }
  if (path) {
    BLI_strncpy(library->path, path, sizeof(library->path));
  }

  return library;
}

/**
 * Unlink and free a library preference member.
 * \note Free's \a library itself.
 */
void BKE_preferences_asset_library_remove(UserDef *userdef, bUserAssetLibrary *library)
{
  BLI_freelinkN(&userdef->asset_libraries, library);
}

void BKE_preferences_asset_library_name_set(UserDef *userdef,
                                            bUserAssetLibrary *library,
                                            const char *name)
{
  BLI_strncpy_utf8(library->name, name, sizeof(library->name));
  BLI_uniquename(&userdef->asset_libraries,
                 library,
                 name,
                 '.',
                 offsetof(bUserAssetLibrary, name),
                 sizeof(library->name));
}

bUserAssetLibrary *BKE_preferences_asset_library_find_from_index(const UserDef *userdef, int index)
{
  return BLI_findlink(&userdef->asset_libraries, index);
}

bUserAssetLibrary *BKE_preferences_asset_library_find_from_name(const UserDef *userdef,
                                                                const char *name)
{
  return BLI_findstring(&userdef->asset_libraries, name, offsetof(bUserAssetLibrary, name));
}

bUserAssetLibrary *BKE_preferences_asset_library_containing_path(const UserDef *userdef,
                                                                 const char *path)
{
  LISTBASE_FOREACH (bUserAssetLibrary *, asset_lib_pref, &userdef->asset_libraries) {
    if (BLI_path_contains(asset_lib_pref->path, path)) {
      return asset_lib_pref;
    }
  }
  return NULL;
}

int BKE_preferences_asset_library_get_index(const UserDef *userdef,
                                            const bUserAssetLibrary *library)
{
  return BLI_findindex(&userdef->asset_libraries, library);
}

void BKE_preferences_asset_library_default_add(UserDef *userdef)
{
  char documents_path[FILE_MAXDIR];

  /* No home or documents path found, not much we can do. */
  if (!BKE_appdir_folder_documents(documents_path) || !documents_path[0]) {
    return;
  }

  bUserAssetLibrary *library = BKE_preferences_asset_library_add(
      userdef, DATA_(BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME), NULL);

  /* Add new "Default" library under '[doc_path]/Blender/Assets'. */
  BLI_path_join(
      library->path, sizeof(library->path), documents_path, N_("Blender"), N_("Assets"), NULL);
}

/** \} */
