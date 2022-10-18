/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

struct BlenderProject;

namespace blender::io::serialize {
class DictionaryValue;
}

namespace blender::bke {

class ProjectSettings;
struct CustomAssetLibraries;

/**
 * Entry point / API for core Blender project management.
 *
 * Responsibilities:
 * - Own and give access to the active project.
 * - Manage the .blender_project/ directory.
 * - Store and manage (including reading & writing) of the .blender_project/settings.json file. The
 *   implementation of this can be found in the internal #ProjectSettings class.
 * - Tag for unsaved changes as needed.
 */
class BlenderProject {
  friend class ProjectSettings;

  /* Path to the project root using native slashes plus a trailing slash. */
  std::string root_path_;
  std::unique_ptr<ProjectSettings> settings_;

 public:
  inline static const StringRefNull SETTINGS_DIRNAME = ".blender_project";
  inline static const StringRefNull SETTINGS_FILENAME = "settings.json";

 public:
  static auto get_active [[nodiscard]] () -> BlenderProject *;
  static auto set_active(std::unique_ptr<BlenderProject> settings) -> BlenderProject *;

  /**
   * Read project settings from the given \a path, which may point to some directory or file inside
   * of the project directory. Both Unix and Windows style slashes are allowed. Path is expected to
   * be normalized.
   *
   * Attempt to read project data from the given \a project_path, which may be either a project
   * root directory or the .blender_project directory, and load it into runtime data. Letting the
   * returned #unique_pointer run out of scope cleanly destructs the runtime project data.
   *
   * \note Does NOT set the loaded project active.
   *
   * \return The loaded project or null on failure.
   */
  static auto load_from_path(StringRef project_path) -> std::unique_ptr<BlenderProject>;

  /**
   * Initializes a blender project by creating a .blender_project directory at the given \a
   * project_root_path.
   * Both Unix and Windows style slashes are allowed.
   *
   * \return True if the settings directory was created, or already existed. False on failure.
   */
  static auto create_settings_directory(StringRef project_root_path) -> bool;
  /**
   * Remove the .blender_project directory with all of its contents at the given \a
   * project_root_path. If this is the path of the active project, it is marked as having changed
   * but it is not unloaded. Runtime project data is still valid at this point.
   *
   * \return True on success.
   */
  static auto delete_settings_directory(StringRef project_root_path) -> bool;

  /**
   * Check if the directory given by \a path contains a .blender_project directory and should thus
   * be considered a project root directory.
   */
  static auto path_is_project_root(StringRef path) -> bool;

  /**
   * Check if \a path points into a project and return the root directory path of that project (the
   * one containing the .blender_project directory). Walks "upwards" through the path and returns
   * the first project found, so if a project is nested inside another one, the nested project is
   * used.
   * Both Unix and Windows style slashes are allowed.
   *
   * \return The project root path or an empty path if not found. The referenced string points into
   *         the input \a path, so slashes are not converted in the returned value.
   */
  static auto project_root_path_find_from_path [[nodiscard]] (StringRef path) -> StringRef;

  /* --- Non-static member functions. --- */

  BlenderProject(StringRef project_root_path, std::unique_ptr<ProjectSettings> settings);

  /**
   * Version of the static #delete_settings_directory() that deletes the settings directory of this
   * project. Always tags as having unsaved changes after successful deletion.
   */
  auto delete_settings_directory() -> bool;

  auto root_path [[nodiscard]] () const -> StringRefNull;
  auto get_settings [[nodiscard]] () const -> ProjectSettings &;

 private:
  static auto active_project_ptr() -> std::unique_ptr<BlenderProject> &;
  /**
   * Get the project root path from a path that is either already the project root, or the
   * .blender_project directory. Returns the path with native slashes plus a trailing slash.
   */
  static auto project_path_to_native_project_root_path(StringRef project_path) -> std::string;
  /**
   * Get the .blender_project directory path from a project root path. Returns the path with native
   * slashes plus a trailing slash. Assumes the path already ends with a native trailing slash.
   */
  static auto project_root_path_to_settings_path(StringRef project_root_path) -> std::string;
  /**
   * Returns the path with native slashes.
   * Assumes the path already ends with a native trailing slash.
   */
  static auto project_root_path_to_settings_filepath(StringRef project_root_path) -> std::string;
};

/**
 * Runtime representation of the project settings (`.blender_project/settings.json`) with IO
 * functionality.
 */
class ProjectSettings {
  std::string project_name_;
  std::unique_ptr<CustomAssetLibraries> asset_libraries_;

  bool has_unsaved_changes_ = false;

 public:
  /**
   * Read project settings from the given \a project_path, which may be either a project root
   * directory or the .blender_project directory.
   * Both Unix and Windows style slashes are allowed. Path is expected to be normalized.
   *
   * \return The read project settings or null in case of failure.
   */
  static auto load_from_disk [[nodiscard]] (StringRef project_path)
  -> std::unique_ptr<ProjectSettings>;
  /**
   * Read project settings from the given \a path, which may point to some directory or file inside
   * of the project directory. Both Unix and Windows style slashes are allowed. Path is expected to
   * be normalized.
   *
   * \return The read project settings or null in case of failure.
   */
  static auto load_from_path [[nodiscard]] (StringRef path) -> std::unique_ptr<ProjectSettings>;

  /** Explicit constructor and destructor needed to manage the CustomAssetLibraries unique_ptr. */
  ProjectSettings();
  /* Implementation defaulted. */
  ~ProjectSettings();

  /**
   * Write project settings to the given \a project_path, which may be either a project root
   * directory or the .blender_project directory. The .blender_project directory must exist.
   * Both Unix and Windows style slashes are allowed. Path is expected to be normalized.
   *
   * \return True on success. If the .blender_project directory doesn't exist, that's treated
   *         as failure.
   */
  auto save_to_disk(StringRef project_path) -> bool;

  void project_name(StringRef new_name);
  auto project_name [[nodiscard]] () const -> StringRefNull;
  auto asset_library_definitions() const -> const ListBase &;
  auto asset_library_definitions() -> ListBase &;
  /**
   * Forcefully tag the project settings for having unsaved changes. This needs to be done if
   * project settings data is modified directly by external code, not via a project settings API
   * function. The API functions set the tag for all changes they manage.
   */
  void tag_has_unsaved_changes();
  /**
   * Returns true if there were any changes done to the settings that have not been written to
   * disk yet. Project settings API functions that change data set this, however when external
   * code modifies project settings data it may have to manually set the tag, see
   * #tag_has_unsaved_changes().
   */
  auto has_unsaved_changes [[nodiscard]] () const -> bool;

 private:
  auto to_dictionary() const -> std::unique_ptr<io::serialize::DictionaryValue>;
};

}  // namespace blender::bke

inline ::BlenderProject *BKE_project_c_handle(blender::bke::BlenderProject *project)
{
  return reinterpret_cast<::BlenderProject *>(project);
}
