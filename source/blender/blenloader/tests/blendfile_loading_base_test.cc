/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "blendfile_loading_base_test.h"

#include "MEM_guardedalloc.h"

#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mball_tessellate.h"
#include "BKE_modifier.h"
#include "BKE_node.hh"
#include "BKE_scene.h"
#include "BKE_vfont.h"

#include "BLI_path_util.h"
#include "BLI_threads.h"

#include "BLO_readfile.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "DNA_genfile.h" /* for DNA_sdna_current_init() */
#include "DNA_windowmanager_types.h"

#include "IMB_imbuf.h"

#include "ED_datafiles.h"

#include "RNA_define.hh"

#include "WM_api.hh"
#include "wm.hh"

#include "GHOST_Path-api.hh"

#include "CLG_log.h"

void BlendfileLoadingBaseTest::SetUpTestCase()
{
  testing::Test::SetUpTestCase();

  /* Minimal code to make loading a blendfile and constructing a depsgraph not crash, copied from
   * main() in creator.c. */
  CLG_init();
  BLI_threadapi_init();

  DNA_sdna_current_init();
  BKE_blender_globals_init();

  BKE_idtype_init();
  BKE_appdir_init();
  IMB_init();
  BKE_modifier_init();
  DEG_register_node_types();
  RNA_init();
  BKE_node_system_init();
  BKE_callback_global_init();
  BKE_vfont_builtin_register(datatoc_bfont_pfb, datatoc_bfont_pfb_size);

  G.background = true;
  G.factory_startup = true;

  /* Allocate a dummy window manager. The real window manager will try and load Python scripts from
   * the release directory, which it won't be able to find. */
  ASSERT_EQ(G.main->wm.first, nullptr);
  G.main->wm.first = MEM_callocN(sizeof(wmWindowManager), __func__);
}

void BlendfileLoadingBaseTest::TearDownTestCase()
{
  if (G.main->wm.first != nullptr) {
    MEM_freeN(G.main->wm.first);
    G.main->wm.first = nullptr;
  }

  /* Copied from WM_exit_ex() in wm_init_exit.cc, and cherry-picked those lines that match the
   * allocation/initialization done in SetUpTestCase(). */
  BKE_blender_free();
  RNA_exit();

  DEG_free_node_types();
  GHOST_DisposeSystemPaths();
  DNA_sdna_current_free();
  BLI_threadapi_exit();

  BKE_blender_atexit();

  BKE_tempdir_session_purge();
  BKE_appdir_exit();
  CLG_exit();

  testing::Test::TearDownTestCase();
}

void BlendfileLoadingBaseTest::TearDown()
{
  BKE_mball_cubeTable_free();
  blendfile_free();
  depsgraph_free();

  testing::Test::TearDown();
}

bool BlendfileLoadingBaseTest::blendfile_load(const char *filepath)
{
  const std::string &test_assets_dir = blender::tests::flags_test_asset_dir();
  if (test_assets_dir.empty()) {
    return false;
  }

  char abspath[FILE_MAX];
  BLI_path_join(abspath, sizeof(abspath), test_assets_dir.c_str(), filepath);

  BlendFileReadReport bf_reports = {nullptr};
  bfile = BLO_read_from_file(abspath, BLO_READ_SKIP_NONE, &bf_reports);
  if (bfile == nullptr) {
    ADD_FAILURE() << "Unable to load file '" << filepath << "' from test assets dir '"
                  << test_assets_dir << "'";
    return false;
  }

  /* Make sure that all view_layers in the file are synced. Depsgraph can make a copy of the whole
   * scene, which will fail when one view layer isn't synced. */
  LISTBASE_FOREACH (ViewLayer *, view_layer, &bfile->curscene->view_layers) {
    BKE_view_layer_synced_ensure(bfile->curscene, view_layer);
  }

  return true;
}

void BlendfileLoadingBaseTest::blendfile_free()
{
  if (bfile == nullptr) {
    return;
  }

  wmWindowManager *wm = static_cast<wmWindowManager *>(bfile->main->wm.first);
  if (wm != nullptr) {
    wm_close_and_free(nullptr, wm);
  }
  BLO_blendfiledata_free(bfile);
  bfile = nullptr;
}

void BlendfileLoadingBaseTest::depsgraph_create(eEvaluationMode depsgraph_evaluation_mode)
{
  depsgraph = DEG_graph_new(
      bfile->main, bfile->curscene, bfile->cur_view_layer, depsgraph_evaluation_mode);
  DEG_graph_build_from_view_layer(depsgraph);
  BKE_scene_graph_update_tagged(depsgraph, bfile->main);
}

void BlendfileLoadingBaseTest::depsgraph_free()
{
  if (depsgraph == nullptr) {
    return;
  }
  DEG_graph_free(depsgraph);
  depsgraph = nullptr;
}
