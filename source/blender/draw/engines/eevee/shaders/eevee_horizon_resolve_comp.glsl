/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/eevee_tracing_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_horizon_resolve)

#include "draw_view_lib.glsl"
#include "eevee_closure_lib.glsl"
#include "eevee_filter_lib.glsl"
#include "eevee_gbuffer_read_lib.glsl"
#include "eevee_lightprobe_eval_lib.glsl"
#include "eevee_reverse_z_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

float3 sample_normal_get(int2 texel, out bool is_processed)
{
  float4 normal = texelFetch(screen_normal_tx, texel, 0);
  is_processed = (normal.w != 0.0f);
  return drw_normal_view_to_world(normal.xyz * 2.0f - 1.0f);
}

float sample_weight_get(float3 center_N, float3 center_P, int2 center_texel, int2 sample_offset)
{
  int2 sample_texel = center_texel + sample_offset;
  int2 sample_texel_fullres = sample_texel * uniform_buf.raytrace.horizon_resolution_scale +
                              uniform_buf.raytrace.horizon_resolution_bias;
  float2 sample_uv = (float2(sample_texel_fullres) + 0.5f) *
                     uniform_buf.raytrace.full_resolution_inv;

  float sample_depth = reverse_z::read(texelFetch(depth_tx, sample_texel_fullres, 0).r);

  bool is_valid;
  float3 sample_N = sample_normal_get(sample_texel, is_valid);
  float3 sample_P = drw_point_screen_to_world(float3(sample_uv, sample_depth));

  if (!is_valid) {
    return 0.0f;
  }

  /* TODO(fclem): Scene parameter. 10000.0f is dependent on scene scale. */
  float depth_weight = filter_planar_weight(center_N, center_P, sample_P, 10000.0f);
  float normal_weight = filter_angle_weight(center_N, sample_N);
  /* Some pixels might have no correct weight (depth & normal weights being very small).
   * To avoid them have invalid energy (because of float precision),
   * we weight all valid samples by a very small amount. */
  float epsilon_weight = 1e-4f;

  return max(epsilon_weight, depth_weight * normal_weight);
}

SphericalHarmonicL1 load_spherical_harmonic(int2 texel, bool valid)
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
  constexpr uint tile_size = RAYTRACE_GROUP_SIZE;
  uint2 tile_coord = unpackUvec2x16(tiles_coord_buf[gl_WorkGroupID.x]);
  int2 texel_fullres = int2(gl_LocalInvocationID.xy + tile_coord * tile_size);

  int2 texel = max(int2(0), texel_fullres - uniform_buf.raytrace.horizon_resolution_bias) /
               uniform_buf.raytrace.horizon_resolution_scale;

  int2 extent = textureSize(gbuf_header_tx, 0).xy;
  if (any(greaterThanEqual(texel_fullres, extent))) {
    return;
  }

  const gbuffer::Layers gbuf = gbuffer::read_layers(texel_fullres);

  if (gbuf.header.is_empty()) {
    return;
  }

  float2 center_uv = (float2(texel_fullres) + 0.5f) * uniform_buf.raytrace.full_resolution_inv;
  float center_depth = reverse_z::read(texelFetch(depth_tx, texel_fullres, 0).r);
  float3 center_P = drw_point_screen_to_world(float3(center_uv, center_depth));
  float3 center_N = gbuf.surface_N();

  SphericalHarmonicL1 accum_sh;
  if (uniform_buf.raytrace.horizon_resolution_scale == 1) {
    accum_sh = load_spherical_harmonic(texel, true);
  }
  else {
    float2 interp = float2(texel_fullres - texel * uniform_buf.raytrace.horizon_resolution_scale -
                           uniform_buf.raytrace.horizon_resolution_bias) /
                    float2(uniform_buf.raytrace.horizon_resolution_scale);
    float4 interp4 = float4(interp, 1.0f - interp);
    float4 bilinear_weight = interp4.zxzx * interp4.wwyy;

    float4 bilateral_weights;
    bilateral_weights.x = sample_weight_get(center_N, center_P, texel, int2(0, 0));
    bilateral_weights.y = sample_weight_get(center_N, center_P, texel, int2(1, 0));
    bilateral_weights.z = sample_weight_get(center_N, center_P, texel, int2(0, 1));
    bilateral_weights.w = sample_weight_get(center_N, center_P, texel, int2(1, 1));

    float4 weights = bilateral_weights * bilinear_weight;

    SphericalHarmonicL1 sh_00 = load_spherical_harmonic(texel + int2(0, 0), weights.x > 0.0f);
    SphericalHarmonicL1 sh_10 = load_spherical_harmonic(texel + int2(1, 0), weights.y > 0.0f);
    SphericalHarmonicL1 sh_01 = load_spherical_harmonic(texel + int2(0, 1), weights.z > 0.0f);
    SphericalHarmonicL1 sh_11 = load_spherical_harmonic(texel + int2(1, 1), weights.w > 0.0f);

    /* Avoid another division at the end. Normalize the weights upfront. */
    weights *= safe_rcp(reduce_add(weights));

    accum_sh = spherical_harmonics_mul(sh_00, weights.x);
    accum_sh = spherical_harmonics_madd(sh_10, weights.y, accum_sh);
    accum_sh = spherical_harmonics_madd(sh_01, weights.z, accum_sh);
    accum_sh = spherical_harmonics_madd(sh_11, weights.w, accum_sh);
  }

  float3 P = center_P;
  float3 Ng = center_N;
  float3 V = drw_world_incident_vector(P);

  LightProbeSample samp = lightprobe_load(P, Ng, V);

  float clamp_indirect = uniform_buf.clamp.surface_indirect;
  samp.volume_irradiance = spherical_harmonics_clamp(samp.volume_irradiance, clamp_indirect);

  const uchar closure_count = gbuf.header.closure_len();
  const uint3 bin_indices = gbuf.header.bin_index_per_layer();
  const float thickness = gbuffer::read_thickness(gbuf.header, texel_fullres);

  for (uchar i = 0; i < GBUFFER_LAYER_MAX && i < closure_count; i++) {
    ClosureUndetermined cl = gbuf.layer_get(i);

    float roughness = closure_apparent_roughness_get(cl);

    float mix_fac = saturate(roughness * uniform_buf.raytrace.roughness_mask_scale -
                             uniform_buf.raytrace.roughness_mask_bias);
    bool use_raytrace = mix_fac < 1.0f;
    bool use_horizon = mix_fac > 0.0f;

    if (!use_horizon) {
      continue;
    }

    LightProbeRay ray = bxdf_lightprobe_ray(cl, P, V, thickness);

    float3 L = ray.dominant_direction;
    float3 vL = drw_normal_world_to_view(L);

    /* Evaluate lighting from horizon scan. */
    float3 radiance = spherical_harmonics_evaluate_lambert(vL, accum_sh);

    /* Evaluate visibility from horizon scan. */
    SphericalHarmonicL1 sh_visibility = spherical_harmonics_swizzle_wwww(accum_sh);
    float occlusion = spherical_harmonics_evaluate_lambert(vL, sh_visibility).x;
    /* FIXME(fclem): Tried to match the old occlusion look. I don't know why it's needed. */
    occlusion *= 0.5f;
    /* TODO(fclem): Ideally, we should just combine both local and distant irradiance and evaluate
     * once. Unfortunately, I couldn't find a way to do the same (1.0 - occlusion) with the
     * spherical harmonic coefficients. */
    float visibility = saturate(1.0f - occlusion);

    /* Apply missing distant lighting. */
    float3 radiance_probe = spherical_harmonics_evaluate_lambert(L, samp.volume_irradiance);
    radiance += visibility * radiance_probe;

    uchar layer_index = bin_indices[i];

    float4 radiance_horizon = float4(radiance, 0.0f);
    float4 radiance_raytrace = float4(0.0f);
    if (use_raytrace) {
      /* TODO(fclem): Layered texture. */
      if (layer_index == 0u) {
        radiance_raytrace = imageLoad(closure0_img, texel_fullres);
      }
      else if (layer_index == 1u) {
        radiance_raytrace = imageLoad(closure1_img, texel_fullres);
      }
      else if (layer_index == 2u) {
        radiance_raytrace = imageLoad(closure2_img, texel_fullres);
      }
    }
    float4 radiance_mixed = mix(radiance_raytrace, radiance_horizon, mix_fac);

    /* TODO(fclem): Layered texture. */
    if (layer_index == 0u) {
      imageStore(closure0_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 1u) {
      imageStore(closure1_img, texel_fullres, radiance_mixed);
    }
    else if (layer_index == 2u) {
      imageStore(closure2_img, texel_fullres, radiance_mixed);
    }
  }
}
