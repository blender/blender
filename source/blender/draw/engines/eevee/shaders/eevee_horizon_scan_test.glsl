/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#include "eevee_horizon_scan_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_test_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define TEST(a, b) if (true)

void main()
{
  TEST(eevee_horizon_scan, Bitmask)
  {
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-1.0f, -1.0f)), 0x00000000u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-1.0f, -0.97f)), 0x00000001u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-1.0f, -0.5f)), 0x000000FFu);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-1.0f, 0.0f)), 0x0000FFFFu);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-0.5f, 0.0f)), 0x0000FF00u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-0.5f, 0.5f)), 0x00FFFF00u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(0.0f, 0.5f)), 0x00FF0000u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(0.0f, 1.0f)), 0xFFFF0000u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(0.5f, 1.0f)), 0xFF000000u);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-1.0f, 1.0f)), 0xFFFFFFFFu);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(-2.0f, 2.0f)), 0xFFFFFFFFu);
    EXPECT_EQ(horizon_scan_angles_to_bitmask(M_PI_2 * float2(0.2f, 0.2f)), 0x00000000u);
  }

  TEST(eevee_horizon_scan, UniformOcclusion)
  {
    constexpr float esp = 1e-4f;
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0x00000000u), 1.0f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0xFFFFFFFFu), 0.0f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0xFFFF0000u), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0x0000FFFFu), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0xAAAAAAAAu), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_uniform(0x55555555u), 0.5f, esp);
  }

  TEST(eevee_horizon_scan, CosineOcclusion)
  {
    constexpr float esp = 1e-5f;
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0x00000000u), 1.0f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0xFFFFFFFFu), 0.0f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0xFFFF0000u), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0x0000FFFFu), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0xAAAA5555u), 0.5f, esp);
    EXPECT_NEAR(horizon_scan_bitmask_to_occlusion_cosine(0x5555AAAAu), 0.5f, esp);
  }
}
