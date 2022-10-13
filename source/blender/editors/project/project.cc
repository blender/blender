/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edproject
 */

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BLT_translation.h"

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.h"

#include "ED_project.h"

using namespace blender;

/** Name of the asset library added by default. Needs translation with `DATA_()`. */
inline static const char *DEFAULT_ASSET_LIBRARY_NAME = N_("Project Library");
inline static const char *DEFAULT_ASSET_LIBRARY_PATH = "//assets/";

void ED_project_set_defaults(BlenderProject *project)
{
  char project_root_dir[FILE_MAXDIR];
  BLI_strncpy(project_root_dir, BKE_project_root_path_get(project), sizeof(project_root_dir));

  /* Set directory name as default project name. */
  char dirname[FILE_MAXFILE];
  BLI_path_slash_rstrip(project_root_dir);
  BLI_split_file_part(project_root_dir, dirname, sizeof(dirname));
  BKE_project_name_set(project, dirname);

  ListBase *libraries = BKE_project_custom_asset_libraries_get(project);
  BKE_asset_library_custom_add(
      libraries, DATA_(DEFAULT_ASSET_LIBRARY_NAME), DEFAULT_ASSET_LIBRARY_PATH);
}
