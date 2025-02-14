/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

struct OrenNayarBsdf {
  SHADER_CLOSURE_BASE;

  float roughness;
  float a;
  float b;
  Spectrum multiscatter_term;
};

static_assert(sizeof(ShaderClosure) >= sizeof(OrenNayarBsdf), "OrenNayarBsdf is too large!");

/* NOTE: This implements the improved Oren-Nayar model by Yasuhiro Fujii
 * (https://mimosa-pudica.net/improved-oren-nayar.html), plus an
 * energy-preserving multi-scattering term based on the OpenPBR specification
 * (https://academysoftwarefoundation.github.io/OpenPBR). */

ccl_device_inline float bsdf_oren_nayar_G(const float cosTheta)
{
  if (cosTheta < 1e-6f) {
    /* The tan(theta) term starts to act up at low cosTheta, so fall back to Taylor expansion. */
    return (M_PI_2_F - 2.0f / 3.0f) - cosTheta;
  }
  const float sinTheta = sin_from_cos(cosTheta);
  const float theta = safe_acosf(cosTheta);
  return sinTheta * (theta - 2.0f / 3.0f - sinTheta * cosTheta) +
         2.0f / 3.0f * (sinTheta / cosTheta) * (1.0f - sqr(sinTheta) * sinTheta);
}

ccl_device Spectrum bsdf_oren_nayar_get_intensity(const ccl_private ShaderClosure *sc,
                                                  const float3 n,
                                                  const float3 v,
                                                  const float3 l)
{
  const ccl_private OrenNayarBsdf *bsdf = (const ccl_private OrenNayarBsdf *)sc;
  const float nl = max(dot(n, l), 0.0f);
  if (bsdf->b <= 0.0f) {
    return make_spectrum(nl * M_1_PI_F);
  }
  const float nv = max(dot(n, v), 0.0f);
  float t = dot(l, v) - nl * nv;

  if (t > 0.0f) {
    t /= max(nl, nv) + FLT_MIN;
  }

  const float single_scatter = bsdf->a + bsdf->b * t;

  const float El = bsdf->a * M_PI_F + bsdf->b * bsdf_oren_nayar_G(nl);
  const Spectrum multi_scatter = bsdf->multiscatter_term * (1.0f - El);

  return nl * (make_spectrum(single_scatter) + multi_scatter);
}

ccl_device int bsdf_oren_nayar_setup(const ccl_private ShaderData *sd,
                                     ccl_private OrenNayarBsdf *bsdf,
                                     const Spectrum color)
{
  bsdf->type = CLOSURE_BSDF_OREN_NAYAR_ID;

  const float sigma = saturatef(bsdf->roughness);
  bsdf->a = 1.0f / (M_PI_F + sigma * (M_PI_2_F - 2.0f / 3.0f));
  bsdf->b = sigma * bsdf->a;

  /* Compute energy compensation term (except for (1.0f - El) factor since it depends on wo). */
  const Spectrum albedo = saturate(color);
  const float Eavg = bsdf->a * M_PI_F + ((M_2PI_F - 5.6f) / 3.0f) * bsdf->b;
  const Spectrum Ems = M_1_PI_F * sqr(albedo) * (Eavg / (1.0f - Eavg)) /
                       (one_spectrum() - albedo * (1.0f - Eavg));
  const float nv = max(dot(bsdf->N, sd->wi), 0.0f);
  const float Ev = bsdf->a * M_PI_F + bsdf->b * bsdf_oren_nayar_G(nv);
  bsdf->multiscatter_term = Ems * (1.0f - Ev);

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_oren_nayar_eval(const ccl_private ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  const ccl_private OrenNayarBsdf *bsdf = (const ccl_private OrenNayarBsdf *)sc;
  const float cosNO = dot(bsdf->N, wo);
  if (cosNO > 0.0f) {
    *pdf = cosNO * M_1_PI_F;
    return bsdf_oren_nayar_get_intensity(sc, bsdf->N, wi, wo);
  }
  *pdf = 0.0f;
  return zero_spectrum();
}

ccl_device int bsdf_oren_nayar_sample(const ccl_private ShaderClosure *sc,
                                      const float3 Ng,
                                      const float3 wi,
                                      const float2 rand,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf)
{
  const ccl_private OrenNayarBsdf *bsdf = (const ccl_private OrenNayarBsdf *)sc;

  sample_cos_hemisphere(bsdf->N, rand, wo, pdf);

  if (dot(Ng, *wo) > 0.0f) {
    *eval = bsdf_oren_nayar_get_intensity(sc, bsdf->N, wi, *wo);
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END
