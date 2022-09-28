/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "BKE_appdir.h"
#include "BKE_project_settings.hh"

#include "BLI_fileops.h"
#include "BLI_function_ref.hh"

#include "testing/testing.h"

namespace blender::bke::tests {

class ProjectSettingsTest : public testing::Test {
  struct ProjectDirectoryRAIIWrapper {
    std::string project_path_;

    ProjectDirectoryRAIIWrapper(StringRefNull project_path)
    {
      /** Assert would be preferable but that would only run in debug builds, and #ASSERT_TRUE()
       * doesn't support printing a message. */
      if (BLI_exists(project_path.c_str())) {
        throw std::runtime_error("Can't execute test, temporary path '" + project_path +
                                 "' already exists");
      }

      BLI_dir_create_recursive(project_path.c_str());
      if (!BLI_exists(project_path.c_str())) {
        throw std::runtime_error("Can't execute test, failed to create path '" + project_path +
                                 "'");
      }
      project_path_ = project_path;
    }

    ~ProjectDirectoryRAIIWrapper()
    {
      if (!project_path_.empty()) {
        BLI_delete(project_path_.c_str(), true, true);
      }
    }
  };

 public:
  /* Run the test on multiple paths or variations of the same path. Useful to test things like
   * unicode paths, with or without trailing slash, etc. */
  void test_foreach_project_path(FunctionRef<void(StringRefNull)> fn)
  {
    std::vector<StringRefNull> subpaths = {
        "temporary-project-root",
        "test-temporary-unicode-dir-Ružena/temporary-project-root",
        /* Same but with trailing slash. */
        "test-temporary-unicode-dir-Ružena/temporary-project-root/",
    };

    BKE_tempdir_init("");

    const std::string tempdir = BKE_tempdir_session();
    for (StringRefNull subpath : subpaths) {
      ProjectDirectoryRAIIWrapper temp_project_path(tempdir + subpath);
      fn(temp_project_path.project_path_);
    }
  }
};

TEST_F(ProjectSettingsTest, create)
{
  test_foreach_project_path([](StringRefNull project_path) {
    if (!ProjectSettings::create_settings_directory(project_path)) {
      /* Not a regular test failure, this may fail if there is a permission issue for example. */
      FAIL() << "Failed to create project directory in '" << project_path
             << "', check permissions";
    }
    std::string project_settings_dir = project_path + "/" + ProjectSettings::SETTINGS_DIRNAME;
    EXPECT_TRUE(BLI_exists(project_settings_dir.c_str()))
        << project_settings_dir + " was not created";
  });
}

/* Load the project by pointing to the project root directory (as opposed to the .blender_project
 * directory). */
TEST_F(ProjectSettingsTest, load_from_project_root_path)
{
  test_foreach_project_path([](StringRefNull project_path) {
    ProjectSettings::create_settings_directory(project_path);

    std::unique_ptr project_settings = ProjectSettings::load_from_disk(project_path);
    EXPECT_NE(project_settings, nullptr);
    EXPECT_EQ(project_settings->project_root_path(), project_path);
  });
}

/* Load the project by pointing to the .blender_project directory (as opposed to the project root
 * directory). */
TEST_F(ProjectSettingsTest, load_from_project_settings_path)
{
  test_foreach_project_path([](StringRefNull project_path) {
    ProjectSettings::create_settings_directory(project_path);

    std::unique_ptr project_settings = ProjectSettings::load_from_disk(
        project_path + "/" + ProjectSettings::SETTINGS_DIRNAME);
    EXPECT_NE(project_settings, nullptr);
    EXPECT_EQ(project_settings->project_root_path(), project_path);
  });
}

}  // namespace blender::bke::tests
