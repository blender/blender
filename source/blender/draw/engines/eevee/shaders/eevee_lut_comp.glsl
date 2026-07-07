/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Used to generate Look Up Tables. Not used in default configuration as the tables are stored in
 * the blender executable. This is only used for reference or to update them.
 */

#include "infos/eevee_lut_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_lut)

#include "eevee_bxdf_sampling_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"

/* Generate BRDF LUT following "Real shading in unreal engine 4" by Brian Karis
 * https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
 * Parametrizing with `x = roughness` and `y = sqrt(1.0f - cos(theta))`.
 * The result is interpreted as: `integral = F0 * scale + F90 * bias - F82_tint * metal_bias`.
 * with `F82_tint = mix(F0, float3(1.0f), pow5f(6.0f / 7.0f)) * (7.0f / pow6f(6.0f / 7.0f)) * (1.0f
 * - F82)`
 */
float4 ggx_brdf_split_sum(float3 lut_coord)
{
  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.x);

  float NV = clamp(1.0f - square(lut_coord.y), 1e-4f, 0.9999f);
  float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);
  float3 N = float3(0.0f, 0.0f, 1.0f);

  /* Integrating BRDF. */
  float scale = 0.0f;
  float bias = 0.0f;
  float metal_bias = 0.0f;
  constexpr uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    float2 rand = hammersley_2d(i, sample_count);
    float3 Xi = sample_cylinder(rand);

    /* Microfacet normal. */
    float3 L = bxdf_ggx_sample_reflection(Xi, V, roughness, false).direction;
    float3 H = normalize(V + L);
    float NL = L.z;

    if (NL > 0.0f) {
      float VH = saturate(dot(V, H));
      float weight = bxdf_ggx_eval_reflection(N, L, V, roughness, false).weight;
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - VH));
      scale += (1.0f - s) * weight;
      bias += s * weight;
      /* F82 tint effect. */
      float b = VH * saturate(pow6f(1.0f - VH));
      metal_bias += b * weight;
    }
  }
  scale /= float(sample_count);
  bias /= float(sample_count);
  metal_bias /= float(sample_count);

  return float4(scale, bias, metal_bias, 0.0f);
}

/* Generate BSDF LUT for `IOR < 1` using Schlick's approximation. Returns the transmittance and the
 * scale and bias for reflectance.
 *
 * The result is interpreted as:
 * `reflectance = F0 * scale + F90 * bias`,
 * `transmittance = (1 - F0) * transmission_factor`. */
float4 ggx_bsdf_split_sum(float3 lut_coord)
{
  float ior = clamp(sqrt(lut_coord.x), 1e-4f, 0.9999f);
  /* ior is sin of critical angle. */
  float critical_cos = sqrt(1.0f - saturate(square(ior)));

  lut_coord.y = lut_coord.y * 2.0f - 1.0f;
  /* Maximize texture usage on both sides of the critical angle. */
  lut_coord.y *= (lut_coord.y > 0.0f) ? (1.0f - critical_cos) : critical_cos;
  /* Center LUT around critical angle to avoid strange interpolation issues when the critical
   * angle is changing. */
  lut_coord.y += critical_cos;
  float NV = clamp(lut_coord.y, 1e-4f, 0.9999f);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.z);

  float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);
  float3 N = float3(0.0f, 0.0f, 1.0f);

  /* Integrating BSDF */
  float scale = 0.0f;
  float bias = 0.0f;
  float transmission_factor = 0.0f;
  constexpr uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    float2 rand = hammersley_2d(i, sample_count);
    float3 Xi = sample_cylinder(rand);

    /* Reflection. */
    float3 R = bxdf_ggx_sample_reflection(Xi, V, roughness, false).direction;
    float NR = R.z;
    if (NR > 0.0f) {
      float3 H = normalize(V + R);
      float3 L = refract(-V, H, 1.0f / ior);
      float HL = abs(dot(H, L));
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = bxdf_ggx_eval_reflection(N, R, V, roughness, false).weight;
      scale += (1.0f - s) * weight;
      bias += s * weight;
    }

    /* Refraction. */
    float3 T = bxdf_ggx_sample_refraction(Xi, V, roughness, ior, 0.0f, false).direction;
    float NT = T.z;
    /* In the case of TIR, `T == float3(0)`. */
    if (NT < 0.0f) {
      float3 H = normalize(ior * T + V);
      float HL = abs(dot(H, T));
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = bxdf_ggx_eval_refraction(N, T, V, roughness, ior, 0.0f, false).weight;
      transmission_factor += (1.0f - s) * weight;
    }
  }
  transmission_factor /= float(sample_count);
  scale /= float(sample_count);
  bias /= float(sample_count);

  return float4(scale, bias, transmission_factor, 0.0f);
}

/* Generate BTDF LUT for `IOR > 1` using Schlick's approximation. Only the transmittance is needed
 * because the scale and bias does not depend on the IOR and can be obtained from the BRDF LUT.
 *
 * Parameterize with `x = sqrt((ior - 1) / (ior + 1))` for higher precision in 1 < IOR < 2,
 * and `y = sqrt(1.0f - cos(theta))`, `z = roughness` similar to BRDF LUT.
 *
 * The result is interpreted as:
 * `transmittance = (1 - F0) * transmission_factor`. */
float4 ggx_btdf_gt_one(float3 lut_coord)
{
  float f0 = clamp(square(lut_coord.x), 1e-4f, 0.9999f);
  float ior = (1.0f + f0) / (1.0f - f0);

  float NV = clamp(1.0f - square(lut_coord.y), 1e-4f, 0.9999f);
  float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);
  float3 N = float3(0.0f, 0.0f, 1.0f);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = square(lut_coord.z);

  /* Integrating BTDF. */
  float transmission_factor = 0.0f;
  constexpr uint sample_count = 512u * 512u;
  for (uint i = 0u; i < sample_count; i++) {
    float2 rand = hammersley_2d(i, sample_count);
    float3 Xi = sample_cylinder(rand);

    /* Refraction. */
    float3 L = bxdf_ggx_sample_refraction(Xi, V, roughness, ior, 0.0f, false).direction;
    float NL = L.z;

    if (NL < 0.0f) {
      float3 H = normalize(ior * L + V);
      float HV = abs(dot(H, V));
      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HV)));

      float weight = bxdf_ggx_eval_refraction(N, L, V, roughness, ior, 0.0f, false).weight;
      transmission_factor += (1.0f - s) * weight;
    }
  }
  transmission_factor /= float(sample_count);

  return float4(transmission_factor, 0.0f, 0.0f, 0.0f);
}

/* Generate SSS translucency profile.
 * We precompute the exit radiance for a slab of homogenous material backface-lit by a directional
 * light. We only integrate for a single color primary since the profile will be applied to each
 * primary independently.
 * For each distance `d` we compute the radiance incoming from an hypothetical parallel plane. */
float4 burley_sss_translucency(float3 lut_coord)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = lut_coord.x * SSS_TRANSMIT_LUT_RADIUS;
  float3 r = thickness / radii;
  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.015 */
  float3 exponential = exp(-3.6f * pow(r, float3(1.11f)));
  float3 gaussian = exp(-pow(3.4f * r, float3(1.6f)));
  float3 fac = square(saturate(0.5f + r / 0.6f));
  float3 profile = saturate(mix(gaussian, exponential, fac));
  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0f - pow5f(lut_coord.x));

  return float4(profile, 0.0f);
}

float4 random_walk_sss_translucency(float3 lut_coord)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = lut_coord.x * SSS_TRANSMIT_LUT_RADIUS;
  float3 r = thickness / radii;
  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.016 */
  float3 scale = float3(0.31f, 0.47f, 0.32f);
  float3 exponent = float3(-22.0f, -5.8f, -0.5f);
  float3 profile = float3(dot(scale, exp(exponent * r.r)),
                          dot(scale, exp(exponent * r.g)),
                          dot(scale, exp(exponent * r.b)));
  profile = saturate(profile - 0.1f);
  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0f - pow5f(lut_coord.x));

  return float4(profile, 0.0f);
}

void main()
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float3 lut_normalized_coordinate = float3(gl_GlobalInvocationID) / float3(table_extent - 1);
  /* Make sure missing cases are noticeable. */
  float4 result = float4(-1);
  switch (uint(table_type)) {
    case LUT_GGX_BRDF_SPLIT_SUM:
      result = ggx_brdf_split_sum(lut_normalized_coordinate);
      break;
    case LUT_GGX_BTDF_IOR_GT_ONE:
      result = ggx_btdf_gt_one(lut_normalized_coordinate);
      break;
    case LUT_GGX_BSDF_SPLIT_SUM:
      result = ggx_bsdf_split_sum(lut_normalized_coordinate);
      break;
    case LUT_BURLEY_SSS_PROFILE:
      result = burley_sss_translucency(lut_normalized_coordinate);
      break;
    case LUT_RANDOM_WALK_SSS_PROFILE:
      result = random_walk_sss_translucency(lut_normalized_coordinate);
      break;
  }
  imageStore(table_img, int3(gl_GlobalInvocationID), result);
}
