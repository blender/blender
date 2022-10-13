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

class BlenderProject {
  std::unique_ptr<ProjectSettings> settings_;

 public:
  static auto get_active [[nodiscard]] () -> BlenderProject *;
  static auto set_active_from_settings(std::unique_ptr<ProjectSettings> settings)
      -> BlenderProject *;

  /**
   * Check if \a path points into a project and return the root directory path of that project (the
   * one containing the .blender_project directory). Walks "upwards" through the path and returns
   * the first project found, so if a project is nested inside another one, the nested project is
   * used.
   * Both Unix and Windows style slashes are allowed.
   * \return the project root path or an empty path if not found.
   */
  static auto project_root_path_find_from_path [[nodiscard]] (StringRef path) -> StringRef;

  explicit BlenderProject(std::unique_ptr<ProjectSettings> settings);

  auto get_settings [[nodiscard]] () const -> ProjectSettings &;

 private:
  static std::unique_ptr<BlenderProject> &active_project_ptr();
};

struct CustomAssetLibraries : NonCopyable {
  ListBase asset_libraries = {nullptr, nullptr}; /* CustomAssetLibraryDefinition */

  CustomAssetLibraries() = default;
  CustomAssetLibraries(ListBase asset_libraries);
  CustomAssetLibraries(CustomAssetLibraries &&);
  ~CustomAssetLibraries();
  auto operator=(CustomAssetLibraries &&) -> CustomAssetLibraries &;
};

class ProjectSettings {
  /* Path to the project root using slashes in the OS native format. */
  std::string project_root_path_;
  std::string project_name_;
  CustomAssetLibraries asset_libraries_ = {};
  bool has_unsaved_changes_ = false;

 public:
  inline static const StringRefNull SETTINGS_DIRNAME = ".blender_project";
  inline static const StringRefNull SETTINGS_FILENAME = "settings.json";

  /**
   * Initializes a blender project by creating a .blender_project directory at the given \a
   * project_root_path.
   * Both Unix and Windows style slashes are allowed.
   * \return True if the settings directory was created, or already existed. False on failure.
   */
  static auto create_settings_directory(StringRef project_root_path) -> bool;

  /**
   * Read project settings from the given \a project_path, which may be either a project root
   * directory or the .blender_project directory.
   * Both Unix and Windows style slashes are allowed. Path is expected to be normalized.
   * \return The read project settings or null in case of failure.
   */
  static auto load_from_disk [[nodiscard]] (StringRef project_path)
  -> std::unique_ptr<ProjectSettings>;
  /**
   * Read project settings from the given \a path, which may point to some directory or file inside
   * of the project directory. Both Unix and Windows style slashes are allowed. Path is expected to
   * be normalized.
   * \return The read project settings or null in case of failure.
   */
  static auto load_from_path [[nodiscard]] (StringRef path) -> std::unique_ptr<ProjectSettings>;
  /**
   * Write project settings to the given \a project_path, which may be either a project root
   * directory or the .blender_project directory. The .blender_project directory must exist.
   * Both Unix and Windows style slashes are allowed. Path is expected to be normalized.
   * \return True on success. If the .blender_project directory doesn't exist, that's treated as
   *         failure.
   */
  auto save_to_disk(StringRef project_path) -> bool;
  /**
   * Remove the .blender_project directory with all of its contents. Does not unload the active
   * project but marks it as having unsaved changes. Runtime project data is still valid.
   * \return True on success.
   */
  auto delete_settings_directory() -> bool;

  explicit ProjectSettings(StringRef project_root_path);

  auto project_root_path [[nodiscard]] () const -> StringRefNull;
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
