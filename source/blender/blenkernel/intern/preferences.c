/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_path_util.h"

#include "BKE_appdir.h"
#include "BKE_asset_library_custom.h"
#include "BKE_preferences.h"

#include "BLT_translation.h"

#include "DNA_asset_types.h"
#include "DNA_defaults.h"
#include "DNA_userdef_types.h"

#define U BLI_STATIC_ASSERT(false, "Global 'U' not allowed, only use arguments passed in!")

/* -------------------------------------------------------------------- */
/** \name Asset Libraries
 * \{ */

void BKE_preferences_custom_asset_library_default_add(UserDef *userdef)
{
  char documents_path[FILE_MAXDIR];

  /* No home or documents path found, not much we can do. */
  if (!BKE_appdir_folder_documents(documents_path) || !documents_path[0]) {
    return;
  }

  CustomAssetLibraryDefinition *library = BKE_asset_library_custom_add(
      &userdef->asset_libraries, DATA_(BKE_PREFS_ASSET_LIBRARY_DEFAULT_NAME), NULL);

  /* Add new "Default" library under '[doc_path]/Blender/Assets'. */
  BLI_path_join(
      library->dirpath, sizeof(library->dirpath), documents_path, N_("Blender"), N_("Assets"));
}

/** \} */
