/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ANIM_animation.hh"

#include "BKE_anim_data.hh"
#include "BKE_animation.hh"
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
class AnimationLayersTest : public testing::Test {
 public:
  Main *bmain;
  Animation *anim;
  Object *cube;
  Object *suzanne;

  static void SetUpTestSuite()
  {
    /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialised properly. */
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
    anim = static_cast<Animation *>(BKE_id_new(bmain, ID_AN, "ANÄnimåtië"));
    cube = BKE_object_add_only_object(bmain, OB_EMPTY, "Küüübus");
    suzanne = BKE_object_add_only_object(bmain, OB_EMPTY, "OBSuzanne");
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(AnimationLayersTest, add_layer)
{
  Layer &layer = anim->layer_add("layer name");

  EXPECT_EQ(anim->layer(0), &layer);
  EXPECT_EQ("layer name", std::string(layer.name));
  EXPECT_EQ(1.0f, layer.influence) << "Expected DNA defaults to be used.";
  EXPECT_EQ(0, anim->layer_active_index)
      << "Expected newly added layer to become the active layer.";
  ASSERT_EQ(0, layer.strips().size()) << "Expected newly added layer to have no strip.";
}

TEST_F(AnimationLayersTest, remove_layer)
{
  Layer &layer0 = anim->layer_add("Test Læür nul");
  Layer &layer1 = anim->layer_add("Test Læür één");
  Layer &layer2 = anim->layer_add("Test Læür twee");

  /* Add some strips to check that they are freed correctly too (implicitly by the
   * memory leak checker). */
  layer0.strip_add(Strip::Type::Keyframe);
  layer1.strip_add(Strip::Type::Keyframe);
  layer2.strip_add(Strip::Type::Keyframe);

  { /* Test removing a layer that is not owned. */
    Animation *other_anim = static_cast<Animation *>(BKE_id_new(bmain, ID_AN, "ANOtherAnim"));
    Layer &other_layer = other_anim->layer_add("Another Layer");
    EXPECT_FALSE(anim->layer_remove(other_layer))
        << "Removing a layer not owned by the animation should be gracefully rejected";
    BKE_id_free(bmain, &other_anim->id);
  }

  EXPECT_TRUE(anim->layer_remove(layer1));
  EXPECT_EQ(2, anim->layers().size());
  EXPECT_STREQ(layer0.name, anim->layer(0)->name);
  EXPECT_STREQ(layer2.name, anim->layer(1)->name);

  EXPECT_TRUE(anim->layer_remove(layer2));
  EXPECT_EQ(1, anim->layers().size());
  EXPECT_STREQ(layer0.name, anim->layer(0)->name);

  EXPECT_TRUE(anim->layer_remove(layer0));
  EXPECT_EQ(0, anim->layers().size());
}

TEST_F(AnimationLayersTest, add_strip)
{
  Layer &layer = anim->layer_add("Test Læür");

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
  Binding &binding = anim->binding_add();
  strip.as<KeyframeStrip>().keyframe_insert(binding, "location", 0, {1.0f, 47.0f}, settings);
  another_strip.as<KeyframeStrip>().keyframe_insert(
      binding, "location", 0, {1.0f, 47.0f}, settings);
}

TEST_F(AnimationLayersTest, remove_strip)
{
  Layer &layer = anim->layer_add("Test Læür");
  Strip &strip0 = layer.strip_add(Strip::Type::Keyframe);
  Strip &strip1 = layer.strip_add(Strip::Type::Keyframe);
  Strip &strip2 = layer.strip_add(Strip::Type::Keyframe);

  /* Add some keys to check that also the strip data is freed correctly. */
  const KeyframeSettings settings = get_keyframe_settings(false);
  Binding &binding = anim->binding_add();
  strip0.as<KeyframeStrip>().keyframe_insert(binding, "location", 0, {1.0f, 47.0f}, settings);
  strip1.as<KeyframeStrip>().keyframe_insert(binding, "location", 0, {1.0f, 47.0f}, settings);
  strip2.as<KeyframeStrip>().keyframe_insert(binding, "location", 0, {1.0f, 47.0f}, settings);

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
    Layer &other_layer = anim->layer_add("Another Layer");
    Strip &other_strip = other_layer.strip_add(Strip::Type::Keyframe);

    EXPECT_FALSE(layer.strip_remove(other_strip))
        << "Removing a strip not owned by the layer should be gracefully rejected";
  }
}

TEST_F(AnimationLayersTest, add_binding)
{
  Binding &binding = anim->binding_add();
  EXPECT_EQ(1, anim->last_binding_handle);
  EXPECT_EQ(1, binding.handle);

  EXPECT_STREQ("", binding.name);
  EXPECT_EQ(0, binding.idtype);

  EXPECT_TRUE(binding.connect_id(cube->id));
  EXPECT_STREQ("", binding.name)
      << "This low-level assignment function should not manipulate the Binding name";
  EXPECT_EQ(GS(cube->id.name), binding.idtype);
}

TEST_F(AnimationLayersTest, add_binding_multiple)
{
  Binding &out_cube = anim->binding_add();
  Binding &out_suzanne = anim->binding_add();
  EXPECT_TRUE(out_cube.connect_id(cube->id));
  EXPECT_TRUE(out_suzanne.connect_id(suzanne->id));

  EXPECT_EQ(2, anim->last_binding_handle);
  EXPECT_EQ(1, out_cube.handle);
  EXPECT_EQ(2, out_suzanne.handle);
}

TEST_F(AnimationLayersTest, anim_assign_id)
{
  /* Assign to the only, 'virgin' Binding, should always work. */
  Binding &out_cube = anim->binding_add();
  ASSERT_TRUE(anim->assign_id(&out_cube, cube->id));
  EXPECT_EQ(out_cube.handle, cube->adt->binding_handle);
  EXPECT_STREQ(out_cube.name, cube->id.name)
      << "The binding should be named after the assigned ID";
  EXPECT_STREQ(out_cube.name, cube->adt->binding_name)
      << "The binding name should be copied to the adt";

  /* Assign another ID to the same Binding. */
  ASSERT_TRUE(anim->assign_id(&out_cube, suzanne->id));
  EXPECT_STREQ(out_cube.name, cube->id.name)
      << "The binding should not be renamed on assignment once it has a name";
  EXPECT_STREQ(out_cube.name, cube->adt->binding_name)
      << "The binding name should be copied to the adt";

  /* Assign Cube to another binding without unassigning first. */
  Binding &another_out_cube = anim->binding_add();
  ASSERT_FALSE(anim->assign_id(&another_out_cube, cube->id))
      << "Assigning animation (with this function) when already assigned should fail.";

  /* Assign Cube to another 'virgin' binding. This should not cause a name
   * collision between the Bindings. */
  anim->unassign_id(cube->id);
  ASSERT_TRUE(anim->assign_id(&another_out_cube, cube->id));
  EXPECT_EQ(another_out_cube.handle, cube->adt->binding_handle);
  EXPECT_STREQ("OBKüüübus.001", another_out_cube.name) << "The binding should be uniquely named";
  EXPECT_STREQ("OBKüüübus.001", cube->adt->binding_name)
      << "The binding name should be copied to the adt";

  /* Create an ID of another type. This should not be assignable to this binding. */
  ID *mesh = static_cast<ID *>(BKE_id_new_nomain(ID_ME, "Mesh"));
  EXPECT_FALSE(anim->assign_id(&out_cube, *mesh))
      << "Mesh should not be animatable by an Object binding";
  BKE_id_free(nullptr, mesh);
}

TEST_F(AnimationLayersTest, rename_binding)
{
  Binding &out_cube = anim->binding_add();
  ASSERT_TRUE(anim->assign_id(&out_cube, cube->id));
  EXPECT_EQ(out_cube.handle, cube->adt->binding_handle);
  EXPECT_STREQ(out_cube.name, cube->id.name)
      << "The binding should be named after the assigned ID";
  EXPECT_STREQ(out_cube.name, cube->adt->binding_name)
      << "The binding name should be copied to the adt";

  anim->binding_name_define(out_cube, "New Binding Name");
  EXPECT_STREQ("New Binding Name", out_cube.name);
  /* At this point the binding name will not have been copied to the cube
   * AnimData. However, I don't want to test for that here, as it's not exactly
   * desirable behaviour, but more of a side-effect of the current
   * implementation. */

  anim->binding_name_propagate(*bmain, out_cube);
  EXPECT_STREQ("New Binding Name", cube->adt->binding_name);

  /* Finally, do another rename, do NOT call the propagate function, then
   * unassign. This should still result in the correct binding name being stored
   * on the ADT. */
  anim->binding_name_define(out_cube, "Even Newer Name");
  anim->unassign_id(cube->id);
  EXPECT_STREQ("Even Newer Name", cube->adt->binding_name);
}

TEST_F(AnimationLayersTest, rename_binding_name_collision)
{
  Binding &binding1 = anim->binding_add();
  Binding &binding2 = anim->binding_add();

  anim->binding_name_define(binding1, "New Binding Name");
  anim->binding_name_define(binding2, "New Binding Name");
  EXPECT_STREQ("New Binding Name", binding1.name);
  EXPECT_STREQ("New Binding Name.001", binding2.name);
}

TEST_F(AnimationLayersTest, find_suitable_binding)
{
  /* ===
   * Empty case, no bindings exist yet and the ID doesn't even have an AnimData. */
  EXPECT_EQ(nullptr, anim->find_suitable_binding_for(cube->id));

  /* ===
   * Binding exists with the same name & type as the ID, but the ID doesn't have any AnimData yet.
   * These should nevertheless be matched up. */
  Binding &binding = anim->binding_add();
  binding.handle = 327;
  STRNCPY_UTF8(binding.name, "OBKüüübus");
  binding.idtype = GS(cube->id.name);
  EXPECT_EQ(&binding, anim->find_suitable_binding_for(cube->id));

  /* ===
   * Binding exists with the same name & type as the ID, and the ID has an AnimData with the same
   * binding name, but a different binding_handle. Since the Animation has not yet been
   * assigned to this ID, the binding_handle should be ignored, and the binding name used for
   * matching. */

  /* Create a binding with a handle that should be ignored.*/
  Binding &other_out = anim->binding_add();
  other_out.handle = 47;

  AnimData *adt = BKE_animdata_ensure_id(&cube->id);
  adt->animation = nullptr;
  /* Configure adt to use the handle of one binding, and the name of the other. */
  adt->binding_handle = other_out.handle;
  STRNCPY_UTF8(adt->binding_name, binding.name);
  EXPECT_EQ(&binding, anim->find_suitable_binding_for(cube->id));

  /* ===
   * Same situation as above (AnimData has name of one binding, but the handle of another),
   * except that the animation data-block has already been assigned. In this case the handle
   * should take precedence. */
  adt->animation = anim;
  id_us_plus(&anim->id);
  EXPECT_EQ(&other_out, anim->find_suitable_binding_for(cube->id));

  /* ===
   * A binding exists, but doesn't match anything in the anim data of the cube. This should fall
   * back to using the ID name. */
  adt->binding_handle = 161;
  STRNCPY_UTF8(adt->binding_name, "¿¿What's this??");
  EXPECT_EQ(&binding, anim->find_suitable_binding_for(cube->id));
}

TEST_F(AnimationLayersTest, KeyframeStrip__keyframe_insert)
{
  Binding &binding = anim->binding_add();
  EXPECT_TRUE(binding.connect_id(cube->id));
  Layer &layer = anim->layer_add("Kübus layer");

  Strip &strip = layer.strip_add(Strip::Type::Keyframe);
  KeyframeStrip &key_strip = strip.as<KeyframeStrip>();

  const KeyframeSettings settings = get_keyframe_settings(false);
  FCurve *fcurve_loc_a = key_strip.keyframe_insert(
      binding, "location", 0, {1.0f, 47.0f}, settings);
  ASSERT_NE(nullptr, fcurve_loc_a)
      << "Expect all the necessary data structures to be created on insertion of a key";

  /* Check the strip was created correctly, with the channels for the binding. */
  ASSERT_EQ(1, key_strip.channelbags().size());
  ChannelBag *channels = key_strip.channelbag(0);
  EXPECT_EQ(binding.handle, channels->binding_handle);

  /* Insert a second key, should insert into the same FCurve as before. */
  FCurve *fcurve_loc_b = key_strip.keyframe_insert(
      binding, "location", 0, {5.0f, 47.1f}, settings);
  ASSERT_EQ(fcurve_loc_a, fcurve_loc_b)
      << "Expect same (binding/rna path/array index) tuple to return the same FCurve.";
  EXPECT_EQ(2, fcurve_loc_b->totvert);

  /* Insert another key for another property, should create another FCurve. */
  FCurve *fcurve_rot = key_strip.keyframe_insert(
      binding, "rotation_quaternion", 0, {1.0f, 0.25f}, settings);
  EXPECT_NE(fcurve_loc_b, fcurve_rot)
      << "Expected rotation and location curves to be different FCurves.";
  EXPECT_EQ(2, channels->fcurves().size()) << "Expected a second FCurve to be created.";
}

}  // namespace blender::animrig::tests
