/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted from Open Shading Language
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011-2022 Blender Foundation. */

#pragma once

CCL_NAMESPACE_BEGIN

typedef struct HairBsdf {
  SHADER_CLOSURE_BASE;

  float3 T;
  float roughness1;
  float roughness2;
  float offset;
} HairBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(HairBsdf), "HairBsdf is too large!");

ccl_device int bsdf_hair_reflection_setup(ccl_private HairBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_HAIR_REFLECTION_ID;
  bsdf->roughness1 = clamp(bsdf->roughness1, 0.001f, 1.0f);
  bsdf->roughness2 = clamp(bsdf->roughness2, 0.001f, 1.0f);
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device int bsdf_hair_transmission_setup(ccl_private HairBsdf *bsdf)
{
  bsdf->type = CLOSURE_BSDF_HAIR_TRANSMISSION_ID;
  bsdf->roughness1 = clamp(bsdf->roughness1, 0.001f, 1.0f);
  bsdf->roughness2 = clamp(bsdf->roughness2, 0.001f, 1.0f);
  return SD_BSDF | SD_BSDF_HAS_EVAL | SD_BSDF_HAS_TRANSMISSION;
}

ccl_device Spectrum bsdf_hair_reflection_eval(ccl_private const ShaderClosure *sc,
                                              const float3 wi,
                                              const float3 wo,
                                              ccl_private float *pdf)
{
  ccl_private const HairBsdf *bsdf = (ccl_private const HairBsdf *)sc;
  if (dot(bsdf->N, wo) < 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float offset = bsdf->offset;
  float3 Tg = bsdf->T;
  float roughness1 = bsdf->roughness1;
  float roughness2 = bsdf->roughness2;

  float Iz = dot(Tg, wi);
  float3 locy = normalize(wi - Tg * Iz);

  float theta_r = M_PI_2_F - fast_acosf(Iz);

  float wo_z = dot(Tg, wo);
  float3 wo_y = normalize(wo - Tg * wo_z);

  float theta_i = M_PI_2_F - fast_acosf(wo_z);
  float cosphi_i = dot(wo_y, locy);

  if (M_PI_2_F - fabsf(theta_i) < 0.001f || cosphi_i < 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float roughness1_inv = 1.0f / roughness1;
  float roughness2_inv = 1.0f / roughness2;
  float phi_i = fast_acosf(cosphi_i) * roughness2_inv;
  phi_i = fabsf(phi_i) < M_PI_F ? phi_i : M_PI_F;
  float costheta_i = fast_cosf(theta_i);

  float a_R = fast_atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);
  float b_R = fast_atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);

  float theta_h = (theta_i + theta_r) * 0.5f;
  float t = theta_h - offset;

  float phi_pdf = fast_cosf(phi_i * 0.5f) * 0.25f * roughness2_inv;
  float theta_pdf = roughness1 /
                    (2 * (t * t + roughness1 * roughness1) * (a_R - b_R) * costheta_i);
  *pdf = phi_pdf * theta_pdf;

  return make_spectrum(*pdf);
}

ccl_device Spectrum bsdf_hair_transmission_eval(ccl_private const ShaderClosure *sc,
                                                const float3 wi,
                                                const float3 wo,
                                                ccl_private float *pdf)
{
  ccl_private const HairBsdf *bsdf = (ccl_private const HairBsdf *)sc;
  if (dot(bsdf->N, wo) >= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float offset = bsdf->offset;
  float3 Tg = bsdf->T;
  float roughness1 = bsdf->roughness1;
  float roughness2 = bsdf->roughness2;
  float Iz = dot(Tg, wi);
  float3 locy = normalize(wi - Tg * Iz);

  float theta_r = M_PI_2_F - fast_acosf(Iz);

  float wo_z = dot(Tg, wo);
  float3 wo_y = normalize(wo - Tg * wo_z);

  float theta_i = M_PI_2_F - fast_acosf(wo_z);
  float phi_i = fast_acosf(dot(wo_y, locy));

  if (M_PI_2_F - fabsf(theta_i) < 0.001f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  float costheta_i = fast_cosf(theta_i);

  float roughness1_inv = 1.0f / roughness1;
  float a_TT = fast_atan2f(((M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  float b_TT = fast_atan2f(((-M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  float c_TT = 2 * fast_atan2f(M_PI_2_F / roughness2, 1.0f);

  float theta_h = (theta_i + theta_r) / 2;
  float t = theta_h - offset;
  float phi = fabsf(phi_i);

  float p = M_PI_F - phi;
  float theta_pdf = roughness1 /
                    (2 * (t * t + roughness1 * roughness1) * (a_TT - b_TT) * costheta_i);
  float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

  *pdf = phi_pdf * theta_pdf;
  return make_spectrum(*pdf);
}

ccl_device int bsdf_hair_reflection_sample(ccl_private const ShaderClosure *sc,
                                           float3 Ng,
                                           float3 wi,
                                           float randu,
                                           float randv,
                                           ccl_private Spectrum *eval,
                                           ccl_private float3 *wo,
                                           ccl_private float *pdf,
                                           ccl_private float2 *sampled_roughness)
{
  ccl_private const HairBsdf *bsdf = (ccl_private const HairBsdf *)sc;
  float offset = bsdf->offset;
  float3 Tg = bsdf->T;
  float roughness1 = bsdf->roughness1;
  float roughness2 = bsdf->roughness2;
  *sampled_roughness = make_float2(roughness1, roughness2);
  float Iz = dot(Tg, wi);
  float3 locy = normalize(wi - Tg * Iz);
  float3 locx = cross(locy, Tg);
  float theta_r = M_PI_2_F - fast_acosf(Iz);

  float roughness1_inv = 1.0f / roughness1;
  float a_R = fast_atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);
  float b_R = fast_atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);

  float t = roughness1 * tanf(randu * (a_R - b_R) + b_R);

  float theta_h = t + offset;
  float theta_i = 2 * theta_h - theta_r;

  float costheta_i, sintheta_i;
  fast_sincosf(theta_i, &sintheta_i, &costheta_i);

  float phi = 2 * safe_asinf(1 - 2 * randv) * roughness2;

  float phi_pdf = fast_cosf(phi * 0.5f) * 0.25f / roughness2;

  float theta_pdf = roughness1 /
                    (2 * (t * t + roughness1 * roughness1) * (a_R - b_R) * costheta_i);

  float sinphi, cosphi;
  fast_sincosf(phi, &sinphi, &cosphi);
  *wo = (cosphi * costheta_i) * locy - (sinphi * costheta_i) * locx + (sintheta_i)*Tg;

  *pdf = fabsf(phi_pdf * theta_pdf);
  if (M_PI_2_F - fabsf(theta_i) < 0.001f)
    *pdf = 0.0f;

  *eval = make_spectrum(*pdf);

  return LABEL_REFLECT | LABEL_GLOSSY;
}

ccl_device int bsdf_hair_transmission_sample(ccl_private const ShaderClosure *sc,
                                             float3 Ng,
                                             float3 wi,
                                             float randu,
                                             float randv,
                                             ccl_private Spectrum *eval,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf,
                                             ccl_private float2 *sampled_roughness)
{
  ccl_private const HairBsdf *bsdf = (ccl_private const HairBsdf *)sc;
  float offset = bsdf->offset;
  float3 Tg = bsdf->T;
  float roughness1 = bsdf->roughness1;
  float roughness2 = bsdf->roughness2;
  *sampled_roughness = make_float2(roughness1, roughness2);
  float Iz = dot(Tg, wi);
  float3 locy = normalize(wi - Tg * Iz);
  float3 locx = cross(locy, Tg);
  float theta_r = M_PI_2_F - fast_acosf(Iz);

  float roughness1_inv = 1.0f / roughness1;
  float a_TT = fast_atan2f(((M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  float b_TT = fast_atan2f(((-M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  float c_TT = 2 * fast_atan2f(M_PI_2_F / roughness2, 1.0f);

  float t = roughness1 * tanf(randu * (a_TT - b_TT) + b_TT);

  float theta_h = t + offset;
  float theta_i = 2 * theta_h - theta_r;

  float costheta_i, sintheta_i;
  fast_sincosf(theta_i, &sintheta_i, &costheta_i);

  float p = roughness2 * tanf(c_TT * (randv - 0.5f));
  float phi = p + M_PI_F;
  float theta_pdf = roughness1 /
                    (2 * (t * t + roughness1 * roughness1) * (a_TT - b_TT) * costheta_i);
  float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

  float sinphi, cosphi;
  fast_sincosf(phi, &sinphi, &cosphi);
  *wo = (cosphi * costheta_i) * locy - (sinphi * costheta_i) * locx + (sintheta_i)*Tg;

  *pdf = fabsf(phi_pdf * theta_pdf);
  if (M_PI_2_F - fabsf(theta_i) < 0.001f) {
    *pdf = 0.0f;
  }

  *eval = make_spectrum(*pdf);

  /* TODO(sergey): Should always be negative, but seems some precision issue
   * is involved here.
   */
  kernel_assert(dot(locy, *wo) < 1e-4f);

  return LABEL_TRANSMIT | LABEL_GLOSSY;
}

CCL_NAMESPACE_END
