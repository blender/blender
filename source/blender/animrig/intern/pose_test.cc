/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"

#include "DNA_object_types.h"

#include "ANIM_action.hh"
#include "ANIM_pose.hh"

#include "CLG_log.h"

#include "RNA_define.hh"

#include "testing/testing.h"

namespace blender {

constexpr char msg_unexpected_modification[] =
    "Properties not stored in the pose are expected to not be modified.";

namespace animrig::tests {

class PoseTest : public testing::Test {
 public:
  Main *bmain;
  Action *pose_action;
  Object *obj_empty;
  Object *obj_armature_a;
  Object *obj_armature_b;
  StripKeyframeData *keyframe_data;
  const animrig::KeyframeSettings key_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO, BEZT_IPO_BEZ};

  static void SetUpTestSuite()
  {
    /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialized properly. */
    CLG_init();

    /* To make id_can_have_animdata() and friends work, the `id_types` array needs to be set up. */
    BKE_idtype_init();

    RNA_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
    RNA_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
    pose_action = BKE_id_new<Action>(bmain, "pose_data");
    Layer &layer = pose_action->layer_add("first_layer");
    Strip &strip = layer.strip_add(*pose_action, Strip::Type::Keyframe);
    keyframe_data = &strip.data<StripKeyframeData>(*pose_action);

    obj_empty = BKE_object_add_only_object(bmain, OB_EMPTY, "obj_empty");
    obj_armature_a = BKE_object_add_only_object(bmain, OB_ARMATURE, "obj_armature_a");
    obj_armature_b = BKE_object_add_only_object(bmain, OB_ARMATURE, "obj_armature_b");

    bArmature *armature = BKE_armature_add(bmain, "ArmatureA");
    obj_armature_a->data = id_cast<ID *>(armature);

    Bone *bone = MEM_new<Bone>("BONE");
    STRNCPY(bone->name, "BoneA");
    BLI_addtail(&armature->bonebase, bone);

    bone = MEM_new<Bone>("BONE");
    STRNCPY(bone->name, "BoneB");
    BLI_addtail(&armature->bonebase, bone);

    BKE_pose_ensure(bmain, obj_armature_a, armature, false);

    armature = BKE_armature_add(bmain, "ArmatureB");
    obj_armature_b->data = id_cast<ID *>(armature);

    bone = MEM_new<Bone>("BONE");
    STRNCPY(bone->name, "BoneA");
    BLI_addtail(&armature->bonebase, bone);

    bone = MEM_new<Bone>("BONE");
    STRNCPY(bone->name, "BoneB");
    BLI_addtail(&armature->bonebase, bone);

    BKE_pose_ensure(bmain, obj_armature_b, armature, false);
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(PoseTest, get_best_slot)
{
  Slot &first_slot = pose_action->slot_add();
  Slot &second_slot = pose_action->slot_add_for_id(obj_empty->id);

  EXPECT_EQ(&get_best_pose_slot_for_id(obj_empty->id, *pose_action), &second_slot);
  EXPECT_EQ(&get_best_pose_slot_for_id(obj_armature_a->id, *pose_action), &first_slot);
}

TEST_F(PoseTest, apply_action_object)
{
  /* Since pose bones live on the object, the code is already set up to handle objects
   * transforms, even though the name suggests it only applies to bones. */
  Slot &first_slot = pose_action->slot_add();
  EXPECT_EQ(obj_empty->loc[0], 0.0f);
  keyframe_data->keyframe_insert(bmain, first_slot, {"location", 0}, {1, 10}, key_settings);
  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action_all_bones(obj_empty, pose_action, first_slot.handle, &eval_context);
  EXPECT_EQ(obj_empty->loc[0], 10.0f);
}

TEST_F(PoseTest, apply_action_all_bones_single_slot)
{
  Slot &first_slot = pose_action->slot_add();

  keyframe_data->keyframe_insert(
      bmain, first_slot, {"pose.bones[\"BoneA\"].location", 0}, {1, 10}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, first_slot, {"pose.bones[\"BoneB\"].location", 1}, {1, 5}, key_settings);

  bPoseChannel *bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  bPoseChannel *bone_b = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneB");

  bone_a->loc[1] = 1.0;
  bone_a->loc[2] = 2.0;

  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action_all_bones(
      obj_armature_a, pose_action, first_slot.handle, &eval_context);
  EXPECT_EQ(bone_a->loc[0], 10.0);
  EXPECT_EQ(bone_b->loc[1], 5.0);

  EXPECT_EQ(bone_a->loc[1], 1.0) << msg_unexpected_modification;
  EXPECT_EQ(bone_a->loc[2], 2.0);
}

TEST_F(PoseTest, apply_action_all_bones_multiple_slots)
{
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);
  Slot &slot_b = pose_action->slot_add_for_id(obj_armature_b->id);

  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].location", 0}, {1, 5}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneB\"].location", 0}, {1, 5}, key_settings);

  keyframe_data->keyframe_insert(
      bmain, slot_b, {"pose.bones[\"BoneA\"].location", 1}, {1, 10}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_b, {"pose.bones[\"BoneB\"].location", 1}, {1, 10}, key_settings);

  bPoseChannel *arm_a_bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  bPoseChannel *arm_a_bone_b = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneB");

  bPoseChannel *arm_b_bone_a = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneA");
  bPoseChannel *arm_b_bone_b = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneB");

  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action_all_bones(obj_armature_a, pose_action, slot_a.handle, &eval_context);

  EXPECT_EQ(arm_a_bone_a->loc[0], 5.0);
  EXPECT_EQ(arm_a_bone_a->loc[1], 0.0) << msg_unexpected_modification;
  EXPECT_EQ(arm_a_bone_a->loc[2], 0.0) << msg_unexpected_modification;

  EXPECT_EQ(arm_a_bone_b->loc[0], 5.0);

  EXPECT_EQ(arm_b_bone_a->loc[1], 0.0) << "Other armature should not be affected yet.";

  animrig::pose_apply_action_all_bones(obj_armature_b, pose_action, slot_b.handle, &eval_context);

  EXPECT_EQ(arm_b_bone_b->loc[0], 0.0) << msg_unexpected_modification;
  EXPECT_EQ(arm_b_bone_b->loc[1], 10.0);
  EXPECT_EQ(arm_b_bone_b->loc[2], 0.0) << msg_unexpected_modification;

  EXPECT_EQ(arm_a_bone_a->loc[0], 5.0) << "Other armature should not be affected.";

  /* Any slot can be applied, even if it hasn't been added for the ID. */
  animrig::pose_apply_action_all_bones(obj_armature_a, pose_action, slot_b.handle, &eval_context);

  EXPECT_EQ(arm_b_bone_b->loc[1], arm_b_bone_a->loc[1])
      << "Applying the same pose should result in the same values.";
}

TEST_F(PoseTest, apply_action_blend_single_slot)
{
  Slot &first_slot = pose_action->slot_add();
  keyframe_data->keyframe_insert(
      bmain, first_slot, {"pose.bones[\"BoneA\"].location", 0}, {1, 10}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, first_slot, {"pose.bones[\"BoneB\"].location", 1}, {1, 5}, key_settings);

  bPoseChannel *bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  bPoseChannel *bone_b = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneB");

  bone_a->loc[0] = 0.0;
  bone_b->loc[1] = 0.0;

  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action_blend_all_bones(
      obj_armature_a, pose_action, first_slot.handle, &eval_context, 1.0);

  EXPECT_NEAR(bone_a->loc[0], 10.0, 0.001);
  EXPECT_NEAR(bone_b->loc[1], 5.0, 0.001);

  bone_a->loc[0] = 0.0;
  bone_b->loc[1] = 0.0;

  animrig::pose_apply_action_blend_all_bones(
      obj_armature_a, pose_action, first_slot.handle, &eval_context, 0.5);

  EXPECT_NEAR(bone_a->loc[0], 5.0, 0.001);
  EXPECT_NEAR(bone_b->loc[1], 2.5, 0.001);

  bone_a->loc[0] = 0.0;
  bone_b->loc[1] = 0.0;

  bone_a->flag |= POSE_SELECTED;
  bone_b->flag &= ~POSE_SELECTED;

  /* This should only affect the selected bone. */
  animrig::pose_apply_action_blend(
      obj_armature_a, pose_action, first_slot.handle, &eval_context, 0.5);

  EXPECT_NEAR(bone_a->loc[0], 5.0, 0.001);
  EXPECT_NEAR(bone_b->loc[1], 0.0, 0.001);
}

TEST_F(PoseTest, apply_action_multiple_objects)
{
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);
  Slot &slot_b = pose_action->slot_add_for_id(obj_armature_b->id);

  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].location", 0}, {1, 5}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneB\"].location", 0}, {1, 5}, key_settings);

  keyframe_data->keyframe_insert(
      bmain, slot_b, {"pose.bones[\"BoneA\"].location", 1}, {1, 10}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_b, {"pose.bones[\"BoneB\"].location", 1}, {1, 10}, key_settings);

  bPoseChannel *arm_a_bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  bPoseChannel *arm_a_bone_b = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneB");

  bPoseChannel *arm_b_bone_a = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneA");
  bPoseChannel *arm_b_bone_b = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneB");

  Vector<bPoseChannel *> all_bones = {arm_a_bone_a, arm_a_bone_b, arm_b_bone_a, arm_b_bone_b};

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->flag &= ~POSE_SELECTED;
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 1.0);

  /* No bones are selected, this should affect all bones. */
  EXPECT_NEAR(arm_a_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[1], 10, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[1], 10, 0.001);

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  arm_a_bone_a->flag |= POSE_SELECTED;

  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 1.0);

  /* Only the one selected bone should be affected. */
  EXPECT_NEAR(arm_a_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 0, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[1], 0, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[1], 0, 0.001);

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  arm_a_bone_a->flag |= POSE_SELECTED;
  arm_b_bone_a->flag |= POSE_SELECTED;

  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 1.0);

  /* Only the two selected bones from different armatures should be affected. */
  EXPECT_NEAR(arm_a_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 0, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[1], 10, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[1], 0, 0.001);

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 0.5);

  /* Blending half way. */
  EXPECT_NEAR(arm_a_bone_a->loc[0], 2.5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 0, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[1], 5, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[1], 0, 0.001);

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  arm_a_bone_a->flag |= POSE_SELECTED;
  arm_a_bone_b->flag |= POSE_SELECTED;
  arm_b_bone_a->flag |= POSE_SELECTED;

  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 1.0);

  EXPECT_NEAR(arm_a_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[1], 10, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[1], 0, 0.001);
}

TEST_F(PoseTest, apply_action_multiple_objects_single_slot)
{
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);

  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].location", 0}, {1, 5}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneB\"].location", 0}, {1, 5}, key_settings);

  bPoseChannel *arm_a_bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  bPoseChannel *arm_a_bone_b = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneB");

  bPoseChannel *arm_b_bone_a = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneA");
  bPoseChannel *arm_b_bone_b = BKE_pose_channel_find_name(obj_armature_b->pose, "BoneB");

  Vector<bPoseChannel *> all_bones = {arm_a_bone_a, arm_a_bone_b, arm_b_bone_a, arm_b_bone_b};

  for (bPoseChannel *pose_bone : all_bones) {
    pose_bone->flag &= ~POSE_SELECTED;
    pose_bone->loc[0] = 0.0;
    pose_bone->loc[1] = 0.0;
  }

  AnimationEvalContext eval_context = {nullptr, 1.0f};
  animrig::pose_apply_action({obj_armature_a, obj_armature_b}, *pose_action, &eval_context, 1.0);

  /* No bones are selected, this should affect all bones. Armature B has no slot, it should fall
   * back to slot 0. */
  EXPECT_NEAR(arm_a_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_a_bone_b->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_b_bone_a->loc[0], 5, 0.001);
  EXPECT_NEAR(arm_b_bone_b->loc[0], 5, 0.001);
}

static void reset_pose_bone_rotations(bPoseChannel &pose_bone)
{
  pose_bone.eul[0] = 0;
  pose_bone.eul[1] = 0;
  pose_bone.eul[2] = 0;

  pose_bone.quat[0] = 1;
  pose_bone.quat[1] = 0;
  pose_bone.quat[2] = 0;
  pose_bone.quat[3] = 0;

  pose_bone.rotAngle = 0;
  pose_bone.rotAxis[0] = 0;
  pose_bone.rotAxis[1] = 0;
  pose_bone.rotAxis[2] = 0;
}

TEST_F(PoseTest, apply_action_differing_rotation_mode_from_euler)
{
  /* When the pose has a different rotation mode than the data it is being applied to, the system
   * should convert the rotation. */
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);

  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_euler", 0}, {1, 3.14}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_euler", 1}, {1, 1}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_euler", 2}, {1, 0}, key_settings);

  bPoseChannel *bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  AnimationEvalContext eval_context = {nullptr, 1.0f};

  /* First check that applying works if the rotation mode matches. */
  bone_a->rotmode = ROT_MODE_XYZ;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  EXPECT_NEAR(bone_a->eul[0], 3.14, 0.001);
  EXPECT_NEAR(bone_a->eul[1], 1, 0.001);
  EXPECT_NEAR(bone_a->eul[2], 0, 0.001);

  BKE_pchan_calc_mat(bone_a);
  float expected_matrix[4][4];
  copy_m4_m4(expected_matrix, bone_a->chan_mat);

  /* Check that other rotation modes work the same as applying euler directly. */
  bone_a->rotmode = ROT_MODE_QUAT;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  bone_a->rotmode = ROT_MODE_AXISANGLE;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  /* Not doing blend testing here since the rotation matrix will not align. Component wise
   * interpolation of euler angles and matrix interpolation is expected to yield different
   * results. */
}

TEST_F(PoseTest, apply_action_differing_rotation_mode_from_quaternion)
{
  /* When the pose has a different rotation mode than the data it is being applied to, the system
   * should convert the rotation. */
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);

  float quaternion[4] = {0.877, 0.11, -0.483, -0.164};
  /* We have to have a normalized quaternion otherwise the resulting matrix will be off between
   * different rotation modes. */
  normalize_qt(quaternion);
  keyframe_data->keyframe_insert(bmain,
                                 slot_a,
                                 {"pose.bones[\"BoneA\"].rotation_quaternion", 0},
                                 {1, quaternion[0]},
                                 key_settings);
  keyframe_data->keyframe_insert(bmain,
                                 slot_a,
                                 {"pose.bones[\"BoneA\"].rotation_quaternion", 1},
                                 {1, quaternion[1]},
                                 key_settings);
  keyframe_data->keyframe_insert(bmain,
                                 slot_a,
                                 {"pose.bones[\"BoneA\"].rotation_quaternion", 2},
                                 {1, quaternion[2]},
                                 key_settings);
  keyframe_data->keyframe_insert(bmain,
                                 slot_a,
                                 {"pose.bones[\"BoneA\"].rotation_quaternion", 3},
                                 {1, quaternion[3]},
                                 key_settings);

  bPoseChannel *bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  AnimationEvalContext eval_context = {nullptr, 1.0f};

  /* First check that applying works if the rotation mode matches. */
  bone_a->rotmode = ROT_MODE_QUAT;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  EXPECT_NEAR(bone_a->quat[0], quaternion[0], 0.001);
  EXPECT_NEAR(bone_a->quat[1], quaternion[1], 0.001);
  EXPECT_NEAR(bone_a->quat[2], quaternion[2], 0.001);
  EXPECT_NEAR(bone_a->quat[3], quaternion[3], 0.001);

  BKE_pchan_calc_mat(bone_a);
  float expected_matrix[4][4];
  copy_m4_m4(expected_matrix, bone_a->chan_mat);

  /* Check that other rotation modes work the same as applying quaternion directly. */
  bone_a->rotmode = ROT_MODE_XYZ;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  bone_a->rotmode = ROT_MODE_AXISANGLE;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  reset_pose_bone_rotations(*bone_a);

  /* Also test with blend factor other than 1. */
  bone_a->rotmode = ROT_MODE_QUAT;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  copy_m4_m4(expected_matrix, bone_a->chan_mat);

  bone_a->rotmode = ROT_MODE_AXISANGLE;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  bone_a->rotmode = ROT_MODE_XYZ;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);
}

TEST_F(PoseTest, apply_action_differing_rotation_mode_from_axisangle)
{
  /* When the pose has a different rotation mode than the data it is being applied to, the system
   * should convert the rotation. */
  Slot &slot_a = pose_action->slot_add_for_id(obj_armature_a->id);

  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_axis_angle", 0}, {1, 0.66}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_axis_angle", 1}, {1, -0.3}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_axis_angle", 2}, {1, 0.86}, key_settings);
  keyframe_data->keyframe_insert(
      bmain, slot_a, {"pose.bones[\"BoneA\"].rotation_axis_angle", 3}, {1, -0.42}, key_settings);

  bPoseChannel *bone_a = BKE_pose_channel_find_name(obj_armature_a->pose, "BoneA");
  AnimationEvalContext eval_context = {nullptr, 1.0f};

  /* First check that applying works if the rotation mode matches. */
  bone_a->rotmode = ROT_MODE_AXISANGLE;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  EXPECT_NEAR(bone_a->rotAngle, 0.66, 0.001);
  EXPECT_NEAR(bone_a->rotAxis[0], -0.3, 0.001);
  EXPECT_NEAR(bone_a->rotAxis[1], 0.86, 0.001);
  EXPECT_NEAR(bone_a->rotAxis[2], -0.42, 0.001);

  BKE_pchan_calc_mat(bone_a);
  float expected_matrix[4][4];
  copy_m4_m4(expected_matrix, bone_a->chan_mat);

  /* Check that other rotation modes work the same as applying quaternion directly. */
  bone_a->rotmode = ROT_MODE_XYZ;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  bone_a->rotmode = ROT_MODE_QUAT;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 1.0);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  reset_pose_bone_rotations(*bone_a);

  /* Also test with blend factor other than 1. */
  bone_a->rotmode = ROT_MODE_AXISANGLE;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  copy_m4_m4(expected_matrix, bone_a->chan_mat);

  bone_a->rotmode = ROT_MODE_QUAT;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);

  bone_a->rotmode = ROT_MODE_XYZ;
  animrig::pose_apply_action({obj_armature_a}, *pose_action, &eval_context, 0.7);
  BKE_pchan_calc_mat(bone_a);
  EXPECT_M4_NEAR(expected_matrix, bone_a->chan_mat, 0.001);
}

}  // namespace animrig::tests
}  // namespace blender
