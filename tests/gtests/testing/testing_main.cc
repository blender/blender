/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include <stdlib.h>

DEFINE_string(test_assets_dir, "", "tests/files directory containing the test assets.");
DEFINE_string(test_release_dir, "", "bin/{blender version} directory of the current build.");

namespace blender::tests {

const std::string &flags_test_asset_dir()
{
  if (FLAGS_test_assets_dir.empty()) {
    ADD_FAILURE() << "Pass the flag --test-assets-dir and point to the tests/files directory.";
  }
  return FLAGS_test_assets_dir;
}

const std::string &flags_test_release_dir()
{
  if (FLAGS_test_release_dir.empty()) {
    ADD_FAILURE()
        << "Pass the flag --test-release-dir and point to the bin/{blender version} directory.";
  }
  return FLAGS_test_release_dir;
}

bool should_ignore_blocklist()
{
  static bool has_env = getenv("BLENDER_TEST_IGNORE_BLOCKLIST");
  return has_env;
}

}  // namespace blender::tests

int main(int argc, char **argv)
{
  MEM_use_guarded_allocator();
  MEM_init_memleak_detection();
  MEM_enable_fail_on_memleak();
  testing::InitGoogleTest(&argc, argv);
  BLENDER_GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  return RUN_ALL_TESTS();
}
