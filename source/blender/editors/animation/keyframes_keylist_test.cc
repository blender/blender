/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "ANIM_action.hh"
#include "ANIM_fcurve.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_keylist.hh"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "CLG_log.h"
#include "testing/testing.h"

#include <functional>
#include <optional>

namespace blender::editor::animation::tests {

const float KEYLIST_NEAR_ERROR = 0.1;
const float FRAME_STEP = 0.005;

/* Build FCurve with keys on frames 10, 20, and 30. */
static void build_fcurve(FCurve &fcurve)
{
  fcurve.totvert = 3;
  fcurve.bezt = MEM_calloc_arrayN<BezTriple>(fcurve.totvert, "BezTriples");
  fcurve.bezt[0].vec[1][0] = 10.0f;
  fcurve.bezt[0].vec[1][1] = 1.0f;
  fcurve.bezt[1].vec[1][0] = 20.0f;
  fcurve.bezt[1].vec[1][1] = 2.0f;
  fcurve.bezt[2].vec[1][0] = 30.0f;
  fcurve.bezt[2].vec[1][1] = 1.0f;
}

static AnimKeylist *create_test_keylist()
{
  FCurve *fcurve = BKE_fcurve_create();
  build_fcurve(*fcurve);

  AnimKeylist *keylist = ED_keylist_create();
  fcurve_to_keylist(nullptr, fcurve, keylist, 0, {-FLT_MAX, FLT_MAX}, false);
  BKE_fcurve_free(fcurve);

  ED_keylist_prepare_for_direct_access(keylist);
  return keylist;
}

static void assert_act_key_column(const ActKeyColumn *column,
                                  const std::optional<float> expected_frame)
{
  if (expected_frame.has_value()) {
    ASSERT_NE(column, nullptr) << "Expected a frame to be found at " << *expected_frame;
    EXPECT_NEAR(column->cfra, *expected_frame, KEYLIST_NEAR_ERROR);
  }
  else {
    EXPECT_EQ(column, nullptr) << "Expected no frame to be found, but found " << column->cfra;
  }
}

using KeylistFindFunction = std::function<const ActKeyColumn *(const AnimKeylist *, float)>;

static void check_keylist_find_range(const AnimKeylist *keylist,
                                     KeylistFindFunction keylist_find_func,
                                     const float frame_from,
                                     const float frame_to,
                                     const std::optional<float> expected_frame)
{
  float cfra = frame_from;
  for (; cfra < frame_to; cfra += FRAME_STEP) {
    const ActKeyColumn *found = keylist_find_func(keylist, cfra);
    assert_act_key_column(found, expected_frame);
  }
}

static void check_keylist_find_next_range(const AnimKeylist *keylist,
                                          const float frame_from,
                                          const float frame_to,
                                          const std::optional<float> expected_frame)
{
  check_keylist_find_range(keylist, ED_keylist_find_next, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_next)
{
  AnimKeylist *keylist = create_test_keylist();

  check_keylist_find_next_range(keylist, 0.0f, 9.99f, 10.0f);
  check_keylist_find_next_range(keylist, 10.0f, 19.99f, 20.0f);
  check_keylist_find_next_range(keylist, 20.0f, 29.99f, 30.0f);
  check_keylist_find_next_range(keylist, 30.0f, 39.99f, std::nullopt);

  ED_keylist_free(keylist);
}

static void check_keylist_find_prev_range(const AnimKeylist *keylist,
                                          const float frame_from,
                                          const float frame_to,
                                          const std::optional<float> expected_frame)
{
  check_keylist_find_range(keylist, ED_keylist_find_prev, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_prev)
{
  AnimKeylist *keylist = create_test_keylist();

  check_keylist_find_prev_range(keylist, 0.0f, 10.00f, std::nullopt);
  check_keylist_find_prev_range(keylist, 10.01f, 20.00f, 10.0f);
  check_keylist_find_prev_range(keylist, 20.01f, 30.00f, 20.0f);
  check_keylist_find_prev_range(keylist, 30.01f, 49.99f, 30.0f);

  ED_keylist_free(keylist);
}

static void check_keylist_find_exact_range(const AnimKeylist *keylist,
                                           const float frame_from,
                                           const float frame_to,
                                           const std::optional<float> expected_frame)
{
  check_keylist_find_range(keylist, ED_keylist_find_exact, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_exact)
{
  AnimKeylist *keylist = create_test_keylist();

  check_keylist_find_exact_range(keylist, 0.0f, 9.99f, std::nullopt);
  check_keylist_find_exact_range(keylist, 9.9901f, 10.01f, 10.0f);
  check_keylist_find_exact_range(keylist, 10.01f, 19.99f, std::nullopt);
  check_keylist_find_exact_range(keylist, 19.9901f, 20.01f, 20.0f);
  check_keylist_find_exact_range(keylist, 20.01f, 29.99f, std::nullopt);
  check_keylist_find_exact_range(keylist, 29.9901f, 30.01f, 30.0f);
  check_keylist_find_exact_range(keylist, 30.01f, 49.99f, std::nullopt);

  ED_keylist_free(keylist);
}

TEST(keylist, find_closest)
{
  AnimKeylist *keylist = create_test_keylist();

  {
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, -1);
    EXPECT_EQ(closest->cfra, 10.0);
  }

  {
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, 10);
    EXPECT_EQ(closest->cfra, 10.0);
  }

  {
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, 14.999);
    EXPECT_EQ(closest->cfra, 10.0);
  }
  {
    /* When the distance between key columns is equal, the previous column is chosen */
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, 15);
    EXPECT_EQ(closest->cfra, 10.0);
  }
  {
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, 15.001);
    EXPECT_EQ(closest->cfra, 20.0);
  }
  {
    const ActKeyColumn *closest = ED_keylist_find_closest(keylist, 30.001);
    EXPECT_EQ(closest->cfra, 30.0);
  }
  ED_keylist_free(keylist);
}

class KeylistSummaryTest : public testing::Test {
 public:
  Main *bmain;
  blender::animrig::Action *action;
  Object *cube;
  Object *armature;
  bArmature *armature_data;
  Bone *bone1;
  Bone *bone2;

  SpaceAction saction = {nullptr};
  bAnimContext ac = {nullptr};

  static void SetUpTestSuite()
  {
    /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialized properly. */
    CLG_init();

    /* To make id_can_have_animdata() and friends work, the `id_types` array needs to be set up. */
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
    G_MAIN = bmain; /* For BKE_animdata_free(). */

    action = &BKE_id_new<bAction>(bmain, "ACÄnimåtië")->wrap();
    cube = BKE_object_add_only_object(bmain, OB_EMPTY, "Küüübus");

    armature_data = BKE_armature_add(bmain, "ARArmature");
    bone1 = reinterpret_cast<Bone *>(MEM_callocN(sizeof(Bone), "KeylistSummaryTest"));
    bone2 = reinterpret_cast<Bone *>(MEM_callocN(sizeof(Bone), "KeylistSummaryTest"));
    STRNCPY_UTF8(bone1->name, "Bone.001");
    STRNCPY_UTF8(bone2->name, "Bone.002");
    BLI_addtail(&armature_data->bonebase, bone1);
    BLI_addtail(&armature_data->bonebase, bone2);
    BKE_armature_bone_hash_make(armature_data);

    armature = BKE_object_add_only_object(bmain, OB_ARMATURE, "OBArmature");
    armature->data = armature_data;
    BKE_pose_ensure(bmain, armature, armature_data, false);

    /*
     * Fill in the common bits for the mock bAnimContext, for an Action editor.
     *
     * Tests should fill in:
     * - ac.obact
     * - ac.active_action_user (= &ac.obact.id)
     */
    saction.ads.filterflag = eDopeSheet_FilterFlag(0);
    ac.bmain = bmain;
    ac.datatype = ANIMCONT_ACTION;
    ac.data = action;
    ac.spacetype = SPACE_ACTION;
    ac.sl = reinterpret_cast<SpaceLink *>(&saction);
    ac.ads = &saction.ads;
    ac.active_action = action;
  }

  void TearDown() override
  {
    ac.obact = nullptr;
    ac.active_action = nullptr;
    ac.active_action_user = nullptr;

    BKE_main_free(bmain);
    G_MAIN = nullptr;
  }
};

TEST_F(KeylistSummaryTest, slot_summary_simple)
{
  /* Test that a key summary is generated correctly for a slot that's animating
   * an object's transforms. */

  using namespace blender::animrig;

  Slot &slot_cube = action->slot_add_for_id(cube->id);
  ASSERT_EQ(ActionSlotAssignmentResult::OK, assign_action_and_slot(action, &slot_cube, cube->id));
  Channelbag &channelbag = action_channelbag_ensure(*action, cube->id);

  FCurve &loc_x = channelbag.fcurve_ensure(bmain, {"location", 0});
  FCurve &loc_y = channelbag.fcurve_ensure(bmain, {"location", 1});
  FCurve &loc_z = channelbag.fcurve_ensure(bmain, {"location", 2});

  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_x, {1.0, 0.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_x, {2.0, 1.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_y, {2.0, 2.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_y, {3.0, 3.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_z, {2.0, 4.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&loc_z, {5.0, 5.0}, {}, {}));

  /* Generate slot summary keylist. */
  AnimKeylist *keylist = ED_keylist_create();
  ac.obact = cube;
  ac.active_action_user = &cube->id;
  action_slot_summary_to_keylist(
      &ac, &cube->id, *action, slot_cube.handle, keylist, 0, {0.0, 6.0});
  ED_keylist_prepare_for_direct_access(keylist);

  const ActKeyColumn *col_0 = ED_keylist_find_exact(keylist, 0.0);
  const ActKeyColumn *col_1 = ED_keylist_find_exact(keylist, 1.0);
  const ActKeyColumn *col_2 = ED_keylist_find_exact(keylist, 2.0);
  const ActKeyColumn *col_3 = ED_keylist_find_exact(keylist, 3.0);
  const ActKeyColumn *col_4 = ED_keylist_find_exact(keylist, 4.0);
  const ActKeyColumn *col_5 = ED_keylist_find_exact(keylist, 5.0);
  const ActKeyColumn *col_6 = ED_keylist_find_exact(keylist, 6.0);

  /* Check that we only have columns at the frames with keys. */
  EXPECT_EQ(nullptr, col_0);
  EXPECT_NE(nullptr, col_1);
  EXPECT_NE(nullptr, col_2);
  EXPECT_NE(nullptr, col_3);
  EXPECT_EQ(nullptr, col_4);
  EXPECT_NE(nullptr, col_5);
  EXPECT_EQ(nullptr, col_6);

  /* Check that the right number of keys are indicated in each column. */
  EXPECT_EQ(1, col_1->totkey);
  EXPECT_EQ(3, col_2->totkey);
  EXPECT_EQ(1, col_3->totkey);
  EXPECT_EQ(1, col_5->totkey);

  ED_keylist_free(keylist);
}

TEST_F(KeylistSummaryTest, slot_summary_bone_selection)
{
  /* Test that a key summary is generated correctly, excluding keys for
   * unselected bones when filter-by-selection is on. */

  using namespace blender::animrig;

  Slot &slot_armature = action->slot_add_for_id(armature->id);
  ASSERT_EQ(ActionSlotAssignmentResult::OK,
            assign_action_and_slot(action, &slot_armature, armature->id));
  Channelbag &channelbag = action_channelbag_ensure(*action, armature->id);

  FCurve &bone1_loc_x = channelbag.fcurve_ensure(
      bmain, {"pose.bones[\"Bone.001\"].location", 0, {}, {}, "Bone.001"});
  FCurve &bone2_loc_x = channelbag.fcurve_ensure(
      bmain, {"pose.bones[\"Bone.002\"].location", 0, {}, {}, "Bone.002"});

  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&bone1_loc_x, {1.0, 0.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&bone1_loc_x, {2.0, 1.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&bone2_loc_x, {2.0, 2.0}, {}, {}));
  ASSERT_EQ(SingleKeyingResult::SUCCESS, insert_vert_fcurve(&bone2_loc_x, {3.0, 3.0}, {}, {}));

  /* Select only Bone.001. */
  bPoseChannel *pose_bone1 = BKE_pose_channel_find_name(armature->pose, bone1->name);
  ASSERT_NE(pose_bone1, nullptr);
  pose_bone1->flag |= POSE_SELECTED;
  bPoseChannel *pose_bone2 = BKE_pose_channel_find_name(armature->pose, bone2->name);
  pose_bone2->flag &= ~POSE_SELECTED;

  /* Generate slot summary keylist. */
  AnimKeylist *keylist = ED_keylist_create();
  saction.ads.filterflag = ADS_FILTER_ONLYSEL; /* Filter by selection. */
  ac.obact = armature;
  ac.active_action_user = &armature->id;
  ac.filters.flag = eDopeSheet_FilterFlag(saction.ads.filterflag);
  action_slot_summary_to_keylist(
      &ac, &armature->id, *action, slot_armature.handle, keylist, 0, {0.0, 6.0});
  ED_keylist_prepare_for_direct_access(keylist);

  const ActKeyColumn *col_1 = ED_keylist_find_exact(keylist, 1.0);
  const ActKeyColumn *col_2 = ED_keylist_find_exact(keylist, 2.0);
  const ActKeyColumn *col_3 = ED_keylist_find_exact(keylist, 3.0);

  /* Check that we only have columns at the frames with keys for Bone.001. */
  EXPECT_NE(nullptr, col_1);
  EXPECT_NE(nullptr, col_2);
  EXPECT_EQ(nullptr, col_3);

  /* Check that the right number of keys are indicated in each column. */
  EXPECT_EQ(1, col_1->totkey);
  EXPECT_EQ(1, col_2->totkey);

  ED_keylist_free(keylist);
}

}  // namespace blender::editor::animation::tests
