/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_asset_library_custom.h"
#include "BKE_blender_project.h"
#include "BKE_blender_project.hh"

#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_space_types.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

namespace blender::bke {

BlenderProject::BlenderProject(std::unique_ptr<ProjectSettings> settings)
    : settings_(std::move(settings))
{
}

/* Construct on First Use idiom. */
std::unique_ptr<BlenderProject> &BlenderProject::active_project_ptr()
{
  static std::unique_ptr<BlenderProject> active_;
  return active_;
}

BlenderProject *BlenderProject::set_active_from_settings(std::unique_ptr<ProjectSettings> settings)
{
  std::unique_ptr<BlenderProject> &active = active_project_ptr();
  if (settings) {
    active = std::make_unique<BlenderProject>(BlenderProject(std::move(settings)));
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

StringRef BlenderProject::project_root_path_find_from_path(StringRef path)
{
  std::string path_native = path;
  BLI_path_slash_native(path_native.data());

  StringRef cur_path = path;

  while (cur_path.size()) {
    std::string pa = StringRef(path_native.c_str(), cur_path.size());
    if (path_is_project_root(pa)) {
      return path.substr(0, cur_path.size());
    }

    /* Walk "up the path" (check the parent next). */
    const int64_t pos_last_slash = cur_path.find_last_of(SEP);
    if (pos_last_slash == StringRef::not_found) {
      break;
    }
    cur_path = cur_path.substr(0, pos_last_slash);
  }

  return "";
}

ProjectSettings &BlenderProject::get_settings() const
{
  BLI_assert(settings_ != nullptr);
  return *settings_;
}

/* ---------------------------------------------------------------------- */

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
  return BLI_exists(std::string(path + SEP_STR + ProjectSettings::SETTINGS_DIRNAME).c_str());
}

}  // namespace blender::bke

/* ---------------------------------------------------------------------- */

using namespace blender;

bool BKE_project_create_settings_directory(const char *project_root_path)
{
  return bke::ProjectSettings::create_settings_directory(project_root_path);
}

bool BKE_project_delete_settings_directory(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  bke::ProjectSettings &settings = project->get_settings();
  return settings.delete_settings_directory();
}

BlenderProject *BKE_project_active_get(void)
{
  return reinterpret_cast<BlenderProject *>(bke::BlenderProject::get_active());
}

void BKE_project_active_unset(void)
{
  bke::BlenderProject::set_active_from_settings(nullptr);
}

bool BKE_project_is_path_project_root(const char *path)
{
  return bke::BlenderProject::path_is_project_root(path);
}

bool BKE_project_contains_path(const char *path)
{
  const StringRef found_root_path = bke::BlenderProject::project_root_path_find_from_path(path);
  return !found_root_path.is_empty();
}

BlenderProject *BKE_project_load_from_path(const char *path)
{
  std::unique_ptr<bke::ProjectSettings> project_settings = bke::ProjectSettings::load_from_path(
      path);
  if (!project_settings) {
    return nullptr;
  }

  return reinterpret_cast<BlenderProject *>(
      MEM_new<bke::BlenderProject>(__func__, std::move(project_settings)));
}

void BKE_project_free(BlenderProject **project_handle)
{
  bke::BlenderProject *project = reinterpret_cast<bke::BlenderProject *>(*project_handle);
  BLI_assert_msg(project != bke::BlenderProject::get_active(),
                 "Projects loaded with #BKE_project_load_from_path() must never be set active.");

  MEM_delete(project);
  *project_handle = nullptr;
}

BlenderProject *BKE_project_active_load_from_path(const char *path)
{
  /* Project should be unset if the path doesn't contain a project root. Unset in the beginning so
   * early exiting behaves correctly. */
  BKE_project_active_unset();

  std::unique_ptr<bke::ProjectSettings> project_settings = bke::ProjectSettings::load_from_path(
      path);
  bke::BlenderProject::set_active_from_settings(std::move(project_settings));

  return BKE_project_active_get();
}

bool BKE_project_settings_save(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  bke::ProjectSettings &settings = project->get_settings();
  return settings.save_to_disk(settings.project_root_path());
}

const char *BKE_project_root_path_get(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  return project->get_settings().project_root_path().c_str();
}

void BKE_project_name_set(const BlenderProject *project_handle, const char *name)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  project->get_settings().project_name(name);
}

const char *BKE_project_name_get(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  return project->get_settings().project_name().c_str();
}

ListBase *BKE_project_custom_asset_libraries_get(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  bke::ProjectSettings &settings = project->get_settings();
  return &settings.asset_library_definitions();
}

void BKE_project_tag_has_unsaved_changes(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  bke::ProjectSettings &settings = project->get_settings();
  settings.tag_has_unsaved_changes();
}

bool BKE_project_has_unsaved_changes(const BlenderProject *project_handle)
{
  const bke::BlenderProject *project = reinterpret_cast<const bke::BlenderProject *>(
      project_handle);
  const bke::ProjectSettings &settings = project->get_settings();
  return settings.has_unsaved_changes();
}
