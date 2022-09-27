/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_project_settings.hh"

#include "BLI_fileops.h"

namespace blender::bke {

ProjectSettings::ProjectSettings(StringRef project_root_path)
    : project_root_path_(project_root_path)
{
}

bool ProjectSettings::create_settings_directory(StringRef project_root_path)
{
  return BLI_dir_create_recursive(std::string(project_root_path + "/" + SETTINGS_DIRNAME).c_str());
}

std::unique_ptr<ProjectSettings> ProjectSettings::load_from_disk(StringRef project_path)
{
  if (!BLI_exists(std::string(project_path).c_str())) {
    return nullptr;
  }

  StringRef project_root_path = project_path;

  const int64_t pos_before_trailing_slash = project_path.find_last_not_of("\\/");
  const StringRef path_no_trailing_slashes = (pos_before_trailing_slash == StringRef::not_found) ?
                                                 project_path :
                                                 project_path.substr(
                                                     0, pos_before_trailing_slash + 1);
  if (path_no_trailing_slashes.endswith(SETTINGS_DIRNAME)) {
    project_root_path = project_path.drop_suffix(SETTINGS_DIRNAME.size() + 1);
  }

  return std::make_unique<ProjectSettings>(project_root_path);
}

StringRefNull ProjectSettings::project_root_path() const
{
  return project_root_path_;
}

}  // namespace blender::bke
