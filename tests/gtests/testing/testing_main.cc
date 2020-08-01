/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

DEFINE_string(test_assets_dir, "", "lib/tests directory from SVN containing the test assets.");
DEFINE_string(test_release_dir, "", "bin/{blender version} directory of the current build.");

namespace blender::tests {

const std::string &flags_test_asset_dir()
{
  if (FLAGS_test_assets_dir.empty()) {
    ADD_FAILURE()
        << "Pass the flag --test-assets-dir and point to the lib/tests directory from SVN.";
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

}  // namespace blender::tests

int main(int argc, char **argv)
{
  MEM_init_memleak_detection();
  testing::InitGoogleTest(&argc, argv);
  BLENDER_GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  return RUN_ALL_TESTS();
}
