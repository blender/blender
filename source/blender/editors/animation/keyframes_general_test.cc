/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "keyframes_general_intern.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_armature.hh"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"

#include "ED_keyframes_edit.hh"

using namespace blender::animrig;

namespace blender::ed::animation::tests {

namespace {

/* std::unique_ptr for FCurve. */
struct fcurve_deleter {
  void operator()(FCurve *fcurve) const
  {
    /* If this F-Curve was registered as "bone", remove it from that registration as well. */
    keyframe_copy_buffer->bone_fcurves.remove(fcurve);

    BKE_fcurve_free(fcurve);
  }
};
using FCurvePtr = std::unique_ptr<FCurve, fcurve_deleter>;

/**
 * Create a "fake" F-Curve. It does not belong to any Action, and has no keys.
 * It just has its RNA path and array index set.
 */
FCurvePtr fake_fcurve(const char *rna_path, const int array_index)
{
  FCurve *fcurve = BKE_fcurve_create();

  if (rna_path) {
    fcurve->rna_path = BLI_strdup(rna_path);
  }
  fcurve->array_index = array_index;

  return FCurvePtr(fcurve);
}

/**
 * Create a fake F-Curve (see above), and pretend it's been added to the copy buffer.
 *
 * This doesn't really add the F-Curve to the copy buffer, but rather just manipulates
 * `keyframe_copy_buffer->bone_fcurves` and `keyframe_copy_buffer->slot_animated_ids` so that the
 * F-Curve matching functions can do their work.
 */
FCurvePtr fake_fcurve_in_buffer(const char *rna_path,
                                const int array_index,
                                const bool is_bone,
                                const slot_handle_t slot_handle = Slot::unassigned,
                                ID *owner_id = nullptr)
{
  FCurvePtr fcurve_ptr = fake_fcurve(rna_path, array_index);

  if (is_bone) {
    keyframe_copy_buffer->bone_fcurves.add(fcurve_ptr.get());
  }

  if (owner_id) {
    keyframe_copy_buffer->slot_animated_ids.add_overwrite(slot_handle, owner_id);
  }
  return fcurve_ptr;
}

}  // namespace

/**
 * Keyframe pasting test suite.
 *
 * Currently this just tests the name flipping & F-Curve matching, and not the actual copy-pasting.
 */
struct keyframes_paste : public testing::Test {
  static void SetUpTestSuite()
  {
    ANIM_fcurves_copybuf_reset();
  }

  static void TearDownTestSuite()
  {
    ANIM_fcurves_copybuf_free();
  }
};

TEST_F(keyframes_paste, flip_names)
{
  EXPECT_EQ(std::nullopt, flip_names("whatever")) << "not a bone prefix";

  EXPECT_EQ("pose.bones[\"head\"]", flip_names("pose.bones[\"head\"]"))
      << "unflippable name should remain unchanged";
  EXPECT_EQ("pose.bones[\"Arm_L\"]", flip_names("pose.bones[\"Arm_R\"]"))
      << "flippable name should be flipped";

  EXPECT_EQ("pose.bones[\"Arm_L\"].rotation_euler",
            flip_names("pose.bones[\"Arm_R\"].rotation_euler"))
      << "flippable name should be flipped";
}

TEST_F(keyframes_paste, pastebuf_match_path_full)
{
  constexpr slot_handle_t unassigned = Slot::unassigned;

  { /* NULL RNA paths. */
    ANIM_fcurves_copybuf_reset();
    FCurvePtr fcurve_target = fake_fcurve(nullptr, 0);
    FCurvePtr fcurve_in_buffer = fake_fcurve_in_buffer(nullptr, 0, false);

    /* Little wrapper for #pastebuf_match_path_full() to make it easier to see
     * the differences between the test-cases. */
    auto call = [&](const bool from_single, const bool to_single, const bool flip) {
      return pastebuf_match_path_full(
          nullptr, *fcurve_target, *fcurve_in_buffer, unassigned, from_single, to_single, flip);
    };

    /* This only matches when `to_single` is true. */
    EXPECT_FALSE(call(false, false, false));
    EXPECT_FALSE(call(false, false, true));
    EXPECT_TRUE(call(false, true, false));
    EXPECT_TRUE(call(false, true, true));
    EXPECT_FALSE(call(true, false, false));
    EXPECT_FALSE(call(true, false, true));
    EXPECT_TRUE(call(true, true, false));
    EXPECT_TRUE(call(true, true, true));
  }

  { /* Many to many, no flipping. */
    ANIM_fcurves_copybuf_reset();
    FCurvePtr fcurve = fake_fcurve("location", 0);

    EXPECT_TRUE(pastebuf_match_path_full(nullptr,
                                         *fcurve,
                                         *fake_fcurve_in_buffer("location", 0, false),
                                         unassigned,
                                         false,
                                         false,
                                         false));
    EXPECT_FALSE(pastebuf_match_path_full(nullptr,
                                          *fcurve,
                                          *fake_fcurve_in_buffer("location", 1, false),
                                          unassigned,
                                          false,
                                          false,
                                          false))
        << "array index mismatch";
    EXPECT_FALSE(pastebuf_match_path_full(nullptr,
                                          *fcurve,
                                          *fake_fcurve_in_buffer("rotation_euler", 0, false),
                                          unassigned,
                                          false,
                                          false,
                                          false))
        << "rna path mismatch";
  }

  /* Many to many, Flipping bone names. */
  {
    ANIM_fcurves_copybuf_reset();
    const bool from_single = false;
    const bool to_single = false;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";
    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, false),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is NOT bone";
    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, false),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is NOT bone";

    EXPECT_FALSE(pastebuf_match_path_full(nullptr,
                                          *fcurve,
                                          *fake_fcurve_in_buffer("location", 0, false),
                                          unassigned,
                                          from_single,
                                          to_single,
                                          flip))
        << "rna path mismatch";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, but array index mismatch";
  }

  /* Many to single (so only array index matters), Flipping bone names requested (but won't happen
   * because 'to single'). */
  {
    ANIM_fcurves_copybuf_reset();
    const bool from_single = false;
    const bool to_single = true;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";
    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_full(nullptr,
                                         *fcurve,
                                         *fake_fcurve_in_buffer("location", 0, false),
                                         unassigned,
                                         from_single,
                                         to_single,
                                         flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"nose\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, but array index mismatch";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, but array index mismatch";
  }

  { /* Single (so array indices won't matter) to Many, Flipping bone names requested. */
    ANIM_fcurves_copybuf_reset();
    const bool from_single = true;
    const bool to_single = false;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";
    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_full(nullptr,
                                          *fcurve,
                                          *fake_fcurve_in_buffer("location", 0, false),
                                          unassigned,
                                          from_single,
                                          to_single,
                                          flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"nose\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, but array index mismatch";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, but array index mismatch";
  }

  {
    /* Single (so array indices won't matter) to Many, NOT flipping bone names. */
    ANIM_fcurves_copybuf_reset();
    const bool from_single = true;
    const bool to_single = false;
    const bool flip = false;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";
    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_full(nullptr,
                                          *fcurve,
                                          *fake_fcurve_in_buffer("location", 0, false),
                                          unassigned,
                                          from_single,
                                          to_single,
                                          flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"nose\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, but array index mismatch";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, but array index mismatch";
  }

  /* Single to Single (so nothing should matter), Flipping bone names requested. */
  {
    ANIM_fcurves_copybuf_reset();
    const bool from_single = true;
    const bool to_single = true;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";
    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_full(nullptr,
                                         *fcurve,
                                         *fake_fcurve_in_buffer("location", 0, false),
                                         unassigned,
                                         from_single,
                                         to_single,
                                         flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"nose\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.R\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, but array index mismatch";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr,
        *fcurve,
        *fake_fcurve_in_buffer("pose.bones[\"hand.L\"].location", 1, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, but array index mismatch";
  }
}

TEST_F(keyframes_paste, pastebuf_match_path_property)
{
  constexpr slot_handle_t unassigned = Slot::unassigned;

  Main *bmain = BKE_main_new();
  ID *arm_ob_id;

  { /* Set up an armature, to test matching on property names. */
    BKE_idtype_init();

    bArmature *armature = BKE_armature_add(bmain, "Armature");
    for (const auto &bone_name : {"hand.L", "hand.R", "middle"}) {
      Bone *bone = static_cast<Bone *>(MEM_mallocN(sizeof(Bone), __func__));
      memset(bone, 0, sizeof(Bone));
      STRNCPY(bone->name, bone_name);
      BLI_addtail(&armature->bonebase, bone);
    }

    Object *armature_object = BKE_object_add_only_object(bmain, OB_ARMATURE, "Armature");
    armature_object->data = armature;
    BKE_pose_ensure(bmain, armature_object, armature, false);

    arm_ob_id = &armature_object->id;
  }

  /* Wrapper function to create an F-Curve in the copy buffer, animating the armature object. */
  const auto fake_armob_fcurve =
      [&](const char *rna_path, const int array_index, const bool is_bone) {
        return fake_fcurve_in_buffer(rna_path, array_index, is_bone, unassigned, arm_ob_id);
      };

  { /* From Single Channel, so array indices are ignored. */
    ANIM_fcurves_copybuf_reset();
    const bool from_single = true;
    const bool to_single = false; /* Doesn't matter, function under test doesn't use this. */
    const bool flip = false;      /* Doesn't matter, function under test doesn't use this. */

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 2, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, other array index";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "same bone, other property";

    EXPECT_FALSE(pastebuf_match_path_property(bmain,
                                              *fcurve,
                                              *fake_armob_fcurve("rotation_euler", 0, false),
                                              unassigned,
                                              from_single,
                                              to_single,
                                              flip))
        << "other struct, same property name";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"missing\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "nonexistent bone, but same property name";

    /* This just tests the current functionality. This may not necessarily be
     * correct / desired behavior. */
    FCurvePtr fcurve_with_long_rna_path = fake_fcurve(
        "pose.bones[\"hand.L\"].weirdly_long_location", 0);
    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "property name suffix-match";
  }

  { /* From Multiple Channels, so array indices matter. */
    ANIM_fcurves_copybuf_reset();
    const bool from_single = false;
    const bool to_single = false; /* Doesn't matter, function under test doesn't use this. */
    const bool flip = false;      /* Doesn't matter, function under test doesn't use this. */

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.R\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 2, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "original path match, other array index";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].rotation_euler", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "same bone, other property";

    EXPECT_FALSE(pastebuf_match_path_property(bmain,
                                              *fcurve,
                                              *fake_armob_fcurve("rotation_euler", 0, false),
                                              unassigned,
                                              from_single,
                                              to_single,
                                              flip))
        << "other struct, same property name";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"missing\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "nonexistent bone, but same property name";

    /* This just tests the current functionality. This may not necessarily be
     * correct / desired behavior. */
    FCurvePtr fcurve_with_long_rna_path = fake_fcurve(
        "pose.bones[\"hand.L\"].weirdly_long_location", 0);
    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_armob_fcurve("pose.bones[\"hand.L\"].location", 0, true),
        unassigned,
        from_single,
        to_single,
        flip))
        << "property name suffix-match";
  }

  { /* Resilience against deleted IDs. */
    ANIM_fcurves_copybuf_reset();
    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);
    Object *object_not_in_main = BKE_object_add_only_object(nullptr, OB_EMPTY, "non-main");

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        *fcurve,
        *fake_fcurve_in_buffer(
            "pose.bones[\"hand.L\"].location", 0, true, unassigned, &object_not_in_main->id),
        unassigned,
        false,
        false,
        false))
        << "copying from deleted ID";

    BKE_id_free(nullptr, &object_not_in_main->id);
  }

  BKE_main_free(bmain);
}

TEST_F(keyframes_paste, pastebuf_match_index_only)
{
  constexpr slot_handle_t unassigned = Slot::unassigned;
  ANIM_fcurves_copybuf_reset();
  FCurvePtr fcurve = fake_fcurve("some_prop", 1);

  EXPECT_TRUE(pastebuf_match_index_only(nullptr,
                                        *fcurve,
                                        *fake_fcurve_in_buffer("location", 1, false),
                                        unassigned,
                                        false,
                                        false,
                                        false));
  EXPECT_FALSE(pastebuf_match_index_only(nullptr,
                                         *fcurve,
                                         *fake_fcurve_in_buffer("location", 2, false),
                                         unassigned,
                                         false,
                                         false,
                                         false));
}

}  // namespace blender::ed::animation::tests
