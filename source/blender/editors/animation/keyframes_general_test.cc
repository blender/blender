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

namespace blender::editor::animation::tests {

namespace {

/* std::unique_ptr for FCurve. */
struct fcurve_deleter {
  void operator()(FCurve *fcurve) const
  {
    BKE_fcurve_free(fcurve);
  }
};
using FCurvePtr = std::unique_ptr<FCurve, fcurve_deleter>;

FCurvePtr fake_fcurve(const char *rna_path, const int array_index)
{
  FCurve *fcurve = BKE_fcurve_create();

  if (rna_path) {
    fcurve->rna_path = BLI_strdup(rna_path);
  }
  fcurve->array_index = array_index;

  return FCurvePtr(fcurve);
}

/* std::unique_ptr for tAnimCopybufItem. */
struct copybuf_item_deleter {
  void operator()(tAnimCopybufItem *aci) const
  {
    tAnimCopybufItem_free(aci);
  }
};
using CopyBufItemPtr = std::unique_ptr<tAnimCopybufItem, copybuf_item_deleter>;

CopyBufItemPtr fake_aci(const char *rna_path, const int array_index, const bool is_bone)
{
  CopyBufItemPtr item(
      static_cast<tAnimCopybufItem *>(MEM_callocN(sizeof(tAnimCopybufItem), __func__)));

  if (rna_path) {
    item->rna_path = BLI_strdup(rna_path);
  }
  item->array_index = array_index;
  item->is_bone = is_bone;

  return item;
}

CopyBufItemPtr fake_aci_owned(const char *rna_path,
                              const int array_index,
                              const bool is_bone,
                              ID *owner)
{
  CopyBufItemPtr aci = fake_aci(rna_path, array_index, is_bone);

  aci->id = owner;
  aci->id_type = GS(owner->name);

  return aci;
}

}  // namespace

TEST(keyframes_paste, flip_names)
{
  EXPECT_EQ(std::nullopt, flip_names(fake_aci("whatever", 1, false).get())) << "is_bone is false";
  EXPECT_EQ(std::nullopt, flip_names(fake_aci("whatever", 1, true).get())) << "not a bone prefix";

  EXPECT_EQ("pose.bones[\"head\"]", flip_names(fake_aci("pose.bones[\"head\"]", 1, true).get()))
      << "unflippable name should remain unchanged";
  EXPECT_EQ("pose.bones[\"Arm_L\"]", flip_names(fake_aci("pose.bones[\"Arm_R\"]", 1, true).get()))
      << "flippable name should be flipped";

  EXPECT_EQ("pose.bones[\"Arm_L\"].rotation_euler",
            flip_names(fake_aci("pose.bones[\"Arm_R\"].rotation_euler", 1, true).get()))
      << "flippable name should be flipped";
}

TEST(keyframes_paste, pastebuf_match_path_full)
{
  { /* NULL RNA paths. */
    FCurvePtr fcurve = fake_fcurve(nullptr, 0);
    tAnimCopybufItem aci{};

    /* This only matches when `to_simple` is true. */
    EXPECT_FALSE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, false, false, false));
    EXPECT_FALSE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, false, false, true));
    EXPECT_TRUE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, false, true, false));
    EXPECT_TRUE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, false, true, true));
    EXPECT_FALSE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, true, false, false));
    EXPECT_FALSE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, true, false, true));
    EXPECT_TRUE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, true, true, false));
    EXPECT_TRUE(pastebuf_match_path_full(nullptr, fcurve.get(), &aci, true, true, true));
  }

  { /* Many to many, no flipping. */
    FCurvePtr fcurve = fake_fcurve("location", 0);

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), false, false, false));
    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 1, false).get(), false, false, false))
        << "array index mismatch";
    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("rotation_euler", 0, false).get(), false, false, false))
        << "rna path mismatch";
  }

  /* Many to many, Flipping bone names. */
  {
    const bool from_single = false;
    const bool to_simple = false;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is bone";
    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, false).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is NOT bone";
    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, false).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is NOT bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), from_single, to_simple, flip))
        << "rna path mismatch";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, but array index mismatch";
  }

  /* Many to single (so only array index matters), Flipping bone names requested (but won't happen
   * because 'to single'). */
  {
    const bool from_single = false;
    const bool to_simple = true;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is bone";
    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), from_single, to_simple, flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"nose\"].rotation_euler", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, but array index mismatch";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, but array index mismatch";
  }

  /* Single (so array indices won't matter) to Many, Flipping bone names requested. */
  {
    const bool from_single = true;
    const bool to_simple = false;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is bone";
    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), from_single, to_simple, flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"nose\"].rotation_euler", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, but array index mismatch";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, but array index mismatch";
  }

  /* Single (so array indices won't matter) to Many, NOT flipping bone names. */
  {
    const bool from_single = true;
    const bool to_simple = false;
    const bool flip = false;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is bone";
    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), from_single, to_simple, flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"nose\"].rotation_euler", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, but array index mismatch";

    EXPECT_FALSE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, but array index mismatch";
  }

  /* Single to Single (so nothing should matter), Flipping bone names requested. */
  {
    const bool from_single = true;
    const bool to_simple = true;
    const bool flip = true;

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, is bone";
    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_full(
        nullptr, fcurve.get(), fake_aci("location", 0, false).get(), from_single, to_simple, flip))
        << "rna path mismatch, ACI is NOT bone";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"nose\"].rotation_euler", 0, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "rna path mismatch, ACI is bone";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.R\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "flipped path match, but array index mismatch";

    EXPECT_TRUE(
        pastebuf_match_path_full(nullptr,
                                 fcurve.get(),
                                 fake_aci("pose.bones[\"hand.L\"].location", 1, true).get(),
                                 from_single,
                                 to_simple,
                                 flip))
        << "original path match, but array index mismatch";
  }
}

TEST(keyframes_paste, pastebuf_match_path_property)
{
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

  { /* From Single Channel, so array indices are ignored. */
    const bool from_single = true;
    const bool to_simple = false; /* Doesn't matter, function under test doesn't use this. */
    const bool flip = false;      /* Doesn't matter, function under test doesn't use this. */

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "original path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.R\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "flipped path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 2, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "original path match, other array index";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].rotation_euler", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "same bone, other property";

    EXPECT_FALSE(
        pastebuf_match_path_property(bmain,
                                     fcurve.get(),
                                     fake_aci_owned("rotation_euler", 0, false, arm_ob_id).get(),
                                     from_single,
                                     to_simple,
                                     flip))
        << "other struct, same property name";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"missing\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "nonexistent bone, but same property name";

    /* This just tests the current functionality. This may not necessarily be
     * correct / desired behavior. */
    FCurvePtr fcurve_with_long_rna_path = fake_fcurve(
        "pose.bones[\"hand.L\"].weirdly_long_location", 0);
    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "property name suffix-match";
  }

  { /* From Multiple Channels, so array indices matter. */
    const bool from_single = false;
    const bool to_simple = false; /* Doesn't matter, function under test doesn't use this. */
    const bool flip = false;      /* Doesn't matter, function under test doesn't use this. */

    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "original path match, is bone";

    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.R\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "flipped path match, is bone";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 2, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "original path match, other array index";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].rotation_euler", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "same bone, other property";

    EXPECT_FALSE(
        pastebuf_match_path_property(bmain,
                                     fcurve.get(),
                                     fake_aci_owned("rotation_euler", 0, false, arm_ob_id).get(),
                                     from_single,
                                     to_simple,
                                     flip))
        << "other struct, same property name";

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"missing\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "nonexistent bone, but same property name";

    /* This just tests the current functionality. This may not necessarily be
     * correct / desired behavior. */
    FCurvePtr fcurve_with_long_rna_path = fake_fcurve(
        "pose.bones[\"hand.L\"].weirdly_long_location", 0);
    EXPECT_TRUE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 0, true, arm_ob_id).get(),
        from_single,
        to_simple,
        flip))
        << "property name suffix-match";
  }

  { /* Resilience against deleted IDs. */
    FCurvePtr fcurve = fake_fcurve("pose.bones[\"hand.L\"].location", 0);
    Object *object_not_in_main = BKE_object_add_only_object(nullptr, OB_EMPTY, "non-main");

    EXPECT_FALSE(pastebuf_match_path_property(
        bmain,
        fcurve.get(),
        fake_aci_owned("pose.bones[\"hand.L\"].location", 0, true, &object_not_in_main->id).get(),
        false,
        false,
        false))
        << "copying from deleted ID";

    BKE_id_free(nullptr, &object_not_in_main->id);
  }

  BKE_main_free(bmain);
}

TEST(keyframes_paste, pastebuf_match_index_only)
{
  FCurvePtr fcurve = fake_fcurve("some_prop", 1);

  EXPECT_TRUE(pastebuf_match_index_only(
      nullptr, fcurve.get(), fake_aci("location", 1, false).get(), false, false, false));
  EXPECT_FALSE(pastebuf_match_index_only(
      nullptr, fcurve.get(), fake_aci("location", 2, false).get(), false, false, false));
}

}  // namespace blender::editor::animation::tests
