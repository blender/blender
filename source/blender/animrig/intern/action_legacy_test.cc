/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"

#include "BLI_listbase.h"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::tests {
class ActionLegacyTest : public testing::Test {
 public:
  Main *bmain;

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
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }

  bAction *create_empty_action()
  {
    return BKE_id_new<bAction>(bmain, "ACAction");
  }

  FCurve *fcurve_add_legacy(bAction *action, const StringRefNull rna_path, const int array_index)
  {
    FCurve *fcurve = MEM_callocN<FCurve>(__func__);
    BKE_fcurve_rnapath_set(*fcurve, rna_path);
    fcurve->array_index = array_index;
    BLI_addtail(&action->curves, fcurve);
    return fcurve;
  }
};

TEST_F(ActionLegacyTest, fcurves_all)
{
  { /* nil pointer. */
    bAction *action = nullptr;
    Vector<FCurve *> fcurves = legacy::fcurves_all(action);
    EXPECT_TRUE(fcurves.is_empty());
  }

  { /* Empty Action. */
    Vector<FCurve *> fcurves = legacy::fcurves_all(create_empty_action());
    EXPECT_TRUE(fcurves.is_empty());
  }

  { /* Legacy Action. */
    bAction *action = create_empty_action();

    FCurve *fcurve = MEM_callocN<FCurve>(__func__);
    BLI_addtail(&action->curves, fcurve);

    Vector<FCurve *> fcurves_expect = {fcurve};
    EXPECT_EQ(fcurves_expect, legacy::fcurves_all(action));
  }
}

TEST_F(ActionLegacyTest, fcurves_all_layered)
{
  Action &action = create_empty_action()->wrap();
  Slot &slot1 = action.slot_add();
  Slot &slot2 = action.slot_add();

  action.layer_keystrip_ensure();
  StripKeyframeData &key_data = action.layer(0)->strip(0)->data<StripKeyframeData>(action);

  FCurve &fcurve1 = key_data.channelbag_for_slot_ensure(slot1).fcurve_ensure(bmain,
                                                                             {"location", 1});
  FCurve &fcurve2 = key_data.channelbag_for_slot_ensure(slot2).fcurve_ensure(bmain, {"scale", 2});

  Vector<FCurve *> fcurves_expect = {&fcurve1, &fcurve2};
  EXPECT_EQ(fcurves_expect, legacy::fcurves_all(&action));
}

TEST_F(ActionLegacyTest, fcurves_for_action_slot)
{
  { /* nil pointer. */
    bAction *action = nullptr;
    Vector<FCurve *> fcurves = legacy::fcurves_for_action_slot(action, Slot::unassigned);
    EXPECT_TRUE(fcurves.is_empty());
  }

  { /* Empty Action. */
    Vector<FCurve *> fcurves = legacy::fcurves_for_action_slot(create_empty_action(),
                                                               Slot::unassigned);
    EXPECT_TRUE(fcurves.is_empty());
  }

  { /* Legacy Action. */
    bAction *action = create_empty_action();

    FCurve *fcurve = MEM_callocN<FCurve>(__func__);
    BLI_addtail(&action->curves, fcurve);

    Vector<FCurve *> fcurves_expect = {fcurve};
    EXPECT_EQ(fcurves_expect, legacy::fcurves_for_action_slot(action, Slot::unassigned));
  }
}

TEST_F(ActionLegacyTest, fcurves_for_action_slot_layered)
{
  Action &action = create_empty_action()->wrap();
  Slot &slot1 = action.slot_add();
  Slot &slot2 = action.slot_add();

  action.layer_keystrip_ensure();
  StripKeyframeData &key_data = action.layer(0)->strip(0)->data<StripKeyframeData>(action);

  FCurve &fcurve1 = key_data.channelbag_for_slot_ensure(slot1).fcurve_ensure(bmain,
                                                                             {"location", 1});
  FCurve &fcurve2 = key_data.channelbag_for_slot_ensure(slot2).fcurve_ensure(bmain, {"scale", 2});

  Vector<FCurve *> fcurve1_expect = {&fcurve1};
  Vector<FCurve *> fcurve2_expect = {&fcurve2};
  EXPECT_EQ(fcurve1_expect, legacy::fcurves_for_action_slot(&action, slot1.handle));
  EXPECT_EQ(fcurve2_expect, legacy::fcurves_for_action_slot(&action, slot2.handle));
}

TEST_F(ActionLegacyTest, action_fcurves_remove_legacy)
{
  { /* Empty Action. */
    bAction *action = create_empty_action();
    EXPECT_FALSE(legacy::action_fcurves_remove(*action, Slot::unassigned, "rotation"));
  }

  { /* Legacy Action. */
    bAction *action = create_empty_action();
    FCurve *fcurve_loc_x = fcurve_add_legacy(action, "location", 0);
    fcurve_add_legacy(action, "rotation_euler", 2);
    fcurve_add_legacy(action, "rotation_mode", 0);
    FCurve *fcurve_loc_y = fcurve_add_legacy(action, "location", 1);

    EXPECT_TRUE(legacy::action_fcurves_remove(*action, Slot::unassigned, "rotation"));
    Vector<FCurve *> fcurves_expect = {fcurve_loc_x, fcurve_loc_y};
    EXPECT_EQ(fcurves_expect, legacy::fcurves_all(action));
  }
}

TEST_F(ActionLegacyTest, action_fcurves_remove_layered)
{
  /* Create an Action with two slots, to check that the 2nd slot is not affected
   * by removal from the 1st. */
  Action &action = create_empty_action()->wrap();
  Slot &slot_1 = action.slot_add();
  Slot &slot_2 = action.slot_add();

  action.layer_keystrip_ensure();
  StripKeyframeData *strip_data = action.strip_keyframe_data()[0];
  Channelbag &bag_1 = strip_data->channelbag_for_slot_ensure(slot_1);
  Channelbag &bag_2 = strip_data->channelbag_for_slot_ensure(slot_2);

  /* Add some F-Curves to each channelbag. */
  FCurve &fcurve_loc_x = bag_1.fcurve_ensure(nullptr, {"location", 0});
  bag_1.fcurve_ensure(nullptr, {"rotation_euler", 2});
  bag_1.fcurve_ensure(nullptr, {"rotation_mode", 0});
  FCurve &fcurve_loc_y = bag_1.fcurve_ensure(nullptr, {"location", 1});

  bag_2.fcurve_ensure(nullptr, {"location", 0});
  bag_2.fcurve_ensure(nullptr, {"rotation_euler", 2});
  bag_2.fcurve_ensure(nullptr, {"rotation_mode", 0});
  bag_2.fcurve_ensure(nullptr, {"location", 1});

  /* Check that removing from slot_1 works as expected. */
  EXPECT_TRUE(legacy::action_fcurves_remove(action, slot_1.handle, "rotation"));

  Vector<FCurve *> fcurves_bag_1_expect = {&fcurve_loc_x, &fcurve_loc_y};
  EXPECT_EQ(fcurves_bag_1_expect, legacy::fcurves_for_action_slot(&action, slot_1.handle));

  EXPECT_EQ(4, bag_2.fcurves().size())
      << "Expected all F-Curves for slot 2 to be there after manipulating slot 1";
}

}  // namespace blender::animrig::tests
