/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include <memory>

#include "BLI_string_ref.hh"

namespace blender::bke {

class ProjectSettings {
  std::string project_root_path_;

 public:
  inline static const StringRefNull SETTINGS_DIRNAME = ".blender_project";

  /**
   * Initializes a blender project by creating a .blender_project directory at the given \a
   * project_root_path.
   * \return True if the settings directory was created, or already existed. False on failure.
   */
  static auto create_settings_directory(StringRef project_root_path) -> bool;

  /**
   * Read project settings from the given \a project_path, which may be either a project root
   * directory or the .blender_project directory.
   * \return The read project settings or null in case of failure.
   */
  static auto load_from_disk [[nodiscard]] (StringRef project_path)
  -> std::unique_ptr<ProjectSettings>;

  explicit ProjectSettings(StringRef project_root_path);

  auto project_root_path [[nodiscard]] () const -> StringRefNull;
};

}  // namespace blender::bke
