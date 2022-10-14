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
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

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

bool ED_project_new(const Main *bmain, const char *project_root_dir, ReportList *reports)
{
  if (!BKE_project_create_settings_directory(project_root_dir)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Failed to create project with unknown error. Is the directory read-only?");
    return false;
  }

  BKE_reportf(reports, RPT_INFO, "Project created and loaded successfully");

  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  bool activated_new_project = false;
  if (blendfile_path[0] && BLI_path_contains(project_root_dir, blendfile_path)) {
    if (BKE_project_active_load_from_path(blendfile_path)) {
      activated_new_project = true;
    }
  }
  else {
    BKE_reportf(reports,
                RPT_WARNING,
                "The current file is not located inside of the new project. This means the new "
                "project is not active");
  }

  /* Some default settings for the project. It has to be loaded for that. */
  BlenderProject *new_project = activated_new_project ?
                                    CTX_wm_project() :
                                    BKE_project_load_from_path(project_root_dir);
  if (new_project) {
    ED_project_set_defaults(new_project);

    /* Write defaults to the hard drive. */
    BKE_project_settings_save(new_project);

    /* We just temporary loaded the project, free it again. */
    if (!activated_new_project) {
      BKE_project_free(&new_project);
    }
  }

  return true;
}
