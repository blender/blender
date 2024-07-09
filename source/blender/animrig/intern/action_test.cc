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

#include "BLI_listbase.h"
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
  layer0.strip_add(Strip::Type::Keyframe);
  layer1.strip_add(Strip::Type::Keyframe);
  layer2.strip_add(Strip::Type::Keyframe);

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

  Strip &strip = layer.strip_add(Strip::Type::Keyframe);
  ASSERT_EQ(1, layer.strips().size());
  EXPECT_EQ(&strip, layer.strip(0));

  constexpr float inf = std::numeric_limits<float>::infinity();
  EXPECT_EQ(-inf, strip.frame_start) << "Expected strip to be infinite.";
  EXPECT_EQ(inf, strip.frame_end) << "Expected strip to be infinite.";
  EXPECT_EQ(0, strip.frame_offset) << "Expected infinite strip to have no offset.";

  Strip &another_strip = layer.strip_add(Strip::Type::Keyframe);
  ASSERT_EQ(2, layer.strips().size());
  EXPECT_EQ(&another_strip, layer.strip(1));

  EXPECT_EQ(-inf, another_strip.frame_start) << "Expected strip to be infinite.";
  EXPECT_EQ(inf, another_strip.frame_end) << "Expected strip to be infinite.";
  EXPECT_EQ(0, another_strip.frame_offset) << "Expected infinite strip to have no offset.";

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Slot &slot = action->slot_add();
  strip.as<KeyframeStrip>().keyframe_insert(slot, {"location", 0}, {1.0f, 47.0f}, settings);
  another_strip.as<KeyframeStrip>().keyframe_insert(
      slot, {"location", 0}, {1.0f, 47.0f}, settings);
}

TEST_F(ActionLayersTest, remove_strip)
{
  Layer &layer = action->layer_add("Test Læür");
  Strip &strip0 = layer.strip_add(Strip::Type::Keyframe);
  Strip &strip1 = layer.strip_add(Strip::Type::Keyframe);
  Strip &strip2 = layer.strip_add(Strip::Type::Keyframe);

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Slot &slot = action->slot_add();
  strip0.as<KeyframeStrip>().keyframe_insert(slot, {"location", 0}, {1.0f, 47.0f}, settings);
  strip1.as<KeyframeStrip>().keyframe_insert(slot, {"location", 0}, {1.0f, 47.0f}, settings);
  strip2.as<KeyframeStrip>().keyframe_insert(slot, {"location", 0}, {1.0f, 47.0f}, settings);

  EXPECT_TRUE(layer.strip_remove(strip1));
  EXPECT_EQ(2, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));
  EXPECT_EQ(&strip2, layer.strip(1));

  EXPECT_TRUE(layer.strip_remove(strip2));
  EXPECT_EQ(1, layer.strips().size());
  EXPECT_EQ(&strip0, layer.strip(0));

  EXPECT_TRUE(layer.strip_remove(strip0));
  EXPECT_EQ(0, layer.strips().size());

  { /* Test removing a strip that is not owned. */
    Layer &other_layer = action->layer_add("Another Layer");
    Strip &other_strip = other_layer.strip_add(Strip::Type::Keyframe);

    EXPECT_FALSE(layer.strip_remove(other_strip))
        << "Removing a strip not owned by the layer should be gracefully rejected";
  }
}

TEST_F(ActionLayersTest, add_remove_strip_of_concrete_type)
{
  Layer &layer = action->layer_add("Test Læür");
  KeyframeStrip &key_strip = layer.strip_add<KeyframeStrip>();

  /* key_strip is of type KeyframeStrip, but should be implicitly converted to a
   * Strip reference. */
  EXPECT_TRUE(layer.strip_remove(key_strip));
}

TEST_F(ActionLayersTest, add_slot)
{
  { /* Creating an 'unused' Slot should just be called 'Slot'. */
    Slot &slot = action->slot_add();
    EXPECT_EQ(1, action->last_slot_handle);
    EXPECT_EQ(1, slot.handle);

    EXPECT_STREQ("XXSlot", slot.name);
    EXPECT_EQ(0, slot.idtype);
  }

  { /* Creating a Slot for a specific ID should name it after the ID. */
    Slot &slot = action->slot_add_for_id(cube->id);
    EXPECT_EQ(2, action->last_slot_handle);
    EXPECT_EQ(2, slot.handle);

    EXPECT_STREQ(cube->id.name, slot.name);
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
  Slot &bind_cube = action->slot_add();
  Slot &bind_suzanne = action->slot_add();
  EXPECT_TRUE(action->assign_id(&bind_cube, cube->id));
  EXPECT_TRUE(action->assign_id(&bind_suzanne, suzanne->id));

  EXPECT_EQ(2, action->last_slot_handle);
  EXPECT_EQ(1, bind_cube.handle);
  EXPECT_EQ(2, bind_suzanne.handle);
}

TEST_F(ActionLayersTest, action_assign_id)
{
  /* Assign to the only, 'virgin' Slot, should always work. */
  Slot &slot_cube = action->slot_add();
  ASSERT_NE(nullptr, slot_cube.runtime);
  ASSERT_STREQ(slot_cube.name, "XXSlot");
  ASSERT_TRUE(action->assign_id(&slot_cube, cube->id));
  EXPECT_EQ(slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ(slot_cube.name, "OBSlot");
  EXPECT_STREQ(slot_cube.name, cube->adt->slot_name)
      << "The slot name should be copied to the adt";

  EXPECT_TRUE(slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Cube to be registered as animated by its slot.";

  /* Assign another ID to the same Slot. */
  ASSERT_TRUE(action->assign_id(&slot_cube, suzanne->id));
  EXPECT_STREQ(slot_cube.name, "OBSlot");
  EXPECT_STREQ(slot_cube.name, cube->adt->slot_name)
      << "The slot name should be copied to the adt";

  EXPECT_TRUE(slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Suzanne to be registered as animated by the Cube slot.";

  { /* Assign Cube to another action+slot without unassigning first. */
    Action *another_anim = static_cast<Action *>(BKE_id_new(bmain, ID_AC, "ACOtherAnim"));
    Slot &another_slot = another_anim->slot_add();
    ASSERT_FALSE(another_anim->assign_id(&another_slot, cube->id))
        << "Assigning Action (with this function) when already assigned should fail.";
    EXPECT_TRUE(slot_cube.users(*bmain).contains(&cube->id))
        << "Expecting Cube to still be registered as animated by its slot.";
  }

  { /* Assign Cube to another slot of the same Action, this should work. */
    const int user_count_pre = action->id.us;
    Slot &slot_cube_2 = action->slot_add();
    ASSERT_TRUE(action->assign_id(&slot_cube_2, cube->id));
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
    action->unassign_id(cube->id);
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
  ASSERT_TRUE(action->assign_id(&another_slot_cube, cube->id));
  EXPECT_EQ(another_slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ("OBSlot.002", another_slot_cube.name) << "The slot should be uniquely named";
  EXPECT_STREQ("OBSlot.002", cube->adt->slot_name) << "The slot name should be copied to the adt";
  EXPECT_TRUE(another_slot_cube.users(*bmain).contains(&cube->id))
      << "Expecting Cube to be registered as animated by the 'another_slot_cube' slot.";

  /* Create an ID of another type. This should not be assignable to this slot. */
  ID *mesh = static_cast<ID *>(BKE_id_new_nomain(ID_ME, "Mesh"));
  EXPECT_FALSE(action->assign_id(&slot_cube, *mesh))
      << "Mesh should not be animatable by an Object slot";
  EXPECT_FALSE(another_slot_cube.users(*bmain).contains(mesh))
      << "Expecting Mesh to not be registered as animated by the 'slot_cube' slot.";
  BKE_id_free(nullptr, mesh);
}

TEST_F(ActionLayersTest, rename_slot)
{
  Slot &slot_cube = action->slot_add();
  ASSERT_TRUE(action->assign_id(&slot_cube, cube->id));
  EXPECT_EQ(slot_cube.handle, cube->adt->slot_handle);
  EXPECT_STREQ("OBSlot", slot_cube.name);
  EXPECT_STREQ(slot_cube.name, cube->adt->slot_name)
      << "The slot name should be copied to the adt";

  action->slot_name_define(slot_cube, "New Slot Name");
  EXPECT_STREQ("New Slot Name", slot_cube.name);
  /* At this point the slot name will not have been copied to the cube
   * AnimData. However, I don't want to test for that here, as it's not exactly
   * desirable behavior, but more of a side-effect of the current
   * implementation. */

  action->slot_name_propagate(*bmain, slot_cube);
  EXPECT_STREQ("New Slot Name", cube->adt->slot_name);

  /* Finally, do another rename, do NOT call the propagate function, then
   * unassign. This should still result in the correct slot name being stored
   * on the ADT. */
  action->slot_name_define(slot_cube, "Even Newer Name");
  action->unassign_id(cube->id);
  EXPECT_STREQ("Even Newer Name", cube->adt->slot_name);
}

TEST_F(ActionLayersTest, slot_name_ensure_prefix)
{
  class AccessibleSlot : public Slot {
   public:
    void name_ensure_prefix()
    {
      Slot::name_ensure_prefix();
    }
  };

  Slot &raw_slot = action->slot_add();
  AccessibleSlot &slot = static_cast<AccessibleSlot &>(raw_slot);
  ASSERT_STREQ("XXSlot", slot.name);
  ASSERT_EQ(0, slot.idtype);

  /* Check defaults, idtype zeroed. */
  slot.name_ensure_prefix();
  EXPECT_STREQ("XXSlot", slot.name);

  /* idtype CA, default name.  */
  slot.idtype = ID_CA;
  slot.name_ensure_prefix();
  EXPECT_STREQ("CASlot", slot.name);

  /* idtype ME, explicit name of other idtype. */
  action->slot_name_define(slot, "CANewName");
  slot.idtype = ID_ME;
  slot.name_ensure_prefix();
  EXPECT_STREQ("MENewName", slot.name);

  /* Zeroing out idtype. */
  slot.idtype = 0;
  slot.name_ensure_prefix();
  EXPECT_STREQ("XXNewName", slot.name);
}

TEST_F(ActionLayersTest, slot_name_prefix)
{
  Slot &slot = action->slot_add();
  EXPECT_EQ("XX", slot.name_prefix_for_idtype());

  slot.idtype = ID_CA;
  EXPECT_EQ("CA", slot.name_prefix_for_idtype());
}

TEST_F(ActionLayersTest, rename_slot_name_collision)
{
  Slot &slot1 = action->slot_add();
  Slot &slot2 = action->slot_add();

  action->slot_name_define(slot1, "New Slot Name");
  action->slot_name_define(slot2, "New Slot Name");
  EXPECT_STREQ("New Slot Name", slot1.name);
  EXPECT_STREQ("New Slot Name.001", slot2.name);
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
  STRNCPY_UTF8(slot.name, "OBKüüübus");
  slot.idtype = GS(cube->id.name);
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * Slot exists with the same name & type as the ID, and the ID has an AnimData with the same
   * slot name, but a different slot_handle. Since the Action has not yet been
   * assigned to this ID, the slot_handle should be ignored, and the slot name used for
   * matching. */

  /* Create a slot with a handle that should be ignored.*/
  Slot &other_slot = action->slot_add();
  other_slot.handle = 47;

  AnimData *adt = BKE_animdata_ensure_id(&cube->id);
  adt->action = nullptr;
  /* Configure adt to use the handle of one slot, and the name of the other. */
  adt->slot_handle = other_slot.handle;
  STRNCPY_UTF8(adt->slot_name, slot.name);
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * Same situation as above (AnimData has name of one slot, but the handle of another),
   * except that the Action has already been assigned. In this case the handle should take
   * precedence. */
  adt->action = action;
  id_us_plus(&action->id);
  EXPECT_EQ(&other_slot, action->find_suitable_slot_for(cube->id));

  /* ===
   * A slot exists, but doesn't match anything in the action data of the cube. This should fall
   * back to using the ID name. */
  adt->slot_handle = 161;
  STRNCPY_UTF8(adt->slot_name, "¿¿What's this??");
  EXPECT_EQ(&slot, action->find_suitable_slot_for(cube->id));
}

TEST_F(ActionLayersTest, strip)
{
  constexpr float inf = std::numeric_limits<float>::infinity();
  Layer &layer0 = action->layer_add("Test Læür nul");
  Strip &strip = layer0.strip_add(Strip::Type::Keyframe);

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
  EXPECT_TRUE(action->assign_id(&slot, cube->id));
  Layer &layer = action->layer_add("Kübus layer");

  Strip &strip = layer.strip_add(Strip::Type::Keyframe);
  KeyframeStrip &key_strip = strip.as<KeyframeStrip>();

  const KeyframeSettings settings = get_keyframe_settings(false);
  SingleKeyingResult result_loc_a = key_strip.keyframe_insert(
      slot, {"location", 0}, {1.0f, 47.0f}, settings);
  ASSERT_EQ(SingleKeyingResult::SUCCESS, result_loc_a)
      << "Expected keyframe insertion to be successful";

  /* Check the strip was created correctly, with the channels for the slot. */
  ASSERT_EQ(1, key_strip.channelbags().size());
  ChannelBag *channels = key_strip.channelbag(0);
  EXPECT_EQ(slot.handle, channels->slot_handle);

  /* Insert a second key, should insert into the same FCurve as before. */
  SingleKeyingResult result_loc_b = key_strip.keyframe_insert(
      slot, {"location", 0}, {5.0f, 47.1f}, settings);
  EXPECT_EQ(SingleKeyingResult::SUCCESS, result_loc_b);
  ASSERT_EQ(1, channels->fcurves().size()) << "Expect insertion with the same (slot/rna "
                                              "path/array index) tuple to go into the same FCurve";
  EXPECT_EQ(2, channels->fcurves()[0]->totvert)
      << "Expect insertion with the same (slot/rna path/array index) tuple to go into the same "
         "FCurve";

  EXPECT_EQ(47.0f, evaluate_fcurve(channels->fcurves()[0], 1.0f));
  EXPECT_EQ(47.1f, evaluate_fcurve(channels->fcurves()[0], 5.0f));

  /* Insert another key for another property, should create another FCurve. */
  SingleKeyingResult result_rot = key_strip.keyframe_insert(
      slot, {"rotation_quaternion", 0}, {1.0f, 0.25f}, settings);
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
  action->assign_id(nullptr, cube->id);

  /* Double-check that the action is considered empty for the test. */
  EXPECT_TRUE(action->is_empty());

  /* A `primary_id` that uses the action should get returned. Every other case
   * should return nullptr. */
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, 0, &cube->id));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, 0, &suzanne->id));
}

TEST_F(ActionLayersTest, action_slot_get_id_for_keying__legacy_action)
{
  FCurve *fcurve = action_fcurve_ensure(bmain, action, nullptr, nullptr, {"location", 0});
  EXPECT_FALSE(fcurve == nullptr);

  action->assign_id(nullptr, cube->id);

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
  action->assign_id(&slot, cube->id);
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, nullptr));
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &cube->id));
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &suzanne->id));

  /* A slot with more than one user should return the passed `primary_id` if it
   * is among its users, and nullptr otherwise. */
  action->assign_id(&slot, suzanne->id);
  EXPECT_EQ(&cube->id, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &cube->id));
  EXPECT_EQ(&suzanne->id,
            action_slot_get_id_for_keying(*bmain, *action, slot.handle, &suzanne->id));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, nullptr));
  EXPECT_EQ(nullptr, action_slot_get_id_for_keying(*bmain, *action, slot.handle, &bob->id));
}

}  // namespace blender::animrig::tests
