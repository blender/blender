/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/closure/alloc.h"
#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

struct OrenNayarParam {
  float roughness;
  float a;
  float b;
  Spectrum multiscatter_term;
};

struct OrenNayarBsdf {
  SHADER_CLOSURE_BASE;

  OrenNayarParam param;
};

static_assert(sizeof(ShaderClosure) >= sizeof(OrenNayarBsdf), "OrenNayarBsdf is too large!");

/* NOTE: This implements the improved Oren-Nayar model by Yasuhiro Fujii
 * (https://mimosa-pudica.net/improved-oren-nayar.html), plus an
 * energy-preserving multi-scattering term based on the OpenPBR specification
 * (https://academysoftwarefoundation.github.io/OpenPBR). */

/* Above certain roughness threshold we switch to Oren Nayar model. */
ccl_device_forceinline bool diffuse_roughness_is_almost_zero(const float alpha)
{
  return alpha < 1e-5f;
}

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

ccl_device Spectrum bsdf_oren_nayar_get_intensity(const ccl_private OrenNayarBsdf *bsdf,
                                                  const float3 n,
                                                  const float3 v,
                                                  const float3 l)
{
  const OrenNayarParam param = bsdf->param;
  const float nl = max(dot(n, l), 0.0f);
  if (param.b <= 0.0f) {
    return make_spectrum(nl * M_1_PI_F);
  }
  const float nv = max(dot(n, v), 0.0f);
  float t = dot(l, v) - nl * nv;

  if (t > 0.0f) {
    t /= max(nl, nv) + FLT_MIN;
  }

  const float single_scatter = param.a + param.b * t;

  const float El = param.a * M_PI_F + param.b * bsdf_oren_nayar_G(nl);
  const Spectrum multi_scatter = param.multiscatter_term * (1.0f - El);

  return nl * (make_spectrum(single_scatter) + multi_scatter);
}

ccl_device_inline OrenNayarParam bsdf_oren_nayar_param(const Spectrum color,
                                                       const float nv,
                                                       const float roughness)
{
  const float sigma = saturatef(roughness);
  const float a = 1.0f / (M_PI_F + sigma * (M_PI_2_F - 2.0f / 3.0f));
  const float b = sigma * a;

  /* Compute energy compensation term (except for (1.0f - El) factor since it depends on wo). */
  const Spectrum albedo = saturate(color);
  const float Eavg = a * M_PI_F + ((M_2PI_F - 5.6f) / 3.0f) * b;
  const Spectrum Ems = M_1_PI_F * sqr(albedo) * (Eavg / (1.0f - Eavg)) /
                       (one_spectrum() - albedo * (1.0f - Eavg));
  const float Ev = a * M_PI_F + b * bsdf_oren_nayar_G(max(nv, 0.0f));

  return {/* .roughness = */ roughness,
          /* .a = */ a,
          /* .b = */ b,
          /* .multiscatter_term = */ Ems * (1.0f - Ev)};
}

ccl_device void bsdf_oren_nayar_setup(ccl_private ShaderData *sd,
                                      const float3 N,
                                      const Spectrum weight,
                                      const float roughness,
                                      const Spectrum color)
{
  ccl_private OrenNayarBsdf *bsdf = (ccl_private OrenNayarBsdf *)bsdf_alloc(
      sd, sizeof(OrenNayarBsdf), weight);
  if (bsdf) {
    bsdf->N = N;
    bsdf->type = CLOSURE_BSDF_OREN_NAYAR_ID;
    bsdf->param = bsdf_oren_nayar_param(color, dot(bsdf->N, sd->wi), roughness);
    sd->flag |= SD_BSDF | SD_BSDF_HAS_EVAL;
  }
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
    return bsdf_oren_nayar_get_intensity(bsdf, bsdf->N, wi, wo);
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
    *eval = bsdf_oren_nayar_get_intensity(bsdf, bsdf->N, wi, *wo);
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }

  return LABEL_REFLECT | LABEL_DIFFUSE;
}

CCL_NAMESPACE_END
