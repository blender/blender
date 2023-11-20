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
  /**
   * \note: When changing the active project, the previously active one will be destroyed, so
   *        pointers may dangle.
   */
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
   * Attempt to load and activate a project based on the given path. If the path doesn't lead
   * to or into a project, the active project is unset. Note that the project will be unset on any
   * failure when loading the project.
   *
   * \note: When setting an active project, the previously active one will be destroyed, so
   * pointers may dangle.
   */
  static auto load_active_from_path(StringRef project_path) -> BlenderProject *;

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
   * Will return false for paths pointing into a project root directory not to a root directory
   * itself.
   */
  static auto path_is_project_root [[nodiscard]] (StringRef path) -> bool;

  /**
   * Check if \a path points to or into a project root path (i.e. if one of the ancestors of the
   * referenced file/directory is a project root directory).
   */
  static auto path_is_within_project [[nodiscard]] (StringRef path) -> bool;

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

  /**
   * Version of #has_unsaved_changes() that allows passing null as \a project for convenience. If
   * \a project is null, false will be returned.
   */
  static auto has_unsaved_changes(const BlenderProject *project) -> bool;

  /* --- Non-static member functions. --- */

  BlenderProject(StringRef project_root_path, std::unique_ptr<ProjectSettings> settings);

  auto save_settings() -> bool;
  /**
   * Version of the static #delete_settings_directory() that deletes the settings directory of this
   * project. Always tags as having unsaved changes after successful deletion.
   */
  auto delete_settings_directory() -> bool;

  auto root_path [[nodiscard]] () const -> StringRefNull;
  auto get_settings [[nodiscard]] () const -> ProjectSettings &;

  void set_project_name(StringRef new_name);
  auto project_name [[nodiscard]] () const -> StringRefNull;
  auto asset_library_definitions() const -> const ListBase &;
  auto asset_library_definitions() -> ListBase &;
  /**
   * Forcefully tag the project settings for having unsaved changes. This needs to be done if
   * project settings data is modified directly by external code, not via a project API function.
   * The API functions set the tag for all changes they manage.
   */
  void tag_has_unsaved_changes();
  /**
   * Returns true if there were any changes done to the settings that have not been written to
   * disk yet. Project API functions that change data set this, however when external code modifies
   * project settings data it may have to manually set the tag, see #tag_has_unsaved_changes().
   *
   * There's a static version of this that takes a project pointer that may be null, for
   * convenience (so the caller doesn't have to null-check).
   */
  auto has_unsaved_changes [[nodiscard]] () const -> bool;

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
  auto asset_library_definitions [[nodiscard]] () const -> const ListBase &;
  auto asset_library_definitions [[nodiscard]] () -> ListBase &;
  /** See #BlenderProject::tag_has_unsaved_changes(). */
  void tag_has_unsaved_changes();
  /** See #BlenderProject::has_unsaved_changes. */
  auto has_unsaved_changes [[nodiscard]] () const -> bool;

 private:
  auto to_dictionary() const -> std::unique_ptr<io::serialize::DictionaryValue>;
};

}  // namespace blender::bke
