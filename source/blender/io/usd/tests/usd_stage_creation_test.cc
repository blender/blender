/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include <pxr/base/plug/registry.h>
#include <pxr/usd/usd/stage.h>

#include <string>

#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"

namespace blender::io::usd {

class USDStageCreationTest : public testing::Test {
};

TEST_F(USDStageCreationTest, JSONFileLoadingTest)
{
  /* Simply the ability to create a USD Stage for a specific filename means that the extension
   * has been recognized by the USD library, and that a USD plugin has been loaded to write such
   * files. Practically, this is a test to see whether the USD JSON files can be found and
   * loaded. */
  std::string filename = "usd-stage-creation-test.usdc";
  pxr::UsdStageRefPtr usd_stage = pxr::UsdStage::CreateNew(filename);
  if (usd_stage != nullptr) {
    /* Even though we don't call `usd_stage->SaveFile()`, a file is still created on the
     * file-system when we call CreateNew(). It's immediately closed, though,
     * so we can safely call `unlink()` here. */
    unlink(filename.c_str());
  }
  else {
    FAIL() << "unable to find suitable USD plugin to write " << filename;
  }
}

}  // namespace blender::io::usd
