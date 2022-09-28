/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_project_settings.hh"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

namespace blender::bke {

ProjectSettings::ProjectSettings(StringRef project_root_path)
    : project_root_path_(project_root_path)
{
}

bool ProjectSettings::create_settings_directory(StringRef project_root_path)
{
  std::string project_root_path_native = project_root_path;
  BLI_path_slash_native(project_root_path_native.data());

  return BLI_dir_create_recursive(
      std::string(project_root_path_native + SEP + SETTINGS_DIRNAME).c_str());
}

static StringRef path_strip_trailing_native_slash(StringRef path)
{
  const int64_t pos_before_trailing_slash = path.find_last_not_of(SEP);
  return (pos_before_trailing_slash == StringRef::not_found) ?
             path :
             path.substr(0, pos_before_trailing_slash + 1);
}

static bool path_contains_project_settings(StringRef path)
{
  return BLI_exists(std::string(path + SEP_STR + ProjectSettings::SETTINGS_DIRNAME).c_str());
}

std::unique_ptr<ProjectSettings> ProjectSettings::load_from_disk(StringRef project_path)
{
  std::string project_path_native = project_path;
  BLI_path_slash_native(project_path_native.data());

  if (!BLI_exists(project_path_native.c_str())) {
    return nullptr;
  }

  StringRef project_root_path = project_path_native;

  const StringRef path_no_trailing_slashes = path_strip_trailing_native_slash(project_path_native);
  if (path_no_trailing_slashes.endswith(SETTINGS_DIRNAME)) {
    project_root_path = StringRef(project_path_native).drop_suffix(SETTINGS_DIRNAME.size() + 1);
  }

  if (!path_contains_project_settings(project_root_path)) {
    return nullptr;
  }

  return std::make_unique<ProjectSettings>(project_root_path);
}

StringRefNull ProjectSettings::project_root_path() const
{
  return project_root_path_;
}

}  // namespace blender::bke
