/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0

 * This code implements the paper [A Microfacet-based Hair Scattering
 * Model](https://onlinelibrary.wiley.com/doi/full/10.1111/cgf.14588) by Weizhen Huang, Matthias B.
 * Hullin and Johannes Hanika. */

#pragma once

#include "kernel/closure/bsdf_util.h"
#include "kernel/sample/lcg.h"
#include "kernel/util/color.h"

CCL_NAMESPACE_BEGIN

typedef struct HuangHairExtra {
  /* Optional modulation factors. */
  float R, TT, TRT;

  /* Local coordinate system. X is stored as `bsdf->N`.*/
  float3 Y, Z;

  /* Incident direction in local coordinate system. */
  float3 wi;

  /* Projected radius from the view direction. */
  float radius;

  /* Squared Eccentricity. */
  float e2;

  /* Valid integration interval. */
  float gamma_m_min, gamma_m_max;
} HuangHairExtra;

typedef struct HuangHairBSDF {
  SHADER_CLOSURE_BASE;

  /* Absorption coefficient. */
  Spectrum sigma;

  /* Microfacet distribution roughness. */
  float roughness;

  /* Cuticle tilt angle. */
  float tilt;

  /* Index of refraction. */
  float eta;

  /* The ratio of the minor axis to the major axis. */
  float aspect_ratio;

  /* Azimuthal offset. */
  float h;

  /* Extra closure for optional modulation factors and local coordinate system. */
  ccl_private HuangHairExtra *extra;
} HuangHairBSDF;

static_assert(sizeof(ShaderClosure) >= sizeof(HuangHairBSDF), "HuangHairBSDF is too large!");
static_assert(sizeof(ShaderClosure) >= sizeof(HuangHairExtra), "HuangHairExtra is too large!");

/* -------------------------------------------------------------------- */
/** \name Hair coordinate system utils.
 * \{ */

/* Returns `sin(theta)` of the given direction. */
ccl_device_inline float sin_theta(const float3 w)
{
  return w.y;
}

/* Returns `cos(theta)` of the given direction. */
ccl_device_inline float cos_theta(const float3 w)
{
  return safe_sqrtf(sqr(w.x) + sqr(w.z));
}

/* Returns `tan(theta)` of the given direction. */
ccl_device_inline float tan_theta(const float3 w)
{
  return sin_theta(w) / cos_theta(w);
}

/* Returns `sin(phi)` and `cos(phi)` of the given direction. */
ccl_device float sin_phi(const float3 w)
{
  return w.x / cos_theta(w);
}

ccl_device float2 sincos_phi(const float3 w)
{
  float c = cos_theta(w);
  return make_float2(w.x / c, w.z / c);
}

/* Extract the theta coordinate from the given direction.
 * -pi < theta < pi */
ccl_device_inline float dir_theta(const float3 w)
{
  return atan2f(sin_theta(w), cos_theta(w));
}

/* Extract the phi coordinate from the given direction, assuming `phi(wi) == 0`.
 * -pi < phi < pi */
ccl_device_inline float dir_phi(const float3 w)
{
  return atan2f(w.x, w.z);
}

/* Extract theta and phi coordinates from the given direction, assuming `phi(wi) == 0`.
 * -pi/2 < theta < pi/2, -pi < phi < pi */
ccl_device_inline float2 dir_sph(const float3 w)
{
  return make_float2(dir_theta(w), dir_phi(w));
}

/* Conversion between `gamma` and `phi`. Notations see Figure 5 in the paper. */
ccl_device_inline float to_phi(float gamma, float b)
{
  if (b == 1.0f) {
    return gamma;
  }
  float sin_gamma, cos_gamma;
  fast_sincosf(gamma, &sin_gamma, &cos_gamma);
  return atan2f(b * sin_gamma, cos_gamma);
}

ccl_device_inline float to_gamma(float phi, float b)
{
  if (b == 1.0f) {
    return phi;
  }
  float sin_phi, cos_phi;
  fast_sincosf(phi, &sin_phi, &cos_phi);
  return atan2f(sin_phi, b * cos_phi);
}

/* Compute the coordinate on the ellipse, given `gamma` and the aspect ratio between the minor axis
 * and the major axis. */
ccl_device_inline float2 to_point(float gamma, float b)
{
  float sin_gamma, cos_gamma;
  fast_sincosf(gamma, &sin_gamma, &cos_gamma);
  return make_float2(sin_gamma, b * cos_gamma);
}

/* Compute the vector direction given by `theta` and `gamma`. */
ccl_device_inline float3 sphg_dir(float theta, float gamma, float b)
{
  float sin_theta, cos_theta, sin_gamma, cos_gamma, sin_phi, cos_phi;

  fast_sincosf(theta, &sin_theta, &cos_theta);
  fast_sincosf(gamma, &sin_gamma, &cos_gamma);

  if (b == 1.0f || fabsf(cos_gamma) < 1e-6f) {
    sin_phi = sin_gamma;
    cos_phi = cos_gamma;
  }
  else {
    float tan_gamma = sin_gamma / cos_gamma;
    float tan_phi = b * tan_gamma;
    cos_phi = signf(cos_gamma) * inversesqrtf(sqr(tan_phi) + 1.0f);
    sin_phi = cos_phi * tan_phi;
  }
  return make_float3(sin_phi * cos_theta, sin_theta, cos_phi * cos_theta);
}

ccl_device_inline float arc_length(float e2, float gamma)
{
  return e2 == 0 ? 1.0f : sqrtf(1.0f - e2 * sqr(sinf(gamma)));
}

/** \} */

#ifdef __HAIR__
/* Set up the hair closure. */
ccl_device int bsdf_hair_huang_setup(ccl_private ShaderData *sd,
                                     ccl_private HuangHairBSDF *bsdf,
                                     uint32_t path_flag)
{
  bsdf->type = CLOSURE_BSDF_HAIR_HUANG_ID;

  bsdf->roughness = clamp(bsdf->roughness, 0.001f, 1.0f);

  /* Negate to keep it consistent with principled hair BSDF. */
  bsdf->tilt = -bsdf->tilt;

  /* Compute local frame. The Y axis is aligned with the curve tangent; the X axis is perpendicular
   to the ray direction for circular cross-sections, or aligned with the major axis for elliptical
   cross-sections. */
  bsdf->extra->Y = safe_normalize(sd->dPdu);
  const float3 X = safe_normalize(cross(sd->dPdu, sd->wi));

  /* h from -1..0..1 means the rays goes from grazing the hair, to hitting it at the center, to
   * grazing the other edge. This is the cosine of the angle between `sd->N` and `X`. */
  bsdf->h = (sd->type & PRIMITIVE_CURVE_RIBBON) ? -sd->v : -dot(X, sd->N);

  kernel_assert(fabsf(bsdf->h) < 1.0f + 1e-4f);
  kernel_assert(isfinite_safe(bsdf->h));

  if (bsdf->aspect_ratio != 1.0f && (sd->type & PRIMITIVE_CURVE)) {
    /* Adjust `bsdf->N` to be orthogonal to `sd->dPdu`. */
    bsdf->N = safe_normalize(cross(sd->dPdu, safe_normalize(cross(bsdf->N, sd->dPdu))));
    /* Align local frame with the curve normal. */
    if (bsdf->aspect_ratio > 1.0f) {
      /* Switch major and minor axis. */
      bsdf->aspect_ratio = 1.0f / bsdf->aspect_ratio;
      const float3 minor_axis = safe_normalize(cross(sd->dPdu, bsdf->N));
      bsdf->N = safe_normalize(cross(minor_axis, sd->dPdu));
    }
  }
  else {
    /* Align local frame with the ray direction so that `phi_i == 0`. */
    bsdf->N = X;
  }

  /* Fill extra closure. */
  if (is_zero(bsdf->N) || !isfinite_safe(bsdf->N)) {
    /* Construct arbitrary local coordinate system. The implementation should ensure smooth
     * transition along the hair shaft. */
    make_orthonormals(bsdf->extra->Y, &bsdf->extra->Z, &bsdf->N);
  }
  else {
    bsdf->extra->Z = safe_normalize(cross(bsdf->N, sd->dPdu));
  }

  const float3 I = make_float3(
      dot(sd->wi, bsdf->N), dot(sd->wi, bsdf->extra->Y), dot(sd->wi, bsdf->extra->Z));
  bsdf->extra->wi = I;
  bsdf->extra->e2 = 1.0f - sqr(bsdf->aspect_ratio);
  bsdf->extra->radius = bsdf->extra->e2 == 0 ?
                            1.0f :
                            sqrtf(1.0f - bsdf->extra->e2 * sqr(I.x) / (sqr(I.x) + sqr(I.z)));

  /* Treat as transparent material if intersection lies outside of the projected radius. */
  if (fabsf(bsdf->h) >= bsdf->extra->radius) {
    /* Remove allocated closures. */
    sd->num_closure--;
    sd->num_closure_left += 2;
    /* Allocate transparent closure. */
    bsdf_transparent_setup(sd, bsdf->weight, path_flag);
    return 0;
  }

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_NEEDS_LCG | SD_BSDF_HAS_TRANSMISSION;
}

#endif /* __HAIR__ */

/* Albedo correction, treat as glass. `rough` has already applied square root. */
ccl_device_forceinline float bsdf_hair_huang_energy_scale(KernelGlobals kg,
                                                          float mu,
                                                          float rough,
                                                          float ior)
{
  const bool inv_table = (ior < 1.0f);
  const int ofs = inv_table ? kernel_data.tables.ggx_glass_inv_E : kernel_data.tables.ggx_glass_E;
  const float z = sqrtf(fabsf((ior - 1.0f) / (ior + 1.0f)));
  return 1.0f / lookup_table_read_3D(kg, rough, mu, z, ofs, 16, 16, 16);
}

/* Sample microfacets from a tilted mesonormal. */
ccl_device_inline float3 sample_wh(
    KernelGlobals kg, const float roughness, const float3 wi, const float3 wm, const float2 rand)
{
  /* Coordinate transformation for microfacet sampling. */
  float3 s, t;
  make_orthonormals(wm, &s, &t);

  const float3 wi_wm = make_float3(dot(wi, s), dot(wi, t), dot(wi, wm));

  const float3 wh_wm = microfacet_ggx_sample_vndf(wi_wm, roughness, roughness, rand);

  const float3 wh = wh_wm.x * s + wh_wm.y * t + wh_wm.z * wm;
  return wh;
}

/* Check micronormal/mesonormal direct visibility from direction `v`. */
ccl_device_inline bool microfacet_visible(const float3 v, const float3 m, const float3 h)
{
  return (dot(v, h) > 0.0f && dot(v, m) > 0.0f);
}

/* Check micronormal/mesonormal direct visibility from directions `wi` and `wo`. */
ccl_device_inline bool microfacet_visible(const float3 wi,
                                          const float3 wo,
                                          const float3 m,
                                          const float3 h)
{
  return microfacet_visible(wi, m, h) && microfacet_visible(wo, m, h);
}

/* Combined shadowing-masking term divided by the shadowing-masking in the incoming direction. */
ccl_device_inline float bsdf_Go(float alpha2, float cos_NI, float cos_NO)
{
  const float lambdaI = bsdf_lambda<MicrofacetType::GGX>(alpha2, cos_NI);
  const float lambdaO = bsdf_lambda<MicrofacetType::GGX>(alpha2, cos_NO);
  return (1.0f + lambdaI) / (1.0f + lambdaI + lambdaO);
}

ccl_device Spectrum bsdf_hair_huang_eval_r(KernelGlobals kg,
                                           ccl_private const ShaderClosure *sc,
                                           const float3 wi,
                                           const float3 wo)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  if (bsdf->extra->R <= 0.0f) {
    return zero_float3();
  }

  /* Get minor axis, assuming major axis is 1. */
  const float b = bsdf->aspect_ratio;

  const float3 wh = normalize(wi + wo);

  const float roughness = bsdf->roughness;
  const float roughness2 = sqr(roughness);

  /* Maximal sample resolution. */
  float res = roughness * 0.7f;

  const float gamma_m_range = bsdf->extra->gamma_m_max - bsdf->extra->gamma_m_min;

  /* Number of intervals should be even. */
  const size_t intervals = 2 * (size_t)ceilf(gamma_m_range / res * 0.5f);

  /* Modified resolution based on numbers of intervals. */
  res = gamma_m_range / float(intervals);

  /* Integrate using Composite Simpson's 1/3 rule. */
  float integral = 0.0f;
  for (size_t i = 0; i <= intervals; i++) {
    const float gamma_m = bsdf->extra->gamma_m_min + i * res;
    const float3 wm = sphg_dir(bsdf->tilt, gamma_m, b);

    if (microfacet_visible(wi, wo, make_float3(wm.x, 0.0f, wm.z), wh)) {
      const float weight = (i == 0 || i == intervals) ? 0.5f : (i % 2 + 1);
      const float cos_mi = dot(wm, wi);
      const float G = bsdf_G<MicrofacetType::GGX>(roughness2, cos_mi, dot(wm, wo));
      integral += weight * bsdf_D<MicrofacetType::GGX>(roughness2, dot(wm, wh)) * G *
                  arc_length(bsdf->extra->e2, gamma_m) *
                  bsdf_hair_huang_energy_scale(kg, cos_mi, sqrtf(roughness), bsdf->eta);
    }
  }

  integral *= (2.0f / 3.0f * res);

  const float F = fresnel_dielectric_cos(dot(wi, wh), bsdf->eta);

  return make_spectrum(bsdf->extra->R * 0.125f * F * integral / bsdf->extra->radius);
}

/* Approximate components beyond TRT (starting TRRT) by summing up a geometric series. Attenuations
 * are approximated from previous interactions. */
ccl_device Spectrum bsdf_hair_huang_eval_trrt(const float T, const float R, const Spectrum A)
{
  /* `T` could be zero due to total internal reflection. Clamp to avoid numerical issues. */
  const float T_avg = max(1.0f - R, 1e-5f);
  const Spectrum TRRT_avg = T * sqr(R) * T_avg * A * A * A;
  return TRRT_avg / (one_spectrum() - A * (1.0f - T_avg));
}

/* Evaluate components beyond R using numerical integration. TT and TRT are computed via combined
 * Monte Carlo-Simpson integration; components beyond TRRT are integrated via Simpson's method. */
ccl_device Spectrum bsdf_hair_huang_eval_residual(KernelGlobals kg,
                                                  ccl_private const ShaderClosure *sc,
                                                  const float3 wi,
                                                  const float3 wo,
                                                  uint rng_quadrature)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  if (bsdf->extra->TT <= 0.0f && bsdf->extra->TRT <= 0.0f) {
    return zero_spectrum();
  }

  /* Get minor axis, assuming major axis is 1. */
  const float b = bsdf->aspect_ratio;
  const bool is_circular = (b == 1.0f);

  const Spectrum mu_a = bsdf->sigma;
  const float eta = bsdf->eta;
  const float inv_eta = 1.0f / eta;

  const float roughness = bsdf->roughness;
  const float roughness2 = sqr(roughness);
  const float sqrt_roughness = sqrtf(roughness);

  float res = roughness * 0.8f;
  const float gamma_m_range = bsdf->extra->gamma_m_max - bsdf->extra->gamma_m_min;
  const size_t intervals = 2 * (size_t)ceilf(gamma_m_range / res * 0.5f);
  res = gamma_m_range / intervals;

  Spectrum S_tt = zero_spectrum();
  Spectrum S_trt = zero_spectrum();
  Spectrum S_trrt = zero_spectrum();
  for (size_t i = 0; i <= intervals; i++) {

    const float gamma_mi = bsdf->extra->gamma_m_min + i * res;

    const float3 wmi = sphg_dir(bsdf->tilt, gamma_mi, b);
    const float3 wmi_ = sphg_dir(0.0f, gamma_mi, b);

    /* Sample `wh1`. */
    const float2 sample1 = make_float2(lcg_step_float(&rng_quadrature),
                                       lcg_step_float(&rng_quadrature));

    const float3 wh1 = sample_wh(kg, roughness, wi, wmi, sample1);
    const float cos_hi1 = dot(wi, wh1);
    if (!(cos_hi1 > 0.0f)) {
      continue;
    }

    const float cos_mi1 = dot(wi, wmi);
    float cos_theta_t1;
    const float T1 = 1.0f - fresnel_dielectric(cos_hi1, eta, &cos_theta_t1);
    const float scale1 = bsdf_hair_huang_energy_scale(kg, cos_mi1, sqrt_roughness, eta);

    /* Refraction at the first interface. */
    const float3 wt = refract_angle(wi, wh1, cos_theta_t1, inv_eta);
    const float phi_t = dir_phi(wt);
    const float gamma_mt = 2.0f * to_phi(phi_t, b) - gamma_mi;
    const float3 wmt = sphg_dir(-bsdf->tilt, gamma_mt, b);
    const float3 wmt_ = sphg_dir(0.0f, gamma_mt, b);

    const float cos_mo1 = dot(-wt, wmi);
    const float cos_mi2 = dot(-wt, wmt);
    const float G1o = bsdf_Go(roughness2, cos_mi1, cos_mo1);
    if (!microfacet_visible(wi, -wt, wmi, wh1) || !microfacet_visible(wi, -wt, wmi_, wh1)) {
      continue;
    }

    const float weight = (i == 0 || i == intervals) ? 0.5f : (i % 2 + 1);

    const Spectrum A_t = exp(mu_a / cos_theta(wt) *
                             (is_circular ?
                                  2.0f * cosf(gamma_mi - phi_t) :
                                  -len(to_point(gamma_mi, b) - to_point(gamma_mt + M_PI_F, b))));

    const float scale2 = bsdf_hair_huang_energy_scale(kg, cos_mi2, sqrt_roughness, inv_eta);

    /* TT */
    if (bsdf->extra->TT > 0.0f) {
      if (dot(wo, wt) >= inv_eta - 1e-5f) { /* Total internal reflection otherwise. */
        float3 wh2 = -wt + inv_eta * wo;
        const float rcp_norm_wh2 = 1.0f / len(wh2);
        wh2 *= rcp_norm_wh2;
        const float cos_mh2 = dot(wmt, wh2);
        if (cos_mh2 >= 0.0f) { /* Microfacet visibility from macronormal. */
          const float cos_hi2 = dot(-wt, wh2);
          const float cos_ho2 = dot(-wo, wh2);
          const float cos_mo2 = dot(-wo, wmt);

          const float T2 = (1.0f - fresnel_dielectric_cos(cos_hi2, inv_eta)) * scale2;
          const float D2 = bsdf_D<MicrofacetType::GGX>(roughness2, cos_mh2);
          const float G2 = bsdf_G<MicrofacetType::GGX>(roughness2, cos_mi2, cos_mo2);

          const Spectrum result = weight * T1 * scale1 * T2 * D2 * G1o * G2 * A_t / cos_mo1 *
                                  cos_mi1 * cos_hi2 * cos_ho2 * sqr(rcp_norm_wh2);

          if (isfinite_safe(result)) {
            S_tt += bsdf->extra->TT * result * arc_length(bsdf->extra->e2, gamma_mt);
          }
        }
      }
    }

    /* TRT and beyond. */
    if (bsdf->extra->TRT > 0.0f) {
      /* Sample `wh2`. */
      const float2 sample2 = make_float2(lcg_step_float(&rng_quadrature),
                                         lcg_step_float(&rng_quadrature));
      const float3 wh2 = sample_wh(kg, roughness, -wt, wmt, sample2);
      const float cos_hi2 = dot(-wt, wh2);
      if (!(cos_hi2 > 0.0f)) {
        continue;
      }
      const float R2 = fresnel_dielectric_cos(cos_hi2, inv_eta);

      const float3 wtr = -reflect(wt, wh2);
      if (dot(-wtr, wo) < inv_eta - 1e-5f) {
        /* Total internal reflection. */
        S_trrt += weight * bsdf_hair_huang_eval_trrt(T1, R2, A_t);
        continue;
      }

      if (!microfacet_visible(-wt, -wtr, wmt, wh2) || !microfacet_visible(-wt, -wtr, wmt_, wh2)) {
        continue;
      }

      const float phi_tr = dir_phi(wtr);
      const float gamma_mtr = gamma_mi - 2.0f * (to_phi(phi_t, b) - to_phi(phi_tr, b)) + M_PI_F;
      const float3 wmtr = sphg_dir(-bsdf->tilt, gamma_mtr, b);
      const float3 wmtr_ = sphg_dir(0.0f, gamma_mtr, b);

      float3 wh3 = wtr + inv_eta * wo;
      const float rcp_norm_wh3 = 1.0f / len(wh3);
      wh3 *= rcp_norm_wh3;
      const float cos_mh3 = dot(wmtr, wh3);
      if (cos_mh3 < 0.0f || !microfacet_visible(wtr, -wo, wmtr, wh3) ||
          !microfacet_visible(wtr, -wo, wmtr_, wh3))
      {
        S_trrt += weight * bsdf_hair_huang_eval_trrt(T1, R2, A_t);
        continue;
      }

      const float cos_hi3 = dot(wh3, wtr);
      const float cos_ho3 = dot(wh3, -wo);
      const float cos_mi3 = dot(wmtr, wtr);

      const float T3 = (1.0f - fresnel_dielectric_cos(cos_hi3, inv_eta)) *
                       bsdf_hair_huang_energy_scale(kg, cos_mi3, sqrt_roughness, inv_eta);
      const float D3 = bsdf_D<MicrofacetType::GGX>(roughness2, cos_mh3);

      const Spectrum A_tr = exp(mu_a / cos_theta(wtr) *
                                -(is_circular ?
                                      2.0f * fabsf(cosf(phi_tr - gamma_mt)) :
                                      len(to_point(gamma_mtr, b) - to_point(gamma_mt, b))));

      const float cos_mo2 = dot(wmt, -wtr);
      const float G2o = bsdf_Go(roughness2, cos_mi2, cos_mo2);
      const float G3 = bsdf_G<MicrofacetType::GGX>(roughness2, cos_mi3, dot(wmtr, -wo));

      const Spectrum result = weight * T1 * scale1 * R2 * scale2 * T3 * D3 * G1o * G2o * G3 * A_t *
                              A_tr / (cos_mo1 * cos_mo2) * cos_mi1 * cos_mi2 * cos_hi3 * cos_ho3 *
                              sqr(rcp_norm_wh3);

      if (isfinite_safe(result)) {
        S_trt += bsdf->extra->TRT * result * arc_length(bsdf->extra->e2, gamma_mtr);
      }

      S_trrt += weight * bsdf_hair_huang_eval_trrt(T1, R2, A_t);
    }
  }

  /* TRRT+ terms, following the approach in [A practical and controllable hair and fur model for
   * production path tracing](https://doi.org/10.1145/2775280.2792559) by Chiang, Matt Jen-Yuan, et
   * al. */
  const float M = longitudinal_scattering(
      sin_theta(wi), cos_theta(wi), sin_theta(wo), cos_theta(wo), 4.0f * bsdf->roughness);
  const float N = M_1_2PI_F;

  return ((S_tt + S_trt) * sqr(inv_eta) / bsdf->extra->radius + S_trrt * M * N * M_2_PI_F) * res /
         3.0f;
}

ccl_device int bsdf_hair_huang_sample(const KernelGlobals kg,
                                      ccl_private const ShaderClosure *sc,
                                      ccl_private ShaderData *sd,
                                      float3 rand,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf,
                                      ccl_private float2 *sampled_roughness)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  const float roughness = bsdf->roughness;
  *sampled_roughness = make_float2(roughness, roughness);

  kernel_assert(fabsf(bsdf->h) < bsdf->extra->radius);

  /* Generate samples. */
  float sample_lobe = rand.x;
  const float sample_h = rand.y;
  const float2 sample_h1 = make_float2(rand.z, lcg_step_float(&sd->lcg_state));
  const float2 sample_h2 = make_float2(lcg_step_float(&sd->lcg_state),
                                       lcg_step_float(&sd->lcg_state));
  const float2 sample_h3 = make_float2(lcg_step_float(&sd->lcg_state),
                                       lcg_step_float(&sd->lcg_state));

  /* Get `wi` in local coordinate. */
  const float3 wi = bsdf->extra->wi;

  const float2 sincos_phi_i = sincos_phi(wi);
  const float sin_phi_i = sincos_phi_i.x;
  const float cos_phi_i = sincos_phi_i.y;

  /* Get minor axis, assuming major axis is 1. */
  const float b = bsdf->aspect_ratio;
  const bool is_circular = (b == 1.0f);

  const float h = sample_h * 2.0f - 1.0f;
  const float gamma_mi = is_circular ?
                             asinf(h) :
                             atan2f(cos_phi_i, -b * sin_phi_i) -
                                 acosf(h * bsdf->extra->radius *
                                       inversesqrtf(sqr(cos_phi_i) + sqr(b * sin_phi_i)));

  /* Macronormal. */
  const float3 wmi_ = sphg_dir(0, gamma_mi, b);

  /* Mesonormal. */
  float st, ct;
  fast_sincosf(bsdf->tilt, &st, &ct);
  const float3 wmi = make_float3(wmi_.x * ct, st, wmi_.z * ct);
  const float cos_mi1 = dot(wmi, wi);

  if (cos_mi1 < 0.0f || dot(wmi_, wi) < 0.0f) {
    /* Macro/mesonormal invisible. */
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  /* Sample R lobe. */
  const float roughness2 = sqr(roughness);
  const float sqrt_roughness = sqrtf(roughness);
  const float3 wh1 = sample_wh(kg, roughness, wi, wmi, sample_h1);
  const float3 wr = -reflect(wi, wh1);

  /* Ensure that this is a valid sample. */
  if (!microfacet_visible(wi, wmi_, wh1)) {
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  float cos_theta_t1;
  const float R1 = fresnel_dielectric(dot(wi, wh1), bsdf->eta, &cos_theta_t1);
  const float scale1 = bsdf_hair_huang_energy_scale(kg, cos_mi1, sqrt_roughness, bsdf->eta);
  const float R = bsdf->extra->R * R1 * scale1 * microfacet_visible(wr, wmi_, wh1) *
                  bsdf_Go(roughness2, cos_mi1, dot(wmi, wr));

  /* Sample TT lobe. */
  const float inv_eta = 1.0f / bsdf->eta;
  const float3 wt = refract_angle(wi, wh1, cos_theta_t1, inv_eta);
  const float phi_t = dir_phi(wt);

  const float gamma_mt = 2.0f * to_phi(phi_t, b) - gamma_mi;
  const float3 wmt = sphg_dir(-bsdf->tilt, gamma_mt, b);
  const float3 wmt_ = sphg_dir(0.0f, gamma_mt, b);

  const float3 wh2 = sample_wh(kg, roughness, -wt, wmt, sample_h2);

  const float3 wtr = -reflect(wt, wh2);

  float3 wh3, wtt, wtrt, wmtr, wtrrt;
  Spectrum TT = zero_spectrum();
  Spectrum TRT = zero_spectrum();
  Spectrum TRRT = zero_spectrum();
  const float cos_mi2 = dot(-wt, wmt);

  if (cos_mi2 > 0.0f && microfacet_visible(-wt, wmi_, wh1) && microfacet_visible(-wt, wmt_, wh2)) {
    const Spectrum mu_a = bsdf->sigma;
    const Spectrum A_t = exp(mu_a / cos_theta(wt) *
                             (is_circular ?
                                  2.0f * cosf(phi_t - gamma_mi) :
                                  -len(to_point(gamma_mi, b) - to_point(gamma_mt + M_PI_F, b))));

    float cos_theta_t2;
    const float R2 = fresnel_dielectric(dot(-wt, wh2), inv_eta, &cos_theta_t2);
    const float T1 = (1.0f - R1) * scale1 * bsdf_Go(roughness2, cos_mi1, dot(wmi, -wt));
    const float T2 = 1.0f - R2;
    const float scale2 = bsdf_hair_huang_energy_scale(kg, cos_mi2, sqrt_roughness, inv_eta);

    wtt = refract_angle(-wt, wh2, cos_theta_t2, bsdf->eta);

    if (dot(wmt, -wtt) > 0.0f && T2 > 0.0f && microfacet_visible(-wtt, wmt_, wh2)) {
      TT = bsdf->extra->TT * T1 * A_t * T2 * scale2 * bsdf_Go(roughness2, cos_mi2, dot(wmt, -wtt));
    }

    /* Sample TRT lobe. */
    const float phi_tr = dir_phi(wtr);
    const float gamma_mtr = gamma_mi - 2.0f * (to_phi(phi_t, b) - to_phi(phi_tr, b)) + M_PI_F;
    wmtr = sphg_dir(-bsdf->tilt, gamma_mtr, b);

    wh3 = sample_wh(kg, roughness, wtr, wmtr, sample_h3);

    float cos_theta_t3;
    const float R3 = fresnel_dielectric(dot(wtr, wh3), inv_eta, &cos_theta_t3);

    wtrt = refract_angle(wtr, wh3, cos_theta_t3, bsdf->eta);

    const float cos_mi3 = dot(wmtr, wtr);
    if (cos_mi3 > 0.0f) {
      const Spectrum A_tr = exp(mu_a / cos_theta(wtr) *
                                -(is_circular ?
                                      2.0f * fabsf(cosf(phi_tr - gamma_mt)) :
                                      len(to_point(gamma_mt, b) - to_point(gamma_mtr, b))));

      const Spectrum TR = T1 * R2 * scale2 * A_t * A_tr *
                          bsdf_hair_huang_energy_scale(kg, cos_mi3, sqrt_roughness, inv_eta) *
                          bsdf_Go(roughness2, cos_mi2, dot(wmt, -wtr));

      const float T3 = 1.0f - R3;

      if (T3 > 0.0f && microfacet_visible(wtr, -wtrt, make_float3(wmtr.x, 0.0f, wmtr.z), wh3)) {
        TRT = bsdf->extra->TRT * TR * make_spectrum(T3) *
              bsdf_Go(roughness2, cos_mi3, dot(wmtr, -wtrt));
      }

      /* Sample TRRT+ terms, following the approach in [A practical and controllable hair and fur
       * model for production path tracing](https://doi.org/10.1145/2775280.2792559) by Chiang,
       * Matt Jen-Yuan, et al. */

      /* Sample `theta_o`. */
      const float rand_theta = max(lcg_step_float(&sd->lcg_state), 1e-5f);
      const float fac = 1.0f +
                        4.0f * bsdf->roughness *
                            logf(rand_theta + (1.0f - rand_theta) * expf(-0.5f / bsdf->roughness));
      const float sin_theta_o = -fac * sin_theta(wi) +
                                cos_from_sin(fac) *
                                    cosf(M_2PI_F * lcg_step_float(&sd->lcg_state)) * cos_theta(wi);
      const float cos_theta_o = cos_from_sin(sin_theta_o);

      /* Sample `phi_o`. */
      const float phi_o = M_2PI_F * lcg_step_float(&sd->lcg_state);
      float sin_phi_o, cos_phi_o;
      fast_sincosf(phi_o, &sin_phi_o, &cos_phi_o);

      /* Compute outgoing direction. */
      wtrrt = make_float3(sin_phi_o * cos_theta_o, sin_theta_o, cos_phi_o * cos_theta_o);

      /* Compute residual term by summing up the geometric series `A * T + A^2 * R * T + ...`.
       * Attenuations are approximated from previous interactions. */
      const Spectrum A_avg = sqrt(A_t * A_tr);
      /* `T` could be zero due to total internal reflection. Clamp to avoid numerical issues. */
      const float T_avg = max(0.5f * (T2 + T3), 1e-5f);
      const Spectrum A_res = A_avg * T_avg / (one_spectrum() - A_avg * (1.0f - T_avg));

      TRRT = TR * R3 * A_res * bsdf_Go(roughness2, cos_mi3, dot(wmtr, -reflect(wtr, wh3)));
    }
  }

  /* Select lobe based on energy. */
  const float r = R;
  const float tt = average(TT);
  const float trt = average(TRT);
  const float trrt = average(TRRT);
  const float total_energy = r + tt + trt + trrt;

  if (total_energy == 0.0f) {
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  float3 local_O;

  sample_lobe *= total_energy;
  if (sample_lobe < r) {
    local_O = wr;
    *eval = make_spectrum(total_energy);
  }
  else if (sample_lobe < (r + tt)) {
    local_O = wtt;
    *eval = TT / tt * total_energy;
  }
  else if (sample_lobe < (r + tt + trt)) {
    local_O = wtrt;
    *eval = TRT / trt * total_energy;
  }
  else {
    local_O = wtrrt;
    *eval = TRRT / trrt * make_spectrum(total_energy);
  }

  /* Get local coordinate system. */
  const float3 X = bsdf->N;
  const float3 Y = bsdf->extra->Y;
  const float3 Z = bsdf->extra->Z;

  /* Transform `wo` to global coordinate system. */
  *wo = local_O.x * X + local_O.y * Y + local_O.z * Z;

  /* Ensure the same pdf is returned for BSDF and emitter sampling. The importance sampling pdf is
   * already factored in the value so this value is only used for MIS. */
  *pdf = 1.0f;

  return LABEL_GLOSSY | LABEL_REFLECT;
}

ccl_device Spectrum bsdf_hair_huang_eval(KernelGlobals kg,
                                         ccl_private const ShaderData *sd,
                                         ccl_private const ShaderClosure *sc,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  kernel_assert(fabsf(bsdf->h) < bsdf->extra->radius);

  /* Get local coordinate system. */
  const float3 X = bsdf->N;
  const float3 Y = bsdf->extra->Y;
  const float3 Z = bsdf->extra->Z;

  /* Transform `wi`/`wo` from global coordinate system to local. */
  const float3 local_I = bsdf->extra->wi;
  const float3 local_O = make_float3(dot(wo, X), dot(wo, Y), dot(wo, Z));

  /* TODO: better estimation of the pdf */
  *pdf = 1.0f;

  /* Early detection of `dot(wo, wmo) < 0`. */
  const float tan_tilt = tanf(bsdf->tilt);
  if (tan_tilt * tan_theta(local_O) < -1.0f) {
    return zero_spectrum();
  }

  /* Compute visible azimuthal range from the incoming direction. */
  const float half_span = acosf(fmaxf(-tan_tilt * tan_theta(local_I), 0.0f));
  if (isnan_safe(half_span)) {
    /* Early detection of `dot(wi, wmi) < 0`. */
    return zero_spectrum();
  }
  const float b = bsdf->aspect_ratio;
  const float phi_i = (b == 1.0f) ? 0.0f : dir_phi(local_I);
  const float gamma_m_min = to_gamma(phi_i - half_span, b);
  float gamma_m_max = to_gamma(phi_i + half_span, b);
  if (gamma_m_max < gamma_m_min) {
    gamma_m_max += M_2PI_F;
  }

  bsdf->extra->gamma_m_min = gamma_m_min + 1e-3f;
  bsdf->extra->gamma_m_max = gamma_m_max - 1e-3f;

  return (bsdf_hair_huang_eval_r(kg, sc, local_I, local_O) +
          bsdf_hair_huang_eval_residual(kg, sc, local_I, local_O, sd->lcg_state)) /
         cos_theta(local_I);
}

/* Implements Filter Glossy by capping the effective roughness. */
ccl_device void bsdf_hair_huang_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  bsdf->roughness = fmaxf(roughness, bsdf->roughness);
}

/* Hair Albedo. Computed by summing up geometric series, assuming circular cross-section and
 * specular reflection. */
ccl_device Spectrum bsdf_hair_huang_albedo(ccl_private const ShaderData *sd,
                                           ccl_private const ShaderClosure *sc)
{
  ccl_private HuangHairBSDF *bsdf = (ccl_private HuangHairBSDF *)sc;

  const float3 wmi = make_float3(bsdf->h, 0.0f, cos_from_sin(bsdf->h));
  float cos_t;
  const float f = fresnel_dielectric(dot(wmi, bsdf->extra->wi), bsdf->eta, &cos_t);
  const float3 wt = refract_angle(bsdf->extra->wi, wmi, cos_t, 1.0f / bsdf->eta);
  const Spectrum A = exp(2.0f * bsdf->sigma * cos_t / (1.0f - sqr(wt.y)));

  return safe_divide(A - 2.0f * f * A + f, one_spectrum() - f * A);
}

CCL_NAMESPACE_END
