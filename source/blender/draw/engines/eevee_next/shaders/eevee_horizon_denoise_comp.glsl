/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_filter_lib.glsl)

vec3 sample_normal_get(ivec2 texel, out bool is_processed)
{
  vec4 normal = texelFetch(screen_normal_tx, texel, 0);
  is_processed = (normal.w != 0.0);
  return normal.xyz * 2.0 - 1.0;
}

float sample_weight_get(
    vec3 center_N, vec3 center_P, ivec2 sample_texel, vec2 sample_uv, ivec2 sample_offset)
{
  ivec2 sample_texel_fullres = sample_texel * uniform_buf.raytrace.horizon_resolution_scale +
                               uniform_buf.raytrace.horizon_resolution_bias;
  float sample_depth = texelFetch(hiz_tx, sample_texel_fullres, 0).r;

  bool is_valid;
  vec3 sample_N = sample_normal_get(sample_texel, is_valid);
  vec3 sample_P = drw_point_screen_to_world(vec3(sample_uv, sample_depth));

  if (!is_valid) {
    return 0.0;
  }

  float gauss = filter_gaussian_factor(1.5, 1.5);

  /* TODO(fclem): Scene parameter. 100.0 is dependent on scene scale. */
  float depth_weight = filter_planar_weight(center_N, center_P, sample_P, 100.0);
  float spatial_weight = filter_gaussian_weight(gauss, length_squared(vec2(sample_offset)));
  float normal_weight = filter_angle_weight(center_N, sample_N);

  return depth_weight * spatial_weight * normal_weight;
}

SphericalHarmonicL1 load_spherical_harmonic(ivec2 texel)
{
  SphericalHarmonicL1 sh;
  sh.L0.M0 = texelFetch(in_sh_0_tx, texel, 0);
  sh.L1.Mn1 = texelFetch(in_sh_1_tx, texel, 0);
  sh.L1.M0 = texelFetch(in_sh_2_tx, texel, 0);
  sh.L1.Mp1 = texelFetch(in_sh_3_tx, texel, 0);
  sh = spherical_harmonics_decompress(sh);
  return sh;
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  vec2 texel_size = 1.0 / vec2(textureSize(in_sh_0_tx, 0).xy);
  ivec2 texel_fullres = texel * uniform_buf.raytrace.horizon_resolution_scale +
                        uniform_buf.raytrace.horizon_resolution_bias;

  bool is_valid;
  float center_depth = texelFetch(hiz_tx, texel_fullres, 0).r;
  vec2 center_uv = vec2(texel) * texel_size;
  vec3 center_P = drw_point_screen_to_world(vec3(center_uv, center_depth));
  vec3 center_N = sample_normal_get(texel, is_valid);

  if (!is_valid) {
#if 0 /* This is not needed as the next stage doesn't do bilinear filtering. */
    imageStore(out_sh_0_img, texel, vec4(0.0));
    imageStore(out_sh_1_img, texel, vec4(0.0));
    imageStore(out_sh_2_img, texel, vec4(0.0));
    imageStore(out_sh_3_img, texel, vec4(0.0));
#endif
    return;
  }

  SphericalHarmonicL1 accum_sh = spherical_harmonics_L1_new();
  float accum_weight = 0.0;
  /* 3x3 filter. */
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      ivec2 sample_offset = ivec2(x, y);
      ivec2 sample_texel = texel + sample_offset;
      vec2 sample_uv = (vec2(sample_texel) + 0.5) * texel_size;
      float sample_weight = sample_weight_get(
          center_N, center_P, sample_texel, sample_uv, sample_offset);
      /* We need to avoid sampling if there no weight as the texture values could be undefined
       * (is_valid is false). */
      if (sample_weight > 0.0) {
        SphericalHarmonicL1 sample_sh = load_spherical_harmonic(sample_texel);
        accum_sh = spherical_harmonics_madd(sample_sh, sample_weight, accum_sh);
        accum_weight += sample_weight;
      }
    }
  }
  accum_sh = spherical_harmonics_mul(accum_sh, safe_rcp(accum_weight));

  accum_sh = spherical_harmonics_compress(accum_sh);

  imageStore(out_sh_0_img, texel, accum_sh.L0.M0);
  imageStore(out_sh_1_img, texel, accum_sh.L1.Mn1);
  imageStore(out_sh_2_img, texel, accum_sh.L1.M0);
  imageStore(out_sh_3_img, texel, accum_sh.L1.Mp1);
}
