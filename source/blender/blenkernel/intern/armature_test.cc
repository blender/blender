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

static const float SCALE_EPSILON = 3.71e-5;
static const float ORTHO_EPSILON = 5e-5;

/** Test that the matrix is orthogonal, i.e. has no scale or shear within acceptable precision. */
static double EXPECT_M3_ORTHOGONAL(const float mat[3][3],
                                   double epsilon_scale,
                                   double epsilon_ortho)
{
  /* Do the checks in double precision to avoid precision issues in the checks themselves. */
  double dmat[3][3];
  copy_m3d_m3(dmat, mat);

  /* Check individual axis scaling. */
  EXPECT_NEAR(len_v3_db(dmat[0]), 1.0, epsilon_scale);
  EXPECT_NEAR(len_v3_db(dmat[1]), 1.0, epsilon_scale);
  EXPECT_NEAR(len_v3_db(dmat[2]), 1.0, epsilon_scale);

  /* Check orthogonality. */
  EXPECT_NEAR(dot_v3v3_db(dmat[0], dmat[1]), 0.0, epsilon_ortho);
  EXPECT_NEAR(dot_v3v3_db(dmat[0], dmat[2]), 0.0, epsilon_ortho);
  EXPECT_NEAR(dot_v3v3_db(dmat[1], dmat[2]), 0.0, epsilon_ortho);

  /* Check determinant to detect flipping and as a secondary volume change check. */
  double determinant = determinant_m3_array_db(dmat);

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
    /* The vector is renormalized to replicate the actual usage. */
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
  test_vec_roll_to_mat3_normalized(input, float(M_PI * 0.5), expected_roll_mat);
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

  printf("             Max determinant error %.10f at %f.\n", delta, tmax);
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

    BLI_addtail(&arm.bonebase, &bone1);    /* bone1 is root bone. */
    BLI_addtail(&arm.bonebase, &bone2);    /* bone2 is root bone. */
    BLI_addtail(&bone2.childbase, &bone3); /* bone3 has bone2 as parent. */

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
