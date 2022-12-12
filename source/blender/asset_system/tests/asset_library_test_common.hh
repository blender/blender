/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <string>

#include "BKE_appdir.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::asset_system::tests {

/**
 * Functionality to setup and access directories on disk within which asset library related testing
 * can be done.
 */
class AssetLibraryTestBase : public testing::Test {
 protected:
  std::string asset_library_root_;
  std::string temp_library_path_;

  static void SetUpTestSuite()
  {
    testing::Test::SetUpTestSuite();
    CLG_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
    testing::Test::TearDownTestSuite();
  }

  void SetUp() override
  {
    const std::string test_files_dir = blender::tests::flags_test_asset_dir();
    if (test_files_dir.empty()) {
      FAIL();
    }

    asset_library_root_ = test_files_dir + SEP_STR + "asset_library";
    temp_library_path_ = "";
  }

  void TearDown() override
  {
    if (!temp_library_path_.empty()) {
      BLI_delete(temp_library_path_.c_str(), true, true);
      temp_library_path_ = "";
    }
  }

  /* Register a temporary path, which will be removed at the end of the test.
   * The returned path ends in a slash. */
  std::string use_temp_path()
  {
    BKE_tempdir_init("");
    const std::string tempdir = BKE_tempdir_session();
    temp_library_path_ = tempdir + "test-temporary-path" + SEP_STR;
    return temp_library_path_;
  }

  std::string create_temp_path()
  {
    std::string path = use_temp_path();
    BLI_dir_create_recursive(path.c_str());
    return path;
  }
};

}  // namespace blender::asset_system::tests
