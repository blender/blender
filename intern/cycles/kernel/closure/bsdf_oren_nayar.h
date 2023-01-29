/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

typedef struct OrenNayarBsdf {
  SHADER_CLOSURE_BASE;

  float roughness;
  float a;
  float b;
} OrenNayarBsdf;

static_assert(sizeof(ShaderClosure) >= sizeof(OrenNayarBsdf), "OrenNayarBsdf is too large!");

ccl_device Spectrum bsdf_oren_nayar_get_intensity(ccl_private const ShaderClosure *sc,
                                                  float3 n,
                                                  float3 v,
                                                  float3 l)
{
  ccl_private const OrenNayarBsdf *bsdf = (ccl_private const OrenNayarBsdf *)sc;
  float nl = max(dot(n, l), 0.0f);
  float nv = max(dot(n, v), 0.0f);
  float t = dot(l, v) - nl * nv;

  if (t > 0.0f)
    t /= max(nl, nv) + FLT_MIN;
  float is = nl * (bsdf->a + bsdf->b * t);
  return make_spectrum(is);
}

ccl_device int bsdf_oren_nayar_setup(ccl_private OrenNayarBsdf *bsdf)
{
  float sigma = bsdf->roughness;

  bsdf->type = CLOSURE_BSDF_OREN_NAYAR_ID;

  sigma = saturatef(sigma);

  float div = 1.0f / (M_PI_F + ((3.0f * M_PI_F - 4.0f) / 6.0f) * sigma);

  bsdf->a = 1.0f * div;
  bsdf->b = sigma * div;

  return SD_BSDF | SD_BSDF_HAS_EVAL;
}

ccl_device Spectrum bsdf_oren_nayar_eval(ccl_private const ShaderClosure *sc,
                                         const float3 wi,
                                         const float3 wo,
                                         ccl_private float *pdf)
{
  ccl_private const OrenNayarBsdf *bsdf = (ccl_private const OrenNayarBsdf *)sc;
  if (dot(bsdf->N, wo) > 0.0f) {
    *pdf = 0.5f * M_1_PI_F;
    return bsdf_oren_nayar_get_intensity(sc, bsdf->N, wi, wo);
  }
  else {
    *pdf = 0.0f;
    return zero_spectrum();
  }
}

ccl_device int bsdf_oren_nayar_sample(ccl_private const ShaderClosure *sc,
                                      float3 Ng,
                                      float3 wi,
                                      float randu,
                                      float randv,
                                      ccl_private Spectrum *eval,
                                      ccl_private float3 *wo,
                                      ccl_private float *pdf)
{
  ccl_private const OrenNayarBsdf *bsdf = (ccl_private const OrenNayarBsdf *)sc;
  sample_uniform_hemisphere(bsdf->N, randu, randv, wo, pdf);

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
