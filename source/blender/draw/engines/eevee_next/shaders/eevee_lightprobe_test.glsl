/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 5

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_test_lib.glsl)

#define TEST(a, b) if (true)

void main()
{
  TEST(eevee_lightprobe, IrradianceBrickIndex)
  {
    float near = 0.5, far = 1.0;
    mat4 pers_mat = projection_perspective(-near, near, -near, near, near, far);
    mat4 normal_mat = invert(transpose(pers_mat));

    LightData light;
    light.clip_near = floatBitsToInt(near);
    light.clip_far = floatBitsToInt(far);
    light.influence_radius_max = far;
    light.type = LIGHT_SPOT;
    light.normal_mat_packed.x = normal_mat[3][2];
    light.normal_mat_packed.y = normal_mat[3][3];

    vec2 atlas_size = vec2(SHADOW_TILEMAP_RES);
    {
      /* Simulate a "2D" plane crossing the frustum diagonally. */
      vec3 lP0 = vec3(-1.0, 0.0, -1.0);
      vec3 lP1 = vec3(0.5, 0.0, -0.5);
      vec3 lTg = normalize(lP1 - lP0);
      vec3 lNg = vec3(-lTg.z, 0.0, lTg.x);

      float expect = 1.0 / (SHADOW_TILEMAP_RES * SHADOW_PAGE_RES);
      EXPECT_NEAR(shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 0), expect, 1e-4);
      EXPECT_NEAR(
          shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 1), expect * 2.0, 1e-4);
      EXPECT_NEAR(
          shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 2), expect * 4.0, 1e-4);
    }
    {
      /* Simulate a "2D" plane crossing the near plane at the center diagonally. */
      vec3 lP0 = vec3(-1.0, 0.0, -1.0);
      vec3 lP1 = vec3(0.0, 0.0, -0.5);
      vec3 lTg = normalize(lP1 - lP0);
      vec3 lNg = vec3(-lTg.z, 0.0, lTg.x);

      float expect = 2.0 / (SHADOW_TILEMAP_RES * SHADOW_PAGE_RES);
      EXPECT_NEAR(shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 0), expect, 1e-4);
      EXPECT_NEAR(
          shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 1), expect * 2.0, 1e-4);
      EXPECT_NEAR(
          shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 2), expect * 4.0, 1e-4);
    }
    {
      /* Simulate a "2D" plane parallel to near clip plane. */
      vec3 lP0 = vec3(-1.0, 0.0, -0.75);
      vec3 lP1 = vec3(0.0, 0.0, -0.75);
      vec3 lTg = normalize(lP1 - lP0);
      vec3 lNg = vec3(-lTg.z, 0.0, lTg.x);

      EXPECT_NEAR(shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 0), 0.0, 1e-4);
      EXPECT_NEAR(shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 1), 0.0, 1e-4);
      EXPECT_NEAR(shadow_slope_bias_get(atlas_size, light, lNg, lP0, vec2(0.0), 2), 0.0, 1e-4);
    }
  }
}
