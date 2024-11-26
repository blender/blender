/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"
#include "ANIM_nla.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_nla.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include <limits>

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::nla::tests {

class NLASlottedActionTest : public testing::Test {
 public:
  Main *bmain;
  Action *action;
  Object *cube;

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
    action = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACÄnimåtië"));
    action->id.us = 0; /* Nothing references this yet. */
    cube = BKE_object_add_only_object(bmain, OB_EMPTY, "Küüübus");
    cube->id.us = 0; /* Nothing references this yet. */
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(NLASlottedActionTest, assign_slot_to_nla_strip)
{
  ASSERT_EQ(action->id.us, 0);

  AnimData *adt = BKE_animdata_ensure_id(&cube->id);
  NlaTrack *track = BKE_nlatrack_new_tail(&adt->nla_tracks, false);

  /* Create a strip. This automatically assigns the Action, but for now with the old flow. */
  NlaStrip *strip = BKE_nlastrip_new(action, cube->id);
  BKE_nlatrack_add_strip(track, strip, false);

  EXPECT_EQ(strip->action_slot_handle, Slot::unassigned);
  EXPECT_STREQ(strip->action_slot_name, "");

  /* Unassign the Action that was automatically assigned via BKE_nlastrip_new(). */
  nla::unassign_action(*strip, cube->id);
  EXPECT_EQ(strip->act, nullptr);
  EXPECT_EQ(action->id.us, 0);

  /* Assign an Action with a never-assigned slot. This should be picked automatically. */
  Slot &virgin_slot = action->slot_add();

  /* Assign the Action. */
  EXPECT_TRUE(nla::assign_action(*strip, *action, cube->id));
  EXPECT_EQ(strip->action_slot_handle, virgin_slot.handle);
  EXPECT_STREQ(strip->action_slot_name, virgin_slot.identifier);
  EXPECT_EQ(action->id.us, 1);
  EXPECT_EQ(strip->act, action);
  EXPECT_EQ(virgin_slot.idtype, GS(cube->id.name));

  /* Unassign the Action. */
  nla::unassign_action(*strip, cube->id);
  EXPECT_EQ(strip->act, nullptr);
  EXPECT_EQ(action->id.us, 0);

  /* Create a slot for this ID. Assigning the Action should auto-pick it. */
  Slot &slot = action->slot_add_for_id(cube->id);
  EXPECT_TRUE(nla::assign_action(*strip, *action, cube->id));
  EXPECT_EQ(strip->action_slot_handle, slot.handle);
  EXPECT_STREQ(strip->action_slot_name, slot.identifier);
  EXPECT_EQ(action->id.us, 1);
  EXPECT_EQ(strip->act, action);
  EXPECT_TRUE(slot.runtime_users().contains(&cube->id));

  /* Unassign the slot, but keep the Action assigned. */
  EXPECT_EQ(nla::assign_action_slot(*strip, nullptr, cube->id), ActionSlotAssignmentResult::OK);
  EXPECT_EQ(strip->action_slot_handle, Slot::unassigned);
  EXPECT_STREQ(strip->action_slot_name, slot.identifier);
  EXPECT_EQ(action->id.us, 1);
  EXPECT_EQ(strip->act, action);
  EXPECT_FALSE(slot.runtime_users().contains(&cube->id));

  /* Unassign the Action, then reassign it. It should pick the same slot again. */
  nla::unassign_action(*strip, cube->id);
  EXPECT_TRUE(nla::assign_action(*strip, *action, cube->id));
  EXPECT_EQ(strip->action_slot_handle, slot.handle);
  EXPECT_TRUE(slot.runtime_users().contains(&cube->id));
}

TEST_F(NLASlottedActionTest, assign_slot_to_multiple_strips)
{
  AnimData *adt = BKE_animdata_ensure_id(&cube->id);
  NlaTrack *track = BKE_nlatrack_new_tail(&adt->nla_tracks, false);

  /* Create two strips. This automatically assigns the Action, but for now with
   * the old flow (so no slots). */
  NlaStrip *strip1 = BKE_nlastrip_new(action, cube->id);
  strip1->start = 1;
  strip1->end = 4;
  NlaStrip *strip2 = BKE_nlastrip_new(action, cube->id);
  strip1->start = 47;
  strip1->end = 327;
  ASSERT_TRUE(BKE_nlatrack_add_strip(track, strip1, false));
  ASSERT_TRUE(BKE_nlatrack_add_strip(track, strip2, false));
  ASSERT_EQ(1, BLI_listbase_count(&adt->nla_tracks));
  ASSERT_EQ(2, BLI_listbase_count(&track->strips));

  nla::unassign_action(*strip1, cube->id);
  nla::unassign_action(*strip2, cube->id);

  /* Create a virgin slot, it should be auto-picked. */
  Slot &slot = action->slot_add();
  EXPECT_TRUE(nla::assign_action(*strip1, *action, cube->id));
  EXPECT_EQ(strip1->action_slot_handle, slot.handle);
  EXPECT_STREQ(strip1->action_slot_name, slot.identifier);
  EXPECT_EQ(slot.idtype, ID_OB);

  /* Assign another slot slot 'manually'. */
  Slot &other_slot = action->slot_add();
  EXPECT_EQ(nla::assign_action_slot(*strip1, &other_slot, cube->id),
            ActionSlotAssignmentResult::OK);
  EXPECT_EQ(strip1->action_slot_handle, other_slot.handle);

  /* Assign the Action + slot to the second strip.*/
  EXPECT_TRUE(nla::assign_action(*strip2, *action, cube->id));
  EXPECT_EQ(nla::assign_action_slot(*strip2, &slot, cube->id), ActionSlotAssignmentResult::OK);

  /* The cube should be registered as user of the slot. */
  EXPECT_TRUE(slot.runtime_users().contains(&cube->id));

  nla::unassign_action(*strip1, cube->id);

  /* The cube should still be registered as user of the slot, as there is a 2nd
   * strip that references it. */
  EXPECT_TRUE(slot.runtime_users().contains(&cube->id));

  /* Remove the last use of this slot. */
  nla::unassign_action(*strip2, cube->id);
  EXPECT_FALSE(slot.runtime_users().contains(&cube->id));
}

}  // namespace blender::animrig::nla::tests
