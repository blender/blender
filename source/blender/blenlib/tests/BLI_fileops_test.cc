/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <fcntl.h>

#include "testing/testing.h"

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_tempfile.h"
#include "BLI_threads.h"

#include BLI_SYSTEM_PID_H

namespace blender::tests {

/*
 * General `BLI_fileops.h` tests.
 */

class FileOpsTest : public testing::Test {
 public:
  /* The base temp directory for all tests using this helper class. Absolute path. */
  std::string temp_dir;

  void SetUp() override
  {
    char temp_dir_c[FILE_MAX];
    BLI_temp_directory_path_get(temp_dir_c, sizeof(temp_dir_c));

    temp_dir = std::string(temp_dir_c) + SEP_STR + "blender_fileops_test_" +
               std::to_string(getpid());
    if (!BLI_exists(temp_dir.c_str())) {
      BLI_dir_create_recursive(temp_dir.c_str());
    }
  }

  void TearDown() override
  {
    if (BLI_exists(temp_dir.c_str())) {
      BLI_delete(temp_dir.c_str(), true, true);
    }
  }
};

TEST_F(FileOpsTest, rename)
{
  const std::string file_name_src = "test_file_src.txt";
  const std::string file_name_dst = "test_file_dst.txt";

  const std::string test_filepath_src = temp_dir + SEP_STR + file_name_src;
  const std::string test_filepath_dst = temp_dir + SEP_STR + file_name_dst;

  ASSERT_FALSE(BLI_exists(test_filepath_src.c_str()));
  ASSERT_FALSE(BLI_exists(test_filepath_dst.c_str()));
  BLI_file_touch(test_filepath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));

  /* `test_filepath_dst` does not exist, so regular rename should succeed. */
  ASSERT_EQ(0, BLI_rename(test_filepath_src.c_str(), test_filepath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_filepath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_filepath_dst.c_str()));

  BLI_file_touch(test_filepath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));

  /* `test_filepath_dst` does exist now, so regular rename should fail. */
  ASSERT_NE(0, BLI_rename(test_filepath_src.c_str(), test_filepath_dst.c_str()));
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_filepath_dst.c_str()));

  BLI_file_touch(test_filepath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));

  /* `test_filepath_dst` does exist now, but overwrite rename should succeed on all systems. */
  ASSERT_EQ(0, BLI_rename_overwrite(test_filepath_src.c_str(), test_filepath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_filepath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_filepath_dst.c_str()));

  BLI_file_touch(test_filepath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));

  /* Keep `test_filepath_dst` read-open before attempting to rename `test_filepath_src` to
   * `test_filepath_dst`.
   *
   * This is expected to succeed on Unix, but fail on Windows. */
  int fd_dst = BLI_open(test_filepath_dst.c_str(), O_BINARY | O_RDONLY, 0);
#ifdef WIN32
  ASSERT_NE(0, BLI_rename_overwrite(test_filepath_src.c_str(), test_filepath_dst.c_str()));
  ASSERT_TRUE(BLI_exists(test_filepath_src.c_str()));
#else
  ASSERT_EQ(0, BLI_rename_overwrite(test_filepath_src.c_str(), test_filepath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_filepath_src.c_str()));
#endif
  ASSERT_TRUE(BLI_exists(test_filepath_dst.c_str()));

  close(fd_dst);

  /*
   * Check directory renaming.
   */

  const std::string dir_name_src = "test_dir_src";
  const std::string dir_name_dst = "test_dir_dst";

  const std::string test_dirpath_src = temp_dir + SEP_STR + dir_name_src;
  const std::string test_dirpath_dst = temp_dir + SEP_STR + dir_name_dst;

  BLI_dir_create_recursive(test_dirpath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));

  /* `test_dirpath_dst` does not exist, so regular rename should succeed. */
  ASSERT_EQ(0, BLI_rename(test_dirpath_src.c_str(), test_dirpath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_dirpath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_dst.c_str()));

  BLI_dir_create_recursive(test_dirpath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));

  /* `test_dirpath_dst` now exists, so regular rename should fail. */
  ASSERT_NE(0, BLI_rename(test_dirpath_src.c_str(), test_dirpath_dst.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_dst.c_str()));

  /* `test_dirpath_dst` now exists, but is empty, so overwrite rename should succeed. */
  ASSERT_EQ(0, BLI_rename_overwrite(test_dirpath_src.c_str(), test_dirpath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_dirpath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_dst.c_str()));

  BLI_dir_create_recursive(test_dirpath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));

  const std::string test_dir_filepath_src = test_dirpath_src + SEP_STR + file_name_src;
  const std::string test_dir_filepath_dst = test_dirpath_dst + SEP_STR + file_name_dst;

  ASSERT_FALSE(BLI_exists(test_dir_filepath_src.c_str()));
  ASSERT_FALSE(BLI_exists(test_dir_filepath_dst.c_str()));
  BLI_file_touch(test_dir_filepath_src.c_str());
  ASSERT_TRUE(BLI_exists(test_dir_filepath_src.c_str()));

  /* `test_dir_filepath_src` does not exist, so regular rename should succeed. */
  ASSERT_EQ(0, BLI_rename(test_dir_filepath_src.c_str(), test_dir_filepath_dst.c_str()));
  ASSERT_FALSE(BLI_exists(test_dir_filepath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dir_filepath_dst.c_str()));

  /* `test_dirpath_dst` exists and is not empty, so regular rename should fail. */
  ASSERT_NE(0, BLI_rename(test_dirpath_src.c_str(), test_dirpath_dst.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_dst.c_str()));

  /* `test_dirpath_dst` exists and is not empty, so even overwrite rename should fail. */
  ASSERT_NE(0, BLI_rename_overwrite(test_dirpath_src.c_str(), test_dirpath_dst.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_src.c_str()));
  ASSERT_TRUE(BLI_exists(test_dirpath_dst.c_str()));
}

/*
 * blender::fstream tests.
 */

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

/*
 * Current Directory operations tests.
 */

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
