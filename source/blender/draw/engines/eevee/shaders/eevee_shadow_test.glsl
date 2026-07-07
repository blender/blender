/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#include "infos/gpu_shader_test_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_shader_test)

#include "eevee_shadow_lib.glsl"

#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_test_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define TEST(a, b) if (true)

void set_clipmap_data(LightData &light,
                      int clipmap_lod_min,
                      int clipmap_lod_max,
                      float clipmap_origin_x,
                      float clipmap_origin_y)
{
  LightSunData sun_data = light.sun();
  sun_data.clipmap_origin = float2(clipmap_origin_x, clipmap_origin_y);
  sun_data.clipmap_lod_min = clipmap_lod_min;
  sun_data.clipmap_lod_max = clipmap_lod_max;
  light.sun() = sun_data;
}

void set_clipmap_base_offset(LightData &light, int2 clipmap_base_offset)
{
  LightSunData sun_data = light.sun();
  sun_data.clipmap_base_offset_pos = clipmap_base_offset;
  sun_data.clipmap_base_offset_neg = int2(0);
  light.sun() = sun_data;
}

void main()
{
  TEST(eevee_shadow, DirectionalMemberSet)
  {
    LightData light;
    set_clipmap_data(light, 1, 2, 3.0f, 4.0f);
    set_clipmap_base_offset(light, int2(5, 6));

    EXPECT_EQ(light.sun().clipmap_lod_min, 1);
    EXPECT_EQ(light.sun().clipmap_lod_max, 2);
    EXPECT_EQ(light.sun().clipmap_origin, float2(3.0f, 4.0f));
    EXPECT_EQ(light.sun().clipmap_base_offset_pos, int2(5, 6));
  }

  TEST(eevee_shadow, DirectionalClipmapLevel)
  {
    LightData light;
    light.type = LIGHT_SUN;
    set_clipmap_data(light, -5, 8, 0.0f, 0.0f);
    light.lod_bias = 0.0f;
    float fac = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 0.0f)), light.sun().clipmap_lod_min);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 0.49f)), 1);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 0.5f)), 1);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 0.51f)), 1);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 0.99f)), 2);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 1.0f)), 2);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 1.01f)), 2);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 12.5f)), 6);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 12.51f)), 6);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 15.9999f)), 6);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 16.0f)), 6);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 16.00001f)), 6);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 5000.0f)), light.sun().clipmap_lod_max);
    /* Produces NaN / Inf, Undefined behavior. */
    // EXPECT_EQ(shadow_directional_level(light, float3(FLT_MAX)),
    // light.sun().clipmap_lod_max);
  }

  TEST(eevee_shadow, DirectionalCascadeLevel)
  {
    LightData light;
    light.type = LIGHT_SUN_ORTHO;
    set_clipmap_data(light, 2, 8, 0.0f, 0.0f);
    float half_size = exp2(float(light.sun().clipmap_lod_min - 1));
    light.lod_bias = light.sun().clipmap_lod_min - 1;
    float fac = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * half_size * 0.0f, 0.0f, 0.0f)), 2);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * half_size * 0.5f, 0.0f, 0.0f)), 2);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * half_size * 1.0f, 0.0f, 0.0f)), 3);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * half_size * 1.5f, 0.0f, 0.0f)), 3);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * half_size * 2.0f, 0.0f, 0.0f)), 4);
    EXPECT_EQ(shadow_directional_level(light, float3(fac * 5000.0f)), light.sun().clipmap_lod_max);
    /* Produces NaN / Inf, Undefined behavior. */
    // EXPECT_EQ(shadow_directional_level(light, float3(FLT_MAX)),
    // light.sun().clipmap_lod_max);
  }

  TEST(eevee_shadow, DirectionalClipmapCoordinates)
  {
    ShadowCoordinates coords;
    float3 lP, camera_lP;

    LightData light;
    light.type = LIGHT_SUN;
    // clipmap_lod_min = 0; /* Range [-0.5f..0.5f]. */
    // clipmap_lod_max = 2; /* Range [-2..2]. */
    set_clipmap_data(light, 0, 2, 0.0f, 0.0f);

    light.tilemap_index = light.sun().clipmap_lod_min;
    light.object_to_world.x = float4(1.0f, 0.0f, 0.0f, 0.0f);
    light.object_to_world.y = float4(0.0f, 1.0f, 0.0f, 0.0f);
    light.object_to_world.z = float4(0.0f, 0.0f, 1.0f, 0.0f);
    light.lod_bias = 0;

    float lod_min_tile_size = exp2(float(light.sun().clipmap_lod_min)) / float(SHADOW_TILEMAP_RES);
    float lod_max_half_size = exp2(float(light.sun().clipmap_lod_max)) / 2.0f;

    camera_lP = float3(0.0f, 0.0f, 0.0f);
    /* Follows ShadowDirectional::end_sync(). */
    set_clipmap_base_offset(light, int2(round(camera_lP.xy / lod_min_tile_size)));
    EXPECT_EQ(light.sun().clipmap_base_offset_pos, int2(0));

    /* Test UVs and tile mapping. */

    lP = float3(1e-5f, 1e-5f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 2), 1e-3f);

    lP = float3(-1e-5f, -1e-5f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tilemap_tile, uint2((SHADOW_TILEMAP_RES / 2) - 1));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 2), 1e-3f);

    lP = float3(-0.5f, -0.5f, 0.0f); /* Min of first LOD. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tilemap_tile, uint2(0));
    // EXPECT_NEAR(coords.uv, float2(0), 1e-3f);

    lP = float3(0.5f, 0.5f, 0.0f); /* Max of first LOD. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES - 1));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES), 1e-3f);

    /* Test clip-map level selection. */

    camera_lP = float3(2.0f, 2.0f, 0.0f);
    /* Follows ShadowDirectional::end_sync(). */
    set_clipmap_base_offset(light, int2(round(camera_lP.xy / lod_min_tile_size)));
    EXPECT_EQ(light.sun().clipmap_base_offset_pos, int2(32));

    lP = float3(2.00001f, 2.00001f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 2), 1e-3f);

    lP = float3(1.50001f, 1.50001f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 4), 1e-3f);

    lP = float3(1.00001f, 1.00001f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 4), 1e-3f);

    lP = float3(-0.0001f, -0.0001f, 0.0f); /* Out of bounds. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.tilemap_tile, uint2(0));
    // EXPECT_NEAR(coords.uv, float2(0), 1e-3f);

    /* Test clip-map offset. */

    set_clipmap_base_offset(light, int2(31, 1));
    lP = float3(2.0001f, 0.0001f, 0.0f);

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2) + uint2(1, -1));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2) + uint2(1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2) + uint2(1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2) + uint2(1, 0));

    /* Test clip-map negative offsets. */

    set_clipmap_base_offset(light, int2(-31, -1));
    lP = float3(-2.0001f, -0.0001f, 0.0f);

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2 - 1) + uint2(-1, 1));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2 - 1) + uint2(-1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2 - 1) + uint2(-1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2 - 1) + uint2(-1, 0));
  }

  TEST(eevee_shadow, DirectionalCascadeCoordinates)
  {
    ShadowCoordinates coords;
    float3 lP, camera_lP;

    LightData light;
    light.type = LIGHT_SUN_ORTHO;
    // clipmap_lod_min = 0; /* Range [-0.5f..0.5f]. */
    // clipmap_lod_max = 2; /* 3 tile-maps. */
    set_clipmap_data(light, 0, 2, 0.0f, 0.0f);
    light.tilemap_index = 1;
    light.object_to_world.x = float4(1.0f, 0.0f, 0.0f, 0.0f);
    light.object_to_world.y = float4(0.0f, 1.0f, 0.0f, 0.0f);
    light.object_to_world.z = float4(0.0f, 0.0f, 1.0f, 0.0f);
    light.lod_bias = light.sun().clipmap_lod_min - 1;
    float lod_tile_size = exp2(float(light.sun().clipmap_lod_min)) / float(SHADOW_TILEMAP_RES);
    float lod_half_size = exp2(float(light.sun().clipmap_lod_min)) / 2.0f;
    float narrowing = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);

    camera_lP = float3(0.0f, 0.0f, 0.0f);
    int level_range_size = light.sun().clipmap_lod_max - light.sun().clipmap_lod_min + 1;
    float2 farthest_tilemap_center = float2(lod_half_size * float(level_range_size - 1), 0.0f);
    set_clipmap_base_offset(
        light, floatBitsToInt(float2(lod_half_size / float(level_range_size - 1), 0.0f)));

    /* Test UVs and tile mapping. */

    lP = float3(1e-8f, 1e-8f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 2), 1e-3f);

    lP = float3(lod_half_size * narrowing - 1e-5f, 1e-8f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES - 1, SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(float(SHADOW_TILEMAP_RES) - 0.5f, SHADOW_TILEMAP_RES / 2),
    // 1e-3f);

    lP = float3(lod_half_size + 1e-5f, 1e-5f, 0.0f);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.tilemap_tile, uint2(SHADOW_TILEMAP_RES - 1, SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES / 2), 1e-3f);

    // lP = float3(-0.5f, -0.5f, 0.0f); /* Min of first LOD. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(0));
    // EXPECT_NEAR(coords.uv, float2(0), 1e-3f);

    // lP = float3(0.5f, 0.5f, 0.0f); /* Max of first LOD. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES - 1));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES), 1e-3f);

    /* Test clip-map level selection. */

    // camera_lP = float3(2.0f, 2.0f, 0.0f);
    /* Follows ShadowDirectional::end_sync(). */
    // set_clipmap_base_offset(light,  ivec2(round(camera_lP.xy / lod_min_tile_size)));
    // EXPECT_EQ(light.sun().clipmap_base_offset_pos, ivec2(32));

    // lP = float3(2.00001f, 2.00001f, 0.0f);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 2), 1e-3f);

    // lP = float3(1.50001f, 1.50001f, 0.0f);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 1);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 4), 1e-3f);

    // lP = float3(1.00001f, 1.00001f, 0.0f);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 2);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, float2(SHADOW_TILEMAP_RES / 4), 1e-3f);

    // lP = float3(-0.0001f, -0.0001f, 0.0f); /* Out of bounds. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 2);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(0));
    // EXPECT_NEAR(coords.uv, float2(0), 1e-3f);

    /* Test clip-map offset. */

    // set_clipmap_base_offset(light,  ivec2(31, 1));
    // lP = float3(2.0001f, 0.0001f, 0.0f);

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2) + uvec2(1, -1));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2) + uvec2(1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2) + uvec2(1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2) + uvec2(1, 0));

    /* Test clip-map negative offsets. */

    // set_clipmap_base_offset(light,  ivec2(-31, -1));
    // lP = float3(-2.0001f, -0.0001f, 0.0f);

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2 - 1) + uvec2(-1, 1));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2 - 1) + uvec2(-1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2 - 1) + uvec2(-1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_tile, uvec2(SHADOW_TILEMAP_RES / 2 - 1) + uvec2(-1, 0));
  }
}
