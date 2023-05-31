/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_fileops.hh"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_tempfile.h"
#include "BLI_threads.h"

#include BLI_SYSTEM_PID_H

namespace blender::tests {

class ChangeWorkingDirectoryTest : public testing::Test {
 public:
  std::string test_temp_dir;

  void SetUp() override
  {
    /* Must use because BLI_change_working_dir() checks that we are on the main thread. */
    BLI_threadapi_init();
  }

  void TearDown() override
  {
    if (!test_temp_dir.empty()) {
      BLI_delete(test_temp_dir.c_str(), true, false);
    }

    BLI_threadapi_exit();
  }

  /* Make a pseudo-unique file name file within the temp directory in a cross-platform manner. */
  static std::string make_pseudo_unique_temp_filename()
  {
    char temp_dir[FILE_MAX];
    BLI_temp_directory_path_get(temp_dir, sizeof(temp_dir));

    const std::string directory_name = "blender_test_" + std::to_string(getpid());

    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), temp_dir, directory_name.c_str());

    return filepath;
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
  char original_cwd_buff[FILE_MAX];
  char *original_cwd = BLI_current_working_dir(original_cwd_buff, sizeof(original_cwd_buff));

  ASSERT_FALSE(original_cwd == nullptr) << "Unable to get the current working directory.";
  /* While some implementation of `getcwd` (or similar) may return allocated memory in some cases,
   * in the context of `BLI_current_working_dir` usages, this is not expected and should not
   * happen. */
  ASSERT_TRUE(original_cwd == original_cwd_buff)
      << "Returned CWD path unexpectedly different than given char buffer.";

  std::string temp_file_name = make_pseudo_unique_temp_filename();
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

  char new_cwd_buff[FILE_MAX];
  char *new_cwd = BLI_current_working_dir(new_cwd_buff, sizeof(new_cwd_buff));
  ASSERT_FALSE(new_cwd == nullptr) << "Unable to get the current working directory.";
  ASSERT_TRUE(new_cwd == new_cwd_buff)
      << "Returned CWD path unexpectedly different than given char buffer.";

#ifdef __APPLE__
  /* The name returned by `std::tmpnam` is fine but the Apple OS method
   * picks the true var folder, not the alias, meaning the below
   * comparison always fails unless we prepend with the correct value. */
  test_temp_dir = "/private" + test_temp_dir;
#endif  // #ifdef __APPLE__

  ASSERT_EQ(BLI_path_cmp_normalized(new_cwd, test_temp_dir.c_str()), 0)
      << "the path of the current working directory should equal the path of the temporary "
         "directory that was created.";

  ASSERT_TRUE(BLI_change_working_dir(original_cwd))
      << "changing directory back to the original working directory should succeed.";

  char final_cwd_buff[FILE_MAX];
  char *final_cwd = BLI_current_working_dir(final_cwd_buff, sizeof(final_cwd_buff));
  ASSERT_FALSE(final_cwd == nullptr) << "Unable to get the current working directory.";
  ASSERT_TRUE(final_cwd == final_cwd_buff)
      << "Returned CWD path unexpectedly different than given char buffer.";

  ASSERT_EQ(BLI_path_cmp_normalized(final_cwd, original_cwd), 0)
      << "The final CWD path should be the same as the original CWD path.";
}

}  // namespace blender::tests
