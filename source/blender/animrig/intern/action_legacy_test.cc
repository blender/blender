/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

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
    return static_cast<bAction *>(BKE_id_new(bmain, ID_AC, "ACAction"));
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

    FCurve *fcurve = static_cast<FCurve *>(MEM_callocN(sizeof(FCurve), __func__));
    BLI_addtail(&action->curves, fcurve);

    Vector<FCurve *> fcurves_expect = {fcurve};
    EXPECT_EQ(fcurves_expect, legacy::fcurves_all(action));
  }
}

#ifdef WITH_ANIM_BAKLAVA
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
#endif /* WITH_ANIM_BAKLAVA */

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

    FCurve *fcurve = static_cast<FCurve *>(MEM_callocN(sizeof(FCurve), __func__));
    BLI_addtail(&action->curves, fcurve);

    Vector<FCurve *> fcurves_expect = {fcurve};
    EXPECT_EQ(fcurves_expect, legacy::fcurves_for_action_slot(action, Slot::unassigned));
  }
}

#ifdef WITH_ANIM_BAKLAVA
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
#endif /* WITH_ANIM_BAKLAVA */

}  // namespace blender::animrig::tests
