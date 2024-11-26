/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_action.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include <limits>

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::animrig::tests {
class ActionLayersTest : public testing::Test {
 public:
  Main *bmain;
  Action *action;
  Object *cube;
  Object *suzanne;
  Object *bob;

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
    cube = BKE_object_add_only_object(bmain, OB_EMPTY, "Küüübus");
    suzanne = BKE_object_add_only_object(bmain, OB_EMPTY, "OBSuzanne");
    bob = BKE_object_add_only_object(bmain, OB_EMPTY, "OBBob");
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(ActionLayersTest, add_layer)
{
  Layer &layer = action->layer_add("layer name");

  EXPECT_EQ(action->layer(0), &layer);
  EXPECT_EQ("layer name", std::string(layer.name));
  EXPECT_EQ(1.0f, layer.influence) << "Expected DNA defaults to be used.";
  EXPECT_EQ(0, action->layer_active_index)
      << "Expected newly added layer to become the active layer.";
  ASSERT_EQ(0, layer.strips().size()) << "Expected newly added layer to have no strip.";
}

TEST_F(ActionLayersTest, add_layer__reset_idroot)
{
  /* An empty Action is a valid legacy Action, and thus can have its idroot set to a non-zero
   * value. If such an Action gets a layer, it no longer is a valid legacy Action, and thus its
   * idtype should be reset to zero. */
  action->idroot = ID_CA; /* Fake that this was assigned to a camera data-block. */
  ASSERT_NE(0, action->idroot) << "action->idroot should not be zero at the start of this test.";

  action->layer_add("layer name");

  EXPECT_EQ(0, action->idroot)
      << "action->idroot should get reset when the Action becomes layered.";
}

TEST_F(ActionLayersTest, remove_layer)
{
  Layer &layer0 = action->layer_add("Test Læür nul");
  Layer &layer1 = action->layer_add("Test Læür één");
  Layer &layer2 = action->layer_add("Test Læür twee");

  /* Add some strips to check that they are freed correctly too (implicitly by the
   * memory leak checker). */
  layer0.strip_add(*action, Strip::Type::Keyframe);
  layer1.strip_add(*action, Strip::Type::Keyframe);
  layer2.strip_add(*action, Strip::Type::Keyframe);

  { /* Test removing a layer that is not owned. */
    Action *other_anim = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACOtherAnim"));
    Layer &other_layer = other_anim->layer_add("Another Layer");
    EXPECT_FALSE(action->layer_remove(other_layer))
        << "Removing a layer not owned by the Action should be gracefully rejected";
    BKE_id_free(bmain, &other_anim->id);
  }

  EXPECT_TRUE(action->layer_remove(layer1));
  EXPECT_EQ(2, action->layers().size());
  EXPECT_STREQ(layer0.name, action->layer(0)->name);
  EXPECT_STREQ(layer2.name, action->layer(1)->name);

  EXPECT_TRUE(action->layer_remove(layer2));
  EXPECT_EQ(1, action->layers().size());
  EXPECT_STREQ(layer0.name, action->layer(0)->name);

  EXPECT_TRUE(action->layer_remove(layer0));
  EXPECT_EQ(0, action->layers().size());
}

TEST_F(ActionLayersTest, add_strip)
{
  Layer &layer = action->layer_add("Test Læür");

  Strip &strip = layer.strip_add(*action, Strip::Type::Keyframe);
  ASSERT_EQ(1, layer.strips().size());
  EXPECT_EQ(&strip, layer.strip(0));

  constexpr float inf = std::numeric_limits<float>::infinity();
  EXPECT_EQ(-inf, strip.frame_start) << "Expected strip to be infinite.";
  EXPECT_EQ(inf, strip.frame_end) << "Expected strip to be infinite.";
  EXPECT_EQ(0, strip.frame_offset) << "Expected infinite strip to have no offset.";

  Strip &another_strip = layer.strip_add(*action, Strip::Type::Keyframe);
  ASSERT_EQ(2, layer.strips().size());
  EXPECT_EQ(&another_strip, layer.strip(1));

  EXPECT_EQ(-inf, another_strip.frame_start) << "Expected strip to be infinite.";
  EXPECT_EQ(inf, another_strip.frame_end) << "Expected strip to be infinite.";
  EXPECT_EQ(0, another_strip.frame_offset) << "Expected infinite strip to have no offset.";

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Slot &slot = action->slot_add();
  strip.data<StripKeyframeData>(*action).keyframe_insert(
      bmain, slot, {"location", 0}, {1.0f, 47.0f}, settings);
  another_strip.data<StripKeyframeData>(*action).keyframe_insert(
      bmain, slot, {"location", 0}, {1.0f, 47.0f}, settings);
}

TEST_F(ActionLayersTest, remove_strip)
{
  Layer &layer = action->layer_add("Test Læür");
  Strip &strip0 = layer.strip_add(*action, Strip::Type::Keyframe);
  Strip &strip1 = layer.strip_add(*action, Strip::Type::Keyframe);
  Strip &strip2 = layer.strip_add(*action, Strip::Type::Keyframe);
  Strip &strip3 = layer.strip_add(*action, Strip::Type::Keyframe);
  StripKeyframeData &strip_data0 = strip0.data<StripKeyframeData>(*action);
  StripKeyframeData &strip_data1 = strip1.data<StripKeyframeData>(*action);
  StripKeyframeData &strip_data2 = strip2.data<StripKeyframeData>(*action);
  StripKeyframeData &strip_data3 = strip3.data<StripKeyframeData>(*action);

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Slot &slot = action->slot_add();
  strip_data0.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 47.0f}, settings);
  strip_data1.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 48.0f}, settings);
  strip_data2.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 49.0f}, settings);
  strip_data3.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 50.0f}, settings);

  EXPECT_EQ(4, action->strip_keyframe_data().size());
  EXPECT_EQ(0, strip0.data_index);
  EXPECT_EQ(1, strip1.data_index);
  EXPECT_EQ(2, strip2.data_index);
  EXPECT_EQ(3, strip3.data_index);

  EXPECT_TRUE(layer.strip_remove(*action, strip1));
  EXPECT_EQ(3, action->strip_keyframe_data().size());
  EXPECT_EQ(3, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));
  EXPECT_EQ(&strip2, layer.strip(1));
  EXPECT_EQ(&strip3, layer.strip(2));
  EXPECT_EQ(0, strip0.data_index);
  EXPECT_EQ(2, strip2.data_index);
  EXPECT_EQ(1, strip3.data_index); /* Swapped in when removing strip 1's data. */
  EXPECT_EQ(&strip_data0, &strip0.data<StripKeyframeData>(*action));
  EXPECT_EQ(&strip_data2, &strip2.data<StripKeyframeData>(*action));
  EXPECT_EQ(&strip_data3, &strip3.data<StripKeyframeData>(*action));

  EXPECT_TRUE(layer.strip_remove(*action, strip2));
  EXPECT_EQ(2, action->strip_keyframe_data().size());
  EXPECT_EQ(2, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));
  EXPECT_EQ(&strip3, layer.strip(1));
  EXPECT_EQ(0, strip0.data_index);
  EXPECT_EQ(1, strip3.data_index);
  EXPECT_EQ(&strip_data0, &strip0.data<StripKeyframeData>(*action));
  EXPECT_EQ(&strip_data3, &strip3.data<StripKeyframeData>(*action));

  EXPECT_TRUE(layer.strip_remove(*action, strip3));
  EXPECT_EQ(1, action->strip_keyframe_data().size());
  EXPECT_EQ(1, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));
  EXPECT_EQ(0, strip0.data_index);
  EXPECT_EQ(&strip_data0, &strip0.data<StripKeyframeData>(*action));

  EXPECT_TRUE(layer.strip_remove(*action, strip0));
  EXPECT_EQ(0, action->strip_keyframe_data().size());
  EXPECT_EQ(0, layer.strips().size());

  { /* Test removing a strip that is not owned. */
    Layer &other_layer = action->layer_add("Another Layer");
    Strip &other_strip = other_layer.strip_add(*action, Strip::Type::Keyframe);

    EXPECT_FALSE(layer.strip_remove(*action, other_strip))
        << "Removing a strip not owned by the layer should be gracefully rejected";
  }
}

/* NOTE: this test creates strip instances in a bespoke way for the purpose of
 * exercising the strip removal code, because at the time of writing we don't
 * have a proper API for creating strip instances. When such an API is added,
 * this test should be updated to use it. */
TEST_F(ActionLayersTest, remove_strip_instances)
{
  Layer &layer = action->layer_add("Test Læür");
  Strip &strip0 = layer.strip_add(*action, Strip::Type::Keyframe);
  Strip &strip1 = layer.strip_add(*action, Strip::Type::Keyframe);
  Strip &strip2 = layer.strip_add(*action, Strip::Type::Keyframe);

  /* Make on of the strips an instance of another. */
  strip0.data_index = strip1.data_index;

  StripKeyframeData &strip_data_0_1 = strip0.data<StripKeyframeData>(*action);
  StripKeyframeData &strip_data_2 = strip2.data<StripKeyframeData>(*action);

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Slot &slot = action->slot_add();
  strip_data_0_1.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 47.0f}, settings);
  strip_data_2.keyframe_insert(bmain, slot, {"location", 0}, {1.0f, 48.0f}, settings);

  EXPECT_EQ(3, action->strip_keyframe_data().size());
  EXPECT_EQ(1, strip0.data_index);
  EXPECT_EQ(1, strip1.data_index);
  EXPECT_EQ(2, strip2.data_index);

  /* Removing an instance should not delete the underlying data as long as there
   * is still another strip using it. */
  EXPECT_TRUE(layer.strip_remove(*action, strip1));
  EXPECT_EQ(3, action->strip_keyframe_data().size());
  EXPECT_EQ(2, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));
  EXPECT_EQ(&strip2, layer.strip(1));
  EXPECT_EQ(1, strip0.data_index);
  EXPECT_EQ(2, strip2.data_index);
  EXPECT_EQ(&strip_data_0_1, &strip0.data<StripKeyframeData>(*action));
  EXPECT_EQ(&strip_data_2, &strip2.data<StripKeyframeData>(*action));

  /* Removing the last user of strip data should also delete the data. */
  EXPECT_TRUE(layer.strip_remove(*action, strip0));
  EXPECT_EQ(2, action->strip_keyframe_data().size());
  EXPECT_EQ(1, layer.strips().size());
  EXPECT_EQ(&strip2, layer.strip(0));
  EXPECT_EQ(1, strip2.data_index);
  EXPECT_EQ(&strip_data_2, &strip2.data<StripKeyframeData>(*action));
}

TEST_F(ActionLayersTest, add_slot)
{
  { /* Creating an 'unused' Slot should just be called 'Slot'. */
    Slot &slot = action->slot_add();
    EXPECT_EQ(1, action->last_slot_handle);
    EXPECT_EQ(1, slot.handle);

    EXPECT_STREQ("XXSlot", slot.identifier);
    EXPECT_EQ(0, slot.idtype);
  }

  { /* Creating a Slot for a specific ID should name it after the ID. */
    Slot &slot = action->slot_add_for_id(cube->id);
    EXPECT_EQ(2, action->last_slot_handle);
    EXPECT_EQ(2, slot.handle);

    EXPECT_STREQ(cube->id.name, slot.identifier);
    EXPECT_EQ(ID_OB, slot.idtype);
  }
}

TEST_F(ActionLayersTest, add_slot__reset_idroot)
{
  /* An empty Action is a valid legacy Action, and thus can have its idroot set
   * to a non-zero value. If such an Action gets a slot, it no longer is a
   * valid legacy Action, and thus its idtype should be reset to zero. */
  action->idroot = ID_CA; /* Fake that this was assigned to a camera data-block. */
  ASSERT_NE(0, action->idroot) << "action->idroot should not be zero at the start of this test.";

  action->slot_add();

  EXPECT_EQ(0, action->idroot)
      << "action->idroot should get reset when the Action becomes layered.";
}

TEST_F(ActionLayersTest, add_slot_multiple)
{
  Slot &slot_cube = action->slot_add();
  Slot &slot_suzanne = action->slot_add();
  EXPECT_TRUE(assign_action(action, cube->id));
  EXPECT_EQ(assign_action_slot(&slot_cube, cube->id), ActionSlotAssignmentResult::OK);
  EXPECT_TRUE(assign_action(action, suzanne->id));
  EXPECT_EQ(assign_action_slot(&slot_suzanne, suzanne->id), ActionSlotAssignmentResult::OK);

  EXPECT_EQ(2, action->last_slot_handle);
  EXPECT_EQ(1, slot_cube.handle);
  EXPECT_EQ(2, slot_suzanne.handle);
}

TEST_F(ActionLayersTest, slot_remove)
{
  { /* Canary test: removing a just-created slot on an otherwise empty Action should work. */
    Slot &slot = action->slot_add();
    EXPECT_EQ(1, slot.handle);
    EXPECT_EQ(1, action->last_slot_handle);

    EXPECT_TRUE(action->slot_remove(slot));
    EXPECT_EQ(1, action->last_slot_handle)
        << "Removing a slot should not change the last-used slot handle.";
    EXPECT_EQ(0, action->slot_array_num);
  }

  { /* Removing a non-existing slot should return false. */
    Slot slot;
    EXPECT_FALSE(action->slot_remove(slot));
  }

  { /* Removing a slot should remove its ChannelBag. */
    Slot &slot = action->slot_add();
    const slot_handle_t slot_handle = slot.handle;
    EXPECT_EQ(2, slot.handle);
    EXPECT_EQ(2, action->last_slot_handle);

    /* Create an F-Curve in a ChannelBag for the slot. */
    action->layer_keystrip_ensure();
    StripKeyframeData &strip_data = action->layer(0)->strip(0)->data<StripKeyframeData>(*action);
    ChannelBag &channelbag = strip_data.channelbag_for_slot_ensure(slot);
    channelbag.fcurve_create_unique(bmain, {"location", 1});

    /* Remove the slot. */
    EXPECT_TRUE(action->slot_remove(slot));
    EXPECT_EQ(2, action->last_slot_handle)
        << "Removing a slot should not change the last-used slot handle.";
    EXPECT_EQ(0, action->slot_array_num);

    /* Check that its channelbag is gone. */
    ChannelBag *found_cbag = strip_data.channelbag_for_slot(slot_handle);
    EXPECT_EQ(found_cbag, nullptr);
  }

  { /* Removing one slot should leave the other two in place. */
    Slot &slot1 = action->slot_add();
    Slot &slot2 = action->slot_add();
    Slot &slot3 = action->slot_add();
    EXPECT_EQ(3, slot1.handle);
    EXPECT_EQ(4, slot2.handle);
    EXPECT_EQ(5, slot3.handle);
    EXPECT_EQ(5, action->last_slot_handle);

    /* For referencing the slot handle after the slot is removed. */
    const slot_handle_t slot2_handle = slot2.handle;

    /* Create a Channel-bag for each slot. */
    action->layer_keystrip_ensure();
    StripKeyframeData &strip_data = action->layer(0)->strip(0)->data<StripKeyframeData>(*action);
    strip_data.channelbag_for_slot_ensure(slot1);
    strip_data.channelbag_for_slot_ensure(slot2);
    strip_data.channelbag_for_slot_ensure(slot3);

    /* Remove the slot. */
    EXPECT_TRUE(action->slot_remove(slot2));
    EXPECT_EQ(5, action->last_slot_handle);

    /* Check the correct slot + channel-bag are removed. */
    EXPECT_EQ(action->slot_for_handle(slot1.handle), &slot1);
    EXPECT_EQ(action->slot_for_handle(slot2_handle), nullptr);
    EXPECT_EQ(action->slot_for_handle(slot3.handle), &slot3);

    EXPECT_NE(strip_data.channelbag_for_slot(slot1.handle), nullptr);
    EXPECT_EQ(strip_data.channelbag_for_slot(slot2_handle), nullptr);
    EXPECT_NE(strip_data.channelbag_for_slot(slot3.handle), nullptr);
  }

  { /* Removing an in-use slot doesn't un-assign it from its users.
     * This is not that important, but it covers the current behavior. */
    Slot &slot = action->slot_add_for_id(cube->id);
    ASSERT_EQ(assign_action_and_slot(action, &slot, cube->id), ActionSlotAssignmentResult::OK);

    ASSERT_TRUE(slot.runtime_users().contains(&cube->id));
    ASSERT_EQ(cube->adt->slot_handle, slot.handle);

    const slot_handle_t removed_slot_handle = slot.handle;
    ASSERT_TRUE(action->slot_remove(slot));
    EXPECT_EQ(cube->adt->slot_handle, removed_slot_handle);
  }

  { /* Creating a slot after removing one should not reuse its handle. */
    action->last_slot_handle = 3; /* To create independence between sub-tests. */
    Slot &slot1 = action->slot_add();
    ASSERT_EQ(4, slot1.handle);
    ASSERT_EQ(4, action->last_slot_handle);
    ASSERT_TRUE(action->slot_remove(slot1));

    Slot &slot2 = action->slot_add();
    EXPECT_EQ(5, slot2.handle);
    EXPECT_EQ(5, action->last_slot_handle);
  }
}

TEST_F(ActionLayersTest, action_assign_id)
{
  /* Assign to the only, 'virgin' Slot, should always work. */
  Slot &slot_cube = action->slot_add();
  ASSERT_NE(nullptr, slot_cube.runtime);
  ASSERT_STREQ(slot_cube.identifier, "XXSlot");
  ASSERT_EQ(assign_action_and_slot(action, &slot_cube, cube->id), ActionSlotAssignmentResult::OK);

  EXPECT_EQ(slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ(slot_cube.identifier, "OBSlot");
  EXPECT_STREQ(slot_cube.identifier, cube->adt->last_slot_identifier)
      << "The slot identifier should be copied to the adt";

  EXPECT_TRUE(slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Cube to be registered as animated by its slot.";

  /* Assign another ID to the same Slot. */
  ASSERT_EQ(assign_action_and_slot(action, &slot_cube, suzanne->id),
            ActionSlotAssignmentResult::OK);
  EXPECT_STREQ(slot_cube.identifier, "OBSlot");
  EXPECT_STREQ(slot_cube.identifier, cube->adt->last_slot_identifier)
      << "The slot identifier should be copied to the adt";

  EXPECT_TRUE(slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Suzanne to be registered as animated by the Cube slot.";

  { /* Assign Cube to another action+slot without unassigning first. */
    Action *another_anim = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACOtherAnim"));
    Slot &another_slot = another_anim->slot_add();
    ASSERT_EQ(assign_action_and_slot(another_anim, &another_slot, cube->id),
              ActionSlotAssignmentResult::OK);
    EXPECT_FALSE(slot_cube.users(*bmain).contains(&cube->id))
        << "Expecting Cube to no longer be registered as user of its old slot.";
    EXPECT_TRUE(another_slot.users(*bmain).contains(&cube->id))
        << "Expecting Cube to be registered as user of its new slot.";
  }

  { /* Assign Cube to another slot of the same Action, this should work. */
    ASSERT_EQ(assign_action_and_slot(action, &slot_cube, cube->id),
              ActionSlotAssignmentResult::OK);
    const int user_count_pre = action->id.us;
    Slot &slot_cube_2 = action->slot_add();
    ASSERT_EQ(assign_action_and_slot(action, &slot_cube_2, cube->id),
              ActionSlotAssignmentResult::OK);
    ASSERT_EQ(action->id.us, user_count_pre)
        << "Assigning to a different slot of the same Action should _not_ change the user "
           "count of that Action";
    EXPECT_FALSE(slot_cube.users(*bmain).contains(&cube->id))
        << "Expecting Cube to no longer be registered as animated by the Cube slot.";
    EXPECT_TRUE(slot_cube_2.users(*bmain).contains(&cube->id))
        << "Expecting Cube to be registered as animated by the 'cube_2' slot.";
  }

  { /* Unassign the Action. */
    const int user_count_pre = action->id.us;
    EXPECT_TRUE(unassign_action(cube->id));
    ASSERT_EQ(action->id.us, user_count_pre - 1)
        << "Unassigning an Action should lower its user count";

    ASSERT_EQ(2, action->slots().size()) << "Expecting the Action to have two Slots";
    EXPECT_FALSE(action->slot(0)->users(*bmain).contains(&cube->id))
        << "Expecting Cube to no longer be registered as animated by any slot.";
    EXPECT_FALSE(action->slot(1)->users(*bmain).contains(&cube->id))
        << "Expecting Cube to no longer be registered as animated by any slot.";
  }

  /* Assign Cube to another 'virgin' slot. This should not cause a name
   * collision between the Slots. */
  Slot &another_slot_cube = action->slot_add();
  ASSERT_EQ(assign_action_and_slot(action, &another_slot_cube, cube->id),
            ActionSlotAssignmentResult::OK);
  EXPECT_EQ(another_slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ("OBSlot.002", another_slot_cube.identifier) << "The slot should be uniquely named";
  EXPECT_STREQ("OBSlot.002", cube->adt->last_slot_identifier)
      << "The slot identifier should be copied to the adt";
  EXPECT_TRUE(another_slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Cube to be registered as animated by the 'another_slot_cube' slot.";

  /* Create an ID of another type. This should not be assignable to this slot. */
  ID *mesh = static_cast<ID *>(BKE_id_new_nomain(ID_ME, "Mesh"));
  ASSERT_TRUE(assign_action(action, *mesh));
  EXPECT_EQ(assign_action_slot(&slot_cube, *mesh), ActionSlotAssignmentResult::SlotNotSuitable)
      << "Mesh should not be animatable by an Object slot";
  EXPECT_FALSE(another_slot_cube.users(*bmain).contains(mesh))
      << "Expecting Mesh to not be registered as animated by the 'slot_cube' slot.";
  BKE_id_free(nullptr, mesh);
}

TEST_F(ActionLayersTest, rename_slot)
{
  Slot &slot_cube = action->slot_add();
  ASSERT_EQ(assign_action_and_slot(action, &slot_cube, cube->id), ActionSlotAssignmentResult::OK);
  EXPECT_EQ(slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ("OBSlot", slot_cube.identifier);
  EXPECT_STREQ(slot_cube.identifier, cube->adt->last_slot_identifier)
      << "The slot identifier should be copied to the adt";

  action->slot_identifier_define(slot_cube, "New Slot Name");
  EXPECT_STREQ("New Slot Name", slot_cube.identifier);
  /* At this point the slot identifier will not have been copied to the cube
   * AnimData. However, I don't want to test for that here, as it's not exactly
   * desirable behavior, but more of a side-effect of the current
   * implementation. */

  action->slot_identifier_propagate(*bmain, slot_cube);
  EXPECT_STREQ("New Slot Name", cube->adt->last_slot_identifier);

  /* Finally, do another rename, do NOT call the propagate function, then
   * unassign. This should still result in the correct slot name being stored
   * on the ADT. */
  action->slot_identifier_define(slot_cube, "Even Newer Name");
  EXPECT_TRUE(unassign_action(cube->id));
  EXPECT_STREQ("Even Newer Name", cube->adt->last_slot_identifier);
}

TEST_F(ActionLayersTest, slot_identifier_ensure_prefix)
{
  class AccessibleSlot : public Slot {
   public:
    void identifier_ensure_prefix()
    {
      Slot::identifier_ensure_prefix();
    }
  };

  Slot &raw_slot = action->slot_add();
  AccessibleSlot &slot = static_cast<AccessibleSlot &>(raw_slot);
  ASSERT_STREQ("XXSlot", slot.identifier);
  ASSERT_EQ(0, slot.idtype);

  /* Check defaults, idtype zeroed. */
  slot.identifier_ensure_prefix();
  EXPECT_STREQ("XXSlot", slot.identifier);

  /* idtype CA, default name.  */
  slot.idtype = ID_CA;
  slot.identifier_ensure_prefix();
  EXPECT_STREQ("CASlot", slot.identifier);

  /* idtype ME, explicit name of other idtype. */
  action->slot_identifier_define(slot, "CANewName");
  slot.idtype = ID_ME;
  slot.identifier_ensure_prefix();
  EXPECT_STREQ("MENewName", slot.identifier);

  /* Zeroing out idtype. */
  slot.idtype = 0;
  slot.identifier_ensure_prefix();
  EXPECT_STREQ("XXNewName", slot.identifier);
}

TEST_F(ActionLayersTest, slot_identifier_prefix)
{
  Slot &slot = action->slot_add();
  EXPECT_EQ("XX", slot.identifier_prefix_for_idtype());

  slot.idtype = ID_CA;
  EXPECT_EQ("CA", slot.identifier_prefix_for_idtype());
}

TEST_F(ActionLayersTest, rename_slot_identifier_collision)
{
  Slot &slot1 = action->slot_add();
  Slot &slot2 = action->slot_add();

  action->slot_identifier_define(slot1, "New Slot Name");
  action->slot_identifier_define(slot2, "New Slot Name");
  EXPECT_STREQ("New Slot Name", slot1.identifier);
  EXPECT_STREQ("New Slot Name.001", slot2.identifier);
}

TEST_F(ActionLayersTest, find_suitable_slot)
{
  /* ===
   * Empty case, no slots exist yet and the ID doesn't even have an AnimData. */
  EXPECT_EQ(nullptr, action->find_suitable_slot_for(cube->id));

  /* ===
   * Slot exists with the same name & type as the ID, but the ID doesn't have any AnimData yet.
   * These should nevertheless be matched up. */
  Slot &slot = action->slot_add();
  slot.handle = 327;
  STRNCPY_UTF8(slot.identifier, "OBKüüübus");
  slot.idtype = GS(cube->id.name);
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * Slot exists with the same name & type as the ID, and the ID has an AnimData with the same
   * slot identifier, but a different slot_handle. Since the Action has not yet been
   * assigned to this ID, the slot_handle should be ignored, and the slot identifier used for
   * matching. */

  /* Create a slot with a handle that should be ignored.*/
  Slot &other_slot = action->slot_add();
  other_slot.handle = 47;

  AnimData *adt = BKE_animdata_ensure_id(&cube->id);
  adt->action = nullptr;
  /* Configure adt to use the handle of one slot, and the identifier of the other. */
  adt->slot_handle = other_slot.handle;
  STRNCPY_UTF8(adt->last_slot_identifier, slot.identifier);
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * Same situation as above (AnimData has identifier of one slot, but the handle of another),
   * except that the Action has already been assigned. In this case the handle should take
   * precedence. */
  adt->action = action;
  id_us_plus(&action->id);
  EXPECT_EQ(&other_slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * A slot exists, but doesn't match anything in the action data of the cube. This should fall
   * back to using the ID name. */
  adt->slot_handle = 161;
  STRNCPY_UTF8(adt->last_slot_identifier, "¿¿What's this??");
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));
}

TEST_F(ActionLayersTest, active_slot)
{
  { /* Empty case, no slots exist yet. */
    EXPECT_EQ(nullptr, action->slot_active_get());

    action->slot_active_set(Slot::unassigned);
    EXPECT_EQ(nullptr, action->slot_active_get());
  }

  { /* Single slot case. */
    Slot &slot_cube = *assign_action_ensure_slot_for_keying(*action, cube->id);
    EXPECT_EQ(nullptr, action->slot_active_get())
        << "Adding the first slot should not change what is the active slot.";

    action->slot_active_set(slot_cube.handle);
    EXPECT_EQ(&slot_cube, action->slot_active_get())
        << "It should be possible to activate the only available slot";
    EXPECT_TRUE(slot_cube.is_active());

    action->slot_active_set(Slot::unassigned);
    EXPECT_EQ(nullptr, action->slot_active_get())
        << "It should be possible to de-activate the only available slot";
    EXPECT_FALSE(slot_cube.is_active());
  }

  {
    /* Multiple slots case. */
    Slot &slot_cube = *action->slot(0);
    action->slot_active_set(slot_cube.handle);

    Slot &slot_suz = *assign_action_ensure_slot_for_keying(*action, suzanne->id);
    Slot &slot_bob = *assign_action_ensure_slot_for_keying(*action, bob->id);
    EXPECT_EQ(&slot_cube, action->slot_active_get())
        << "Adding a subsequent slot should not change what is the active slot.";
    EXPECT_TRUE(slot_cube.is_active());

    action->slot_active_set(slot_suz.handle);
    EXPECT_EQ(&slot_suz, action->slot_active_get());
    EXPECT_FALSE(slot_cube.is_active());
    EXPECT_TRUE(slot_suz.is_active());
    EXPECT_FALSE(slot_bob.is_active());

    action->slot_active_set(slot_bob.handle);
    EXPECT_EQ(&slot_bob, action->slot_active_get());
    EXPECT_FALSE(slot_cube.is_active());
    EXPECT_FALSE(slot_suz.is_active());
    EXPECT_TRUE(slot_bob.is_active());

    action->slot_active_set(Slot::unassigned);
    EXPECT_EQ(nullptr, action->slot_active_get());
    EXPECT_FALSE(slot_cube.is_active());
    EXPECT_FALSE(slot_suz.is_active());
    EXPECT_FALSE(slot_bob.is_active());
  }
}

TEST_F(ActionLayersTest, strip)
{
  constexpr float inf = std::numeric_limits<float>::infinity();
  Layer &layer0 = action->layer_add("Test Læür nul");
  Strip &strip = layer0.strip_add(*action, Strip::Type::Keyframe);

  strip.resize(-inf, inf);
  EXPECT_TRUE(strip.contains_frame(0.0f));
  EXPECT_TRUE(strip.contains_frame(-100000.0f));
  EXPECT_TRUE(strip.contains_frame(100000.0f));
  EXPECT_TRUE(strip.is_last_frame(inf));

  strip.resize(1.0f, 2.0f);
  EXPECT_FALSE(strip.contains_frame(0.0f))
      << "Strip should not contain frames before its first frame";
  EXPECT_TRUE(strip.contains_frame(1.0f)) << "Strip should contain its first frame.";
  EXPECT_TRUE(strip.contains_frame(2.0f)) << "Strip should contain its last frame.";
  EXPECT_FALSE(strip.contains_frame(2.0001f))
      << "Strip should not contain frames after its last frame";

  EXPECT_FALSE(strip.is_last_frame(1.0f));
  EXPECT_FALSE(strip.is_last_frame(1.5f));
  EXPECT_FALSE(strip.is_last_frame(1.9999f));
  EXPECT_TRUE(strip.is_last_frame(2.0f));
  EXPECT_FALSE(strip.is_last_frame(2.0001f));

  /* Same test as above, but with much larger end frame number. This is 2 hours at 24 FPS. */
  strip.resize(1.0f, 172800.0f);
  EXPECT_TRUE(strip.contains_frame(172800.0f)) << "Strip should contain its last frame.";
  EXPECT_FALSE(strip.contains_frame(172800.1f))
      << "Strip should not contain frames after its last frame";

  /* You can't get much closer to the end frame before it's considered equal. */
  EXPECT_FALSE(strip.is_last_frame(172799.925f));
  EXPECT_TRUE(strip.is_last_frame(172800.0f));
  EXPECT_FALSE(strip.is_last_frame(172800.075f));
}

TEST_F(ActionLayersTest, KeyframeStrip__keyframe_insert)
{
  Slot &slot = action->slot_add();
  ASSERT_EQ(assign_action_and_slot(action, &slot, cube->id), ActionSlotAssignmentResult::OK);
  Layer &layer = action->layer_add("Kübus layer");

  Strip &strip = layer.strip_add(*action, Strip::Type::Keyframe);
  StripKeyframeData &strip_data = strip.data<StripKeyframeData>(*action);

  const KeyframeSettings settings = get_keyframe_settings(false);
  SingleKeyingResult result_loc_a = strip_data.keyframe_insert(
      bmain, slot, {"location", 0}, {1.0f, 47.0f}, settings);
  ASSERT_EQ(SingleKeyingResult::SUCCESS, result_loc_a)
      << "Expected keyframe insertion to be successful";

  /* Check the strip was created correctly, with the channels for the slot. */
  ASSERT_EQ(1, strip_data.channelbags().size());
  ChannelBag *channels = strip_data.channelbag(0);
  EXPECT_EQ(slot.handle, channels->slot_handle);

  /* Insert a second key, should insert into the same FCurve as before. */
  SingleKeyingResult result_loc_b = strip_data.keyframe_insert(
      bmain, slot, {"location", 0}, {5.0f, 47.1f}, settings);
  EXPECT_EQ(SingleKeyingResult::SUCCESS, result_loc_b);
  ASSERT_EQ(1, channels->fcurves().size()) << "Expect insertion with the same (slot/rna "
                                              "path/array index) tuple to go into the same FCurve";
  EXPECT_EQ(2, channels->fcurves()[0]->totvert)
      << "Expect insertion with the same (slot/rna path/array index) tuple to go into the same "
         "FCurve";

  EXPECT_EQ(47.0f, evaluate_fcurve(channels->fcurves()[0], 1.0f));
  EXPECT_EQ(47.1f, evaluate_fcurve(channels->fcurves()[0], 5.0f));

  /* Insert another key for another property, should create another FCurve. */
  SingleKeyingResult result_rot = strip_data.keyframe_insert(
      bmain, slot, {"rotation_quaternion", 0}, {1.0f, 0.25f}, settings);
  EXPECT_EQ(SingleKeyingResult::SUCCESS, result_rot);
  ASSERT_EQ(2, channels->fcurves().size()) << "Expected a second FCurve to be created.";
  EXPECT_EQ(2, channels->fcurves()[0]->totvert);
  EXPECT_EQ(1, channels->fcurves()[1]->totvert);
}

TEST_F(ActionLayersTest, is_action_assignable_to)
{
  EXPECT_TRUE(is_action_assignable_to(nullptr, ID_OB))
      << "nullptr Actions should be assignable to any type.";
  EXPECT_TRUE(is_action_assignable_to(nullptr, ID_CA))
      << "nullptr Actions should be assignable to any type.";

  EXPECT_TRUE(is_action_assignable_to(action, ID_OB))
      << "Empty Actions should be assignable to any type.";
  EXPECT_TRUE(is_action_assignable_to(action, ID_CA))
      << "Empty Actions should be assignable to any type.";

  /* Make the Action a legacy one. */
  FCurve fake_fcurve;
  BLI_addtail(&action->curves, &fake_fcurve);
  ASSERT_FALSE(action->is_empty());
  ASSERT_TRUE(action->is_action_legacy());
  ASSERT_EQ(0, action->idroot);

  EXPECT_TRUE(is_action_assignable_to(action, ID_OB))
      << "Legacy Actions with idroot=0 should be assignable to any type.";
  EXPECT_TRUE(is_action_assignable_to(action, ID_CA))
      << "Legacy Actions with idroot=0 should be assignable to any type.";

  /* Set the legacy idroot. */
  action->idroot = ID_CA;
  EXPECT_FALSE(is_action_assignable_to(action, ID_OB))
      << "Legacy Actions with idroot=ID_CA should NOT be assignable to ID_OB.";
  EXPECT_TRUE(is_action_assignable_to(action, ID_CA))
      << "Legacy Actions with idroot=CA should be assignable to ID_CA.";

  /* Make the Action a layered one. */
  BLI_poptail(&action->curves);
  action->layer_add("layer");
  ASSERT_EQ(0, action->idroot) << "Adding a layer should clear the idroot.";

  EXPECT_TRUE(is_action_assignable_to(action, ID_OB))
      << "Layered Actions should be assignable to any type.";
  EXPECT_TRUE(is_action_assignable_to(action, ID_CA))
      << "Layered Actions should be assignable to any type.";
}

TEST_F(ActionLayersTest, action_slot_get_id_for_keying__empty_action)
{
  EXPECT_TRUE(assign_action(action, cube->id));

  /* Double-check that the action is considered empty for the test. */
  EXPECT_TRUE(action->is_empty());

  /* None should return an ID, since there are no slots yet which could have this ID assigned.
   * Assignment of the Action itself (cube) shouldn't matter. */
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, &cube->id));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, &suzanne->id));
}

TEST_F(ActionLayersTest, action_slot_get_id_for_keying__legacy_action)
{
  FCurve *fcurve = action_fcurve_ensure_legacy(bmain, action, nullptr, nullptr, {"location", 0});
  EXPECT_FALSE(fcurve == nullptr);

  EXPECT_TRUE(assign_action(action, cube->id));

  /* Double-check that the action is considered legacy for the test. */
  EXPECT_TRUE(action->is_action_legacy());

  /* A `primary_id` that uses the action should get returned. Every other case
   * should return nullptr. */
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, 0, &cube->id));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, &suzanne->id));
}

TEST_F(ActionLayersTest, action_slot_get_id_for_keying__layered_action)
{
  Slot &slot = action->slot_add();

  /* Double-check that the action is considered layered for the test. */
  EXPECT_TRUE(action->is_action_layered());

  /* A slot with no users should never return a user. */
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &cube->id));

  /* A slot with precisely one user should always return that user. */
  ASSERT_EQ(assign_action_and_slot(action, &slot, cube->id), ActionSlotAssignmentResult::OK);
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, nullptr));
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &cube->id));
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &suzanne->id));

  /* A slot with more than one user should return the passed `primary_id` if it
   * is among its users, and nullptr otherwise. */
  ASSERT_EQ(assign_action_and_slot(action, &slot, suzanne->id), ActionSlotAssignmentResult::OK);
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &cube->id));
  EXPECT_EQ(&suzanne->id,
            action_slot_get_id_for_keying(*bmain, *action, slot.handle, &suzanne->id));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &bob->id));
}

TEST_F(ActionLayersTest, conversion_to_layered)
{
  EXPECT_TRUE(action->is_empty());
  FCurve *legacy_fcu_0 = action_fcurve_ensure_legacy(
      bmain, action, "Test", nullptr, {"location", 0});
  FCurve *legacy_fcu_1 = action_fcurve_ensure_legacy(
      bmain, action, "Test", nullptr, {"location", 1});

  KeyframeSettings settings;
  settings.handle = HD_AUTO;
  settings.interpolation = BEZT_IPO_BEZ;
  settings.keyframe_type = BEZT_KEYTYPE_KEYFRAME;
  insert_vert_fcurve(legacy_fcu_0, {0, 0}, settings, INSERTKEY_NOFLAGS);
  insert_vert_fcurve(legacy_fcu_0, {1, 1}, settings, INSERTKEY_NOFLAGS);
  add_fmodifier(&legacy_fcu_1->modifiers, FMODIFIER_TYPE_NOISE, legacy_fcu_1);

  Action *converted = convert_to_layered_action(*bmain, *action);
  ASSERT_TRUE(converted != action);
  EXPECT_STREQ(converted->id.name, "ACACÄnimåtië_layered");
  Strip *strip = converted->layer(0)->strip(0);
  StripKeyframeData &strip_data = strip->data<StripKeyframeData>(*converted);
  ChannelBag *bag = strip_data.channelbag(0);
  ASSERT_EQ(bag->fcurve_array_num, 2);
  ASSERT_EQ(bag->fcurve_array[0]->totvert, 2);

  ASSERT_EQ(BLI_listbase_count(&action->groups), 1);
  ASSERT_EQ(BLI_listbase_count(&converted->groups), 0);

  ASSERT_EQ(bag->channel_groups().size(), 1);
  bActionGroup *group = bag->channel_group(0);
  ASSERT_EQ(group->fcurve_range_length, 2);
  ASSERT_STREQ(group->name, "Test");

  ASSERT_TRUE(bag->fcurve_array[0]->modifiers.first == nullptr);
  ASSERT_TRUE(bag->fcurve_array[1]->modifiers.first != nullptr);

  Action *long_name_action = static_cast<Action *>(BKE_id_new(
      bmain, ID_AC, "name_for_an_action_that_is_exactly_64_chars_which_is_MAX_ID_NAME"));
  action_fcurve_ensure_legacy(bmain, long_name_action, "Long", nullptr, {"location", 0});
  converted = convert_to_layered_action(*bmain, *long_name_action);
  /* AC gets added automatically by Blender, the long name is shortened to make space for
   * "_layered". */
  EXPECT_STREQ(converted->id.name,
               "ACname_for_an_action_that_is_exactly_64_chars_which_is_MA_layered");
}

TEST_F(ActionLayersTest, conversion_to_layered_action_groups)
{
  EXPECT_TRUE(action->is_empty());
  action_fcurve_ensure_legacy(bmain, action, "Test", nullptr, {"location", 0});
  action_fcurve_ensure_legacy(bmain, action, "Test", nullptr, {"rotation_euler", 1});
  action_fcurve_ensure_legacy(bmain, action, "Test_Two", nullptr, {"scale", 1});
  action_fcurve_ensure_legacy(bmain, action, "Test_Three", nullptr, {"show_name", 1});
  action_fcurve_ensure_legacy(bmain, action, "Test_Rename", nullptr, {"show_axis", 1});

  bActionGroup *rename_group = static_cast<bActionGroup *>(BLI_findlink(&action->groups, 3));
  ASSERT_NE(rename_group, nullptr);
  ASSERT_STREQ(rename_group->name, "Test_Rename");
  /* Forcing a duplicate name which was allowed by legacy actions. */
  strcpy(rename_group->name, "Test");

  Action *converted = convert_to_layered_action(*bmain, *action);
  Strip *strip = converted->layer(0)->strip(0);
  StripKeyframeData &strip_data = strip->data<StripKeyframeData>(*converted);
  ChannelBag *bag = strip_data.channelbag(0);

  ASSERT_EQ(BLI_listbase_count(&converted->groups), 0);
  ASSERT_EQ(bag->channel_groups().size(), 4);

  bActionGroup *test_group = bag->channel_group(0);
  EXPECT_STREQ(test_group->name, "Test");
  EXPECT_EQ(test_group->fcurve_range_length, 2);

  bActionGroup *test_two_group = bag->channel_group(1);
  EXPECT_STREQ(test_two_group->name, "Test_Two");
  EXPECT_EQ(test_two_group->fcurve_range_length, 1);
  EXPECT_STREQ(bag->fcurve_array[test_two_group->fcurve_range_start]->rna_path, "scale");

  bActionGroup *test_three_group = bag->channel_group(2);
  EXPECT_STREQ(test_three_group->name, "Test_Three");
  EXPECT_EQ(test_three_group->fcurve_range_length, 1);
  EXPECT_STREQ(bag->fcurve_array[test_three_group->fcurve_range_start]->rna_path, "show_name");

  bActionGroup *test_rename_group = bag->channel_group(3);
  EXPECT_STREQ(test_rename_group->name, "Test.001");
  EXPECT_EQ(test_rename_group->fcurve_range_length, 1);
  EXPECT_STREQ(bag->fcurve_array[test_rename_group->fcurve_range_start]->rna_path, "show_axis");

  ASSERT_NE(converted, action);
}

TEST_F(ActionLayersTest, empty_to_layered)
{
  ASSERT_TRUE(action->is_empty());
  Action *converted = convert_to_layered_action(*bmain, *action);
  ASSERT_TRUE(converted != action);
  ASSERT_TRUE(converted->is_action_layered());
  ASSERT_FALSE(converted->is_action_legacy());
}

TEST_F(ActionLayersTest, action_move_slot)
{
  Action *action_2 = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "Action 2"));
  EXPECT_TRUE(action->is_empty());

  Slot &slot_cube = action->slot_add();
  Slot &slot_suzanne = action_2->slot_add();
  EXPECT_EQ(assign_action_and_slot(action, &slot_cube, cube->id), ActionSlotAssignmentResult::OK);
  EXPECT_EQ(assign_action_and_slot(action_2, &slot_suzanne, suzanne->id),
            ActionSlotAssignmentResult::OK);

  PointerRNA cube_rna_pointer = RNA_id_pointer_create(&cube->id);
  PointerRNA suzanne_rna_pointer = RNA_id_pointer_create(&suzanne->id);

  action_fcurve_ensure(bmain, action, "Test", &cube_rna_pointer, {"location", 0});
  action_fcurve_ensure(bmain, action, "Test", &cube_rna_pointer, {"rotation_euler", 1});

  action_fcurve_ensure(bmain, action_2, "Test_2", &suzanne_rna_pointer, {"location", 0});
  action_fcurve_ensure(bmain, action_2, "Test_2", &suzanne_rna_pointer, {"rotation_euler", 1});

  ASSERT_EQ(action->layer_array_num, 1);
  ASSERT_EQ(action_2->layer_array_num, 1);

  Layer *layer_1 = action->layer(0);
  Layer *layer_2 = action_2->layer(0);

  ASSERT_EQ(layer_1->strip_array_num, 1);
  ASSERT_EQ(layer_2->strip_array_num, 1);

  StripKeyframeData &strip_data_1 = layer_1->strip(0)->data<StripKeyframeData>(*action);
  StripKeyframeData &strip_data_2 = layer_2->strip(0)->data<StripKeyframeData>(*action_2);

  ASSERT_EQ(strip_data_1.channelbag_array_num, 1);
  ASSERT_EQ(strip_data_2.channelbag_array_num, 1);

  ChannelBag *bag_1 = strip_data_1.channelbag(0);
  ChannelBag *bag_2 = strip_data_2.channelbag(0);

  ASSERT_EQ(bag_1->fcurve_array_num, 2);
  ASSERT_EQ(bag_2->fcurve_array_num, 2);

  move_slot(*bmain, slot_suzanne, *action_2, *action);

  ASSERT_EQ(strip_data_1.channelbag_array_num, 2);
  ASSERT_EQ(strip_data_2.channelbag_array_num, 0);

  ASSERT_EQ(action->slot_array_num, 2);
  ASSERT_EQ(action_2->slot_array_num, 0);

  /* Action should have been reassigned. */
  ASSERT_EQ(action, cube->adt->action);
  ASSERT_EQ(action, suzanne->adt->action);
}

/*-----------------------------------------------------------*/

/* Allocate fcu->bezt, and also return a unique_ptr to it for easily freeing the memory. */
static void allocate_keyframes(FCurve &fcu, const size_t num_keyframes)
{
  fcu.bezt = MEM_cnew_array<BezTriple>(num_keyframes, __func__);
}

/* Append keyframe, assumes that fcu->bezt is allocated and has enough space. */
static void add_keyframe(FCurve &fcu, float x, float y)
{
  /* The insert_keyframe functions are in the editors, so we cannot link to those here. */
  BezTriple the_keyframe;
  memset(&the_keyframe, 0, sizeof(the_keyframe));

  /* Copied from insert_vert_fcurve() in `keyframing.cc`. */
  the_keyframe.vec[0][0] = x - 1.0f;
  the_keyframe.vec[0][1] = y;
  the_keyframe.vec[1][0] = x;
  the_keyframe.vec[1][1] = y;
  the_keyframe.vec[2][0] = x + 1.0f;
  the_keyframe.vec[2][1] = y;

  memcpy(&fcu.bezt[fcu.totvert], &the_keyframe, sizeof(the_keyframe));
  fcu.totvert++;
}

static void add_fcurve_to_action(Action &action, FCurve &fcu)
{
  Slot &slot = action.slot_array_num > 0 ? *action.slot(0) : action.slot_add();
  action.layer_keystrip_ensure();
  StripKeyframeData &strip_data = action.layer(0)->strip(0)->data<StripKeyframeData>(action);
  ChannelBag &cbag = strip_data.channelbag_for_slot_ensure(slot);
  cbag.fcurve_append(fcu);
}

class ActionQueryTest : public testing::Test {
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

  Action &action_new()
  {
    return *static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACÄnimåtië"));
  }
};

TEST_F(ActionQueryTest, BKE_action_frame_range_calc)
{
  /* No FCurves. */
  {
    const Action &empty = action_new();
    EXPECT_EQ((float2{0.0f, 0.0f}), empty.get_frame_range_of_keys(false));
  }

  /* One curve with one key. */
  {
    FCurve &fcu = *MEM_cnew<FCurve>(__func__);
    allocate_keyframes(fcu, 1);
    add_keyframe(fcu, 1.0f, 2.0f);

    Action &action = action_new();
    add_fcurve_to_action(action, fcu);

    const float2 frame_range = action.get_frame_range_of_keys(false);
    EXPECT_FLOAT_EQ(frame_range[0], 1.0f);
    EXPECT_FLOAT_EQ(frame_range[1], 1.0f);
  }

  /* Two curves with one key each on different frames. */
  {
    FCurve &fcu1 = *MEM_cnew<FCurve>(__func__);
    FCurve &fcu2 = *MEM_cnew<FCurve>(__func__);
    allocate_keyframes(fcu1, 1);
    allocate_keyframes(fcu2, 1);
    add_keyframe(fcu1, 1.0f, 2.0f);
    add_keyframe(fcu2, 1.5f, 2.0f);

    Action &action = action_new();
    add_fcurve_to_action(action, fcu1);
    add_fcurve_to_action(action, fcu2);

    const float2 frame_range = action.get_frame_range_of_keys(false);
    EXPECT_FLOAT_EQ(frame_range[0], 1.0f);
    EXPECT_FLOAT_EQ(frame_range[1], 1.5f);
  }

  /* One curve with two keys. */
  {
    FCurve &fcu = *MEM_cnew<FCurve>(__func__);
    allocate_keyframes(fcu, 2);
    add_keyframe(fcu, 1.0f, 2.0f);
    add_keyframe(fcu, 1.5f, 2.0f);

    Action &action = action_new();
    add_fcurve_to_action(action, fcu);

    const float2 frame_range = action.get_frame_range_of_keys(false);
    EXPECT_FLOAT_EQ(frame_range[0], 1.0f);
    EXPECT_FLOAT_EQ(frame_range[1], 1.5f);
  }

  /* TODO: action with fcurve modifiers. */
}

/*-----------------------------------------------------------*/

class ChannelBagTest : public testing::Test {
 public:
  ChannelBag *channel_bag;

  static void SetUpTestSuite() {}

  static void TearDownTestSuite() {}

  void SetUp() override
  {
    channel_bag = new ChannelBag();
  }

  void TearDown() override
  {
    delete channel_bag;
  }
};

TEST_F(ChannelBagTest, fcurve_move)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "group0"});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "group0"});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "group1"});
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "group1"});
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, std::nullopt});

  ASSERT_EQ(5, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());

  bActionGroup &group0 = *channel_bag->channel_group(0);
  bActionGroup &group1 = *channel_bag->channel_group(1);

  /* Moving an fcurve to where it already is should be fine. */
  channel_bag->fcurve_move(fcu0, 0);
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group1, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  /* Move to first. */
  channel_bag->fcurve_move(fcu4, 0);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&fcu4, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu4.grp);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group1, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu3.grp);

  /* Move to last. */
  channel_bag->fcurve_move(fcu1, 4);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&fcu4, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu4.grp);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group1, fcu3.grp);
  EXPECT_EQ(nullptr, fcu1.grp);

  /* Move to middle. */
  channel_bag->fcurve_move(fcu4, 2);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu2.grp);
  EXPECT_EQ(&group1, fcu4.grp);
  EXPECT_EQ(&group1, fcu3.grp);
  EXPECT_EQ(nullptr, fcu1.grp);
}

TEST_F(ChannelBagTest, channel_group_create)
{
  ASSERT_TRUE(channel_bag->channel_groups().is_empty());

  bActionGroup &group0 = channel_bag->channel_group_create("Foo");
  ASSERT_EQ(channel_bag->channel_groups().size(), 1);
  EXPECT_EQ(StringRef{group0.name}, StringRef{"Foo"});
  EXPECT_EQ(group0.fcurve_range_start, 0);
  EXPECT_EQ(group0.fcurve_range_length, 0);
  EXPECT_EQ(&group0, channel_bag->channel_group(0));

  /* Set for testing purposes. Does not reflect actual fcurves in this test. */
  group0.fcurve_range_length = 2;

  bActionGroup &group1 = channel_bag->channel_group_create("Bar");
  ASSERT_EQ(channel_bag->channel_groups().size(), 2);
  EXPECT_EQ(StringRef{group1.name}, StringRef{"Bar"});
  EXPECT_EQ(group1.fcurve_range_start, 2);
  EXPECT_EQ(group1.fcurve_range_length, 0);
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));

  /* Set for testing purposes. Does not reflect actual fcurves in this test. */
  group1.fcurve_range_length = 1;

  bActionGroup &group2 = channel_bag->channel_group_create("Yar");
  ASSERT_EQ(channel_bag->channel_groups().size(), 3);
  EXPECT_EQ(StringRef{group2.name}, StringRef{"Yar"});
  EXPECT_EQ(group2.fcurve_range_start, 3);
  EXPECT_EQ(group2.fcurve_range_length, 0);
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(&group2, channel_bag->channel_group(2));
}

TEST_F(ChannelBagTest, channel_group_remove)
{
  bActionGroup &group0 = channel_bag->channel_group_create("Group0");
  bActionGroup &group1 = channel_bag->channel_group_create("Group1");
  bActionGroup &group2 = channel_bag->channel_group_create("Group2");

  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "Group0"});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "Group0"});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "Group2"});
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "Group2"});
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, std::nullopt});

  ASSERT_EQ(3, channel_bag->channel_groups().size());
  ASSERT_EQ(5, channel_bag->fcurves().size());

  /* Attempt to remove a group that's not in the channel bag. Shouldn't do
   * anything. */
  bActionGroup bogus;
  EXPECT_EQ(false, channel_bag->channel_group_remove(bogus));
  ASSERT_EQ(3, channel_bag->channel_groups().size());
  ASSERT_EQ(5, channel_bag->fcurves().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(&group2, channel_bag->channel_group(2));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group2, fcu2.grp);
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  /* Removing an empty group shouldn't affect the fcurves at all. */
  EXPECT_EQ(true, channel_bag->channel_group_remove(group1));
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  ASSERT_EQ(5, channel_bag->fcurves().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group2, channel_bag->channel_group(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group2, fcu2.grp);
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  /* Removing a group that's not at the end of the group array should move its
   * fcurves to be just after the grouped fcurves. */
  EXPECT_EQ(true, channel_bag->channel_group_remove(group0));
  ASSERT_EQ(1, channel_bag->channel_groups().size());
  ASSERT_EQ(5, channel_bag->fcurves().size());
  EXPECT_EQ(&group2, channel_bag->channel_group(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(nullptr, fcu1.grp);
  EXPECT_EQ(&group2, fcu2.grp);
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  /* Removing a group at the end of the group array shouldn't move its
   * fcurves. */
  EXPECT_EQ(true, channel_bag->channel_group_remove(group2));
  ASSERT_EQ(0, channel_bag->channel_groups().size());
  ASSERT_EQ(5, channel_bag->fcurves().size());
  EXPECT_EQ(&fcu2, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(nullptr, fcu1.grp);
  EXPECT_EQ(nullptr, fcu2.grp);
  EXPECT_EQ(nullptr, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);
}

TEST_F(ChannelBagTest, channel_group_find)
{
  bActionGroup &group0a = channel_bag->channel_group_create("Foo");
  bActionGroup &group1a = channel_bag->channel_group_create("Bar");
  bActionGroup &group2a = channel_bag->channel_group_create("Yar");

  bActionGroup *group0b = channel_bag->channel_group_find("Foo");
  bActionGroup *group1b = channel_bag->channel_group_find("Bar");
  bActionGroup *group2b = channel_bag->channel_group_find("Yar");

  EXPECT_EQ(&group0a, group0b);
  EXPECT_EQ(&group1a, group1b);
  EXPECT_EQ(&group2a, group2b);

  EXPECT_EQ(nullptr, channel_bag->channel_group_find("Wat"));
}

TEST_F(ChannelBagTest, channel_group_ensure)
{
  bActionGroup &group0 = channel_bag->channel_group_create("Foo");
  bActionGroup &group1 = channel_bag->channel_group_create("Bar");
  EXPECT_EQ(channel_bag->channel_groups().size(), 2);

  EXPECT_EQ(&group0, &channel_bag->channel_group_ensure("Foo"));
  EXPECT_EQ(channel_bag->channel_groups().size(), 2);

  EXPECT_EQ(&group1, &channel_bag->channel_group_ensure("Bar"));
  EXPECT_EQ(channel_bag->channel_groups().size(), 2);

  bActionGroup &group2 = channel_bag->channel_group_ensure("Yar");
  ASSERT_EQ(channel_bag->channel_groups().size(), 3);
  EXPECT_EQ(&group2, channel_bag->channel_group(2));
}

TEST_F(ChannelBagTest, channel_group_fcurve_creation)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, std::nullopt});
  EXPECT_EQ(1, channel_bag->fcurves().size());
  EXPECT_TRUE(channel_bag->channel_groups().is_empty());

  /* If an fcurve already exists, then ensuring it with a channel group in the
   * fcurve descriptor should NOT add it that group, nor should the group be
   * created if it doesn't already exist. */
  channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "group0"});
  EXPECT_EQ(1, channel_bag->fcurves().size());
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_TRUE(channel_bag->channel_groups().is_empty());

  /* Creating a new fcurve with a channel group in the fcurve descriptor should
   * create the group and put the fcurve in it.  This also implies that the
   * fcurve will be added before any non-grouped fcurves in the array. */
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "group0"});
  ASSERT_EQ(2, channel_bag->fcurves().size());
  ASSERT_EQ(1, channel_bag->channel_groups().size());
  bActionGroup &group0 = *channel_bag->channel_group(0);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);

  /* Creating a new fcurve with a second channel group in the fcurve descriptor
   * should create the group and put the fcurve in it.  This also implies that
   * the fcurve will be added before non-grouped fcurves, but after other
   * grouped ones. */
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "group1"});
  ASSERT_EQ(3, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  bActionGroup &group1 = *channel_bag->channel_group(1);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);

  /* Creating a new fcurve with the first channel group again should put it at
   * the end of that group. */
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "group0"});
  ASSERT_EQ(4, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(3));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group0, fcu3.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);

  /* Finally, creating a new fcurve with the second channel group again should
   * also put it at the end of that group. */
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, "group1"});
  ASSERT_EQ(5, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group0, fcu3.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group1, fcu4.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
}

TEST_F(ChannelBagTest, channel_group_fcurve_removal)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "group0"});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "group0"});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "group1"});
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "group1"});
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, std::nullopt});

  ASSERT_EQ(5, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());

  bActionGroup &group0 = *channel_bag->channel_group(0);
  bActionGroup &group1 = *channel_bag->channel_group(1);

  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group1, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->fcurve_remove(fcu3);
  ASSERT_EQ(4, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->fcurve_remove(fcu0);
  ASSERT_EQ(3, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->fcurve_remove(fcu1);
  ASSERT_EQ(2, channel_bag->fcurves().size());
  ASSERT_EQ(1, channel_bag->channel_groups().size());
  EXPECT_EQ(&group1, channel_bag->channel_group(0));
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->fcurve_remove(fcu4);
  ASSERT_EQ(1, channel_bag->fcurves().size());
  ASSERT_EQ(1, channel_bag->channel_groups().size());
  EXPECT_EQ(&group1, channel_bag->channel_group(0));
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&group1, fcu2.grp);

  channel_bag->fcurve_remove(fcu2);
  ASSERT_EQ(0, channel_bag->fcurves().size());
  ASSERT_EQ(0, channel_bag->channel_groups().size());
}

TEST_F(ChannelBagTest, channel_group_move)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "group0"});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "group1"});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "group1"});
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "group2"});
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, std::nullopt});

  ASSERT_EQ(5, channel_bag->fcurves().size());
  ASSERT_EQ(3, channel_bag->channel_groups().size());

  bActionGroup &group0 = *channel_bag->channel_group(0);
  bActionGroup &group1 = *channel_bag->channel_group(1);
  bActionGroup &group2 = *channel_bag->channel_group(2);

  channel_bag->channel_group_move(group0, 2);
  EXPECT_EQ(&group1, channel_bag->channel_group(0));
  EXPECT_EQ(&group2, channel_bag->channel_group(1));
  EXPECT_EQ(&group0, channel_bag->channel_group(2));
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(2, group2.fcurve_range_start);
  EXPECT_EQ(1, group2.fcurve_range_length);
  EXPECT_EQ(3, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group1, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->channel_group_move(group1, 1);
  EXPECT_EQ(&group2, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(&group0, channel_bag->channel_group(2));
  EXPECT_EQ(0, group2.fcurve_range_start);
  EXPECT_EQ(1, group2.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(3, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(&fcu3, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(&group1, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(nullptr, fcu4.grp);

  channel_bag->channel_group_move(group0, 0);
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group2, channel_bag->channel_group(1));
  EXPECT_EQ(&group1, channel_bag->channel_group(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group2.fcurve_range_start);
  EXPECT_EQ(1, group2.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu0.grp);
  EXPECT_EQ(&group2, fcu3.grp);
  EXPECT_EQ(&group1, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);
}

TEST_F(ChannelBagTest, channel_group_move_fcurve_into)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, std::nullopt});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, std::nullopt});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, std::nullopt});
  bActionGroup &group0 = channel_bag->channel_group_create("group0");
  bActionGroup &group1 = channel_bag->channel_group_create("group1");

  ASSERT_EQ(3, channel_bag->fcurves().size());
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  ASSERT_EQ(2, channel_bag->channel_groups().size());
  EXPECT_EQ(&group0, channel_bag->channel_group(0));
  EXPECT_EQ(&group1, channel_bag->channel_group(1));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(0, group0.fcurve_range_length);
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(0, group1.fcurve_range_length);

  channel_bag->fcurve_assign_to_channel_group(fcu2, group1);
  EXPECT_EQ(&fcu2, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(0, group0.fcurve_range_length);
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);

  channel_bag->fcurve_assign_to_channel_group(fcu1, group0);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);

  channel_bag->fcurve_assign_to_channel_group(fcu0, group1);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);

  channel_bag->fcurve_assign_to_channel_group(fcu0, group0);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(2, group0.fcurve_range_length);
  EXPECT_EQ(2, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);

  channel_bag->fcurve_assign_to_channel_group(fcu1, group1);
  EXPECT_EQ(&fcu0, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(2));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
}

TEST_F(ChannelBagTest, channel_group_fcurve_ungroup)
{
  FCurve &fcu0 = channel_bag->fcurve_ensure(nullptr, {"fcu0", 0, std::nullopt, "group0"});
  FCurve &fcu1 = channel_bag->fcurve_ensure(nullptr, {"fcu1", 0, std::nullopt, "group0"});
  FCurve &fcu2 = channel_bag->fcurve_ensure(nullptr, {"fcu2", 0, std::nullopt, "group1"});
  FCurve &fcu3 = channel_bag->fcurve_ensure(nullptr, {"fcu3", 0, std::nullopt, "group1"});
  FCurve &fcu4 = channel_bag->fcurve_ensure(nullptr, {"fcu4", 0, std::nullopt, std::nullopt});

  ASSERT_EQ(5, channel_bag->fcurves().size());
  ASSERT_EQ(2, channel_bag->channel_groups().size());

  bActionGroup &group0 = *channel_bag->channel_group(0);
  bActionGroup &group1 = *channel_bag->channel_group(1);

  /* Attempting to ungroup an fcurve that's not in the channel bag should fail. */
  FCurve bogus = {};
  EXPECT_FALSE(channel_bag->fcurve_ungroup(bogus));

  /* Attempting to ungroup an fcurve that's already ungrouped is fine. */
  EXPECT_TRUE(channel_bag->fcurve_ungroup(fcu4));

  /* Ungroup each fcurve until all are ungrouped. */

  EXPECT_TRUE(channel_bag->fcurve_ungroup(fcu0));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(2, group1.fcurve_range_length);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(&group1, fcu3.grp);
  EXPECT_EQ(nullptr, fcu4.grp);
  EXPECT_EQ(nullptr, fcu0.grp);

  EXPECT_TRUE(channel_bag->fcurve_ungroup(fcu3));
  EXPECT_EQ(0, group0.fcurve_range_start);
  EXPECT_EQ(1, group0.fcurve_range_length);
  EXPECT_EQ(1, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&fcu1, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(4));
  EXPECT_EQ(&group0, fcu1.grp);
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(nullptr, fcu3.grp);

  EXPECT_TRUE(channel_bag->fcurve_ungroup(fcu1));
  EXPECT_EQ(1, channel_bag->channel_groups().size());
  EXPECT_EQ(&group1, channel_bag->channel_group(0));
  EXPECT_EQ(0, group1.fcurve_range_start);
  EXPECT_EQ(1, group1.fcurve_range_length);
  EXPECT_EQ(&fcu2, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu4, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(4));
  EXPECT_EQ(&group1, fcu2.grp);
  EXPECT_EQ(nullptr, fcu4.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(nullptr, fcu3.grp);
  EXPECT_EQ(nullptr, fcu1.grp);

  EXPECT_TRUE(channel_bag->fcurve_ungroup(fcu2));
  EXPECT_EQ(0, channel_bag->channel_groups().size());
  EXPECT_EQ(&fcu4, channel_bag->fcurve(0));
  EXPECT_EQ(&fcu0, channel_bag->fcurve(1));
  EXPECT_EQ(&fcu3, channel_bag->fcurve(2));
  EXPECT_EQ(&fcu1, channel_bag->fcurve(3));
  EXPECT_EQ(&fcu2, channel_bag->fcurve(4));
  EXPECT_EQ(nullptr, fcu4.grp);
  EXPECT_EQ(nullptr, fcu0.grp);
  EXPECT_EQ(nullptr, fcu3.grp);
  EXPECT_EQ(nullptr, fcu1.grp);
  EXPECT_EQ(nullptr, fcu2.grp);
}

/*-----------------------------------------------------------*/

class ActionFCurveMoveTest : public testing::Test {
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

  static FCurve *fcurve_create(const StringRefNull rna_path, const int array_index)
  {
    FCurve *fcurve = BKE_fcurve_create();
    fcurve->rna_path = BLI_strdupn(rna_path.c_str(), array_index);
    return fcurve;
  };
};

TEST_F(ActionFCurveMoveTest, test_fcurve_move_legacy)
{
  Action &action_src = action_add(*this->bmain, "SourceAction");
  Action &action_dst = action_add(*this->bmain, "DestinationAction");

  /* Add F-Curves to source Action. */
  BLI_addtail(&action_src.curves, this->fcurve_create("source_prop", 0));
  FCurve *fcurve_to_move = this->fcurve_create("source_prop", 2);
  BLI_addtail(&action_src.curves, fcurve_to_move);

  /* Add F-Curves to destination Action. */
  BLI_addtail(&action_dst.curves, this->fcurve_create("dest_prop", 0));

  ASSERT_TRUE(action_src.is_action_legacy());
  ASSERT_TRUE(action_dst.is_action_legacy());

  action_fcurve_move(action_dst, Slot::unassigned, action_src, *fcurve_to_move);

  EXPECT_TRUE(action_src.is_action_legacy());
  EXPECT_TRUE(action_dst.is_action_legacy());

  EXPECT_EQ(-1, BLI_findindex(&action_src.curves, fcurve_to_move))
      << "F-Curve should no longer exist in source Action";
  EXPECT_EQ(1, BLI_findindex(&action_dst.curves, fcurve_to_move))
      << "F-Curve should exist in destination Action";

  EXPECT_EQ(1, BLI_listbase_count(&action_src.curves))
      << "Source Action should still have the other F-Curve";
  EXPECT_EQ(2, BLI_listbase_count(&action_dst.curves))
      << "Destination Action should have its original and the moved F-Curve";
}

TEST_F(ActionFCurveMoveTest, test_fcurve_move_layered)
{
  Action &action_src = action_add(*this->bmain, "SourceAction");
  Action &action_dst = action_add(*this->bmain, "DestinationAction");

  /* Add F-Curves to source Action. */
  Slot &slot_src = action_src.slot_add();
  action_src.layer_keystrip_ensure();
  StripKeyframeData &strip_data_src = action_src.layer(0)->strip(0)->data<StripKeyframeData>(
      action_src);
  ChannelBag &cbag_src = strip_data_src.channelbag_for_slot_ensure(slot_src);

  cbag_src.fcurve_ensure(this->bmain, {"source_prop", 0});
  FCurve &fcurve_to_move = cbag_src.fcurve_ensure(this->bmain, {"source_prop", 2});
  bActionGroup &group_src = cbag_src.channel_group_create("Gröpje");
  cbag_src.fcurve_assign_to_channel_group(fcurve_to_move, group_src);

  /* Add F-Curves to destination Action. */
  Slot &slot_dst = action_dst.slot_add();
  action_dst.layer_keystrip_ensure();
  StripKeyframeData &strip_data_dst = action_dst.layer(0)->strip(0)->data<StripKeyframeData>(
      action_dst);
  ChannelBag &cbag_dst = strip_data_dst.channelbag_for_slot_ensure(slot_dst);

  cbag_dst.fcurve_ensure(this->bmain, {"dest_prop", 0});

  ASSERT_TRUE(action_src.is_action_layered());
  ASSERT_TRUE(action_dst.is_action_layered());

  action_fcurve_move(action_dst, slot_dst.handle, action_src, fcurve_to_move);

  EXPECT_TRUE(action_src.is_action_layered());
  EXPECT_TRUE(action_dst.is_action_layered());

  EXPECT_EQ(nullptr, cbag_src.fcurve_find({fcurve_to_move.rna_path, fcurve_to_move.array_index}))
      << "F-Curve should no longer exist in source Action";
  EXPECT_EQ(&fcurve_to_move,
            cbag_dst.fcurve_find({fcurve_to_move.rna_path, fcurve_to_move.array_index}))
      << "F-Curve should exist in destination Action";

  EXPECT_EQ(1, cbag_src.fcurves().size()) << "Source Action should still have the other F-Curve";
  EXPECT_EQ(2, cbag_dst.fcurves().size())
      << "Destination Action should have its original and the moved F-Curve";

  bActionGroup *group_dst = cbag_dst.channel_group_find("Gröpje");
  ASSERT_NE(nullptr, group_dst) << "Expected channel group to be created";
  ASSERT_EQ(group_dst, fcurve_to_move.grp) << "Expected group membership to move as well";
}

}  // namespace blender::animrig::tests
