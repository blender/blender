/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Pipelines to generate Look Up Tables for BxDFs. Not used in the default configuration as the
 * tables are stored in the blender executable. This is used for reference or to update them.
 */

#pragma once

#include "eevee_bxdf_lut_lib.bsl.hh"
#include "eevee_bxdf_sampling_lib.glsl"
#include "eevee_defines.hh"
#include "eevee_light_shared.hh"
#include "eevee_precompute_shared.hh"
#include "eevee_sampling_lib.glsl"
#include "eevee_subsurface_shared.hh"
#include "eevee_uniform_shared.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_math_base_lib.glsl"

namespace eevee::lut {

/* -------------------------------------------------------------------- */
/** \name LUT models
 * \{ */

/**
 * Generate 2D GGX BRDF LUT.
 *
 * Follows the split sum approximation used in [Real shading in unreal engine 4]
 * (https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf).
 *
 * The 2D LUT is parameterized on:
 * - `x = roughness`,
 * - `y = sqrt(1 - cos(theta))`,
 *
 * and the result is interpreted as:
 * - `integral = F0 * scale + F90 * bias - F82_tint * metal_bias`,
 *   where `F82_tint = mix(F0, float3(1), pow5f(6/7) * 7 / pow6f(6/7)) * (1 - F82)`.
 */
struct GGXBrdfClosure {
  float roughness;
  float3 V;

  static GGXBrdfClosure from_params(float3 params)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(params.x);

    float NV = clamp(1.0f - square(params.y), 1e-4f, 0.9999f);
    float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);

    return {roughness, V};
  }

  GGXBrdfData eval(float3 Xi) const
  {
    /* Geometric normal. */
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

    /* Return values. */
    GGXBrdfData brdf = {};

    /* Sample light vector, recover half vector as microfacet normal. */
    float3 L = bxdf_ggx_sample_reflection(Xi, V, roughness, false).direction;
    float3 H = normalize(V + L);

    /* Restrict light vector to positive hemisphere. */
    float NL = L.z;
    if (NL > 0.0f) {
      float weight = bxdf_ggx_eval_reflection(N, L, V, roughness, false).weight;
      float VH = saturate(dot(V, H));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - VH));

      /* F82 tint effect. */
      float b = VH * saturate(pow6f(1.0f - VH));

      brdf.scale = (1.0f - s) * weight;
      brdf.bias = s * weight;
      brdf.metal_bias = b * weight;
    }

    return brdf;
  }
};

/**
 * Generate 3D GGX BSDF LUT.
 *
 * Follows the split sum approximation used in [Real shading in unreal engine 4]
 * (https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf).
 * and uses Schlick's approximation to weight R, T components.
 *
 * The 3D LUT is parameterized on:
 * - `x = sqrt((ior - 1) / (ior + 1))`,
 * - `y = sqrt(1 - cos(theta))`,
 * - `z = roughness`,
 *
 * and output is interpreted as:
 * - `reflectance = F0 * scale + F90 * bias`,
 * - `transmittance = (1 - F0) * transmission_factor`.
 */
struct GGXBsdfClosure {
  float roughness;
  float ior;
  float3 V;

  static GGXBsdfClosure from_params(float3 params)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(params.z);

    /* ior is sin of critical angle. */
    float ior = clamp(sqrt(params.x), 1e-4f, 0.9999f);
    float critical_cos = sqrt(1.0f - saturate(square(ior)));

    /* Maximize texture usage on both sides of the critical angle. */
    params.y = params.y * 2.0f - 1.0f;
    params.y *= (params.y > 0.0f) ? (1.0f - critical_cos) : critical_cos;
    /* Center LUT around critical angle to avoid strange interpolation
     * issues when the critical angle is changing. */
    params.y += critical_cos;

    float NV = clamp(params.y, 1e-4f, 0.9999f);
    float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);

    return {roughness, ior, V};
  }

  GGXBsdfData eval(float3 Xi) const
  {
    /* Geometric normal. */
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

    GGXBsdfData bsdf = {};

    /* Reflection, restricted to positive hemisphere. */
    float3 R = bxdf_ggx_sample_reflection(Xi, V, roughness, false).direction;
    float NR = R.z;
    if (NR > 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(V + R);
      float3 L = refract(-V, H, 1.0f / ior);
      float HL = abs(dot(H, L));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = bxdf_ggx_eval_reflection(N, R, V, roughness, false).weight;
      bsdf.scale = (1.0f - s) * weight;
      bsdf.bias = s * weight;
    }

    /* Refraction, restricted to negative hemisphere. */
    float3 T = bxdf_ggx_sample_refraction(Xi, V, roughness, ior, Thickness{}, false).direction;
    float NT = T.z;
    /* In the case of TIR, `T == float3(0)`. */
    if (NT < 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(ior * T + V);
      float HL = abs(dot(H, T));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = bxdf_ggx_eval_refraction(N, T, V, roughness, ior, Thickness{}, false).weight;
      bsdf.transmission_factor = (1.0f - s) * weight;
    }

    return bsdf;
  }
};

/**
 * Generate 3D GGX BTDF LUT, for IOR > 1.
 *
 * Using Schlick's approximation; only the transmittance is needed because scale and
 * bias do not depend on the IOR, and can be obtained independently from the BRDF LUT.
 *
 * The 3D LUT is parameterized on:
 * - `x = sqrt((ior - 1) / (ior + 1))` for higher precision in the range `1 < IOR < 2`
 * - `y = sqrt(1.0f - cos(theta))`
 * - `z = roughness`
 *
 * and output is interpreted as:
 * - `transmittance = (1 - F0) * transmission_factor`.
 */
struct GGXBtdfGt1Closure {
  float roughness;
  float ior;
  float3 V;

  static GGXBtdfGt1Closure from_params(float3 params)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(params.z);

    float F0 = clamp(square(params.x), 1e-4f, 0.9999f);
    float ior = (1.0f + F0) / (1.0f - F0);

    float NV = clamp(1.0f - square(params.y), 1e-4f, 0.9999f);
    float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);

    return {roughness, ior, V};
  }

  GGXBtdfGt1Data eval(float3 Xi) const
  {
    /* Geometric normal. */
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

    GGXBtdfGt1Data btdf = {};

    /* Refraction, restricted to negative hemisphere. */
    float3 L = bxdf_ggx_sample_refraction(Xi, V, roughness, ior, Thickness{}, false).direction;
    float NL = L.z;
    if (NL < 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(ior * L + V);
      float HV = abs(dot(H, V));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HV)));

      float weight = bxdf_ggx_eval_refraction(N, L, V, roughness, ior, Thickness{}, false).weight;
      btdf.transmission_factor = (1.0f - s) * weight;
    }

    return btdf;
  }
};

/**
 * Generate Christensen-Burley translucency profile.
 *
 * Generally follows:
 * [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering]
 * (https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf)
 * with empirical fitting to match cycles. Precomputes exit radiance for a slab of homogeneous
 * material, back-lit by a directional light. We only care about a single color primary, as the
 * profile is applied to each primary independently. The other components are for debugging.
 *
 * Params: `x = distance`, while output is interpreted as exit radiance.
 */
float4 burley_sss_translucency(float3 params)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = params.x * SSS_TRANSMIT_LUT_RADIUS;
  float3 r = thickness / radii;

  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.015 */
  float3 exponential = exp(-3.6f * pow(r, float3(1.11f)));
  float3 gaussian = exp(-pow(3.4f * r, float3(1.6f)));
  float3 fac = square(saturate(0.5f + r / 0.6f));
  float3 profile = saturate(mix(gaussian, exponential, fac));

  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0f - pow5f(params.x));

  return float4(profile, 0.0f);
}

/* Note: unused. */
float4 random_walk_sss_translucency(float3 params)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = params.x * SSS_TRANSMIT_LUT_RADIUS;
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
  profile *= saturate(1.0f - pow5f(params.x));

  return float4(profile, 0.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LUT computation, integration
 * \{ */

template<typename F> float4 integrate(const F &func)
{
  constexpr uint sample_count = 512u * 512u;

  /* TODO(not_mark): Remove workaround for BSL-spec #4. */
  // F func = F::from_params(params);

  /* Measure func using N samples. */
  float4 measure = float4(0.0f);
  for (uint i = 1u; i <= sample_count; i++) {
    /* Warp sequence to a point on the unit cylinder. */
    float2 rand = hammersley_2d(i, sample_count);
    float3 Xi = sample_cylinder(rand);

    /* Add sample to measure. */
    measure += func.eval(Xi).pack();
  }
  return measure / float(sample_count);
}
template float4 integrate<GGXBrdfClosure>(const GGXBrdfClosure &);
template float4 integrate<GGXBsdfClosure>(const GGXBsdfClosure &);
template float4 integrate<GGXBtdfGt1Closure>(const GGXBtdfGt1Closure &);

struct LUT {
  [[image(0, read_write, SFLOAT_32_32_32_32)]] image3D image;
  [[push_constant]] const int type;
  [[push_constant]] const int3 extent;
};

[[compute]] [[local_size(LUT_WORKGROUP_SIZE, LUT_WORKGROUP_SIZE)]]
void comp_main([[global_invocation_id]] const uint3 global_id, [[resource_table]] LUT &lut)
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float3 lut_normalized_coordinate = float3(global_id) / float3(lut.extent - 1);

  /* Make sure missing cases are noticeable. */
  float4 result = float4(-1);
  switch (uint(lut.type)) {
    case LUT_GGX_BRDF_SPLIT_SUM:
      result = integrate(GGXBrdfClosure::from_params(lut_normalized_coordinate));
      break;
    case LUT_GGX_BSDF_SPLIT_SUM:
      result = integrate(GGXBsdfClosure::from_params(lut_normalized_coordinate));
      break;
    case LUT_GGX_BTDF_IOR_GT_ONE:
      result = integrate(GGXBtdfGt1Closure::from_params(lut_normalized_coordinate));
      break;
    case LUT_BURLEY_SSS_PROFILE:
      result = burley_sss_translucency(lut_normalized_coordinate);
      break;
    case LUT_RANDOM_WALK_SSS_PROFILE:
      result = random_walk_sss_translucency(lut_normalized_coordinate);
      break;
  }

  imageStore(lut.image, int3(global_id), result);
}

/** \} */

PipelineCompute comp(comp_main);

}  // namespace eevee::lut
