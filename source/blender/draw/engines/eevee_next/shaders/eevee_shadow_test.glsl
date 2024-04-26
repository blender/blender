/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Directive for resetting the line numbering so the failing tests lines can be printed.
 * This conflict with the shader compiler error logging scheme.
 * Comment out for correct compilation error line. */
#line 9

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_test_lib.glsl)

#define TEST(a, b) if (true)

void set_clipmap_data(inout LightData light,
                      int clipmap_lod_min,
                      int clipmap_lod_max,
                      float clipmap_origin_x,
                      float clipmap_origin_y)
{
  /* WATCH: Can get out of sync with light_sun_data_get(). */
  light.do_not_access_directly._pad3 = vec2(clipmap_origin_x, clipmap_origin_y);
  light.do_not_access_directly._pad4 = intBitsToFloat(clipmap_lod_min);
  light.do_not_access_directly._pad5 = intBitsToFloat(clipmap_lod_max);
}

void set_clipmap_base_offset(inout LightData light, ivec2 clipmap_base_offset)
{
  /* WATCH: Can get out of sync with light_sun_data_get(). */
  light.do_not_access_directly.shadow_scale = intBitsToFloat(0);
  light.do_not_access_directly.shadow_projection_shift = intBitsToFloat(0);
  light.do_not_access_directly._pad0_reserved = intBitsToFloat(clipmap_base_offset.x);
  light.do_not_access_directly._pad1_reserved = intBitsToFloat(clipmap_base_offset.y);
}

void main()
{
  TEST(eevee_shadow, DirectionalMemberSet)
  {
    LightData light;
    /* Test the setter functions define in this file to make sure
     * that they are not out of sync with `light_sun_data_get`. */
    set_clipmap_data(light, 1, 2, 3.0, 4.0);
    set_clipmap_base_offset(light, ivec2(5, 6));

    EXPECT_EQ(light_sun_data_get(light).clipmap_lod_min, 1);
    EXPECT_EQ(light_sun_data_get(light).clipmap_lod_max, 2);
    EXPECT_EQ(light_sun_data_get(light).clipmap_origin, vec2(3.0, 4.0));
    EXPECT_EQ(light_sun_data_get(light).clipmap_base_offset_pos, ivec2(5, 6));
  }

  TEST(eevee_shadow, DirectionalClipmapLevel)
  {
    LightData light;
    light.type = LIGHT_SUN;
    set_clipmap_data(light, -5, 8, 0.0, 0.0);
    light.lod_bias = 0.0;
    float fac = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 0.0)),
              light_sun_data_get(light).clipmap_lod_min);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 0.49)), 1);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 0.5)), 1);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 0.51)), 1);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 0.99)), 2);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 1.0)), 2);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 1.01)), 2);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 12.5)), 6);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 12.51)), 6);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 15.9999)), 6);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 16.0)), 6);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 16.00001)), 6);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 5000.0)),
              light_sun_data_get(light).clipmap_lod_max);
    /* Produces NaN / Inf, Undefined behavior. */
    // EXPECT_EQ(shadow_directional_level(light, vec3(FLT_MAX)),
    // light_sun_data_get(light).clipmap_lod_max);
  }

  TEST(eevee_shadow, DirectionalCascadeLevel)
  {
    LightData light;
    light.type = LIGHT_SUN_ORTHO;
    set_clipmap_data(light, 2, 8, 0.0, 0.0);
    float half_size = exp2(float(light_sun_data_get(light).clipmap_lod_min - 1));
    light.lod_bias = light_sun_data_get(light).clipmap_lod_min - 1;
    float fac = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * half_size * 0.0, 0.0, 0.0)), 2);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * half_size * 0.5, 0.0, 0.0)), 2);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * half_size * 1.0, 0.0, 0.0)), 3);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * half_size * 1.5, 0.0, 0.0)), 3);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * half_size * 2.0, 0.0, 0.0)), 4);
    EXPECT_EQ(shadow_directional_level(light, vec3(fac * 5000.0)),
              light_sun_data_get(light).clipmap_lod_max);
    /* Produces NaN / Inf, Undefined behavior. */
    // EXPECT_EQ(shadow_directional_level(light, vec3(FLT_MAX)),
    // light_sun_data_get(light).clipmap_lod_max);
  }

  TEST(eevee_shadow, DirectionalClipmapCoordinates)
  {
    ShadowCoordinates coords;
    vec3 lP, camera_lP;

    LightData light;
    light.type = LIGHT_SUN;
    // clipmap_lod_min = 0; /* Range [-0.5..0.5]. */
    // clipmap_lod_max = 2; /* Range [-2..2]. */
    set_clipmap_data(light, 0, 2, 0.0, 0.0);

    light.tilemap_index = light_sun_data_get(light).clipmap_lod_min;
    light.object_to_world.x = float4(1.0, 0.0, 0.0, 0.0);
    light.object_to_world.y = float4(0.0, 1.0, 0.0, 0.0);
    light.object_to_world.z = float4(0.0, 0.0, 1.0, 0.0);
    light.lod_bias = 0;

    float lod_min_tile_size = exp2(float(light_sun_data_get(light).clipmap_lod_min)) /
                              float(SHADOW_TILEMAP_RES);
    float lod_max_half_size = exp2(float(light_sun_data_get(light).clipmap_lod_max)) / 2.0;

    camera_lP = vec3(0.0, 0.0, 0.0);
    /* Follows ShadowDirectional::end_sync(). */
    set_clipmap_base_offset(light, ivec2(round(camera_lP.xy / lod_min_tile_size)));
    EXPECT_EQ(light_sun_data_get(light).clipmap_base_offset_pos, ivec2(0));

    /* Test UVs and tile mapping. */

    lP = vec3(1e-5, 1e-5, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 2), 1e-3);

    lP = vec3(-1e-5, -1e-5, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tile_coord, ivec2((SHADOW_TILEMAP_RES / 2) - 1));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 2), 1e-3);

    lP = vec3(-0.5, -0.5, 0.0); /* Min of first LOD. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(0));
    EXPECT_NEAR(coords.uv, vec2(0), 1e-3);

    lP = vec3(0.5, 0.5, 0.0); /* Max of first LOD. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES - 1));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES), 1e-3);

    /* Test clip-map level selection. */

    camera_lP = vec3(2.0, 2.0, 0.0);
    /* Follows ShadowDirectional::end_sync(). */
    set_clipmap_base_offset(light, ivec2(round(camera_lP.xy / lod_min_tile_size)));
    EXPECT_EQ(light_sun_data_get(light).clipmap_base_offset_pos, ivec2(32));

    lP = vec3(2.00001, 2.00001, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 2), 1e-3);

    lP = vec3(1.50001, 1.50001, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 4));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 4), 1e-3);

    lP = vec3(1.00001, 1.00001, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 4));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 4), 1e-3);

    lP = vec3(-0.0001, -0.0001, 0.0); /* Out of bounds. */
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.tile_coord, ivec2(0));
    EXPECT_NEAR(coords.uv, vec2(0), 1e-3);

    /* Test clip-map offset. */

    set_clipmap_base_offset(light, ivec2(31, 1));
    lP = vec3(2.0001, 0.0001, 0.0);

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, -1));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    /* Test clip-map negative offsets. */

    set_clipmap_base_offset(light, ivec2(-31, -1));
    lP = vec3(-2.0001, -0.0001, 0.0);

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 1));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));

    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));
  }

  TEST(eevee_shadow, DirectionalCascadeCoordinates)
  {
    ShadowCoordinates coords;
    vec3 lP, camera_lP;

    LightData light;
    light.type = LIGHT_SUN_ORTHO;
    // clipmap_lod_min = 0; /* Range [-0.5..0.5]. */
    // clipmap_lod_max = 2; /* 3 tile-maps. */
    set_clipmap_data(light, 0, 2, 0.0, 0.0);
    light.tilemap_index = 1;
    light.object_to_world.x = float4(1.0, 0.0, 0.0, 0.0);
    light.object_to_world.y = float4(0.0, 1.0, 0.0, 0.0);
    light.object_to_world.z = float4(0.0, 0.0, 1.0, 0.0);
    light.lod_bias = light_sun_data_get(light).clipmap_lod_min - 1;
    float lod_tile_size = exp2(float(light_sun_data_get(light).clipmap_lod_min)) /
                          float(SHADOW_TILEMAP_RES);
    float lod_half_size = exp2(float(light_sun_data_get(light).clipmap_lod_min)) / 2.0;
    float narrowing = float(SHADOW_TILEMAP_RES - 1) / float(SHADOW_TILEMAP_RES);

    camera_lP = vec3(0.0, 0.0, 0.0);
    int level_range_size = light_sun_data_get(light).clipmap_lod_max -
                           light_sun_data_get(light).clipmap_lod_min + 1;
    vec2 farthest_tilemap_center = vec2(lod_half_size * float(level_range_size - 1), 0.0);
    set_clipmap_base_offset(
        light, floatBitsToInt(vec2(lod_half_size / float(level_range_size - 1), 0.0)));

    /* Test UVs and tile mapping. */

    lP = vec3(1e-8, 1e-8, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.lod_relative, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 2), 1e-3);

    lP = vec3(lod_half_size * narrowing - 1e-5, 1e-8, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 1);
    EXPECT_EQ(coords.lod_relative, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES - 1, SHADOW_TILEMAP_RES / 2));
    EXPECT_NEAR(coords.uv, vec2(float(SHADOW_TILEMAP_RES) - 0.5, SHADOW_TILEMAP_RES / 2), 1e-3);

    lP = vec3(lod_half_size + 1e-5, 1e-5, 0.0);
    coords = shadow_directional_coordinates(light, lP);
    EXPECT_EQ(coords.tilemap_index, 2);
    EXPECT_EQ(coords.lod_relative, 0);
    EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES - 1, SHADOW_TILEMAP_RES / 2));
    EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES, SHADOW_TILEMAP_RES / 2), 1e-3);

    // lP = vec3(-0.5, -0.5, 0.0); /* Min of first LOD. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tile_coord, ivec2(0));
    // EXPECT_NEAR(coords.uv, vec2(0), 1e-3);

    // lP = vec3(0.5, 0.5, 0.0); /* Max of first LOD. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES - 1));
    // EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES), 1e-3);

    /* Test clip-map level selection. */

    // camera_lP = vec3(2.0, 2.0, 0.0);
    /* Follows ShadowDirectional::end_sync(). */
    // set_clipmap_base_offset(light,  ivec2(round(camera_lP.xy / lod_min_tile_size)));
    // EXPECT_EQ(light_sun_data_get(light).clipmap_base_offset_pos, ivec2(32));

    // lP = vec3(2.00001, 2.00001, 0.0);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 0);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2));
    // EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 2), 1e-3);

    // lP = vec3(1.50001, 1.50001, 0.0);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 1);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 4), 1e-3);

    // lP = vec3(1.00001, 1.00001, 0.0);
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 2);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 4));
    // EXPECT_NEAR(coords.uv, vec2(SHADOW_TILEMAP_RES / 4), 1e-3);

    // lP = vec3(-0.0001, -0.0001, 0.0); /* Out of bounds. */
    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tilemap_index, 2);
    // EXPECT_EQ(coords.tile_coord, ivec2(0));
    // EXPECT_NEAR(coords.uv, vec2(0), 1e-3);

    /* Test clip-map offset. */

    // set_clipmap_base_offset(light,  ivec2(31, 1));
    // lP = vec3(2.0001, 0.0001, 0.0);

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, -1));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2) + ivec2(1, 0));

    /* Test clip-map negative offsets. */

    // set_clipmap_base_offset(light,  ivec2(-31, -1));
    // lP = vec3(-2.0001, -0.0001, 0.0);

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 1));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));

    // coords = shadow_directional_coordinates(light, lP);
    // EXPECT_EQ(coords.tile_coord, ivec2(SHADOW_TILEMAP_RES / 2 - 1) + ivec2(-1, 0));
  }
}
