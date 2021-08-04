/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

#include "BKE_armature.hh"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_armature_types.h"

#include "testing/testing.h"

namespace blender::bke::tests {

static const float FLOAT_EPSILON = 1.2e-7;

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

TEST(vec_roll_to_mat3_normalized, Rotationmatrix)
{
  float negative_y_axis[3][3];
  unit_m3(negative_y_axis);
  negative_y_axis[0][0] = negative_y_axis[1][1] = -1.0f;

  const float roll = 0.0f;
  float roll_mat[3][3];

  /* If normalized_vector is -Y, simple symmetry by Z axis. */
  {
    const float normalized_vector[3] = {0.0f, -1.0f, 0.0f};
    vec_roll_to_mat3_normalized(normalized_vector, roll, roll_mat);
    EXPECT_M3_NEAR(roll_mat, negative_y_axis, FLT_EPSILON);
  }

  /* If normalized_vector is far enough from -Y, apply the general case. */
  {
    const float expected_roll_mat[3][3] = {{1.000000f, 0.000000f, 0.000000f},
                                           {0.000000f, -0.999989986f, -0.000000f},
                                           {0.000000f, 0.000000f, 1.000000f}};

    const float normalized_vector[3] = {0.0f, -1.0f + 1e-5f, 0.0f};
    vec_roll_to_mat3_normalized(normalized_vector, roll, roll_mat);
    EXPECT_M3_NEAR(roll_mat, expected_roll_mat, FLT_EPSILON);
  }

#if 0
  /* TODO: This test will pass after fixing T82455) */
  /* If normalized_vector is close to -Y and
   * it has X and Z values above a threshold,
   * apply the special case. */
  {
    const float expected_roll_mat[3][3] = {{0.000000f, -9.99999975e-06f, 1.000000f},
                                           {9.99999975e-06f, -0.999999881f, 9.99999975e-06f},
                                           {1.000000f, -9.99999975e-06, 0.000000f}};
    const float normalized_vector[3] = {1e-24, -0.999999881, 0};
    vec_roll_to_mat3_normalized(normalized_vector, roll, roll_mat);
    EXPECT_M3_NEAR(roll_mat, expected_roll_mat, FLT_EPSILON);
  }
#endif

  /* If normalized_vector is in a critical range close to -Y, apply the special case. */
  {
    const float expected_roll_mat[3][3] = {{0.000000f, -9.99999975e-06f, 1.000000f},
                                           {9.99999975e-06f, -0.999999881f, 9.99999975e-06f},
                                           {1.000000f, -9.99999975e-06f, 0.000000f}};

    const float normalized_vector[3] = {1e-5f, -0.999999881f, 1e-5f}; /* Corner Case. */
    vec_roll_to_mat3_normalized(normalized_vector, roll, roll_mat);
    EXPECT_M3_NEAR(roll_mat, expected_roll_mat, FLT_EPSILON);
  }

  /* If normalized_vector is far enough from -Y, apply the general case. */
  {
    const float expected_roll_mat[3][3] = {{0.788675129f, -0.577350259f, -0.211324856f},
                                           {0.577350259f, 0.577350259f, 0.577350259f},
                                           {-0.211324856f, -0.577350259f, 0.788675129f}};

    const float vector[3] = {1.0f, 1.0f, 1.0f}; /* Arbitrary Value. */
    float normalized_vector[3];
    normalize_v3_v3(normalized_vector, vector);
    vec_roll_to_mat3_normalized(normalized_vector, roll, roll_mat);
    EXPECT_M3_NEAR(roll_mat, expected_roll_mat, FLT_EPSILON);
  }
}

class BKE_armature_find_selected_bones_test : public testing::Test {
 protected:
  bArmature arm;
  Bone bone1, bone2, bone3;

  void SetUp() override
  {
    strcpy(bone1.name, "bone1");
    strcpy(bone2.name, "bone2");
    strcpy(bone3.name, "bone3");

    arm.bonebase = {nullptr, nullptr};
    bone1.childbase = {nullptr, nullptr};
    bone2.childbase = {nullptr, nullptr};
    bone3.childbase = {nullptr, nullptr};

    BLI_addtail(&arm.bonebase, &bone1);     // bone1 is root bone
    BLI_addtail(&arm.bonebase, &bone2);     // bone2 is root bone
    BLI_addtail(&bone2.childbase, &bone3);  // bone3 has bone2 as parent

    /* Make sure the armature & its bones are visible, to make them selectable. */
    arm.layer = bone1.layer = bone2.layer = bone3.layer = 1;
  }
};

TEST_F(BKE_armature_find_selected_bones_test, some_bones_selected)
{
  bone1.flag = BONE_SELECTED;
  bone2.flag = 0;
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
  bone1.flag = bone2.flag = bone3.flag = 0;

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

}  // namespace blender::bke::tests
