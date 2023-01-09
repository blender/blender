/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 5

#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_test_lib.glsl)

#define TEST(a, b) if (true)

void main()
{
  TEST(math_matrix, MatrixInverse)
  {
    mat3x3 mat = mat3x3_diagonal(2);
    mat3x3 inv = invert(mat);
    mat3x3 expect = mat3x3_diagonal(0.5f);
    EXPECT_NEAR(inv, expect, 1e-5f);

    bool success;
    mat3x3 m2 = mat3x3_all(1);
    mat3x3 inv2 = invert(m2, success);
    mat3x3 expect2 = mat3x3_all(0);
    EXPECT_NEAR(inv2, expect2, 1e-5f);
    EXPECT_FALSE(success);
  }

  TEST(math_matrix, MatrixDeterminant)
  {
    mat2x2 m2 = mat2x2(vec2(1, 2), vec2(3, 4));
    mat3x3 m3 = mat3x3(vec3(1, 2, 3), vec3(-3, 4, -5), vec3(5, -6, 7));
    mat4x4 m4 = mat4x4(vec4(1, 2, -3, 3), vec4(3, 4, -5, 3), vec4(5, 6, 7, -3), vec4(5, 6, 7, 1));
    EXPECT_NEAR(determinant(m2), -2.0f, 1e-8f);
    EXPECT_NEAR(determinant(m3), -16.0f, 1e-8f);
    EXPECT_NEAR(determinant(m4), -112.0f, 1e-8f);
  }

  TEST(math_matrix, MatrixAdjoint)
  {
    mat2x2 m2 = mat2x2(vec2(1, 2), vec2(3, 4));
    mat3x3 m3 = mat3x3(vec3(1, 2, 3), vec3(-3, 4, -5), vec3(5, -6, 7));
    mat4x4 m4 = mat4x4(vec4(1, 2, -3, 3), vec4(3, 4, -5, 3), vec4(5, 6, 7, -3), vec4(5, 6, 7, 1));
    mat2x2 expect2 = transpose(mat2x2(vec2(4, -3), vec2(-2, 1)));
    mat3x3 expect3 = transpose(mat3x3(vec3(-2, -4, -2), vec3(-32, -8, 16), vec3(-22, -4, 10)));
    mat4x4 expect4 = transpose(mat4x4(vec4(232, -184, -8, -0),
                                      vec4(-128, 88, 16, 0),
                                      vec4(80, -76, 4, 28),
                                      vec4(-72, 60, -12, -28)));
    EXPECT_NEAR(adjoint(m2), expect2, 1e-8f);
    EXPECT_NEAR(adjoint(m3), expect3, 1e-8f);
    EXPECT_NEAR(adjoint(m4), expect4, 1e-8f);
  }

  TEST(math_matrix, MatrixInit)
  {
    mat4x4 expect;

    mat4x4 m = from_location(vec3(1, 2, 3));
    expect = mat4x4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));

    expect = transpose(mat4x4(vec4(0.411982, -0.833738, -0.36763, 0),
                              vec4(-0.0587266, -0.426918, 0.902382, 0),
                              vec4(-0.909297, -0.350175, -0.224845, 0),
                              vec4(0, 0, 0, 1)));
    EulerXYZ euler = EulerXYZ(1, 2, 3);
    Quaternion quat = to_quaternion(euler);
    AxisAngle axis_angle = to_axis_angle(euler);
    m = mat4(from_rotation(euler));
    EXPECT_NEAR(m, expect, 1e-5);
    m = mat4(from_rotation(quat));
    EXPECT_NEAR(m, expect, 1e-5);
    m = mat4(from_rotation(axis_angle));
    EXPECT_NEAR(m, expect, 3e-4); /* Has some precision issue on some platform. */

    m = from_scale(vec4(1, 2, 3, 4));
    expect = mat4x4(vec4(1, 0, 0, 0), vec4(0, 2, 0, 0), vec4(0, 0, 3, 0), vec4(0, 0, 0, 4));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));

    m = mat4(from_scale(vec3(1, 2, 3)));
    expect = mat4x4(vec4(1, 0, 0, 0), vec4(0, 2, 0, 0), vec4(0, 0, 3, 0), vec4(0, 0, 0, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));

    m = mat4(from_scale(vec2(1, 2)));
    expect = mat4x4(vec4(1, 0, 0, 0), vec4(0, 2, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));

    m = from_loc_rot(vec3(1, 2, 3), EulerXYZ(1, 2, 3));
    expect = mat4x4(vec4(0.411982, -0.0587266, -0.909297, 0),
                    vec4(-0.833738, -0.426918, -0.350175, 0),
                    vec4(-0.36763, 0.902382, -0.224845, 0),
                    vec4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));

    m = from_loc_rot_scale(vec3(1, 2, 3), EulerXYZ(1, 2, 3), vec3(1, 2, 3));
    expect = mat4x4(vec4(0.411982, -0.0587266, -0.909297, 0),
                    vec4(-1.66748, -0.853835, -0.700351, 0),
                    vec4(-1.10289, 2.70714, -0.674535, 0),
                    vec4(1, 2, 3, 1));
    EXPECT_TRUE(is_equal(m, expect, 0.00001));
  }

  TEST(math_matrix, MatrixModify)
  {
    const float epsilon = 1e-6;
    mat4x4 result, expect;
    mat4x4 m1 = mat4x4(vec4(0, 3, 0, 0), vec4(2, 0, 0, 0), vec4(0, 0, 2, 0), vec4(0, 0, 0, 1));

    expect = mat4x4(vec4(0, 3, 0, 0), vec4(2, 0, 0, 0), vec4(0, 0, 2, 0), vec4(4, 9, 2, 1));
    result = translate(m1, vec3(3, 2, 1));
    EXPECT_NEAR(result, expect, epsilon);

    expect = mat4x4(vec4(0, 3, 0, 0), vec4(2, 0, 0, 0), vec4(0, 0, 2, 0), vec4(4, 0, 0, 1));
    result = translate(m1, vec2(0, 2));
    EXPECT_NEAR(result, expect, epsilon);

    expect = mat4x4(vec4(0, 0, -2, 0), vec4(2, 0, 0, 0), vec4(0, 3, 0, 0), vec4(0, 0, 0, 1));
    result = rotate(m1, AxisAngle(vec3(0, 1, 0), M_PI_2));
    EXPECT_NEAR(result, expect, epsilon);

    expect = mat4x4(vec4(0, 9, 0, 0), vec4(4, 0, 0, 0), vec4(0, 0, 8, 0), vec4(0, 0, 0, 1));
    result = scale(m1, vec3(3, 2, 4));
    EXPECT_NEAR(result, expect, epsilon);

    expect = mat4x4(vec4(0, 9, 0, 0), vec4(4, 0, 0, 0), vec4(0, 0, 2, 0), vec4(0, 0, 0, 1));
    result = scale(m1, vec2(3, 2));
    EXPECT_NEAR(result, expect, epsilon);
  }

  TEST(math_matrix, MatrixCompareTest)
  {
    mat4x4 m1 = mat4x4(vec4(0, 3, 0, 0), vec4(2, 0, 0, 0), vec4(0, 0, 2, 0), vec4(0, 0, 0, 1));
    mat4x4 m2 = mat4x4(
        vec4(0, 3.001, 0, 0), vec4(1.999, 0, 0, 0), vec4(0, 0, 2.001, 0), vec4(0, 0, 0, 1.001));
    mat4x4 m3 = mat4x4(
        vec4(0, 3.001, 0, 0), vec4(1, 1, 0, 0), vec4(0, 0, 2.001, 0), vec4(0, 0, 0, 1.001));
    mat4x4 m4 = mat4x4(vec4(0, 1, 0, 0), vec4(1, 0, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
    mat4x4 m5 = mat4x4(vec4(0, 0, 0, 0), vec4(0, 0, 0, 0), vec4(0, 0, 0, 0), vec4(0, 0, 0, 0));
    mat4x4 m6 = mat4x4(vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
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
    mat4x4 m = mat4x4(vec4(0, 3, 0, 0), vec4(2, 0, 0, 0), vec4(0, 0, 2, 0), vec4(0, 1, 0, 1));
    EulerXYZ expect_eul = EulerXYZ(0, 0, M_PI_2);
    Quaternion expect_qt = Quaternion(0, -M_SQRT1_2, M_SQRT1_2, 0);
    vec3 expect_scale = vec3(3, 2, 2);
    vec3 expect_location = vec3(0, 1, 0);

    EXPECT_NEAR(as_vec3(to_euler(m)), as_vec3(expect_eul), 0.0002);
    EXPECT_NEAR(as_vec4(to_quaternion(m)), as_vec4(expect_qt), 0.0002);
    EXPECT_NEAR(to_scale(m), expect_scale, 0.00001);

    vec4 expect_sz = vec4(3, 2, 2, M_SQRT2);
    vec4 size;
    mat4x4 m1 = normalize_and_get_size(m, size);
    EXPECT_TRUE(is_unit_scale(m1));
    EXPECT_NEAR(size, expect_sz, 0.0002);

    mat4x4 m2 = normalize(m);
    EXPECT_TRUE(is_unit_scale(m2));

    EulerXYZ eul;
    Quaternion qt;
    vec3 scale;
    to_rot_scale(mat3x3(m), eul, scale);
    to_rot_scale(mat3x3(m), qt, scale);
    EXPECT_NEAR(scale, expect_scale, 0.00001);
    EXPECT_NEAR(as_vec4(qt), as_vec4(expect_qt), 0.0002);
    EXPECT_NEAR(as_vec3(eul), as_vec3(expect_eul), 0.0002);

    vec3 loc;
    to_loc_rot_scale(m, loc, eul, scale);
    to_loc_rot_scale(m, loc, qt, scale);
    EXPECT_NEAR(scale, expect_scale, 0.00001);
    EXPECT_NEAR(loc, expect_location, 0.00001);
    EXPECT_NEAR(as_vec4(qt), as_vec4(expect_qt), 0.0002);
    EXPECT_NEAR(as_vec3(eul), as_vec3(expect_eul), 0.0002);
  }

  TEST(math_matrix, MatrixTranspose)
  {
    mat4x4 m = mat4x4(vec4(1, 2, 3, 4), vec4(5, 6, 7, 8), vec4(9, 1, 2, 3), vec4(2, 5, 6, 7));
    mat4x4 expect = mat4x4(vec4(1, 5, 9, 2), vec4(2, 6, 1, 5), vec4(3, 7, 2, 6), vec4(4, 8, 3, 7));
    EXPECT_EQ(transpose(m), expect);
  }

  TEST(math_matrix, MatrixInterpolationRegular)
  {
    /* Test 4x4 matrix interpolation without singularity, i.e. without axis flip. */

    /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs.
     */
    /* This matrix represents T=(0.1, 0.2, 0.3), R=(40, 50, 60) degrees, S=(0.7, 0.8, 0.9) */
    mat4x4 m2 = transpose(mat4x4(vec4(0.224976f, -0.333770f, 0.765074f, 0.100000f),
                                 vec4(0.389669f, 0.647565f, 0.168130f, 0.200000f),
                                 vec4(-0.536231f, 0.330541f, 0.443163f, 0.300000f),
                                 vec4(0.000000f, 0.000000f, 0.000000f, 1.000000f)));
    mat4x4 m1 = mat4x4_identity();
    mat4x4 result;
    const float epsilon = 2e-5;
    result = interpolate_fast(m1, m2, 0.0f);
    EXPECT_NEAR(result, m1, epsilon);
    result = interpolate_fast(m1, m2, 1.0f);
    EXPECT_NEAR(result, m2, epsilon);

    /* This matrix is based on the current implementation of the code, and isn't guaranteed to be
     * correct. It's just consistent with the current implementation. */
    mat4x4 expect = transpose(mat4x4(vec4(0.690643f, -0.253244f, 0.484996f, 0.050000f),
                                     vec4(0.271924f, 0.852623f, 0.012348f, 0.100000f),
                                     vec4(-0.414209f, 0.137484f, 0.816778f, 0.150000f),
                                     vec4(0.000000f, 0.000000f, 0.000000f, 1.000000f)));
    result = interpolate_fast(m1, m2, 0.5f);
    EXPECT_NEAR(result, expect, epsilon);
  }

  TEST(math_matrix, MatrixTransform)
  {
    vec3 expect, result;
    const vec3 p = vec3(1, 2, 3);
    mat4x4 m4 = from_loc_rot(vec3(10, 0, 0), EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
    mat3x3 m3 = from_rotation(EulerXYZ(M_PI_2, M_PI_2, M_PI_2));
    mat4x4 pers4 = projection_perspective(-0.1f, 0.1f, -0.1f, 0.1f, -0.1f, -1.0f);
    mat3x3 pers3 = mat3x3(vec3(1, 0, 0.1f), vec3(0, 1, 0.1f), vec3(0, 0.1f, 1));

    expect = vec3(13, 2, -1);
    result = transform_point(m4, p);
    EXPECT_NEAR(result, expect, 1e-2);

    expect = vec3(3, 2, -1);
    result = transform_point(m3, p);
    EXPECT_NEAR(result, expect, 1e-5);

    result = transform_direction(m4, p);
    EXPECT_NEAR(result, expect, 1e-5);

    result = transform_direction(m3, p);
    EXPECT_NEAR(result, expect, 1e-5);

    expect = vec3(-0.5, -1, -1.7222222);
    result = project_point(pers4, p);
    EXPECT_NEAR(result, expect, 1e-5);

    vec2 expect2 = vec2(0.76923, 1.61538);
    vec2 result2 = project_point(pers3, p.xy);
    EXPECT_NEAR(result2, expect2, 1e-5);
  }

  TEST(math_matrix, MatrixProjection)
  {
    mat4x4 expect;
    mat4x4 ortho = projection_orthographic(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
    mat4x4 pers1 = projection_perspective(-0.2f, 0.3f, -0.2f, 0.4f, -0.2f, -0.5f);
    mat4x4 pers2 = projection_perspective_fov(
        atan(-0.2f), atan(0.3f), atan(-0.2f), atan(0.4f), -0.2f, -0.5f);

    expect = transpose(mat4x4(vec4(4.0f, 0.0f, 0.0f, -0.2f),
                              vec4(0.0f, 3.33333f, 0.0f, -0.333333f),
                              vec4(0.0f, 0.0f, 6.66667f, -2.33333f),
                              vec4(0.0f, 0.0f, 0.0f, 1.0f)));
    EXPECT_NEAR(ortho, expect, 1e-5);

    expect = transpose(mat4x4(vec4(-0.8f, 0.0f, 0.2f, 0.0f),
                              vec4(0.0f, -0.666667f, 0.333333f, 0.0f),
                              vec4(0.0f, 0.0f, -2.33333f, 0.666667f),
                              vec4(0.0f, 0.0f, -1.0f, 1.0f)));
    EXPECT_NEAR(pers1, expect, 1e-4);

    expect = transpose(mat4x4(vec4(4.0f, 0.0f, 0.2f, 0.0f),
                              vec4(0.0f, 3.33333f, 0.333333f, 0.0f),
                              vec4(0.0f, 0.0f, -2.33333f, 0.666667f),
                              vec4(0.0f, 0.0f, -1.0f, 1.0f)));
    EXPECT_NEAR(pers2, expect, 1e-4);
  }
}
