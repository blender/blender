/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/closure/bsdf_util.h"
#include "kernel/sample/mapping.h"
#include "kernel/util/lookup_table.h"

#include "util/math_fast.h"

CCL_NAMESPACE_BEGIN

enum MicrofacetType {
  BECKMANN,
  GGX,
};

enum MicrofacetFresnel {
  NONE = 0,
  DIELECTRIC,
  DIELECTRIC_TINT, /* used by the OSL MaterialX closures */
  CONDUCTOR,
  GENERALIZED_SCHLICK,
  F82_TINT,
};

struct FresnelDielectricTint {
  FresnelThinFilm thin_film;

  Spectrum reflection_tint;
  Spectrum transmission_tint;
};

struct FresnelConductor {
  FresnelThinFilm thin_film;
  complex<Spectrum> ior;
};

struct FresnelGeneralizedSchlick {
  FresnelThinFilm thin_film;

  Spectrum reflection_tint;
  Spectrum transmission_tint;
  /* Reflectivity at perpendicular (F0) and glancing (F90) angles. */
  Spectrum f0, f90;
  /* Negative exponent signals a special case where the real Fresnel is remapped to F0...F90. */
  float exponent;
};

struct FresnelF82Tint {
  FresnelThinFilm thin_film;

  /* Perpendicular reflectivity. */
  Spectrum f0;
  /* Precomputed (1-cos)^6 factor for edge tint. */
  Spectrum b;
};

struct MicrofacetBsdf {
  SHADER_CLOSURE_BASE;

  float alpha_x, alpha_y, ior;

  /* Used to account for missing energy due to the single-scattering microfacet model.
   * This could be included in bsdf->weight as well, but there it would mess up the color
   * channels.
   * Note that this is currently only used by GGX. */
  float energy_scale;

  /* Fresnel model to apply, as well as the extra data for it.
   * For NONE and DIELECTRIC, no extra storage is needed, so the pointer is nullptr for them. */
  int fresnel_type;
  ccl_private void *fresnel;

  float3 T;
};

static_assert(sizeof(ShaderClosure) >= sizeof(MicrofacetBsdf), "MicrofacetBsdf is too large!");

/* Beckmann VNDF importance sampling algorithm from:
 * Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals.
 * Eric Heitz and Eugene d'Eon, EGSR 2014.
 * https://hal.inria.fr/hal-00996995v2/document */

ccl_device_forceinline float3 microfacet_beckmann_sample_vndf(const float3 wi,
                                                              const float alpha_x,
                                                              const float alpha_y,
                                                              const float2 rand)
{
  /* 1. stretch wi */
  float3 wi_ = make_float3(alpha_x * wi.x, alpha_y * wi.y, wi.z);
  wi_ = normalize(wi_);

  /* 2. sample P22_{wi}(x_slope, y_slope, 1, 1) */
  float2 slope;
  float cos_phi_i = 1.0f;
  float sin_phi_i = 0.0f;

  if (wi_.z >= 0.99999f) {
    /* Special case (normal incidence). */
    const float r = sqrtf(-logf(rand.x));
    const float phi = M_2PI_F * rand.y;
    slope = polar_to_cartesian(r, phi);
  }
  else {
    /* Precomputations. */
    const float cos_theta_i = wi_.z;
    const float sin_theta_i = sin_from_cos(cos_theta_i);
    const float tan_theta_i = sin_theta_i / cos_theta_i;
    const float cot_theta_i = 1.0f / tan_theta_i;
    const float erf_a = fast_erff(cot_theta_i);
    const float exp_a2 = expf(-cot_theta_i * cot_theta_i);
    const float SQRT_PI_INV = 0.56418958354f;

    const float invlen = 1.0f / sin_theta_i;
    cos_phi_i = wi_.x * invlen;
    sin_phi_i = wi_.y * invlen;

    /* Based on paper from Wenzel Jakob
     * An Improved Visible Normal Sampling Routine for the Beckmann Distribution
     *
     * http://www.mitsuba-renderer.org/~wenzel/files/visnormal.pdf
     *
     * Reformulation from OpenShadingLanguage which avoids using inverse
     * trigonometric functions.
     */

    /* Sample slope X.
     *
     * Compute a coarse approximation using the approximation:
     * `exp(-ierf(x)^2) ~= 1 - x * x`
     * `solve y = 1 + b + K * (1 - b * b)`
     */
    const float K = tan_theta_i * SQRT_PI_INV;
    const float y_approx = rand.x * (1.0f + erf_a + K * (1 - erf_a * erf_a));
    const float y_exact = rand.x * (1.0f + erf_a + K * exp_a2);
    const float b = K > 0 ? (0.5f - sqrtf(K * (K - y_approx + 1.0f) + 0.25f)) / K :
                            y_approx - 1.0f;

    float inv_erf = fast_ierff(b);
    float2 begin = make_float2(-1.0f, -y_exact);
    float2 end = make_float2(erf_a, 1.0f + erf_a + K * exp_a2 - y_exact);
    float2 current = make_float2(b, 1.0f + b + K * expf(-sqr(inv_erf)) - y_exact);

    /* Find root in a monotonic interval using newton method, under given precision and maximal
     * iterations. Falls back to bisection if newton step produces results outside of the valid
     * interval. */
    const float precision = 1e-6f;
    const int max_iter = 3;
    int iter = 0;
    while (fabsf(current.y) > precision && iter++ < max_iter) {
      if (signf(begin.y) == signf(current.y)) {
        begin.x = current.x;
        begin.y = current.y;
      }
      else {
        end.x = current.x;
      }
      const float newton_x = current.x - current.y / (1.0f - inv_erf * tan_theta_i);
      current.x = (newton_x >= begin.x && newton_x <= end.x) ? newton_x : 0.5f * (begin.x + end.x);
      inv_erf = fast_ierff(current.x);
      current.y = 1.0f + current.x + K * expf(-sqr(inv_erf)) - y_exact;
    }

    slope.x = inv_erf;
    slope.y = fast_ierff(2.0f * rand.y - 1.0f);
  }

  /* 3. rotate */
  slope = make_float2(cos_phi_i * slope.x - sin_phi_i * slope.y,
                      sin_phi_i * slope.x + cos_phi_i * slope.y);

  /* 4. unstretch */
  slope *= make_float2(alpha_x, alpha_y);

  /* 5. compute normal */
  return normalize(make_float3(-slope.x, -slope.y, 1.0f));
}

/* GGX VNDF importance sampling algorithm from:
 * Sampling the GGX Distribution of Visible Normals.
 * Eric Heitz, JCGT Vol. 7, No. 4, 2018.
 * https://jcgt.org/published/0007/04/01/ */
ccl_device_forceinline float3 microfacet_ggx_sample_vndf(const float3 wi,
                                                         const float alpha_x,
                                                         const float alpha_y,
                                                         const float2 rand)
{
  /* Section 3.2: Transforming the view direction to the hemisphere configuration. */
  const float3 wi_ = normalize(make_float3(alpha_x * wi.x, alpha_y * wi.y, wi.z));

  /* Section 4.1: Orthonormal basis. */
  const float lensq = sqr(wi_.x) + sqr(wi_.y);
  float3 T1;
  float3 T2;
  if (lensq > 1e-7f) {
    T1 = make_float3(-wi_.y, wi_.x, 0.0f) * inversesqrtf(lensq);
    T2 = cross(wi_, T1);
  }
  else {
    /* Normal incidence, any basis is fine. */
    T1 = make_float3(1.0f, 0.0f, 0.0f);
    T2 = make_float3(0.0f, 1.0f, 0.0f);
  }

  /* Section 4.2: Parameterization of the projected area. */
  float2 t = sample_uniform_disk(rand);
  t.y = mix(safe_sqrtf(1.0f - sqr(t.x)), t.y, 0.5f * (1.0f + wi_.z));

  /* Section 4.3: Reprojection onto hemisphere. */
  const float3 H_ = to_global(disk_to_hemisphere(t), T1, T2, wi_);

  /* Section 3.4: Transforming the normal back to the ellipsoid configuration. */
  return normalize(make_float3(alpha_x * H_.x, alpha_y * H_.y, max(0.0f, H_.z)));
}

/* Computes the Fresnel reflectance and transmittance given the Microfacet BSDF and the cosine of
 * the incoming angle `cos_theta_i`.
 * Also returns the cosine of the angle between the normal and the refracted ray as `r_cos_theta_t`
 * if provided. */
ccl_device_forceinline void microfacet_fresnel(KernelGlobals kg,
                                               const ccl_private MicrofacetBsdf *bsdf,
                                               const float cos_theta_i,
                                               ccl_private float *r_cos_theta_t,
                                               ccl_private Spectrum *r_reflectance,
                                               ccl_private Spectrum *r_transmittance)
{
  /* Whether the closure has reflective or transmissive lobes. */
  const bool has_reflection = !CLOSURE_IS_REFRACTION(bsdf->type);
  const bool has_transmission = CLOSURE_IS_GLASS(bsdf->type) || !has_reflection;

  if (bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC) {
    const Spectrum F = make_spectrum(fresnel_dielectric(cos_theta_i, bsdf->ior, r_cos_theta_t));
    *r_reflectance = F;
    *r_transmittance = one_spectrum() - F;
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC_TINT) {
    ccl_private FresnelDielectricTint *fresnel = (ccl_private FresnelDielectricTint *)
                                                     bsdf->fresnel;
    const float F = fresnel_dielectric(cos_theta_i, bsdf->ior, r_cos_theta_t);
    *r_reflectance = F * fresnel->reflection_tint;
    *r_transmittance = (1.0f - F) * fresnel->transmission_tint;
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::CONDUCTOR) {
    ccl_private FresnelConductor *fresnel = (ccl_private FresnelConductor *)bsdf->fresnel;

    if (fresnel->thin_film.thickness > THINFILM_THICKNESS_CUTOFF) {
      *r_reflectance = fresnel_iridescence<Spectrum>(
          kg, 1.0f, fresnel->thin_film, fresnel->ior, nullptr, cos_theta_i, r_cos_theta_t);
    }
    else {
      *r_reflectance = fresnel_conductor(cos_theta_i, fresnel->ior);
    }

    *r_transmittance = zero_spectrum();
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::F82_TINT) {
    /* F82-Tint model, described in "Novel aspects of the Adobe Standard Material" by Kutz et al.
     * Essentially, this is the usual Schlick Fresnel with an additional cosI*(1-cosI)^6
     * term which modulates the reflectivity around acos(1/7) degrees (ca. 82°). */
    ccl_private FresnelF82Tint *fresnel = (ccl_private FresnelF82Tint *)bsdf->fresnel;

    if (fresnel->thin_film.thickness > THINFILM_THICKNESS_CUTOFF) {
      /* Estimate n and k by reinterpreting F0 and F82 as r and g from "Artist Friendly Metallic
       * Fresnel" by Ole Gulbrandsen. */
      const Spectrum r = min(fresnel->f0, make_float3(0.999f));
      const Spectrum g = fresnel_f82(1.0f / 7.0f, fresnel->f0, fresnel->b);

      const Spectrum sqrt_r = sqrt(r);
      const Spectrum n = mix((1.0f + sqrt_r) / (1.0f - sqrt_r), (1.0f - r) / (1.0f + r), g);
      const Spectrum k = safe_sqrt((r * sqr(n + 1) - sqr(n - 1)) / (1.0f - r));

      *r_reflectance = fresnel_iridescence<Spectrum>(
          kg, 1.0f, fresnel->thin_film, {n, k}, &g, cos_theta_i, r_cos_theta_t);
    }
    else {
      *r_reflectance = fresnel_f82(cos_theta_i, fresnel->f0, fresnel->b);
    }

    *r_transmittance = zero_spectrum();
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::GENERALIZED_SCHLICK) {
    ccl_private FresnelGeneralizedSchlick *fresnel = (ccl_private FresnelGeneralizedSchlick *)
                                                         bsdf->fresnel;
    Spectrum F;
    if (fresnel->thin_film.thickness > THINFILM_THICKNESS_CUTOFF) {
      /* Iridescence doesn't combine well with the general case. We only expose it through the
       * Principled BSDF for now, so it's fine to not support custom exponents and F90. */
      kernel_assert(fresnel->exponent < 0.0f);
      kernel_assert(fresnel->f90 == one_spectrum());
      F = fresnel_iridescence<float>(
          kg, 1.0f, fresnel->thin_film, {bsdf->ior, 0.0f}, nullptr, cos_theta_i, r_cos_theta_t);
      /* Apply F0 scaling (here per-channel, since iridescence produces colored output).
       * Note that the usual approach (as used below) cannot be used here, since F may be below
       * F0_real. Therefore, use a different approach: Scale the result by (F0 / F0_real), with
       * the strength of the scaling depending on how close F is to F0_real.
       * There isn't one single "correct" way to do this, it's just for artistic control anyways.
       */
      const float F0_real = F0_from_ior(bsdf->ior);
      if (F0_real > 1e-5f && !isequal(F, one_spectrum())) {
        FOREACH_SPECTRUM_CHANNEL (i) {
          const float s = saturatef(inverse_lerp(1.0f, F0_real, GET_SPECTRUM_CHANNEL(F, i)));
          const float factor = GET_SPECTRUM_CHANNEL(fresnel->f0, i) / F0_real;
          GET_SPECTRUM_CHANNEL(F, i) *= mix(1.0f, factor, s);
        }
      }
    }
    else if (fresnel->exponent < 0.0f) {
      /* Special case: Use real Fresnel curve to determine the interpolation between F0 and F90.
       * Used by Principled BSDF. */
      const float F_real = fresnel_dielectric(cos_theta_i, bsdf->ior, r_cos_theta_t);
      const float F0_real = F0_from_ior(bsdf->ior);
      const float s = saturatef(inverse_lerp(F0_real, 1.0f, F_real));
      F = mix(fresnel->f0, fresnel->f90, s);
    }
    else {
      /* Regular case: Generalized Schlick term. */
      const float cos_theta_t_sq = 1.0f - (1.0f - sqr(cos_theta_i)) / sqr(bsdf->ior);
      if (cos_theta_t_sq <= 0.0f) {
        /* Total internal reflection */
        *r_reflectance = fresnel->reflection_tint * (float)has_reflection;
        *r_transmittance = zero_spectrum();
        return;
      }
      const float cos_theta_t = sqrtf(cos_theta_t_sq);
      if (r_cos_theta_t) {
        *r_cos_theta_t = cos_theta_t;
      }

      /* TODO(lukas): Is a special case for exponent==5 worth it? */
      /* When going from a higher to a lower IOR, we must use the transmitted angle. */
      const float fresnel_angle = ((bsdf->ior < 1.0f) ? cos_theta_t : cos_theta_i);
      const float s = powf(1.0f - fresnel_angle, fresnel->exponent);
      F = mix(fresnel->f0, fresnel->f90, s);
    }
    *r_reflectance = F * fresnel->reflection_tint;
    *r_transmittance = (one_spectrum() - F) * fresnel->transmission_tint;
  }
  else {
    kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::NONE);
    /* No Fresnel used, this is either purely reflective or purely refractive closure. */
    *r_reflectance = *r_transmittance = one_spectrum();

    /* Exclude total internal reflection. */
    if (has_transmission && fresnel_dielectric(cos_theta_i, bsdf->ior, r_cos_theta_t) == 1.0f) {
      *r_transmittance = zero_spectrum();
    }
  }

  *r_reflectance *= (float)has_reflection;
  *r_transmittance *= (float)has_transmission;
}

ccl_device_inline void microfacet_ggx_preserve_energy(KernelGlobals kg,
                                                      ccl_private MicrofacetBsdf *bsdf,
                                                      const ccl_private ShaderData *sd,
                                                      const Spectrum Fss)
{
  const float mu = dot(sd->wi, bsdf->N);
  const float rough = sqrtf(sqrtf(bsdf->alpha_x * bsdf->alpha_y));

  float E;
  float E_avg;
  if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_ID) {
    E = lookup_table_read_2D(kg, rough, mu, kernel_data.tables.ggx_E, 32, 32);
    E_avg = lookup_table_read(kg, rough, kernel_data.tables.ggx_Eavg, 32);
  }
  else if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID) {
    int ofs = kernel_data.tables.ggx_glass_E;
    int avg_ofs = kernel_data.tables.ggx_glass_Eavg;
    float ior = bsdf->ior;
    if (ior < 1.0f) {
      ior = 1.0f / ior;
      ofs = kernel_data.tables.ggx_glass_inv_E;
      avg_ofs = kernel_data.tables.ggx_glass_inv_Eavg;
    }
    /* TODO: Bias mu towards more precision for low values. */
    const float z = sqrtf(fabsf((ior - 1.0f) / (ior + 1.0f)));
    E = lookup_table_read_3D(kg, rough, mu, z, ofs, 16, 16, 16);
    E_avg = lookup_table_read_2D(kg, rough, z, avg_ofs, 16, 16);
  }
  else {
    kernel_assert(false);
    E = 1.0f;
    E_avg = 1.0f;
  }

  const float missing_factor = ((1.0f - E) / E);
  bsdf->energy_scale = 1.0f + missing_factor;

  /* Check if we need to account for extra darkening/saturation due to multi-bounce Fresnel. */
  if (!isequal(Fss, one_spectrum())) {
    /* Fms here is based on the appendix of "Revisiting Physically Based Shading at Imageworks"
     * by Christopher Kulla and Alejandro Conty,
     * with one Fss cancelled out since this is just a multiplier on top of
     * the single-scattering BSDF, which already contains one bounce of Fresnel. */
    const Spectrum Fms = Fss * E_avg / (one_spectrum() - Fss * (1.0f - E_avg));
    /* Since we already include the energy compensation in bsdf->energy_scale,
     * this term is what's needed to make the full BSDF * weight * energy_scale
     * computation work out to the correct value. */
    const Spectrum darkening = (one_spectrum() + Fms * missing_factor) / bsdf->energy_scale;
    bsdf->weight *= darkening;
    bsdf->sample_weight *= average(darkening);
  }
}

/* This function estimates the albedo of the BSDF (NOT including the bsdf->weight) as caused by
 * the applied Fresnel model for the given view direction.
 * The base microfacet model is assumed to have an albedo of 1 (we have the energy preservation
 * code for that), but e.g. a reflection-only closure with Fresnel applied can end up having
 * a very low overall albedo.
 * This is used to adjust the sample weight, as well as for the Diff/Gloss/Trans Color pass
 * and the Denoising Albedo pass.
 *
 * TODO: The Schlick LUT seems to assume energy preservation, which is not true for GGX. if
 * energy-preserving then transmission should just be `1 - reflection`. For dielectric we could
 * probably split the LUT for multiGGX if smooth assumption is not good enough. */
ccl_device Spectrum bsdf_microfacet_estimate_albedo(KernelGlobals kg,
                                                    const ccl_private ShaderData *sd,
                                                    const ccl_private MicrofacetBsdf *bsdf,
                                                    const bool eval_reflection,
                                                    const bool eval_transmission)
{
  const float cos_NI = dot(sd->wi, bsdf->N);
  Spectrum reflectance;
  Spectrum transmittance;
  microfacet_fresnel(kg, bsdf, cos_NI, nullptr, &reflectance, &transmittance);

  reflectance *= (float)eval_reflection;
  transmittance *= (float)eval_transmission;

  /* Use lookup tables for generalized Schlick reflection, otherwise assume smooth surface. */
  if (is_zero(reflectance)) {
    /* Reflectivity is either zero or not requested, so don't compute the more complex estimate. */
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::GENERALIZED_SCHLICK) {
    ccl_private FresnelGeneralizedSchlick *fresnel = (ccl_private FresnelGeneralizedSchlick *)
                                                         bsdf->fresnel;

    if (fresnel->thin_film.thickness > THINFILM_THICKNESS_CUTOFF) {
      /* Precomputing LUTs for thin-film iridescence isn't viable, so fall back to the specular
       * reflection approximation from the microfacet_fresnel call above in that case. */
    }
    else {
      const float rough = sqrtf(sqrtf(bsdf->alpha_x * bsdf->alpha_y));
      float s;
      if (fresnel->exponent < 0.0f) {
        const float z = sqrtf(fabsf((bsdf->ior - 1.0f) / (bsdf->ior + 1.0f)));
        s = lookup_table_read_3D(
            kg, rough, cos_NI, z, kernel_data.tables.ggx_gen_schlick_ior_s, 16, 16, 16);
      }
      else {
        const float z = 1.0f / (0.2f * fresnel->exponent + 1.0f);
        s = lookup_table_read_3D(
            kg, rough, cos_NI, z, kernel_data.tables.ggx_gen_schlick_s, 16, 16, 16);
      }
      reflectance = mix(fresnel->f0, fresnel->f90, s) * fresnel->reflection_tint;
    }
  }
  else if (bsdf->fresnel_type == MicrofacetFresnel::F82_TINT) {
    ccl_private FresnelF82Tint *fresnel = (ccl_private FresnelF82Tint *)bsdf->fresnel;

    if (fresnel->thin_film.thickness > THINFILM_THICKNESS_CUTOFF) {
      /* Precomputing LUTs for thin-film iridescence isn't viable, so fall back to the specular
       * reflection approximation from the microfacet_fresnel call above in that case. */
    }
    else {
      const float rough = sqrtf(sqrtf(bsdf->alpha_x * bsdf->alpha_y));
      const float s = lookup_table_read_3D(
          kg, rough, cos_NI, 0.5f, kernel_data.tables.ggx_gen_schlick_s, 16, 16, 16);
      /* TODO: Precompute B factor term and account for it here. */
      reflectance = mix(fresnel->f0, one_spectrum(), s);
    }
  }
  else if ((bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC ||
            bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC_TINT) &&
           bsdf->ior > 1.0f)
  {
    /* We can re-use the ggx_gen_schlick_ior_s table here, since it's already precomputed for our
     * exponent<0 corner case where we use the real dielectric Fresnel. */
    const float rough = sqrtf(sqrtf(bsdf->alpha_x * bsdf->alpha_y));
    const float z = sqrtf(fabsf((bsdf->ior - 1.0f) / (bsdf->ior + 1.0f)));
    const float s = lookup_table_read_3D(
        kg, rough, cos_NI, z, kernel_data.tables.ggx_gen_schlick_ior_s, 16, 16, 16);
    reflectance = make_spectrum(mix(F0_from_ior(bsdf->ior), 1.0f, s));
    if (bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC_TINT) {
      ccl_private FresnelDielectricTint *fresnel = (ccl_private FresnelDielectricTint *)
                                                       bsdf->fresnel;
      reflectance *= fresnel->reflection_tint;
    }
  }

  return reflectance + transmittance;
}

/* Smith shadowing-masking term, here in the non-separable form.
 * For details, see:
 * Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs.
 * Eric Heitz, JCGT Vol. 3, No. 2, 2014.
 * https://jcgt.org/published/0003/02/03/ */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_lambda_from_sqr_alpha_tan_n(const float sqr_alpha_tan_n)
{
  if (m_type == MicrofacetType::GGX) {
    /* Equation 72. */
    return 0.5f * (sqrtf(1.0f + sqr_alpha_tan_n) - 1.0f);
  }
  kernel_assert(m_type == MicrofacetType::BECKMANN);
  /* Approximation from below Equation 69. */
  if (sqr_alpha_tan_n < 0.39f) {
    /* Equivalent to a >= 1.6f, but also handles sqr_alpha_tan_n == 0.0f cleanly. */
    return 0.0f;
  }

  const float a = inversesqrtf(sqr_alpha_tan_n);
  return ((0.396f * a - 1.259f) * a + 1.0f) / ((2.181f * a + 3.535f) * a);
}

template<MicrofacetType m_type>
ccl_device_inline float bsdf_lambda(const float alpha2, const float cos_N)
{
  return bsdf_lambda_from_sqr_alpha_tan_n<m_type>(alpha2 * fmaxf(1.0f / sqr(cos_N) - 1.0f, 0.0f));
}

template<MicrofacetType m_type>
ccl_device_inline float bsdf_aniso_lambda(const float alpha_x, const float alpha_y, const float3 V)
{
  const float sqr_alpha_tan_n = (sqr(alpha_x * V.x) + sqr(alpha_y * V.y)) / sqr(V.z);
  return bsdf_lambda_from_sqr_alpha_tan_n<m_type>(sqr_alpha_tan_n);
}

/* Mono-directional shadowing-masking term. */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_G(const float alpha2, const float cos_N)
{
  return 1.0f / (1.0f + bsdf_lambda<m_type>(alpha2, cos_N));
}

/* Combined shadowing-masking term. */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_G(const float alpha2, const float cos_NI, const float cos_NO)
{
  return 1.0f / (1.0f + bsdf_lambda<m_type>(alpha2, cos_NI) + bsdf_lambda<m_type>(alpha2, cos_NO));
}

/* Normal distribution function. */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_D(const float alpha2, const float cos_NH)
{
  const float cos_NH2 = min(sqr(cos_NH), 1.0f);
  const float one_minus_cos_NH2 = 1.0f - cos_NH2;

  if (m_type == MicrofacetType::BECKMANN) {
    return 1.0f / (expf(one_minus_cos_NH2 / (cos_NH2 * alpha2)) * M_PI_F * alpha2 * sqr(cos_NH2));
  }
  kernel_assert(m_type == MicrofacetType::GGX);
  return alpha2 / (M_PI_F * sqr(one_minus_cos_NH2 + alpha2 * cos_NH2));
}

template<MicrofacetType m_type>
ccl_device_inline float bsdf_aniso_D(const float alpha_x, const float alpha_y, float3 H)
{
  H /= make_float3(alpha_x, alpha_y, 1.0f);

  const float cos_NH2 = sqr(H.z);
  const float alpha2 = alpha_x * alpha_y;

  if (m_type == MicrofacetType::BECKMANN) {
    return expf(-(sqr(H.x) + sqr(H.y)) / cos_NH2) / (M_PI_F * alpha2 * sqr(cos_NH2));
  }
  kernel_assert(m_type == MicrofacetType::GGX);
  return M_1_PI_F / (alpha2 * sqr(len_squared(H)));
}

/* Do not set `SD_BSDF_HAS_EVAL` flag if the squared roughness is below a certain threshold. */
ccl_device_forceinline int bsdf_microfacet_eval_flag(const ccl_private MicrofacetBsdf *bsdf)
{
  return (bsdf->alpha_x * bsdf->alpha_y > BSDF_ROUGHNESS_SQ_THRESH) ? SD_BSDF_HAS_EVAL : 0;
}

template<MicrofacetType m_type>
ccl_device Spectrum bsdf_microfacet_eval(KernelGlobals kg,
                                         const ccl_private ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  const ccl_private MicrofacetBsdf *bsdf = (const ccl_private MicrofacetBsdf *)sc;

  /* Whether the closure has reflective or transmissive lobes. */
  const bool has_reflection = !CLOSURE_IS_REFRACTION(bsdf->type);
  const bool has_transmission = CLOSURE_IS_GLASS(bsdf->type) || !has_reflection;

  const float3 N = bsdf->N;
  const float cos_NI = dot(N, wi);
  const float cos_NO = dot(N, wo);

  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;

  const bool is_transmission = (cos_NO < 0.0f);

  /* Check whether the pair of directions is valid for evaluation:
   * - Incoming direction has to be in the upper hemisphere (Cycles convention)
   * - Specular cases can't be evaluated, only sampled.
   * - Purely reflective closures can't have refraction.
   * - Purely refractive closures can't have reflection.
   */
  if ((cos_NI <= 0) || !bsdf_microfacet_eval_flag(bsdf) ||
      (is_transmission && !has_transmission) || (!is_transmission && !has_reflection))
  {
    return zero_spectrum();
  }

  /* Compute half vector. */
  /* TODO: deal with the case when `bsdf->ior` is close to one. */
  /* TODO: check if the refraction configuration is valid. See `btdf_ggx()` in
   * `eevee_bxdf_lib.glsl`. */
  float3 H = is_transmission ? -(bsdf->ior * wo + wi) : (wi + wo);
  const float inv_len_H = safe_divide(1.0f, len(H));
  H *= inv_len_H;

  /* Compute Fresnel coefficients. */
  const float cos_HI = dot(H, wi);
  Spectrum reflectance;
  Spectrum transmittance;
  microfacet_fresnel(kg, bsdf, cos_HI, nullptr, &reflectance, &transmittance);

  if (is_zero(reflectance) && is_zero(transmittance)) {
    return zero_spectrum();
  }

  const float cos_NH = dot(N, H);
  float D;
  float lambdaI;
  float lambdaO;

  /* NOTE: we could add support for anisotropic transmission, although it will make dispersion
   * harder to compute. */
  if (alpha_x == alpha_y || is_transmission) { /* Isotropic. */
    const float alpha2 = alpha_x * alpha_y;
    D = bsdf_D<m_type>(alpha2, cos_NH);
    lambdaI = bsdf_lambda<m_type>(alpha2, cos_NI);
    lambdaO = bsdf_lambda<m_type>(alpha2, cos_NO);
  }
  else { /* Anisotropic. */
    float3 X;
    float3 Y;
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);

    const float3 local_H = make_float3(dot(X, H), dot(Y, H), cos_NH);
    const float3 local_I = make_float3(dot(X, wi), dot(Y, wi), cos_NI);
    const float3 local_O = make_float3(dot(X, wo), dot(Y, wo), cos_NO);

    D = bsdf_aniso_D<m_type>(alpha_x, alpha_y, local_H);

    lambdaI = bsdf_aniso_lambda<m_type>(alpha_x, alpha_y, local_I);
    lambdaO = bsdf_aniso_lambda<m_type>(alpha_x, alpha_y, local_O);
  }

  const float common = D / cos_NI *
                       (is_transmission ? sqr(bsdf->ior * inv_len_H) * fabsf(cos_HI * dot(H, wo)) :
                                          0.25f);

  const float pdf_reflect = average(reflectance) / average(reflectance + transmittance);
  const float lobe_pdf = is_transmission ? 1.0f - pdf_reflect : pdf_reflect;

  *pdf = common * lobe_pdf / (1.0f + lambdaI);
  return (is_transmission ? transmittance : reflectance) * common / (1.0f + lambdaO + lambdaI);
}

template<MicrofacetType m_type>
ccl_device int bsdf_microfacet_sample(KernelGlobals kg,
                                      const ccl_private ShaderClosure *sc,
                                      const float3 Ng,
                                      const float3 wi,
                                      const float3 rand,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf,
                                      ccl_private float2 *sampled_roughness,
                                      ccl_private float *eta)
{
  const ccl_private MicrofacetBsdf *bsdf = (const ccl_private MicrofacetBsdf *)sc;

  const float3 N = bsdf->N;
  const float cos_NI = dot(N, wi);
  if (cos_NI <= 0) {
    /* Incident angle from the lower hemisphere is invalid. */
    return LABEL_NONE;
  }

  const float m_eta = bsdf->ior;
  const float m_inv_eta = safe_divide(1.0f, bsdf->ior);
  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;
  bool m_singular = !bsdf_microfacet_eval_flag(bsdf);

  /* Half vector. */
  float3 H;
  /* Needed for anisotropic microfacets later. */
  float3 local_H;
  float3 local_I;
  if (m_singular) {
    H = N;
  }
  else {
    float3 X;
    float3 Y;
    if (alpha_x == alpha_y) {
      make_orthonormals(N, &X, &Y);
    }
    else {
      make_orthonormals_tangent(N, bsdf->T, &X, &Y);
    }

    /* Importance sampling with distribution of visible normals. Vectors are transformed to local
     * space before and after sampling. */
    local_I = make_float3(dot(X, wi), dot(Y, wi), cos_NI);
    if (m_type == MicrofacetType::GGX) {
      local_H = microfacet_ggx_sample_vndf(local_I, alpha_x, alpha_y, make_float2(rand));
    }
    else {
      /* m_type == MicrofacetType::BECKMANN */
      local_H = microfacet_beckmann_sample_vndf(local_I, alpha_x, alpha_y, make_float2(rand));
    }

    H = to_global(local_H, X, Y, N);
  }
  const float cos_HI = dot(H, wi);

  /* The angle between the half vector and the refracted ray. Not used when sampling reflection. */
  float cos_HO;
  /* Compute Fresnel coefficients. */
  Spectrum reflectance;
  Spectrum transmittance;
  microfacet_fresnel(kg, bsdf, cos_HI, &cos_HO, &reflectance, &transmittance);

  if (is_zero(reflectance) && is_zero(transmittance)) {
    return LABEL_NONE;
  }

  /* Decide between refraction and reflection based on the energy. */
  const float pdf_reflect = average(reflectance) / average(reflectance + transmittance);
  const bool do_refract = (rand.z >= pdf_reflect);

  /* Compute actual reflected or refracted direction. */
  *wo = do_refract ? refract_angle(wi, H, cos_HO, m_inv_eta) : 2.0f * cos_HI * H - wi;

  /* Ensure that the sampled direction lies in the correct hemisphere.
   * Note that the check against Ng is only performed in the sampling code, not the evaluation.
   * This is technically inconsistent, but required in order to avoid shadow terminator artifacts
   * on smooth geometry (which we'd get if we checked Ng in evaluation) while ensuring that
   * sampling doesn't return supposed reflection rays going into the geometry and vice versa.
   * The same is done for other closures as well. */
  const float cos_NO = dot(N, *wo);
  const float cos_NgO = dot(Ng, *wo);
  if ((cos_NgO < 0) != do_refract || (cos_NO < 0) != do_refract) {
    return LABEL_NONE;
  }

  if (do_refract) {
    *eval = transmittance;
    *pdf = 1.0f - pdf_reflect;
    /* If the IOR is close enough to 1.0, just treat the interaction as specular. */
    m_singular = m_singular || (fabsf(m_eta - 1.0f) < 1e-4f);
  }
  else {
    *eval = reflectance;
    *pdf = pdf_reflect;
  }

  if (m_singular) {
    /* Some high number for MIS. */
    *pdf *= 1e6f;
    *eval *= 1e6f;
  }
  else {
    float D;
    float lambdaI;
    float lambdaO;

    /* TODO: add support for anisotropic transmission. */
    if (alpha_x == alpha_y || do_refract) { /* Isotropic. */
      const float alpha2 = alpha_x * alpha_y;
      const float cos_NH = local_H.z;
      const float cos_NO = dot(N, *wo);

      D = bsdf_D<m_type>(alpha2, cos_NH);
      lambdaO = bsdf_lambda<m_type>(alpha2, cos_NO);
      lambdaI = bsdf_lambda<m_type>(alpha2, cos_NI);
    }
    else { /* Anisotropic. */
      const float3 local_O = 2.0f * cos_HI * local_H - local_I;

      D = bsdf_aniso_D<m_type>(alpha_x, alpha_y, local_H);

      lambdaO = bsdf_aniso_lambda<m_type>(alpha_x, alpha_y, local_O);
      lambdaI = bsdf_aniso_lambda<m_type>(alpha_x, alpha_y, local_I);
    }

    const float common = D / cos_NI *
                         (do_refract ? fabsf(cos_HI * cos_HO) / sqr(cos_HO + cos_HI * m_inv_eta) :
                                       0.25f);

    *pdf *= common / (1.0f + lambdaI);
    *eval *= common / (1.0f + lambdaI + lambdaO);
  }

  *sampled_roughness = make_float2(alpha_x, alpha_y);
  *eta = do_refract ? m_eta : 1.0f;

  return (do_refract ? LABEL_TRANSMIT : LABEL_REFLECT) |
         (m_singular ? LABEL_SINGULAR : LABEL_GLOSSY);
}

/* Fresnel term setup functions. These get called after the distribution-specific setup functions
 * like bsdf_microfacet_ggx_setup. */

ccl_device void bsdf_microfacet_setup_fresnel_conductor(KernelGlobals kg,
                                                        ccl_private MicrofacetBsdf *bsdf,
                                                        const ccl_private ShaderData *sd,
                                                        ccl_private FresnelConductor *fresnel,
                                                        const bool preserve_energy)
{
  bsdf->fresnel_type = MicrofacetFresnel::CONDUCTOR;
  bsdf->fresnel = fresnel;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_albedo(kg, sd, bsdf, true, true));

  if (preserve_energy) {
    microfacet_ggx_preserve_energy(kg, bsdf, sd, fresnel_conductor_Fss(fresnel->ior));
  }
}

ccl_device void bsdf_microfacet_setup_fresnel_dielectric_tint(
    KernelGlobals kg,
    ccl_private MicrofacetBsdf *bsdf,
    const ccl_private ShaderData *sd,
    ccl_private FresnelDielectricTint *fresnel,
    const bool preserve_energy)
{
  bsdf->fresnel_type = MicrofacetFresnel::DIELECTRIC_TINT;
  bsdf->fresnel = fresnel;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_albedo(kg, sd, bsdf, true, true));

  if (preserve_energy) {
    /* Assume that the transmissive tint makes up most of the overall color. */
    Spectrum Fss = fresnel->transmission_tint;
    if (is_zero(fresnel->transmission_tint)) {
      /* For purely reflective closures, use the reflection component. */
      Fss = fresnel_dielectric_Fss(bsdf->ior) * fresnel->reflection_tint;
    }
    microfacet_ggx_preserve_energy(kg, bsdf, sd, Fss);
  }
}

ccl_device void bsdf_microfacet_setup_fresnel_generalized_schlick(
    KernelGlobals kg,
    ccl_private MicrofacetBsdf *bsdf,
    const ccl_private ShaderData *sd,
    ccl_private FresnelGeneralizedSchlick *fresnel,
    const bool preserve_energy)
{
  fresnel->f0 = saturate(fresnel->f0);
  bsdf->fresnel_type = MicrofacetFresnel::GENERALIZED_SCHLICK;
  bsdf->fresnel = fresnel;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_albedo(kg, sd, bsdf, true, true));

  if (preserve_energy) {
    Spectrum Fss = one_spectrum();
    /* Multi-bounce Fresnel is only supported for reflective lobes here. */
    if (is_zero(fresnel->transmission_tint)) {
      float s;
      if (fresnel->exponent < 0.0f) {
        const float F0 = F0_from_ior(bsdf->ior);
        const float Fss = fresnel_dielectric_Fss(bsdf->ior);
        s = saturatef(inverse_lerp(F0, 1.0f, Fss));
      }
      else {
        /* Integral of 2*cosI * (1 - cosI)^exponent over 0...1. */
        s = 2.0f / ((fresnel->exponent + 3.0f) * fresnel->exponent + 2.0f);
      }
      /* Due to the linearity of the generalized model, this ends up working. */
      Fss = fresnel->reflection_tint * mix(fresnel->f0, fresnel->f90, s);
    }
    else {
      /* For transmissive BSDFs, assume that the transmissive tint makes up most of the overall
       * color. */
      Fss = fresnel->transmission_tint;
    }

    microfacet_ggx_preserve_energy(kg, bsdf, sd, Fss);
  }
}

ccl_device void bsdf_microfacet_setup_fresnel_f82_tint(KernelGlobals kg,
                                                       ccl_private MicrofacetBsdf *bsdf,
                                                       const ccl_private ShaderData *sd,
                                                       ccl_private FresnelF82Tint *fresnel,
                                                       const Spectrum f82_tint,
                                                       const bool preserve_energy)
{
  if (isequal(f82_tint, one_spectrum())) {
    fresnel->b = zero_spectrum();
  }
  else {
    fresnel->b = fresnel_f82tint_B(fresnel->f0, f82_tint);
  }

  bsdf->fresnel_type = MicrofacetFresnel::F82_TINT;
  bsdf->fresnel = fresnel;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_albedo(kg, sd, bsdf, true, true));

  if (preserve_energy) {
    microfacet_ggx_preserve_energy(kg, bsdf, sd, fresnel_f82_Fss(fresnel->f0, fresnel->b));
  }
}

ccl_device void bsdf_microfacet_setup_fresnel_constant(KernelGlobals kg,
                                                       ccl_private MicrofacetBsdf *bsdf,
                                                       const ccl_private ShaderData *sd,
                                                       const Spectrum color)
{
  /* Constant Fresnel is a special case - the color is already baked into the closure's
   * weight, so we just need to perform the energy preservation. */
  kernel_assert(bsdf->fresnel_type == MicrofacetFresnel::NONE ||
                bsdf->fresnel_type == MicrofacetFresnel::DIELECTRIC);

  microfacet_ggx_preserve_energy(kg, bsdf, sd, color);
}

ccl_device void bsdf_microfacet_setup_fresnel_dielectric(KernelGlobals kg,
                                                         ccl_private MicrofacetBsdf *bsdf,
                                                         const ccl_private ShaderData *sd)
{
  bsdf->fresnel_type = MicrofacetFresnel::DIELECTRIC;
  bsdf->sample_weight *= average(bsdf_microfacet_estimate_albedo(kg, sd, bsdf, true, true));

  const float Fss = fresnel_dielectric_Fss(bsdf->ior);
  microfacet_ggx_preserve_energy(kg, bsdf, sd, make_spectrum(Fss));
}

/* GGX microfacet with Smith shadow-masking from:
 *
 * Microfacet Models for Refraction through Rough Surfaces
 * B. Walter, S. R. Marschner, H. Li, K. E. Torrance, EGSR 2007
 *
 * Anisotropic from:
 *
 * Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs
 * E. Heitz, Research Report 2014
 *
 * Anisotropy is only supported for reflection currently, but adding it for
 * transmission is just a matter of copying code from reflection if needed. */

ccl_device int bsdf_microfacet_ggx_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = saturatef(bsdf->alpha_y);

  bsdf->fresnel_type = MicrofacetFresnel::NONE;
  bsdf->energy_scale = 1.0f;
  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_ID;

  return SD_BSDF | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device int bsdf_microfacet_ggx_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->fresnel_type = MicrofacetFresnel::NONE;
  bsdf->energy_scale = 1.0f;
  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;

  return SD_BSDF | SD_BSDF_HAS_TRANSMISSION | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device int bsdf_microfacet_ggx_glass_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->fresnel_type = MicrofacetFresnel::DIELECTRIC;
  bsdf->energy_scale = 1.0f;
  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID;

  return SD_BSDF | SD_BSDF_HAS_TRANSMISSION | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device void bsdf_microfacet_blur(ccl_private ShaderClosure *sc, const float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

ccl_device Spectrum bsdf_microfacet_ggx_eval(KernelGlobals kg,
                                             const ccl_private ShaderClosure *sc,
                                             const float3 wi,
                                             const float3 wo,
                                             ccl_private float *pdf)
{
  const ccl_private MicrofacetBsdf *bsdf = (const ccl_private MicrofacetBsdf *)sc;
  return bsdf->energy_scale * bsdf_microfacet_eval<MicrofacetType::GGX>(kg, sc, wi, wo, pdf);
}

ccl_device int bsdf_microfacet_ggx_sample(KernelGlobals kg,
                                          const ccl_private ShaderClosure *sc,
                                          const float3 Ng,
                                          const float3 wi,
                                          const float3 rand,
                                          ccl_private Spectrum *eval,
                                          ccl_private float3 *wo,
                                          ccl_private float *pdf,
                                          ccl_private float2 *sampled_roughness,
                                          ccl_private float *eta)
{

  const int label = bsdf_microfacet_sample<MicrofacetType::GGX>(
      kg, sc, Ng, wi, rand, eval, wo, pdf, sampled_roughness, eta);
  *eval *= ((const ccl_private MicrofacetBsdf *)sc)->energy_scale;
  return label;
}

/* Beckmann microfacet with Smith shadow-masking from:
 *
 * Microfacet Models for Refraction through Rough Surfaces
 * B. Walter, S. R. Marschner, H. Li, K. E. Torrance, EGSR 2007 */

ccl_device int bsdf_microfacet_beckmann_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = saturatef(bsdf->alpha_y);

  bsdf->fresnel_type = MicrofacetFresnel::NONE;
  bsdf->type = CLOSURE_BSDF_MICROFACET_BECKMANN_ID;

  return SD_BSDF | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device int bsdf_microfacet_beckmann_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->fresnel_type = MicrofacetFresnel::NONE;
  bsdf->type = CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;

  return SD_BSDF | SD_BSDF_HAS_TRANSMISSION | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device int bsdf_microfacet_beckmann_glass_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->fresnel_type = MicrofacetFresnel::DIELECTRIC;
  bsdf->type = CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID;

  return SD_BSDF | SD_BSDF_HAS_TRANSMISSION | bsdf_microfacet_eval_flag(bsdf);
}

ccl_device Spectrum bsdf_microfacet_beckmann_eval(KernelGlobals kg,
                                                  const ccl_private ShaderClosure *sc,
                                                  const float3 wi,
                                                  const float3 wo,
                                                  ccl_private float *pdf)
{
  return bsdf_microfacet_eval<MicrofacetType::BECKMANN>(kg, sc, wi, wo, pdf);
}

ccl_device int bsdf_microfacet_beckmann_sample(KernelGlobals kg,
                                               const ccl_private ShaderClosure *sc,
                                               const float3 Ng,
                                               const float3 wi,
                                               const float3 rand,
                                               ccl_private Spectrum *eval,
                                               ccl_private float3 *wo,
                                               ccl_private float *pdf,
                                               ccl_private float2 *sampled_roughness,
                                               ccl_private float *eta)
{
  return bsdf_microfacet_sample<MicrofacetType::BECKMANN>(
      kg, sc, Ng, wi, rand, eval, wo, pdf, sampled_roughness, eta);
}

CCL_NAMESPACE_END
