/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::tests {
class ActionIteratorsTest : public testing::Test {
 public:
  Main *bmain;
  Action *action;

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
    action = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACLayeredAction"));
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(ActionIteratorsTest, iterate_all_fcurves_of_slot)
{
  Slot &cube_slot = action->slot_add();
  Slot &monkey_slot = action->slot_add();
  EXPECT_TRUE(action->is_action_layered());

  /* Try iterating an empty action. */
  blender::Vector<FCurve *> no_fcurves;
  action_foreach_fcurve(
      *action, cube_slot.handle, [&](FCurve &fcurve) { no_fcurves.append(&fcurve); });

  ASSERT_TRUE(no_fcurves.is_empty());

  Layer &layer = action->layer_add("Layer One");
  Strip &strip = layer.strip_add(Strip::Type::Keyframe);
  KeyframeStrip &key_strip = strip.as<KeyframeStrip>();
  const KeyframeSettings settings = get_keyframe_settings(false);

  /* Insert 3 FCurves for each slot. */
  for (int i = 0; i < 3; i++) {
    SingleKeyingResult result_cube = key_strip.keyframe_insert(
        bmain, cube_slot, {"location", i}, {1.0f, 0.0f}, settings);
    ASSERT_EQ(SingleKeyingResult::SUCCESS, result_cube)
        << "Expected keyframe insertion to be successful";

    SingleKeyingResult result_monkey = key_strip.keyframe_insert(
        bmain, monkey_slot, {"rotation", i}, {1.0f, 0.0f}, settings);
    ASSERT_EQ(SingleKeyingResult::SUCCESS, result_monkey)
        << "Expected keyframe insertion to be successful";
  }

  /* Get all FCurves. */
  blender::Vector<FCurve *> cube_fcurves;
  action_foreach_fcurve(
      *action, cube_slot.handle, [&](FCurve &fcurve) { cube_fcurves.append(&fcurve); });

  ASSERT_EQ(cube_fcurves.size(), 3);
  for (FCurve *fcurve : cube_fcurves) {
    ASSERT_STREQ(fcurve->rna_path, "location");
  }

  /* Get only FCurves with index 0 which should be 1. */
  blender::Vector<FCurve *> monkey_fcurves;
  action_foreach_fcurve(*action, monkey_slot.handle, [&](FCurve &fcurve) {
    if (fcurve.array_index == 0) {
      monkey_fcurves.append(&fcurve);
    }
  });

  ASSERT_EQ(monkey_fcurves.size(), 1);
  ASSERT_STREQ(monkey_fcurves[0]->rna_path, "rotation");

  /* Slots handles are just numbers. Passing in a slot handle that doesn't exist should return
   * nothing. */
  blender::Vector<FCurve *> invalid_slot_fcurves;
  action_foreach_fcurve(*action, monkey_slot.handle + cube_slot.handle, [&](FCurve &fcurve) {
    invalid_slot_fcurves.append(&fcurve);
  });
  ASSERT_TRUE(invalid_slot_fcurves.is_empty());
}
}  // namespace blender::animrig::tests
