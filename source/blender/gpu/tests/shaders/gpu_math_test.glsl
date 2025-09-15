/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#include "gpu_shader_test_lib.glsl"

#include "gpu_shader_math_axis_angle_lib.glsl"
#include "gpu_shader_math_euler_lib.glsl"
#include "gpu_shader_math_matrix_adjoint_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_math_matrix_interpolate_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_matrix_normalize_lib.glsl"
#include "gpu_shader_math_matrix_projection_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_quaternion_lib.glsl"
#include "gpu_shader_math_rotation_conversion_lib.glsl"
#include "gpu_shader_math_rotation_lib.glsl"

#define TEST(a, b) if (true)

void main()
{
  TEST(math_matrix, MatrixInverse)
  {
    float3x3 mat = mat3x3_diagonal(2);
    float3x3 inv = invert(mat);
    float3x3 expect = mat3x3_diagonal(0.5f);
    EXPECT_NEAR(inv, expect, 1e-5f);

    bool success;
    float3x3 m2 = mat3x3_all(1);
    float3x3 inv2 = invert(m2, success);
    float3x3 expect2 = mat3x3_all(0);
    EXPECT_NEAR(inv2, expect2, 1e-5f);
    EXPECT_FALSE(success);
  }

  TEST(math_matrix, MatrixDeterminant)
  {
    float2x2 m2 = float2x2(float2(1, 2), float2(3, 4));
    float3x3 m3 = float3x3(float3(1, 2, 3), float3(-3, 4, -5), float3(5, -6, 7));
    float4x4 m4 = float4x4(
        float4(1, 2, -3, 3), float4(3, 4, -5, 3), float4(5, 6, 7, -3), float4(5, 6, 7, 1));
    EXPECT_NEAR(determinant(m2), -2.0f, 1e-8f);
    EXPECT_NEAR(determinant(m3), -16.0f, 1e-8f);
    EXPECT_NEAR(determinant(m4), -112.0f, 1e-8f);
  }

  TEST(math_matrix, MatrixAdjoint)
  {
    float2x2 m2 = float2x2(float2(1, 2), float2(3, 4));
    float3x3 m3 = float3x3(float3(1, 2, 3), float3(-3, 4, -5), float3(5, -6, 7));
    float4x4 m4 = float4x4(
        float4(1, 2, -3, 3), float4(3, 4, -5, 3), float4(5, 6, 7, -3), float4(5, 6, 7, 1));
    float2x2 expect2 = transpose(float2x2(float2(4, -3), float2(-2, 1)));
    float3x3 expect3 = transpose(
        float3x3(float3(-2, -4, -2), float3(-32, -8, 16), float3(-22, -4, 10)));
    float4x4 expect4 = transpose(float4x4(float4(232, -184, -8, -0),
                                          float4(-128, 88, 16, 0),
                                          float4(80, -76, 4, 28),
                                          float4(-72, 60, -12, -28)));
    EXPECT_NEAR(adjoint(m2), expect2, 1e-8f);
    EXPECT_NEAR(adjoint(m3), expect3, 1e-8f);
    EXPECT_NEAR(adjoint(m4), expect4, 1e-8f);
  }

  TEST(math_matrix, MatrixInit)
  {
    float4x4 expect;

    float4x4 m = from_location(float3(1, 2, 3));
    expect = float4x4(
        float4(1, 0, 0, 0), float4(0, 1, 0, 0), float4(0, 0, 1, 0), float4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));

    expect = transpose(float4x4(float4(0.411982f, -0.833738f, -0.36763f, 0),
                                float4(-0.0587266f, -0.426918f, 0.902382f, 0),
                                float4(-0.909297f, -0.350175f, -0.224845f, 0),
                                float4(0, 0, 0, 1)));
    EulerXYZ euler = EulerXYZ(1, 2, 3);
    Quaternion quat = to_quaternion(euler);
    AxisAngle axis_angle = to_axis_angle(euler);
    m = to_float4x4(from_rotation(euler));
    EXPECT_NEAR(m, expect, 1e-5f);
    m = to_float4x4(from_rotation(quat));
    EXPECT_NEAR(m, expect, 1e-5f);
    m = to_float4x4(from_rotation(axis_angle));
    EXPECT_NEAR(m, expect, 3e-4f); /* Has some precision issue on some platform. */

    m = from_scale(float4(1, 2, 3, 4));
    expect = float4x4(
        float4(1, 0, 0, 0), float4(0, 2, 0, 0), float4(0, 0, 3, 0), float4(0, 0, 0, 4));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));

    m = to_float4x4(from_scale(float3(1, 2, 3)));
    expect = float4x4(
        float4(1, 0, 0, 0), float4(0, 2, 0, 0), float4(0, 0, 3, 0), float4(0, 0, 0, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));

    m = to_float4x4(from_scale(float2(1, 2)));
    expect = float4x4(
        float4(1, 0, 0, 0), float4(0, 2, 0, 0), float4(0, 0, 1, 0), float4(0, 0, 0, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));

    m = from_loc_rot(float3(1, 2, 3), EulerXYZ(1, 2, 3));
    expect = float4x4(float4(0.411982f, -0.0587266f, -0.909297f, 0),
                      float4(-0.833738f, -0.426918f, -0.350175f, 0),
                      float4(-0.36763f, 0.902382f, -0.224845f, 0),
                      float4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));

    m = from_loc_rot_scale(float3(1, 2, 3), EulerXYZ(1, 2, 3), float3(1, 2, 3));
    expect = float4x4(float4(0.411982f, -0.0587266f, -0.909297f, 0),
                      float4(-1.66748f, -0.853835f, -0.700351f, 0),
                      float4(-1.10289f, 2.70714f, -0.674535f, 0),
                      float4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001f));
  }

  TEST(math_matrix, MatrixModify)
  {
    constexpr float epsilon = 1e-6f;
    float4x4 result, expect;
    float4x4 m1 = float4x4(
        float4(0, 3, 0, 0), float4(2, 0, 0, 0), float4(0, 0, 2, 0), float4(0, 0, 0, 1));

    expect = float4x4(
        float4(0, 3, 0, 0), float4(2, 0, 0, 0), float4(0, 0, 2, 0), float4(4, 9, 2, 1));
    result = translate(m1, float3(3, 2, 1));
    EXPECT_NEAR(result, expect, epsilon);

    expect = float4x4(
        float4(0, 3, 0, 0), float4(2, 0, 0, 0), float4(0, 0, 2, 0), float4(4, 0, 0, 1));
    result = translate(m1, float2(0, 2));
    EXPECT_NEAR(result, expect, epsilon);

    expect = float4x4(
        float4(0, 0, -2, 0), float4(2, 0, 0, 0), float4(0, 3, 0, 0), float4(0, 0, 0, 1));
    result = rotate(m1, AxisAngle(float3(0, 1, 0), M_PI_2));
    EXPECT_NEAR(result, expect, epsilon);

    expect = float4x4(
        float4(0, 9, 0, 0), float4(4, 0, 0, 0), float4(0, 0, 8, 0), float4(0, 0, 0, 1));
    result = scale(m1, float3(3, 2, 4));
    EXPECT_NEAR(result, expect, epsilon);

    expect = float4x4(
        float4(0, 9, 0, 0), float4(4, 0, 0, 0), float4(0, 0, 2, 0), float4(0, 0, 0, 1));
    result = scale(m1, float2(3, 2));
    EXPECT_NEAR(result, expect, epsilon);
  }

  TEST(math_matrix, MatrixCompareTest)
  {
    float4x4 m1 = float4x4(
        float4(0, 3, 0, 0), float4(2, 0, 0, 0), float4(0, 0, 2, 0), float4(0, 0, 0, 1));
    float4x4 m2 = float4x4(float4(0, 3.001f, 0, 0),
                           float4(1.999f, 0, 0, 0),
                           float4(0, 0, 2.001f, 0),
                           float4(0, 0, 0, 1.001f));
    float4x4 m3 = float4x4(float4(0, 3.001f, 0, 0),
                           float4(1, 1, 0, 0),
                           float4(0, 0, 2.001f, 0),
                           float4(0, 0, 0, 1.001f));
    float4x4 m4 = float4x4(
        float4(0, 1, 0, 0), float4(1, 0, 0, 0), float4(0, 0, 1, 0), float4(0, 0, 0, 1));
    float4x4 m5 = float4x4(
        float4(0, 0, 0, 0), float4(0, 0, 0, 0), float4(0, 0, 0, 0), float4(0, 0, 0, 0));
    float4x4 m6 = float4x4(
        float4(1, 0, 0, 0), float4(0, 1, 0, 0), float4(0, 0, 1, 0), float4(0, 0, 0, 1));
    EXPECT_TRUE(is_equal(m1, m2, 0.01f));
    EXPECT_FALSE(is_equal(m1, m2, 0.0001f));
    EXPECT_FALSE(is_equal(m1, m3, 0.01f));
    EXPECT_TRUE(is_orthogonal(m1));
    EXPECT_FALSE(is_orthogonal(m3));
    EXPECT_TRUE(is_orthonormal(m4));
    EXPECT_FALSE(is_orthonormal(m1));
    EXPECT_FALSE(is_orthonormal(m3));
    EXPECT_FALSE(is_uniformly_scaled(m1));
    EXPECT_TRUE(is_uniformly_scaled(m4));
    EXPECT_FALSE(is_zero(m4));
    EXPECT_TRUE(is_zero(m5));
    EXPECT_TRUE(is_negative(m4));
    EXPECT_FALSE(is_negative(m5));
    EXPECT_FALSE(is_negative(m6));
  }

  TEST(math_matrix, MatrixMethods)
  {
    float4x4 m = float4x4(
        float4(0, 3, 0, 0), float4(2, 0, 0, 0), float4(0, 0, 2, 0), float4(0, 1, 0, 1));
    EulerXYZ expect_eul = EulerXYZ(0, 0, M_PI_2);
    Quaternion expect_qt = Quaternion(0, -M_SQRT1_2, M_SQRT1_2, 0);
    float3 expect_scale = float3(3, 2, 2);
    float3 expect_location = float3(0, 1, 0);

    EXPECT_NEAR(to_euler(m).as_float3(), expect_eul.as_float3(), 0.0002f);
    EXPECT_NEAR(to_quaternion(m).as_float4(), expect_qt.as_float4(), 0.0002f);
    EXPECT_NEAR(to_scale(m), expect_scale, 0.00001f);

    float4 expect_size = float4(3, 2, 2, M_SQRT2);
    float4 size;
    float4x4 m1 = normalize_and_get_size(m, size);
    EXPECT_TRUE(is_unit_scale(m1));
    EXPECT_NEAR(size, expect_size, 0.0002f);

    float4x4 m2 = normalize(m);
    EXPECT_TRUE(is_unit_scale(m2));

    EulerXYZ eul;
    Quaternion qt;
    float3 scale;
    to_rot_scale(to_float3x3(m), eul, scale);
    to_rot_scale(to_float3x3(m), qt, scale);
    EXPECT_NEAR(scale, expect_scale, 0.00001f);
    EXPECT_NEAR(qt.as_float4(), expect_qt.as_float4(), 0.0002f);
    EXPECT_NEAR(eul.as_float3(), expect_eul.as_float3(), 0.0002f);

    float3 loc;
    to_loc_rot_scale(m, loc, eul, scale);
    to_loc_rot_scale(m, loc, qt, scale);
    EXPECT_NEAR(scale, expect_scale, 0.00001f);
    EXPECT_NEAR(loc, expect_location, 0.00001f);
    EXPECT_NEAR(qt.as_float4(), expect_qt.as_float4(), 0.0002f);
    EXPECT_NEAR(eul.as_float3(), expect_eul.as_float3(), 0.0002f);
  }

  TEST(math_matrix, MatrixTranspose)
  {
    float4x4 m = float4x4(
        float4(1, 2, 3, 4), float4(5, 6, 7, 8), float4(9, 1, 2, 3), float4(2, 5, 6, 7));
    float4x4 expect = float4x4(
        float4(1, 5, 9, 2), float4(2, 6, 1, 5), float4(3, 7, 2, 6), float4(4, 8, 3, 7));
    EXPECT_EQ(transpose(m), expect);
  }

  TEST(math_matrix, MatrixInterpolationRegular)
  {
    /* Test 4x4 matrix interpolation without singularity, i.e. without axis flip. */

    /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs.
     */
    /* This matrix represents T=(0.1, 0.2, 0.3), R=(40, 50, 60) degrees, S=(0.7, 0.8, 0.9) */
    float4x4 m2 = transpose(float4x4(float4(0.224976f, -0.333770f, 0.765074f, 0.100000f),
                                     float4(0.389669f, 0.647565f, 0.168130f, 0.200000f),
                                     float4(-0.536231f, 0.330541f, 0.443163f, 0.300000f),
                                     float4(0.000000f, 0.000000f, 0.000000f, 1.000000f)));
    float4x4 m1 = mat4x4_identity();
    float4x4 result;
    constexpr float epsilon = 2e-5f;
    result = interpolate_fast(m1, m2, 0.0f);
    EXPECT_NEAR(result, m1, epsilon);
    result = interpolate_fast(m1, m2, 1.0f);
    EXPECT_NEAR(result, m2, epsilon);

    /* This matrix is based on the current implementation of the code, and isn't guaranteed to be
     * correct. It's just consistent with the current implementation. */
    float4x4 expect = transpose(float4x4(float4(0.690643f, -0.253244f, 0.484996f, 0.050000f),
                                         float4(0.271924f, 0.852623f, 0.012348f, 0.100000f),
                                         float4(-0.414209f, 0.137484f, 0.816778f, 0.150000f),
                                         float4(0.000000f, 0.000000f, 0.000000f, 1.000000f)));
    result = interpolate_fast(m1, m2, 0.5f);
    EXPECT_NEAR(result, expect, epsilon);
  }

  TEST(math_matrix, MatrixTransform)
  {
    float3 expect, result;
    constexpr float3 p = float3(1, 2, 3);
    float4x4 m4 = from_loc_rot(float3(10, 0, 0), EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
    float3x3 m3 = from_rotation(EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
    float4x4 pers4 = projection_perspective(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, -1.0f);
    float3x3 pers3 = float3x3(float3(1, 0, 0.1f), float3(0, 1, 0.1f), float3(0, 0.1f, 1));

    expect = float3(13, 2, -1);
    result = transform_point(m4, p);
    EXPECT_NEAR(result, expect, 1e-2f);

    expect = float3(3, 2, -1);
    result = transform_point(m3, p);
    EXPECT_NEAR(result, expect, 1e-5f);

    result = transform_direction(m4, p);
    EXPECT_NEAR(result, expect, 1e-5f);

    result = transform_direction(m3, p);
    EXPECT_NEAR(result, expect, 1e-5f);

    expect = float3(-0.333333f, -0.666667f, -1.14815f);
    result = project_point(pers4, p);
    EXPECT_NEAR(result, expect, 1e-5f);

    float2 expect2 = float2(0.76923f, 1.61538f);
    float2 result2 = project_point(pers3, p.xy);
    EXPECT_NEAR(result2, expect2, 1e-5f);
  }

  TEST(math_matrix, MatrixProjection)
  {
    float4x4 expect;
    float4x4 ortho = projection_orthographic(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
    float4x4 pers1 = projection_perspective(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
    float4x4 pers2 = projection_perspective_fov(
        atan(-0.2f), atan(0.3f), atan(-0.2f), atan(0.4f), -0.2f, -0.5f);

    expect = transpose(float4x4(float4(4.0f, 0.0f, 0.0f, -0.2f),
                                float4(0.0f, 3.33333f, 0.0f, -0.333333f),
                                float4(0.0f, 0.0f, 6.66667f, -2.33333f),
                                float4(0.0f, 0.0f, 0.0f, 1.0f)));
    EXPECT_NEAR(ortho, expect, 1e-5f);

    expect = transpose(float4x4(float4(-0.8f, 0.0f, 0.2f, 0.0f),
                                float4(0.0f, -0.666667f, 0.333333f, 0.0f),
                                float4(0.0f, 0.0f, -2.33333f, 0.666667f),
                                float4(0.0f, 0.0f, -1.0f, 0.0f)));
    EXPECT_NEAR(pers1, expect, 1e-4f);

    expect = transpose(float4x4(float4(4.0f, 0.0f, 0.2f, 0.0f),
                                float4(0.0f, 3.33333f, 0.333333f, 0.0f),
                                float4(0.0f, 0.0f, -2.33333f, 0.666667f),
                                float4(0.0f, 0.0f, -1.0f, 0.0f)));
    EXPECT_NEAR(pers2, expect, 1e-4f);
  }

  TEST(math_matrix, OrderedInt)
  {
    /* Identity. */
    EXPECT_EQ(orderedIntBitsToFloat(floatBitsToOrderedInt(0.5f)), 0.5f);
    EXPECT_EQ(orderedIntBitsToFloat(floatBitsToOrderedInt(-0.5f)), -0.5f);
    EXPECT_EQ(orderedIntBitsToFloat(floatBitsToOrderedInt(0.0f)), 0.0f);
    EXPECT_EQ(orderedIntBitsToFloat(floatBitsToOrderedInt(-0.0f)), -0.0f);

    EXPECT_GE(floatBitsToOrderedInt(-0.5f), floatBitsToOrderedInt(-1.0f));
    EXPECT_LE(floatBitsToOrderedInt(0.5f), floatBitsToOrderedInt(1.0f));
    EXPECT_LE(floatBitsToOrderedInt(-0.5f), floatBitsToOrderedInt(1.0f));
    EXPECT_GE(floatBitsToOrderedInt(0.5f), floatBitsToOrderedInt(-1.0f));
    EXPECT_LE(floatBitsToOrderedInt(-0.0f), floatBitsToOrderedInt(0.0f));
  }
}
