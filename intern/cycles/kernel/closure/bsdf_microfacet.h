/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

#include "kernel/closure/bsdf_util.h"

#include "kernel/sample/pattern.h"

#include "kernel/util/lookup_table.h"

CCL_NAMESPACE_BEGIN

typedef struct MicrofacetExtra {
  Spectrum color, cspec0;
  Spectrum fresnel_color;
  float clearcoat;
} MicrofacetExtra;

typedef struct MicrofacetBsdf {
  SHADER_CLOSURE_BASE;

  float alpha_x, alpha_y, ior;
  ccl_private MicrofacetExtra *extra;
  float3 T;
} MicrofacetBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(MicrofacetBsdf), "MicrofacetBsdf is too large!");

/* Beckmann and GGX microfacet importance sampling. */

ccl_device_inline void microfacet_beckmann_sample_slopes(KernelGlobals kg,
                                                         const float cos_theta_i,
                                                         const float sin_theta_i,
                                                         float randu,
                                                         float randv,
                                                         ccl_private float *slope_x,
                                                         ccl_private float *slope_y,
                                                         ccl_private float *G1i)
{
  /* special case (normal incidence) */
  if (cos_theta_i >= 0.99999f) {
    const float r = sqrtf(-logf(randu));
    const float phi = M_2PI_F * randv;
    *slope_x = r * cosf(phi);
    *slope_y = r * sinf(phi);
    *G1i = 1.0f;
    return;
  }

  /* precomputations */
  const float tan_theta_i = sin_theta_i / cos_theta_i;
  const float inv_a = tan_theta_i;
  const float cot_theta_i = 1.0f / tan_theta_i;
  const float erf_a = fast_erff(cot_theta_i);
  const float exp_a2 = expf(-cot_theta_i * cot_theta_i);
  const float SQRT_PI_INV = 0.56418958354f;
  const float Lambda = 0.5f * (erf_a - 1.0f) + (0.5f * SQRT_PI_INV) * (exp_a2 * inv_a);
  const float G1 = 1.0f / (1.0f + Lambda); /* masking */

  *G1i = G1;

#if defined(__KERNEL_GPU__)
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
   *   exp(-ierf(x)^2) ~= 1 - x * x
   *   solve y = 1 + b + K * (1 - b * b)
   */
  float K = tan_theta_i * SQRT_PI_INV;
  float y_approx = randu * (1.0f + erf_a + K * (1 - erf_a * erf_a));
  float y_exact = randu * (1.0f + erf_a + K * exp_a2);
  float b = K > 0 ? (0.5f - sqrtf(K * (K - y_approx + 1.0f) + 0.25f)) / K : y_approx - 1.0f;

  /* Perform newton step to refine toward the true root. */
  float inv_erf = fast_ierff(b);
  float value = 1.0f + b + K * expf(-inv_erf * inv_erf) - y_exact;
  /* Check if we are close enough already,
   * this also avoids NaNs as we get close to the root.
   */
  if (fabsf(value) > 1e-6f) {
    b -= value / (1.0f - inv_erf * tan_theta_i); /* newton step 1. */
    inv_erf = fast_ierff(b);
    value = 1.0f + b + K * expf(-inv_erf * inv_erf) - y_exact;
    b -= value / (1.0f - inv_erf * tan_theta_i); /* newton step 2. */
    /* Compute the slope from the refined value. */
    *slope_x = fast_ierff(b);
  }
  else {
    /* We are close enough already. */
    *slope_x = inv_erf;
  }
  *slope_y = fast_ierff(2.0f * randv - 1.0f);
#else
  /* Use precomputed table on CPU, it gives better performance. */
  int beckmann_table_offset = kernel_data.tables.beckmann_offset;

  *slope_x = lookup_table_read_2D(
      kg, randu, cos_theta_i, beckmann_table_offset, BECKMANN_TABLE_SIZE, BECKMANN_TABLE_SIZE);
  *slope_y = fast_ierff(2.0f * randv - 1.0f);
#endif
}

/* GGX microfacet importance sampling from:
 *
 * Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals.
 * E. Heitz and E. d'Eon, EGSR 2014
 */

ccl_device_inline void microfacet_ggx_sample_slopes(const float cos_theta_i,
                                                    const float sin_theta_i,
                                                    float randu,
                                                    float randv,
                                                    ccl_private float *slope_x,
                                                    ccl_private float *slope_y,
                                                    ccl_private float *G1i)
{
  /* special case (normal incidence) */
  if (cos_theta_i >= 0.99999f) {
    const float r = sqrtf(randu / (1.0f - randu));
    const float phi = M_2PI_F * randv;
    *slope_x = r * cosf(phi);
    *slope_y = r * sinf(phi);
    *G1i = 1.0f;

    return;
  }

  /* precomputations */
  const float tan_theta_i = sin_theta_i / cos_theta_i;
  const float G1_inv = 0.5f * (1.0f + safe_sqrtf(1.0f + tan_theta_i * tan_theta_i));

  *G1i = 1.0f / G1_inv;

  /* sample slope_x */
  const float A = 2.0f * randu * G1_inv - 1.0f;
  const float AA = A * A;
  const float tmp = 1.0f / (AA - 1.0f);
  const float B = tan_theta_i;
  const float BB = B * B;
  const float D = safe_sqrtf(BB * (tmp * tmp) - (AA - BB) * tmp);
  const float slope_x_1 = B * tmp - D;
  const float slope_x_2 = B * tmp + D;
  *slope_x = (A < 0.0f || slope_x_2 * tan_theta_i > 1.0f) ? slope_x_1 : slope_x_2;

  /* sample slope_y */
  float S;

  if (randv > 0.5f) {
    S = 1.0f;
    randv = 2.0f * (randv - 0.5f);
  }
  else {
    S = -1.0f;
    randv = 2.0f * (0.5f - randv);
  }

  const float z = (randv * (randv * (randv * 0.27385f - 0.73369f) + 0.46341f)) /
                  (randv * (randv * (randv * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
  *slope_y = S * z * safe_sqrtf(1.0f + (*slope_x) * (*slope_x));
}

ccl_device_forceinline float3 microfacet_sample_stretched(KernelGlobals kg,
                                                          const float3 wi,
                                                          const float alpha_x,
                                                          const float alpha_y,
                                                          const float randu,
                                                          const float randv,
                                                          bool beckmann,
                                                          ccl_private float *G1i)
{
  /* 1. stretch wi */
  float3 wi_ = make_float3(alpha_x * wi.x, alpha_y * wi.y, wi.z);
  wi_ = normalize(wi_);

  /* get polar coordinates of wi_ */
  float costheta_ = 1.0f;
  float sintheta_ = 0.0f;
  float cosphi_ = 1.0f;
  float sinphi_ = 0.0f;

  if (wi_.z < 0.99999f) {
    costheta_ = wi_.z;
    sintheta_ = safe_sqrtf(1.0f - costheta_ * costheta_);

    float invlen = 1.0f / sintheta_;
    cosphi_ = wi_.x * invlen;
    sinphi_ = wi_.y * invlen;
  }

  /* 2. sample P22_{wi}(x_slope, y_slope, 1, 1) */
  float slope_x, slope_y;

  if (beckmann) {
    microfacet_beckmann_sample_slopes(
        kg, costheta_, sintheta_, randu, randv, &slope_x, &slope_y, G1i);
  }
  else {
    microfacet_ggx_sample_slopes(costheta_, sintheta_, randu, randv, &slope_x, &slope_y, G1i);
  }

  /* 3. rotate */
  float tmp = cosphi_ * slope_x - sinphi_ * slope_y;
  slope_y = sinphi_ * slope_x + cosphi_ * slope_y;
  slope_x = tmp;

  /* 4. unstretch */
  slope_x = alpha_x * slope_x;
  slope_y = alpha_y * slope_y;

  /* 5. compute normal */
  return normalize(make_float3(-slope_x, -slope_y, 1.0f));
}

/* Calculate the reflection color
 *
 * If fresnel is used, the color is an interpolation of the F0 color and white
 * with respect to the fresnel
 *
 * Else it is simply white
 */
ccl_device_forceinline Spectrum reflection_color(ccl_private const MicrofacetBsdf *bsdf,
                                                 float3 L,
                                                 float3 H)
{
  Spectrum F = one_spectrum();
  bool use_fresnel = (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID ||
                      bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID);
  if (use_fresnel) {
    float F0 = fresnel_dielectric_cos(1.0f, bsdf->ior);

    F = interpolate_fresnel_color(L, H, bsdf->ior, F0, bsdf->extra->cspec0);
  }

  return F;
}

ccl_device_forceinline float D_GTR1(float NdotH, float alpha)
{
  if (alpha >= 1.0f)
    return M_1_PI_F;
  float alpha2 = alpha * alpha;
  float t = 1.0f + (alpha2 - 1.0f) * NdotH * NdotH;
  return (alpha2 - 1.0f) / (M_PI_F * logf(alpha2) * t);
}

ccl_device_forceinline void bsdf_microfacet_fresnel_color(ccl_private const ShaderData *sd,
                                                          ccl_private MicrofacetBsdf *bsdf)
{
  kernel_assert(CLOSURE_IS_BSDF_MICROFACET_FRESNEL(bsdf->type));

  float F0 = fresnel_dielectric_cos(1.0f, bsdf->ior);
  bsdf->extra->fresnel_color = interpolate_fresnel_color(
      sd->wi, bsdf->N, bsdf->ior, F0, bsdf->extra->cspec0);

  if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
    bsdf->extra->fresnel_color *= 0.25f * bsdf->extra->clearcoat;
  }

  bsdf->sample_weight *= average(bsdf->extra->fresnel_color);
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
  bsdf->extra = NULL;

  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = saturatef(bsdf->alpha_y);

  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_ID;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

/* Required to maintain OSL interface. */
ccl_device int bsdf_microfacet_ggx_isotropic_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_y = bsdf->alpha_x;

  return bsdf_microfacet_ggx_setup(bsdf);
}

ccl_device int bsdf_microfacet_ggx_fresnel_setup(ccl_private MicrofacetBsdf *bsdf,
                                                 ccl_private const ShaderData *sd)
{
  bsdf->extra->cspec0 = saturate(bsdf->extra->cspec0);

  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = saturatef(bsdf->alpha_y);

  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID;

  bsdf_microfacet_fresnel_color(sd, bsdf);

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device int bsdf_microfacet_ggx_clearcoat_setup(ccl_private MicrofacetBsdf *bsdf,
                                                   ccl_private const ShaderData *sd)
{
  bsdf->extra->cspec0 = saturate(bsdf->extra->cspec0);

  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID;

  bsdf_microfacet_fresnel_color(sd, bsdf);

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device int bsdf_microfacet_ggx_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->extra = NULL;

  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->type = CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;

  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_HAS_TRANSMISSION;
}

ccl_device void bsdf_microfacet_ggx_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

ccl_device Spectrum bsdf_microfacet_ggx_eval_reflect(ccl_private const MicrofacetBsdf *bsdf,
                                                     const float3 N,
                                                     const float3 wi,
                                                     const float3 wo,
                                                     ccl_private float *pdf,
                                                     const float alpha_x,
                                                     const float alpha_y,
                                                     const float cosNI,
                                                     const float cosNO)
{
  if (!(cosNI > 0 && cosNO > 0)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  /* get half vector */
  float3 m = normalize(wi + wo);
  float alpha2 = alpha_x * alpha_y;
  float D, G1i, G1o;

  if (alpha_x == alpha_y) {
    /* isotropic
     * eq. 20: (F*G*D)/(4*in*on)
     * eq. 33: first we calculate D(m) */
    float cosThetaM = dot(N, m);
    float cosThetaM2 = cosThetaM * cosThetaM;
    float cosThetaM4 = cosThetaM2 * cosThetaM2;
    float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;

    if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
      /* use GTR1 for clearcoat */
      D = D_GTR1(cosThetaM, bsdf->alpha_x);

      /* the alpha value for clearcoat is a fixed 0.25 => alpha2 = 0.25 * 0.25 */
      alpha2 = 0.0625f;
    }
    else {
      /* use GTR2 otherwise */
      D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
    }

    /* eq. 34: now calculate G1(i,m) and G1(o,m) */
    G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
    G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
  }
  else {
    /* anisotropic */
    float3 X, Y, Z = N;
    make_orthonormals_tangent(Z, bsdf->T, &X, &Y);

    /* distribution */
    float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
    float slope_x = -local_m.x / (local_m.z * alpha_x);
    float slope_y = -local_m.y / (local_m.z * alpha_y);
    float slope_len = 1 + slope_x * slope_x + slope_y * slope_y;

    float cosThetaM = local_m.z;
    float cosThetaM2 = cosThetaM * cosThetaM;
    float cosThetaM4 = cosThetaM2 * cosThetaM2;

    D = 1 / ((slope_len * slope_len) * M_PI_F * alpha2 * cosThetaM4);

    /* G1(i,m) and G1(o,m) */
    float tanThetaI2 = (1 - cosNI * cosNI) / (cosNI * cosNI);
    float cosPhiI = dot(wi, X);
    float sinPhiI = dot(wi, Y);

    float alphaI2 = (cosPhiI * cosPhiI) * (alpha_x * alpha_x) +
                    (sinPhiI * sinPhiI) * (alpha_y * alpha_y);
    alphaI2 /= cosPhiI * cosPhiI + sinPhiI * sinPhiI;

    G1i = 2 / (1 + safe_sqrtf(1 + alphaI2 * tanThetaI2));

    float tanThetaO2 = (1 - cosNO * cosNO) / (cosNO * cosNO);
    float cosPhiO = dot(wo, X);
    float sinPhiO = dot(wo, Y);

    float alphaO2 = (cosPhiO * cosPhiO) * (alpha_x * alpha_x) +
                    (sinPhiO * sinPhiO) * (alpha_y * alpha_y);
    alphaO2 /= cosPhiO * cosPhiO + sinPhiO * sinPhiO;

    G1o = 2 / (1 + safe_sqrtf(1 + alphaO2 * tanThetaO2));
  }

  float G = G1i * G1o;

  /* eq. 20 */
  float common = D * 0.25f / cosNI;

  Spectrum F = reflection_color(bsdf, wo, m);
  if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
    F *= 0.25f * bsdf->extra->clearcoat;
  }

  Spectrum out = F * G * common;

  /* eq. 2 in distribution of visible normals sampling
   * `pm = Dw = G1i * dot(m, I) * D / dot(N, I);` */

  /* eq. 38 - but see also:
   * eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
   * `pdf = pm * 0.25 / dot(m, I);` */
  *pdf = G1i * common;

  return out;
}

ccl_device Spectrum bsdf_microfacet_ggx_eval_transmit(ccl_private const MicrofacetBsdf *bsdf,
                                                      const float3 N,
                                                      const float3 wi,
                                                      const float3 wo,
                                                      ccl_private float *pdf,
                                                      const float alpha_x,
                                                      const float alpha_y,
                                                      const float cosNI,
                                                      const float cosNO)
{
  if (cosNI <= 0 || cosNO >= 0) {
    *pdf = 0.0f;
    return zero_spectrum(); /* vectors on same side -- not possible */
  }
  /* compute half-vector of the refraction (eq. 16) */
  float m_eta = bsdf->ior;
  float3 ht = -(m_eta * wo + wi);
  float3 Ht = normalize(ht);
  float cosHI = dot(Ht, wi);
  float cosHO = dot(Ht, wo);

  float D, G1i, G1o;

  /* eq. 33: first we calculate D(m) with m=Ht: */
  float alpha2 = alpha_x * alpha_y;
  float cosThetaM = dot(N, Ht);
  float cosThetaM2 = cosThetaM * cosThetaM;
  float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
  float cosThetaM4 = cosThetaM2 * cosThetaM2;
  D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

  /* eq. 34: now calculate G1(i,m) and G1(o,m) */
  G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
  G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));

  float G = G1i * G1o;

  /* probability */
  float Ht2 = dot(ht, ht);

  /* eq. 2 in distribution of visible normals sampling
   * pm = Dw = G1i * dot(m, I) * D / dot(N, I); */

  /* out = fabsf(cosHI * cosHO) * (m_eta * m_eta) * G * D / (cosNI * Ht2)
   * pdf = pm * (m_eta * m_eta) * fabsf(cosHO) / Ht2 */
  float common = D * (m_eta * m_eta) / (cosNI * Ht2);
  float out = G * fabsf(cosHI * cosHO) * common;
  *pdf = G1i * fabsf(cosHI * cosHO) * common;

  return make_spectrum(out);
}

ccl_device Spectrum bsdf_microfacet_ggx_eval(ccl_private const ShaderClosure *sc,
                                             const float3 Ng,
                                             const float3 wi,
                                             const float3 wo,
                                             ccl_private float *pdf)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  const bool m_refractive = bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;
  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;
  const float cosNgO = dot(Ng, wo);

  if (((cosNgO < 0.0f) != m_refractive) || alpha_x * alpha_y <= 1e-7f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float3 N = bsdf->N;
  const float cosNI = dot(N, wi);
  const float cosNO = dot(N, wo);

  return (cosNgO < 0.0f) ? bsdf_microfacet_ggx_eval_transmit(
                               bsdf, N, wi, wo, pdf, alpha_x, alpha_y, cosNI, cosNO) :
                           bsdf_microfacet_ggx_eval_reflect(
                               bsdf, N, wi, wo, pdf, alpha_x, alpha_y, cosNI, cosNO);
}

ccl_device int bsdf_microfacet_ggx_sample(KernelGlobals kg,
                                          ccl_private const ShaderClosure *sc,
                                          float3 Ng,
                                          float3 wi,
                                          float randu,
                                          float randv,
                                          ccl_private Spectrum *eval,
                                          ccl_private float3 *wo,
                                          ccl_private float *pdf,
                                          ccl_private float2 *sampled_roughness,
                                          ccl_private float *eta)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  float alpha_x = bsdf->alpha_x;
  float alpha_y = bsdf->alpha_y;
  bool m_refractive = bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID;

  *sampled_roughness = make_float2(alpha_x, alpha_y);
  *eta = m_refractive ? 1.0f / bsdf->ior : bsdf->ior;

  float3 N = bsdf->N;
  int label;

  float cosNI = dot(N, wi);
  if (cosNI > 0) {
    float3 X, Y, Z = N;

    if (alpha_x == alpha_y)
      make_orthonormals(Z, &X, &Y);
    else
      make_orthonormals_tangent(Z, bsdf->T, &X, &Y);

    /* importance sampling with distribution of visible normals. vectors are
     * transformed to local space before and after */
    float3 local_I = make_float3(dot(X, wi), dot(Y, wi), cosNI);
    float3 local_m;
    float G1i;

    local_m = microfacet_sample_stretched(
        kg, local_I, alpha_x, alpha_y, randu, randv, false, &G1i);

    float3 m = X * local_m.x + Y * local_m.y + Z * local_m.z;
    float cosThetaM = local_m.z;

    /* reflection or refraction? */
    if (!m_refractive) {
      float cosMI = dot(m, wi);
      label = LABEL_REFLECT | LABEL_GLOSSY;

      if (cosMI > 0) {
        /* eq. 39 - compute actual reflected direction */
        *wo = 2 * cosMI * m - wi;

        if (dot(Ng, *wo) > 0) {
          if (alpha_x * alpha_y <= 1e-7f) {
            /* some high number for MIS */
            *pdf = 1e6f;
            *eval = make_spectrum(1e6f);

            bool use_fresnel = (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID ||
                                bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID);

            /* if fresnel is used, calculate the color with reflection_color(...) */
            if (use_fresnel) {
              *eval *= reflection_color(bsdf, *wo, m);
            }

            label = LABEL_REFLECT | LABEL_SINGULAR;
          }
          else {
            /* microfacet normal is visible to this ray */
            /* eq. 33 */
            float alpha2 = alpha_x * alpha_y;
            float D, G1o;

            if (alpha_x == alpha_y) {
              /* isotropic */
              float cosThetaM2 = cosThetaM * cosThetaM;
              float cosThetaM4 = cosThetaM2 * cosThetaM2;
              float tanThetaM2 = 1 / (cosThetaM2)-1;

              /* eval BRDF*cosNO */
              float cosNO = dot(N, *wo);

              if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
                /* use GTR1 for clearcoat */
                D = D_GTR1(cosThetaM, bsdf->alpha_x);

                /* the alpha value for clearcoat is a fixed 0.25 => alpha2 = 0.25 * 0.25 */
                alpha2 = 0.0625f;

                /* recalculate G1i */
                G1i = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNI * cosNI) / (cosNI * cosNI)));
              }
              else {
                /* use GTR2 otherwise */
                D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));
              }

              /* eq. 34: now calculate G1(o,m) */
              G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));
            }
            else {
              /* anisotropic distribution */
              float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
              float slope_x = -local_m.x / (local_m.z * alpha_x);
              float slope_y = -local_m.y / (local_m.z * alpha_y);
              float slope_len = 1 + slope_x * slope_x + slope_y * slope_y;

              float cosThetaM = local_m.z;
              float cosThetaM2 = cosThetaM * cosThetaM;
              float cosThetaM4 = cosThetaM2 * cosThetaM2;

              D = 1 / ((slope_len * slope_len) * M_PI_F * alpha2 * cosThetaM4);

              /* calculate G1(o,m) */
              float cosNO = dot(N, *wo);

              float tanThetaO2 = (1 - cosNO * cosNO) / (cosNO * cosNO);
              float cosPhiO = dot(*wo, X);
              float sinPhiO = dot(*wo, Y);

              float alphaO2 = (cosPhiO * cosPhiO) * (alpha_x * alpha_x) +
                              (sinPhiO * sinPhiO) * (alpha_y * alpha_y);
              alphaO2 /= cosPhiO * cosPhiO + sinPhiO * sinPhiO;

              G1o = 2 / (1 + safe_sqrtf(1 + alphaO2 * tanThetaO2));
            }

            /* see eval function for derivation */
            float common = (G1i * D) * 0.25f / cosNI;
            *pdf = common;

            Spectrum F = reflection_color(bsdf, *wo, m);

            *eval = G1o * common * F;
          }

          if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
            *eval *= 0.25f * bsdf->extra->clearcoat;
          }
        }
        else {
          *eval = zero_spectrum();
          *pdf = 0.0f;
        }
      }
    }
    else {
      label = LABEL_TRANSMIT | LABEL_GLOSSY;

      float3 R, T;
      float m_eta = bsdf->ior, fresnel;
      bool inside;

      fresnel = fresnel_dielectric(m_eta, m, wi, &R, &T, &inside);

      if (!inside && fresnel != 1.0f) {
        *wo = T;

        if (alpha_x * alpha_y <= 1e-7f || fabsf(m_eta - 1.0f) < 1e-4f) {
          /* some high number for MIS */
          *pdf = 1e6f;
          *eval = make_spectrum(1e6f);
          label = LABEL_TRANSMIT | LABEL_SINGULAR;
        }
        else {
          /* eq. 33 */
          float alpha2 = alpha_x * alpha_y;
          float cosThetaM2 = cosThetaM * cosThetaM;
          float cosThetaM4 = cosThetaM2 * cosThetaM2;
          float tanThetaM2 = 1 / (cosThetaM2)-1;
          float D = alpha2 / (M_PI_F * cosThetaM4 * (alpha2 + tanThetaM2) * (alpha2 + tanThetaM2));

          /* eval BRDF*cosNO */
          float cosNO = dot(N, *wo);

          /* eq. 34: now calculate G1(o,m) */
          float G1o = 2 / (1 + safe_sqrtf(1 + alpha2 * (1 - cosNO * cosNO) / (cosNO * cosNO)));

          /* eq. 21 */
          float cosHO = dot(m, *wo);
          float cosHI = dot(m, wi);
          float Ht2 = m_eta * cosHO + cosHI;
          Ht2 *= Ht2;

          /* see eval function for derivation */
          float common = (G1i * D) * (m_eta * m_eta) / (cosNO * Ht2);
          float out = G1o * fabsf(cosHI * cosHO) * common;
          *pdf = cosHI * fabsf(cosHO) * common;

          *eval = make_spectrum(out);
        }
      }
      else {
        *eval = zero_spectrum();
        *pdf = 0.0f;
      }
    }
  }
  else {
    label = (m_refractive) ? LABEL_TRANSMIT | LABEL_GLOSSY : LABEL_REFLECT | LABEL_GLOSSY;
  }
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

  bsdf->type = CLOSURE_BSDF_MICROFACET_BECKMANN_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

/* Required to maintain OSL interface. */
ccl_device int bsdf_microfacet_beckmann_isotropic_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_y = bsdf->alpha_x;

  return bsdf_microfacet_beckmann_setup(bsdf);
}

ccl_device int bsdf_microfacet_beckmann_refraction_setup(ccl_private MicrofacetBsdf *bsdf)
{
  bsdf->alpha_x = saturatef(bsdf->alpha_x);
  bsdf->alpha_y = bsdf->alpha_x;

  bsdf->type = CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_HAS_TRANSMISSION;
}

ccl_device void bsdf_microfacet_beckmann_blur(ccl_private ShaderClosure *sc, float roughness)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)sc;

  bsdf->alpha_x = fmaxf(roughness, bsdf->alpha_x);
  bsdf->alpha_y = fmaxf(roughness, bsdf->alpha_y);
}

ccl_device_inline float bsdf_beckmann_G1(float alpha, float cos_n)
{
  cos_n *= cos_n;
  float invA = alpha * safe_sqrtf((1.0f - cos_n) / cos_n);
  if (invA < 0.625f) {
    return 1.0f;
  }

  float a = 1.0f / invA;
  return ((2.181f * a + 3.535f) * a) / ((2.577f * a + 2.276f) * a + 1.0f);
}

ccl_device_inline float bsdf_beckmann_aniso_G1(
    float alpha_x, float alpha_y, float cos_n, float cos_phi, float sin_phi)
{
  cos_n *= cos_n;
  sin_phi *= sin_phi;
  cos_phi *= cos_phi;
  alpha_x *= alpha_x;
  alpha_y *= alpha_y;

  float alphaO2 = (cos_phi * alpha_x + sin_phi * alpha_y) / (cos_phi + sin_phi);
  float invA = safe_sqrtf(alphaO2 * (1 - cos_n) / cos_n);
  if (invA < 0.625f) {
    return 1.0f;
  }

  float a = 1.0f / invA;
  return ((2.181f * a + 3.535f) * a) / ((2.577f * a + 2.276f) * a + 1.0f);
}

ccl_device Spectrum bsdf_microfacet_beckmann_eval_reflect(ccl_private const MicrofacetBsdf *bsdf,
                                                          const float3 N,
                                                          const float3 wi,
                                                          const float3 wo,
                                                          ccl_private float *pdf,
                                                          const float alpha_x,
                                                          const float alpha_y,
                                                          const float cosNI,
                                                          const float cosNO)
{
  if (!(cosNI > 0 && cosNO > 0)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  /* get half vector */
  float3 m = normalize(wi + wo);

  float alpha2 = alpha_x * alpha_y;
  float D, G1i, G1o;

  if (alpha_x == alpha_y) {
    /* isotropic
     * eq. 20: (F*G*D)/(4*in*on)
     * eq. 25: first we calculate D(m) */
    float cosThetaM = dot(N, m);
    float cosThetaM2 = cosThetaM * cosThetaM;
    float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
    float cosThetaM4 = cosThetaM2 * cosThetaM2;
    D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 * cosThetaM4);

    /* eq. 26, 27: now calculate G1(i,m) and G1(o,m) */
    G1i = bsdf_beckmann_G1(alpha_x, cosNI);
    G1o = bsdf_beckmann_G1(alpha_x, cosNO);
  }
  else {
    /* anisotropic */
    float3 X, Y, Z = N;
    make_orthonormals_tangent(Z, bsdf->T, &X, &Y);

    /* distribution */
    float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
    float slope_x = -local_m.x / (local_m.z * alpha_x);
    float slope_y = -local_m.y / (local_m.z * alpha_y);

    float cosThetaM = local_m.z;
    float cosThetaM2 = cosThetaM * cosThetaM;
    float cosThetaM4 = cosThetaM2 * cosThetaM2;

    D = expf(-slope_x * slope_x - slope_y * slope_y) / (M_PI_F * alpha2 * cosThetaM4);

    /* G1(i,m) and G1(o,m) */
    G1i = bsdf_beckmann_aniso_G1(alpha_x, alpha_y, cosNI, dot(wi, X), dot(wi, Y));
    G1o = bsdf_beckmann_aniso_G1(alpha_x, alpha_y, cosNO, dot(wo, X), dot(wo, Y));
  }

  float G = G1i * G1o;

  /* eq. 20 */
  float common = D * 0.25f / cosNI;
  float out = G * common;

  /* eq. 2 in distribution of visible normals sampling
   * pm = Dw = G1i * dot(m, I) * D / dot(N, I); */

  /* eq. 38 - but see also:
   * eq. 17 in http://www.graphics.cornell.edu/~bjw/wardnotes.pdf
   * pdf = pm * 0.25 / dot(m, I); */
  *pdf = G1i * common;

  return make_spectrum(out);
}

ccl_device Spectrum bsdf_microfacet_beckmann_eval_transmit(ccl_private const MicrofacetBsdf *bsdf,
                                                           const float3 N,
                                                           const float3 wi,
                                                           const float3 wo,
                                                           ccl_private float *pdf,
                                                           const float alpha_x,
                                                           const float alpha_y,
                                                           const float cosNI,
                                                           const float cosNO)
{
  if (cosNI <= 0 || cosNO >= 0) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float m_eta = bsdf->ior;
  /* compute half-vector of the refraction (eq. 16) */
  float3 ht = -(m_eta * wo + wi);
  float3 Ht = normalize(ht);
  float cosHI = dot(Ht, wi);
  float cosHO = dot(Ht, wo);

  /* eq. 25: first we calculate D(m) with m=Ht: */
  float alpha2 = alpha_x * alpha_y;
  float cosThetaM = min(dot(N, Ht), 1.0f);
  float cosThetaM2 = cosThetaM * cosThetaM;
  float tanThetaM2 = (1 - cosThetaM2) / cosThetaM2;
  float cosThetaM4 = cosThetaM2 * cosThetaM2;
  float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 * cosThetaM4);

  /* eq. 26, 27: now calculate G1(i,m) and G1(o,m) */
  float G1i = bsdf_beckmann_G1(alpha_x, cosNI);
  float G1o = bsdf_beckmann_G1(alpha_x, cosNO);
  float G = G1i * G1o;

  /* probability */
  float Ht2 = dot(ht, ht);

  /* eq. 2 in distribution of visible normals sampling
   * pm = Dw = G1i * dot(m, I) * D / dot(N, I); */

  /* out = fabsf(cosHI * cosHO) * (m_eta * m_eta) * G * D / (cosNI * Ht2)
   * pdf = pm * (m_eta * m_eta) * fabsf(cosHO) / Ht2 */
  float common = D * (m_eta * m_eta) / (cosNI * Ht2);
  float out = G * fabsf(cosHI * cosHO) * common;
  *pdf = G1i * fabsf(cosHI * cosHO) * common;

  return make_spectrum(out);
}

ccl_device Spectrum bsdf_microfacet_beckmann_eval(ccl_private const ShaderClosure *sc,
                                                  const float3 Ng,
                                                  const float3 wi,
                                                  const float3 wo,
                                                  ccl_private float *pdf)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  const bool m_refractive = bsdf->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;
  const float cosNgO = dot(Ng, wo);

  if (((cosNgO < 0.0f) != m_refractive) || alpha_x * alpha_y <= 1e-7f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float3 N = bsdf->N;
  const float cosNI = dot(N, wi);
  const float cosNO = dot(N, wo);

  return (cosNO < 0.0f) ? bsdf_microfacet_beckmann_eval_transmit(
                              bsdf, N, wi, wo, pdf, alpha_x, alpha_y, cosNI, cosNO) :
                          bsdf_microfacet_beckmann_eval_reflect(
                              bsdf, N, wi, wo, pdf, alpha_x, alpha_y, cosNI, cosNO);
}

ccl_device int bsdf_microfacet_beckmann_sample(KernelGlobals kg,
                                               ccl_private const ShaderClosure *sc,
                                               float3 Ng,
                                               float3 wi,
                                               float randu,
                                               float randv,
                                               ccl_private Spectrum *eval,
                                               ccl_private float3 *wo,
                                               ccl_private float *pdf,
                                               ccl_private float2 *sampled_roughness,
                                               ccl_private float *eta)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  float alpha_x = bsdf->alpha_x;
  float alpha_y = bsdf->alpha_y;
  bool m_refractive = bsdf->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID;
  float3 N = bsdf->N;
  int label;

  *sampled_roughness = make_float2(alpha_x, alpha_y);
  *eta = m_refractive ? 1.0f / bsdf->ior : bsdf->ior;

  float cosNI = dot(N, wi);
  if (cosNI > 0) {
    float3 X, Y, Z = N;

    if (alpha_x == alpha_y)
      make_orthonormals(Z, &X, &Y);
    else
      make_orthonormals_tangent(Z, bsdf->T, &X, &Y);

    /* importance sampling with distribution of visible normals. vectors are
     * transformed to local space before and after */
    float3 local_I = make_float3(dot(X, wi), dot(Y, wi), cosNI);
    float3 local_m;
    float G1i;

    local_m = microfacet_sample_stretched(kg, local_I, alpha_x, alpha_x, randu, randv, true, &G1i);

    float3 m = X * local_m.x + Y * local_m.y + Z * local_m.z;
    float cosThetaM = local_m.z;

    /* reflection or refraction? */
    if (!m_refractive) {
      label = LABEL_REFLECT | LABEL_GLOSSY;
      float cosMI = dot(m, wi);

      if (cosMI > 0) {
        /* eq. 39 - compute actual reflected direction */
        *wo = 2 * cosMI * m - wi;

        if (dot(Ng, *wo) > 0) {
          if (alpha_x * alpha_y <= 1e-7f) {
            /* some high number for MIS */
            *pdf = 1e6f;
            *eval = make_spectrum(1e6f);
            label = LABEL_REFLECT | LABEL_SINGULAR;
          }
          else {
            /* microfacet normal is visible to this ray
             * eq. 25 */
            float alpha2 = alpha_x * alpha_y;
            float D, G1o;

            if (alpha_x == alpha_y) {
              /* Isotropic distribution. */
              float cosThetaM2 = cosThetaM * cosThetaM;
              float cosThetaM4 = cosThetaM2 * cosThetaM2;
              float tanThetaM2 = 1 / (cosThetaM2)-1;
              D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 * cosThetaM4);

              /* eval BRDF*cosNO */
              float cosNO = dot(N, *wo);

              /* eq. 26, 27: now calculate G1(o,m) */
              G1o = bsdf_beckmann_G1(alpha_x, cosNO);
            }
            else {
              /* anisotropic distribution */
              float3 local_m = make_float3(dot(X, m), dot(Y, m), dot(Z, m));
              float slope_x = -local_m.x / (local_m.z * alpha_x);
              float slope_y = -local_m.y / (local_m.z * alpha_y);

              float cosThetaM = local_m.z;
              float cosThetaM2 = cosThetaM * cosThetaM;
              float cosThetaM4 = cosThetaM2 * cosThetaM2;

              D = expf(-slope_x * slope_x - slope_y * slope_y) / (M_PI_F * alpha2 * cosThetaM4);

              /* G1(o,m) */
              G1o = bsdf_beckmann_aniso_G1(
                  alpha_x, alpha_y, dot(*wo, N), dot(*wo, X), dot(*wo, Y));
            }

            float G = G1i * G1o;

            /* see eval function for derivation */
            float common = D * 0.25f / cosNI;
            float out = G * common;
            *pdf = G1i * common;

            *eval = make_spectrum(out);
          }
        }
        else {
          *eval = zero_spectrum();
          *pdf = 0.0f;
        }
      }
    }
    else {
      label = LABEL_TRANSMIT | LABEL_GLOSSY;

      float3 R, T;
      float m_eta = bsdf->ior, fresnel;
      bool inside;

      fresnel = fresnel_dielectric(m_eta, m, wi, &R, &T, &inside);

      if (!inside && fresnel != 1.0f) {
        *wo = T;

        if (alpha_x * alpha_y <= 1e-7f || fabsf(m_eta - 1.0f) < 1e-4f) {
          /* some high number for MIS */
          *pdf = 1e6f;
          *eval = make_spectrum(1e6f);
          label = LABEL_TRANSMIT | LABEL_SINGULAR;
        }
        else {
          /* eq. 33 */
          float alpha2 = alpha_x * alpha_y;
          float cosThetaM2 = cosThetaM * cosThetaM;
          float cosThetaM4 = cosThetaM2 * cosThetaM2;
          float tanThetaM2 = 1 / (cosThetaM2)-1;
          float D = expf(-tanThetaM2 / alpha2) / (M_PI_F * alpha2 * cosThetaM4);

          /* eval BRDF*cosNO */
          float cosNO = dot(N, *wo);

          /* eq. 26, 27: now calculate G1(o,m) */
          float G1o = bsdf_beckmann_G1(alpha_x, cosNO);
          float G = G1i * G1o;

          /* eq. 21 */
          float cosHI = dot(m, wi);
          float cosHO = dot(m, *wo);
          float Ht2 = m_eta * cosHO + cosHI;
          Ht2 *= Ht2;

          /* see eval function for derivation */
          float common = D * (m_eta * m_eta) / (cosNI * Ht2);
          float out = G * fabsf(cosHO * cosHI) * common;
          *pdf = G1i * cosHI * fabsf(cosHO) * common;

          *eval = make_spectrum(out);
        }
      }
      else {
        *eval = zero_spectrum();
        *pdf = 0.0f;
      }
    }
  }
  else {
    label = (m_refractive) ? LABEL_TRANSMIT | LABEL_GLOSSY : LABEL_REFLECT | LABEL_GLOSSY;
  }
  return label;
}

CCL_NAMESPACE_END
