/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_lightprobe_eval_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_closure_lib.glsl)

float bilateral_depth_weight(vec3 center_N, vec3 center_P, vec3 sample_P)
{
  vec4 center_plane_eq = vec4(center_N, -dot(center_N, center_P));
  /* Only compare distance to the center plane formed by the normal. */
  float depth_delta = dot(center_plane_eq, vec4(sample_P, 1.0));
  /* TODO(fclem): Scene parameter. This is dependent on scene scale. */
  const float scale = 10000.0;
  float weight = exp2(-scale * square(depth_delta));
  return weight;
}

float bilateral_spatial_weight(float sigma, vec2 offset_from_center)
{
  /* From https://github.com/tranvansang/bilateral-filter/blob/master/fshader.frag */
  float fac = -1.0 / square(sigma);
  /* Take two standard deviation. */
  fac *= 2.0;
  float weight = exp2(fac * length_squared(offset_from_center));
  return weight;
}

float bilateral_normal_weight(vec3 center_N, vec3 sample_N)
{
  float facing_ratio = dot(center_N, sample_N);
  float weight = saturate(pow8f(facing_ratio));
  return weight;
}

/* In order to remove some more fireflies, "tone-map" the color samples during the accumulation. */
vec3 to_accumulation_space(vec3 color)
{
  /* This 4 factor is to avoid killing too much energy. */
  /* TODO(fclem): Parameter? */
  color /= 4.0;
  color = color / (1.0 + reduce_add(color));
  return color;
}
vec3 from_accumulation_space(vec3 color)
{
  color = color / (1.0 - reduce_add(color));
  color *= 4.0;
  return color;
}

vec3 load_normal(ivec2 texel)
{
  return gbuffer_read(gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel).surface_N;
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);

  ivec2 texel_fullres = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);
  ivec2 texel = (texel_fullres) / uniform_buf.raytrace.resolution_scale;

  ivec2 extent = textureSize(gbuf_header_tx, 0).xy;
  if (any(greaterThanEqual(texel_fullres, extent))) {
    return;
  }

  vec2 center_uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;
  float center_depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 center_P = drw_point_screen_to_world(vec3(center_uv, center_depth));

  if (center_depth == 1.0) {
    /* Do not trace for background */
    return;
  }

  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  bool has_valid_closure = closure_index < gbuf.closure_count;
  if (!has_valid_closure) {
    return;
  }

  ClosureUndetermined closure_center = gbuffer_closure_get(gbuf, closure_index);

  vec3 center_N = closure_center.N;
  float roughness = closure_apparent_roughness_get(closure_center);

  float mix_fac = saturate(roughness * uniform_buf.raytrace.roughness_mask_scale -
                           uniform_buf.raytrace.roughness_mask_bias);
  bool use_raytrace = mix_fac < 1.0;
  bool use_horizon = mix_fac > 0.0;

  if (use_horizon == false) {
    return;
  }

  vec3 accum_radiance = vec3(0.0);
  float accum_occlusion = 0.0;
  float accum_weight = 0.0;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      ivec2 offset = ivec2(x, y);
      ivec2 sample_texel = texel + ivec2(x, y);
      ivec2 sample_texel_fullres = sample_texel * uniform_buf.raytrace.resolution_scale +
                                   uniform_buf.raytrace.resolution_bias;
      ivec3 sample_tile = ivec3(sample_texel_fullres / RAYTRACE_GROUP_SIZE, closure_index);
      /* Make sure the sample has been processed and do not contain garbage data. */
      if (imageLoad(tile_mask_img, sample_tile).r == 0u) {
        continue;
      }

      float sample_depth = texelFetch(depth_tx, sample_texel_fullres, 0).r;
      vec2 sample_uv = (vec2(sample_texel_fullres) + 0.5) *
                       uniform_buf.raytrace.full_resolution_inv;
      vec3 sample_P = drw_point_screen_to_world(vec3(sample_uv, sample_depth));

      /* Background case. */
      if (sample_depth == 0.0) {
        continue;
      }

      vec3 sample_N = load_normal(sample_texel_fullres);

      float depth_weight = bilateral_depth_weight(center_N, center_P, sample_P);
      float spatial_weight = bilateral_spatial_weight(1.5, vec2(offset));
      float normal_weight = bilateral_normal_weight(center_N, sample_N);

      float weight = depth_weight * spatial_weight * normal_weight;

      vec3 radiance = imageLoad(horizon_radiance_img, sample_texel).rgb;
      /* Do not gather unprocessed pixels. */
      if (all(equal(radiance, FLT_11_11_10_MAX))) {
        continue;
      }
      float occlusion = imageLoad(horizon_occlusion_img, sample_texel).r;
      accum_radiance += to_accumulation_space(radiance) * weight;
      accum_occlusion += occlusion * weight;
      accum_weight += weight;
    }
  }
  float occlusion = accum_occlusion * safe_rcp(accum_weight);
  vec3 radiance = from_accumulation_space(accum_radiance * safe_rcp(accum_weight));

  vec3 P = center_P;
  vec3 N = center_N;
  vec3 Ng = center_N;
  vec3 V = drw_world_incident_vector(P);
  /* Fallback to nearest light-probe. */
  LightProbeSample samp = lightprobe_load(P, Ng, V);
  vec3 radiance_probe = spherical_harmonics_evaluate_lambert(N, samp.volume_irradiance);
  /* Apply missing distant lighting. */
  radiance += occlusion * radiance_probe;

  vec4 radiance_horizon = vec4(radiance, 0.0);
  vec4 radiance_raytrace = use_raytrace ? imageLoad(radiance_img, texel_fullres) : vec4(0.0);

  vec4 radiance_mixed = mix(radiance_raytrace, radiance_horizon, mix_fac);

  imageStore(radiance_img, texel_fullres, radiance_mixed);
}
