/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/types.h"

#include "util/math_fast.h"

CCL_NAMESPACE_BEGIN

struct HairBsdf {
  SHADER_CLOSURE_BASE;

  float3 T;
  float roughness1;
  float roughness2;
  float offset;
};

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

ccl_device Spectrum bsdf_hair_reflection_eval(const ccl_private ShaderClosure *sc,
                                              const float3 wi,
                                              const float3 wo,
                                              ccl_private float *pdf)
{
  const ccl_private HairBsdf *bsdf = (const ccl_private HairBsdf *)sc;
  if (dot(bsdf->N, wo) < 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float offset = bsdf->offset;
  const float3 Tg = bsdf->T;
  const float roughness1 = bsdf->roughness1;
  const float roughness2 = bsdf->roughness2;

  const float Iz = dot(Tg, wi);
  const float3 locy = normalize(wi - Tg * Iz);

  const float theta_r = M_PI_2_F - fast_acosf(Iz);

  const float wo_z = dot(Tg, wo);
  const float3 wo_y = normalize(wo - Tg * wo_z);

  const float theta_i = M_PI_2_F - fast_acosf(wo_z);
  const float cosphi_i = dot(wo_y, locy);

  if (M_PI_2_F - fabsf(theta_i) < 0.001f || cosphi_i < 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float roughness1_inv = 1.0f / roughness1;
  const float roughness2_inv = 1.0f / roughness2;
  float phi_i = fast_acosf(cosphi_i) * roughness2_inv;
  phi_i = fabsf(phi_i) < M_PI_F ? phi_i : M_PI_F;
  const float costheta_i = fast_cosf(theta_i);

  const float a_R = fast_atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);
  const float b_R = fast_atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);

  const float theta_h = (theta_i + theta_r) * 0.5f;
  const float t = theta_h - offset;

  const float phi_pdf = fast_cosf(phi_i * 0.5f) * 0.25f * roughness2_inv;
  const float theta_pdf = roughness1 /
                          (2 * (t * t + roughness1 * roughness1) * (a_R - b_R) * costheta_i);
  *pdf = phi_pdf * theta_pdf;

  return make_spectrum(*pdf);
}

ccl_device Spectrum bsdf_hair_transmission_eval(const ccl_private ShaderClosure *sc,
                                                const float3 wi,
                                                const float3 wo,
                                                ccl_private float *pdf)
{
  const ccl_private HairBsdf *bsdf = (const ccl_private HairBsdf *)sc;
  if (dot(bsdf->N, wo) >= 0.0f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float offset = bsdf->offset;
  const float3 Tg = bsdf->T;
  const float roughness1 = bsdf->roughness1;
  const float roughness2 = bsdf->roughness2;
  const float Iz = dot(Tg, wi);
  const float3 locy = normalize(wi - Tg * Iz);

  const float theta_r = M_PI_2_F - fast_acosf(Iz);

  const float wo_z = dot(Tg, wo);
  const float3 wo_y = normalize(wo - Tg * wo_z);

  const float theta_i = M_PI_2_F - fast_acosf(wo_z);
  const float phi_i = fast_acosf(dot(wo_y, locy));

  if (M_PI_2_F - fabsf(theta_i) < 0.001f) {
    *pdf = 0.0f;
    return zero_spectrum();
  }

  const float costheta_i = fast_cosf(theta_i);

  const float roughness1_inv = 1.0f / roughness1;
  const float a_TT = fast_atan2f(((M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  const float b_TT = fast_atan2f(((-M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  const float c_TT = 2 * fast_atan2f(M_PI_2_F / roughness2, 1.0f);

  const float theta_h = (theta_i + theta_r) / 2;
  const float t = theta_h - offset;
  const float phi = fabsf(phi_i);

  const float p = M_PI_F - phi;
  const float theta_pdf = roughness1 /
                          (2 * (t * t + roughness1 * roughness1) * (a_TT - b_TT) * costheta_i);
  const float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

  *pdf = phi_pdf * theta_pdf;
  return make_spectrum(*pdf);
}

ccl_device int bsdf_hair_reflection_sample(const ccl_private ShaderClosure *sc,
                                           const float3 Ng,
                                           const float3 wi,
                                           const float2 rand,
                                           ccl_private Spectrum *eval,
                                           ccl_private float3 *wo,
                                           ccl_private float *pdf,
                                           ccl_private float2 *sampled_roughness)
{
  const ccl_private HairBsdf *bsdf = (const ccl_private HairBsdf *)sc;
  const float offset = bsdf->offset;
  const float3 Tg = bsdf->T;
  const float roughness1 = bsdf->roughness1;
  const float roughness2 = bsdf->roughness2;
  *sampled_roughness = make_float2(roughness1, roughness2);
  const float Iz = dot(Tg, wi);
  const float3 locy = normalize(wi - Tg * Iz);
  const float3 locx = cross(locy, Tg);
  const float theta_r = M_PI_2_F - fast_acosf(Iz);

  const float roughness1_inv = 1.0f / roughness1;
  const float a_R = fast_atan2f(((M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);
  const float b_R = fast_atan2f(((-M_PI_2_F + theta_r) * 0.5f - offset) * roughness1_inv, 1.0f);

  const float t = roughness1 * tanf(rand.x * (a_R - b_R) + b_R);

  const float theta_h = t + offset;
  const float theta_i = 2 * theta_h - theta_r;

  float costheta_i;
  float sintheta_i;
  fast_sincosf(theta_i, &sintheta_i, &costheta_i);

  const float phi = 2 * safe_asinf(1 - 2 * rand.y) * roughness2;

  const float phi_pdf = fast_cosf(phi * 0.5f) * 0.25f / roughness2;

  const float theta_pdf = roughness1 /
                          (2 * (t * t + roughness1 * roughness1) * (a_R - b_R) * costheta_i);

  float sinphi;
  float cosphi;
  fast_sincosf(phi, &sinphi, &cosphi);
  *wo = (cosphi * costheta_i) * locy - (sinphi * costheta_i) * locx + (sintheta_i)*Tg;

  *pdf = fabsf(phi_pdf * theta_pdf);
  if (M_PI_2_F - fabsf(theta_i) < 0.001f) {
    *pdf = 0.0f;
  }

  *eval = make_spectrum(*pdf);

  return LABEL_REFLECT | LABEL_GLOSSY;
}

ccl_device int bsdf_hair_transmission_sample(const ccl_private ShaderClosure *sc,
                                             const float3 Ng,
                                             const float3 wi,
                                             const float2 rand,
                                             ccl_private Spectrum *eval,
                                             ccl_private float3 *wo,
                                             ccl_private float *pdf,
                                             ccl_private float2 *sampled_roughness)
{
  const ccl_private HairBsdf *bsdf = (const ccl_private HairBsdf *)sc;
  const float offset = bsdf->offset;
  const float3 Tg = bsdf->T;
  const float roughness1 = bsdf->roughness1;
  const float roughness2 = bsdf->roughness2;
  *sampled_roughness = make_float2(roughness1, roughness2);
  const float Iz = dot(Tg, wi);
  const float3 locy = normalize(wi - Tg * Iz);
  const float3 locx = cross(locy, Tg);
  const float theta_r = M_PI_2_F - fast_acosf(Iz);

  const float roughness1_inv = 1.0f / roughness1;
  const float a_TT = fast_atan2f(((M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  const float b_TT = fast_atan2f(((-M_PI_2_F + theta_r) / 2 - offset) * roughness1_inv, 1.0f);
  const float c_TT = 2 * fast_atan2f(M_PI_2_F / roughness2, 1.0f);

  const float t = roughness1 * tanf(rand.x * (a_TT - b_TT) + b_TT);

  const float theta_h = t + offset;
  const float theta_i = 2 * theta_h - theta_r;

  float costheta_i;
  float sintheta_i;
  fast_sincosf(theta_i, &sintheta_i, &costheta_i);

  const float p = roughness2 * tanf(c_TT * (rand.y - 0.5f));
  const float phi = p + M_PI_F;
  const float theta_pdf = roughness1 /
                          (2 * (t * t + roughness1 * roughness1) * (a_TT - b_TT) * costheta_i);
  const float phi_pdf = roughness2 / (c_TT * (p * p + roughness2 * roughness2));

  float sinphi;
  float cosphi;
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
