/* SPDX-FileCopyrightText: 2017-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Pipelines to generate Look Up Tables for BxDFs. Not used in the default configuration as the
 * tables are stored in the blender executable. This is used for reference or to update them.
 */

#pragma once

#include "eevee_bxdf.bsl.hh"
#include "eevee_bxdf_lut_lib.bsl.hh"
#include "eevee_precompute_shared.hh"
#include "eevee_sampling_lib.bsl.hh"
#include "eevee_subsurface_shared.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_precise.bsl.hh"

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
 * The 2D LUT is parameterized as:
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

  static GGXBrdfClosure from_coords(float3 coords)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(coords.x);

    float NV = clamp(1.0f - square(coords.y), 1e-4f, 0.9999f);
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
    float3 L = GGX::sample_reflection(Xi, V, roughness, false).direction;
    float3 H = normalize(V + L);

    /* Restrict light vector to positive hemisphere. */
    float NL = L.z;
    if (NL > 0.0f) {
      float weight = GGX::eval_reflection(N, L, V, roughness, false).weight;
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
 * Generate 3D GGX BSDF LUT, for IOR < 1.
 *
 * Follows the split sum approximation used in [Real shading in unreal engine 4]
 * (https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf).
 * and uses Schlick's approximation to weight R, T components.
 *
 * The 3D LUT is parameterized as:
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

  static GGXBsdfClosure from_coords(float3 coords)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(coords.z);

    /* ior is sin of critical angle. */
    float ior = clamp(sqrt(coords.x), 1e-4f, 0.9999f);
    float critical_cos = sqrt(1.0f - saturate(square(ior)));

    /* Maximize texture usage on both sides of the critical angle. */
    coords.y = coords.y * 2.0f - 1.0f;
    coords.y *= (coords.y > 0.0f) ? (1.0f - critical_cos) : critical_cos;
    /* Center LUT around critical angle to avoid strange interpolation
     * issues when the critical angle is changing. */
    coords.y += critical_cos;

    float NV = clamp(coords.y, 1e-4f, 0.9999f);
    float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);

    return {roughness, ior, V};
  }

  GGXBsdfData eval(float3 Xi) const
  {
    /* Geometric normal. */
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

    GGXBsdfData bsdf = {};

    /* Reflection, restricted to positive hemisphere. */
    float3 R = GGX::sample_reflection(Xi, V, roughness, false).direction;
    float NR = R.z;
    if (NR > 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(V + R);
      float3 L = refract(-V, H, 1.0f / ior);
      float HL = abs(dot(H, L));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = GGX::eval_reflection(N, R, V, roughness, false).weight;
      bsdf.scale = (1.0f - s) * weight;
      bsdf.bias = s * weight;
    }

    /* Refraction, restricted to negative hemisphere. */
    float3 T = GGX::sample_refraction(Xi, V, roughness, ior, Thickness{}, false).direction;
    float NT = T.z;
    /* In the case of TIR, `T == float3(0)`. */
    if (NT < 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(ior * T + V);
      float HL = abs(dot(H, T));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HL)));

      float weight = GGX::eval_refraction(N, T, V, roughness, ior, Thickness{}, false).weight;
      bsdf.transmission_factor = (1.0f - s) * weight;
    }

    return bsdf;
  }
};

/**
 * Generate Oren Nayar LUT.
 *
 * Reference:
 *
 * [portsmouth2024] Jamie Portsmouth, Peter Kutz and Stephen Hill.
 *                  EON: A practical energy-preserving rough diffuse BRDF (2024)
 *                  https://arxiv.org/pdf/2410.18026
 *
 * The 2D LUT is parameterized as:
 * - `x = roughness`,
 * - `y = cos(theta)`,
 *
 * and output is interpreted as:
 * - `reflectance = color * Ess + Fms * Ems`,
 * where `Fms = color^2 * Eavg / (1 - Eavg) / (1 - color * (1 - Eavg))` and is computed during
 * runtime.
 *
 */

struct OrenNayarClosure {
  OrenNayar closure;
  float3 V;

  static OrenNayarClosure from_coords(const float3 coords)
  {
    const float roughness = coords.x;
    const float NV = coords.y;
    const float3 V = float3(sin_from_cos(NV), 0.0f, NV);

    return {.closure = OrenNayar::init(roughness), .V = V};
  }

  static OrenNayarClosure from_ltc_coords(const float3 coords)
  {
    const ltc::CoordParam coord_param = ltc::from_isotropic_coords(coords);
    const float roughness = coord_param.roughness;
    const float cos_theta = coord_param.cos_theta;
    /* Avoid numerical issues at grazing angles. */
    const float NV = max(cos_theta, 1e-4f);
    const float3 V = float3(sin_from_cos(NV), 0.0f, NV);

    return {.closure = OrenNayar::init(roughness), .V = V};
  }

  OrenNayarData eval(const float3 rand) const
  {
    const float3 L = sample_cos_hemisphere(rand);
    const float nl = max(L.z, 0.0f);
    const float nv = max(V.z, 0.0f);

    return {closure.single_scatter(L, V, nl, nv),
            saturate(1.0f - closure.E(nv)) * saturate(1.0f - closure.E(nl))};
  }
};

/**
 * Generate 3D GGX BTDF LUT, for IOR > 1.
 *
 * Using Schlick's approximation; only the transmittance is needed because scale and
 * bias do not depend on the IOR, and can be obtained independently from the BRDF LUT.
 *
 * The 3D LUT is parameterized as:
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

  static GGXBtdfGt1Closure from_coords(float3 coords)
  {
    /* We use squared roughness for approximate perceptual linearity
     * following [Physically Based Shading at Disney]
     * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
     * Section 5.4. */
    float roughness = square(coords.z);

    float F0 = clamp(square(coords.x), 1e-4f, 0.9999f);
    float ior = (1.0f + F0) / (1.0f - F0);

    float NV = clamp(1.0f - square(coords.y), 1e-4f, 0.9999f);
    float3 V = float3(sqrt(1.0f - square(NV)), 0.0f, NV);

    return {roughness, ior, V};
  }

  GGXBtdfGt1Data eval(float3 Xi) const
  {
    /* Geometric normal. */
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

    GGXBtdfGt1Data btdf = {};

    /* Refraction, restricted to negative hemisphere. */
    float3 L = GGX::sample_refraction(Xi, V, roughness, ior, Thickness{}, false).direction;
    float NL = L.z;
    if (NL < 0.0f) {
      /* Recover half vector as GGX normal. */
      float3 H = normalize(ior * L + V);
      float HV = abs(dot(H, V));

      /* Schlick's Fresnel. */
      float s = saturate(pow5f(1.0f - saturate(HV)));

      float weight = GGX::eval_refraction(N, L, V, roughness, ior, Thickness{}, false).weight;
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
 * Parameters: `x = distance`, while output is interpreted as exit radiance.
 */
float4 burley_sss_translucency(float3 coords)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = coords.x * SSS_TRANSMIT_LUT_RADIUS;
  float3 r = thickness / radii;

  /* Manual fit based on cycles render of a backlit slab of varying thickness.
   * Mean Error: 0.003
   * Max Error: 0.015 */
  float3 exponential = exp(-3.6f * pow(r, float3(1.11f)));
  float3 gaussian = exp(-pow(3.4f * r, float3(1.6f)));
  float3 fac = square(saturate(0.5f + r / 0.6f));
  float3 profile = saturate(mix(gaussian, exponential, fac));

  /* Mask off the end progressively to 0. */
  profile *= saturate(1.0f - pow5f(coords.x));

  return float4(profile, 0.0f);
}

/* Note: unused. */
float4 random_walk_sss_translucency(float3 coords)
{
  /* Note that we only store the 1st (radius == 1) component.
   * The others are here for debugging overall appearance. */
  float3 radii = float3(1.0f, 0.2f, 0.1f);
  float thickness = coords.x * SSS_TRANSMIT_LUT_RADIUS;
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
  profile *= saturate(1.0f - pow5f(coords.x));

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
  // F func = F::from_coords(coords);

  /* Measure func using N samples. */
  float4 measure = float4(0.0f);
  for (uint i = 1u; i <= sample_count; i++) {
    /* Warp sequence to a point on the unit cylinder. */
    float2 rand = random::hammersley_2d(i, sample_count);
    float3 Xi = sample_cylinder(rand);

    /* Add sample to measure. */
    measure += func.eval(Xi).pack();
  }
  return measure / float(sample_count);
}
template float4 integrate<GGXBrdfClosure>(const GGXBrdfClosure &);
template float4 integrate<GGXBsdfClosure>(const GGXBsdfClosure &);
template float4 integrate<GGXBtdfGt1Closure>(const GGXBtdfGt1Closure &);
template float4 integrate<OrenNayarClosure>(const OrenNayarClosure &);

struct LUT {
  [[image(0, read_write, SFLOAT_32_32_32_32)]] image3D image;
  [[push_constant]] const int type;
  [[push_constant]] const int3 extent;
};

/**
 * Fit an isotropic BSDF to LTC.
 *
 * Based on the original code from
 * [Real-Time Polygonal-Light Shading with Linearly Transformed Cosines]
 * (https://eheitzresearch.wordpress.com/415-2/)
 * by Eric Heitz et. al, with modifications:
 * 1. LTC matrices are initialized with identity matrix instead of with results from neighboring
 *    pixels, so that we can fit them all in parallel. This works fine for diffuse BSDFs such as
 *    Oren-Nayar.
 * 2. Instead of m23, which will amount to 0 in the end, the following code fits with parameters at
 *    the 4 corners as described in the paper, and ensures that the determinant of M is 1, so that
 *    we don't need to compute and store det(M) additionally.
 */
struct LTCFitter {
  /* Parametric representation. */
  float m11, m31, m13, m33;
  float3 X, Y, Z;

  /* Matrix Representation. */
  float3x3 M;
  float3x3 invM;

  bool isotropic;

  float4 pack() const
  {
    return float4(m11, m31, m13, m33);
  }

  float4 pack_inverse() const
  {
    return ltc::pack_isotropic_matrix(invM);
  }

  /* Compute matrix from parameters. */
  void update()
  {
    /* Ensure the determinant of M is 1. */
    const float m22 = 1.0f / (m11 * m33 - m31 * m13);
    M = float3x3(X, Y, Z) * float3x3(m11, 0, m31, 0, m22, 0, m13, 0, m33);
    invM = inverse(M);
  }

  void update(const float4 params)
  {
    constexpr float MIN_ALPHA = 0.0001f;

    m11 = max(params.x, MIN_ALPHA);
    if (isotropic) {
      m33 = 1.0f / square(m11);
      m13 = 0.0f;
      m31 = 0.0f;
    }
    else {
      m31 = params.y;
      m13 = params.z;
      m33 = params.w;
    }

    update();
  }

  BsdfEval eval(const float3 &L) const
  {
    constexpr float3 N = float3(0.0f, 0.0f, 1.0f);
    const float pdf = pdf_ltc(L, N, M, invM, 1.0f);

    return {.throughput = pdf, .pdf = pdf, .weight = 1.0f};
  }

  float3 sample(const float2 rand) const
  {
    return sample_ltc(M, rand);
  }
};

template<typename T> float3 average_light_dir(const T &lut)
{
  /* Geometric normal. */
  constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

  float3 average_dir = float3(0.0f);
  constexpr float sample_count = float(50u * 50u);

  for (float i = 0.5f; i < sample_count; i++) {
    /* Warp sequence to a point on the unit cylinder. */
    const float3 rand = sample_cylinder(random::hammersley_2d(i, sample_count));
    const float3 L = lut.closure.bsdf_sample(rand, lut.V).direction;
    if (L.z > 0.0f) {
      average_dir += lut.closure.bsdf_eval(N, lut.V, L).weight * L;
    }
  }

  /* Clear y component, which should be zero with isotropic BRDFs. */
  average_dir.y = 0.0f;

  return normalize(average_dir);
}
template float3 average_light_dir<OrenNayarClosure>(const OrenNayarClosure &);

template<typename BSDF> LTCFitter init_ltc_fit(const BSDF &bsdf)
{
  LTCFitter fitter;
  /* Initialize with diffuse. */
  /* NOTE: this works well with diffuse BSDFs such as Oren Nayar and Sheen. For GGX we will need
   * better initial guesses. */
  fitter.m13 = fitter.m31 = 0.0f;
  fitter.m11 = fitter.m33 = 1.0f;

  fitter.isotropic = (bsdf.V.z == 1.0f);

  const float3 L = fitter.isotropic ? float3(0.0f, 0.0f, 1.0f) : average_light_dir(bsdf);
  fitter.X = float3(L.z, 0, -L.x);
  fitter.Y = float3(0.0f, 1.0f, 0.0f);
  fitter.Z = L;

  fitter.update();

  return fitter;
}
template LTCFitter init_ltc_fit<OrenNayarClosure>(const OrenNayarClosure &);

template<typename BSDF> float error_(const LTCFitter &fitter, const BSDF &bsdf, const float3 L)
{
  /* Geometric normal. */
  constexpr float3 N = float3(0.0f, 0.0f, 1.0f);

  const BsdfEval eval_closure = bsdf.closure.bsdf_eval(N, bsdf.V, L);
  const BsdfEval eval_ltc = fitter.eval(L);
  const float error = abs(eval_closure.throughput - eval_ltc.throughput);

  /* L3 error with MIS weight. */
  return (error * error * error) / (eval_ltc.pdf + eval_closure.pdf);
}
template float error_<OrenNayarClosure>(const LTCFitter &, const OrenNayarClosure &, const float3);

template<typename BSDF>
float compute_error(LTCFitter &fitter, const float4 params, const BSDF &bsdf)
{
  fitter.update(params);

  KahanSum sum = {};
  constexpr float sample_count = float(50u * 50u);

  /* Importance sample LTC. */
  for (float i = 0.5f; i < sample_count; i++) {
    const float2 rand = random::hammersley_2d(i, sample_count);
    const float3 L = fitter.sample(rand);
    sum.add(error_(fitter, bsdf, L));
  }

  /* Importance sample BRDF. */
  for (float i = 0.5f; i < sample_count; i++) {
    const float3 rand = sample_cylinder(random::hammersley_2d(i, sample_count));
    const float3 L = bsdf.closure.bsdf_sample(rand, bsdf.V).direction;
    sum.add(error_(fitter, bsdf, L));
  }

  return sum.sum / sample_count;
}
template float compute_error<OrenNayarClosure>(LTCFitter &,
                                               const float4,
                                               const OrenNayarClosure &);

/* Downhill simplex solver:
 * http://en.wikipedia.org/wiki/Nelder%E2%80%93Mead_method#One_possible_variation_of_the_NM_algorithm
 * using the termination criterion from Numerical Recipes in C++ (3rd Ed.). */
template<typename Destination>
float NelderMead(float4 &result,
                 const float4 start,
                 const float delta,
                 const float tolerance,
                 const int max_iters,
                 LTCFitter objective_fn,
                 const Destination &dest)
{
  /* Standard coefficients from Nelder-Mead. */
  constexpr float reflect = 1.0f;
  constexpr float expand = 2.0f;
  constexpr float contract = 0.5f;
  constexpr float shrink = 0.5f;

  constexpr int DIM = 4;
  constexpr int NUM_POINTS = DIM + 1;

  float4 s[NUM_POINTS];
  float error[NUM_POINTS];

  /* Initialize simplex. */
  s[0] = start;
  for (int i = 1; i < NUM_POINTS; i++) {
    s[i] = start;
    s[i][i - 1] += delta;
  }

  /* Evaluate function at each point on simplex. */
  for (int i = 0; i < NUM_POINTS; i++) {
    error[i] = compute_error(objective_fn, s[i], dest);
  }

  int lo = 0, hi, nh;

  for (int j = 0; j < max_iters; j++) {
    /* Find lowest, highest and next highest. */
    lo = hi = nh = 0;
    for (int i = 1; i < NUM_POINTS; i++) {
      if (error[i] < error[lo]) {
        lo = i;
      }
      if (error[i] > error[hi]) {
        nh = hi;
        hi = i;
      }
      else if (error[i] > error[nh]) {
        nh = i;
      }
    }

    /* Stop if we've reached the required tolerance level. */
    const float a = abs(error[lo]);
    const float b = abs(error[hi]);
    if (2.0f * abs(a - b) < (a + b) * tolerance) {
      break;
    }

    /* Compute centroid (excluding the worst point). */
    float4 o = float4(0.0f);
    for (int i = 0; i < NUM_POINTS; i++) {
      if (i == hi) {
        continue;
      }
      o += s[i];
    }
    o /= DIM;

    /* Reflection. */
    const float4 r = o + reflect * (o - s[hi]);
    const float error_r = compute_error(objective_fn, r, dest);

    if (error_r < error[nh]) {
      if (error_r < error[lo]) {
        /* Expansion. */
        const float4 e = o + expand * (o - s[hi]);
        const float error_e = compute_error(objective_fn, e, dest);

        if (error_e < error_r) {
          s[hi] = e;
          error[hi] = error_e;
          continue;
        }
      }

      s[hi] = r;
      error[hi] = error_r;
      continue;
    }

    /* Contraction. */
    const float4 c = o - contract * (o - s[hi]);
    const float error_c = compute_error(objective_fn, c, dest);

    if (error_c < error[hi]) {
      s[hi] = c;
      error[hi] = error_c;
      continue;
    }

    /* Reduction. */
    for (int k = 0; k < NUM_POINTS; k++) {
      if (k == lo) {
        continue;
      }
      s[k] = s[lo] + shrink * (s[k] - s[lo]);
      error[k] = compute_error(objective_fn, s[k], dest);
    }
  }

  /* Return best point and the error. */
  result = s[lo];
  return error[lo];
}
template float NelderMead<OrenNayarClosure>(
    float4 &, const float4, float, float, int, LTCFitter, const OrenNayarClosure &);

template<typename BSDF> float4 fit_ltc(const float3 coords, const float epsilon = 0.05f)
{
  const BSDF bsdf = BSDF::from_ltc_coords(coords);
  LTCFitter fitter = init_ltc_fit<BSDF>(bsdf);

  /* Find best-fit LTC lobe. */
  float4 result;
  /* float error = */ NelderMead(result, fitter.pack(), epsilon, 1e-5f, 200, fitter, bsdf);

  /* Update LTC with best fitting values. */
  fitter.update(result);
  return fitter.pack_inverse();
}
template float4 fit_ltc<OrenNayarClosure>(const float3, const float);

[[compute]] [[local_size(LUT_WORKGROUP_SIZE, LUT_WORKGROUP_SIZE)]] [[no_fast_math]]
void comp_main([[global_invocation_id]] const uint3 global_id, [[resource_table]] LUT &lut)
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float3 lut_normalized_coordinate = float3(global_id) / float3(lut.extent - 1);

  /* Make sure missing cases are noticeable. */
  float4 result = float4(-1);
  switch (PrecomputeType(lut.type)) {
    case LUT_GGX_BRDF_SPLIT_SUM:
      result = integrate(GGXBrdfClosure::from_coords(lut_normalized_coordinate));
      break;
    case LUT_GGX_BSDF_SPLIT_SUM:
      result = integrate(GGXBsdfClosure::from_coords(lut_normalized_coordinate));
      break;
    case LUT_GGX_BTDF_IOR_GT_ONE:
      result = integrate(GGXBtdfGt1Closure::from_coords(lut_normalized_coordinate));
      break;
    case LUT_BURLEY_SSS_PROFILE:
      result = burley_sss_translucency(lut_normalized_coordinate);
      break;
    case LUT_RANDOM_WALK_SSS_PROFILE:
      result = random_walk_sss_translucency(lut_normalized_coordinate);
      break;
    case LUT_OREN_NAYAR_SPLIT_SUM:
      result = integrate(OrenNayarClosure::from_coords(lut_normalized_coordinate));
      break;
    case LUT_OREN_NAYAR_LTC:
      result = fit_ltc<OrenNayarClosure>(lut_normalized_coordinate);
      break;
  }

  imageStore(lut.image, int3(global_id), result);
}

/** \} */

PipelineCompute comp(comp_main);

}  // namespace eevee::lut
