/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anim_data.hh"
#include "BKE_appdir.hh"
#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "BLO_userdef_default.h"

#include "IMB_imbuf.hh"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "CLG_log.h"

#include "testing/testing.h"

namespace blender::bke::tests {

TEST(scene, frame_snap_by_seconds)
{
  Scene fake_scene = {};

  /* Regular 24 FPS snapping. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 1.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 59));
  EXPECT_FLOAT_EQ(72.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 60));
  EXPECT_FLOAT_EQ(9984.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));

  /* 12 FPS snapping by incrementing the base. */
  fake_scene.r.frs_sec = 24;
  fake_scene.r.frs_sec_base = 2.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 47));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 49));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 53));
  EXPECT_FLOAT_EQ(60.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 54));
  EXPECT_FLOAT_EQ(9996.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 1.0, 10000.0));

  /* 0.1 FPS snapping to 2-second intervals. */
  fake_scene.r.frs_sec = 1;
  fake_scene.r.frs_sec_base = 10.0;
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.0));
  EXPECT_FLOAT_EQ(48.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.1));
  EXPECT_FLOAT_EQ(48.2, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 48.2));
  EXPECT_FLOAT_EQ(10000.0, BKE_scene_frame_snap_by_seconds(&fake_scene, 2.0, 10000.0));
}

class SceneTest : public ::testing::Test {
 public:
  Main *bmain;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_appdir_init();
    IMB_init();
    BKE_idtype_init();
    /* #BKE_scene_duplicate() uses #U::dupflag. */
    U = blender::dna::shallow_copy(U_default);
  }

  static void TearDownTestSuite()
  {
    IMB_exit();
    BKE_appdir_exit();
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(SceneTest, linked_copy_id_remapping)
{
  Scene *scene_src = BKE_id_new<Scene>(bmain, "Scene_source");
  bAction *action_src = BKE_id_new<bAction>(bmain, "Scene_source_action");
  BKE_animdata_set_action(nullptr, &scene_src->id, action_src);
  AnimData *animdata_src = BKE_animdata_from_id(&scene_src->id);
  ASSERT_NE(animdata_src, nullptr);
  EXPECT_EQ(animdata_src->action, action_src);

  constexpr blender::StringRef idp_scene2scene_name = "scene2scene";
  constexpr blender::StringRef idp_scene2action_name = "scene2action";
  constexpr blender::StringRef idp_action2scene_name = "action2scene";
  constexpr blender::StringRef idp_action2action_name = "action2action";

  IDProperty *scene_idgroup_src = IDP_EnsureProperties(&scene_src->id);
  IDP_AddToGroup(scene_idgroup_src,
                 bke::idprop::create(idp_scene2scene_name, &scene_src->id).release());
  IDP_AddToGroup(scene_idgroup_src,
                 bke::idprop::create(idp_scene2action_name, &action_src->id).release());

  IDProperty *action_idgroup_src = IDP_EnsureProperties(&action_src->id);
  IDP_AddToGroup(action_idgroup_src,
                 bke::idprop::create(idp_action2scene_name, &scene_src->id).release());
  IDP_AddToGroup(action_idgroup_src,
                 bke::idprop::create(idp_action2action_name, &action_src->id).release());

  Scene *scene_copy = BKE_scene_duplicate(bmain, scene_src, SCE_COPY_LINK_COLLECTION);

  /* Source data should remain unchanged. */

  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_src, idp_scene2scene_name)),
            &scene_src->id);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_src, idp_scene2action_name)),
            &action_src->id);

  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(action_idgroup_src, idp_action2scene_name)),
            &scene_src->id);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(action_idgroup_src, idp_action2action_name)),
            &action_src->id);

  /* Copied data should have its ID usages remapped to new copies if possible. */

  EXPECT_NE(scene_copy, scene_src);
  AnimData *animdata_copy = BKE_animdata_from_id(&scene_copy->id);
  ASSERT_NE(animdata_copy, nullptr);
  EXPECT_NE(animdata_copy, animdata_src);
  bAction *action_copy = animdata_copy->action;
  ASSERT_NE(action_copy, nullptr);
  EXPECT_NE(action_copy, action_src);

  IDProperty *scene_idgroup_copy = IDP_GetProperties(&scene_copy->id);
  ASSERT_NE(scene_idgroup_copy, nullptr);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_copy, idp_scene2scene_name)),
            &scene_copy->id);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(scene_idgroup_copy, idp_scene2action_name)),
            &action_copy->id);

  IDProperty *action_idgroup_copy = IDP_GetProperties(&action_copy->id);
  ASSERT_NE(action_idgroup_copy, nullptr);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(action_idgroup_copy, idp_action2scene_name)),
            &scene_copy->id);
  EXPECT_EQ(IDP_ID_get(IDP_GetPropertyFromGroup(action_idgroup_copy, idp_action2action_name)),
            &action_copy->id);
}

}  // namespace blender::bke::tests
