/* SPDX-License-Identifier: Apache-2.0 */
#include "testing/testing.h"

#include "BLI_fileops.hh"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

namespace blender::tests {

class ChangeWorkingDirectoryTest : public testing::Test {
 public:
  std::string test_temp_dir;

  void TearDown() override
  {
    if (!test_temp_dir.empty()) {
      BLI_delete(test_temp_dir.c_str(), true, false);
    }

    BLI_threadapi_exit();
  }
};

TEST(fileops, fstream_open_string_filename)
{
  const std::string test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  const std::string filepath = test_files_dir + "/asset_library/новый/blender_assets.cats.txt";
  fstream in(filepath, std::ios_base::in);
  ASSERT_TRUE(in.is_open()) << "could not open " << filepath;
  in.close(); /* This should not crash. */

  /* Reading the file not tested here. That's deferred to `std::fstream` anyway. */
}

TEST(fileops, fstream_open_charptr_filename)
{
  const std::string test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  const std::string filepath_str = test_files_dir + "/asset_library/новый/blender_assets.cats.txt";
  const char *filepath = filepath_str.c_str();
  fstream in(filepath, std::ios_base::in);
  ASSERT_TRUE(in.is_open()) << "could not open " << filepath;
  in.close(); /* This should not crash. */

  /* Reading the file not tested here. That's deferred to `std::fstream` anyway. */
}

TEST_F(ChangeWorkingDirectoryTest, change_working_directory)
{
  /* Must use because BLI_change_working_dir() checks that we are on the main thread. */
  BLI_threadapi_init();

  char original_wd[FILE_MAX];
  if (!BLI_current_working_dir(original_wd, FILE_MAX)) {
    FAIL() << "unable to get the current working directory";
  }

  std::string temp_file_name(std::tmpnam(nullptr));
  test_temp_dir = temp_file_name + "_новый";

  if (BLI_exists(test_temp_dir.c_str())) {
    BLI_delete(test_temp_dir.c_str(), true, false);
  }

  ASSERT_FALSE(BLI_change_working_dir(test_temp_dir.c_str()))
      << "changing directory to a non-existent directory is expected to fail.";

  ASSERT_TRUE(BLI_dir_create_recursive(test_temp_dir.c_str()))
      << "temporary directory should have been created successfully.";

  ASSERT_TRUE(BLI_change_working_dir(test_temp_dir.c_str()))
      << "temporary directory should succeed changing directory.";

  char cwd[FILE_MAX];
  if (!BLI_current_working_dir(cwd, FILE_MAX)) {
    FAIL() << "unable to get the current working directory";
  }

#ifdef __APPLE__
  /* The name returned by std::tmpnam is fine but the Apple OS method
   * picks the true var folder, not the alias, meaning the below
   * comparison always fails unless we prepend with the correct value. */
  test_temp_dir = "/private" + test_temp_dir;
#endif // #ifdef __APPLE__

  ASSERT_EQ(BLI_path_cmp_normalized(cwd, test_temp_dir.c_str()), 0)
      << "the path of the current working directory should equal the path of the temporary "
         "directory that was created.";

  ASSERT_TRUE(BLI_change_working_dir(original_wd))
      << "changing directory back to the original working directory should succeed.";
}

}  // namespace blender::tests
