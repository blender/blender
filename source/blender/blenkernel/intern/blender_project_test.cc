/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#include "BKE_appdir.h"
#include "BKE_blender_project.h"
#include "BKE_blender_project.hh"
#include "BKE_main.h"

#include "BLI_fileops.h"
#include "BLI_function_ref.hh"
#include "BLI_path_util.h"
#include "BLI_vector.hh"

#include "BLO_readfile.h"

#include "blendfile_loading_base_test.h"

#include "testing/testing.h"

namespace blender::bke::tests {

struct SVNFiles {
  const std::string svn_root = blender::tests::flags_test_asset_dir();
  const std::string project_root_rel = "blender_project/the_project";
  const std::string project_root = svn_root + "/blender_project/the_project";
};

class ProjectTest : public testing::Test {
  /* RAII helper to delete the created directories reliably after testing or on errors. */
  struct ProjectDirectoryRAIIWrapper {
    std::string project_path_;
    /* Path with OS preferred slashes ('/' on Unix, '\' on Windows). Important for some file
     * operations. */
    std::string native_project_path_;
    std::string base_path_;

    ProjectDirectoryRAIIWrapper(StringRefNull base_path, StringRefNull relative_project_path)
    {
      BLI_assert_msg(ELEM(base_path.back(), SEP, ALTSEP),
                     "Expected base_path to have trailing slash");
      std::string project_path = base_path + relative_project_path;

      native_project_path_ = project_path;
      BLI_path_slash_native(native_project_path_.data());
      if (native_project_path_.back() != SEP) {
        native_project_path_ += SEP;
      }

      /** Assert would be preferable but that would only run in debug builds, and #ASSERT_TRUE()
       * doesn't support printing a message. */
      if (BLI_exists(native_project_path_.c_str())) {
        throw std::runtime_error("Can't execute test, temporary path '" + project_path +
                                 "' already exists");
      }

      BLI_dir_create_recursive(native_project_path_.c_str());
      if (!BLI_exists(native_project_path_.c_str())) {
        throw std::runtime_error("Can't execute test, failed to create path '" +
                                 native_project_path_ + "'");
      }

      base_path_ = base_path;
      project_path_ = project_path;
      BLI_assert(StringRef{&project_path_[base_path.size()]} == relative_project_path);
    }

    ~ProjectDirectoryRAIIWrapper()
    {
      if (!project_path_.empty()) {
        /* Cut the path off at the first slash after the base path, so we delete the directory
         * created for the test. */
        const size_t first_slash_pos = native_project_path_.find_first_of(SEP, base_path_.size());
        std::string path_to_delete = native_project_path_;
        if (first_slash_pos != std::string::npos) {
          path_to_delete.erase(first_slash_pos);
        }
        BLI_delete(path_to_delete.c_str(), true, true);
        BLI_assert(!BLI_exists(native_project_path_.c_str()));
      }
    }
  };

 public:
  /* Run the test on multiple paths or variations of the same path. Useful to test things like
   * unicode paths, with or without trailing slash, non-native slashes, etc. The callback gets both
   * the unmodified path (possibly with non-native slashes), and the path converted to native
   * slashes passed. Call functions under test with the former, and use the latter to check the
   * results with BLI_fileops.h functions */
  void test_foreach_project_path(FunctionRef<void(StringRefNull /* project_path */,
                                                  StringRefNull /* project_path_native */)> fn)
  {
    const Vector<StringRefNull> subpaths = {
        "temporary-project-root",
        "test-temporary-unicode-dir-новый/temporary-project-root",
        /* Same but with trailing slash. */
        "test-temporary-unicode-dir-новый/temporary-project-root/",
        /* Windows style slash. */
        "test-temporary-unicode-dir-новый\\temporary-project-root",
        /* Windows style trailing slash. */
        "test-temporary-unicode-dir-новый\\temporary-project-root\\",
    };

    BKE_tempdir_init("");

    const std::string tempdir = BKE_tempdir_session();
    for (StringRefNull subpath : subpaths) {
      ProjectDirectoryRAIIWrapper temp_project_path(tempdir, subpath);
      fn(temp_project_path.project_path_, temp_project_path.native_project_path_);
    }
  }

  void TearDown() override
  {
    BKE_project_active_unset();
  }
};

TEST_F(ProjectTest, settings_create)
{
  test_foreach_project_path([](StringRefNull project_path, StringRefNull project_path_native) {
    if (!BlenderProject::create_settings_directory(project_path)) {
      /* Not a regular test failure, this may fail if there is a permission issue for example. */
      FAIL() << "Failed to create project directory in '" << project_path
             << "', check permissions";
    }
    std::string project_settings_dir = project_path_native + SEP_STR +
                                       BlenderProject::SETTINGS_DIRNAME;
    EXPECT_TRUE(BLI_exists(project_settings_dir.c_str()) &&
                BLI_is_dir(project_settings_dir.c_str()))
        << project_settings_dir + " was not created";
  });
}

/* Load the project by pointing to the project root directory (as opposed to the .blender_project
 * directory). */
TEST_F(ProjectTest, load_from_project_root_path)
{
  test_foreach_project_path([](StringRefNull project_path, StringRefNull project_path_native) {
    BlenderProject::create_settings_directory(project_path);

    std::unique_ptr project = BlenderProject::load_from_path(project_path);
    EXPECT_NE(project, nullptr);
    EXPECT_EQ(project->root_path(), project_path_native);
    EXPECT_EQ(project->get_settings().project_name(), "");
  });
}

/* Load the project by pointing to the .blender_project directory (as opposed to the project root
 * directory). */
TEST_F(ProjectTest, load_from_project_settings_path)
{
  test_foreach_project_path([](StringRefNull project_path, StringRefNull project_path_native) {
    BlenderProject::create_settings_directory(project_path);

    std::unique_ptr project = BlenderProject::load_from_path(
        project_path + (ELEM(project_path.back(), SEP, ALTSEP) ? "" : SEP_STR) +
        BlenderProject::SETTINGS_DIRNAME);
    EXPECT_NE(project, nullptr);
    EXPECT_EQ(project->root_path(), project_path_native);
    EXPECT_EQ(project->get_settings().project_name(), "");
  });
}

static void project_settings_match_expected_from_svn(const ProjectSettings &project_settings)
{
  EXPECT_EQ(project_settings.project_name(), "Ružena");

  const ListBase &asset_libraries = project_settings.asset_library_definitions();
  CustomAssetLibraryDefinition *first = (CustomAssetLibraryDefinition *)asset_libraries.first;
  EXPECT_STREQ(first->name, "Lorem Ipsum");
  EXPECT_STREQ(first->dirpath, "assets");
  EXPECT_EQ(first->next, asset_libraries.last);
  CustomAssetLibraryDefinition *last = (CustomAssetLibraryDefinition *)asset_libraries.last;
  EXPECT_EQ(last->prev, asset_libraries.first);
  EXPECT_STREQ(last->name, "Материалы");
  EXPECT_STREQ(last->dirpath, "новый\\assets");
}

TEST_F(ProjectTest, settings_json_read)
{
  SVNFiles svn_files{};
  std::unique_ptr from_project_settings = ProjectSettings::load_from_disk(svn_files.project_root);
  EXPECT_NE(from_project_settings, nullptr);
  project_settings_match_expected_from_svn(*from_project_settings);
}

TEST_F(ProjectTest, settings_json_write)
{
  SVNFiles svn_files{};
  std::unique_ptr from_project_settings = ProjectSettings::load_from_disk(svn_files.project_root);

  /* Take the settings read from the SVN files and write it to /tmp/ projects. */
  test_foreach_project_path(
      [&from_project_settings](StringRefNull to_project_path, StringRefNull) {
        BlenderProject::create_settings_directory(to_project_path);

        if (!from_project_settings->save_to_disk(to_project_path)) {
          FAIL();
        }

        /* Now check if the settings written to disk match the expectations. */
        std::unique_ptr written_settings = ProjectSettings::load_from_disk(to_project_path);
        EXPECT_NE(written_settings, nullptr);
        project_settings_match_expected_from_svn(*written_settings);
      });
}

TEST_F(ProjectTest, settings_read_change_write)
{
  SVNFiles svn_files{};
  std::unique_ptr from_project_settings = ProjectSettings::load_from_disk(svn_files.project_root);

  EXPECT_FALSE(from_project_settings->has_unsaved_changes());

  /* Take the settings read from the SVN files and write it to /tmp/ projects. */
  test_foreach_project_path(
      [&from_project_settings](StringRefNull to_project_path, StringRefNull) {
        BlenderProject::create_settings_directory(to_project_path);

        from_project_settings->project_name("новый");
        EXPECT_TRUE(from_project_settings->has_unsaved_changes());

        if (!from_project_settings->save_to_disk(to_project_path)) {
          FAIL();
        }
        EXPECT_FALSE(from_project_settings->has_unsaved_changes());

        /* Now check if the settings written to disk match the expectations. */
        std::unique_ptr written_settings = ProjectSettings::load_from_disk(to_project_path);
        EXPECT_NE(written_settings, nullptr);
        EXPECT_EQ(written_settings->project_name(), "новый");
        EXPECT_FALSE(from_project_settings->has_unsaved_changes());
      });
}

TEST_F(ProjectTest, project_root_path_find_from_path)
{
  /* Test the temporarily created directories with their various path formats. */
  test_foreach_project_path([](StringRefNull project_path, StringRefNull /*project_path_native*/) {
    /* First test without a .blender_project directory present. */
    EXPECT_EQ(BlenderProject::project_root_path_find_from_path(project_path), "");

    BlenderProject::create_settings_directory(project_path);
    EXPECT_EQ(BlenderProject::project_root_path_find_from_path(project_path), project_path);
  });

  SVNFiles svn_files{};

  /* Test the prepared project directory from the libs SVN repository. */
  EXPECT_EQ(BlenderProject::project_root_path_find_from_path(svn_files.project_root +
                                                             "/some_project_file.blend"),
            svn_files.project_root);
  EXPECT_EQ(BlenderProject::project_root_path_find_from_path(
                svn_files.project_root +
                "/unicode_subdirectory_новый/another_subdirectory/some_nested_project_file.blend"),
            svn_files.project_root);
}

class BlendfileProjectLoadingTest : public BlendfileLoadingBaseTest {
};

/* Test if loading the blend file loads the project data as expected. */
TEST_F(BlendfileProjectLoadingTest, load_blend_file)
{
  EXPECT_EQ(BKE_project_active_get(), nullptr);

  if (!blendfile_load("blender_project/the_project/some_project_file.blend")) {
    FAIL();
  }

  ::BlenderProject *svn_project = BKE_project_active_load_from_path(bfile->main->filepath);
  EXPECT_NE(svn_project, nullptr);
  EXPECT_EQ(BKE_project_active_get(), svn_project);
  EXPECT_STREQ("Ružena", BKE_project_name_get(svn_project));
  /* Note: The project above will be freed once a different active project is set. So get the path
   * for future comparisons. */
  std::string svn_project_path = BKE_project_root_path_get(svn_project);

  blendfile_free();

  /* Check if loading a different .blend updates the project properly */
  if (!blendfile_load("blender_project/the_project/unicode_subdirectory_новый/"
                      "another_subdirectory/some_nested_project_file.blend"))
  {
    FAIL();
  }
  ::BlenderProject *svn_project_from_nested = BKE_project_active_load_from_path(
      bfile->main->filepath);
  EXPECT_NE(svn_project_from_nested, nullptr);
  EXPECT_EQ(BKE_project_active_get(), svn_project_from_nested);
  EXPECT_STREQ(svn_project_path.c_str(), BKE_project_root_path_get(svn_project_from_nested));
  EXPECT_STREQ("Ružena", BKE_project_name_get(svn_project_from_nested));
  blendfile_free();

  /* Check if loading a .blend that's not in the project unsets the project properly. */
  if (!blendfile_load("blender_project/not_a_project_file.blend")) {
    FAIL();
  }
  BKE_project_active_load_from_path(bfile->main->filepath);
  EXPECT_EQ(BKE_project_active_get(), nullptr);
}

TEST_F(ProjectTest, project_load_and_delete)
{
  test_foreach_project_path([](StringRefNull project_path, StringRefNull project_path_native) {
    BlenderProject::create_settings_directory(project_path);

    ::BlenderProject *project = BKE_project_active_load_from_path(project_path.c_str());
    EXPECT_NE(project, nullptr);
    EXPECT_FALSE(BKE_project_has_unsaved_changes(project));

    std::string project_settings_dir = project_path_native + SEP_STR +
                                       BlenderProject::SETTINGS_DIRNAME;

    EXPECT_TRUE(BLI_exists(project_settings_dir.c_str()));
    if (!BKE_project_delete_settings_directory(project)) {
      FAIL();
    }
    /* Runtime project should still exist, but with unsaved changes. */
    EXPECT_NE(project, nullptr);
    EXPECT_TRUE(BKE_project_has_unsaved_changes(project));
    EXPECT_FALSE(BLI_exists(project_settings_dir.c_str()));
  });
}

}  // namespace blender::bke::tests
