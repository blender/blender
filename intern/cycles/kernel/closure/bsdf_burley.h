/* SPDX-FileCopyrightText: 2009-2025 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/closure/bsdf_util.h"
#include "kernel/sample/mapping.h"

CCL_NAMESPACE_BEGIN

#ifdef __OSL__

typedef struct BurleyBsdf {
  SHADER_CLOSURE_BASE;

  float roughness;
} BurleyBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(BurleyBsdf), "BurleyBsdf is too large!");

ccl_device Spectrum bsdf_burley_get_intensity(const float roughness,
                                              const float3 n,
                                              const float3 v,
                                              const float3 l)
{
  const float NdotL = dot(n, l);
  const float NdotV = dot(n, v);
  const float fl = schlick_fresnel(NdotL);
  const float fv = schlick_fresnel(NdotV);
  const float LdotH = dot(l, normalize(l + v));
  const float F90 = 0.5f + (2.0f * roughness * LdotH * LdotH);
  return make_spectrum(M_1_PI_F * NdotL * mix(1.0f, F90, fl) * mix(1.0f, F90, fv));
}

ccl_device int bsdf_burley_setup(ccl_private BurleyBsdf *bsdf, const float roughness)
{
  bsdf->type = CLOSURE_BSDF_BURLEY_ID;
  bsdf->roughness = saturatef(roughness);
  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_burley_eval(ccl_private const ShaderClosure *sc,
                                     const float3 wi,
                                     const float3 wo,
                                     ccl_private float *pdf)
{
  ccl_private const BurleyBsdf *bsdf = (ccl_private const BurleyBsdf *)sc;

  const float cosNO = dot(bsdf->N, wo);
  if (cosNO > 0.0f) {
    *pdf = cosNO * M_1_PI_F;
    return bsdf_burley_get_intensity(bsdf->roughness, bsdf->N, wi, wo);
  }
  else {
    *pdf = 0.0f;
    return zero_spectrum();
  }
}

ccl_device int bsdf_burley_sample(ccl_private const ShaderClosure *sc,
                                  float3 Ng,
                                  float3 wi,
                                  float2 rand,
                                  ccl_private Spectrum *eval,
                                  ccl_private float3 *wo,
                                  ccl_private float *pdf)
{
  ccl_private const BurleyBsdf *bsdf = (ccl_private const BurleyBsdf *)sc;
  float3 N = bsdf->N;

  // distribution over the hemisphere
  sample_cos_hemisphere(N, rand, wo, pdf);

  if (dot(Ng, *wo) > 0.0f) {
    *eval = bsdf_burley_get_intensity(bsdf->roughness, bsdf->N, wi, *wo);
  }
  else {
    *pdf = 0.0f;
    *eval = zero_spectrum();
  }
  return LABEL_REFLECT | LABEL_DIFFUSE;
}

#endif /* __OSL__ */

CCL_NAMESPACE_END
