/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_path_util.h"

#include "BLI_fileops.h"

#include "BKE_blender_project.hh"

namespace blender::bke {

BlenderProject::BlenderProject(const StringRef project_root_path,
                               std::unique_ptr<ProjectSettings> settings)
    : settings_(std::move(settings))
{
  root_path_ = BlenderProject::project_path_to_native_project_root_path(project_root_path);
  BLI_assert(root_path_.back() == SEP);
}

/* ---------------------------------------------------------------------- */
/** \name Active project management (static storage)
 * \{ */

/* Construct on First Use idiom. */
std::unique_ptr<BlenderProject> &BlenderProject::active_project_ptr()
{
  static std::unique_ptr<BlenderProject> active_;
  return active_;
}

BlenderProject *BlenderProject::set_active(std::unique_ptr<BlenderProject> project)
{
  std::unique_ptr<BlenderProject> &active = active_project_ptr();
  if (project) {
    active = std::move(project);
  }
  else {
    active = nullptr;
  }

  return active.get();
}

BlenderProject *BlenderProject::get_active()
{
  std::unique_ptr<BlenderProject> &active = active_project_ptr();
  return active.get();
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Project and project settings management.
 * \{ */

std::unique_ptr<BlenderProject> BlenderProject::load_from_path(StringRef project_path)
{
  const StringRef project_root_path = project_root_path_find_from_path(project_path);

  std::unique_ptr<bke::ProjectSettings> project_settings = bke::ProjectSettings::load_from_path(
      project_root_path);
  if (!project_settings) {
    return nullptr;
  }

  return std::make_unique<BlenderProject>(project_root_path, std::move(project_settings));
}

BlenderProject *BlenderProject::load_active_from_path(StringRef path)
{
  /* Project should be unset if the path doesn't contain a project root. Unset in the beginning so
   * early exiting behaves correctly. */
  bke::BlenderProject::set_active(nullptr);

  std::unique_ptr<bke::BlenderProject> project = bke::BlenderProject::load_from_path(path);

  return bke::BlenderProject::set_active(std::move(project));
}

bool BlenderProject::create_settings_directory(StringRef project_path)
{
  std::string project_root_path = project_path_to_native_project_root_path(project_path);
  std::string settings_path = project_root_path_to_settings_path(project_root_path);

  return BLI_dir_create_recursive(settings_path.c_str());
}

bool BlenderProject::save_settings()
{
  return settings_->save_to_disk(root_path_);
}

bool BlenderProject::delete_settings_directory(StringRef project_path)
{
  std::string project_root_path = project_path_to_native_project_root_path(project_path);
  std::string settings_path = project_root_path_to_settings_path(project_root_path);

  /* Returns 0 on success. */
  if (BLI_delete(settings_path.c_str(), true, true)) {
    return false;
  }

  BlenderProject *active_project = get_active();
  if (active_project &&
      BLI_path_cmp_normalized(project_root_path.c_str(), active_project->root_path().c_str()))
  {
    active_project->settings_->tag_has_unsaved_changes();
  }
  return true;
}

bool BlenderProject::has_unsaved_changes(const BlenderProject *project)
{
  if (!project) {
    return false;
  }
  return project->has_unsaved_changes();
}

bool BlenderProject::delete_settings_directory()
{
  if (!delete_settings_directory(root_path_)) {
    return false;
  }

  settings_->tag_has_unsaved_changes();
  return true;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Simple getters & setters
 * \{ */

StringRefNull BlenderProject::root_path() const
{
  return root_path_;
}

ProjectSettings &BlenderProject::get_settings() const
{
  BLI_assert(settings_ != nullptr);
  return *settings_;
}

void BlenderProject::set_project_name(StringRef new_name)
{
  settings_->project_name(new_name);
}

StringRefNull BlenderProject::project_name() const
{
  return settings_->project_name();
}

const ListBase &BlenderProject::asset_library_definitions() const
{
  return settings_->asset_library_definitions();
}
ListBase &BlenderProject::asset_library_definitions()
{
  return settings_->asset_library_definitions();
}

void BlenderProject::tag_has_unsaved_changes()
{
  settings_->tag_has_unsaved_changes();
}

bool BlenderProject::has_unsaved_changes() const
{
  return settings_->has_unsaved_changes();
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Path stuff
 * \{ */

StringRef BlenderProject::project_root_path_find_from_path(StringRef path)
{
  /* There are two versions of the path used here: One copy that is converted to native slashes,
   * and the unmodified original path from the input. */

  std::string path_native = path;
  BLI_path_slash_native(path_native.data());

  StringRef cur_path = path;

  while (cur_path.size()) {
    std::string cur_path_native = StringRef(path_native.c_str(), cur_path.size());
    if (path_is_project_root(cur_path_native)) {
      return path.substr(0, cur_path.size());
    }

    /* Walk "up the path" (check the parent next). */
    const int64_t pos_last_slash = cur_path_native.find_last_of(SEP);
    if (pos_last_slash == StringRef::not_found) {
      break;
    }
    cur_path = cur_path.substr(0, pos_last_slash);
  }

  return "";
}

static StringRef path_strip_trailing_native_slash(StringRef path)
{
  const int64_t pos_before_trailing_slash = path.find_last_not_of(SEP);
  return (pos_before_trailing_slash == StringRef::not_found) ?
             path :
             path.substr(0, pos_before_trailing_slash + 1);
}

bool BlenderProject::path_is_project_root(StringRef path)
{
  path = path_strip_trailing_native_slash(path);
  return BLI_exists(std::string(path + SEP_STR + SETTINGS_DIRNAME).c_str());
}

bool BlenderProject::path_is_within_project(StringRef path)
{
  const StringRef found_root_path = project_root_path_find_from_path(path);
  return !found_root_path.is_empty();
}

std::string BlenderProject::project_path_to_native_project_root_path(StringRef project_path)
{
  std::string project_path_native = project_path;
  BLI_path_slash_native(project_path_native.data());

  const StringRef path_no_trailing_slashes = path_strip_trailing_native_slash(project_path_native);
  if (path_no_trailing_slashes.endswith(SETTINGS_DIRNAME)) {
    return StringRef(path_no_trailing_slashes).drop_suffix(SETTINGS_DIRNAME.size());
  }

  return std::string(path_no_trailing_slashes) + SEP;
}

std::string BlenderProject::project_root_path_to_settings_path(StringRef project_root_path)
{
  BLI_assert(project_root_path.back() == SEP);
  return project_root_path + SETTINGS_DIRNAME + SEP;
}

std::string BlenderProject::project_root_path_to_settings_filepath(StringRef project_root_path)
{
  BLI_assert(project_root_path.back() == SEP);
  return project_root_path_to_settings_path(project_root_path) + SETTINGS_FILENAME;
}

/** \} */

}  // namespace blender::bke
