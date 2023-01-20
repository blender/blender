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

enum MicrofacetType {
  BECKMANN,
  GGX,
};

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
  /* Special case (normal incidence). */
  if (cos_theta_i >= 0.99999f) {
    const float r = sqrtf(-logf(randu));
    const float phi = M_2PI_F * randv;
    *slope_x = r * cosf(phi);
    *slope_y = r * sinf(phi);
    *G1i = 1.0f;

    return;
  }

  /* Precomputations. */
  const float tan_theta_i = sin_theta_i / cos_theta_i;
  const float inv_a = tan_theta_i;
  const float cot_theta_i = 1.0f / tan_theta_i;
  const float erf_a = fast_erff(cot_theta_i);
  const float exp_a2 = expf(-cot_theta_i * cot_theta_i);
  const float SQRT_PI_INV = 0.56418958354f;
  const float Lambda = 0.5f * (erf_a - 1.0f) + (0.5f * SQRT_PI_INV) * (exp_a2 * inv_a);
  const float G1 = 1.0f / (1.0f + Lambda); /* masking */

  *G1i = G1;

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
  const float K = tan_theta_i * SQRT_PI_INV;
  const float y_approx = randu * (1.0f + erf_a + K * (1 - erf_a * erf_a));
  const float y_exact = randu * (1.0f + erf_a + K * exp_a2);
  float b = K > 0 ? (0.5f - sqrtf(K * (K - y_approx + 1.0f) + 0.25f)) / K : y_approx - 1.0f;

  float inv_erf = fast_ierff(b);
  float2 begin = make_float2(-1.0f, -y_exact);
  float2 end = make_float2(erf_a, 1.0f + erf_a + K * exp_a2 - y_exact);
  float2 current = make_float2(b, 1.0f + b + K * expf(-sqr(inv_erf)) - y_exact);

  /* Find root in a monotonic interval using newton method, under given precision and maximal
   * iterations. Falls back to bisection if newton step produces results outside of the valid
   * interval.*/
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

  *slope_x = inv_erf;
  *slope_y = fast_ierff(2.0f * randv - 1.0f);
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
  /* Special case (normal incidence). */
  if (cos_theta_i >= 0.99999f) {
    const float r = sqrtf(randu / (1.0f - randu));
    const float phi = M_2PI_F * randv;
    *slope_x = r * cosf(phi);
    *slope_y = r * sinf(phi);
    *G1i = 1.0f;

    return;
  }

  /* Precomputations. */
  const float tan_theta_i = sin_theta_i / cos_theta_i;
  const float G1_inv = 0.5f * (1.0f + safe_sqrtf(1.0f + tan_theta_i * tan_theta_i));

  *G1i = 1.0f / G1_inv;

  /* Sample slope_x. */
  const float A = 2.0f * randu * G1_inv - 1.0f;
  const float AA = A * A;
  const float tmp = 1.0f / (AA - 1.0f);
  const float B = tan_theta_i;
  const float BB = B * B;
  const float D = safe_sqrtf(BB * (tmp * tmp) - (AA - BB) * tmp);
  const float slope_x_1 = B * tmp - D;
  const float slope_x_2 = B * tmp + D;
  *slope_x = (A < 0.0f || slope_x_2 * tan_theta_i > 1.0f) ? slope_x_1 : slope_x_2;

  /* Sample slope_y. */
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

template<MicrofacetType m_type>
ccl_device_forceinline float3 microfacet_sample_stretched(KernelGlobals kg,
                                                          const float3 wi,
                                                          const float alpha_x,
                                                          const float alpha_y,
                                                          const float randu,
                                                          const float randv,
                                                          ccl_private float *G1i)
{
  /* 1. stretch wi */
  float3 wi_ = make_float3(alpha_x * wi.x, alpha_y * wi.y, wi.z);
  wi_ = normalize(wi_);

  /* Compute polar coordinates of wi_. */
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

  if (m_type == MicrofacetType::BECKMANN) {
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

  bool use_clearcoat = bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID;
  bool use_fresnel = (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID || use_clearcoat);

  if (use_fresnel) {
    float F0 = fresnel_dielectric_cos(1.0f, bsdf->ior);

    F = interpolate_fresnel_color(L, H, bsdf->ior, F0, bsdf->extra->cspec0);
  }

  if (use_clearcoat) {
    F *= 0.25f * bsdf->extra->clearcoat;
  }

  return F;
}

/* Generalized Trowbridge-Reitz for clearcoat. */
ccl_device_forceinline float bsdf_clearcoat_D(float alpha2, float cos_NH)
{
  if (alpha2 >= 1.0f) {
    return M_1_PI_F;
  }

  const float t = 1.0f + (alpha2 - 1.0f) * cos_NH * cos_NH;
  return (alpha2 - 1.0f) / (M_PI_F * logf(alpha2) * t);
}

/* Monodirectional shadowing-masking term. */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_G1_from_sqr_alpha_tan_n(float sqr_alpha_tan_n)
{
  if (m_type == MicrofacetType::GGX) {
    return 2.0f / (1.0f + sqrtf(1.0f + sqr_alpha_tan_n));
  }
  else {
    /* m_type == MicrofacetType::BECKMANN */
    const float a = inversesqrtf(sqr_alpha_tan_n);
    return (a > 1.6f) ? 1.0f : ((2.181f * a + 3.535f) * a) / ((2.577f * a + 2.276f) * a + 1.0f);
  }
}

template<MicrofacetType m_type> ccl_device_inline float bsdf_G1(float alpha2, float cos_N)
{
  return bsdf_G1_from_sqr_alpha_tan_n<m_type>(alpha2 * fmaxf(1.0f / (cos_N * cos_N) - 1.0f, 0.0f));
}

template<MicrofacetType m_type>
ccl_device_inline float bsdf_aniso_G1(float alpha_x, float alpha_y, float3 V)
{
  return bsdf_G1_from_sqr_alpha_tan_n<m_type>((sqr(alpha_x * V.x) + sqr(alpha_y * V.y)) /
                                              sqr(V.z));
}

/* Smith's separable shadowing-masking term. */
template<MicrofacetType m_type>
ccl_device_inline float bsdf_G(float alpha2, float cos_NI, float cos_NO)
{
  return bsdf_G1<m_type>(alpha2, cos_NI) * bsdf_G1<m_type>(alpha2, cos_NO);
}

/* Normal distribution function. */
template<MicrofacetType m_type> ccl_device_inline float bsdf_D(float alpha2, float cos_NH)
{
  const float cos_NH2 = sqr(cos_NH);

  if (m_type == MicrofacetType::BECKMANN) {
    return expf((1.0f - 1.0f / cos_NH2) / alpha2) / (M_PI_F * alpha2 * sqr(cos_NH2));
  }
  else {
    /* m_type == MicrofacetType::GGX */
    return alpha2 / (M_PI_F * sqr(1.0f + (alpha2 - 1.0f) * cos_NH2));
  }
}

template<MicrofacetType m_type>
ccl_device_inline float bsdf_aniso_D(float alpha_x, float alpha_y, float3 H)
{
  H /= make_float3(alpha_x, alpha_y, 1.0f);

  const float cos_NH2 = sqr(H.z);
  const float alpha2 = alpha_x * alpha_y;

  if (m_type == MicrofacetType::BECKMANN) {
    return expf(-(sqr(H.x) + sqr(H.y)) / cos_NH2) / (M_PI_F * alpha2 * sqr(cos_NH2));
  }
  else {
    /* m_type == MicrofacetType::GGX */
    return M_1_PI_F / (alpha2 * sqr(len_squared(H)));
  }
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

template<MicrofacetType m_type>
ccl_device Spectrum bsdf_microfacet_eval(ccl_private const ShaderClosure *sc,
                                         const float3 Ng,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  ccl_private const MicrofacetBsdf *bsdf = (ccl_private const MicrofacetBsdf *)sc;
  const bool m_refractive = (bsdf->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID) ||
                            (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID);

  const float3 N = bsdf->N;
  const float cos_NI = dot(N, wi);
  const float cos_NO = dot(N, wo);
  const float cos_NgO = dot(Ng, wo);

  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;

  if ((cos_NI <= 0) || ((cos_NgO < 0.0f) != m_refractive) || ((cos_NO < 0.0f) != m_refractive) ||
      (alpha_x * alpha_y <= 1e-7f)) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  /* Compute half vector. */
  float3 H = m_refractive ? -(bsdf->ior * wo + wi) : (wi + wo);
  const float inv_len_H = 1.0f / len(H);
  H *= inv_len_H;

  const float cos_NH = dot(N, H);
  float D, G1i, G1o;

  /* TODO: add support for anisotropic transmission. */
  if (alpha_x == alpha_y || m_refractive) { /* Isotropic. */
    float alpha2 = alpha_x * alpha_y;

    if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
      D = bsdf_clearcoat_D(alpha2, cos_NH);

      /* The masking-shadowing term for clearcoat has a fixed alpha of 0.25
       * => alpha2 = 0.25 * 0.25 */
      alpha2 = 0.0625f;
    }
    else {
      D = bsdf_D<m_type>(alpha2, cos_NH);
    }

    G1i = bsdf_G1<m_type>(alpha2, cos_NI);
    G1o = bsdf_G1<m_type>(alpha2, cos_NO);
  }
  else { /* Anisotropic. */
    float3 X, Y;
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);

    const float3 local_H = make_float3(dot(X, H), dot(Y, H), cos_NH);
    const float3 local_I = make_float3(dot(X, wi), dot(Y, wi), cos_NI);
    const float3 local_O = make_float3(dot(X, wo), dot(Y, wo), cos_NO);

    D = bsdf_aniso_D<m_type>(alpha_x, alpha_y, local_H);

    G1i = bsdf_aniso_G1<m_type>(alpha_x, alpha_y, local_I);
    G1o = bsdf_aniso_G1<m_type>(alpha_x, alpha_y, local_O);
  }

  const float common = G1i * D / cos_NI *
                       (m_refractive ?
                            sqr(bsdf->ior * inv_len_H) * fabsf(dot(H, wi) * dot(H, wo)) :
                            0.25f);

  *pdf = common;

  const Spectrum F = m_refractive ? one_spectrum() : reflection_color(bsdf, wo, H);

  return F * G1o * common;
}

template<MicrofacetType m_type>
ccl_device int bsdf_microfacet_sample(KernelGlobals kg,
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

  const float m_eta = bsdf->ior;
  const bool m_refractive = (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID) ||
                            (bsdf->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID);
  int label = m_refractive ? LABEL_TRANSMIT : LABEL_REFLECT;

  const float3 N = bsdf->N;
  const float cos_NI = dot(N, wi);
  if (cos_NI <= 0) {
    return label | LABEL_GLOSSY;
  }

  float3 X, Y;
  const float alpha_x = bsdf->alpha_x;
  const float alpha_y = bsdf->alpha_y;
  if (alpha_x == alpha_y) {
    make_orthonormals(N, &X, &Y);
  }
  else {
    make_orthonormals_tangent(N, bsdf->T, &X, &Y);
  }

  /* Importance sampling with distribution of visible normals. Vectors are transformed to local
   * space before and after sampling. */
  float G1i;
  const float3 local_I = make_float3(dot(X, wi), dot(Y, wi), cos_NI);
  const float3 local_H = microfacet_sample_stretched<m_type>(
      kg, local_I, alpha_x, alpha_y, randu, randv, &G1i);

  const float3 H = X * local_H.x + Y * local_H.y + N * local_H.z;
  const float cos_NH = local_H.z;
  const float cos_HI = dot(H, wi);

  bool valid = false;
  if (m_refractive) {
    float3 R, T;
    bool inside;

    float fresnel = fresnel_dielectric(m_eta, H, wi, &R, &T, &inside);
    *wo = T;

    valid = !inside && fresnel != 1.0f;
  }
  else {
    /* Eq. 39 - compute actual reflected direction */
    *wo = 2 * cos_HI * H - wi;

    valid = dot(Ng, *wo) > 0;
  }

  if (!valid) {
    *eval = zero_spectrum();
    *pdf = 0.0f;
    return label | LABEL_GLOSSY;
  }

  if (alpha_x * alpha_y <= 1e-7f || (m_refractive && fabsf(m_eta - 1.0f) < 1e-4f)) {
    label |= LABEL_SINGULAR;
    /* Some high number for MIS. */
    *pdf = 1e6f;
    *eval = make_spectrum(1e6f);

    bool use_fresnel = (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_FRESNEL_ID ||
                        bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID);

    if (use_fresnel && !m_refractive) {
      *eval *= reflection_color(bsdf, *wo, H);
    }
  }
  else {
    label |= LABEL_GLOSSY;
    float cos_NO = dot(N, *wo);
    float D, G1o;

    /* TODO: add support for anisotropic transmission. */
    if (alpha_x == alpha_y || m_refractive) { /* Isotropic. */
      float alpha2 = alpha_x * alpha_y;

      if (bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_CLEARCOAT_ID) {
        D = bsdf_clearcoat_D(alpha2, cos_NH);

        /* The masking-shadowing term for clearcoat has a fixed alpha of 0.25
         * => alpha2 = 0.25 * 0.25 */
        alpha2 = 0.0625f;

        /* Recalculate G1i. */
        G1i = bsdf_G1<m_type>(alpha2, cos_NI);
      }
      else {
        D = bsdf_D<m_type>(alpha2, cos_NH);
      }

      G1o = bsdf_G1<m_type>(alpha2, cos_NO);
    }
    else { /* Anisotropic. */
      const float3 local_O = make_float3(dot(X, *wo), dot(Y, *wo), cos_NO);

      D = bsdf_aniso_D<m_type>(alpha_x, alpha_y, local_H);

      G1o = bsdf_aniso_G1<m_type>(alpha_x, alpha_y, local_O);
    }

    const float cos_HO = dot(H, *wo);
    const float common = G1i * D / cos_NI *
                         (m_refractive ? fabsf(cos_HI * cos_HO) / sqr(cos_HO + cos_HI / m_eta) :
                                         0.25f);

    *pdf = common;

    Spectrum F = m_refractive ? one_spectrum() : reflection_color(bsdf, *wo, H);

    *eval = G1o * common * F;
  }

  *sampled_roughness = make_float2(alpha_x, alpha_y);
  *eta = m_refractive ? 1.0f / m_eta : m_eta;

  return label;
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

ccl_device Spectrum bsdf_microfacet_ggx_eval(ccl_private const ShaderClosure *sc,
                                             const float3 Ng,
                                             const float3 wi,
                                             const float3 wo,
                                             ccl_private float *pdf)
{
  return bsdf_microfacet_eval<MicrofacetType::GGX>(sc, Ng, wi, wo, pdf);
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
  return bsdf_microfacet_sample<MicrofacetType::GGX>(
      kg, sc, Ng, wi, randu, randv, eval, wo, pdf, sampled_roughness, eta);
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

ccl_device Spectrum bsdf_microfacet_beckmann_eval(ccl_private const ShaderClosure *sc,
                                                  const float3 Ng,
                                                  const float3 wi,
                                                  const float3 wo,
                                                  ccl_private float *pdf)
{
  return bsdf_microfacet_eval<MicrofacetType::BECKMANN>(sc, Ng, wi, wo, pdf);
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
  return bsdf_microfacet_sample<MicrofacetType::BECKMANN>(
      kg, sc, Ng, wi, randu, randv, eval, wo, pdf, sampled_roughness, eta);
}

CCL_NAMESPACE_END
