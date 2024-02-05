/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLO_readfile.h"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

#include "usd.h"

namespace blender::io::usd {

const StringRefNull usdz_export_test_filename = "usd/usdz_export_test.blend";
char temp_dir[FILE_MAX];
char temp_output_dir[FILE_MAX];
char output_filepath[FILE_MAX];

class UsdUsdzExportTest : public BlendfileLoadingBaseTest {
 protected:
  bContext *context = nullptr;

 public:
  bool load_file_and_depsgraph(const StringRefNull &filepath,
                               const eEvaluationMode eval_mode = DAG_EVAL_VIEWPORT)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(eval_mode);

    context = CTX_create();
    CTX_data_main_set(context, bfile->main);
    CTX_data_scene_set(context, bfile->curscene);

    return true;
  }

  virtual void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();

    BKE_tempdir_init(nullptr);
    const char *temp_base_dir = BKE_tempdir_base();

    BLI_path_join(temp_dir, FILE_MAX, temp_base_dir, "usdz_test_temp_dir");
    BLI_dir_create_recursive(temp_dir);

    BLI_path_join(temp_output_dir, FILE_MAX, temp_base_dir, "usdz_test_output_dir");
    BLI_dir_create_recursive(temp_output_dir);

    BLI_path_join(output_filepath, FILE_MAX, temp_output_dir, "output_новый.usdz");
  }

  virtual void TearDown() override
  {
    BlendfileLoadingBaseTest::TearDown();
    CTX_free(context);
    context = nullptr;

    BLI_delete(temp_dir, true, true);
    BLI_delete(temp_output_dir, true, true);
  }
};

TEST_F(UsdUsdzExportTest, usdz_export)
{
  if (!load_file_and_depsgraph(usdz_export_test_filename)) {
    ADD_FAILURE();
    return;
  }

  /* File sanity check. */
  ASSERT_EQ(BLI_listbase_count(&bfile->main->objects), 4)
      << "Blender scene should have 4 objects.";

  char original_cwd_buff[FILE_MAX];
  char *original_cwd = BLI_current_working_dir(original_cwd_buff, sizeof(original_cwd_buff));
  /* Buffer is expected to be returned by #BLI_current_working_dir, although in theory other
   * returns are possible on some platforms, this is not handled by this code. */
  ASSERT_EQ(original_cwd, original_cwd_buff)
      << "BLI_current_working_dir is not expected to return a different value than the given char "
         "buffer.";

  USDExportParams params;
  params.export_materials = false;
  params.visible_objects_only = false;

  bool result = USD_export(context, output_filepath, &params, false, nullptr);
  ASSERT_TRUE(result) << "usd export to " << output_filepath << " failed.";

  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(output_filepath);
  ASSERT_TRUE(bool(stage)) << "unable to open stage for the exported usdz file.";

  pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/root/Cube"));
  EXPECT_TRUE(bool(test_prim)) << "Cube prim should exist in exported usdz file.";

  test_prim = stage->GetPrimAtPath(pxr::SdfPath("/root/Cylinder"));
  EXPECT_TRUE(bool(test_prim)) << "Cylinder prim should exist in exported usdz file.";

  test_prim = stage->GetPrimAtPath(pxr::SdfPath("/root/Icosphere"));
  EXPECT_TRUE(bool(test_prim)) << "Icosphere prim should exist in exported usdz file.";

  test_prim = stage->GetPrimAtPath(pxr::SdfPath("/root/Sphere"));
  EXPECT_TRUE(bool(test_prim)) << "Sphere prim should exist in exported usdz file.";

  char final_cwd_buff[FILE_MAX];
  char *final_cwd = BLI_current_working_dir(final_cwd_buff, sizeof(final_cwd_buff));
  /* Buffer is expected to be returned by #BLI_current_working_dir, although in theory other
   * returns are possible on some platforms, this is not handled by this code. */
  ASSERT_EQ(final_cwd, final_cwd_buff) << "BLI_current_working_dir is not expected to return "
                                          "a different value than the given char buffer.";
  EXPECT_TRUE(STREQ(original_cwd, final_cwd))
      << "Final CWD should be the same as the original one.";
}

}  // namespace blender::io::usd
