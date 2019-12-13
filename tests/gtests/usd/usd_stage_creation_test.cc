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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "testing/testing.h"
#include <pxr/usd/usd/stage.h>

#include <string>

extern "C" {
#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"

/* Workaround to make it possible to pass a path at runtime to USD. See creator.c. */
void usd_initialise_plugin_path(const char *datafiles_usd_path);
}

DEFINE_string(test_blender_executable_dir, "", "Blender's installation directory.");

class USDStageCreationTest : public testing::Test {
};

TEST_F(USDStageCreationTest, JSONFileLoadingTest)
{
  std::string filename = "usd-stage-creation-test.usdc";

  if (FLAGS_test_blender_executable_dir.empty()) {
    FAIL() << "Pass the --test-blender-executable-dir flag";
  }

  /* Required on Linux to make BKE_appdir_folder_id() find the datafiles.
   * Without going to this directory, Blender looks for the datafiles in
   * .../bin/tests instead of .../bin */
  const char *blender_executable_dir = FLAGS_test_blender_executable_dir.c_str();
  if (chdir(blender_executable_dir) < 0) {
    FAIL() << "unable to change directory to " << FLAGS_test_blender_executable_dir;
  }

  const char *usd_datafiles_relpath = BKE_appdir_folder_id(BLENDER_DATAFILES, "usd");
  EXPECT_NE(usd_datafiles_relpath, nullptr) << "Unable to find datafiles/usd";

  char usd_datafiles_abspath[FILE_MAX];
  BLI_path_join(usd_datafiles_abspath,
                sizeof(usd_datafiles_abspath),
                blender_executable_dir,
                usd_datafiles_relpath,
                NULL);

  usd_initialise_plugin_path(usd_datafiles_abspath);

  /* Simply the ability to create a USD Stage for a specific filename means that the extension
   * has been recognised by the USD library, and that a USD plugin has been loaded to write such
   * files. Practically, this is a test to see whether the USD JSON files can be found and
   * loaded. */
  pxr::UsdStageRefPtr usd_stage = pxr::UsdStage::CreateNew(filename);
  if (usd_stage != nullptr) {
    /* Even though we don't call usd_stage->SaveFile(), a file is still created on the filesystem
     * when we call CreateNew(). It's immediately closed, though, so we can safely call unlink()
     * here. */
    unlink(filename.c_str());
  }
  else {
    FAIL() << "unable to find suitable USD plugin to write " << filename;
  }
}
