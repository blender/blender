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
 * The Original Code is Copyright (C) 2020 by Blender Foundation.
 */
#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BKE_appdir.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"

#include "BLI_string.h"

#include "RE_engine.h"

#include "IMB_imbuf.h"

#include "CLG_log.h"

#include "RNA_access.h"

namespace blender::bke::tests {

TEST(view_layer, aov_unique_names)
{
  /* Set Up */
  CLG_init();
  BKE_idtype_init();
  BKE_appdir_init();
  IMB_init();
  RE_engines_init();

  Scene scene = {{nullptr}};
  IDType_ID_SCE.init_data(&scene.id);
  ViewLayer *view_layer = static_cast<ViewLayer *>(scene.view_layers.first);

  RenderEngineType *engine_type = RE_engines_find(scene.r.engine);
  RenderEngine *engine = RE_engine_create(engine_type);

  EXPECT_FALSE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_EQ(view_layer->active_aov, nullptr);

  /* Add an AOV */
  ViewLayerAOV *aov1 = BKE_view_layer_add_aov(view_layer);
  BKE_view_layer_verify_aov(engine, &scene, view_layer);
  EXPECT_EQ(view_layer->active_aov, aov1);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov1->flag & AOV_CONFLICT) != 0);

  /* Add a second AOV */
  ViewLayerAOV *aov2 = BKE_view_layer_add_aov(view_layer);
  BKE_view_layer_verify_aov(engine, &scene, view_layer);
  EXPECT_EQ(view_layer->active_aov, aov2);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov1->flag & AOV_CONFLICT) != 0);
  EXPECT_FALSE((aov2->flag & AOV_CONFLICT) != 0);
  EXPECT_TRUE(STREQ(aov1->name, "AOV"));
  EXPECT_TRUE(STREQ(aov2->name, "AOV_001"));

  /* Revert previous resolution */
  BLI_strncpy(aov2->name, "AOV", MAX_NAME);
  BKE_view_layer_verify_aov(engine, &scene, view_layer);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov1->flag & AOV_CONFLICT) != 0);
  EXPECT_FALSE((aov2->flag & AOV_CONFLICT) != 0);
  EXPECT_TRUE(STREQ(aov1->name, "AOV"));
  EXPECT_TRUE(STREQ(aov2->name, "AOV_001"));

  /* Resolve by removing AOV resolution */
  BKE_view_layer_remove_aov(view_layer, aov2);
  aov2 = nullptr;
  BKE_view_layer_verify_aov(engine, &scene, view_layer);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov1->flag & AOV_CONFLICT) != 0);

  /* Tear down */
  RE_engine_free(engine);
  RE_engines_exit();
  IDType_ID_SCE.free_data(&scene.id);
  IMB_exit();
  BKE_appdir_exit();
  CLG_exit();
}

static void test_render_pass_conflict(Scene *scene,
                                      RenderEngine *engine,
                                      ViewLayer *view_layer,
                                      ViewLayerAOV *aov,
                                      const char *render_pass_name,
                                      const char *rna_prop_name)
{
  PointerRNA ptr;
  RNA_pointer_create(&scene->id, &RNA_ViewLayer, view_layer, &ptr);
  RNA_boolean_set(&ptr, rna_prop_name, false);

  /* Rename to Conflicting name */
  BLI_strncpy(aov->name, render_pass_name, MAX_NAME);
  BKE_view_layer_verify_aov(engine, scene, view_layer);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov->flag & AOV_CONFLICT) != 0);
  EXPECT_TRUE(STREQ(aov->name, render_pass_name));

  /* Activate render pass */
  RNA_boolean_set(&ptr, rna_prop_name, true);
  BKE_view_layer_verify_aov(engine, scene, view_layer);
  EXPECT_FALSE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_TRUE((aov->flag & AOV_CONFLICT) != 0);
  EXPECT_TRUE(STREQ(aov->name, render_pass_name));

  /* Deactivate render pass */
  RNA_boolean_set(&ptr, rna_prop_name, false);
  BKE_view_layer_verify_aov(engine, scene, view_layer);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov->flag & AOV_CONFLICT) != 0);
  EXPECT_TRUE(STREQ(aov->name, render_pass_name));
}

TEST(view_layer, aov_conflict)
{
  /* Set Up */
  CLG_init();
  BKE_appdir_init();
  IMB_init();
  RE_engines_init();

  Scene scene = {{nullptr}};
  IDType_ID_SCE.init_data(&scene.id);
  ViewLayer *view_layer = static_cast<ViewLayer *>(scene.view_layers.first);

  RenderEngineType *engine_type = RE_engines_find(scene.r.engine);
  RenderEngine *engine = RE_engine_create(engine_type);

  EXPECT_FALSE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_EQ(view_layer->active_aov, nullptr);

  /* Add an AOV */
  ViewLayerAOV *aov = BKE_view_layer_add_aov(view_layer);
  BKE_view_layer_verify_aov(engine, &scene, view_layer);
  EXPECT_EQ(view_layer->active_aov, aov);
  EXPECT_TRUE(BKE_view_layer_has_valid_aov(view_layer));
  EXPECT_FALSE((aov->flag & AOV_CONFLICT) != 0);

  test_render_pass_conflict(&scene, engine, view_layer, aov, "Depth", "use_pass_z");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Normal", "use_pass_normal");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Mist", "use_pass_mist");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Shadow", "use_pass_shadow");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "AO", "use_pass_ambient_occlusion");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Emit", "use_pass_emit");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Env", "use_pass_environment");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "DiffDir", "use_pass_diffuse_direct");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "DiffCol", "use_pass_diffuse_color");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "GlossDir", "use_pass_glossy_direct");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "GlossCol", "use_pass_glossy_color");

  /* Tear down */
  RE_engine_free(engine);
  RE_engines_exit();
  IDType_ID_SCE.free_data(&scene.id);
  IMB_exit();
  BKE_appdir_exit();
  CLG_exit();
}

}  // namespace blender::bke::tests
