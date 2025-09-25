/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "BKE_appdir.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"

#include "BLI_string.h"

#include "RE_engine.h"

#include "IMB_imbuf.hh"

#include "CLG_log.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

namespace blender::bke::tests {

TEST(view_layer, aov_unique_names)
{
  /* Set Up */
  CLG_init();
  BKE_idtype_init();
  BKE_appdir_init();
  IMB_init();
  RE_engines_init();

  Scene scene = {};
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
  STRNCPY(aov2->name, "AOV");
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
  PointerRNA ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ViewLayer, view_layer);
  RNA_boolean_set(&ptr, rna_prop_name, false);

  /* Rename to Conflicting name */
  STRNCPY(aov->name, render_pass_name);
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

  Scene scene = {};
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
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Ambient Occlusion", "use_pass_ambient_occlusion");
  test_render_pass_conflict(&scene, engine, view_layer, aov, "Emission", "use_pass_emit");
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Environment", "use_pass_environment");
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Diffuse Direct", "use_pass_diffuse_direct");
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Diffuse Color", "use_pass_diffuse_color");
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Glossy Direct", "use_pass_glossy_direct");
  test_render_pass_conflict(
      &scene, engine, view_layer, aov, "Glossy Color", "use_pass_glossy_color");

  /* Tear down */
  RE_engine_free(engine);
  RE_engines_exit();
  IDType_ID_SCE.free_data(&scene.id);
  IMB_exit();
  BKE_appdir_exit();
  CLG_exit();
}

}  // namespace blender::bke::tests
