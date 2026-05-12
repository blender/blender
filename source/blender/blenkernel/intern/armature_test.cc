/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_gtest_base.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "testing/testing.h"

namespace blender::bke::tests {

static const float FLOAT_EPSILON = 1.2e-7;

static const float SCALE_EPSILON = 3.71e-5;
static const float ORTHO_EPSILON = 5e-5;

/** Test that the matrix is orthogonal, i.e. has no scale or shear within acceptable precision. */
static double EXPECT_M3_ORTHOGONAL(const float mat[3][3],
                                   double epsilon_scale,
                                   double epsilon_ortho)
{
  /* Do the checks in double precision to avoid precision issues in the checks themselves. */
  double3x3 dmat;
  copy_m3d_m3(dmat.ptr(), mat);

  /* Check individual axis scaling. */
  EXPECT_NEAR(math::length(dmat[0]), 1.0, epsilon_scale);
  EXPECT_NEAR(math::length(dmat[1]), 1.0, epsilon_scale);
  EXPECT_NEAR(math::length(dmat[2]), 1.0, epsilon_scale);

  /* Check orthogonality. */
  EXPECT_NEAR(dot_v3v3_db(dmat[0], dmat[1]), 0.0, epsilon_ortho);
  EXPECT_NEAR(dot_v3v3_db(dmat[0], dmat[2]), 0.0, epsilon_ortho);
  EXPECT_NEAR(dot_v3v3_db(dmat[1], dmat[2]), 0.0, epsilon_ortho);

  /* Check determinant to detect flipping and as a secondary volume change check. */
  double determinant = math::determinant(dmat);

  EXPECT_NEAR(determinant, 1.0, epsilon_ortho);

  return determinant;
}

TEST(mat3_vec_to_roll, UnitMatrix)
{
  float unit_matrix[3][3];
  float roll;

  unit_m3(unit_matrix);

  /* Any vector with a unit matrix should return zero roll. */
  mat3_vec_to_roll(unit_matrix, unit_matrix[0], &roll);
  EXPECT_FLOAT_EQ(0.0f, roll);

  mat3_vec_to_roll(unit_matrix, unit_matrix[1], &roll);
  EXPECT_FLOAT_EQ(0.0f, roll);

  mat3_vec_to_roll(unit_matrix, unit_matrix[2], &roll);
  EXPECT_FLOAT_EQ(0.0f, roll);

  {
    /* Non-unit vector. */
    float vector[3] = {1.0f, 1.0f, 1.0f};
    mat3_vec_to_roll(unit_matrix, vector, &roll);
    EXPECT_NEAR(0.0f, roll, FLOAT_EPSILON);

    /* Normalized version of the above vector. */
    normalize_v3(vector);
    mat3_vec_to_roll(unit_matrix, vector, &roll);
    EXPECT_NEAR(0.0f, roll, FLOAT_EPSILON);
  }
}

TEST(mat3_vec_to_roll, Rotationmatrix)
{
  float rotation_matrix[3][3];
  float roll;

  const float rot_around_x[3] = {1.234f, 0.0f, 0.0f};
  eul_to_mat3(rotation_matrix, rot_around_x);

  {
    const float unit_axis_x[3] = {1.0f, 0.0f, 0.0f};
    mat3_vec_to_roll(rotation_matrix, unit_axis_x, &roll);
    EXPECT_NEAR(1.234f, roll, FLOAT_EPSILON);
  }

  {
    const float unit_axis_y[3] = {0.0f, 1.0f, 0.0f};
    mat3_vec_to_roll(rotation_matrix, unit_axis_y, &roll);
    EXPECT_NEAR(0, roll, FLOAT_EPSILON);
  }

  {
    const float unit_axis_z[3] = {0.0f, 0.0f, 1.0f};
    mat3_vec_to_roll(rotation_matrix, unit_axis_z, &roll);
    EXPECT_NEAR(0, roll, FLOAT_EPSILON);
  }

  {
    const float between_x_and_y[3] = {1.0f, 1.0f, 0.0f};
    mat3_vec_to_roll(rotation_matrix, between_x_and_y, &roll);
    EXPECT_NEAR(0.57158958f, roll, FLOAT_EPSILON);
  }
}

/** Generic function to test vec_roll_to_mat3_normalized. */
static double test_vec_roll_to_mat3_normalized(const float input[3],
                                               float roll,
                                               const float expected_roll_mat[3][3],
                                               bool normalize = true)
{
  float input_normalized[3];
  float roll_mat[3][3];

  if (normalize) {
    /* The vector is re-normalized to replicate the actual usage. */
    normalize_v3_v3(input_normalized, input);
  }
  else {
    copy_v3_v3(input_normalized, input);
  }

  vec_roll_to_mat3_normalized(input_normalized, roll, roll_mat);

  EXPECT_V3_NEAR(roll_mat[1], input_normalized, FLT_EPSILON);

  if (expected_roll_mat) {
    EXPECT_M3_NEAR(roll_mat, expected_roll_mat, FLT_EPSILON);
  }

  return EXPECT_M3_ORTHOGONAL(roll_mat, SCALE_EPSILON, ORTHO_EPSILON);
}

/** Binary search to test where the code switches to the most degenerate special case. */
static double find_flip_boundary(double x, double z)
{
  /* Irrational scale factor to ensure values aren't 'nice', have a lot of rounding errors,
   * and can't accidentally produce the exact result returned by the special case. */
  const double scale = M_1_PI / 10;
  double theta = x * x + z * z;
  double minv = 0, maxv = 1e-2;

  while (maxv - minv > FLT_EPSILON * 1e-3) {
    double mid = (minv + maxv) / 2;

    float roll_mat[3][3];
    float input[3] = {float(x * mid * scale),
                      -float(sqrt(1 - theta * mid * mid) * scale),
                      float(z * mid * scale)};

    normalize_v3(input);
    vec_roll_to_mat3_normalized(input, 0, roll_mat);

    /* The special case assigns exact constants rather than computing. */
    if (roll_mat[0][0] == -1 && roll_mat[0][1] == 0 && roll_mat[2][1] == 0) {
      minv = mid;
    }
    else {
      maxv = mid;
    }
  }

  return sqrt(theta) * (minv + maxv) * 0.5;
}

TEST(vec_roll_to_mat3_normalized, FlippedBoundary1)
{
  EXPECT_NEAR(find_flip_boundary(0, 1), 2.50e-4, 0.01e-4);
}

TEST(vec_roll_to_mat3_normalized, FlippedBoundary2)
{
  EXPECT_NEAR(find_flip_boundary(1, 1), 2.50e-4, 0.01e-4);
}

/* Test cases close to the -Y axis. */
TEST(vec_roll_to_mat3_normalized, Flipped1)
{
  /* If normalized_vector is -Y, simple symmetry by Z axis. */
  const float input[3] = {0.0f, -1.0f, 0.0f};
  const float expected_roll_mat[3][3] = {
      {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat, false);
}

TEST(vec_roll_to_mat3_normalized, Flipped2)
{
  /* If normalized_vector is close to -Y and
   * it has X and Z values below a threshold,
   * simple symmetry by Z axis. */
  const float input[3] = {1e-24, -0.999999881, 0};
  const float expected_roll_mat[3][3] = {
      {-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat, false);
}

TEST(vec_roll_to_mat3_normalized, Flipped3)
{
  /* If normalized_vector is in a critical range close to -Y, apply the special case. */
  const float input[3] = {2.5e-4f, -0.999999881f, 2.5e-4f}; /* Corner Case. */
  const float expected_roll_mat[3][3] = {{0.000000f, -2.5e-4f, -1.000000f},
                                         {2.5e-4f, -0.999999881f, 2.5e-4f},
                                         {-1.000000f, -2.5e-4f, 0.000000f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat, false);
}

/* Test 90 degree rotations. */
TEST(vec_roll_to_mat3_normalized, Rotate90_Z_CW)
{
  /* Rotate 90 around Z. */
  const float input[3] = {1, 0, 0};
  const float expected_roll_mat[3][3] = {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Rotate90_Z_CCW)
{
  /* Rotate 90 around Z. */
  const float input[3] = {-1, 0, 0};
  const float expected_roll_mat[3][3] = {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Rotate90_X_CW)
{
  /* Rotate 90 around X. */
  const float input[3] = {0, 0, -1};
  const float expected_roll_mat[3][3] = {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Rotate90_X_CCW)
{
  /* Rotate 90 around X. */
  const float input[3] = {0, 0, 1};
  const float expected_roll_mat[3][3] = {{1, 0, 0}, {0, 0, 1}, {0, -1, 0}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

/* Test the general case when the vector is far enough from -Y. */
TEST(vec_roll_to_mat3_normalized, Generic1)
{
  const float input[3] = {1.0f, 1.0f, 1.0f}; /* Arbitrary Value. */
  const float expected_roll_mat[3][3] = {{0.788675129f, -0.577350259f, -0.211324856f},
                                         {0.577350259f, 0.577350259f, 0.577350259f},
                                         {-0.211324856f, -0.577350259f, 0.788675129f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Generic2)
{
  const float input[3] = {1.0f, -1.0f, 1.0f}; /* Arbitrary Value. */
  const float expected_roll_mat[3][3] = {{0.211324856f, -0.577350259f, -0.788675129f},
                                         {0.577350259f, -0.577350259f, 0.577350259f},
                                         {-0.788675129f, -0.577350259f, 0.211324856f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Generic3)
{
  const float input[3] = {-1.0f, -1.0f, 1.0f}; /* Arbitrary Value. */
  const float expected_roll_mat[3][3] = {{0.211324856f, 0.577350259f, 0.788675129f},
                                         {-0.577350259f, -0.577350259f, 0.577350259f},
                                         {0.788675129f, -0.577350259f, 0.211324856f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

TEST(vec_roll_to_mat3_normalized, Generic4)
{
  const float input[3] = {-1.0f, -1.0f, -1.0f}; /* Arbitrary Value. */
  const float expected_roll_mat[3][3] = {{0.211324856f, 0.577350259f, -0.788675129f},
                                         {-0.577350259f, -0.577350259f, -0.577350259f},
                                         {-0.788675129f, 0.577350259f, 0.211324856f}};
  test_vec_roll_to_mat3_normalized(input, 0.0f, expected_roll_mat);
}

/* Test roll. */
TEST(vec_roll_to_mat3_normalized, Roll1)
{
  const float input[3] = {1.0f, 1.0f, 1.0f}; /* Arbitrary Value. */
  const float expected_roll_mat[3][3] = {{0.211324856f, 0.577350259f, -0.788675129f},
                                         {0.577350259f, 0.577350259f, 0.577350259f},
                                         {0.788675129f, -0.577350259f, -0.211324856f}};
  test_vec_roll_to_mat3_normalized(input, float(M_PI_2), expected_roll_mat);
}

/** Test that the matrix is orthogonal for an input close to -Y. */
static double test_vec_roll_to_mat3_orthogonal(double s, double x, double z)
{
  const float input[3] = {float(x), float(s * sqrt(1 - x * x - z * z)), float(z)};

  return test_vec_roll_to_mat3_normalized(input, 0.0f, nullptr);
}

/** Test that the matrix is orthogonal for a range of inputs close to -Y. */
static void test_vec_roll_to_mat3_orthogonal(double s, double x1, double x2, double y1, double y2)
{
  const int count = 5000;
  double delta = 0;
  double tmax = 0;

  for (int i = 0; i <= count; i++) {
    double t = double(i) / count;
    double det = test_vec_roll_to_mat3_orthogonal(s, interpd(x2, x1, t), interpd(y2, y1, t));

    /* Find and report maximum error in the matrix determinant. */
    double curdelta = abs(det - 1);
    if (curdelta > delta) {
      delta = curdelta;
      tmax = t;
    }
  }

  printf("             Max determinant deviation %.10f at %f.\n", delta, tmax);
}

#define TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(name, s, x1, x2, y1, y2) \
  TEST(vec_roll_to_mat3_normalized, name) \
  { \
    test_vec_roll_to_mat3_orthogonal(s, x1, x2, y1, y2); \
  }

/* Moving from -Y towards X. */
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_005, -1, 0, 0, 3e-4, 0.005)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_010, -1, 0, 0, 0.005, 0.010)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_050, -1, 0, 0, 0.010, 0.050)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_100, -1, 0, 0, 0.050, 0.100)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_200, -1, 0, 0, 0.100, 0.200)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_000_300, -1, 0, 0, 0.200, 0.300)

/* Moving from -Y towards X and Y. */
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_005_005, -1, 3e-4, 0.005, 3e-4, 0.005)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_010_010, -1, 0.005, 0.010, 0.005, 0.010)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_050_050, -1, 0.010, 0.050, 0.010, 0.050)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_100_100, -1, 0.050, 0.100, 0.050, 0.100)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoN_200_200, -1, 0.100, 0.200, 0.100, 0.200)

/* Moving from +Y towards X. */
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoP_000_005, 1, 0, 0, 0, 0.005)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoP_000_100, 1, 0, 0, 0.005, 0.100)

/* Moving from +Y towards X and Y. */
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoP_005_005, 1, 0, 0.005, 0, 0.005)
TEST_VEC_ROLL_TO_MAT3_ORTHOGONAL(OrthoP_100_100, 1, 0.005, 0.100, 0.005, 0.100)

class BKE_armature_find_selected_bones_test : public testing::Test {
 protected:
  bArmature arm = {};
  Bone bone1 = {}, bone2 = {}, bone3 = {};

  void SetUp() override
  {
    STRNCPY(bone1.name, "bone1");
    STRNCPY(bone2.name, "bone2");
    STRNCPY(bone3.name, "bone3");

    BLI_addtail(&arm.bonebase, &bone1);    /* bone1 is root bone. */
    BLI_addtail(&arm.bonebase, &bone2);    /* bone2 is root bone. */
    BLI_addtail(&bone2.childbase, &bone3); /* bone3 has bone2 as parent. */
  }
};

TEST_F(BKE_armature_find_selected_bones_test, some_bones_selected)
{
  bone1.flag = BONE_SELECTED;
  bone2.flag = eBone_Flag{};
  bone3.flag = BONE_SELECTED;

  std::vector<Bone *> seen_bones;
  auto callback = [&](Bone *bone) { seen_bones.push_back(bone); };

  SelectedBonesResult result = BKE_armature_find_selected_bones(&arm, callback);

  ASSERT_EQ(seen_bones.size(), 2) << "Expected 2 selected bones, got " << seen_bones.size();
  EXPECT_EQ(seen_bones[0], &bone1);
  EXPECT_EQ(seen_bones[1], &bone3);

  EXPECT_FALSE(result.all_bones_selected); /* Bone 2 was not selected. */
  EXPECT_FALSE(result.no_bones_selected);  /* Bones 1 and 3 were selected. */
}

TEST_F(BKE_armature_find_selected_bones_test, no_bones_selected)
{
  bone1.flag = bone2.flag = bone3.flag = eBone_Flag{};

  std::vector<Bone *> seen_bones;
  auto callback = [&](Bone *bone) { seen_bones.push_back(bone); };

  SelectedBonesResult result = BKE_armature_find_selected_bones(&arm, callback);

  EXPECT_TRUE(seen_bones.empty()) << "Expected no selected bones, got " << seen_bones.size();
  EXPECT_FALSE(result.all_bones_selected);
  EXPECT_TRUE(result.no_bones_selected);
}

TEST_F(BKE_armature_find_selected_bones_test, all_bones_selected)
{
  bone1.flag = bone2.flag = bone3.flag = BONE_SELECTED;

  std::vector<Bone *> seen_bones;
  auto callback = [&](Bone *bone) { seen_bones.push_back(bone); };

  SelectedBonesResult result = BKE_armature_find_selected_bones(&arm, callback);

  ASSERT_EQ(seen_bones.size(), 3) << "Expected 3 selected bones, got " << seen_bones.size();
  EXPECT_EQ(seen_bones[0], &bone1);
  EXPECT_EQ(seen_bones[1], &bone2);
  EXPECT_EQ(seen_bones[2], &bone3);

  EXPECT_TRUE(result.all_bones_selected);
  EXPECT_FALSE(result.no_bones_selected);
}

class BoneIndexTestBase {
 public:
  Main *bmain = nullptr;
  bArmature *armature = nullptr;
  Bone *bone_root = nullptr;
  Bone *bone_child1 = nullptr;
  Bone *bone_child2 = nullptr;

  void create_armature()
  {
    armature = BKE_armature_add(bmain, "Test Armature");

    bone_root = MEM_new<Bone>(__func__);
    bone_child1 = MEM_new<Bone>(__func__);
    bone_child2 = MEM_new<Bone>(__func__);

    bone_child1->parent = bone_root;
    bone_child2->parent = bone_root;

    STRNCPY(bone_root->name, "bone_root");
    STRNCPY(bone_child1->name, "bone_child1");
    STRNCPY(bone_child2->name, "bone_child2");

    BLI_addtail(&armature->bonebase, bone_root);
    BLI_addtail(&bone_root->childbase, bone_child1);
    BLI_addtail(&bone_root->childbase, bone_child2);
  }
};

class ArmatureBoneIndexTest : public BoneIndexTestBase, public BlenderGTestBase {
 public:
  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(ArmatureBoneIndexTest, armature_init_data)
{
  create_armature();

  EXPECT_EQ(0, armature->runtime->bones.size())
      << "After low-level bone adding, the runtime array should be empty.";
  ASSERT_NE(0, armature->id.session_uid);
  EXPECT_EQ(uint64_t(armature->id.session_uid) << 32, armature->runtime->bones_generation_count);

  EXPECT_EQ(bone_root, armature->bone_get_indexed(0));
  EXPECT_EQ(3, armature->runtime->bones.size())
      << "After calling bone_get_index(), the runtime array should be filled.";

  EXPECT_EQ(bone_child1, armature->bone_get_indexed(1));

  armature->runtime->bones_tag_rebuild();
  EXPECT_EQ(bone_child2, armature->bone_get_indexed(2));
  EXPECT_EQ(3, armature->runtime->bones.size())
      << "After clearing the bone array, calling bone_get_index() should rebuild it.";
}

TEST_F(ArmatureBoneIndexTest, armature_copy_data)
{
  create_armature();
  EXPECT_EQ(bone_root, armature->bone_get_indexed(0)); /* Ensure the bone array is created. */

  /* Regular copy, should get its own generation counter. */
  bArmature *copy = id_cast<bArmature *>(BKE_id_copy(this->bmain, &armature->id));
  ASSERT_NE(0, armature->id.session_uid);
  EXPECT_NE(armature->runtime->bones_generation_count, copy->runtime->bones_generation_count);
  EXPECT_EQ(3, copy->runtime->bones.size());

  /* Getting the indexed bone should return a copied bone, and not the original. */
  Bone *copybone_root = static_cast<Bone *>(copy->bonebase.first);
  EXPECT_EQ(copybone_root, copy->bone_get_indexed(0));

  /* CoW-copy, should reuse the generation counter. */
  bArmature *cow_copy = id_cast<bArmature *>(
      BKE_id_copy_in_lib(this->bmain,
                         std::nullopt,
                         &armature->id,
                         std::nullopt,
                         nullptr,
                         LIB_ID_COPY_DEFAULT | LIB_ID_COPY_SET_COPIED_ON_WRITE));
  ASSERT_NE(0, armature->id.session_uid);
  EXPECT_EQ(armature->runtime->bones_generation_count, cow_copy->runtime->bones_generation_count);
  EXPECT_EQ(3, cow_copy->runtime->bones.size());
}

class PoseBoneIndexTest : public BoneIndexTestBase, public BlenderGTestBase {
 public:
  Object *object = nullptr;

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }

  void create_object()
  {
    create_armature();
    object = BKE_object_add_only_object(bmain, OB_ARMATURE, "PoseObject");
    object->data = &armature->id;
    id_us_plus(&armature->id);
    BKE_pose_rebuild(bmain, object, armature, /*do_id_user=*/true);
  }
};

TEST_F(PoseBoneIndexTest, pose_bone_get)
{
  create_object();

  bPoseChannel *pchan_root = BKE_pose_channel_find_name(object->pose, bone_root->name);
  bPoseChannel *pchan_child1 = BKE_pose_channel_find_name(object->pose, bone_child1->name);
  bPoseChannel *pchan_child2 = BKE_pose_channel_find_name(object->pose, bone_child2->name);
  EXPECT_EQ(bone_root, pchan_root->bone_get(*object));
  EXPECT_EQ(bone_child1, pchan_child1->bone_get(*object));
  EXPECT_EQ(bone_child2, pchan_child2->bone_get(*object));
}

TEST_F(PoseBoneIndexTest, reassign_armature_copy)
{
  create_object();

  bPoseChannel *pchan_root = BKE_pose_channel_find_name(object->pose, bone_root->name);
  bPoseChannel *pchan_child1 = BKE_pose_channel_find_name(object->pose, bone_child1->name);
  bPoseChannel *pchan_child2 = BKE_pose_channel_find_name(object->pose, bone_child2->name);

  /* Create a copy of the armature. */
  bArmature *copy = id_cast<bArmature *>(BKE_id_copy(this->bmain, &armature->id));
  Bone *copybone_root = BKE_armature_find_bone_name(copy, bone_root->name);
  Bone *copybone_child1 = BKE_armature_find_bone_name(copy, bone_child1->name);
  Bone *copybone_child2 = BKE_armature_find_bone_name(copy, bone_child2->name);

  /* Check that the copy actually copied all the bones. Since this test trusts these pointers,
   * better to be safe than sorry. */
  ASSERT_NE(bone_root, copybone_root);
  ASSERT_NE(bone_child1, copybone_child1);
  ASSERT_NE(bone_child2, copybone_child2);

  /* Check that the copy did indeed not change the order of the bones. */
  Array<Bone *> expected_bones = {copybone_root, copybone_child1, copybone_child2};
  ASSERT_EQ(expected_bones, copy->runtime->bones);

  /* Assign the armature copy. After this, getting bone pointers should just work without
   * explicitly rebuilding the pose. */
  id_us_min(&armature->id);
  object->data = &copy->id;
  id_us_plus(&copy->id);

  EXPECT_EQ(copybone_root, pchan_root->bone_get(*object));
  EXPECT_EQ(copybone_child1, pchan_child1->bone_get(*object));
  EXPECT_EQ(copybone_child2, pchan_child2->bone_get(*object));

  /* Getting from the original armature should also work, because bone indices haven't changed. */
  EXPECT_EQ(bone_root, pchan_root->bone_get(*armature));
  EXPECT_EQ(bone_child1, pchan_child1->bone_get(*armature));
  EXPECT_EQ(bone_child2, pchan_child2->bone_get(*armature));
}

TEST_F(PoseBoneIndexTest, pose_rebuild_test)
{
  /* After exiting Armature Edit mode, all Bones get recreated from EditBones. This clears the
   * bones array, which triggers a rebuild of that array on first access. But, when this access
   * happens via pchan->bone_get(), the pchan's bone index may need rebuilding as well. */

  create_object(); /* Create armature + pose with 3 bones. */

  bPoseChannel *pchan_root = BKE_pose_channel_find_name(object->pose, bone_root->name);
  bPoseChannel *pchan_child1 = BKE_pose_channel_find_name(object->pose, bone_child1->name);

  /* Mimick going into Armature Edit mode and deleting bone 'child2'. */
  BLI_remlink(&bone_root->childbase, bone_child2);
  MEM_delete(bone_child2);
  bone_child2 = nullptr;
  armature->runtime->bones_tag_rebuild();

  EXPECT_EQ(bone_root, pchan_root->bone_get(*object));
  EXPECT_EQ(bone_child1, pchan_child1->bone_get(*object));
  /* This will fail, because bone_get() asserts the bone index is valid. */
  // EXPECT_EQ(nullptr, pchan_child2->bone_get(*object));

  const uint64_t armature_generation_count = armature->runtime->bones_generation_count;
  const uint64_t object_generation_count = object->runtime->pose_bones_generation_count;
  EXPECT_EQ(armature_generation_count, object_generation_count);
}

}  // namespace blender::bke::tests
