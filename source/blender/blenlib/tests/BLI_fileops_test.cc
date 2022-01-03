/* Apache License, Version 2.0 */

#include "BLI_fileops.hh"

#include "testing/testing.h"

namespace blender::tests {

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

}  // namespace blender::tests
