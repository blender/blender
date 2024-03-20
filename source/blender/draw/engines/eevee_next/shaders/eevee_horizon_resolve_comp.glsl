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
#pragma BLENDER_REQUIRE(eevee_filter_lib.glsl)

vec3 sample_normal_get(ivec2 texel, out bool is_processed)
{
  vec4 normal = texelFetch(screen_normal_tx, texel, 0);
  is_processed = (normal.w != 0.0);
  return drw_normal_view_to_world(normal.xyz * 2.0 - 1.0);
}

float sample_weight_get(vec3 center_N, vec3 center_P, ivec2 center_texel, ivec2 sample_offset)
{
  ivec2 sample_texel = center_texel + sample_offset;
  ivec2 sample_texel_fullres = sample_texel * uniform_buf.raytrace.horizon_resolution_scale +
                               uniform_buf.raytrace.horizon_resolution_bias;
  vec2 sample_uv = (vec2(sample_texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;

  float sample_depth = texelFetch(depth_tx, sample_texel_fullres, 0).r;

  bool is_valid;
  vec3 sample_N = sample_normal_get(sample_texel, is_valid);
  vec3 sample_P = drw_point_screen_to_world(vec3(sample_uv, sample_depth));

  if (!is_valid) {
    return 0.0;
  }

  /* TODO(fclem): Scene parameter. 10000.0 is dependent on scene scale. */
  float depth_weight = filter_planar_weight(center_N, center_P, sample_P, 10000.0);
  float normal_weight = filter_angle_weight(center_N, sample_N);

  return depth_weight * normal_weight;
}

SphericalHarmonicL1 load_spherical_harmonic(ivec2 texel, bool valid)
{
  if (!valid) {
    /* We need to avoid sampling if there no weight as the texture values could be undefined
     * (is_valid is false). */
    return spherical_harmonics_L1_new();
  }
  SphericalHarmonicL1 sh;
  sh.L0.M0 = texelFetch(horizon_radiance_0_tx, texel, 0);
  sh.L1.Mn1 = texelFetch(horizon_radiance_1_tx, texel, 0);
  sh.L1.M0 = texelFetch(horizon_radiance_2_tx, texel, 0);
  sh.L1.Mp1 = texelFetch(horizon_radiance_3_tx, texel, 0);
  return spherical_harmonics_decompress(sh);
}

void main()
{
  const uint tile_size = RAYTRACE_GROUP_SIZE;
  uvec2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  ivec2 texel_fullres = ivec2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  ivec2 texel = max(ivec2(0), texel_fullres - uniform_buf.raytrace.horizon_resolution_bias) /
                uniform_buf.raytrace.horizon_resolution_scale;

  ivec2 extent = textureSize(gbuf_header_tx, 0).xy;
  if (any(greaterThanEqual(texel_fullres, extent))) {
    return;
  }

  GBufferReader gbuf = gbuffer_read(
      gbuf_header_tx, gbuf_closure_tx, gbuf_normal_tx, texel_fullres);

  if (gbuf.header == 0u) {
    return;
  }

  vec2 center_uv = (vec2(texel_fullres) + 0.5) * uniform_buf.raytrace.full_resolution_inv;
  float center_depth = texelFetch(depth_tx, texel_fullres, 0).r;
  vec3 center_P = drw_point_screen_to_world(vec3(center_uv, center_depth));
  vec3 center_N = gbuf.surface_N;

  SphericalHarmonicL1 accum_sh;
  if (uniform_buf.raytrace.horizon_resolution_scale == 1) {
    accum_sh = load_spherical_harmonic(texel, true);
  }
  else {
    vec2 interp = vec2(texel_fullres - texel * uniform_buf.raytrace.horizon_resolution_scale -
                       uniform_buf.raytrace.horizon_resolution_bias) /
                  vec2(uniform_buf.raytrace.horizon_resolution_scale);
    vec4 interp4 = vec4(interp, 1.0 - interp);
    vec4 bilinear_weight = interp4.zxzx * interp4.wwyy;

    vec4 bilateral_weights;
    bilateral_weights.x = sample_weight_get(center_N, center_P, texel, ivec2(0, 0));
    bilateral_weights.y = sample_weight_get(center_N, center_P, texel, ivec2(1, 0));
    bilateral_weights.z = sample_weight_get(center_N, center_P, texel, ivec2(0, 1));
    bilateral_weights.w = sample_weight_get(center_N, center_P, texel, ivec2(1, 1));

    vec4 weights = bilateral_weights * bilinear_weight;

    SphericalHarmonicL1 sh_00 = load_spherical_harmonic(texel + ivec2(0, 0), weights.x > 0.0);
    SphericalHarmonicL1 sh_10 = load_spherical_harmonic(texel + ivec2(1, 0), weights.y > 0.0);
    SphericalHarmonicL1 sh_01 = load_spherical_harmonic(texel + ivec2(0, 1), weights.z > 0.0);
    SphericalHarmonicL1 sh_11 = load_spherical_harmonic(texel + ivec2(1, 1), weights.w > 0.0);

    /* Avoid another division at the end. Normalize the weights upfront. */
    weights *= safe_rcp(reduce_add(weights));

    accum_sh = spherical_harmonics_mul(sh_00, weights.x);
    accum_sh = spherical_harmonics_madd(sh_10, weights.y, accum_sh);
    accum_sh = spherical_harmonics_madd(sh_01, weights.z, accum_sh);
    accum_sh = spherical_harmonics_madd(sh_11, weights.w, accum_sh);
  }

  vec3 P = center_P;
  vec3 Ng = center_N;
  vec3 V = drw_world_incident_vector(P);

  LightProbeSample samp = lightprobe_load(P, Ng, V);

  for (int i = 0; i < GBUFFER_LAYER_MAX && i < gbuf.closure_count; i++) {
    ClosureUndetermined cl = gbuffer_closure_get(gbuf, i);

    float roughness = closure_apparent_roughness_get(cl);

    float mix_fac = saturate(roughness * uniform_buf.raytrace.roughness_mask_scale -
                             uniform_buf.raytrace.roughness_mask_bias);
    bool use_raytrace = mix_fac < 1.0;
    bool use_horizon = mix_fac > 0.0;

    if (!use_horizon) {
      continue;
    }

    vec3 N = cl.N;

    vec3 L;
    switch (cl.type) {
      case CLOSURE_BSDF_MICROFACET_GGX_REFLECTION_ID:
        L = lightprobe_reflection_dominant_dir(cl.N, V, roughness);
        break;
      case CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID:
        L = lightprobe_refraction_dominant_dir(cl.N, V, to_closure_refraction(cl).ior, roughness);
        break;
      case CLOSURE_BSDF_TRANSLUCENT_ID:
        L = -N;
        break;
      default:
        L = N;
        break;
    }
    vec3 vL = drw_normal_world_to_view(L);

    /* Evaluate lighting from horizon scan. */
    /* TODO(fclem): Evaluate depending on BSDF. */
    vec3 radiance = spherical_harmonics_evaluate_lambert(vL, accum_sh);

    /* Evaluate visibility from horizon scan. */
    SphericalHarmonicL1 sh_visibility = spherical_harmonics_swizzle_wwww(accum_sh);
    float occlusion = spherical_harmonics_evaluate_lambert(vL, sh_visibility).x;
    /* FIXME(fclem): Tried to match the old occlusion look. I don't know why it's needed. */
    occlusion *= 0.5;
    /* TODO(fclem): Ideally, we should just combine both local and distant irradiance and evaluate
     * once. Unfortunately, I couldn't find a way to do the same (1.0 - occlusion) with the
     * spherical harmonic coefficients. */
    float visibility = saturate(1.0 - occlusion);

    /* Apply missing distant lighting. */
    vec3 radiance_probe = spherical_harmonics_evaluate_lambert(L, samp.volume_irradiance);
    radiance += visibility * radiance_probe;

    int layer_index = gbuffer_closure_get_bin_index(gbuf, i);

    vec4 radiance_horizon = vec4(radiance, 0.0);
    vec4 radiance_raytrace = vec4(0.0);
    if (use_raytrace) {
      /* TODO(fclem): Layered texture. */
      if (layer_index == 0) {
        radiance_raytrace = imageLoad(closure0_img, texel_fullres);
      }
      else if (layer_index == 1) {
        radiance_raytrace = imageLoad(closure1_img, texel_fullres);
      }
      else if (layer_index == 2) {
        radiance_raytrace = imageLoad(closure2_img, texel_fullres);
      }
    }
    vec4 radiance_mixed = mix(radiance_raytrace, radiance_horizon, mix_fac);

    /* TODO(fclem): Layered texture. */
    if (layer_index == 0) {
      imageStore(closure0_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 1) {
      imageStore(closure1_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 2) {
      imageStore(closure2_img, texel_fullres, radiance_mixed);
    }
  }
}
