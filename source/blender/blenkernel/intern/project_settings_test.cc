/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "BKE_appdir.h"
#include "BKE_project_settings.hh"

#include "BLI_fileops.h"

#include "testing/testing.h"

namespace blender::bke::tests {

class ProjectSettingsTest : public testing::Test {
 public:
  std::string temp_project_root_path_;

  void TearDown() override
  {
    if (!temp_project_root_path_.empty()) {
      BLI_delete(temp_project_root_path_.c_str(), true, true);
      temp_project_root_path_ = "";
    }
  }

  StringRefNull create_temp_project_path()
  {
    BKE_tempdir_init("");
    const std::string tempdir = BKE_tempdir_session();
    temp_project_root_path_ = tempdir + "test-temporary-project-root";

    /** Assert would be preferable but that would only run in debug builds, and #ASSERT_TRUE()
     * doesn't support printing a message. */
    if (BLI_exists(temp_project_root_path_.c_str())) {
      throw std::runtime_error("Can't execute test, temporary path '" + temp_project_root_path_ +
                               "' already exists");
    }

    BLI_dir_create_recursive(temp_project_root_path_.c_str());
    if (!BLI_exists(temp_project_root_path_.c_str())) {
      throw std::runtime_error("Can't execute test, failed to create path '" +
                               temp_project_root_path_ + "'");
    }
    return temp_project_root_path_;
  }
};

TEST_F(ProjectSettingsTest, create)
{
  StringRefNull project_path = create_temp_project_path();

  if (!ProjectSettings::create_settings_directory(project_path)) {
    /* Not a regular test failure, this may fail if there is a permission issue for example. */
    FAIL() << "Failed to create project directory in '" << project_path << "', check permissions";
  }
  std::string project_settings_dir = project_path + "/" + ProjectSettings::SETTINGS_DIRNAME;
  EXPECT_TRUE(BLI_exists(project_settings_dir.c_str()))
      << project_settings_dir + " was not created";
}

/* Load the project by pointing to the project root directory (as opposed to the .blender_project
 * directory). */
TEST_F(ProjectSettingsTest, load_project_root)
{
  StringRefNull project_path = create_temp_project_path();
  ProjectSettings::create_settings_directory(project_path);

  std::unique_ptr project_settings = ProjectSettings::load_from_disk(project_path);
  EXPECT_NE(project_settings, nullptr);
  EXPECT_EQ(project_settings->project_root_path(), project_path);
}

/* Load the project by pointing to the .blender_project directory (ass opposed to the project root
 * directory). */
TEST_F(ProjectSettingsTest, load_project_settings_dir)
{
  StringRefNull project_path = create_temp_project_path();
  ProjectSettings::create_settings_directory(project_path);

  std::unique_ptr project_settings = ProjectSettings::load_from_disk(
      project_path + "/" + ProjectSettings::SETTINGS_DIRNAME);
  EXPECT_NE(project_settings, nullptr);
  EXPECT_EQ(project_settings->project_root_path(), project_path);
}

}  // namespace blender::bke::tests
