/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"

namespace blender::io::serialize {
class DictionaryValue;
}

namespace blender::bke {

class ProjectSettings;
struct CustomAssetLibraries;

class BlenderProject {
  inline static std::unique_ptr<BlenderProject> active_;

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

  auto get_settings [[nodiscard]] () const -> ProjectSettings &;

 private:
  explicit BlenderProject(std::unique_ptr<ProjectSettings> settings);
};

class ProjectSettings {
  /* Path to the project root using slashes in the OS native format. */
  std::string project_root_path_;
  std::string project_name_;
  std::unique_ptr<CustomAssetLibraries> asset_libraries_;
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
  auto has_unsaved_changes [[nodiscard]] () const -> bool;

 private:
  auto to_dictionary() const -> std::unique_ptr<io::serialize::DictionaryValue>;
};

struct CustomAssetLibraries {
  ListBase asset_libraries = {nullptr, nullptr}; /* CustomAssetLibraryDefinition */

  CustomAssetLibraries(ListBase asset_libraries);
  ~CustomAssetLibraries();
};

}  // namespace blender::bke
