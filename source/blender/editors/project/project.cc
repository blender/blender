/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edproject
 */

#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BLT_translation.h"

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.hh"
#include "BKE_context.hh"
#include "BKE_main.h"
#include "BKE_report.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_project.hh"

using namespace blender;

/** Name of the asset library added by default. Needs translation with `DATA_()`. */
inline static const char *DEFAULT_ASSET_LIBRARY_NAME = N_("Project Library");
inline static const char *DEFAULT_ASSET_LIBRARY_PATH = "//assets/";

void ED_project_set_defaults(bke::BlenderProject &project)
{
  char project_root_dir[FILE_MAXDIR];
  BLI_strncpy(project_root_dir, project.root_path().c_str(), sizeof(project_root_dir));

  /* Set directory name as default project name. */
  char dirname[FILE_MAXFILE];
  BLI_path_slash_rstrip(project_root_dir);
  BLI_path_split_file_part(project_root_dir, dirname, sizeof(dirname));
  project.set_project_name(dirname);

  ListBase &libraries = project.asset_library_definitions();
  BKE_asset_library_custom_add(
      &libraries, DATA_(DEFAULT_ASSET_LIBRARY_NAME), DEFAULT_ASSET_LIBRARY_PATH);
}

bool ED_project_new(const Main *bmain, const char *project_root_dir, ReportList *reports)
{
  if (!bke::BlenderProject::create_settings_directory(project_root_dir)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Failed to create project with unknown error. Is the directory read-only?");
    return false;
  }

  std::unique_ptr<bke::BlenderProject> loaded_project = bke::BlenderProject::load_from_path(
      project_root_dir);

  /* Some default settings for the project. */
  if (loaded_project) {
    ED_project_set_defaults(*loaded_project);
    /* Write defaults to the hard drive. */
    loaded_project->save_settings();
  }

  BKE_reportf(reports, RPT_INFO, "Project created and loaded successfully");

  const char *blend_path = BKE_main_blendfile_path(bmain);
  const bool blend_is_in_project = blend_path[0] &&
                                   BLI_path_contains(project_root_dir, blend_path);
  if (blend_is_in_project) {
    bke::BlenderProject::set_active(std::move(loaded_project));
  }
  else {
    BKE_reportf(reports,
                RPT_WARNING,
                "The current file is not located inside of the new project. This means the new "
                "project is not active");
  }

  return true;
}
